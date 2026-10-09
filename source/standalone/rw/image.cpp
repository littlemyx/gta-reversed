// P2B-03a: RwImage* / RtBMP* / RtPNG* on top of librw (rw::Image).
//   Game-called (rwapi.h): RwImageCreate, RwImageDestroy, RwImageAllocatePixels, RwImageFindRasterFormat, RwImageSetFromRaster,
//   RtBMPImageRead, RtPNGImageWrite.
//   Declared in rwextra.h (03a block): RwImageFreePixels, RwImageRead, RwImageReadMaskedImage, RwImageWrite, RwImageGetPath/SetPath,
//   RwImageCopy, RwImageResize, RwImageResample, RwImageCreateResample, RwImageApplyMask, RwImageMakeMask, RtBMPImageWrite, RtPNGImageRead
//   (+ inline RwImageSetStride / SetPixels / SetPalette in rwextra.h; the Get* accessors are macros in rwaccessors.h).
// Function classes (see .notes/P2B_SHIM_PLAN.md): D = direct, A = adapter, W = written here because librw has nothing equivalent.
//
// RW image layout (shared with librw, rwpixel.h): top-down rows; 32 bit = R,G,B,A bytes, 24 bit = R,G,B, 16 bit = ARGB1555 word, 8 bit = index
// byte, 4 bit = index per BYTE (librw/RW-shim convention), palette = R,G,B,A x (16 | 256). Differences to stock librw handled here:
//   * RwImageAllocatePixels pads the stride to a multiple of 4 bytes like RW (librw packs rows). librw's own readers/writers only assume the
//     stride they produced themselves, so every librw writer is fed a tight copy (RtPNGImageWrite, RwImageWrite) and the BMP reader / writer are
//     written here (librw's readBMP computes the row padding as `w*bpp % 4`, wrong for 24 bit images whose width is not a multiple of 4; its
//     writer drops the 4-bit packing rules for odd widths and cannot write a top-down file either).
//   * RwImageSetPixels / SetPalette only store the pointer (the image does not own it afterwards), librw's setPixels transfers ownership.
//   * librw's Raster::setFromImage / toImage convert few formats: RwImageSetFromRaster converts any lockable raster format into any image depth.
// Only needs fakerw + librw + the CRT (also built by the PCH-less unit test tests/standalone/rw_raster_image_test.cpp). Excluded from unity builds.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

// librw's D3D9 device internals (D3dRaster: palette / bpp / customFormat of a locked raster)
#include <src/d3d/rwd3dimpl.h>

#include "rwpixel.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static_assert(rwIMAGEALLOCATED == 1 && rwIMAGEGAMMACORRECTED == 2);

namespace {
using rwshim::Px;

inline int BytesPerPixel(int depth) { return depth < 8 ? 1 : depth / 8; }
inline int AlignedStride(int width, int depth) { return (width * BytesPerPixel(depth) + 3) & ~3; }

char s_ImagePath[1024] = "";

bool FileExists(const char* path) {
    if (!path || !path[0]) {
        return false;
    }
    std::FILE* f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }
    std::fclose(f);
    return true;
}

bool ReadFile(const char* path, std::vector<uint8_t>& out) {
    std::FILE* f = path ? std::fopen(path, "rb") : nullptr;
    if (!f) {
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    const long len = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(len > 0 ? (size_t)len : 0);
    const bool ok = len > 0 && std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

// Image::create + allocate with RW's dword aligned stride (and optional zero fill, which RW does not do but costs nothing).
RwImage* NewImage(int w, int h, int depth) {
    if (w <= 0 || h <= 0) {
        return nullptr;
    }
    RwImage* img = RwImageCreate(w, h, depth);
    if (img && !RwImageAllocatePixels(img)) {
        RwImageDestroy(img);
        return nullptr;
    }
    return img;
}

// Pixel data of `src` into `dst` (same size). Index images copy indices + palette to an index image of at least the same palette size;
// everything else goes through RGBA. Returns false if the combination cannot be expressed (true colour -> index image).
bool ConvertInto(RwImage* dst, const RwImage* src) {
    if (!dst || !src || !dst->pixels || !src->pixels || dst->width != src->width || dst->height != src->height) {
        return false;
    }
    if (dst->depth <= 8) {
        if (src->depth > 8 || !src->palette || !dst->palette || (1 << src->depth) > (1 << dst->depth)) {
            return false;
        }
        for (int y = 0; y < dst->height; ++y) {
            std::memcpy(dst->pixels + (size_t)y * dst->stride, src->pixels + (size_t)y * src->stride, dst->width);
        }
        std::memset(dst->palette, 0, (1 << dst->depth) * 4);
        std::memcpy(dst->palette, src->palette, (1 << src->depth) * 4);
        return true;
    }
    for (int y = 0; y < dst->height; ++y) {
        for (int x = 0; x < dst->width; ++x) {
            rwshim::ImagePut(dst, x, y, rwshim::ImageGet(src, x, y));
        }
    }
    return true;
}

// Tight (stride == width * bpp) copy that every librw writer understands; 16 bit images become 32 bit (PNG has no 1555).
RwImage* MakeTightCopy(const RwImage* src) {
    const int depth = src->depth == 16 ? 32 : src->depth;
    RwImage* t = rw::Image::create(src->width, src->height, depth);
    if (!t) {
        return nullptr;
    }
    t->allocate();   // tight stride + palette for 4 / 8 bit
    if (!ConvertInto(t, src)) {
        t->destroy();
        return nullptr;
    }
    return t;
}

// ---- BMP ----
uint32_t Le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t Le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
void Put16(std::FILE* f, unsigned v) { std::fputc(v & 0xFF, f); std::fputc((v >> 8) & 0xFF, f); }
void Put32(std::FILE* f, uint32_t v) { Put16(f, v & 0xFFFF); Put16(f, v >> 16); }

// Bilinear / box resampling of one dst pixel (centre mapped into src space).
Px Sample(const RwImage* src, float fx0, float fy0, float fx1, float fy1) {
    // [fx0, fx1) x [fy0, fy1) in source pixels. A footprint wider than one pixel = box filter, otherwise bilinear around the centre.
    const float wx = fx1 - fx0, wy = fy1 - fy0;
    const int   sw = src->width, sh = src->height;
    auto clampi = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
    float r = 0, g = 0, b = 0, a = 0;
    if (wx > 1.0f || wy > 1.0f) {
        const int x0 = clampi((int)std::floor(fx0), 0, sw - 1), x1 = clampi((int)std::ceil(fx1) - 1, x0, sw - 1);
        const int y0 = clampi((int)std::floor(fy0), 0, sh - 1), y1 = clampi((int)std::ceil(fy1) - 1, y0, sh - 1);
        float total = 0;
        for (int y = y0; y <= y1; ++y) {
            const float cy = std::fmin(fy1, (float)y + 1) - std::fmax(fy0, (float)y);
            for (int x = x0; x <= x1; ++x) {
                const float cx = std::fmin(fx1, (float)x + 1) - std::fmax(fx0, (float)x);
                const float wgt = (cx > 0 ? cx : 0) * (cy > 0 ? cy : 0);
                const Px p = rwshim::ImageGet(src, x, y);
                r += wgt * p.r; g += wgt * p.g; b += wgt * p.b; a += wgt * p.a;
                total += wgt;
            }
        }
        if (total <= 0) total = 1;
        r /= total; g /= total; b /= total; a /= total;
    } else {
        const float cx = (fx0 + fx1) * 0.5f - 0.5f, cy = (fy0 + fy1) * 0.5f - 0.5f;
        const int   ix = (int)std::floor(cx), iy = (int)std::floor(cy);
        const float tx = cx - (float)ix, ty = cy - (float)iy;
        const Px p00 = rwshim::ImageGet(src, clampi(ix, 0, sw - 1), clampi(iy, 0, sh - 1));
        const Px p10 = rwshim::ImageGet(src, clampi(ix + 1, 0, sw - 1), clampi(iy, 0, sh - 1));
        const Px p01 = rwshim::ImageGet(src, clampi(ix, 0, sw - 1), clampi(iy + 1, 0, sh - 1));
        const Px p11 = rwshim::ImageGet(src, clampi(ix + 1, 0, sw - 1), clampi(iy + 1, 0, sh - 1));
        auto mix = [&](uint8_t a00, uint8_t a10, uint8_t a01, uint8_t a11) {
            return (a00 * (1 - tx) + a10 * tx) * (1 - ty) + (a01 * (1 - tx) + a11 * tx) * ty;
        };
        r = mix(p00.r, p10.r, p01.r, p11.r); g = mix(p00.g, p10.g, p01.g, p11.g);
        b = mix(p00.b, p10.b, p01.b, p11.b); a = mix(p00.a, p10.a, p01.a, p11.a);
    }
    auto q = [](float v) { return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v + 0.5f)); };
    return { q(r), q(g), q(b), q(a) };
}

bool ResampleInto(RwImage* dst, const RwImage* src) {
    if (!dst->pixels || !src->pixels || dst->depth <= 8) {
        return false;
    }
    const float sx = (float)src->width / (float)dst->width, sy = (float)src->height / (float)dst->height;
    for (int y = 0; y < dst->height; ++y) {
        for (int x = 0; x < dst->width; ++x) {
            rwshim::ImagePut(dst, x, y, Sample(src, x * sx, y * sy, (x + 1) * sx, (y + 1) * sy));
        }
    }
    return true;
}
} // namespace

// D (+ validation): RW images start with no pixels, stride 0.
RwImage* RwImageCreate(RwInt32 width, RwInt32 height, RwInt32 depth) {
    if (width <= 0 || height <= 0 || (depth != 4 && depth != 8 && depth != 16 && depth != 24 && depth != 32)) {
        return nullptr;
    }
    return rw::Image::create(width, height, depth);
}

// D: frees the pixels / palette it allocated itself (flags), never ones set through RwImageSetPixels / SetPalette.
RwBool RwImageDestroy(RwImage* image) {
    if (!image) {
        return FALSE;
    }
    image->destroy();
    return TRUE;
}

// A: dword aligned stride like RW, palette (R,G,B,A x 16 / 256) for 4 / 8 bit. Already-allocated pixels / palette are kept. Memory is zeroed.
RwImage* RwImageAllocatePixels(RwImage* image) {
    if (!image) {
        return nullptr;
    }
    if (!image->pixels) {
        const int bpp = BytesPerPixel(image->depth);
        const int stride = AlignedStride(image->width, image->depth);
        auto* px = static_cast<uint8_t*>(rwNew((size_t)stride * image->height, rw::MEMDUR_EVENT | rw::ID_IMAGE));
        if (!px) {
            return nullptr;
        }
        std::memset(px, 0, (size_t)stride * image->height);
        image->bpp    = bpp;
        image->stride = stride;
        image->pixels = px;
        image->flags |= 1;
    }
    if (!image->palette && (image->depth == 4 || image->depth == 8)) {
        const size_t n = (size_t)(1 << image->depth) * 4;
        auto* pal = static_cast<uint8_t*>(rwNew(n, rw::MEMDUR_EVENT | rw::ID_IMAGE));
        if (!pal) {
            return nullptr;
        }
        std::memset(pal, 0, n);
        image->palette = pal;
        image->flags |= 2;
    }
    return image;
}

// A: librw's free() keeps the flags consistent; RW returns the image.
RwImage* RwImageFreePixels(RwImage* image) {
    if (!image) {
        return nullptr;
    }
    image->free();
    return image;
}

// A: d3d9 rule (RW's too): 32 bit images with any non-opaque pixel -> 8888, opaque -> 888; 24 -> 888; 16 -> 1555; palettized images stay PAL8 / PAL4
// only if the device takes P8 (it does not once the device is open, librw clears isP8supported) and are expanded to 32 bit otherwise.
// `nRasterType` is the raster type (rwRASTERTYPENORMAL / rwRASTERTYPETEXTURE); the type bits are OR'ed into *npFormat, so the result is directly
// usable as the `flags` of RwRasterCreate. Sizes are the image's (no power-of-two rounding). Returns the image, NULL if the depth is unusable.
RwImage* RwImageFindRasterFormat(RwImage* ipImage, RwInt32 nRasterType, RwInt32* npWidth, RwInt32* npHeight, RwInt32* npDepth, RwInt32* npFormat) {
    if (!ipImage || !ipImage->pixels || !npWidth || !npHeight || !npDepth || !npFormat) {
        return nullptr;
    }
    const int type = nRasterType & rwRASTERTYPEMASK;
    if (type != rwRASTERTYPENORMAL && type != rwRASTERTYPETEXTURE) {
        return nullptr;
    }
    RwInt32 w = 0, h = 0, d = 0, f = 0;
    if (!rw::Raster::imageFindRasterFormat(ipImage, rwRASTERTYPETEXTURE, &w, &h, &d, &f)) {
        return nullptr;
    }
    // The exe reports the depth of the raster format it picks (0x85C674 table: 8888 / 888 -> 32, 16 bit formats -> 16, LUM8 -> 8); librw says 24 for 888.
    if ((f & 0x0F00) == rwRASTERFORMAT8888 || (f & 0x0F00) == rwRASTERFORMAT888) {
        d = 32;
    }
    *npWidth  = w;
    *npHeight = h;
    *npDepth  = d;
    *npFormat = (f & ~rwRASTERTYPEMASK) | nRasterType;
    return ipImage;
}

// W: the image must already own pixels of the raster's size (RwImageAllocatePixels); converted from the raster's texels: any lockable format
// (8888, 888, 1555, 555, 565, 4444, LUM8, PAL8 / PAL4, DXT1/3/5 through librw's decompressor, camera rasters through the lock's read-back copy).
// Image depth 32: alpha from the raster (0xFF when it has none); 24: dropped; 16: ARGB1555; 8 / 4: needs a raster of the same kind (indices +
// palette copied). Returns NULL if sizes differ or the combination is impossible.
RwImage* RwImageSetFromRaster(RwImage* image, RwRaster* raster) {
    if (!image || !raster || !image->pixels || image->width != raster->width || image->height != raster->height) {
        return nullptr;
    }
    rw::Raster* root = raster->parent && raster->parent != raster ? raster->parent : raster;
    const rw::d3d::D3dRaster* nr = GETD3DRASTEREXT(root);
    if (!nr->texture && (root->type & rwRASTERTYPEMASK) != rwRASTERTYPECAMERA) {
        return nullptr;
    }
    if (nr->customFormat) {                         // DXT: librw decompresses to a 32 bit image
        RwImage* tmp = root->toImage();
        if (!tmp) {
            return nullptr;
        }
        const bool ok = ConvertInto(image, tmp);
        tmp->destroy();
        return ok ? image : nullptr;
    }
    const int fmt = raster->format;
    const bool pal = (fmt & (rwRASTERFORMATPAL8 | rwRASTERFORMATPAL4)) != 0;
    if (image->depth <= 8 && (!pal || !image->palette || !nr->palette)) {
        return nullptr;
    }
    if (!pal && rwshim::FormatBytes(fmt) != (int)nr->bpp) {
        return nullptr;
    }
    const RwUInt8* base = RwRasterLock(raster, 0, rwRASTERLOCKREAD);
    if (!base) {
        return nullptr;
    }
    const int stride = raster->stride;
    const int bpp = (int)nr->bpp;
    const auto* rpal = static_cast<const uint8_t*>(nr->palette);
    bool ok = true;
    if (image->depth <= 8) {
        const int n = (fmt & rwRASTERFORMATPAL4) ? 16 : 256, m = 1 << image->depth;
        for (int y = 0; y < image->height; ++y) {
            std::memcpy(image->pixels + (size_t)y * image->stride, base + y * stride, image->width);
        }
        std::memset(image->palette, 0, m * 4);
        std::memcpy(image->palette, rpal, (n < m ? n : m) * 4);
    } else {
        for (int y = 0; y < image->height && ok; ++y) {
            const uint8_t* row = base + y * stride;
            for (int x = 0; x < image->width; ++x) {
                Px p;
                if (pal) {
                    const uint8_t* c = rpal + row[x] * 4;
                    p = { c[0], c[1], c[2], c[3] };
                } else {
                    uint32_t t = 0;
                    std::memcpy(&t, row + x * bpp, bpp);
                    if (!rwshim::DecodeTexel(t, fmt, &p)) {
                        ok = false;
                        break;
                    }
                }
                rwshim::ImagePut(image, x, y, p);
            }
        }
    }
    RwRasterUnlock(raster);
    return ok ? image : nullptr;
}

// A: width / height / depth of `dst` must match `src` (RW asserts); dst must own pixels; 16 / 24 / 32 bit images convert freely, index images
// copy indices + palette to an index image with at least as many palette entries.
RwImage* RwImageCopy(RwImage* destImage, const RwImage* sourceImage) {
    return ConvertInto(destImage, sourceImage) ? destImage : nullptr;
}

// W: resizes in place with a box filter (shrinking) / bilinear (growing). Palettized images come out 32 bit (RW resamples in true colour, too).
RwImage* RwImageResize(RwImage* image, RwInt32 width, RwInt32 height) {
    if (!image || !image->pixels || width <= 0 || height <= 0) {
        return nullptr;
    }
    const int depth = image->depth <= 8 ? 32 : image->depth;
    RwImage* tmp = NewImage(width, height, depth);
    if (!tmp || !ResampleInto(tmp, image)) {
        if (tmp) RwImageDestroy(tmp);
        return nullptr;
    }
    image->free();
    if (image->depth <= 8) {
        image->palette = nullptr;
    }
    image->width = width; image->height = height; image->depth = depth;
    image->bpp = tmp->bpp; image->stride = tmp->stride;
    image->pixels = tmp->pixels; image->palette = nullptr; image->flags = 1;
    tmp->pixels = nullptr; tmp->flags = 0;
    RwImageDestroy(tmp);
    return image;
}

// W: dst keeps its size; true colour (16 / 24 / 32 bit) destination, any source depth.
RwImage* RwImageResample(RwImage* dstImage, const RwImage* srcImage) {
    if (!dstImage || !srcImage) {
        return nullptr;
    }
    return ResampleInto(dstImage, srcImage) ? dstImage : nullptr;
}

RwImage* RwImageCreateResample(const RwImage* srcImage, RwInt32 width, RwInt32 height) {
    if (!srcImage || !srcImage->pixels || width <= 0 || height <= 0) {
        return nullptr;
    }
    RwImage* dst = NewImage(width, height, srcImage->depth <= 8 ? 32 : srcImage->depth);
    if (!dst || !ResampleInto(dst, srcImage)) {
        if (dst) RwImageDestroy(dst);
        return nullptr;
    }
    return dst;
}

// A: both images need pixels and the same size; librw sets the alpha of `image` from the mask's alpha (index images: palette alpha), which
// RwImageMakeMask() derives from the colour intensity.
RwImage* RwImageApplyMask(RwImage* image, const RwImage* mask) {
    if (!image || !mask || !image->pixels || !mask->pixels || image->width != mask->width || image->height != mask->height) {
        return nullptr;
    }
    image->applyMask(const_cast<RwImage*>(mask));
    return image;
}

RwImage* RwImageMakeMask(RwImage* image) {
    if (!image || !image->pixels) {
        return nullptr;
    }
    image->makeMask();
    return image;
}

// D: `imageName` has no extension; librw tries the registered formats (tga, bmp, png) in that order through the search path of RwImageSetPath.
RwImage* RwImageRead(const RwChar* imageName) {
    return imageName ? rw::Image::read(imageName) : nullptr;
}

RwImage* RwImageReadMaskedImage(const RwChar* imageName, const RwChar* maskname) {
    return imageName ? rw::Image::readMasked(imageName, maskname) : nullptr;
}

// A: format by the extension of `imageName` (bmp, png, tga); NULL for others or if the file cannot be written.
RwImage* RwImageWrite(RwImage* image, const RwChar* imageName) {
    if (!image || !imageName) {
        return nullptr;
    }
    const char* dot = std::strrchr(imageName, '.');
    if (!dot) {
        return nullptr;
    }
    if (!_stricmp(dot, ".bmp")) return RtBMPImageWrite(image, imageName);
    if (!_stricmp(dot, ".png")) return RtPNGImageWrite(image, imageName);
    if (!_stricmp(dot, ".tga")) {
        RwImage* t = MakeTightCopy(image);
        if (!t) return nullptr;
        rw::writeTGA(t, imageName);
        t->destroy();
        return FileExists(imageName) ? image : nullptr;
    }
    return nullptr;
}

// A: librw splits the list on ';' and prepends each entry to the name, so entries end with a separator. RW keeps the string for GetPath.
const RwChar* RwImageSetPath(const RwChar* path) {
    if (!path) {
        s_ImagePath[0] = 0;
        rw::Image::setSearchPath(nullptr);
        return nullptr;
    }
    std::strncpy(s_ImagePath, path, sizeof(s_ImagePath) - 1);
    s_ImagePath[sizeof(s_ImagePath) - 1] = 0;
    rw::Image::setSearchPath(s_ImagePath);
    return path;
}

RwChar* RwImageGetPath(void) {
    return s_ImagePath;
}

// W: BMP 1 / 4 / 8 bit paletted (1 bit becomes a 4 bit image), 24 bit, 32 bit (BI_RGB or BI_BITFIELDS with the standard BGRA masks; alpha only
// from a V4+ header with an alpha mask), uncompressed, bottom-up or top-down. 4 bit images use one byte per pixel (see the layout note).
// NULL if the file is missing or not such a BMP.
RwImage* RtBMPImageRead(const RwChar* imageName) {
    std::vector<uint8_t> f;
    if (!ReadFile(imageName, f) || f.size() < 26 || f[0] != 'B' || f[1] != 'M') {
        return nullptr;
    }
    const uint32_t dataOff = Le32(&f[10]);
    const uint32_t hdr = Le32(&f[14]);
    int32_t  w, h;
    unsigned bits, comp = 0, clrUsed = 0;
    uint32_t masks[4] = { 0, 0, 0, 0 };
    if (hdr == 12) {
        w = Le16(&f[18]); h = Le16(&f[20]); bits = Le16(&f[24]);
    } else if (hdr >= 40 && f.size() >= 14 + hdr) {
        w = (int32_t)Le32(&f[18]); h = (int32_t)Le32(&f[22]); bits = Le16(&f[28]); comp = Le32(&f[30]); clrUsed = Le32(&f[46]);
        if (comp == 3 && f.size() >= 14 + 40 + 12) {
            for (int i = 0; i < 3; ++i) masks[i] = Le32(&f[14 + 40 + i * 4]);
            if (hdr >= 56) masks[3] = Le32(&f[14 + 52]);
        }
    } else {
        return nullptr;
    }
    const bool topDown = h < 0;
    if (topDown) h = -h;
    if (w <= 0 || h <= 0 || w > 32768 || h > 32768) {
        return nullptr;
    }
    bool hasAlpha = false;
    if (comp == 3 && bits == 32) {
        if (masks[0] != 0x00FF0000 || masks[1] != 0x0000FF00 || masks[2] != 0x000000FF) return nullptr;
        hasAlpha = masks[3] == 0xFF000000;
    } else if (comp != 0) {
        return nullptr;
    }
    if (bits != 1 && bits != 4 && bits != 8 && bits != 24 && bits != 32) {
        return nullptr;
    }
    const int depth = bits == 1 ? 4 : (int)bits;
    const size_t rowBytes = (((size_t)w * bits + 31) / 32) * 4;
    if (dataOff > f.size() || rowBytes * (size_t)h > f.size() - dataOff) {
        return nullptr;
    }
    RwImage* img = NewImage(w, h, depth);
    if (!img) {
        return nullptr;
    }
    if (bits <= 8) {
        const size_t entry = hdr == 12 ? 3 : 4;
        const size_t nPal = clrUsed ? clrUsed : (size_t)1 << bits;
        const size_t palOff = 14 + hdr;
        for (size_t i = 0; i < nPal && i < (size_t)(1 << depth) && palOff + (i + 1) * entry <= f.size(); ++i) {
            const uint8_t* e = &f[palOff + i * entry];
            img->palette[i * 4 + 0] = e[2]; img->palette[i * 4 + 1] = e[1]; img->palette[i * 4 + 2] = e[0]; img->palette[i * 4 + 3] = 0xFF;
        }
    }
    for (int y = 0; y < h; ++y) {
        const uint8_t* src = &f[dataOff + rowBytes * (size_t)(topDown ? y : h - 1 - y)];
        uint8_t* dst = img->pixels + (size_t)y * img->stride;
        for (int x = 0; x < w; ++x) {
            switch (bits) {
            case 1: dst[x] = (src[x >> 3] >> (7 - (x & 7))) & 1; break;
            case 4: dst[x] = (x & 1) ? (src[x >> 1] & 0xF) : (src[x >> 1] >> 4); break;
            case 8: dst[x] = src[x]; break;
            case 24: dst[x * 3 + 0] = src[x * 3 + 2]; dst[x * 3 + 1] = src[x * 3 + 1]; dst[x * 3 + 2] = src[x * 3 + 0]; break;
            case 32: dst[x * 4 + 0] = src[x * 4 + 2]; dst[x * 4 + 1] = src[x * 4 + 1]; dst[x * 4 + 2] = src[x * 4 + 0];
                     dst[x * 4 + 3] = hasAlpha ? src[x * 4 + 3] : 0xFF; break;
            }
        }
    }
    return img;
}

// W: uncompressed bottom-up BMP: 4 / 8 bit with palette, 24 bit; 32 bit and 16 bit images are written as 24 bit (the alpha channel is dropped, as
// in RW, BMP has no alpha). Returns the image, NULL if the file cannot be created.
RwImage* RtBMPImageWrite(RwImage* image, const RwChar* imageName) {
    if (!image || !image->pixels || !imageName || image->width <= 0 || image->height <= 0) {
        return nullptr;
    }
    const int depth = image->depth;
    const int outBits = depth == 4 ? 4 : (depth == 8 ? 8 : 24);
    if ((depth == 4 || depth == 8) && !image->palette) {
        return nullptr;
    }
    const int pallen = outBits <= 8 ? (1 << outBits) : 0;
    const size_t rowBytes = (((size_t)image->width * outBits + 31) / 32) * 4;
    const uint32_t dataOff = 14 + 40 + (uint32_t)pallen * 4;
    std::FILE* f = std::fopen(imageName, "wb");
    if (!f) {
        return nullptr;
    }
    std::fputc('B', f); std::fputc('M', f);
    Put32(f, dataOff + (uint32_t)(rowBytes * image->height));
    Put32(f, 0);
    Put32(f, dataOff);
    Put32(f, 40); Put32(f, (uint32_t)image->width); Put32(f, (uint32_t)image->height);
    Put16(f, 1); Put16(f, (unsigned)outBits); Put32(f, 0); Put32(f, (uint32_t)(rowBytes * image->height));
    Put32(f, 2835); Put32(f, 2835); Put32(f, (uint32_t)pallen); Put32(f, 0);
    for (int i = 0; i < pallen; ++i) {
        const uint8_t* c = image->palette + i * 4;
        std::fputc(c[2], f); std::fputc(c[1], f); std::fputc(c[0], f); std::fputc(0, f);
    }
    std::vector<uint8_t> row(rowBytes);
    for (int y = image->height - 1; y >= 0; --y) {
        std::fill(row.begin(), row.end(), 0);
        const uint8_t* src = image->pixels + (size_t)y * image->stride;
        for (int x = 0; x < image->width; ++x) {
            if (outBits == 4) {
                row[x >> 1] |= (uint8_t)((src[x] & 0xF) << ((x & 1) ? 0 : 4));
            } else if (outBits == 8) {
                row[x] = src[x];
            } else {
                const Px p = rwshim::ImageGet(image, x, y);
                row[x * 3 + 0] = p.b; row[x * 3 + 1] = p.g; row[x * 3 + 2] = p.r;
            }
        }
        std::fwrite(row.data(), 1, row.size(), f);
    }
    const bool ok = std::fflush(f) == 0;
    std::fclose(f);
    return ok ? image : nullptr;
}

// A: librw's reader asserts on a missing file.
RwImage* RtPNGImageRead(const RwChar* imageName) {
    if (!FileExists(imageName)) {
        return nullptr;
    }
    return rw::readPNG(imageName);
}

// A: lodepng gets a tight copy (our rows are dword padded; 16 bit images become 32 bit without touching the original, librw's writer would
// convert in place). RW returns the image on success; librw's writer returns void, so success = the file exists afterwards.
RwImage* RtPNGImageWrite(RwImage* image, const RwChar* imageName) {
    if (!image || !image->pixels || !imageName) {
        return nullptr;
    }
    RwImage* t = MakeTightCopy(image);
    if (!t) {
        return nullptr;
    }
    std::remove(imageName);
    rw::writePNG(t, imageName);
    t->destroy();
    return FileExists(imageName) ? image : nullptr;
}
#endif
