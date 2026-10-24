// Stream R / R-5: D3D9 native texture reader for the GL3 backend (portable build). See texture_gl3.h.
//
// SA TXD textures are chunk 0x15 with platform 9 (D3D9): STRUCT { platform, filter/addressing, name[32], mask[32], rasterFormat, d3dFormat, w, h,
// depth, numLevels, rasterType, flags (1 alpha, 2 cube, 4 auto mip gen, 8 compressed) [, palette], { levelSize, data } * numLevels }.
// The D3D build (standalone/rw/texture.cpp) creates D3D textures; here the data goes into a librw gl3 TEXTURE raster:
//   * uncompressed: the raster holds d3d9 texel words (rw::gl3::d3dTexelOrder, set by RwEngineOpen): 8888 = B,G,R,A bytes, 888 = X8R8G8B8, 1555 = A1R5G5B5,
//     565, 555, 4444 = A4R4G4B4, LUM8: the file bytes are copied unchanged (no swizzle);
//   * PAL8 / PAL4: expanded at load to 8888 (alpha flag set or the file says 8888) or 888, exactly like the D3D build (palette entries R,G,B,A -> B,G,R,A);
//   * DXT1 / DXT3 / DXT5: uploaded as compressed blocks (GL_EXT_texture_compression_s3tc), level data untouched. DXT1 always uses the RGBA internal format so
//     the 1 bit alpha mode of a block works as in D3D regardless of the file's alpha flag (natras->hasAlpha follows the flag);
//   * rows: top row first, as in the file (R-4 rule: the game's v coordinate is compensated by the Im2D / Im3D twins, NotsaGl3TextureNeedsVFlip);
//   * DXT chains: levels below 4x4 are stored with size 0 in SA's files (D3D keeps them blank); they are filled with the previous level's block (counted);
//   * mip chain: exactly the file's levels (never auto-generated, like the D3D reader); raster->format keeps the file's mip bits.
// Honest gaps: cube maps (never in SA PC files) fail; DXT2 / DXT4 (premultiplied alpha FOURCC) fail; NPOT textures are created as is (counted).
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

#include "raster_gl3.h"
#include "texture_gl3.h"

namespace {
NotsaGl3TextureStats s_Stats{};

void Warn(const char* name, const char* fmt, ...) {
    char    buf[400];
    va_list a;
    va_start(a, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);
    std::fprintf(stderr, "[txd-gl3] texture \"%.31s\": %s\n", name, buf);
}

constexpr uint32_t kDXT1 = 0x31545844, kDXT3 = 0x33545844, kDXT5 = 0x35545844;
constexpr int      kPAL8 = 0x2000, kPAL4 = 0x4000, kMip = 0x8000, kAutoMip = 0x1000;

bool IsPot(int v) { return v > 0 && (v & (v - 1)) == 0; }

int FullChain(int w, int h) {
    int n = 1;
    while (w > 1 || h > 1) {
        ++n;
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
    }
    return n;
}

int BytesPerTexel(int colour) {
    switch (colour) {
    case 0x100: case 0x200: case 0x300: case 0xA00: return 2; // 1555 565 4444 555
    case 0x400: return 1;                                     // LUM8
    case 0x500: case 0x600: return 4;                         // 8888 888 (X8R8G8B8)
    }
    return 0;
}
} // namespace

NotsaGl3TextureStats& NotsaGl3TextureStatsGet() { return s_Stats; }
void                  NotsaGl3TextureStatsReset() { s_Stats = {}; }

RwTexture* NotsaGl3ReadD3D9Texture(rw::Stream* stream) {
    RwTexture* tex = rw::Texture::create(nullptr);
    if (!tex) {
        s_Stats.failed++;
        return nullptr;
    }
    // exe 0x4CD820: filter (bits 0-7) and U / V addressing (bits 8-15) are merged into the fresh texture's word, the upper 16 bits of the file are dropped
    tex->filterAddressing = stream->readU32() & 0xFFFFu;
    stream->read8(tex->name, 32);
    stream->read8(tex->mask, 32);
    tex->name[31] = '\0';
    tex->mask[31] = '\0';

    const int32_t format    = stream->readI32();
    const int32_t d3dformat = stream->readI32();
    const int32_t width     = stream->readU16();
    const int32_t height    = stream->readU16();
    const int32_t depth     = stream->readU8();
    const int32_t numLevels = stream->readU8();
    const int32_t type      = stream->readU8();
    const int32_t flags     = stream->readU8(); // 1 alpha, 2 cube, 4 auto mip generation, 8 compressed
    (void)depth;

    auto fail = [&](const char* why, uint32_t arg = 0) -> RwTexture* {
        Warn(tex->name, "%s (0x%x) format 0x%x d3dfmt 0x%x %dx%d levels %d flags 0x%x", why, arg, format, d3dformat, width, height, numLevels, flags);
        s_Stats.failed++;
        tex->destroy();
        return nullptr;
    };
    if (width <= 0 || height <= 0 || numLevels <= 0) {
        return fail("empty texture");
    }
    if (flags & 2) {
        return fail("cube map");
    }
    if (!IsPot(width) || !IsPot(height)) {
        s_Stats.npot++;
    }
    while (glGetError() != GL_NO_ERROR) {
    }

    const bool compressed = (flags & 8) != 0;
    const int  rasterType = ((type & 0xF) == rw::Raster::NORMAL) ? rw::Raster::NORMAL : rw::Raster::TEXTURE;
    RwRaster*  raster     = nullptr;
    uint8_t    palette[256 * 4] = {};
    bool       paletted = false;
    int        colour   = format & 0xF00;
    // mip bits as the raster is created: the file's levels are uploaded one by one, never generated
    int createMip = numLevels > 1 ? kMip : 0;

    if (compressed) {
        int dxt = 0;
        switch ((uint32_t)d3dformat) {
        case kDXT1: dxt = 1; break;
        case kDXT3: dxt = 3; break;
        case kDXT5: dxt = 5; break;
        default: return fail("unsupported compressed D3DFORMAT", (uint32_t)d3dformat);
        }
        if (!rw::gl3::gl3Caps.dxtSupported) {
            return fail("GL_EXT_texture_compression_s3tc missing");
        }
        raster = rw::Raster::create(width, height, 16, colour | createMip | rasterType | rw::Raster::DONTALLOCATE);
        if (!raster) {
            return fail("Raster::create failed");
        }
        rw::gl3::allocateDXT(raster, dxt, numLevels, 1);
        Gl3Raster* nr = GETGL3RASTEREXT(raster);
        nr->hasAlpha  = (flags & 1) || dxt != 1;
    } else {
        int fmt = colour;
        if (format & (kPAL8 | kPAL4)) {
            // expand at load (see the header comment); PAL4 files carry a 32 entry palette and 8 bit indices
            paletted = true;
            stream->read8(palette, (format & kPAL4) ? 4 * 32 : 4 * 256);
            if (fmt != rw::Raster::C8888 && fmt != rw::Raster::C888) {
                fmt = (flags & 1) ? rw::Raster::C8888 : rw::Raster::C888;
            }
            colour = fmt;
        }
        if (BytesPerTexel(fmt) == 0) {
            return fail("unsupported raster format", (uint32_t)fmt);
        }
        raster = rw::Raster::create(width, height, 32, fmt | createMip | rasterType);
        if (!raster) {
            return fail("Raster::create failed (gl3 refused the format)", (uint32_t)fmt);
        }
        Gl3Raster* nr = GETGL3RASTEREXT(raster);
        if (colour == rw::Raster::C8888 || colour == rw::Raster::C4444 || colour == rw::Raster::C1555) {
            nr->hasAlpha = (flags & 1) != 0;
        }
        // the chain gl3 allocated is the full one; the file may stop earlier
        const int have = nr->numLevels;
        if (numLevels > 1 && numLevels < have) {
            nr->numLevels = (int8_t)numLevels;
            const uint32_t prev = rw::gl3::bindTexture(nr->texid);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, numLevels - 1);
            rw::gl3::bindTexture(prev);
        }
        // report the true storage format; mip bits as in the file
        raster->format = (raster->format & ~(kMip | kAutoMip)) | (format & (kMip | kAutoMip));
    }
    tex->raster = raster;
    if (compressed) {
        raster->format = (raster->format & ~(kMip | kAutoMip)) | (format & (kMip | kAutoMip));
    }

    const int have = raster->getNumLevels();
    std::vector<uint8_t> prevBlocks; // last level of a compressed chain that had data
    for (int i = 0; i < numLevels; ++i) {
        const uint32_t size = stream->readU32();
        if (i >= have) {
            stream->seek(size);
            continue;
        }
        const int lw = std::max(1, width >> i);
        const int lh = std::max(1, height >> i);
        std::vector<uint8_t> src(size);
        stream->read8(src.data(), size);
        uint8_t* dst = raster->lock(i, rw::Raster::LOCKWRITE | rw::Raster::LOCKNOFETCH);
        if (!dst) {
            Warn(tex->name, "lock of level %d failed", i);
            return fail("lock failed", (uint32_t)i);
        }
        if (compressed) {
            // blocks as stored; the raster level holds max(1, ..) blocks, the file must agree
            const int      blockBytes = ((uint32_t)d3dformat == kDXT1) ? 8 : 16;
            const uint32_t want       = (uint32_t)(std::max(1, (lw + 3) / 4) * std::max(1, (lh + 3) / 4) * blockBytes);
            if (size == 0 && !prevBlocks.empty()) {
                // the files store 0 bytes for the 2x2 / 1x1 levels of a DXT chain (D3D leaves them blank); gl3 needs complete levels: repeat the 4x4 block
                std::memcpy(dst, prevBlocks.data(), std::min<size_t>(prevBlocks.size(), want));
                s_Stats.zeroLevels++;
            } else {
                if (size != want) {
                    s_Stats.levelSizeMismatch++;
                }
                std::memcpy(dst, src.data(), std::min<size_t>(size, want));
                prevBlocks.assign(src.begin(), src.begin() + std::min<size_t>(size, want));
            }
        } else if (paletted) {
            const int stride = raster->stride; // width * 4 at this level
            for (int y = 0; y < lh; ++y) {
                const uint8_t* in  = src.data() + (size_t)y * lw;
                uint8_t*       out = dst + (size_t)y * stride;
                for (int x = 0; x < lw && (size_t)y * lw + x < size; ++x) {
                    const uint8_t* c = palette + in[x] * 4;
                    out[x * 4 + 0] = c[2];
                    out[x * 4 + 1] = c[1];
                    out[x * 4 + 2] = c[0];
                    out[x * 4 + 3] = (colour == rw::Raster::C888) ? 0xFF : c[3];
                }
            }
            if (size != (uint32_t)lw * lh) {
                s_Stats.levelSizeMismatch++;
            }
        } else {
            const int      bpp    = BytesPerTexel(colour);
            const uint32_t tight  = (uint32_t)lw * lh * bpp;
            const int      stride = raster->stride;
            if (size == tight) {
                for (int y = 0; y < lh; ++y) {
                    std::memcpy(dst + (size_t)y * stride, src.data() + (size_t)y * lw * bpp, (size_t)lw * bpp);
                }
            } else if (colour == rw::Raster::C888 && size == (uint32_t)lw * lh * 3) {
                // legacy D3DFMT_R8G8B8 (B,G,R): widen to X8R8G8B8
                for (int y = 0; y < lh; ++y) {
                    for (int x = 0; x < lw; ++x) {
                        const uint8_t* in  = src.data() + ((size_t)y * lw + x) * 3;
                        uint8_t*       out = dst + (size_t)y * stride + x * 4;
                        out[0] = in[0]; out[1] = in[1]; out[2] = in[2]; out[3] = 0xFF;
                    }
                }
            } else {
                s_Stats.levelSizeMismatch++;
                const uint32_t n = std::min<uint32_t>(size, tight);
                std::memcpy(dst, src.data(), n);
            }
        }
        raster->unlock(i);
    }
    if (glGetError() != GL_NO_ERROR) {
        s_Stats.glErrors++;
        Warn(tex->name, "GL error during upload (format 0x%x d3dfmt 0x%x %dx%d)", format, d3dformat, width, height);
        while (glGetError() != GL_NO_ERROR) {
        }
    }
    s_Stats.loaded++;
    return tex;
}
#endif
