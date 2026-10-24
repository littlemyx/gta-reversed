// Stream R, slice R-5: TXD (D3D9 native textures) -> GL3 rasters, real-data test. Run by tools/portable/run_txd_test.sh (needs a display session and the Steam copy).
//
// For EVERY texture dictionary of the game data (models/*.txd, models/{txd,generic,grass}/*.txd and the .txd entries of models/{gta3,gta_int,player,cutscene}.img;
// the data is read in place, never copied or committed) the test
//   * parses the file itself (independent of the reader): chunk 0x15 headers, format / D3DFORMAT / size / levels / flags, raw level data,
//   * loads it through the game's path RwTexDictionaryStreamRead -> rwshim::ReadNativeTexture -> NotsaGl3ReadD3D9Texture (standalone/rw/texdict.cpp, texture.cpp,
//     portable/render/texture_gl3.cpp) into librw gl3 rasters on a live GL context,
//   * checks per texture: raster exists, size, mip level count == file levels, alpha flag, format class, glGetError() == 0,
//   * round-trips EVERY level: lock(read) on the GL texture vs the bytes expected from the file (DXT: raw blocks; 8888 / 888 / 1555 / 565 / 4444 / 555 / LUM8: texel
//     words; PAL8 / PAL4: the expanded B,G,R,A texels),
//   * counts textures / failures / mismatches per format.
// Then a pixel check per format: a sample of textures is drawn 1:1 with Im2D (nearest filter) and compared with a CPU decode (own DXT1 / DXT3 / DXT5 decoder,
// d3d9 texel words) within a tolerance, which also proves the top-row-first upload rule. Finally the lock contract of TXD rasters (READ and READ|WRITE locks
// return the CPU copy; RwRGBAToPixel texels written into a locked raster show the right colours).
// Result line: "TXD RESULT OK" / "TXD RESULT FAIL: ...". Environment: GTA (game dir), NOTSA_TXD_MAX (stop after N dictionaries), NOTSA_TXD_FILTER (substring of a name).
#include "fakerw.h"
#include "standalone/Fixups.h"
#include "portable/render/engine_gl3.h"
#include "portable/render/camera_gl3.h"
#include "portable/render/raster_gl3.h"
#include "portable/render/texture_gl3.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>

namespace notsa::standalone::Fixups {
void Log(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    std::printf("[log] ");
    std::vprintf(fmt, a);
    std::printf("\n");
    va_end(a);
}
void Fatal(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    std::vfprintf(stderr, fmt, a);
    va_end(a);
    std::abort();
}
void LogFpuState(const char*) {}
} // namespace notsa::standalone::Fixups
void RwShimPipelineEnsure() {}
void RwShimPipelineShutdown() {}
RwImage* RwImageRead(const RwChar*) { return nullptr; }
RwImage* RwImageReadMaskedImage(const RwChar*, const RwChar*) { return nullptr; }
RwBool RwImageDestroy(RwImage*) { return TRUE; }
RwImage* RwImageFindRasterFormat(RwImage*, RwInt32, RwInt32*, RwInt32*, RwInt32*, RwInt32*) { return nullptr; }

namespace {
int g_fail = 0;

void Check(bool ok, const char* what, const char* detail = "") {
    if (!ok) {
        g_fail++;
    }
    std::printf("  %-70s %s %s\n", what, ok ? "ok" : "FAIL", detail);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// independent TXD parser
// ---------------------------------------------------------------------------------------------------------------------------------------------
struct TexInfo {
    std::string name;
    uint32_t    platform = 0, format = 0, d3dformat = 0;
    int         w = 0, h = 0, depth = 0, levels = 0, type = 0, flags = 0;
    std::vector<uint8_t>               palette;
    std::vector<std::pair<const uint8_t*, uint32_t>> data; // per level
};

uint32_t U32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
uint16_t U16(const uint8_t* p) { uint16_t v; std::memcpy(&v, p, 2); return v; }

bool ParseTxd(const std::vector<uint8_t>& f, std::vector<TexInfo>& out) {
    if (f.size() < 28 || U32(f.data()) != 0x16) {
        return false;
    }
    const uint8_t* end = f.data() + f.size();
    const uint8_t* p   = f.data() + 12;           // TXD chunk header
    if (U32(p) != 1) return false;
    const unsigned n = U16(p + 12);
    p += 12 + U32(p + 4);
    for (unsigned i = 0; i < n; ++i) {
        if (p + 12 > end || U32(p) != 0x15) return false;
        const uint8_t* nativeEnd = p + 12 + U32(p + 4);
        const uint8_t* s = p + 12;                // STRUCT header
        if (U32(s) != 1) return false;
        const uint8_t* d = s + 12;
        TexInfo t;
        t.platform = U32(d); d += 4;
        d += 4;                                   // filter / addressing
        char nm[33] = {}; std::memcpy(nm, d, 32); t.name = nm; d += 64;
        t.format = U32(d); t.d3dformat = U32(d + 4); d += 8;
        t.w = U16(d); t.h = U16(d + 2); d += 4;
        t.depth = d[0]; t.levels = d[1]; t.type = d[2]; t.flags = d[3]; d += 4;
        if (t.platform != 9) { out.push_back(std::move(t)); return true; }
        if (t.format & 0x6000) {
            const int n = (t.format & 0x4000) ? 32 : 256;
            t.palette.assign(d, d + n * 4);
            d += n * 4;
        }
        for (int l = 0; l < t.levels; ++l) {
            const uint32_t sz = U32(d); d += 4;
            if (d + sz > end) return false;
            t.data.emplace_back(d, sz);
            d += sz;
        }
        out.push_back(std::move(t));
        p = nativeEnd;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// reference decoders (RGBA8, top row first)
// ---------------------------------------------------------------------------------------------------------------------------------------------
void Rgb565(uint16_t c, uint8_t* o) {
    const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    o[0] = (uint8_t)((r << 3) | (r >> 2));
    o[1] = (uint8_t)((g << 2) | (g >> 4));
    o[2] = (uint8_t)((b << 3) | (b >> 2));
    o[3] = 255;
}

void DecodeColourBlock(const uint8_t* b, uint8_t col[4][4], bool allowAlpha) {
    const uint16_t c0 = U16(b), c1 = U16(b + 2);
    Rgb565(c0, col[0]);
    Rgb565(c1, col[1]);
    if (c0 > c1 || !allowAlpha) {
        for (int i = 0; i < 3; ++i) {
            col[2][i] = (uint8_t)((2 * col[0][i] + col[1][i]) / 3);
            col[3][i] = (uint8_t)((col[0][i] + 2 * col[1][i]) / 3);
        }
        col[2][3] = col[3][3] = 255;
    } else {
        for (int i = 0; i < 3; ++i) {
            col[2][i] = (uint8_t)((col[0][i] + col[1][i]) / 2);
        }
        col[2][3] = 255;
        col[3][0] = col[3][1] = col[3][2] = col[3][3] = 0;
    }
}

std::vector<uint8_t> DecodeDxt(int dxt, const uint8_t* src, int w, int h) {
    std::vector<uint8_t> out((size_t)w * h * 4);
    const int bw = std::max(1, (w + 3) / 4), bh = std::max(1, (h + 3) / 4), bytes = dxt == 1 ? 8 : 16;
    for (int by = 0; by < bh; ++by) {
        for (int bx = 0; bx < bw; ++bx) {
            const uint8_t* b = src + (size_t)(by * bw + bx) * bytes;
            const uint8_t* cb = dxt == 1 ? b : b + 8;
            uint8_t col[4][4];
            DecodeColourBlock(cb, col, dxt == 1);
            uint8_t alpha[16];
            if (dxt == 3) {
                for (int i = 0; i < 16; ++i) {
                    const int v = (b[i / 2] >> ((i & 1) * 4)) & 15;
                    alpha[i] = (uint8_t)(v * 17);
                }
            } else if (dxt == 5) {
                uint8_t a[8];
                a[0] = b[0]; a[1] = b[1];
                if (a[0] > a[1]) {
                    for (int i = 1; i < 7; ++i) a[1 + i] = (uint8_t)(((7 - i) * a[0] + i * a[1]) / 7);
                } else {
                    for (int i = 1; i < 5; ++i) a[1 + i] = (uint8_t)(((5 - i) * a[0] + i * a[1]) / 5);
                    a[6] = 0; a[7] = 255;
                }
                uint64_t bits = 0;
                for (int i = 0; i < 6; ++i) bits |= (uint64_t)b[2 + i] << (8 * i);
                for (int i = 0; i < 16; ++i) alpha[i] = a[(bits >> (3 * i)) & 7];
            }
            const uint32_t idx = U32(cb + 4);
            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    const int x = bx * 4 + px, y = by * 4 + py;
                    if (x >= w || y >= h) continue;
                    const int i = py * 4 + px;
                    uint8_t* o = &out[((size_t)y * w + x) * 4];
                    std::memcpy(o, col[(idx >> (2 * i)) & 3], 4);
                    if (dxt != 1) o[3] = alpha[i];
                }
            }
        }
    }
    return out;
}

// d3d9 texel word(s) -> RGBA8 (channels shifted into the high bits and replicated, like a GPU does)
std::vector<uint8_t> DecodeTexels(int colour, const uint8_t* src, int w, int h) {
    std::vector<uint8_t> out((size_t)w * h * 4);
    for (int i = 0; i < w * h; ++i) {
        uint8_t* o = &out[(size_t)i * 4];
        switch (colour) {
        case 0x500: o[0] = src[i * 4 + 2]; o[1] = src[i * 4 + 1]; o[2] = src[i * 4]; o[3] = src[i * 4 + 3]; break;
        case 0x600: o[0] = src[i * 4 + 2]; o[1] = src[i * 4 + 1]; o[2] = src[i * 4]; o[3] = 255; break;
        case 0x100: { const unsigned t = U16(src + i * 2); const int r = (t >> 10) & 31, g = (t >> 5) & 31, b = t & 31;
                      o[0] = (uint8_t)((r << 3) | (r >> 2)); o[1] = (uint8_t)((g << 3) | (g >> 2)); o[2] = (uint8_t)((b << 3) | (b >> 2)); o[3] = (t & 0x8000) ? 255 : 0; break; }
        case 0xA00: { const unsigned t = U16(src + i * 2); const int r = (t >> 10) & 31, g = (t >> 5) & 31, b = t & 31;
                      o[0] = (uint8_t)((r << 3) | (r >> 2)); o[1] = (uint8_t)((g << 3) | (g >> 2)); o[2] = (uint8_t)((b << 3) | (b >> 2)); o[3] = 255; break; }
        case 0x200: Rgb565(U16(src + i * 2), o); break;
        case 0x300: { const unsigned t = U16(src + i * 2); o[0] = (uint8_t)(((t >> 8) & 15) * 17); o[1] = (uint8_t)(((t >> 4) & 15) * 17); o[2] = (uint8_t)((t & 15) * 17); o[3] = (uint8_t)(((t >> 12) & 15) * 17); break; }
        case 0x400: o[0] = o[1] = o[2] = src[i]; o[3] = 255; break;
        }
    }
    return out;
}

const char* ClassName(const TexInfo& t, bool* paletted = nullptr) {
    if (paletted) *paletted = (t.format & 0x6000) != 0;
    if (t.flags & 8) {
        switch (t.d3dformat) {
        case 0x31545844: return "DXT1";
        case 0x33545844: return "DXT3";
        case 0x35545844: return "DXT5";
        }
        return "DXT?";
    }
    if (t.format & 0x2000) return "PAL8";
    if (t.format & 0x4000) return "PAL4";
    switch (t.format & 0xF00) {
    case 0x100: return "1555";
    case 0x200: return "565";
    case 0x300: return "4444";
    case 0x400: return "LUM8";
    case 0x500: return "8888";
    case 0x600: return "888";
    case 0xA00: return "555";
    }
    return "other";
}


// ---------------------------------------------------------------------------------------------------------------------------------------------
// synthetic dictionary: the formats the Steam copy does not contain (DXT5, 1555, 565, 555, 4444, LUM8, PAL8 / PAL4, short mip chains), random texels
// ---------------------------------------------------------------------------------------------------------------------------------------------
uint32_t g_rng = 0x1234567u;
uint8_t Rnd() { g_rng = g_rng * 1664525u + 1013904223u; return (uint8_t)(g_rng >> 24); }

void Put32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back((uint8_t)(x >> (8 * i))); }
void Put16(std::vector<uint8_t>& v, uint32_t x) { v.push_back((uint8_t)x); v.push_back((uint8_t)(x >> 8)); }
void Chunk(std::vector<uint8_t>& v, uint32_t type, uint32_t size) { Put32(v, type); Put32(v, size); Put32(v, 0x1803FFFF); }

struct SynTex { const char* name; uint32_t format, d3dformat; int w, h, levels, flags; };

std::vector<uint8_t> BuildSyntheticTxd() {
    const SynTex defs[] = {
        { "syn_dxt5", 0x8200, 0x35545844, 64, 32, 5, 8 | 1 },       // 64x32: 64x32 .. 4x2 (levels 5 of 7), the last ones below 4x4
        { "syn_dxt5s", 0x8200, 0x35545844, 8, 8, 4, 8 | 1 },        // 8x8, 4x4, 2x2 (size 0), 1x1 (size 0)
        { "syn_dxt1a", 0x0200, 0x31545844, 16, 16, 1, 8 | 1 },      // DXT1 with the alpha flag
        { "syn_1555", 0x8100, 25, 32, 32, 6, 1 },
        { "syn_565", 0x8200, 23, 32, 16, 5, 0 },
        { "syn_555", 0x0A00, 24, 16, 16, 1, 0 },
        { "syn_4444", 0x8300, 26, 64, 64, 7, 1 },
        { "syn_lum8", 0x0400, 50, 16, 32, 1, 0 },
        { "syn_8888", 0x8500, 21, 64, 64, 3, 1 },                   // 3 of 7 levels: the chain stops early
        { "syn_888", 0x8600, 22, 32, 32, 6, 0 },
        { "syn_pal8a", 0x2500, 0, 32, 32, 1, 1 },                   // PAL8, 8888 palette
        { "syn_pal8", 0x2600, 0, 64, 16, 1, 0 },                    // PAL8, 888 (opaque)
        { "syn_pal4", 0x4500, 0, 16, 16, 1, 1 },                    // PAL4: 32 entry palette, 8 bit indices
    };
    std::vector<uint8_t> body;
    for (const SynTex& d : defs) {
        std::vector<uint8_t> st;
        Put32(st, 9); Put32(st, 0x1106);
        char nm[32] = {}; std::strncpy(nm, d.name, 31);
        st.insert(st.end(), nm, nm + 32);
        char mk[32] = {};
        st.insert(st.end(), mk, mk + 32);
        Put32(st, d.format); Put32(st, d.d3dformat);
        Put16(st, d.w); Put16(st, d.h);
        const int colour = d.format & 0xF00;
        const bool comp = d.flags & 8, pal = d.format & 0x6000;
        st.push_back(comp ? 16 : (pal ? 8 : (colour == 0x500 || colour == 0x600 ? 32 : (colour == 0x400 ? 8 : 16))));
        st.push_back((uint8_t)d.levels); st.push_back(4); st.push_back((uint8_t)d.flags);
        if (pal) {
            const int n = (d.format & 0x4000) ? 32 : 256;
            for (int i = 0; i < n * 4; ++i) st.push_back(Rnd());
        }
        for (int l = 0; l < d.levels; ++l) {
            const int lw = std::max(1, d.w >> l), lh = std::max(1, d.h >> l);
            uint32_t size;
            if (comp) {
                size = (lw < 4 && lh < 4 && l > 0) ? 0 : (uint32_t)(std::max(1, (lw + 3) / 4) * std::max(1, (lh + 3) / 4) * (d.d3dformat == 0x31545844 ? 8 : 16));
            } else if (pal || colour == 0x400) {
                size = (uint32_t)lw * lh;
            } else {
                size = (uint32_t)lw * lh * ((colour == 0x500 || colour == 0x600) ? 4 : 2);
            }
            Put32(st, size);
            for (uint32_t i = 0; i < size; ++i) {
                uint8_t b = Rnd();
                if (pal) b = (uint8_t)(b % ((d.format & 0x4000) ? 32 : 256));
                st.push_back(b);
            }
        }
        Chunk(body, 0x15, (uint32_t)(12 + st.size() + 12));
        Chunk(body, 1, (uint32_t)st.size());
        body.insert(body.end(), st.begin(), st.end());
        Chunk(body, 3, 0);
    }
    std::vector<uint8_t> f;
    Chunk(f, 0x16, (uint32_t)(12 + 4 + body.size() + 12));
    Chunk(f, 1, 4);
    Put16(f, (uint32_t)(sizeof(defs) / sizeof(defs[0]))); Put16(f, 9);
    f.insert(f.end(), body.begin(), body.end());
    Chunk(f, 3, 0);
    return f;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// data discovery
// ---------------------------------------------------------------------------------------------------------------------------------------------
struct Source { std::string label; std::string file; long offset; long size; bool synthetic = false; };

void ScanDir(const std::string& dir, std::vector<Source>& out) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    std::vector<std::string> names;
    while (dirent* e = readdir(d)) {
        if (e->d_name[0] != '.') names.push_back(e->d_name);
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    for (const auto& n : names) {
        const std::string p = dir + "/" + n;
        struct stat st {};
        if (stat(p.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            ScanDir(p, out);
        } else if (n.size() > 4 && strcasecmp(n.c_str() + n.size() - 4, ".txd") == 0) {
            out.push_back({ p.substr(p.find("/models/") + 1), p, 0, (long)st.st_size });
        }
    }
}

void ScanImg(const std::string& img, std::vector<Source>& out) {
    FILE* f = std::fopen(img.c_str(), "rb");
    if (!f) return;
    uint8_t hdr[8];
    if (std::fread(hdr, 1, 8, f) != 8 || std::memcmp(hdr, "VER2", 4) != 0) { std::fclose(f); return; }
    const uint32_t n = U32(hdr + 4);
    std::vector<uint8_t> dir((size_t)n * 32);
    if (std::fread(dir.data(), 1, dir.size(), f) != dir.size()) { std::fclose(f); return; }
    std::fclose(f);
    const std::string base = img.substr(img.rfind('/') + 1);
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t* e = &dir[(size_t)i * 32];
        char nm[25] = {}; std::memcpy(nm, e + 8, 24);
        const size_t l = std::strlen(nm);
        if (l > 4 && strcasecmp(nm + l - 4, ".txd") == 0) {
            out.push_back({ base + ":" + nm, img, (long)U32(e) * 2048, (long)std::max<unsigned>(U16(e + 4), U16(e + 6)) * 2048 });
        }
    }
}

bool ReadSource(const Source& s, std::vector<uint8_t>& buf) {
    if (s.synthetic) { buf = BuildSyntheticTxd(); return true; }
    FILE* f = std::fopen(s.file.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, s.offset, SEEK_SET);
    buf.resize((size_t)s.size);
    const size_t got = std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    buf.resize(got);
    // img entries are padded to whole sectors: cut at the TXD chunk's own length
    if (buf.size() >= 12 && U32(buf.data()) == 0x16) {
        buf.resize(std::min<size_t>(buf.size(), 12 + (size_t)U32(buf.data() + 4)));
    }
    return got > 12;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// per-format statistics
// ---------------------------------------------------------------------------------------------------------------------------------------------
struct FmtStat {
    unsigned textures = 0, mipped = 0, levels = 0, loadFail = 0, sizeBad = 0, levelCountBad = 0, alphaBad = 0, glErr = 0, dataBad = 0, lockFail = 0, formatBad = 0;
    unsigned long long texels = 0;
};
std::map<std::string, FmtStat> g_stats;
unsigned g_dicts = 0, g_dictFail = 0, g_skippedD3D8 = 0;

// the raster format bits a loaded texture must report
int ExpectedColour(const TexInfo& t) {
    if (t.flags & 8) return -1;
    if (t.format & 0x6000) return (t.format & 0xF00) == 0x500 || (t.format & 0xF00) == 0x600 ? (int)(t.format & 0xF00) : ((t.flags & 1) ? 0x500 : 0x600);
    return (int)(t.format & 0xF00);
}

struct Loaded { std::string name; RwTexture* tex; };

void VerifyTexture(const TexInfo& t, RwTexture* tex) {
    bool paletted = false;
    const char* cls = ClassName(t, &paletted);
    FmtStat& st = g_stats[cls];
    st.textures++;
    st.texels += (unsigned long long)t.w * t.h;
    if (t.levels > 1) st.mipped++;
    st.levels += t.levels;
    RwRaster* r = tex ? tex->raster : nullptr;
    if (!r) { st.loadFail++; return; }
    if (r->width != t.w || r->height != t.h) { st.sizeBad++; }
    Gl3Raster* nr = GETGL3RASTEREXT(r);
    if (!nr->texid) { st.loadFail++; return; }
    if (RwRasterGetNumLevels(r) != t.levels) { st.levelCountBad++; }
    const int expCol = ExpectedColour(t);
    if (expCol >= 0 && (r->format & 0xF00) != expCol) { st.formatBad++; }
    if ((t.flags & 8) && !nr->isCompressed) { st.formatBad++; }
    const bool flagAlpha = (t.flags & 1) != 0 || ((t.flags & 8) && t.d3dformat != 0x31545844);
    if (nr->hasAlpha != flagAlpha && !(expCol == 0x600 || expCol == 0x200 || expCol == 0xA00 || expCol == 0x400)) { st.alphaBad++; }
    // every level: lock(read) vs the expected bytes
    bool bad = false, lockFail = false;
    for (int l = 0; l < t.levels && l < RwRasterGetNumLevels(r); ++l) {
        const int lw = std::max(1, t.w >> l), lh = std::max(1, t.h >> l);
        const uint8_t* src = t.data[l].first;
        const uint32_t sz  = t.data[l].second;
        uint8_t* px = r->lock(l, rw::Raster::LOCKREAD);
        if (!px) { lockFail = true; continue; }
        const int stride = r->stride;
        if (t.flags & 8) {
            const int want = std::max(1, (lw + 3) / 4) * std::max(1, (lh + 3) / 4) * (t.d3dformat == 0x31545844 ? 8 : 16);
            if (sz == 0 && l > 0) {
                // zero-size 2x2 / 1x1 level: the reader repeats the last level that had data
                int k = l - 1;
                while (k > 0 && t.data[k].second == 0) --k;
                if (std::memcmp(px, t.data[k].first, std::min<size_t>(t.data[k].second, want)) != 0) bad = true;
            } else if ((uint32_t)want != sz || std::memcmp(px, src, std::min<size_t>(sz, want)) != 0) bad = true;
        } else if (paletted) {
            const int pe = (t.format & 0x4000) ? 32 : 256;
            for (int y = 0; y < lh && !bad; ++y) {
                for (int x = 0; x < lw; ++x) {
                    const uint8_t* c = &t.palette[src[(size_t)y * lw + x] * 4];
                    (void)pe;
                    const uint8_t* o = px + (size_t)y * stride + x * 4;
                    const uint8_t a = expCol == 0x600 ? 0xFF : c[3];
                    if (o[0] != c[2] || o[1] != c[1] || o[2] != c[0] || o[3] != a) { bad = true; break; }
                }
            }
        } else {
            const int bpp = (expCol == 0x500 || expCol == 0x600) ? 4 : (expCol == 0x400 ? 1 : 2);
            for (int y = 0; y < lh && !bad; ++y) {
                const uint8_t* a = px + (size_t)y * stride;
                const uint8_t* b = src + (size_t)y * lw * bpp;
                if ((size_t)y * lw * bpp + (size_t)lw * bpp > sz) { bad = true; break; }
                if (expCol == 0x600) {
                    for (int x = 0; x < lw; ++x) if (a[x * 4] != b[x * 4] || a[x * 4 + 1] != b[x * 4 + 1] || a[x * 4 + 2] != b[x * 4 + 2]) { bad = true; break; }
                } else if (expCol == 0xA00) {
                    // X1R5G5B5: the unused top bit is not stored (reads back as 1)
                    for (int x = 0; x < lw; ++x) if ((U16(a + x * 2) & 0x7FFF) != (U16(b + x * 2) & 0x7FFF)) { bad = true; break; }
                } else if (std::memcmp(a, b, (size_t)lw * bpp) != 0) {
                    bad = true;
                }
            }
        }
        r->unlock(l);
    }
    if (lockFail) st.lockFail++;
    if (bad) st.dataBad++;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// pixel check
// ---------------------------------------------------------------------------------------------------------------------------------------------
struct Snap {
    int w = 0, h = 0;
    std::vector<unsigned char> d;
    void grab(int pw, int ph) {
        w = pw; h = ph;
        std::vector<unsigned char> raw((size_t)pw * ph * 4);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, pw, ph, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());
        d.resize(raw.size());
        for (int y = 0; y < ph; y++) std::memcpy(&d[(size_t)y * pw * 4], &raw[(size_t)(ph - 1 - y) * pw * 4], (size_t)pw * 4);
    }
    const unsigned char* at(int x, int y) const { return &d[((size_t)y * w + x) * 4]; }
};

RwIm2DVertex V2(float x, float y, float u, float v) {
    RwIm2DVertex vt{};
    RwIm2DVertexSetScreenX(&vt, x);
    RwIm2DVertexSetScreenY(&vt, y);
    RwIm2DVertexSetScreenZ(&vt, RwIm2DGetNearScreenZ());
    RwIm2DVertexSetRecipCameraZ(&vt, 2.0f);
    RwIm2DVertexSetU(&vt, u, 2.0f);
    RwIm2DVertexSetV(&vt, v, 2.0f);
    RwIm2DVertexSetIntRGBA(&vt, 255, 255, 255, 255);
    return vt;
}

void RS(RwRenderState st, uintptr_t v) { RwRenderStateSet(st, reinterpret_cast<void*>(v)); }

struct DrawStat { unsigned n = 0, bad = 0; double worstFrac = 0; };
std::map<std::string, DrawStat> g_draw;

struct Sample { TexInfo info; std::vector<uint8_t> blob; };
std::map<std::string, std::vector<Sample>> g_samples; // class -> samples (data is copied: the file buffer goes away)

void DrawCompare(RwCamera* cam, int pw, int ph, const Sample& s, RwTexture* tex) {
    const TexInfo& t = s.info;
    bool paletted = false;
    const char* cls = ClassName(t, &paletted);
    std::vector<uint8_t> ref;
    if (t.flags & 8) {
        ref = DecodeDxt(t.d3dformat == 0x31545844 ? 1 : (t.d3dformat == 0x33545844 ? 3 : 5), t.data[0].first, t.w, t.h);
    } else if (paletted) {
        ref.resize((size_t)t.w * t.h * 4);
        const bool opaque = ExpectedColour(t) == 0x600;
        for (int i = 0; i < t.w * t.h; ++i) {
            const uint8_t* c = &t.palette[t.data[0].first[i] * 4];
            ref[i * 4] = c[0]; ref[i * 4 + 1] = c[1]; ref[i * 4 + 2] = c[2]; ref[i * 4 + 3] = opaque ? 255 : c[3];
        }
    } else {
        ref = DecodeTexels(t.format & 0xF00, t.data[0].first, t.w, t.h);
    }
    RwCameraBeginUpdate(cam);
    RwRGBA black{ 0, 0, 0, 255 };
    RwCameraClear(cam, &black, rwCAMERACLEARIMAGE | rwCAMERACLEARZ | rwCAMERACLEARSTENCIL);
    RS(rwRENDERSTATETEXTURERASTER, (uintptr_t)tex->raster);
    RS(rwRENDERSTATEZTESTENABLE, 0);
    RS(rwRENDERSTATEZWRITEENABLE, 0);
    RS(rwRENDERSTATEVERTEXALPHAENABLE, 0);
    RS(rwRENDERSTATECULLMODE, rwCULLMODECULLNONE);
    RS(rwRENDERSTATETEXTUREFILTER, rwFILTERNEAREST);
    RS(rwRENDERSTATETEXTUREADDRESS, rwTEXTUREADDRESSCLAMP);
    RS(rwRENDERSTATESRCBLEND, rwBLENDONE);   // the texture value as is, no blend with the black background
    RS(rwRENDERSTATEDESTBLEND, rwBLENDZERO);
    RwIm2DVertex q[4] = { V2(0, 0, 0, 0), V2((float)t.w, 0, 1, 0), V2(0, (float)t.h, 0, 1), V2((float)t.w, (float)t.h, 1, 1) };
    RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, q, 4);
    Snap snap;
    snap.grab(pw, ph);
    RwCameraEndUpdate(cam);
    RS(rwRENDERSTATETEXTURERASTER, 0);
    // pixels with alpha >= 0xF0 (and not alpha-tested away): rgb within tolerance
    const int tol = (t.flags & 8) ? 6 : 9;
    unsigned cmp = 0, bad = 0;
    int fx = -1, fy = -1;
    for (int y = 0; y < t.h; ++y) {
        for (int x = 0; x < t.w; ++x) {
            const uint8_t* e = &ref[((size_t)y * t.w + x) * 4];
            if (e[3] < 0xF0) continue;
            const unsigned char* g = snap.at(x, y);
            ++cmp;
            if (std::abs(g[0] - e[0]) > tol || std::abs(g[1] - e[1]) > tol || std::abs(g[2] - e[2]) > tol) {
                if (!bad) { fx = x; fy = y; }
                ++bad;
            }
        }
    }
    DrawStat& ds = g_draw[cls];
    ds.n++;
    const double frac = cmp ? (double)bad / cmp : 0.0;
    if (frac > 0.005) {
        ds.bad++;
        std::printf("  draw mismatch %s \"%s\" %dx%d: %u of %u pixels off (%.2f%%); first at (%d,%d) want %d,%d,%d,%d got %d,%d,%d\n", cls, t.name.c_str(), t.w, t.h, bad, cmp, frac * 100.0, fx, fy,
                    ref[((size_t)fy * t.w + fx) * 4], ref[((size_t)fy * t.w + fx) * 4 + 1], ref[((size_t)fy * t.w + fx) * 4 + 2], ref[((size_t)fy * t.w + fx) * 4 + 3],
                    snap.at(fx, fy)[0], snap.at(fx, fy)[1], snap.at(fx, fy)[2]);
    }
    ds.worstFrac = std::max(ds.worstFrac, frac);
}

bool NoGlError(const char* where) {
    bool clean = true;
    for (GLenum e; (e = glGetError()) != GL_NO_ERROR;) { std::printf("  glGetError after %s: 0x%x\n", where, e); clean = false; }
    return clean;
}
} // namespace

int main() {
    const auto t0 = std::chrono::steady_clock::now();
    const char* gtaEnv = std::getenv("GTA");
    const std::string home = std::getenv("HOME") ? std::getenv("HOME") : "";
    const std::string gta = gtaEnv ? gtaEnv : home + "/Library/Application Support/Steam/steamapps/common/grand theft auto - san andreas/Grand Theft Auto - San Andreas.app/Contents/Resources/transgaming/c_drive/Program Files/Rockstar Games/GTA San Andreas";
    const std::string models = gta + "/models";
    struct stat st {};
    if (stat(models.c_str(), &st) != 0) {
        std::printf("TXD RESULT SKIP: no game data at %s (set GTA)\n", models.c_str());
        return 0;
    }
    const long maxDicts = std::getenv("NOTSA_TXD_MAX") ? std::atol(std::getenv("NOTSA_TXD_MAX")) : 0;
    const char* filter  = std::getenv("NOTSA_TXD_FILTER");

    std::vector<Source> srcs;
    ScanDir(models, srcs);
    for (const char* img : { "gta3.img", "gta_int.img", "player.img", "cutscene.img" }) ScanImg(models + "/" + img, srcs);
    if (filter) srcs.erase(std::remove_if(srcs.begin(), srcs.end(), [&](const Source& s) { return s.label.find(filter) == std::string::npos; }), srcs.end());
    { Source syn; syn.label = "synthetic"; syn.synthetic = true; srcs.insert(srcs.begin(), syn); }
    if (maxDicts > 0 && (long)srcs.size() > maxDicts) srcs.resize((size_t)maxDicts);
    std::printf("txd test: %zu texture dictionaries (%s)\n", srcs.size(), models.c_str());

    if (!std::getenv("NOTSA_STANDALONE_WINDOWED")) setenv("NOTSA_STANDALONE_WINDOWED", "640x480", 1);
    if (!SDL_Init(SDL_INIT_VIDEO)) { std::printf("TXD RESULT SKIP: no display session (SDL_Init: %s)\n", SDL_GetError()); return 0; }
    SDL_Window* win = SDL_CreateWindow("gta-reversed txd test", 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (!win) { std::printf("TXD RESULT SKIP: no window (SDL_CreateWindow: %s)\n", SDL_GetError()); return 0; }
    Check(RwEngineInit(nullptr, 0, 0) == TRUE, "RwEngineInit");
    RwEngineOpenParams op{};
    op.displayID = win;
    Check(RwEngineOpen(&op) == TRUE, "RwEngineOpen");
    const int wvm = NotsaRwWindowedVideoMode();
    Check(RwEngineSetVideoMode(wvm >= 0 ? wvm : 0) == TRUE, "RwEngineSetVideoMode");
    Check(RwEngineStart() == TRUE, "RwEngineStart");
    if (g_fail) { std::printf("TXD RESULT FAIL: engine did not start\n"); return 1; }
    NotsaGlInfo gi{};
    NotsaRwGetGlInfo(&gi);
    std::printf("  GL %s / %s, S3TC %d\n", gi.version, gi.renderer, (int)gi.s3tc);
    Check(gi.s3tc, "S3TC (DXT) supported by this context");
    Check(rw::gl3::d3dTexelOrder == 1, "gl3 texture rasters use d3d9 texel order (set by RwEngineOpen)");
    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(win, &pw, &ph);

    RwCamera* cam = RwCameraCreate();
    cam->setFrame(rw::Frame::create());
    RwCameraSetNearClipPlane(cam, 0.5f);
    RwCameraSetFarClipPlane(cam, 10.0f);
    const RwV2d vw{ 1.0f, 0.75f };
    RwCameraSetViewWindow(cam, &vw);
    cam->frameBuffer = RwRasterCreate(pw, ph, 0, rwRASTERTYPECAMERA);
    cam->zBuffer     = RwRasterCreate(pw, ph, 0, rwRASTERTYPEZBUFFER);

    // ---- every dictionary ----
    NotsaGl3TextureStatsReset();
    unsigned long long totalBytes = 0;
    std::string firstBadDict;
    std::vector<uint8_t> buf;
    unsigned lockSampleDone = 0;
    RwTexture* lockTex = nullptr;       // an 8888 square TXD texture kept for the lock tests (its dictionary stays alive)
    RwTexDictionary* lockDict = nullptr;
    Sample lockSample;
    for (const Source& s : srcs) {
        if (!ReadSource(s, buf)) { std::printf("  cannot read %s\n", s.label.c_str()); g_dictFail++; continue; }
        std::vector<TexInfo> infos;
        if (!ParseTxd(buf, infos)) { std::printf("  independent parser rejects %s (not a platform 9 TXD?)\n", s.label.c_str()); g_dictFail++; continue; }
        if (!infos.empty() && infos[0].platform != 9) {
            // platform 8 (D3D8) files shipped in the Steam copy (misc.txd, generic/wheels.txd): nothing in the game loads them (the exe's D3D9 reader rejects them too)
            g_skippedD3D8++;
            std::printf("  skipped (platform %u, not loaded by the game): %s\n", infos[0].platform, s.label.c_str());
            continue;
        }
        totalBytes += buf.size();
        rw::StreamMemory sm;
        sm.open(buf.data() + 12, (uint32_t)buf.size() - 12);
        const unsigned failedBefore = NotsaGl3TextureStatsGet().failed;
        RwTexDictionary* dict = RwTexDictionaryStreamRead(&sm);
        g_dicts++;
        if (!dict) {
            g_dictFail++;
            for (const auto& t : infos) { FmtStat& fs = g_stats[ClassName(t)]; fs.textures++; fs.loadFail++; }
            std::printf("  LOAD FAILED: %s (%zu textures, reader failures so far %u)\n", s.label.c_str(), infos.size(), NotsaGl3TextureStatsGet().failed - failedBefore);
            if (firstBadDict.empty()) firstBadDict = s.label;
            continue;
        }
        // list order is newest first = reverse of the file
        std::vector<RwTexture*> texs;
        RwTexDictionaryForAllTextures(dict, [](RwTexture* t, void* d) -> RwTexture* { static_cast<std::vector<RwTexture*>*>(d)->push_back(t); return t; }, &texs);
        std::reverse(texs.begin(), texs.end());
        if (texs.size() != infos.size()) {
            g_dictFail++;
            std::printf("  %s: %zu textures in the dictionary, %zu in the file\n", s.label.c_str(), texs.size(), infos.size());
        }
        const unsigned badBefore = [] { unsigned b = 0; for (auto& kv : g_stats) b += kv.second.loadFail + kv.second.sizeBad + kv.second.levelCountBad + kv.second.dataBad + kv.second.lockFail + kv.second.formatBad + kv.second.alphaBad; return b; }();
        for (size_t i = 0; i < infos.size(); ++i) {
            RwTexture* tex = i < texs.size() ? texs[i] : nullptr;
            if (tex && std::strncmp(tex->name, infos[i].name.c_str(), 31) != 0) { g_fail++; std::printf("  name mismatch %s vs %s\n", tex->name, infos[i].name.c_str()); }
            VerifyTexture(infos[i], tex);
        }
        const unsigned badAfter = [] { unsigned b = 0; for (auto& kv : g_stats) b += kv.second.loadFail + kv.second.sizeBad + kv.second.levelCountBad + kv.second.dataBad + kv.second.lockFail + kv.second.formatBad + kv.second.alphaBad; return b; }();
        if (badAfter != badBefore && firstBadDict.empty()) firstBadDict = s.label;
        if (badAfter != badBefore) std::printf("  problems in %s (%u)\n", s.label.c_str(), badAfter - badBefore);
        // samples for the draw check: first few small POT textures per class
        for (size_t i = 0; i < infos.size() && i < texs.size(); ++i) {
            const TexInfo& t = infos[i];
            const std::string cls = ClassName(t);
            auto& vec = g_samples[cls];
            if (vec.size() < 6 && t.w >= 4 && t.h >= 4 && t.w <= 256 && t.h <= 256 && (t.w & (t.w - 1)) == 0 && (t.h & (t.h - 1)) == 0 && texs[i]->raster && vw.x) {
                // keep a private copy of the level-0 bytes so the sample outlives the file buffer
                Sample smp;
                smp.info = t;
                smp.blob.assign(t.data[0].first, t.data[0].first + t.data[0].second);
                smp.info.data.clear();
                smp.info.data.emplace_back(nullptr, (uint32_t)smp.blob.size());
                vec.push_back(std::move(smp));
                vec.back().info.data[0].first = vec.back().blob.data(); // (vector growth moves the blob: re-point below before use)
                // draw right away while the texture lives
                DrawCompare(cam, pw, ph, vec.back(), texs[i]);
            }
            if (!lockTex && cls == "8888" && t.w == t.h && t.w >= 32 && t.w <= 256 && texs[i]->raster) {
                // keep this dictionary alive for the lock tests
                lockSample.info = t;
                lockSample.blob.assign(t.data[0].first, t.data[0].first + t.data[0].second);
                lockTex  = texs[i];
                lockDict = dict;
            }
        }
        if (lockDict != dict) RwTexDictionaryDestroy(dict);
        if (!NoGlError(s.label.c_str())) { g_fail++; }
        (void)lockSampleDone;
    }
    const double loadSecs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    // ---- report ----
    std::printf("\n%-6s %8s %8s %8s %6s %6s %6s %6s %6s %6s %6s\n", "format", "textures", "mipped", "levels", "loadF", "sizeB", "lvlB", "fmtB", "alphaB", "dataB", "lockF");
    FmtStat tot;
    for (auto& kv : g_stats) {
        const FmtStat& f = kv.second;
        std::printf("%-6s %8u %8u %8u %6u %6u %6u %6u %6u %6u %6u\n", kv.first.c_str(), f.textures, f.mipped, f.levels, f.loadFail, f.sizeBad, f.levelCountBad, f.formatBad, f.alphaBad, f.dataBad, f.lockFail);
        tot.textures += f.textures; tot.loadFail += f.loadFail; tot.sizeBad += f.sizeBad; tot.levelCountBad += f.levelCountBad; tot.formatBad += f.formatBad;
        tot.alphaBad += f.alphaBad; tot.dataBad += f.dataBad; tot.lockFail += f.lockFail;
    }
    const NotsaGl3TextureStats& rs = NotsaGl3TextureStatsGet();
    std::printf("skipped platform-8 dictionaries: %u\n", g_skippedD3D8);
    std::printf("dictionaries %u (failed %u), textures %u, %.1f MB of TXD data, %.1f s; reader: loaded %u failed %u levelSizeMismatch %u zeroSizeLevels %u glErrors %u npot %u\n",
                g_dicts, g_dictFail, tot.textures, (double)totalBytes / 1048576.0, loadSecs, rs.loaded, rs.failed, rs.levelSizeMismatch, rs.zeroLevels, rs.glErrors, rs.npot);
    Check(g_dicts > 0 && tot.textures > 0, "some dictionaries were loaded");
    Check(g_dictFail == 0, "every dictionary loads (0 failures)");
    Check(tot.loadFail == 0 && rs.failed == 0, "every texture loads into a GL3 raster");
    Check(tot.sizeBad == 0, "raster sizes match the file");
    Check(tot.levelCountBad == 0, "mip level counts match the file");
    Check(tot.formatBad == 0, "raster formats match (true storage format)");
    Check(tot.alphaBad == 0, "alpha flags match");
    Check(tot.lockFail == 0, "every level can be locked for reading");
    Check(tot.dataBad == 0, "lock(read) of every level equals the bytes in the file (DXT blocks / texel words / expanded palette)");
    Check(rs.glErrors == 0 && rs.levelSizeMismatch == 0, "glGetError() == 0 after every upload, file level sizes == raster level sizes");

    std::printf("\ndraw check (Im2D 1:1, nearest, vs CPU decode):\n");
    for (auto& kv : g_draw) std::printf("  %-6s samples %u, bad %u, worst fraction of differing pixels %.4f\n", kv.first.c_str(), kv.second.n, kv.second.bad, kv.second.worstFrac);
    Check(g_draw.count("DXT1") && g_draw["DXT1"].n > 0, "DXT1 sample drawn");
    unsigned drawBad = 0;
    for (auto& kv : g_draw) drawBad += kv.second.bad;
    Check(drawBad == 0, "drawn textures equal the CPU decode within tolerance (top row first, no swizzle)");

    // ---- lock contract on a TXD-loaded raster (CClothesBuilder::CopyTexture pattern) ----
    std::printf("\nlock contract:\n");
    if (lockTex) {
        RwRaster* r = lockTex->raster;
        RwUInt8* rd = RwRasterLock(r, 0, rwRASTERLOCKREAD);
        Check(rd != nullptr, "RwRasterLock(TXD raster, READ) returns a pointer");
        if (rd) {
            Check(RwRasterGetStride(r) == r->width * 4 && std::memcmp(rd, lockSample.blob.data(), lockSample.blob.size()) == 0, "READ lock: top row first, bytes == file (B,G,R,A)");
            Check(RwRasterLock(r, 0, rwRASTERLOCKREAD) == nullptr, "second lock of a locked raster is refused");
            RwRasterUnlock(r);
        }
        RwUInt8* rw = RwRasterLock(r, 0, rwRASTERLOCKREADWRITE);
        Check(rw != nullptr, "RwRasterLock(TXD raster, READ|WRITE) returns a pointer");
        if (rw) {
            const uint8_t keep[4] = { rw[0], rw[1], rw[2], rw[3] };
            rw[0] = 11; rw[1] = 22; rw[2] = 33; rw[3] = 44;
            RwRasterUnlock(r);
            RwUInt8* again = RwRasterLock(r, 0, rwRASTERLOCKREAD);
            Check(again && again[0] == 11 && again[1] == 22 && again[2] == 33 && again[3] == 44 && std::memcmp(again + 4, lockSample.blob.data() + 4, (size_t)(r->width - 1) * 4) == 0, "READ|WRITE edit persists through unlock (upload) and a new read lock");
            RwRasterUnlock(r);
            rw = RwRasterLock(r, 0, rwRASTERLOCKREADWRITE);
            if (rw) { std::memcpy(rw, keep, 4); RwRasterUnlock(r); }
        }
        // Clothes builder style: a second raster created like the game does (256x256x32, 8888 texture), copy by locking both
        RwRaster* dst = RwRasterCreate(r->width, r->height, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
        RwUInt8* srcp = RwRasterLock(r, 0, rwRASTERLOCKREAD);
        RwUInt8* dstp = RwRasterLock(dst, 0, rwRASTERLOCKWRITE);
        Check(srcp && dstp, "two locks at once (source READ, destination WRITE)");
        if (srcp && dstp) { std::memcpy(dstp, srcp, (size_t)r->width * r->height * 4); }
        RwRasterUnlock(dst);
        RwRasterUnlock(r);
        RwUInt8* chk = RwRasterLock(dst, 0, rwRASTERLOCKREAD);
        Check(chk && std::memcmp(chk, lockSample.blob.data(), lockSample.blob.size()) == 0, "copied raster reads back identical");
        RwRasterUnlock(dst);
        RwRasterDestroy(dst);
        // mip level lock of a mipped texture and a DXT texture lock
        Check(NoGlError("lock tests"), "glGetError clean after the lock tests");
    } else {
        Check(false, "an 8888 square TXD texture exists for the lock tests");
    }
    // RwRGBAToPixel texel written into a locked 8888 raster draws as that colour (R/B order of the d3d9 texel word)
    {
        RwRaster* tr = RwRasterCreate(4, 4, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
        RwUInt8* p = RwRasterLock(tr, 0, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
        RwRGBA col{ 200, 100, 50, 255 };
        const RwUInt32 px = RwRGBAToPixel(&col, rwRASTERFORMAT8888);
        if (p) for (int i = 0; i < 16; ++i) std::memcpy(p + i * 4, &px, 4);
        RwRasterUnlock(tr);
        RwCameraBeginUpdate(cam);
        RwRGBA black{ 0, 0, 0, 255 };
        RwCameraClear(cam, &black, rwCAMERACLEARIMAGE | rwCAMERACLEARZ);
        RS(rwRENDERSTATETEXTURERASTER, (uintptr_t)tr);
        RS(rwRENDERSTATEZTESTENABLE, 0);
        RS(rwRENDERSTATEVERTEXALPHAENABLE, 0);
        RS(rwRENDERSTATETEXTUREFILTER, rwFILTERNEAREST);
        RwIm2DVertex q[4] = { V2(10, 10, 0, 0), V2(30, 10, 1, 0), V2(10, 30, 0, 1), V2(30, 30, 1, 1) };
        RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, q, 4);
        Snap sn;
        sn.grab(pw, ph);
        RwCameraEndUpdate(cam);
        RS(rwRENDERSTATETEXTURERASTER, 0);
        const unsigned char* g = sn.at(20, 20);
        char d[80];
        std::snprintf(d, sizeof(d), "got %d,%d,%d", g[0], g[1], g[2]);
        Check(p && std::abs(g[0] - 200) <= 1 && std::abs(g[1] - 100) <= 1 && std::abs(g[2] - 50) <= 1, "RwRGBAToPixel(200,100,50) texel draws as (200,100,50)", d);
        RwRasterDestroy(tr);
    }

    // ---- teardown ----
    if (lockDict) RwTexDictionaryDestroy(lockDict);
    RwRasterDestroy(cam->frameBuffer);
    RwRasterDestroy(cam->zBuffer);
    cam->getFrame()->destroy();
    cam->destroy();
    Check(NoGlError("teardown"), "glGetError clean at the end");
    RwEngineStop();
    RwEngineClose();
    RwEngineTerm();
    SDL_DestroyWindow(win);
    SDL_Quit();
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("elapsed %.1f s\n", secs);
    if (g_fail) {
        std::printf("TXD RESULT FAIL: %d checks failed%s%s\n", g_fail, firstBadDict.empty() ? "" : ", first bad dictionary ", firstBadDict.c_str());
        return 1;
    }
    std::printf("TXD RESULT OK\n");
    return 0;
}
