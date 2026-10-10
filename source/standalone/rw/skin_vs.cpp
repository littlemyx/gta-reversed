// P2B-24a: the exe's D3D9 vertex-shader COMPOSER of the RenderWare skin pipeline (RW "vertexshader" module), ported 1:1.
//
// The exe does not ship precompiled skin shaders: for every (light set, flags) key it concatenates a vs_1_1 / vs_2_0 / vs_2_x text with sprintf, assembles it with its statically
// linked D3DXAssembleShader and keeps the shader in a 256-entry cache. The skin render callback (0x7C8060, ported elsewhere) asks this module for the shader of the
// current key and for the constant register layout that goes with it. All addresses are .text of gta_sa_compact.exe:
//   0x75EDD0 NumLightConstants      0x75EE60 cache reset/free       0x75EED0 cache lookup (GetVertexShader)       0x75F0B0 compose front: register layout + text + assemble +
//   CreateVertexShader (0x7652AE D3DXAssembleShader wrapper, 0x7FAC60 CreateVertexShader wrapper)     0x75F240..0x760C4D the text builder (strings at 0x8D6420..0x8D7364 in .data)
//   0x760CF0 init (shader model from the caps, light-constant table at 0xC94AF8)
//
// Deliberate differences:
//  * the exe assembles with D3DXAssembleShader; we load D3DAssemble from d3dcompiler_47.dll at run time (Windows and Wine). Missing DLL / function: GetVertexShader returns null and logs once.
//  * the exe appends the text to a 4 KB static buffer (0xC93AF8) without a bound check; ComposeText returns a std::string.
//  * CreateVertexShader (0x7FAC60) also stores -1 in the exe's vertex-shader state cache 0x8E2448 on success: that cache belongs to the pipeline code of the caller.
//  * the model / constant count come from the caps of rw::d3d::d3ddevice (Init) instead of the exe's caps copy at 0xC9BF00; the `and al, 0xC1` the exe's init applies to its caps copy (0xC9BFEC)
//    in the vs_1_1 branch is not reproduced (it only touches the exe's private copy).
//  * the first cache entry's initial shader value (-1, a bogus pointer that a key of 0xFFFFFFFF would return) is kept for the lookup logic but never returned as a shader.
// Needs only fakerw + librw + the CRT; excluded from the unity build.
#ifdef NOTSA_RW_LIBRW
#include "skin_vs.h"
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>

#include <cstdio>
#include <cstring>

namespace notsa::skinvs {
namespace {

using u8  = std::uint8_t;
using u32 = std::uint32_t;

// 0x87522C: the swizzle characters "xyzw" (movsx'ed one by one) and, at 0x875230.., the table of swizzle strings indexed by a component count (the exe indexes it
// with ebp*4 + 0x87522C; index 1.. = "x", "xy", "xyz", "xyzw"; 5.. = "xxx", "yyy", "zzz", "www" are used as per-light replicated swizzles at 0x875240 + 4*i)
// NOTE: the exe indexes these bytes with the weight index i < N, N up to 7 (key.b[2] bits 1..3). Real skins have N <= 4; for N >= 5 the exe reads on into the pointer table that
// follows "xyzw" (bytes of the little-endian pointers 0x8D6418, 0x8D6414, ... which are NUL / control / 0x8D characters), reproduced here byte for byte.
constexpr signed char  kComp[36]   = { 'x', 'y', 'z', 'w', 0x18, 0x64, (signed char)0x8D, 0, 0x14, 0x64, (signed char)0x8D, 0, 0x10, 0x64, (signed char)0x8D, 0, 0x08, 0x64, (signed char)0x8D, 0,
                                       0x04, 0x64, (signed char)0x8D, 0, 0x00, 0x64, (signed char)0x8D, 0, (signed char)0xFC, 0x63, (signed char)0x8D, 0, (signed char)0xF8, 0x63, (signed char)0x8D, 0 };
constexpr const char* kSwz[9]     = { nullptr, "x", "xy", "xyz", "xyzw", "xxx", "yyy", "zzz", "www" };

// 0xC94AF8: constants taken by the texture-coordinate generation mode M (filled by the init 0x760CF0: [1]=[2]=[6]=[7]=[9]=2, [3]=[10]=4, [11]=6)
constexpr int kTexGenConsts[64] = { 0, 2, 2, 4, 0, 0, 2, 2, 0, 2, 4, 6 };

// ------------------------------------------------------------------------------------------------ module state (the exe's globals)
struct CacheEntry { u32 key; u32 stamp; IDirect3DVertexShader9* shader; u8 layout[8]; };   // 0x14 bytes at 0xC926F8
constexpr int kCacheMax = 0x100;

struct State {
    int       model = kModelVS2x;      // 0xC94C00
    unsigned  maxConsts = 0x100;       // 0xC94C04
    u32       stamp = 0;               // [RwEngineInstance + 8] (u16)
    CacheEntry cache[kCacheMax + 2];   // 0xC926F8
    int       count = 0;               // 0xC94BF8
    int       cur = 0;                 // 0xC94BFC (pointer to the most recently used entry)
    bool      loggedNoAssembler = false;
    IDirect3DVertexShader9* (*hook)(const std::string&) = nullptr;   // test hook: replaces D3DAssemble + CreateVertexShader
    State() { Reset(); }
    void Reset() {
        std::memset(cache, 0, sizeof cache);
        cache[0].key = 0xFFFFFFFFu; cache[0].stamp = 0xFFFFFFFFu; cache[0].shader = reinterpret_cast<IDirect3DVertexShader9*>(~uintptr_t(0));
        std::memset(cache[0].layout, 0xFF, 8);
        count = 0; cur = 0;
    }
} g;

// ------------------------------------------------------------------------------------------------ text builder
struct Text {
    std::string s;
    template<class... A> void P(const char* fmt, A... a) {
        char buf[512];
        const int n = std::snprintf(buf, sizeof buf, fmt, a...);
        if (n > 0) s.append(buf, static_cast<size_t>(n));
    }
};

} // namespace

// 0x75EDD0
int NumLightConstants(const Key& k) {
    int n = 5;                                                       // c0..c4
    const unsigned a = k.b[0] & 0xF, bb = k.b[0] >> 4, c = k.b[1] & 0xF;
    if (a)  n = static_cast<int>(a) * 2 + 5;
    if (bb) n = n + ((static_cast<int>(bb) + 3) >> 2) + static_cast<int>(bb) * 2;
    if (c)  n = n + ((static_cast<int>(c) + 3) >> 2) + static_cast<int>(c) * 4;
    if (k.b[2] & 0x80) ++n;                                          // material colour
    if (k.b[3] & 3) ++n;                                             // fog
    if (const unsigned m = k.b[3] >> 2) n += kTexGenConsts[m];
    if (k.b[2] & 1) ++n;                                             // morph
    return n;
}

namespace {
// the flags 0x75F0B0 derives from the key before it allocates registers
struct Flags {
    unsigned N, M, fog;
    bool morph, prelit, normalize, matColor;
    bool hasNormal, hasTangent;
};
Flags GetFlags(const Key& k) {
    Flags f{};
    const unsigned b0 = k.b[0], b1 = k.b[1], b2 = k.b[2], b3 = k.b[3];
    f.N = (b2 >> 1) & 7;
    f.M = b3 >> 2;
    f.fog = b3 & 3;
    f.morph = b2 & 1;
    f.prelit = (b2 & 0x10) != 0;
    f.normalize = ((b2 >> 6) & 1) != 0;
    f.matColor = (b2 >> 7) != 0;
    const bool needNormals = b0 != 0 || (b1 & 0xF) != 0 || f.M == 1 || f.M == 2 || f.M == 3 || f.M == 8 || f.M == 9 || f.M == 10 || f.M == 11;   // 0x75F10F
    if (needNormals && (b2 & 0x20)) {
        f.hasNormal = true;                                                                       // 0x75F14D
        f.hasTangent = (f.M == 8 || f.M == 9);                                                    // 0x75F152
    }
    return f;
}
// group base registers + bone base (variables [esp+0x10] / [esp+0x14] / ebx of 0x75F0B0)
struct Regs { int groupB = -1, groupC = -1, bone = 0; };
Regs AllocRegs(const Key& k, const Flags& f, unsigned maxConsts, Layout& lay) {
    Regs r;
    std::memset(lay.reg, 0xFF, sizeof lay.reg);                                                   // 0x75F176
    int ebx = 5;
    const unsigned a = k.b[0] & 0xF, bb = k.b[0] >> 4, c = k.b[1] & 0xF;
    if (a)  ebx = static_cast<int>(a) * 2 + 5;
    if (bb) { r.groupB = ebx; ebx = ebx + ((static_cast<int>(bb) + 3) >> 2) + static_cast<int>(bb) * 2; }
    if (c)  { r.groupC = ebx; ebx = ebx + ((static_cast<int>(c) + 3) >> 2) + static_cast<int>(c) * 4; }
    if (f.matColor) lay.reg[0] = static_cast<u8>(ebx++);                                          // 0x75F1D9
    if (f.fog > 0)  lay.reg[1] = static_cast<u8>(ebx++);                                          // 0x75F1E4
    if (f.morph)    lay.reg[3] = static_cast<u8>(ebx++);                                          // 0x75F1F2
    if (f.N)        lay.reg[4] = static_cast<u8>(ebx);                                            // 0x75F1FA
    if (f.M) {
        if (const int t = kTexGenConsts[f.M]) {
            if (f.N) lay.reg[2] = static_cast<u8>(maxConsts - static_cast<unsigned>(t));          // 0x75F218: the matrices sit at the END of the constant file
            else   { lay.reg[2] = static_cast<u8>(ebx); ebx += t; }                               // 0x75F21D
        }
    }
    r.bone = ebx;
    return r;
}
} // namespace

Layout ComputeLayout(const Key& k, unsigned maxConsts) {
    Layout lay;
    AllocRegs(k, GetFlags(k), maxConsts, lay);
    return lay;
}

std::string ComposeText(const Key& k, Layout& lay, int model, unsigned maxConsts) {
    const unsigned b0 = k.b[0], b1 = k.b[1], b2 = k.b[2];
    const Flags f = GetFlags(k);
    const Regs  rg = AllocRegs(k, f, maxConsts, lay);
    const unsigned N = f.N, M = f.M;
    const int bone = rg.bone;
    const unsigned texSets = b1 >> 4;

    Text t;
    // ---- 0x75F222: shader model header
    switch (model) {
    case kModelVS2x: t.P("vs_2_x\n"); break;      // 0x75F240
    case kModelVS20: t.P("vs_2_0\n"); break;      // 0x75F258
    case kModelVS11: t.P("vs_1_1\n"); break;      // 0x75F26F
    default: return {};
    }
    // ---- 0x75F285: input declarations
    t.P("dcl_position0 v0\n");
    if (N) {
        if (N > 1) t.P("dcl_blendweight0 v1\n");
        t.P("dcl_blendindices0 v2\n");
    }
    if (b2 & 0x10)    t.P("dcl_color0 v4\n");
    if (f.hasNormal)  t.P("dcl_normal0 v3\n");
    if (f.hasTangent) t.P("dcl_tangent0 v13\n");
    {   // 0x75F347: jump table 0x760CA4 / byte table 0x760CB0 on M-1
        static constexpr u8 kDclCase[11] = { 0, 0, 0, 1, 0, 0, 1, 2, 2, 0, 0 };
        const unsigned idx = M - 1;
        const int dc = idx > 10 ? 2 : kDclCase[idx];
        if (dc == 0)      { if (b1 & 0xF0) t.P("dcl_texcoord0 v5\n"); }                         // 0x75F360
        else if (dc == 1) { if ((b1 & 0xF0) > 0x10) t.P("dcl_texcoord1 v6\n"); }                // 0x75F37E
        else              { for (unsigned i = 0; i < texSets; ++i) t.P("dcl_texcoord%u v%u\n", i, i + 5); }   // 0x75F3A2
    }
    if (f.morph) {                                                                              // 0x75F3E0
        t.P("dcl_position1 v14\n");
        if (f.hasNormal) t.P("dcl_normal1 v15\n");
    }

    // ---- 0x75F420: position (and normal) source registers; morphing; skinning
    const char* pos  = "v0";
    const char* norm = f.hasNormal ? "v3" : nullptr;
    if (!f.morph && N == 0) {
        t.P("m4x4 oPos.xyzw, v0, c[0]\n");                                                     // 0x75F44A
    } else {
        bool toSkin = true;
        if (f.morph) {                                                                          // 0x75F468
            const int e = lay.reg[3];
            if (N == 0) {                                                                       // 0x75F480
                t.P("mul r3, v0, c[%d].x\n", e);
                t.P("mad r3, v14, c[%d].y, r3\n", e);
                pos = "r3";
                if (f.hasNormal) {
                    t.P("mul r5, v3, c[%d].x\n", e);
                    t.P("mad r5, v15, c[%d].y, r5\n", e);
                    norm = "r5";                                                                // 0x75F910
                }
                toSkin = false;
            } else {                                                                            // 0x75F4F1
                t.P("mul r2, v0, c[%d].x\n", e);
                t.P("mad r2, v14, c[%d].y, r2\n", e);
                pos = "r2";
                if (f.hasNormal) {
                    t.P("mul r4, v3, c[%d].x\n", e);
                    t.P("mad r4, v15, c[%d].y, r4\n", e);
                    norm = "r4";
                }
            }
        }
        if (toSkin && N) {                                                                      // 0x75F561
            const char* w = kSwz[N];
            if (N == 1) {                                                                       // 0x75F56C
                t.P(model == kModelVS11 ? "mov a0.x, v2.x\n" : "mova a0.x, v2.x\n");
                t.P("m4x3 r3.xyz, %s, c[a0.x + %d]\n", pos, bone);
                if (f.hasNormal) t.P("m3x3 r5.xyz, %s, c[a0.x + %d]\n", norm, bone);
            } else if (f.hasNormal) {                                                           // 0x75F5EB: full 3x4 matrix blended into r8/r9/r10
                if (model >= kModelVS20) {                                                      // 0x75F605
                    t.P("mova a0.%s, v2\n", w);
                    for (unsigned i = 0; i < N; ++i) {
                        const int c = kComp[i];
                        if (i == 0) {                                                           // 0x75F684
                            t.P("mul r8, v1.%c, c[a0.%c + %d]\n", 'x', 'x', bone);
                            t.P("mul r9, v1.%c, c[a0.%c + %d]\n", 'x', 'x', bone + 1);
                            t.P("mul r10, v1.%c, c[a0.%c + %d]\n", 'x', 'x', bone + 2);
                        } else {                                                                // 0x75F63C
                            t.P("mad r8, v1.%c, c[a0.%c + %d], r8\n", c, c, bone);
                            t.P("mad r9, v1.%c, c[a0.%c + %d], r9\n", c, c, bone + 1);
                            t.P("mad r10, v1.%c, c[a0.%c + %d], r10\n", c, c, bone + 2);
                        }
                    }
                } else {                                                                        // 0x75F6DF (vs_1_1: one address register component at a time)
                    for (unsigned i = 0; i < N; ++i) {
                        const int c = kComp[i];
                        t.P("mov a0.x, v2.%c\n", c);
                        if (i == 0) {                                                           // 0x75F75B
                            t.P("mul r8, v1.%c, c[a0.x + %d]\n", 'x', bone);
                            t.P("mul r9, v1.%c, c[a0.x + %d]\n", 'x', bone + 1);
                            t.P("mul r10, v1.%c, c[a0.x + %d]\n", 'x', bone + 2);
                        } else {                                                                // 0x75F70F
                            t.P("mad r8, v1.%c, c[a0.x + %d], r8\n", c, bone);
                            t.P("mad r9, v1.%c, c[a0.x + %d], r9\n", c, bone + 1);
                            t.P("mad r10, v1.%c, c[a0.x + %d], r10\n", c, bone + 2);
                        }
                    }
                }
                t.P("m4x3 r3.xyz, %s, r8\n", pos);                                              // 0x75F7B5
                t.P("m3x3 r5.xyz, %s, r8\n", norm);
            } else if (model >= kModelVS20) {                                                   // 0x75F7EF: position only
                t.P("mova a0.%s, v2\n", w);
                for (unsigned i = 0; i < N; ++i) {
                    t.P("m4x3 r5.xyz, %s, c[a0.%c + %d]\n", pos, kComp[i], bone);              // 0x75F820
                    if (i == 0) t.P("mul r3.xyz, v1.%c, r5.xyz\n", 'x');
                    else        t.P("mad r3.xyz, v1.%c, r5.xyz, r3.xyz\n", kComp[i]);
                }
            } else {                                                                            // 0x75F883
                for (unsigned i = 0; i < N; ++i) {
                    t.P("mov a0.x, v2.%c\n", kComp[i]);
                    t.P("m4x3 r5.xyz, %s, c[a0.x + %d]\n", pos, bone);
                    if (i == 0) t.P("mul r3.xyz, v1.%c, r5.xyz\n", 'x');
                    else        t.P("mad r3.xyz, v1.%c, r5.xyz, r3.xyz\n", kComp[i]);
                }
            }
            pos = "r3";                                                                         // 0x75F900
            if (f.hasNormal) norm = "r5";
        }
        t.P("mov r3.w, v0.w\n");                                                                // 0x75F918
        t.P("m4x4 oPos.xyzw, r3, c[0]\n");
    }

    // ---- 0x75F946: fog
    if (f.fog > 0) {
        const int e = lay.reg[1];
        t.P("dp4 r0.w, %s, c[3]\n", pos);
        if (f.fog == 1) {                                                                       // 0x75FA29
            t.P("mad oFog, r0.w, c[%d].x, c[%d].y\n", e, e);
        } else if (f.fog == 2) {                                                                // 0x75F9E6
            t.P("mul r0.w, r0.w, c[%d].x\n", e);
            t.P("expp r0.w, r0.w\n");
            t.P("rcp oFog, r0.w\n");
        } else if (f.fog == 3) {                                                                // 0x75F990
            t.P("mul r0.w, r0.w, c[%d].x\n", e);
            t.P("mul r0.w, r0.w, r0.w\n");
            t.P("expp r0.w, r0.w\n");
            t.P("rcp oFog, r0.w\n");
        }
    }

    // ---- 0x75FA46: renormalise the normal
    if (f.hasNormal && f.normalize) {
        t.P("dp3 r5.w, %s, %s\n", norm, norm);
        t.P("rsq r5.w, r5.w\n");
        t.P("mul r5.xyz, %s, r5.w\n", norm);
        norm = "r5";
    }

    // ---- lighting: tangent space (bump, M = 8 / 9) at 0x760107, else the per-light loops at 0x75FAAC
    if (f.hasTangent) {
        if (N) {                                                                                // 0x760107
            if (N == 1) t.P("m3x3 r4.xyz, v13, c[a0.x + %d]\n", bone);
            else        t.P("m3x3 r4.xyz, v13, r8\n");
            if (f.normalize) {
                t.P("dp3 r4.w, r4, r4\n");
                t.P("rsq r4.w, r4.w\n");
                t.P("mul r4.xyz, r4, r4.w\n");
            }
            t.P("slt r4.w, v13.w, v13.w\n");
        } else {
            t.P("mov r4, v13\n");                                                               // 0x76019C
        }
        if (f.morph || N) t.P("mov %s.w, v3.w\n", norm);                                        // 0x7601C0
        t.P("mul r2, r4.zxyw, %s.yzxw\n", norm);                                                // 0x7601D8
        t.P("mad r2, r4.yzxw, %s.zxyw, -r2\n", norm);
        if (f.normalize) {
            t.P("dp3 r2.w, r2, r2\n");
            t.P("rsq r2.w, r2.w\n");
            t.P("mul r2.xyz, r2, r2.w\n");
        }
        if (f.prelit) t.P("mov oD0, v4\n");                                                     // 0x76024A
        char wc = 0;                                                                            // 'w' (directional) or 'z' (point / spot): which c[5] component scales the result
        if (b0 & 0xF) {                                                                         // 0x760267
            t.P("dp3 r1.x, c[5], r4\n");
            t.P("dp3 r1.y, c[5], r2\n");
            t.P("dp3 r1.z, c[5], %s\n", norm);
            t.P("dp3 r1.w, r1, r1\n");
            t.P("rsq r1.w, r1.w\n");
            t.P("mul r1.xyz, r1, r1.w\n");
            wc = 'w';
        } else if (b0 & 0xF0) {                                                                 // 0x760329 / 0x760331
            t.P("add r7.xyz, -c[1 + 5].xyz, %s.xyz\n", pos);
            t.P("dp3 r7.w, r7, r7\n");
            t.P("rsq r7.w, r7.w\n");
            t.P("mul r7.xyz, r7, r7.w\n");
            t.P("rcp r7.w, r7.w\n");
            t.P("mad r7.w, r7.w, c[5].x, c[5].y\n");
            t.P("max r7.w, r7.w, c[5].w\n");
            t.P("dp3 r1.x, r7, r4\n");
            t.P("dp3 r1.y, r7, r2\n");
            t.P("dp3 r1.z, r7, %s\n", norm);
            t.P("dp3 r1.w, r1, r1\n");
            t.P("rsq r1.w, r1.w\n");
            t.P("mul r1.xyz, r1, r1.w\n");
            t.P("mul r1.xyz, r1, r7.w\n");
            wc = 'z';
        } else if (b1 & 0xF) {                                                                  // 0x7605A3
            t.P("add r7.xyz, -c[1 + 5].xyz, %s.xyz\n", pos);
            t.P("dp3 r7.w, r7, r7\n");
            t.P("rsq r7.w, r7.w\n");
            t.P("mul r7.xyz, r7, r7.w\n");
            t.P("dp3 r9.w, r7, c[2 + 5]\n");
            t.P("mad r9.w, r9.w, c[3 + 5].x, c[3 + 5].y\n");
            t.P("rcp r7.w, r7.w\n");
            t.P("mad r7.w, r7.w, c[5].x, c[5].y\n");
            t.P("max r7.w, r7.w, c[5].w\n");
            t.P("max r9.w, r9.w, c[5].w\n");
            t.P("dp3 r1.x, r7, r4\n");
            t.P("dp3 r1.y, r7, r2\n");
            t.P("dp3 r1.z, r7, %s\n", norm);
            t.P("dp3 r1.w, r1, r1\n");
            t.P("rsq r1.w, r1.w\n");
            t.P("mul r1.xyz, r1, r1.w\n");
            t.P("mul r1.xyz, r1, r7.w\n");
            t.P("mul r1.xyz, r1, r9.w\n");
            wc = 'z';
        }
        if (wc) {                                                                               // the light result goes to the specular colour (M == 9, prelit: oD1) / diffuse (oD0) / texture set 2
            const char* out = M == 9 ? (f.prelit ? "oD1" : "oD0") : "oT2";
            if (wc == 'w') t.P("mad %s.xyz, r1, c[5].w, c[5].w\n", out);                        // 0x760302 / 0x76031F / 0x7602F4
            else           t.P("mad %s.xyz, r1, c[5].z, c[5].z\n", out);                        // 0x760743 / 0x760765 / 0x76045D
        }
        if (M == 9) {                                                                           // 0x76047F: tangent-space light vectors to oT2 / oT3
            int e = lay.reg[2];
            t.P("dp3 oT2.x, c[%d], r4\n", e);
            t.P("dp3 oT2.y, c[%d], r2\n", e);
            t.P("dp3 oT2.z, c[%d], %s\n", e, norm);
            ++e;
            t.P("dp3 oT3.x, c[%d], r4\n", e);
            t.P("dp3 oT3.y, c[%d], r2\n", e);
            t.P("dp3 oT3.z, c[%d], %s\n", e, norm);
        }
        t.P("mov oT0.xy, v%d\n", static_cast<int>(texSets) + 4);                                 // 0x760517
        t.P("mov oT1.xy, v5\n");
    } else {
        const int L = static_cast<int>((b0 & 0xF) + (b1 & 0xF) + (b0 >> 4));                     // 0x75FAB2
        int remaining = L;
        if (f.hasNormal && L > 0) {                                                             // 0x75FAD0
            if (f.prelit) t.P("add r11, v4, c[4]\n"); else t.P("mov r11, c[4]\n");
            // one light's colour accumulation: the very last light of the shader writes oD0 directly unless the material colour is multiplied afterwards
            auto accumulate = [&](int reg, int j) {
                if (remaining <= 1 && !f.matColor) t.P("mad oD0, c[%d], r6.%c, r11\n", reg, kComp[j]);                // 0x75FBC0
                else                               t.P("mad r11.xyz, c[%d].xyz, r6.%s, r11.xyz\n", reg, kSwz[5 + j]);  // 0x75FBDC
                --remaining;
            };
            // group A (directional): direction c[5+2i], colour c[6+2i], chunks of 4 lights
            if (const int a = static_cast<int>(b0 & 0xF)) {
                int base = 6;
                for (int done = 0; done < a; done += 4, base += 8) {                             // 0x75FB39
                    const int n = a - done < 4 ? a - done : 4;
                    for (int j = 0; j < n; ++j) t.P("dp3 r6.%c, c[%d], %s\n", kComp[j], base - 1 + 2 * j, norm);
                    t.P("max r6.%s, c[5].w, -r6.%s\n", kSwz[n], kSwz[n]);
                    for (int j = 0; j < n; ++j) accumulate(base + 2 * j, j);
                }
            }
            // group B (point): packed constant at gB, position gB+1+2i, colour gB+2+2i
            if (const int bcount = static_cast<int>(b0 >> 4)) {
                int gB = rg.groupB;
                for (int done = 0; done < bcount; done += 4) {                                   // 0x75FC57
                    const int n = bcount - done < 4 ? bcount - done : 4;
                    int reg = gB + 1;
                    for (int j = 0; j < n; ++j, reg += 2) {                                      // 0x75FC7D
                        t.P("add r7.xyz, -c[%d].xyz, %s.xyz\n", reg, pos);
                        t.P("dp3 r1.%c, r7, r7\n", kComp[j]);
                        t.P("rsq r1.%c, r1.%c\n", kComp[j], kComp[j]);
                        t.P("dp3 r6.%c, %s, r7\n", kComp[j], norm);
                    }
                    const char* sw = kSwz[n];
                    t.P("max r8.%s, c[%d].%s, r1.%s\n", sw, gB, sw, sw);                         // 0x75FCF9
                    t.P("max r6.%s, c[%d].wwww, -r6.%s\n", sw, gB + 1, sw);
                    t.P("add r8.%s, -c[%d].%s, r8.%s\n", sw, gB, sw, sw);
                    t.P("mul r6.%s, r6.%s, r8.%s\n", sw, sw, sw);
                    reg = gB + 2;
                    for (int j = 0; j < n; ++j, reg += 2) accumulate(reg, j);                    // 0x75FD75
                    gB += 2 * n + 1;                                                             // 0x75FDDA
                }
            }
            // group C (spot): packed constant at gC, position gC+1+4i, direction gC+2+4i, cone gC+3+4i, colour gC+4+4i
            if (const int ccount = static_cast<int>(b1 & 0xF)) {
                int gC = rg.groupC;
                for (int done = 0; done < ccount; done += 4) {                                   // 0x75FE19
                    const int n = ccount - done < 4 ? ccount - done : 4;
                    int reg = gC + 3;
                    for (int j = 0; j < n; ++j, reg += 4) {                                      // 0x75FE3F
                        t.P("add r7.xyz, -c[%d].xyz, %s.xyz\n", reg - 2, pos);
                        t.P("dp3 r1.%c, r7, r7\n", kComp[j]);
                        t.P("rsq r1.%c, r1.%c\n", kComp[j], kComp[j]);
                        t.P("dp3 r6.%c, %s, r7\n", kComp[j], norm);
                        t.P("mul r7.xyz, r7.xyz, r1.%c\n", kComp[j]);
                        t.P("dp3 r7.w, r7, c[%d]\n", reg - 1);
                        t.P("mad r9.%c, r7.w, c[%d].x, c[%d].y\n", kComp[j], reg, reg);
                    }
                    const char* sw = kSwz[n];
                    t.P("max r8.%s, c[%d].%s, r1.%s\n", sw, gC, sw, sw);                         // 0x75FF19
                    t.P("max r9.%s, c[%d].wwww, r9.%s\n", sw, gC + 1, sw);
                    t.P("max r6.%s, c[%d].wwww, -r6.%s\n", sw, gC + 1, sw);
                    t.P("add r8.%s, -c[%d].%s, r8.%s\n", sw, gC, sw, sw);
                    t.P("mul r6.%s, r6.%s, r8.%s\n", sw, sw, sw);
                    t.P("mul r6.%s, r6.%s, r9.%s\n", sw, sw, sw);
                    reg = gC + 4;
                    for (int j = 0; j < n; ++j, reg += 4) accumulate(reg, j);                    // 0x75FFC6
                    gC += 4 * n + 1;                                                             // 0x760026
                }
            }
            if (f.matColor) t.P("mul oD0, r11, c[%d]\n", lay.reg[0]);                            // 0x760045
        } else if (f.matColor) {                                                                // 0x760074
            if (f.prelit) t.P("add r11, v4, c[4]\n"); else t.P("mov r11, c[4]\n");
            t.P("mul oD0, r11, c[%d]\n", lay.reg[0]);
        } else if (f.prelit) {                                                                  // 0x7600CF
            t.P("add oD0, v4, c[4]\n");
        } else {
            t.P("mov oD0, c[4]\n");                                                             // 0x7600ED
        }
    }

    // ---- 0x76054D: texture coordinates
    auto copySets = [&]() { for (unsigned i = 0; i < texSets; ++i) t.P("mov oT%u.xy, v%u\n", i, i + 5); };   // 0x760BD6 / 0x760C13
    if (M == 0 || M > 11) {
        copySets();
    } else {
        const int e = lay.reg[2];
        auto envBase = [&]() {                                                                  // 0x760842 (M == 1)
            t.P("dp3 r9.x, %s, c[%d]\n", norm, e);
            t.P("dp3 r9.y, %s, c[%d]\n", norm, e + 1);
            t.P("mov oT0.xy, v5\n");
            t.P("add oT1.xy, r9.xy, v5.xy\n");
        };
        auto reflect = [&]() {                                                                  // 0x76094D.. (M == 10 and the tail of M == 11): reflection vector in r7
            t.P("dp3 r7.w, r7, r7\n");
            t.P("rsq r7.w, r7.w\n");
            t.P("mul r7.xyz, r7, r7.w\n");
            t.P("dp3 r7.w, r7, %s\n", norm);
            t.P("add r7.w, r7.w, r7.w\n");
            t.P("mad r7, r7.w, %s, -r7\n", norm);
        };
        switch (M) {
        case 1: envBase(); break;                                                               // 0x76083E
        case 2:                                                                                 // 0x76077B
            if (b1 & 0xF0) {
                t.P("mov oT0.xy, v5\n");
                t.P("dp4 oT1.x, %s, c[%d]\n", norm, e);
                t.P("dp4 oT1.y, %s, c[%d]\n", norm, e + 1);
            } else {
                t.P("dp4 oT0.x, %s, c[%d]\n", norm, e);
                t.P("dp4 oT0.y, %s, c[%d]\n", norm, e + 1);
            }
            break;
        case 3:                                                                                 // 0x760803
            t.P("dp4 oT2.x, %s, c[%d]\n", norm, e + 2);
            t.P("dp4 oT2.y, %s, c[%d]\n", norm, e + 3);
            envBase();
            break;
        case 4: t.P("mov oT0.xy, v6\n"); break;                                                 // 0x76089E
        case 5: t.P("mov oT0.xy, v5\n"); t.P("mov oT1.xy, v5\n"); break;                        // 0x7608B9
        case 6:                                                                                 // 0x7608E9
            t.P("dp4 oT0.x, v5, c[%d]\n", e);
            t.P("dp4 oT0.y, v5, c[%d]\n", e + 1);
            break;
        case 7:                                                                                 // 0x76091B
            t.P("dp4 oT0.x, v6, c[%d]\n", e);
            t.P("dp4 oT0.y, v6, c[%d]\n", e + 1);
            break;
        case 8: case 9: break;                                                                  // tangent space: done above
        case 10:                                                                                // 0x76094D
            t.P("add r7.xyz, c[%d].xyz, -%s.xyz\n", e, pos);
            reflect();
            if (b1 & 0xF0) {
                t.P("mov oT0.xy, v5\n");
                t.P("dp3 oT1.x, r7, c[%d]\n", e + 1);
                t.P("dp3 oT1.y, r7, c[%d]\n", e + 2);
                t.P("dp3 oT1.z, r7, c[%d]\n", e + 3);
            } else {
                t.P("dp3 oT0.x, r7, c[%d]\n", e + 1);
                t.P("dp3 oT0.y, r7, c[%d]\n", e + 2);
                t.P("dp3 oT0.z, r7, c[%d]\n", e + 3);
            }
            break;
        case 11:                                                                                // 0x760AA5
            envBase();
            t.P("add r7.xyz, c[%d].xyz, -%s.xyz\n", e + 2, pos);
            reflect();
            t.P("dp3 oT2.x, r7, c[%d]\n", e + 3);
            t.P("dp3 oT2.y, r7, c[%d]\n", e + 4);
            t.P("dp3 oT2.z, r7, c[%d]\n", e + 5);
            break;
        }
    }
    return std::move(t.s);
}

// ------------------------------------------------------------------------------------------------ assembler + device
namespace {

struct IBlob : IUnknown {                                                                       // ID3DXBuffer / ID3DBlob share this vtable layout
    virtual void*  STDMETHODCALLTYPE GetBufferPointer() = 0;
    virtual SIZE_T STDMETHODCALLTYPE GetBufferSize() = 0;
};
using D3DAssembleFn = HRESULT(WINAPI*)(const void*, SIZE_T, const char*, const void*, void*, UINT, IBlob**, IBlob**);

D3DAssembleFn LoadAssembler() {
    static D3DAssembleFn fn = []() -> D3DAssembleFn {
        HMODULE h = LoadLibraryA("d3dcompiler_47.dll");
        return h ? reinterpret_cast<D3DAssembleFn>(GetProcAddress(h, "D3DAssemble")) : nullptr;
    }();
    return fn;
}

// 0x7652AE (D3DXAssembleShader) + 0x7FAC60 (CreateVertexShader) + the Release of the blob, 0x760C4D..0x760C9D
IDirect3DVertexShader9* Assemble(const std::string& text) {
    if (g.hook) return g.hook(text);
    D3DAssembleFn assemble = LoadAssembler();
    if (!assemble) {
        if (!g.loggedNoAssembler) { g.loggedNoAssembler = true; std::fprintf(stderr, "skinvs: d3dcompiler_47.dll / D3DAssemble unavailable, skin vertex shaders disabled\n"); }
        return nullptr;
    }
    IDirect3DVertexShader9* shader = nullptr;
    IBlob* code = nullptr;
    IBlob* errors = nullptr;
    const HRESULT hr = assemble(text.data(), text.size(), nullptr, nullptr, nullptr, 0, &code, &errors);
    if (hr >= 0 && code) {
        if (rw::d3d::d3ddevice)
            rw::d3d::d3ddevice->CreateVertexShader(static_cast<const DWORD*>(code->GetBufferPointer()), &shader);
    } else if (errors) {
        std::fprintf(stderr, "skinvs: D3DAssemble failed (0x%08lX): %s\n", static_cast<unsigned long>(hr), static_cast<const char*>(errors->GetBufferPointer()));
    }
    if (code) code->Release();
    if (errors) errors->Release();
    return shader;
}

void ReleaseShader(IDirect3DVertexShader9* s) {                                                 // 0x7FAC90
    if (s && s != reinterpret_cast<IDirect3DVertexShader9*>(~uintptr_t(0))) s->Release();
}

u32 KeyOf(const Key& k) { u32 v; std::memcpy(&v, k.b, 4); return v; }

// 0x75F0B0: layout + text + assemble
IDirect3DVertexShader9* Compose(const Key& k, Layout& lay) {
    const std::string text = ComposeText(k, lay, g.model, g.maxConsts);
    if (text.empty()) return nullptr;
    return Assemble(text);
}

} // namespace

// 0x75EED0
IDirect3DVertexShader9* GetVertexShader(const Key& k, Layout& lay) {
    const u32 key = KeyOf(k);
    int idx = g.cur;
    if (g.cache[idx].key != key) {
        // binary search over the first `count` entries (unsigned order of the key), 0x75EEE6..0x75EF9A
        int lo = 0, hi = g.count - 1, found = -1, insertAt = 0;
        int n = g.count;
        bool miss = true;
        while (n > 0) {
            if (n <= 1) {                                                                       // 0x75EF7F
                const u32 ek = g.cache[lo].key;
                if (key == ek) { found = lo; miss = false; }
                else if (key < ek) insertAt = lo;
                else insertAt = lo + 1;
                break;
            }
            const int mid = lo + (n + 1) / 2 - 1;
            const u32 ek = g.cache[mid].key;
            if (key == ek) { found = mid; miss = false; break; }
            if (key < ek) hi = mid - 1; else lo = mid + 1;
            n = hi - lo + 1;
            if (n <= 0) { insertAt = lo; break; }
        }
        if (g.count <= 0) insertAt = 0;
        if (!miss) {
            g.cur = found;
        } else {
            IDirect3DVertexShader9* sh = Compose(k, lay);                                       // 0x75EF9A
            if (g.count >= kCacheMax) {                                                         // 0x75EFB4: replace the least recently used entry
                int victim = 0;
                for (int i = 1; i < g.count; ++i) if (g.cache[i].stamp < g.cache[victim].stamp) victim = i;
                CacheEntry& e = g.cache[victim];
                e.key = key;
                e.stamp = g.stamp & 0xFFFF;
                ReleaseShader(e.shader);
                e.shader = sh;
                std::memcpy(e.layout, lay.reg, 8);
                g.cur = victim;
                return sh;
            }
            if (insertAt < g.count)                                                             // 0x75F019: make room (memmove 0x822A20)
                std::memmove(&g.cache[insertAt + 1], &g.cache[insertAt], static_cast<size_t>(g.count - insertAt) * sizeof(CacheEntry));
            CacheEntry& e = g.cache[insertAt];
            g.cur = insertAt;
            e.key = key;
            e.stamp = g.stamp & 0xFFFF;
            e.shader = sh;
            std::memcpy(e.layout, lay.reg, 8);
            ++g.count;
            return sh;
        }
        idx = found;
    }
    // 0x75EF49: hit
    CacheEntry& e = g.cache[idx];
    e.stamp = g.stamp & 0xFFFF;
    std::memcpy(lay.reg, e.layout, 8);
    IDirect3DVertexShader9* sh = e.shader;
    return sh == reinterpret_cast<IDirect3DVertexShader9*>(~uintptr_t(0)) ? nullptr : sh;
}

IDirect3DVertexShader9* GetVertexShader(const Key& k) { Layout l; return GetVertexShader(k, l); }

std::string ComposeText(const Key& k, Layout& lay) { return ComposeText(k, lay, g.model, g.maxConsts); }

void SetModel(int model, unsigned maxConstants) { g.model = model; g.maxConsts = maxConstants; }
int  Model() { return g.model; }
unsigned MaxConstants() { return g.maxConsts; }
void SetFrameStamp(std::uint16_t stamp) { g.stamp = stamp; }
int  CacheCount() { return g.count; }
bool CacheEntryAt(int idx, std::uint32_t& key, std::uint32_t& stamp, IDirect3DVertexShader9*& shader, std::uint8_t layout[8]) {
    if (idx < 0 || idx >= kCacheMax + 2) return false;
    key = g.cache[idx].key; stamp = g.cache[idx].stamp; shader = g.cache[idx].shader;
    std::memcpy(layout, g.cache[idx].layout, 8);
    return true;
}
void SetAssembleHook(IDirect3DVertexShader9* (*hook)(const std::string&)) { g.hook = hook; }

// 0x75EE60
void Shutdown() {
    for (int i = 0; i < g.count; ++i) {
        ReleaseShader(g.cache[i].shader);
        g.cache[i].shader = nullptr;
    }
    g.Reset();
}

// 0x760CF0
void Init() {
    g.Reset();
    D3DCAPS9 caps{};
    if (!rw::d3d::d3ddevice || FAILED(rw::d3d::d3ddevice->GetDeviceCaps(&caps))) return;
    const unsigned ver = caps.VertexShaderVersion & 0xFFFF;
    if (ver >= 0x300)      g.model = kModelVS2x;
    else if (ver >= 0x200) g.model = static_cast<int>(caps.VS20Caps.DynamicFlowControlDepth) > 0 ? kModelVS2x : kModelVS20;   // [0xC9BFFC] (= D3DCAPS9 + 0xFC) <= 0 -> vs_2_0
    else if (ver >= 0x100) g.model = kModelVS11;
    else { g.model = 0; return; }
    g.maxConsts = caps.MaxVertexShaderConst < 0x100 ? caps.MaxVertexShaderConst : 0x100;
}

} // namespace notsa::skinvs
#endif
