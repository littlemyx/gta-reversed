// P2B-03a unit test: RwRaster* / RwImage* / RtBMP* / RtPNG* shim over librw/D3D9.
//  1. RwImage: creation / dword aligned stride, BMP + PNG round trips (odd widths, 4/8/24/32 bit, palettes), search-path read, copy / resample /
//     mask, RwRGBAToPixel
//  2. (needs a D3D9 HAL adapter; works under Wine/wined3d) rasters of every format the game creates: lock / write / unlock / read back, stride,
//     double lock, mip levels, sub rasters, image -> raster -> image round trips, camera texture clear / RenderFast / Render / RenderScaled
//  3. optional argv[1]: a real BMP (SA's models/txd/title_pc_US.bmp, 1024x1024 8 bit) is read, converted to a raster and compared
// Exit code 0 = all checks passed.
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

// engine globals (rwglobals.cpp needs the game PCH): only the members frame.cpp / camera.cpp touch
static RwGlobals s_RwGlobals{};
RwGlobals* RwEngineInstance = &s_RwGlobals;
bool RwInitialized = false;

static int g_fail = 0;
#define CHECK(c) do { const bool ok_ = !!(c); std::printf("%-4s %s\n", ok_ ? "ok" : "FAIL", #c); if (!ok_) ++g_fail; } while (0)
#define CHECKV(c, fmt, ...) do { const bool ok_ = !!(c); std::printf("%-4s %s   " fmt "\n", ok_ ? "ok" : "FAIL", #c, __VA_ARGS__); if (!ok_) ++g_fail; } while (0)

static char g_tmp[512];
static const char* TmpFile(const char* name) {
    char dir[400];
    GetTempPathA(sizeof(dir), dir);
    std::snprintf(g_tmp, sizeof(g_tmp), "%s%s", dir, name);
    return g_tmp;
}

// image with a deterministic pattern: r = 7x+3y, g = 5x+11y+1, b = 13x^y, a = (x*37+y*11) (or 255)
static void Pattern(int x, int y, unsigned char* c, bool alpha) {
    c[0] = (unsigned char)(7 * x + 3 * y);
    c[1] = (unsigned char)(5 * x + 11 * y + 1);
    c[2] = (unsigned char)(13 * (x ^ y));
    c[3] = alpha ? (unsigned char)(x * 37 + y * 11 + 4) : 255;
}

static RwImage* MakePatternImage(int w, int h, int depth, bool alpha = false) {
    RwImage* img = RwImageCreate(w, h, depth);
    if (!img || !RwImageAllocatePixels(img)) return nullptr;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            unsigned char c[4];
            Pattern(x, y, c, alpha);
            unsigned char* p = img->pixels + y * img->stride;
            switch (depth) {
            case 32: std::memcpy(p + x * 4, c, 4); break;
            case 24: std::memcpy(p + x * 3, c, 3); break;
            case 16: { unsigned v = ((c[3] & 0x80) << 8) | ((c[0] & 0xF8) << 7) | ((c[1] & 0xF8) << 2) | (c[2] >> 3); p[x * 2] = v & 0xFF; p[x * 2 + 1] = v >> 8; break; }
            case 8: p[x] = (unsigned char)((x * 3 + y * 5) % 256); break;
            case 4: p[x] = (unsigned char)((x + 2 * y) % 16); break;
            }
        }
    if (depth <= 8) {
        for (int i = 0; i < (1 << depth); ++i) {
            img->palette[i * 4 + 0] = (unsigned char)(i * 3 + 1);
            img->palette[i * 4 + 1] = (unsigned char)(255 - i);
            img->palette[i * 4 + 2] = (unsigned char)(i * 7);
            img->palette[i * 4 + 3] = 255;
        }
    }
    return img;
}

static bool SameImage(const RwImage* a, const RwImage* b) {
    if (!a || !b || a->width != b->width || a->height != b->height || a->depth != b->depth) return false;
    const int bpp = a->depth < 8 ? 1 : a->depth / 8;
    for (int y = 0; y < a->height; ++y)
        if (std::memcmp(a->pixels + y * a->stride, b->pixels + y * b->stride, a->width * bpp)) return false;
    if (a->depth <= 8) return std::memcmp(a->palette, b->palette, (1 << a->depth) * 4) == 0;
    return true;
}

static unsigned ReadPixel(IDirect3DDevice9* dev, int x, int y) {
    IDirect3DSurface9 *rt = nullptr, *sys = nullptr;
    unsigned px = 0xDEADBEEF;
    if (SUCCEEDED(dev->GetRenderTarget(0, &rt))) {
        D3DSURFACE_DESC d{};
        rt->GetDesc(&d);
        if (SUCCEEDED(dev->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr)) && SUCCEEDED(dev->GetRenderTargetData(rt, sys))) {
            D3DLOCKED_RECT lr{};
            if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
                px = *(unsigned*)((char*)lr.pBits + y * lr.Pitch + x * 4) | 0xFF000000u;
                sys->UnlockRect();
            }
        }
        if (sys) sys->Release();
        rt->Release();
    }
    return px;
}

//--------------------------------------------------------------------------------------------------
static void ImageTests() {
    std::printf("--- RwImage\n");
    // creation / stride / palette
    RwImage* i24 = RwImageCreate(5, 3, 24);
    CHECK(i24 && i24->pixels == nullptr && i24->stride == 0 && i24->depth == 24 && i24->width == 5 && i24->height == 3);
    CHECK(RwImageAllocatePixels(i24) == i24 && i24->stride == 16 && i24->pixels && i24->palette == nullptr);       // 15 -> 16
    CHECK(RwImageGetStride(i24) == 16 && RwImageGetWidth(i24) == 5 && RwImageGetHeight(i24) == 3 && RwImageGetDepth(i24) == 24 && RwImageGetPixels(i24) == i24->pixels);
    RwImage* i32 = RwImageCreate(3, 2, 32); RwImageAllocatePixels(i32);
    CHECK(i32->stride == 12 && i32->palette == nullptr);
    RwImage* i8 = RwImageCreate(5, 2, 8); RwImageAllocatePixels(i8);
    CHECK(i8->stride == 8 && i8->palette != nullptr && RwImageGetPalette(i8) == i8->palette);
    RwImage* i4 = RwImageCreate(7, 2, 4); RwImageAllocatePixels(i4);
    CHECK(i4->stride == 8 && i4->palette != nullptr);
    CHECK(RwImageCreate(0, 4, 32) == nullptr && RwImageCreate(4, 4, 12) == nullptr);
    CHECK(RwImageFreePixels(i24) == i24 && i24->pixels == nullptr);
    CHECK(RwImageAllocatePixels(i24) == i24 && i24->pixels != nullptr);
    // SetPixels / SetStride / SetPalette only store (not owned)
    static unsigned char ext[64]; static unsigned char pal[1024];
    RwImage* ex = RwImageCreate(4, 4, 8);
    RwImageSetPixels(ex, ext); RwImageSetStride(ex, 4); RwImageSetPalette(ex, (RwRGBA*)pal);
    CHECK(ex->pixels == ext && ex->stride == 4 && ex->palette == pal);
    CHECK(RwImageDestroy(ex) == TRUE);                      // must not free the static buffers (would crash)
    CHECK(RwImageDestroy(nullptr) == FALSE);

    // RwRGBAToPixel
    RwRGBA c{0x12, 0x34, 0x56, 0x78};
    CHECK(RwRGBAToPixel(&c, rwRASTERFORMAT8888) == 0x78123456u);
    CHECK(RwRGBAToPixel(&c, rwRASTERFORMAT888) == 0xFF123456u);
    CHECK(RwRGBAToPixel(&c, rwRASTERFORMAT1555) == 0x08CAu);
    CHECK(RwRGBAToPixel(&c, rwRASTERFORMAT565) == (unsigned)(((0x12 & 0xF8) << 8) | ((0x34 & 0xFC) << 3) | (0x56 >> 3)));
    CHECK(RwRGBAToPixel(&c, rwRASTERFORMAT4444) == 0x7135u);
    RwRGBA white{255, 255, 255, 255};
    // exe 0x7FEE20: format 0 is encoded as 8888, only bits 8-11 of the format count, 555 has no alpha bit, LUM8 = (r*30+g*59+b*11)/100 * a / 255
    CHECK(RwRGBAToPixel(&c, 0) == 0x78123456u && RwRGBAToPixel(&c, rwRASTERFORMAT8888 | rwRASTERFORMATPAL8 | rwRASTERFORMATMIPMAP) == 0x78123456u);
    CHECK(RwRGBAToPixel(&c, rwRASTERFORMAT555) == (unsigned)(((0x12 & 0xF8) << 7) | ((0x34 & 0xF8) << 2) | (0x56 >> 3)));
    {
        RwRGBA lum{200, 100, 50, 128};                   // (6000 + 5900 + 550) / 100 = 124; 124 * 128 / 255 = 62
        RwRGBA lumOpaque{200, 100, 50, 255};
        CHECK(RwRGBAToPixel(&lum, rwRASTERFORMATLUM8) == 62u && RwRGBAToPixel(&lumOpaque, rwRASTERFORMATLUM8) == 124u);
    }
    CHECK(RwRGBAToPixel(&white, rwRASTERFORMAT1555) == 0xFFFFu && RwRGBAToPixel(&white, rwRASTERFORMAT565) == 0xFFFFu && RwRGBAToPixel(&white, rwRASTERFORMAT4444) == 0xFFFFu);

    // BMP: odd widths so the row padding matters, every depth
    const struct { int w, h, depth; bool alpha; } cases[] = { {5, 3, 24, false}, {4, 4, 24, false}, {7, 5, 8, false}, {7, 3, 4, false}, {9, 2, 4, false}, {6, 6, 32, false}, {3, 3, 16, false} };
    for (const auto& k : cases) {
        RwImage* a = MakePatternImage(k.w, k.h, k.depth, k.alpha);
        const char* f = TmpFile("rw_raster_image_test.bmp");
        std::remove(f);
        RwImage* w = RtBMPImageWrite(a, f);
        RwImage* r = RtBMPImageRead(f);
        std::printf("  bmp %dx%d depth %d: write %s, read %s\n", k.w, k.h, k.depth, w ? "ok" : "NULL", r ? "ok" : "NULL");
        CHECK(w == a && r);
        if (k.depth == 32 || k.depth == 16) {                // written as 24 bit, alpha dropped / 1555 expanded: compare colours
            CHECK(r && r->depth == 24 && r->width == k.w && r->height == k.h);
            bool ok = r != nullptr;
            for (int y = 0; r && y < k.h; ++y)
                for (int x = 0; x < k.w; ++x) {
                    unsigned char exp[3], got[3];
                    if (k.depth == 32) std::memcpy(exp, a->pixels + y * a->stride + x * 4, 3);
                    else { const unsigned v = a->pixels[y * a->stride + x * 2] | (a->pixels[y * a->stride + x * 2 + 1] << 8);
                           exp[0] = (unsigned char)(((v >> 10) & 31) * 255 / 31); exp[1] = (unsigned char)(((v >> 5) & 31) * 255 / 31); exp[2] = (unsigned char)((v & 31) * 255 / 31); }
                    std::memcpy(got, r->pixels + y * r->stride + x * 3, 3);
                    ok = ok && !std::memcmp(exp, got, 3);
                }
            CHECK(ok);
        } else {
            CHECK(SameImage(a, r));
        }
        if (r) {
            // file layout: bottom-up rows, rows padded to 4 bytes
            std::FILE* fp = std::fopen(f, "rb");
            std::fseek(fp, 0, SEEK_END);
            const long len = std::ftell(fp);
            std::fclose(fp);
            const int ob = k.depth == 4 ? 4 : (k.depth == 8 ? 8 : 24);
            const long expect = 14 + 40 + (ob <= 8 ? (1 << ob) * 4 : 0) + (long)((k.w * ob + 31) / 32) * 4 * k.h;
            CHECKV(len == expect, "len %ld expect %ld", len, expect);
        }
        if (r) RwImageDestroy(r);
        RwImageDestroy(a);
    }
    CHECK(RtBMPImageRead(TmpFile("no_such_file.bmp")) == nullptr);
    {   // top-down BMP (negative height), handcrafted: 2x2 24 bit, rows padded to 8
        unsigned char b[14 + 40 + 16] = { 'B', 'M', 0x46, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0, 2, 0, 0, 0, 0xFE, 0xFF, 0xFF, 0xFF, 1, 0, 24, 0 };
        unsigned char* px = b + 54;
        const unsigned char row0[6] = { 3, 2, 1, 6, 5, 4 }, row1[6] = { 9, 8, 7, 12, 11, 10 };     // BGR
        std::memcpy(px, row0, 6); std::memcpy(px + 8, row1, 6);
        const char* f = TmpFile("rw_raster_image_td.bmp");
        std::FILE* fp = std::fopen(f, "wb"); std::fwrite(b, 1, sizeof(b), fp); std::fclose(fp);
        RwImage* r = RtBMPImageRead(f);
        CHECK(r && r->width == 2 && r->height == 2 && r->depth == 24);
        CHECK(r && r->pixels[0] == 1 && r->pixels[1] == 2 && r->pixels[2] == 3 && r->pixels[3] == 4 && r->pixels[r->stride] == 7 && r->pixels[r->stride + 3] == 10);
        if (r) RwImageDestroy(r);
    }

    // PNG
    const struct { int w, h, depth; bool alpha; } pcases[] = { {5, 3, 24, false}, {6, 4, 32, true}, {7, 5, 8, false}, {8, 2, 4, false}, {3, 3, 16, false} };
    for (const auto& k : pcases) {
        RwImage* a = MakePatternImage(k.w, k.h, k.depth, k.alpha);
        const char* f = TmpFile("rw_raster_image_test.png");
        RwImage* w = RtPNGImageWrite(a, f);
        RwImage* r = RtPNGImageRead(f);
        std::printf("  png %dx%d depth %d: write %s, read %s (depth %d)\n", k.w, k.h, k.depth, w ? "ok" : "NULL", r ? "ok" : "NULL", r ? r->depth : -1);
        CHECK(w == a && r);
        if (k.depth == 16) {
            CHECK(r && (r->depth == 32 || r->depth == 24) && r->width == k.w);       // lodepng drops an all-opaque alpha channel
            bool ok = r != nullptr;
            for (int y = 0; r && y < k.h; ++y)
                for (int x = 0; x < k.w; ++x) {
                    const unsigned v = a->pixels[y * a->stride + x * 2] | (a->pixels[y * a->stride + x * 2 + 1] << 8);
                    const unsigned char* g = r->pixels + y * r->stride + x * (r->depth / 8);
                    ok = ok && g[0] == (unsigned char)(((v >> 10) & 31) * 255 / 31) && g[1] == (unsigned char)(((v >> 5) & 31) * 255 / 31) &&
                         g[2] == (unsigned char)((v & 31) * 255 / 31) && (r->depth == 24 || g[3] == ((v & 0x8000) ? 255 : 0));
                }
            CHECK(ok);
            CHECK(a->depth == 16);                          // the original was not converted in place
        } else if (k.depth == 4) {
            CHECK(SameImage(a, r) || (r && r->depth == 4));  // lodepng may keep 4 bit; palette order is the file's
            if (r && r->depth == 4) CHECK(SameImage(a, r));
        } else {
            CHECK(SameImage(a, r));
        }
        if (r) RwImageDestroy(r);
        RwImageDestroy(a);
    }
    CHECK(RtPNGImageRead(TmpFile("no_such_file.png")) == nullptr);                 // librw asserts here, shim returns NULL
    CHECK(RtPNGImageWrite(nullptr, TmpFile("x.png")) == nullptr);

    // path + extension search (RwImageRead) and RwImageWrite by extension
    {
        char dir[400]; GetTempPathA(sizeof(dir), dir);
        RwImage* a = MakePatternImage(6, 6, 24);
        CHECK(RwImageWrite(a, TmpFile("rw_raster_image_search.bmp")) == a);
        CHECK(RwImageWrite(a, TmpFile("rw_raster_image_search.png")) == a);
        CHECK(RwImageWrite(a, TmpFile("rw_raster_image_search.xyz")) == nullptr);
        CHECK(RwImageSetPath(dir) == dir && std::strcmp(RwImageGetPath(), dir) == 0);
        RwImage* r = RwImageRead("rw_raster_image_search");
        CHECK(r && SameImage(a, r));
        if (r) RwImageDestroy(r);
        CHECK(RwImageRead("rw_raster_image_does_not_exist") == nullptr);
        RwImage* m = RwImageReadMaskedImage("rw_raster_image_search", "rw_raster_image_search");   // mask = image itself: alpha = max channel
        CHECK(m && m->depth == 32);
        if (m) {
            bool ok = true;
            for (int y = 0; y < 6; ++y) for (int x = 0; x < 6; ++x) {
                unsigned char c[4]; Pattern(x, y, c, false);
                const unsigned char mx = c[0] > c[1] ? (c[0] > c[2] ? c[0] : c[2]) : (c[1] > c[2] ? c[1] : c[2]);
                ok = ok && m->pixels[y * m->stride + x * 4 + 3] == mx && m->pixels[y * m->stride + x * 4] == c[0];
            }
            CHECK(ok);
            RwImageDestroy(m);
        }
        RwImageSetPath(nullptr);
        CHECK(RwImageGetPath()[0] == 0);
        RwImageDestroy(a);
    }

    // copy / resize / resample / mask
    {
        RwImage* a = MakePatternImage(5, 4, 24);
        RwImage* b = RwImageCreate(5, 4, 32); RwImageAllocatePixels(b);
        CHECK(RwImageCopy(b, a) == b);
        bool ok = true;
        for (int y = 0; y < 4; ++y) for (int x = 0; x < 5; ++x) ok = ok && !std::memcmp(a->pixels + y * a->stride + x * 3, b->pixels + y * b->stride + x * 4, 3) && b->pixels[y * b->stride + x * 4 + 3] == 255;
        CHECK(ok);
        RwImage* bad = RwImageCreate(4, 4, 32); RwImageAllocatePixels(bad);
        CHECK(RwImageCopy(bad, a) == nullptr);
        RwImageDestroy(bad);
        // solid colour stays solid through resample (up and down)
        RwImage* s = RwImageCreate(4, 4, 32); RwImageAllocatePixels(s);
        for (int i = 0; i < 16; ++i) { s->pixels[i * 4] = 10; s->pixels[i * 4 + 1] = 200; s->pixels[i * 4 + 2] = 77; s->pixels[i * 4 + 3] = 255; }
        RwImage* up = RwImageCreateResample(s, 16, 8);
        CHECK(up && up->width == 16 && up->height == 8 && up->pixels[0] == 10 && up->pixels[(7 * 16 + 15) * 4 + 1] == 200 && up->pixels[(3 * 16 + 5) * 4 + 2] == 77);
        CHECK(RwImageResize(s, 2, 2) == s && s->width == 2 && s->height == 2 && s->pixels[3 * 4 + 1] == 200);
        // 2x2 -> 1x1 box filter averages
        RwImage* q = RwImageCreate(2, 2, 32); RwImageAllocatePixels(q);
        const unsigned char qv[4] = { 0, 100, 200, 255 };
        for (int i = 0; i < 4; ++i) { q->pixels[i * 4] = qv[i]; q->pixels[i * 4 + 1] = 0; q->pixels[i * 4 + 2] = 0; q->pixels[i * 4 + 3] = 255; }
        RwImage* one = RwImageCreateResample(q, 1, 1);
        CHECKV(one && one->pixels[0] == 139, "avg %d", one ? one->pixels[0] : -1);   // (0+100+200+255)/4 = 138.75 -> 139
        // mask: makeMask copies the biggest channel to alpha, applyMask copies mask alpha
        RwImage* mk = MakePatternImage(5, 4, 32);
        RwImage* msk = MakePatternImage(5, 4, 32);
        CHECK(RwImageMakeMask(msk) == msk);
        CHECK(RwImageApplyMask(mk, msk) == mk && mk->pixels[3] == msk->pixels[3]);
        RwImage* wrongSize = RwImageCreate(2, 2, 32); RwImageAllocatePixels(wrongSize);
        CHECK(RwImageApplyMask(mk, wrongSize) == nullptr);
        for (RwImage* z : { a, b, s, up, q, one, mk, msk, wrongSize }) if (z) RwImageDestroy(z);
    }
}

//--------------------------------------------------------------------------------------------------
struct FmtCase { const char* name; int flags; int w, h; int depth; int bytes; };

static void RasterFormatTests() {
    std::printf("--- rasters: every format the game creates\n");
    const FmtCase cases[] = {
        { "8888 texture (grain)", rwRASTERFORMAT8888 | rwRASTERTYPETEXTURE, 16, 8, 32, 4 },
        { "888 | PIXELLOCKEDWRITE (plate)", rwRASTERFORMAT888 | rwRASTERPIXELLOCKEDWRITE, 64, 16, 32, 4 },
        { "8888 | PIXELLOCKEDWRITE (sign)", rwRASTERFORMAT8888 | rwRASTERPIXELLOCKEDWRITE, 24, 8, 32, 4 },
        { "1555 texture", rwRASTERFORMAT1555 | rwRASTERTYPETEXTURE, 8, 8, 16, 2 },
        { "565 texture", rwRASTERFORMAT565 | rwRASTERTYPETEXTURE, 8, 8, 16, 2 },
        { "4444 texture", rwRASTERFORMAT4444 | rwRASTERTYPETEXTURE, 8, 8, 16, 2 },
        { "555 texture", rwRASTERFORMAT555 | rwRASTERTYPETEXTURE, 8, 8, 16, 2 },
        { "8888 NORMAL type", rwRASTERFORMAT8888 | rwRASTERTYPENORMAL, 8, 4, 32, 4 },
    };
    for (const auto& k : cases) {
        std::printf("  [%s]\n", k.name);
        RwRaster* r = RwRasterCreate(k.w, k.h, k.depth, k.flags);
        CHECK(r != nullptr);
        if (!r) continue;
        CHECK(RwRasterGetWidth(r) == k.w && RwRasterGetHeight(r) == k.h && RwRasterGetDepth(r) == k.depth);
        CHECK(((RwRasterGetFormat(r)) == (k.flags & rwRASTERFORMATPIXELFORMATMASK)) && ((RwRasterGetType(r) == (k.flags & rwRASTERTYPEMASK))));
        CHECK(RwRasterGetParent(r) == r);
        CHECK(RwRasterGetStride(r) == k.w * k.bytes);                      // before lock: the packed pitch
        RwUInt8* p = RwRasterLock(r, 0, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
        CHECK(p != nullptr);
        if (!p) { RwRasterDestroy(r); continue; }
        const int stride = RwRasterGetStride(r);
        CHECKV(stride >= k.w * k.bytes && (r->privateFlags & rwRASTERPIXELLOCKEDWRITE), "stride %d", stride);
        CHECK(RwRasterLock(r, 0, rwRASTERLOCKREAD) == nullptr);            // already locked
        for (int y = 0; y < k.h; ++y)
            for (int x = 0; x < k.w * k.bytes; ++x) p[y * stride + x] = (unsigned char)(x * 7 + y * 13 + 1);
        CHECK(RwRasterUnlock(r) == r && !(r->privateFlags & (rwRASTERPIXELLOCKEDREAD | rwRASTERPIXELLOCKEDWRITE)));
        CHECK(RwRasterGetStride(r) == k.w * k.bytes);                      // restored
        CHECK(RwRasterUnlock(r) == r);                                     // not locked: no-op (librw would crash)
        p = RwRasterLock(r, 0, rwRASTERLOCKREAD);
        CHECK(p != nullptr);
        bool same = p != nullptr;
        for (int y = 0; p && y < k.h; ++y)
            for (int x = 0; x < k.w * k.bytes; ++x) same = same && p[y * RwRasterGetStride(r) + x] == (unsigned char)(x * 7 + y * 13 + 1);
        CHECK(same);
        RwRasterUnlock(r);
        CHECK(RwRasterGetNumLevels(r) == 1);
        CHECK(RwRasterLock(r, 1, rwRASTERLOCKREAD) == nullptr);            // no such level
        CHECK(RwRasterDestroy(r) == TRUE);
    }
    CHECK(RwRasterCreate(-1, 8, 32, rwRASTERFORMAT8888 | rwRASTERTYPETEXTURE) == nullptr);

    // mip chain (RwTextureRead sets MIPMAP | AUTOMIPMAP; AUTOMIPMAP alone means the same in RW)
    RwRaster* m = RwRasterCreate(64, 32, 0, rwRASTERFORMAT8888 | rwRASTERFORMATAUTOMIPMAP | rwRASTERTYPETEXTURE);
    CHECK(m != nullptr);
    if (m) {
        // D3DUSAGE_AUTOGENMIPMAP textures expose one level to the API (the chain is generated by the driver)
        CHECKV(GETD3DRASTEREXT(m)->autogenMipmap && RwRasterGetNumLevels(m) == 1, "levels %d autogen %d", RwRasterGetNumLevels(m), (int)GETD3DRASTEREXT(m)->autogenMipmap);
        CHECK(RwRasterLock(m, 1, rwRASTERLOCKREAD) == nullptr);
        RwUInt8* l0 = RwRasterLock(m, 0, rwRASTERLOCKWRITE);
        CHECK(l0 != nullptr);
        RwRasterUnlock(m);
        CHECK(RwRasterGetWidth(m) == 64 && RwRasterGetHeight(m) == 32);
        RwRasterDestroy(m);
    }
    RwRaster* explicitMips = RwRasterCreate(16, 16, 0, rwRASTERFORMAT8888 | rwRASTERFORMATMIPMAP | rwRASTERTYPETEXTURE);
    CHECK(explicitMips && RwRasterGetNumLevels(explicitMips) == 5);
    if (explicitMips) RwRasterDestroy(explicitMips);

    // DONTALLOCATE, sub rasters
    std::printf("  [sub rasters]\n");
    RwRaster* parent = RwRasterCreate(16, 16, 32, rwRASTERFORMAT8888 | rwRASTERTYPETEXTURE);
    RwRaster* sub = RwRasterCreate(0, 0, 0, rwRASTERTYPETEXTURE | rwRASTERDONTALLOCATE);
    CHECK(parent && sub && RwRasterLock(sub, 0, rwRASTERLOCKREAD) == nullptr);
    RwRect bad{ 10, 10, 8, 8 }, good{ 4, 6, 8, 5 };
    CHECK(RwRasterSubRaster(sub, parent, &bad) == nullptr);
    CHECK(RwRasterSubRaster(parent, parent, &good) == nullptr);       // not DONTALLOCATE
    if (parent && sub) {
        CHECK(RwRasterSubRaster(sub, parent, &good) == sub);
        RwInt16 ox = -1, oy = -1;
        CHECK(RwRasterGetOffset(sub, &ox, &oy) == sub && ox == 4 && oy == 6);
        CHECK(RwRasterGetWidth(sub) == 8 && RwRasterGetHeight(sub) == 5 && RwRasterGetParent(sub) == parent && RwRasterGetDepth(sub) == 32 && RwRasterGetFormat(sub) == rwRASTERFORMAT8888);
        RwUInt8* pp = RwRasterLock(parent, 0, rwRASTERLOCKWRITE);
        const int stride = RwRasterGetStride(parent);
        std::memset(pp, 0, stride * 16);
        RwRasterUnlock(parent);
        RwUInt8* sp = RwRasterLock(sub, 0, rwRASTERLOCKWRITE);
        CHECK(sp && RwRasterLock(parent, 0, rwRASTERLOCKREAD) == nullptr);       // the parent is locked through the sub raster
        if (sp) { for (int y = 0; y < 5; ++y) for (int x = 0; x < 8; ++x) *(unsigned*)(sp + y * RwRasterGetStride(sub) + x * 4) = 0xFF00FF00u; }
        RwRasterUnlock(sub);
        pp = RwRasterLock(parent, 0, rwRASTERLOCKREAD);
        bool ok = pp != nullptr;
        for (int y = 0; pp && y < 16; ++y) for (int x = 0; x < 16; ++x) {
            const bool inside = x >= 4 && x < 12 && y >= 6 && y < 11;
            ok = ok && *(unsigned*)(pp + y * RwRasterGetStride(parent) + x * 4) == (inside ? 0xFF00FF00u : 0u);
        }
        CHECK(ok);
        RwRasterUnlock(parent);
        CHECK(RwRasterDestroy(sub) == TRUE);
        RwRasterGetStride(parent);
        CHECK(RwRasterLock(parent, 0, rwRASTERLOCKREAD) != nullptr);              // parent texture still alive
        RwRasterUnlock(parent);
    }
    if (parent) RwRasterDestroy(parent);

    // PAL8: the open d3d9 device has no P8 textures (isP8supported == 0), so such a raster cannot exist: NULL, no crash (TXD loading expands palettes, 03b)
    {
        RwRaster* p8 = RwRasterCreate(8, 8, 8, rwRASTERFORMATPAL8 | rwRASTERFORMAT8888 | rwRASTERTYPETEXTURE);
        std::printf("  PAL8 raster on this device: %s (isP8supported=%d)\n", p8 ? "created" : "NULL", (int)rw::d3d::isP8supported);
        CHECK((p8 != nullptr) == (rw::d3d::isP8supported != 0));
        if (p8) { CHECK(RwRasterLockPalette(p8, rwRASTERLOCKWRITE) != nullptr); RwRasterUnlockPalette(p8); RwRasterDestroy(p8); }
    }

    // palette lock on a non paletted raster
    RwRaster* np = RwRasterCreate(4, 4, 32, rwRASTERFORMAT8888 | rwRASTERTYPETEXTURE);
    CHECK(np && RwRasterLockPalette(np, rwRASTERLOCKREAD) == nullptr && RwRasterUnlockPalette(np) == np);
    if (np) RwRasterDestroy(np);
}

static void ImageRasterTests() {
    std::printf("--- image <-> raster\n");
    // FindRasterFormat
    RwInt32 w = 0, h = 0, d = 0, f = 0;
    RwImage* i24 = MakePatternImage(8, 4, 24);
    CHECK(RwImageFindRasterFormat(i24, rwRASTERTYPETEXTURE, &w, &h, &d, &f) == i24 && w == 8 && h == 4 && d == 32 && f == (rwRASTERFORMAT888 | rwRASTERTYPETEXTURE));
    RwImage* i32 = MakePatternImage(8, 4, 32, true);
    CHECK(RwImageFindRasterFormat(i32, rwRASTERTYPETEXTURE, &w, &h, &d, &f) == i32 && d == 32 && f == (rwRASTERFORMAT8888 | rwRASTERTYPETEXTURE));
    RwImage* i32o = MakePatternImage(8, 4, 32, false);        // all opaque -> 888
    CHECK(RwImageFindRasterFormat(i32o, rwRASTERTYPETEXTURE, &w, &h, &d, &f) == i32o && d == 32 && f == (rwRASTERFORMAT888 | rwRASTERTYPETEXTURE));
    RwImage* i16 = MakePatternImage(8, 4, 16, true);
    CHECK(RwImageFindRasterFormat(i16, rwRASTERTYPETEXTURE, &w, &h, &d, &f) == i16 && d == 16 && f == (rwRASTERFORMAT1555 | rwRASTERTYPETEXTURE));
    RwImage* i8 = MakePatternImage(8, 4, 8);                   // device open: P8 unsupported -> expanded
    CHECK(RwImageFindRasterFormat(i8, rwRASTERTYPETEXTURE, &w, &h, &d, &f) == i8 && d == 32 && f == (rwRASTERFORMAT888 | rwRASTERTYPETEXTURE));
    CHECK(RwImageFindRasterFormat(i8, rwRASTERTYPECAMERA, &w, &h, &d, &f) == nullptr);
    CHECK(RwImageFindRasterFormat(i8, rwRASTERTYPENORMAL, &w, &h, &d, &f) == i8 && f == rwRASTERFORMAT888);

    // PlayerSkin sequence: FindRasterFormat -> Create(flags) -> SetFromImage -> SetFromRaster
    struct { RwImage* img; const char* name; } rt[] = { { i24, "24" }, { i32, "32 alpha" }, { i32o, "32 opaque" }, { i16, "16" }, { i8, "8 pal" } };
    for (auto& t : rt) {
        std::printf("  [%s]\n", t.name);
        RwImage* img = t.img;
        RwImageFindRasterFormat(img, rwRASTERTYPETEXTURE, &w, &h, &d, &f);
        RwRaster* r = RwRasterCreate(w, h, d, f);
        CHECK(r != nullptr);
        if (!r) continue;
        CHECK(RwRasterSetFromImage(r, img) == r);
        // read back what is in the texture
        RwUInt8* px = RwRasterLock(r, 0, rwRASTERLOCKREAD);
        bool ok = px != nullptr;
        for (int y = 0; px && y < img->height; ++y) for (int x = 0; x < img->width; ++x) {
            unsigned char c[4];
            if (img->depth == 8) { const unsigned char* pe = img->palette + img->pixels[y * img->stride + x] * 4; std::memcpy(c, pe, 4); }
            else if (img->depth == 16) { const unsigned v = img->pixels[y * img->stride + x * 2] | (img->pixels[y * img->stride + x * 2 + 1] << 8);
                c[0] = (unsigned char)(((v >> 10) & 31) * 255 / 31); c[1] = (unsigned char)(((v >> 5) & 31) * 255 / 31); c[2] = (unsigned char)((v & 31) * 255 / 31); c[3] = v & 0x8000 ? 255 : 0; }
            else { const int bpp = img->depth / 8; std::memcpy(c, img->pixels + y * img->stride + x * bpp, 3); c[3] = bpp == 4 ? img->pixels[y * img->stride + x * 4 + 3] : 255; }
            const unsigned char* q = px + y * RwRasterGetStride(r);
            if (RwRasterGetFormat(r) == rwRASTERFORMAT1555) {
                const unsigned v = q[x * 2] | (q[x * 2 + 1] << 8);
                ok = ok && (unsigned char)(((v >> 10) & 31) * 255 / 31) == c[0] && (unsigned char)(((v >> 5) & 31) * 255 / 31) == c[1] && (unsigned char)((v & 31) * 255 / 31) == c[2] && ((v >> 15) ? 255 : 0) == c[3];
            } else {
                const unsigned v = *(const unsigned*)(q + x * 4);
                const bool a888 = RwRasterGetFormat(r) == rwRASTERFORMAT888;
                ok = ok && (v & 0xFF) == c[2] && ((v >> 8) & 0xFF) == c[1] && ((v >> 16) & 0xFF) == c[0] && (a888 || (v >> 24) == c[3]);
            }
        }
        RwRasterUnlock(r);
        CHECK(ok);
        // and back into an image of the source depth (or 32 for palettized)
        const int od = img->depth <= 8 ? 32 : img->depth;
        RwImage* back = RwImageCreate(img->width, img->height, od);
        RwImageAllocatePixels(back);
        CHECK(RwImageSetFromRaster(back, r) == back);
        bool same = true;
        for (int y = 0; y < img->height; ++y) for (int x = 0; x < img->width; ++x) {
            unsigned char c[4];
            if (img->depth == 8) std::memcpy(c, img->palette + img->pixels[y * img->stride + x] * 4, 4);
            else if (img->depth == 16) { const unsigned v = img->pixels[y * img->stride + x * 2] | (img->pixels[y * img->stride + x * 2 + 1] << 8);
                c[0] = (unsigned char)(((v >> 10) & 31) * 255 / 31); c[1] = (unsigned char)(((v >> 5) & 31) * 255 / 31); c[2] = (unsigned char)((v & 31) * 255 / 31); c[3] = v & 0x8000 ? 255 : 0; }
            else { const int bpp = img->depth / 8; std::memcpy(c, img->pixels + y * img->stride + x * bpp, 3); c[3] = bpp == 4 ? img->pixels[y * img->stride + x * 4 + 3] : 255; }
            if (od == 16) {
                const unsigned v = back->pixels[y * back->stride + x * 2] | (back->pixels[y * back->stride + x * 2 + 1] << 8);
                same = same && (unsigned char)(((v >> 10) & 31) * 255 / 31) == c[0] && (v & 0x8000 ? 255 : 0) == c[3];
            } else {
                const unsigned char* g = back->pixels + y * back->stride + x * (od / 8);
                same = same && g[0] == c[0] && g[1] == c[1] && g[2] == c[2] && (od == 24 || g[3] == c[3]);
            }
        }
        CHECK(same);
        RwImageDestroy(back);
        RwRasterDestroy(r);
    }
    // size mismatch / wrong type
    {
        RwRaster* r = RwRasterCreate(4, 4, 32, rwRASTERFORMAT8888 | rwRASTERTYPETEXTURE);
        CHECK(r && RwRasterSetFromImage(r, i24) == nullptr);                          // 8x4 into 4x4: librw asserts
        RwImage* smallTex = RwImageCreate(2, 2, 32);
        CHECK(RwImageSetFromRaster(smallTex, r) == nullptr);                             // no pixels
        RwImageAllocatePixels(smallTex);
        CHECK(RwImageSetFromRaster(smallTex, r) == nullptr);                             // size mismatch
        RwImageDestroy(smallTex);
        if (r) RwRasterDestroy(r);
    }
    // 16 bit formats 565 / 4444 / 555 / LUM8: through RwRasterSetFromImage (librw cannot) and back
    {
        const struct { int fmt; const char* name; int tol; } f16[] = { { rwRASTERFORMAT565, "565", 8 }, { rwRASTERFORMAT4444, "4444", 16 }, { rwRASTERFORMAT555, "555", 8 } };
        RwImage* src = MakePatternImage(8, 4, 32, true);
        for (auto& k : f16) {
            RwRaster* r = RwRasterCreate(8, 4, 16, k.fmt | rwRASTERTYPETEXTURE);
            CHECK(r && RwRasterSetFromImage(r, src) == r);
            RwImage* back = RwImageCreate(8, 4, 32); RwImageAllocatePixels(back);
            CHECK(RwImageSetFromRaster(back, r) == back);
            int worst = 0, alphaWorst = 0;
            for (int y = 0; y < 4; ++y) for (int x = 0; x < 8; ++x) for (int c = 0; c < 3; ++c) {
                const int dlt = std::abs((int)back->pixels[y * back->stride + x * 4 + c] - (int)src->pixels[y * src->stride + x * 4 + c]);
                if (dlt > worst) worst = dlt;
            }
            for (int y = 0; y < 4; ++y) for (int x = 0; x < 8; ++x) {
                const int dlt = std::abs((int)back->pixels[y * back->stride + x * 4 + 3] - (int)src->pixels[y * src->stride + x * 4 + 3]);
                if (dlt > alphaWorst) alphaWorst = dlt;
            }
            CHECKV(worst < k.tol, "%s colour error %d (< %d), alpha error %d", k.name, worst, k.tol, alphaWorst);
            if (k.fmt == rwRASTERFORMAT4444) CHECK(alphaWorst < 16);
            RwImageDestroy(back);
            RwRasterDestroy(r);
        }
        RwImageDestroy(src);
    }
    // raster texel -> image pixel: channels are shifted up without replicating the low bits (exe 0x7FF070 / 0x7FF450); 4444 alpha nibble F -> FF
    {
        struct { int fmt; uint16_t texel; unsigned char want[4]; } e[] = {
            { rwRASTERFORMAT565,  0xFFFF, { 0xF8, 0xFC, 0xF8, 0xFF } },
            { rwRASTERFORMAT565,  0x0821, { 0x08, 0x04, 0x08, 0xFF } },
            { rwRASTERFORMAT1555, 0xFFFF, { 0xF8, 0xF8, 0xF8, 0xFF } },
            { rwRASTERFORMAT1555, 0x7FFF, { 0xF8, 0xF8, 0xF8, 0x00 } },
            { rwRASTERFORMAT555,  0x7FFF, { 0xF8, 0xF8, 0xF8, 0xFF } },
            { rwRASTERFORMAT4444, 0xFFFF, { 0xF0, 0xF0, 0xF0, 0xFF } },
            { rwRASTERFORMAT4444, 0x8123, { 0x10, 0x20, 0x30, 0x80 } },
        };
        for (auto& k : e) {
            RwRaster* r = RwRasterCreate(2, 2, 16, k.fmt | rwRASTERTYPETEXTURE);
            RwImage* back = RwImageCreate(2, 2, 32); RwImageAllocatePixels(back);
            unsigned char* p = r ? RwRasterLock(r, 0, rwRASTERLOCKWRITE) : nullptr;
            CHECK(p != nullptr);
            if (p) {
                for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) std::memcpy(p + y * RwRasterGetStride(r) + x * 2, &k.texel, 2);
                RwRasterUnlock(r);
                CHECK(RwImageSetFromRaster(back, r) == back);
                CHECKV(std::memcmp(back->pixels, k.want, 4) == 0, "fmt %x texel %04x -> %02x%02x%02x%02x", k.fmt, k.texel, back->pixels[0], back->pixels[1], back->pixels[2], back->pixels[3]);
            }
            RwImageDestroy(back);
            if (r) RwRasterDestroy(r);
        }
    }
    for (RwImage* z : { i24, i32, i32o, i16, i8 }) RwImageDestroy(z);
}

//--------------------------------------------------------------------------------------------------
static void CameraRasterTests(IDirect3DDevice9* dev) {
    std::printf("--- camera / z buffer / camera texture rasters\n");
    const auto& pp = rw::d3d::d3d9Globals.present;
    RwRaster* fb = RwRasterCreate(pp.BackBufferWidth, pp.BackBufferHeight, 0, rwRASTERTYPECAMERA);
    RwRaster* zb = RwRasterCreate(pp.BackBufferWidth, pp.BackBufferHeight, 0, rwRASTERTYPEZBUFFER);
    CHECKV(fb && zb, "fb %p zb %p", (void*)fb, (void*)zb);
    if (fb && zb) {
        std::printf("  camera raster %dx%d depth %d format %x, z %dx%d depth %d\n", fb->width, fb->height, fb->depth, fb->format, zb->width, zb->height, zb->depth);
        CHECK(RwRasterGetType(fb) == rwRASTERTYPECAMERA && RwRasterGetType(zb) == rwRASTERTYPEZBUFFER && fb->depth == 32 && zb->depth >= 16);
        CHECK(RwRasterGetNumLevels(fb) == 1 && RwRasterGetNumLevels(zb) == 1);
        CHECK(RwRasterLock(zb, 0, rwRASTERLOCKREAD) == nullptr);                    // z buffers are not lockable
    }
    // camera texture (render target): clear through the context, read back through the lock
    RwRaster* ct = RwRasterCreate(64, 32, 32, rwRASTERTYPECAMERATEXTURE);
    CHECK(ct != nullptr);
    if (ct) {
        std::printf("  camera texture depth %d format %x\n", ct->depth, ct->format);
        CHECK(RwRasterGetCurrentContext() == nullptr && RwRasterClear(0xFF336699) == FALSE);
        CHECK(RwRasterPushContext(ct) == ct && RwRasterGetCurrentContext() == ct);
        CHECK(RwRasterClear(0xFF336699) == TRUE);
        RwRect part{ 8, 4, 16, 8 };
        CHECK(RwRasterClearRect(&part, 0xFFAA0000) == TRUE);
        CHECK(RwRasterPopContext() == ct && RwRasterGetCurrentContext() == nullptr);
        CHECK(RwRasterLock(ct, 0, rwRASTERLOCKWRITE) == nullptr);                   // render targets cannot be written
        RwUInt8* p = RwRasterLock(ct, 0, rwRASTERLOCKREAD | rwRASTERLOCKNOFETCH);   // NOFETCH is stripped (librw would assert)
        CHECK(p != nullptr);
        if (p) {
            const int stride = RwRasterGetStride(ct);
            const unsigned inside = *(unsigned*)(p + 6 * stride + 10 * 4) | 0xFF000000u, outside = *(unsigned*)(p + 1 * stride + 2 * 4) | 0xFF000000u;
            const unsigned edge = *(unsigned*)(p + 11 * stride + 23 * 4) | 0xFF000000u;     // x 8+16 = 24 is outside, 23 inside; y 4+8 = 12 outside, 11 inside
            const unsigned outEdge = *(unsigned*)(p + 12 * stride + 24 * 4) | 0xFF000000u;
            CHECKV(inside == 0xFFAA0000u && outside == 0xFF336699u && edge == 0xFFAA0000u && outEdge == 0xFF336699u, "inside %08X outside %08X edge %08X outEdge %08X", inside, outside, edge, outEdge);
        }
        RwRasterUnlock(ct);
        CHECK(RwRasterGetStride(ct) == 64 * 4);

        // PostEffects: push the front buffer (camera texture), RwRasterRenderFast(camera raster, 0, 0), pop
        if (fb) {
            CHECK(SUCCEEDED(dev->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFFC81E32, 1.0f, 0)));
            RwRaster* smallTex = RwRasterCreate(pp.BackBufferWidth, pp.BackBufferHeight, 0, rwRASTERTYPECAMERATEXTURE);
            CHECK(smallTex != nullptr);
            if (smallTex) {
                CHECK(RwRasterRenderFast(fb, 0, 0) == nullptr);                       // no context: NULL (librw would crash)
                RwRasterPushContext(smallTex);
                CHECK(RwRasterRenderFast(fb, 0, 0) == fb);
                RwRasterPopContext();
                RwUInt8* q = RwRasterLock(smallTex, 0, rwRASTERLOCKREAD);
                CHECK(q != nullptr);
                if (q) {
                    const unsigned a = *(unsigned*)(q + 3 * RwRasterGetStride(smallTex) + 5 * 4) | 0xFF000000u;
                    CHECKV(a == 0xFFC81E32u, "front buffer copy pixel %08X", a);
                }
                RwRasterUnlock(smallTex);
                RwRasterDestroy(smallTex);
            }
        }
        RwRasterDestroy(ct);
    }
    if (fb) RwRasterDestroy(fb);
    if (zb) RwRasterDestroy(zb);
}

static void RenderTests(IDirect3DDevice9* dev) {
    std::printf("--- RwRasterRender / RenderScaled on the current camera\n");
    const auto& pp = rw::d3d::d3d9Globals.present;
    RwCamera* cam = RwCameraCreate();
    RwFrame* fr = RwFrameCreate();
    RwCameraSetFrame(cam, fr);
    RwRaster* fb = RwRasterCreate(pp.BackBufferWidth, pp.BackBufferHeight, 0, rwRASTERTYPECAMERA);
    RwRaster* zb = RwRasterCreate(pp.BackBufferWidth, pp.BackBufferHeight, 0, rwRASTERTYPEZBUFFER);
    RwCameraSetRaster(cam, fb); RwCameraSetZRaster(cam, zb);
    RwRaster* tex = RwRasterCreate(8, 8, 32, rwRASTERFORMAT8888 | rwRASTERTYPETEXTURE);
    CHECK(cam && fr && fb && zb && tex);
    if (!(cam && fb && zb && tex)) return;
    RwUInt8* p = RwRasterLock(tex, 0, rwRASTERLOCKWRITE);
    for (int i = 0; i < 64; ++i) ((unsigned*)p)[i] = 0xFF00FF00u;     // opaque green
    RwRasterUnlock(tex);
    CHECK(RwRasterRender(tex, 10, 10) == nullptr);                    // outside Begin/EndUpdate
    RwRGBA black{ 0, 0, 0, 255 };
    CHECK(RwCameraBeginUpdate(cam) == cam);
    RwCameraClear(cam, &black, rwCAMERACLEARIMAGE | rwCAMERACLEARZ);
    CHECK(RwRasterRender(tex, 10, 10) == tex);
    CHECK(ReadPixel(dev, 12, 12) == 0xFF00FF00u);
    CHECK(ReadPixel(dev, 17, 17) == 0xFF00FF00u);
    CHECK(ReadPixel(dev, 18, 12) == 0xFF000000u && ReadPixel(dev, 12, 18) == 0xFF000000u && ReadPixel(dev, 9, 9) == 0xFF000000u);
    RwRect to{ 40, 30, 20, 16 };
    CHECK(RwRasterRenderScaled(tex, &to) == tex);
    CHECK(ReadPixel(dev, 50, 38) == 0xFF00FF00u && ReadPixel(dev, 59, 45) == 0xFF00FF00u && ReadPixel(dev, 60, 38) == 0xFF000000u && ReadPixel(dev, 39, 38) == 0xFF000000u);
    RwCameraEndUpdate(cam);
    RwRasterDestroy(tex);
    RwCameraSetRaster(cam, nullptr); RwCameraSetZRaster(cam, nullptr);
    RwRasterDestroy(fb); RwRasterDestroy(zb);
    RwCameraSetFrame(cam, nullptr);
    RwCameraDestroy(cam); RwFrameDestroy(fr);
}

static void RealFileTests(const char* bmpPath) {
    std::printf("--- real asset: %s\n", bmpPath);
    RwImage* img = RtBMPImageRead(bmpPath);
    CHECKV(img != nullptr, "read %s", bmpPath);
    if (!img) return;
    std::printf("  %dx%d depth %d stride %d palette[1] %02X%02X%02X\n", img->width, img->height, img->depth, img->stride, img->palette ? img->palette[4] : 0, img->palette ? img->palette[5] : 0, img->palette ? img->palette[6] : 0);
    CHECK(img->width == 1024 && img->height == 1024 && img->depth == 8 && img->palette && img->stride == 1024);
    // statistics: the title screen is not flat
    int used[256] = {}; for (int i = 0; i < 1024 * 1024; ++i) used[img->pixels[i]] = 1;
    int n = 0; for (int i = 0; i < 256; ++i) n += used[i];
    CHECKV(n > 8, "distinct indices %d", n);
    RwInt32 w, h, d, f;
    CHECK(RwImageFindRasterFormat(img, rwRASTERTYPETEXTURE, &w, &h, &d, &f) == img);
    RwRaster* r = RwRasterCreate(w, h, d, f);
    CHECKV(r && RwRasterSetFromImage(r, img) == r, "raster %dx%d depth %d format %x", w, h, d, f);
    if (r) {
        RwImage* back = RwImageCreate(w, h, 32); RwImageAllocatePixels(back);
        CHECK(RwImageSetFromRaster(back, r) == back);
        bool ok = true;
        for (int i = 0; i < 1024 * 1024 && ok; i += 97) {
            const unsigned char* pe = img->palette + img->pixels[(i / 1024) * img->stride + i % 1024] * 4;
            const unsigned char* g = back->pixels + (i / 1024) * back->stride + (i % 1024) * 4;
            ok = pe[0] == g[0] && pe[1] == g[1] && pe[2] == g[2];
        }
        CHECK(ok);
        // and a PNG round trip of the real image
        const char* pf = TmpFile("rw_raster_image_real.png");
        CHECK(RtPNGImageWrite(img, pf) == img);
        RwImage* pr = RtPNGImageRead(pf);
        CHECK(pr && pr->width == 1024 && pr->height == 1024);
        if (pr) {
            bool same = true;
            for (int i = 0; i < 1024 * 1024 && same; i += 101) {
                const unsigned char* pe = img->palette + img->pixels[(i / 1024) * img->stride + i % 1024] * 4;
                if (pr->depth == 8) { const unsigned char* q = pr->palette + pr->pixels[(i / 1024) * pr->stride + i % 1024] * 4; same = !std::memcmp(pe, q, 3); }
                else { const unsigned char* q = pr->pixels + (i / 1024) * pr->stride + (i % 1024) * (pr->depth / 8); same = !std::memcmp(pe, q, 3); }
            }
            CHECK(same);
            RwImageDestroy(pr);
        }
        RwImageDestroy(back);
        RwRasterDestroy(r);
    }
    RwImageDestroy(img);
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    HWND wnd = CreateWindowA("STATIC", "rw_raster_image_test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(wnd != nullptr);
    rw::MemoryFunctions mf{};   // plain CRT heap, same as the engine layer (engine.cpp)
    mf.rwmalloc  = [](size_t sz, unsigned) -> void* { return sz ? std::malloc(sz) : nullptr; };
    mf.rwrealloc = [](void* p, size_t sz, unsigned) -> void* { return std::realloc(p, sz); };
    mf.rwfree    = [](void* p) { std::free(p); };
    CHECK(rw::Engine::init(&mf));
    rw::EngineOpenParams params{};
    params.window = wnd;
    CHECK(rw::Engine::open(&params));
    const bool opened = rw::d3d::d3d9Globals.d3d9 != nullptr;
    std::printf("D3D9 object: %s\n", opened ? "yes" : "NO");
    rw::Engine::start();
    IDirect3DDevice9* dev = rw::d3d::d3ddevice;
    std::printf("D3D9 device: %s\n", dev ? "yes" : "NO (device-less run: only the image tests)");
    RwEngineInstance->dOpenDevice.zBufferNear = 0; RwEngineInstance->dOpenDevice.zBufferFar = 1;

    ImageTests();
    if (dev) {
        RasterFormatTests();
        ImageRasterTests();
        CameraRasterTests(dev);
        RenderTests(dev);
    }
    if (argc > 1) {
        if (dev) RealFileTests(argv[1]);
        else {   // device-less: reading is still testable
            RwImage* img = RtBMPImageRead(argv[1]);
            CHECK(img && img->width == 1024 && img->depth == 8);
            if (img) RwImageDestroy(img);
        }
    }

    if (dev) rw::Engine::stop();
    rw::Engine::close();
    rw::Engine::term();
    DestroyWindow(wnd);
    std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "PASSED", g_fail);
    return g_fail;
}
