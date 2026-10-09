// P2B-03a: pixel helpers shared by raster.cpp and image.cpp (private to source/standalone/rw, not a fakerw header).
//
// Everything goes through a 4 x uint8 RGBA intermediate:
//   RwImage pixels   (librw layout, same as RW): 32 bit = R,G,B,A bytes; 24 bit = R,G,B; 16 bit = ARGB1555 word (bit 15 alpha);
//                    8 bit = palette index per byte; 4 bit = palette index per BYTE (librw/RW-shim convention: one byte per pixel, not packed);
//                    palette = 4 bytes R,G,B,A per entry (16 / 256 entries). Rows are top-down.
//   d3d9 raster texels (little-endian words): C8888 = B,G,R,A; C888 = X8R8G8B8 (B,G,R,X; the raster depth is 32); C1555 / C555 = A1R5G5B5 /
//                    X1R5G5B5; C565 = R5G6B5; C4444 = A4R4G4B4; LUM8 = one luminance byte; PAL8 = index per byte, palette in D3dRaster::palette (RGBA).
// 5-bit channels expand with v*255/31 (what librw's conv_RGBA8888_from_ARGB1555 does), 4-bit with v*17, narrowing truncates (RW's RwRGBAToPixel).
#pragma once
#ifdef NOTSA_RW_LIBRW
#include <cstdint>

namespace rwshim {

struct Px {
    uint8_t r, g, b, a;
};

inline uint8_t Expand5(unsigned v) { return (uint8_t)(v * 0xFF / 0x1F); }
inline uint8_t Expand6(unsigned v) { return (uint8_t)(v * 0xFF / 0x3F); }

// ---- RwImage side (top-down, `row` = image->pixels + y * image->stride) ----
inline Px ImageGet(const rw::Image* img, int x, int y) {
    const uint8_t* row = img->pixels + (size_t)y * img->stride;
    switch (img->depth) {
    case 32: {
        const uint8_t* p = row + x * 4;
        return { p[0], p[1], p[2], p[3] };
    }
    case 24: {
        const uint8_t* p = row + x * 3;
        return { p[0], p[1], p[2], 0xFF };
    }
    case 16: {
        const unsigned w = row[x * 2] | (row[x * 2 + 1] << 8);
        return { Expand5((w >> 10) & 0x1F), Expand5((w >> 5) & 0x1F), Expand5(w & 0x1F), (uint8_t)((w & 0x8000) ? 0xFF : 0) };
    }
    case 8:
    case 4: {
        const uint8_t* c = img->palette ? img->palette + row[x] * 4 : nullptr;
        if (!c) {
            return { row[x], row[x], row[x], 0xFF };
        }
        return { c[0], c[1], c[2], c[3] };
    }
    }
    return { 0, 0, 0, 0xFF };
}

// depth 8 / 4 are index images: callers store indices themselves
inline void ImagePut(rw::Image* img, int x, int y, Px p) {
    uint8_t* row = img->pixels + (size_t)y * img->stride;
    switch (img->depth) {
    case 32: {
        uint8_t* d = row + x * 4;
        d[0] = p.r; d[1] = p.g; d[2] = p.b; d[3] = p.a;
        break;
    }
    case 24: {
        uint8_t* d = row + x * 3;
        d[0] = p.r; d[1] = p.g; d[2] = p.b;
        break;
    }
    case 16: {
        const unsigned w = ((p.a & 0x80) << 8) | ((p.r & 0xF8) << 7) | ((p.g & 0xF8) << 2) | (p.b >> 3);
        row[x * 2]     = (uint8_t)(w & 0xFF);
        row[x * 2 + 1] = (uint8_t)(w >> 8);
        break;
    }
    }
}

// ---- RW raster format side: bytes per texel for the formats RwRasterCreate can make a CPU-lockable texture of ----
inline int FormatBytes(int fmt) {
    if (fmt & (0x2000 | 0x4000)) return 1;                            // PAL8 / PAL4 (index bytes)
    switch (fmt & 0x0F00) {
    case 0x0100: case 0x0200: case 0x0300: case 0x0A00: return 2;   // 1555 565 4444 555
    case 0x0400: return 1;                                           // LUM8
    case 0x0500: case 0x0600: return 4;                              // 8888 888 (X8R8G8B8)
    }
    return 0;
}

// RGBA -> texel word for a (non palettized) raster format. Returns false for formats it cannot encode (depth/stencil, default).
inline bool EncodeTexel(Px p, int fmt, uint32_t* out) {
    switch (fmt & 0x0F00) {
    case 0x0100: *out = ((p.a & 0x80) << 8) | ((p.r & 0xF8) << 7) | ((p.g & 0xF8) << 2) | (p.b >> 3); return true;      // 1555
    case 0x0200: *out = ((p.r & 0xF8) << 8) | ((p.g & 0xFC) << 3) | (p.b >> 3); return true;                             // 565
    case 0x0300: *out = ((p.a & 0xF0) << 8) | ((p.r & 0xF0) << 4) | (p.g & 0xF0) | (p.b >> 4); return true;              // 4444
    case 0x0400: *out = (p.r * 30 + p.g * 59 + p.b * 11) / 100; return true;                                              // LUM8
    case 0x0500: *out = ((uint32_t)p.a << 24) | ((uint32_t)p.r << 16) | ((uint32_t)p.g << 8) | p.b; return true;           // 8888
    case 0x0600: *out = 0xFF000000u | ((uint32_t)p.r << 16) | ((uint32_t)p.g << 8) | p.b; return true;                     // 888 (X8R8G8B8)
    case 0x0A00: *out = ((p.r & 0xF8) << 7) | ((p.g & 0xF8) << 2) | (p.b >> 3); return true;                              // 555
    }
    return false;
}

inline bool DecodeTexel(uint32_t t, int fmt, Px* out) {
    switch (fmt & 0x0F00) {
    case 0x0100: *out = { Expand5((t >> 10) & 0x1F), Expand5((t >> 5) & 0x1F), Expand5(t & 0x1F), (uint8_t)((t & 0x8000) ? 0xFF : 0) }; return true;
    case 0x0200: *out = { Expand5((t >> 11) & 0x1F), Expand6((t >> 5) & 0x3F), Expand5(t & 0x1F), 0xFF }; return true;
    case 0x0300: *out = { (uint8_t)(((t >> 8) & 0xF) * 17), (uint8_t)(((t >> 4) & 0xF) * 17), (uint8_t)((t & 0xF) * 17), (uint8_t)(((t >> 12) & 0xF) * 17) }; return true;
    case 0x0400: *out = { (uint8_t)t, (uint8_t)t, (uint8_t)t, 0xFF }; return true;
    case 0x0500: *out = { (uint8_t)(t >> 16), (uint8_t)(t >> 8), (uint8_t)t, (uint8_t)(t >> 24) }; return true;
    case 0x0600: *out = { (uint8_t)(t >> 16), (uint8_t)(t >> 8), (uint8_t)t, 0xFF }; return true;
    case 0x0A00: *out = { Expand5((t >> 10) & 0x1F), Expand5((t >> 5) & 0x1F), Expand5(t & 0x1F), 0xFF }; return true;
    }
    return false;
}

} // namespace rwshim
#endif
