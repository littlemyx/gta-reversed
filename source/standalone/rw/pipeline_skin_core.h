#pragma once
/*
    P2B-24b: the numeric / control core of the exe's skin HARDWARE render path (the "vs_1_1 path" of the RenderWare skin pipeline), header-only so that the
    pipeline glue (pipeline_skin.cpp) and the exe-oracle test (tests/standalone/rw_skin_hw_oracle_test.cpp) share exactly one implementation.

    Everything here is free of D3D / librw object types: the glue marshals librw's objects into the small structs below, the tests marshal the same
    random data into the exe's own RW 3.6 structures and compare bit for bit (see the test for the coverage).

    Ported from gta_sa_compact.exe (all .text):
      0x7C78A0  bone matrix builder (3 float4 rows per bone, transposed)          BuildBoneMatrices
      0x7CB190  skin pipeline "begin": the shader key from the geometry/fog        BeginKey
      0x761170  light constants (ambient / directional / point / spot, object space) CollectLights + BuildLightConstants
      0x761720  rescale of the light colours by the material ambient / diffuse      ScaleLightColours
      0x761820  material colour constant, 0x7618B0 fog constant                    MaterialColourConst / FogConst
      0x761030  per-mesh render states + draw                                      MeshRender
      0x7C8060  the HW render function itself                                      RenderHW
      0x764650 / 0x7646E0 / 0x7647B0 / 0x764D30 / 0x764E70 / 0x764F60  the "world matrix" module (object-space transforms, c0..c3)   WorldXform
      0x751AE0  _rwACos-like polynomial (lifted by tools/standalone/lift_x87.py -> pipeline_skin_lifted.inc)
    The statically linked D3DX kernels the exe calls through its CPU-dispatch table (0x8D7490) are ported from the SSE variants the table holds on every
    modern CPU (0x7B92C6 D3DXMatrixMultiply, 0x7BA139 D3DXMatrixMultiplyTranspose, 0x7B9BC5 D3DXVec3TransformNormal, 0x7B9D6E D3DXVec3TransformCoord),
    with the same intrinsics in the same order (the sums are NOT in one common order).

    Float discipline: x87 intermediates are `double` (project rule, see rwmath_exact.h), every `fstp dword` is a (float) cast; SSE code is written with SSE intrinsics.
*/
#include "fakerw.h"
#include "rwmath_exact.h"
#include <emmintrin.h>
#include <xmmintrin.h>
NOTSA_CLANG_SSE2_BEGIN

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace rwskin {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;

constexpr u32 kMatrixIdentity = 0x20000;

//--------------------------------------------------------------------------------------------------
// minimal harness for the lifted x87 code (same conventions as rwx::lift in camera_sync.h)
//--------------------------------------------------------------------------------------------------
namespace lift {
using R = std::uintptr_t;
struct Ret { R eax; double st0; };
inline u32   RU32(R a) { u32 v; std::memcpy(&v, reinterpret_cast<const void*>(a), 4); return v; }
inline u32   RU16(R a) { std::uint16_t v; std::memcpy(&v, reinterpret_cast<const void*>(a), 2); return v; }
inline u32   RU8(R a) { return *reinterpret_cast<const u8*>(a); }
inline float RF32(R a) { float v; std::memcpy(&v, reinterpret_cast<const void*>(a), 4); return v; }
inline double RF64(R a) { double v; std::memcpy(&v, reinterpret_cast<const void*>(a), 8); return v; }
inline void  WU32(R a, u32 v) { std::memcpy(reinterpret_cast<void*>(a), &v, 4); }
inline void  WU16(R a, u32 v) { const std::uint16_t w = static_cast<std::uint16_t>(v); std::memcpy(reinterpret_cast<void*>(a), &w, 2); }
inline void  WU8(R a, u32 v) { *reinterpret_cast<u8*>(a) = static_cast<u8>(v); }
inline void  WF64(R a, double v) { std::memcpy(reinterpret_cast<void*>(a), &v, 8); }
inline bool  PARITY8(u32 v) { v &= 0xFFu; v ^= v >> 4; v ^= v >> 2; v ^= v >> 1; return (v & 1u) == 0; }
inline double FromBits64(std::uint64_t u) { double d; std::memcpy(&d, &u, 8); return d; }
#define MEMABS(a) (std::abort(), 0u)
#include "pipeline_skin_lifted.inc"
#undef MEMABS
} // namespace lift

// 0x751AE0 on a light's minusCosAngle: the exe passes the light, the routine reads [light + 0x28]; the result is the x87 stack top (double here)
inline double LightAcos(float minusCosAngle) {
    alignas(16) u8 stack[0x40] = {};
    alignas(16) u8 fakeLight[0x40] = {};
    std::memcpy(fakeLight + 0x28, &minusCosAngle, 4);
    lift::R esp = reinterpret_cast<lift::R>(stack + 0x30);
    esp -= 4; lift::WU32(esp, static_cast<u32>(reinterpret_cast<lift::R>(fakeLight)));
    esp -= 4; // return address slot
    return lift::acos_751AE0(esp).st0;
}

//--------------------------------------------------------------------------------------------------
// D3DX (SSE variants of the exe's dispatch table)
//--------------------------------------------------------------------------------------------------
inline __m128 Row(const float* m, int r) { return _mm_loadu_ps(m + 4 * r); }
inline __m128 Splat(float v) { return _mm_set1_ps(v); }

// 0x7B92C6 D3DXMatrixMultiply(out, a, b): rows 0 and 3 are summed as a tree, rows 1 and 2 sequentially (exactly the exe's instruction order)
inline void D3dxMatrixMultiply(float* out, const float* a, const float* b) {
    const __m128 b0 = Row(b, 0), b1 = Row(b, 1), b2 = Row(b, 2), b3 = Row(b, 3);
    __m128 r0 = _mm_mul_ps(Splat(a[0]), b0), r0b = _mm_mul_ps(Splat(a[1]), b1), r0c = _mm_mul_ps(Splat(a[2]), b2), r0d = _mm_mul_ps(Splat(a[3]), b3);
    r0 = _mm_add_ps(_mm_add_ps(r0, r0b), _mm_add_ps(r0c, r0d));
    __m128 r1 = _mm_mul_ps(Splat(a[4]), b0);
    r1 = _mm_add_ps(r1, _mm_mul_ps(Splat(a[5]), b1));
    r1 = _mm_add_ps(r1, _mm_mul_ps(Splat(a[6]), b2));
    r1 = _mm_add_ps(r1, _mm_mul_ps(Splat(a[7]), b3));
    __m128 r2 = _mm_mul_ps(Splat(a[8]), b0);
    r2 = _mm_add_ps(r2, _mm_mul_ps(Splat(a[9]), b1));
    r2 = _mm_add_ps(r2, _mm_mul_ps(Splat(a[10]), b2));
    r2 = _mm_add_ps(r2, _mm_mul_ps(Splat(a[11]), b3));
    __m128 r3 = _mm_mul_ps(b0, Splat(a[12])), r3b = _mm_mul_ps(b1, Splat(a[13])), r3c = _mm_mul_ps(b2, Splat(a[14])), r3d = _mm_mul_ps(b3, Splat(a[15]));
    r3 = _mm_add_ps(_mm_add_ps(r3, r3b), _mm_add_ps(r3c, r3d));
    _mm_storeu_ps(out, r0);
    _mm_storeu_ps(out + 4, r1);
    _mm_storeu_ps(out + 8, r2);
    _mm_storeu_ps(out + 12, r3);
}

// 0x7BA139 D3DXMatrixMultiplyTranspose(out, a, b) = transpose(a * b); every row is summed sequentially
inline void D3dxMatrixMultiplyTranspose(float* out, const float* a, const float* b) {
    const __m128 b0 = Row(b, 0), b1 = Row(b, 1), b2 = Row(b, 2), b3 = Row(b, 3);
    __m128 r[4];
    for (int i = 0; i < 4; i++) {
        __m128 s = _mm_mul_ps(Splat(a[4 * i]), b0);
        s = _mm_add_ps(s, _mm_mul_ps(Splat(a[4 * i + 1]), b1));
        s = _mm_add_ps(s, _mm_mul_ps(Splat(a[4 * i + 2]), b2));
        s = _mm_add_ps(s, _mm_mul_ps(Splat(a[4 * i + 3]), b3));
        r[i] = s;
    }
    float t[4][4];
    for (int i = 0; i < 4; i++) {
        _mm_storeu_ps(t[i], r[i]);
    }
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            out[4 * j + i] = t[i][j];
        }
    }
}

// 0x7B91FA D3DXMatrixTranspose
inline void D3dxMatrixTranspose(float* out, const float* a) {
    float t[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            t[4 * j + i] = a[4 * i + j];
        }
    }
    std::memcpy(out, t, sizeof(t));
}

// 0x7B9BC5 D3DXVec3TransformNormal(out, v, m): (x*row0 + y*row1) + z*row2, per lane (only xyz are stored)
inline void D3dxVec3TransformNormal(float* out, const float* v, const float* m) {
    __m128 r = _mm_add_ps(_mm_mul_ps(Splat(v[0]), Row(m, 0)), _mm_mul_ps(Splat(v[1]), Row(m, 1)));
    r = _mm_add_ps(r, _mm_mul_ps(Splat(v[2]), Row(m, 2)));
    alignas(16) float t[4];
    _mm_store_ps(t, r);
    out[0] = t[0]; out[1] = t[1]; out[2] = t[2];
}

// 0x7B9D6E D3DXVec3TransformCoord(out, v, m): (x*row0 + y*row1) + (z*row2 + row3), then the exe's reciprocal-estimate divide by w (rcpps + one Newton step)
inline void D3dxVec3TransformCoord(float* out, const float* v, const float* m) {
    const __m128 t3 = _mm_add_ps(_mm_mul_ps(Splat(v[2]), Row(m, 2)), Row(m, 3));
    __m128 r = _mm_mul_ps(Splat(v[0]), Row(m, 0));
    r = _mm_add_ps(r, _mm_mul_ps(Splat(v[1]), Row(m, 1)));
    r = _mm_add_ps(r, t3);
    const __m128 est = _mm_rcp_ps(r);
    const __m128 two = _mm_add_ps(est, est);
    const __m128 sq  = _mm_mul_ps(_mm_mul_ps(est, r), est);
    const __m128 rc  = _mm_sub_ps(two, sq);
    const __m128 w   = _mm_shuffle_ps(rc, rc, 0xFF);
    r = _mm_mul_ps(r, w);
    alignas(16) float t[4];
    _mm_store_ps(t, r);
    out[0] = t[0]; out[1] = t[1]; out[2] = t[2];
}

//--------------------------------------------------------------------------------------------------
// the "world matrix" module of the exe (0x764650 .. 0x764F60)
//--------------------------------------------------------------------------------------------------
inline double M(float a, float b) { return static_cast<double>(a) * static_cast<double>(b); }

// 0x764890: inverse of an orthonormal RwMatrix into a D3D matrix (transposed 3x3, translation = -(pos . axis), x87 order of the asm)
inline void OrthoInverse(float* d, const RwMatrix& s) {
    const float* p = reinterpret_cast<const float*>(&s);
    d[12] = rwx::F(-((M(p[13], p[1]) + M(p[12], p[0])) + M(p[14], p[2])));
    d[0]  = p[0];  d[4]  = p[1];  d[8]  = p[2];
    d[13] = rwx::F(-((M(p[12], p[4]) + M(p[5], p[13])) + M(p[14], p[6])));
    d[1]  = p[4];  d[5]  = p[5];  d[9]  = p[6];
    d[14] = rwx::F(-((M(p[12], p[8]) + M(p[9], p[13])) + M(p[14], p[10])));
    d[2]  = p[8];  d[6]  = p[9];  d[10] = p[10];
    d[3] = 0.0f; d[7] = 0.0f; d[11] = 0.0f; d[15] = 1.0f;
}

class WorldXform {
public:
    // 0x764650: the current world matrix (NULL = none); drops the cached inverse
    void Set(const RwMatrix* w) {
        has_      = w != nullptr;
        invState_ = 0;
        if (w) {
            world_ = *w;
        }
    }
    bool Has() const { return has_; }

    // 0x7646E0: c0..c3 = transpose(world * (view * proj)) (world omitted when none)
    void WorldViewProjT(float* out, const float* view, const float* proj) const {
        float vp[16];
        D3dxMatrixMultiply(vp, view, proj);
        if (has_) {
            float w[16];
            std::memcpy(w, &world_, sizeof(w));
            w[3] = 0.0f; w[7] = 0.0f; w[11] = 0.0f; w[15] = 1.0f; // 0x8D7390 keeps its identity pads; only xyz of the four rows are copied
            D3dxMatrixMultiplyTranspose(out, w, vp);
        } else {
            D3dxMatrixTranspose(out, vp);
        }
    }

    // 0x7647B0: the inverse of the world matrix as a D3D matrix (identity when there is none / it is flagged identity)
    void InverseD3D(float* out) const {
        const float* inv = Inverse();
        std::memcpy(out, inv, 16 * sizeof(float));
    }

    // 0x764D30: a direction into object space (inverse, orthonormalised when the world matrix is not orthonormal)
    void Direction(float* out, const float* in) const {
        if (Trivial()) {
            std::memcpy(out, in, 3 * sizeof(float));
            return;
        }
        if ((world_.flags & 3) == 3) {
            D3dxVec3TransformNormal(out, in, Inverse());
            return;
        }
        RwMatrix on;
        rwx::OrthoNormalize(&on, reinterpret_cast<const RwMatrix*>(Inverse())); // 0x7F1930 on the cached inverse
        D3dxVec3TransformNormal(out, in, reinterpret_cast<const float*>(&on));
    }

    // 0x764E70: a position into object space
    void Position(float* out, const float* in) const {
        if (Trivial()) {
            std::memcpy(out, in, 3 * sizeof(float));
            return;
        }
        D3dxVec3TransformCoord(out, in, Inverse());
    }

    // 0x764F60: a world-space radius scaled by the inverse of the world matrix's scale
    float ObjectRadius(float radius) const {
        if (!has_ || (world_.flags & 3) == 3) {
            return radius;
        }
        float v[3] = {rwx::FromBits(0x3F13CD3Au), rwx::FromBits(0x3F13CD3Au), rwx::FromBits(0x3F13CD3Au)};
        D3dxVec3TransformNormal(v, v, Inverse());
        const float len2 = rwx::F((M(v[1], v[1]) + M(v[0], v[0])) + M(v[2], v[2]));
        return rwx::F(M(rwx::InvSqrt(len2), radius));
    }

private:
    bool Trivial() const { return !has_ || (world_.flags & kMatrixIdentity) == kMatrixIdentity; }
    const float* Inverse() const {
        // the exe's "identity" block at 0x8753D0 is an RwMatrix identity: its fourth word is the RwMatrix flags (identity | orthonormal), so the bone builder's
        // multiplications by it take the identity shortcut (copy of the other operand)
        static const float kIdentity[16] = {1, 0, 0, rwx::FromBits(0x00020003U), 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        if (Trivial()) {
            return kIdentity;
        }
        if (invState_ == 0) {
            if ((world_.flags & 3) == 3) {
                OrthoInverse(inv_, world_);
            } else {
                RwMatrix g;
                rwx::Invert(&g, &world_); // 0x7F2070
                std::memcpy(inv_, &g, sizeof(inv_));
                inv_[3] = 0.0f; inv_[7] = 0.0f; inv_[11] = 0.0f; inv_[15] = 1.0f;
            }
            invState_ = 1;
        }
        return inv_;
    }

    bool             has_      = false;
    RwMatrix         world_{};
    mutable int      invState_ = 0;
    mutable float    inv_[16]{};
};

//--------------------------------------------------------------------------------------------------
// 0x7C78A0: the bone matrices (3 float4 per bone: the transposed upper 3x4 of the product)
//--------------------------------------------------------------------------------------------------
struct BoneSource {
    u32             hierFlags   = 0;       // RpHAnimHierarchy flags: 2 = no matrices (use the node frames' LTMs), 0x4000 = local space matrices
    int             numNodes    = 0;
    const RwMatrix* skinToBone  = nullptr; // inverse bind matrices, numNodes entries
    const RwMatrix* hierMatrices = nullptr; // hierarchy matrices (flags & 2 == 0)
    const RwMatrix* const* nodeLTM = nullptr; // LTM of every node frame (flags & 2)
};
constexpr u32 kHierNoMatrices = 0x2, kHierLocalSpace = 0x4000;

inline void StoreBone(float* o, const RwMatrix& t) {
    const float* m = reinterpret_cast<const float*>(&t);
    o[0] = m[0]; o[1] = m[4]; o[2] = m[8];  o[3] = m[12];
    o[4] = m[1]; o[5] = m[5]; o[6] = m[9];  o[7] = m[13];
    o[8] = m[2]; o[9] = m[6]; o[10] = m[10]; o[11] = m[14];
}

inline void BuildBoneMatrices(float* out, const BoneSource& s, const WorldXform& world) {
    RwMatrix t1, t2, w;
    if (s.hierFlags & kHierNoMatrices) {
        world.InverseD3D(reinterpret_cast<float*>(&w));
        for (int i = 0; i < s.numNodes; i++, out += 12) {
            rwx::Multiply(&t1, &s.skinToBone[i], s.nodeLTM[i]);
            rwx::Multiply(&t2, &t1, &w);
            StoreBone(out, t2);
        }
    } else if (s.hierFlags & kHierLocalSpace) {
        for (int i = 0; i < s.numNodes; i++, out += 12) {
            rwx::Multiply(&t1, &s.skinToBone[i], &s.hierMatrices[i]);
            StoreBone(out, t1);
        }
    } else {
        world.InverseD3D(reinterpret_cast<float*>(&w));
        for (int i = 0; i < s.numNodes; i++, out += 12) {
            rwx::Multiply(&t1, &s.skinToBone[i], &s.hierMatrices[i]);
            rwx::Multiply(&t2, &t1, &w);
            StoreBone(out, t2);
        }
    }
}

//--------------------------------------------------------------------------------------------------
// the shader key (see skin_vs.h): b0 = dir count | point count << 4, b1 = spot count | texcoord sets << 4, b2 = flags, b3 = fog mode | texgen << 2
//--------------------------------------------------------------------------------------------------
struct LightKey { u8 b[4]; };

enum GeometryFlags : u32 { kGeoPrelit = 0x8, kGeoNormals = 0x10, kGeoLight = 0x20, kGeoModulate = 0x40 };

// 0x7CB190 (fog: RwRenderStateGet(FOGENABLE) != 0, then FOGTYPE 1 linear / 2 exp / 3 exp2)
inline LightKey BeginKey(u32 geometryFlags, u32 numTexCoordSets, bool fogEnabled, u32 fogType) {
    LightKey k{};
    k.b[1] = static_cast<u8>(numTexCoordSets << 4);
    if (geometryFlags & kGeoPrelit) {
        k.b[2] |= 0x10;
    }
    if (geometryFlags & kGeoNormals) {
        k.b[2] |= 0x60;
    }
    if (geometryFlags & kGeoModulate) {
        k.b[2] |= 0x80;
    }
    if (fogEnabled) {
        if (fogType == 1) {
            k.b[3] = static_cast<u8>((k.b[3] & 0xFD) | 1);
        } else if (fogType == 2) {
            k.b[3] = static_cast<u8>((k.b[3] & 0xFE) | 2);
        } else if (fogType == 3) {
            k.b[3] |= 3;
        }
    }
    return k;
}

//--------------------------------------------------------------------------------------------------
// 0x761170: the lights
//--------------------------------------------------------------------------------------------------
enum LightType : int { kLightDirectional = 1, kLightAmbient = 2, kLightPoint = 0x80, kLightSpot = 0x81, kLightSpotSoft = 0x82 };
constexpr u32 kLightFlagAtomics = 0x1; // rpLIGHTLIGHTATOMICS
constexpr int kMaxLightsPerKind = 15;

struct LightRef {
    int      type          = 0;
    u32      flags         = 0;
    float    color[3]      = {};
    float    radius        = 0.0f;
    float    minusCosAngle = 0.0f;
    RwMatrix ltm{};        // the light frame's LTM (at = direction, pos = position)
};

struct Collected {
    float    ambient[3] = {};
    int      nDir = 0, nPoint = 0, nSpot = 0;
    LightRef dir[kMaxLightsPerKind], point[kMaxLightsPerKind], spot[kMaxLightsPerKind];
    bool     found = false;
};

// The world's global lights (list order) and, for geometry with normals, the local lights touching the atomic's world bounding sphere.
// `Lights` yields LightRef by index; the sphere test is the exe's (0x76130C..0x76133A): accepted when (r_light + r_sphere)^2 > |sphere - light|^2
// (x87 doubles, no float spill).
struct WorldView {
    int             numGlobal = 0;
    const LightRef* global    = nullptr;
    int             numLocal  = 0;
    const LightRef* local     = nullptr;
    float           sphere[4] = {}; // x y z radius, RpAtomicGetWorldBoundingSphere
};

inline Collected CollectLights(const WorldView& wv, u32 geometryFlags) {
    Collected c;
    const bool normals = (geometryFlags & kGeoNormals) != 0;
    for (int i = 0; i < wv.numGlobal; i++) {
        const LightRef& l = wv.global[i];
        if (!(l.flags & kLightFlagAtomics)) {
            continue;
        }
        if (l.type == kLightDirectional) {                 // (the exe tests subtype 1 first, then 2)
            if (normals && c.nDir < kMaxLightsPerKind) {
                c.dir[c.nDir++] = l;
                c.found = true;
            }
        } else if (l.type == kLightAmbient) {
            for (int k = 0; k < 3; k++) {
                c.ambient[k] = rwx::F(rwx::D(l.color[k]) + rwx::D(c.ambient[k])); // fld colour; fadd [buf]; fstp [buf]
            }
        }
    }
    if (!normals) {
        return c;
    }
    for (int i = 0; i < wv.numLocal; i++) {
        const LightRef& l = wv.local[i];
        if (!(l.flags & kLightFlagAtomics)) {
            continue;
        }
        const double a = static_cast<double>(wv.sphere[0]) - static_cast<double>(l.ltm.pos.x);
        const double b = static_cast<double>(wv.sphere[1]) - static_cast<double>(l.ltm.pos.y);
        const double cc = static_cast<double>(wv.sphere[2]) - static_cast<double>(l.ltm.pos.z);
        const double r = static_cast<double>(l.radius) + static_cast<double>(wv.sphere[3]);
        const double d2 = (b * b + a * a) + cc * cc;
        if (!(r * r > d2)) {
            continue; // fcompp C0|C3 (r^2 <= d2) or unordered
        }
        if (l.type == kLightPoint) {
            if (c.nPoint < kMaxLightsPerKind) {
                c.point[c.nPoint++] = l;
                c.found = true;
            }
        } else if (l.type > kLightPoint && l.type <= kLightSpotSoft) {
            if (c.nSpot < kMaxLightsPerKind) {
                c.spot[c.nSpot++] = l;
                c.found = true;
            }
        }
    }
    return c;
}

inline void Put3(float* d, const float* s) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; }

// 0x761170 from the point where the lights are collected on: `buf` = c4 (the ambient constant); returns the next free constant; `key` gets the light counts.
// (b0/b1-low are rewritten, b1-high (texture sets) is kept: the exe starts with b0 = 0, b1 &= 0xF0.)
inline float* BuildLightConstants(float* buf, u32 geometryFlags, bool haveWorld, const Collected& c, const WorldXform& wx, LightKey& key) {
    key.b[0] = 0;
    key.b[1] &= 0xF0;
    buf[0] = 0.0f; buf[1] = 0.0f; buf[2] = 0.0f;
    buf[3] = (geometryFlags & kGeoPrelit) ? 0.0f : 1.0f;
    if (!(geometryFlags & kGeoLight) || !haveWorld) {
        return buf + 4;
    }
    buf[0] = c.ambient[0]; buf[1] = c.ambient[1]; buf[2] = c.ambient[2];
    float* p = buf + 4;
    if (!c.found) {
        return p;
    }
    if (c.nDir) {
        for (int i = 0; i < c.nDir; i++) {
            wx.Direction(p, &c.dir[i].ltm.at.x);   // 0x764D30(ltm + 0x20)
            p[3] = 0.0f;
            p += 4;
            Put3(p, c.dir[i].color);
            p[3] = 0.0f;
            p += 4;
        }
        key.b[0] = static_cast<u8>((key.b[0] & 0xF0) | (c.nDir & 0xF));
    }
    if (c.nPoint) {
        float* slot = nullptr;
        for (int i = 0; i < c.nPoint; i++) {
            if ((i & 3) == 0) {
                slot = p;
                p += 4;
            }
            wx.Position(p, &c.point[i].ltm.pos.x); // 0x764E70(ltm + 0x30)
            p[3] = 0.0f;
            p += 4;
            Put3(p, c.point[i].color);
            p[3] = 0.0f;
            *slot = wx.ObjectRadius(c.point[i].radius);
            *slot = rwx::F(1.0 / static_cast<double>(*slot));
            slot++;
            p += 4;
        }
        key.b[0] = static_cast<u8>((key.b[0] & 0x0F) | ((c.nPoint << 4) & 0xF0));
    }
    if (c.nSpot) {
        float* slot = nullptr;
        for (int i = 0; i < c.nSpot; i++) {
            const LightRef& l = c.spot[i];
            if ((i & 3) == 0) {
                slot = p;
                p += 4;
            }
            wx.Position(p, &l.ltm.pos.x);
            p[3] = 0.0f;
            p += 4;
            wx.Direction(p, &l.ltm.at.x);
            p[3] = 0.0f;
            p += 4;
            const double cosv = std::cos(LightAcos(l.minusCosAngle)); // 0x751AE0, then fcos
            // 0x81: hard edge; 0x82: soft edge unless cos >= 0.999f ([0x863E2C]; fcom leaves C0 clear for >= and unordered)
            if (l.type == kLightSpot || !(cosv < static_cast<double>(rwx::FromBits(0x3F7FBE77U)))) {
                p[0] = rwx::FromBits(0x4479FFFFU);
                p[1] = rwx::F(-cosv * static_cast<double>(rwx::FromBits(0x4479FFFFU))); // fchs; fmul [0x875290]
            } else {
                const double recip = 1.0 / (1.0 - cosv);
                p[0] = rwx::F(recip);
                p[1] = rwx::F(-cosv * recip);
            }
            p[2] = 0.5f;
            p[3] = 0.0f;
            p += 4;
            Put3(p, l.color);
            p[3] = 0.0f;
            *slot = wx.ObjectRadius(l.radius);
            *slot = rwx::F(1.0 / static_cast<double>(*slot));
            slot++;
            p += 4;
        }
        key.b[1] = static_cast<u8>((key.b[1] & 0xF0) | (c.nSpot & 0xF));
    }
    return p;
}


//--------------------------------------------------------------------------------------------------
// light / material / fog constants (0x761720, 0x761820, 0x7618B0)
//--------------------------------------------------------------------------------------------------
// 0x761720: multiplies the ambient colour by `ambient` and every light colour by `diffuse` (in place, c4 = buf), returns the number of constants from c4 to
// the end of the last light (the exe uploads them with SetVertexShaderConstantF(4, buf, count))
inline int ScaleLightColours(float* buf, const LightKey& key, float ambient, float diffuse) {
    float* p = buf;
    for (int k = 0; k < 3; k++) {
        p[k] = rwx::F(M(ambient, p[k]));
    }
    p += 4;
    for (int i = 0; i < (key.b[0] & 0xF); i++, p += 8) {
        for (int k = 0; k < 3; k++) {
            p[4 + k] = rwx::F(M(diffuse, p[4 + k]));
        }
    }
    for (int i = 0; i < (key.b[0] >> 4); i++, p += 8) {
        if ((i & 3) == 0) {
            p += 4;
        }
        for (int k = 0; k < 3; k++) {
            p[4 + k] = rwx::F(M(diffuse, p[4 + k]));
        }
    }
    for (int i = 0; i < (key.b[1] & 0xF); i++, p += 16) {
        if ((i & 3) == 0) {
            p += 4;
        }
        for (int k = 0; k < 3; k++) {
            p[12 + k] = rwx::F(M(diffuse, p[12 + k]));
        }
    }
    return static_cast<int>((p - buf) / 4);
}

// 0x761820: the material colour (RGBA bytes) as 4 floats / 255 (0x859A3C = 1/255f)
inline void MaterialColourConst(float* out, u32 rgba) {
    const float k = rwx::FromBits(0x3B808081U);
    for (int i = 0; i < 4; i++) {
        out[i] = rwx::F(rwx::D(static_cast<float>((rgba >> (8 * i)) & 0xFF)) * rwx::D(k));
    }
}

// 0x7618B0: the fog constant (linear: 1/(far - fog), -fog/(far - fog); exp: density * log2(e) in x; w = 1)
inline void FogConst(float* out, u32 fogMode, float camFar, float camFogPlane, float fogDensity) {
    out[2] = 0.0f;
    out[3] = 1.0f;
    if (fogMode == 1) {
        const double d = 1.0 / (static_cast<double>(camFar) - static_cast<double>(camFogPlane));
        out[0] = rwx::F(d);
        out[1] = rwx::F(-(d * static_cast<double>(camFogPlane)));
    } else {
        out[0] = rwx::F(rwx::D(fogDensity) * rwx::D(rwx::FromBits(0x3FB8AA3BU))); // [0x875294] = log2(e)
        out[1] = 0.0f;
    }
}

//--------------------------------------------------------------------------------------------------
// the render function 0x7C8060 + the table entries it calls (0x761030 mesh render)
//--------------------------------------------------------------------------------------------------
struct SkinData {
    int       boneLimit    = 0;       // per-mesh bone budget (the exe's skin+0x1C)
    int       numUsedBones = 0;
    const u8* usedBones    = nullptr;
    int       numWeights   = 0;
    const u8* remap        = nullptr; // bone id -> constant slot (skin split data), rleCount/rle may be null
    const u8* rleCount     = nullptr; // per mesh: start, size
    const u8* rle          = nullptr; // runs: first bone id, count
};

struct MeshData {
    const void* material   = nullptr;
    const void* texture    = nullptr;
    u32         color      = 0xFFFFFFFFu; // RGBA bytes in memory order (r = byte 0)
    float       ambient    = 1.0f;        // material surface properties
    float       diffuse    = 1.0f;
    u32         vertexAlpha = 0;
    u32         baseIndex = 0, numVertices = 0, startIndex = 0, numPrimitives = 0;
    void*       vertexShader = nullptr;   // out: the shader the mesh was drawn with (the exe stores it in the instance and clears it again)
};

struct Env {
    WorldXform*     world = nullptr;      // 0xC94C70 module
    const RwMatrix* atomicLTM = nullptr;
    const float*    view = nullptr;       // D3D view / projection matrices (16 floats)
    const float*    proj = nullptr;
    // Begin / Lighting
    u32             geometryFlags = 0;
    u32             numTexCoordSets = 0;
    bool            fogEnabled = false;
    u32             fogType = 0;
    float           fogDensity = 0.0f;
    float           camFar = 0.0f, camFogPlane = 0.0f;
    bool            haveWorld = false;    // RwEngineInstance->curWorld
    WorldView       lights;
    // skin
    SkinData        skin;
    BoneSource      bones;
    // geometry
    void*           indexBuffer = nullptr;
    u32             primType = 0;
    int             numMeshes = 0;
    MeshData*       meshes = nullptr;
    // caps (0xC978E8 / 0xC978EC)
    u32             maxBones = 0;
    u32             maxConstants = 0;
    int           (*numLightConstants)(const LightKey&) = nullptr;  // 0x75EDD0
    float*          constBuf = nullptr;   // 256 * 4 floats = the exe's 0xC970A0 block (c0..)
    float*          boneBuf = nullptr;    // numNodes * 12 floats = 0xC978AC
    bool            buildBones = true;    // false: boneBuf is already filled (glue: identity bones for a skin without hierarchy)
};

// 0x7C8060. `Dev` is the device side:
//   SetConst(start, const float*, count)  IDirect3DDevice9::SetVertexShaderConstantF (vtable +0x178)
//   SetRenderState(state, value)          0x7FC2D0
//   VertexAlpha(bool)                     0x7FE0A0
//   SetIndices(ib) [only for ib != 0], SetStreams(), SetDecl()      (header setup; the exe caches ib / declaration)
//   GetShader(const MeshData&, const LightKey&, u8 info[8]) -> void*  table[2] = 0x761010 -> 0x75EED0
//   SetPSNull(), SetTexture(tex), SetTSS(stage, type, value), SetVS(vs), Flush(), DrawIndexed(prim, base, minIndex, nVerts, start, nPrims), Draw(prim, start, nPrims)
template <class Dev>
void MeshRender(Dev& dev, const MeshData& m, void* indexBuffer, u32 primType) { // 0x761030
    dev.SetPSNull();
    if (m.texture) {
        dev.SetTexture(m.texture);
        dev.SetTSS(0, 1, 4); // COLOROP MODULATE
        dev.SetTSS(0, 2, 2); // COLORARG1 TEXTURE
        dev.SetTSS(0, 3, 0); // COLORARG2 DIFFUSE
        dev.SetTSS(0, 4, 4); // ALPHAOP MODULATE
        dev.SetTSS(0, 5, 2); // ALPHAARG1 TEXTURE
        dev.SetTSS(0, 6, 0); // ALPHAARG2 DIFFUSE
    } else {
        dev.SetTexture(nullptr);
        dev.SetTSS(0, 1, 3); // COLOROP SELECTARG2
        dev.SetTSS(0, 3, 0);
        dev.SetTSS(0, 4, 3); // ALPHAOP SELECTARG2
        dev.SetTSS(0, 6, 0);
    }
    dev.SetVS(m.vertexShader);
    dev.Flush();
    if (indexBuffer) {
        dev.DrawIndexed(primType, m.baseIndex, 0, m.numVertices, m.startIndex, m.numPrimitives);
    } else {
        dev.Draw(primType, m.baseIndex, m.numPrimitives);
    }
}

template <class Dev>
void RenderHW(Dev& dev, Env& e) {
    LightKey key = BeginKey(e.geometryFlags, e.numTexCoordSets, e.fogEnabled, e.fogType);              // 0x7CB190
    float prevAmbient = 1.0f, prevDiffuse = 1.0f;                                                         // [esp+0x18], [esp+0x24]
    u8    cacheColourReg = 0xFF, cacheFogReg = 0xFF, cacheBoneReg = 0xFF;                                 // [esp+0x12], [esp+0x13], [esp+0x11]
    // 0x7C809C..: the number of weights goes into key byte 2, bits 1..3
    key.b[2] = static_cast<u8>((key.b[2] & ~0x0E) | ((e.skin.numWeights * 2) & 0x0E));
    dev.SetRenderState(0x88, 1);                                                                          // D3DRS_CLIPPING
    e.world->Set(e.atomicLTM);                                                                            // 0x764650
    e.world->WorldViewProjT(e.constBuf, e.view, e.proj);                                                  // 0x7646E0 -> c0..c3
    const Collected col = CollectLights(e.lights, e.geometryFlags);
    float* const c4 = e.constBuf + 16;
    float* const end = BuildLightConstants(c4, e.geometryFlags, e.haveWorld, col, *e.world, key);         // 0x761170
    dev.SetConst(0, e.constBuf, static_cast<unsigned>((end - e.constBuf) / 4));
    if (e.buildBones) {
        BuildBoneMatrices(e.boneBuf, e.bones, *e.world);                                                  // 0x7C78A0
    }
    if (e.indexBuffer) {
        dev.SetIndices(e.indexBuffer);
    }
    dev.SetStreams();
    dev.SetDecl();

    const u8*  rleCount = e.skin.rleCount;
    u8         info[8]  = {};
    u32        cachedColour = e.numMeshes ? e.meshes[0].color : 0;
    for (int mi = 0; mi < e.numMeshes; mi++) {
        MeshData& m = e.meshes[mi];
        dev.VertexAlpha(!((m.color >> 24) == 0xFF && m.vertexAlpha == 0));                               // 0x7FE0A0
        m.vertexShader = dev.GetShader(m, key, info);
        if (static_cast<u32>(e.numLightConstants(key)) + 3u * static_cast<u32>(e.skin.boneLimit) > e.maxConstants) {   // 0x7C8221 .. 0x7C8295
            do {
                if (key.b[1] & 0xF) {
                    key.b[1] = static_cast<u8>((key.b[1] & 0xF0) | ((key.b[1] - 1) & 0xF));
                } else if (key.b[0] & 0xF0) {
                    key.b[0] = static_cast<u8>((key.b[0] & 0x0F) | ((key.b[0] & 0xF0) - 0x10));
                } else if (key.b[0] & 0xF) {
                    key.b[0] = static_cast<u8>((key.b[0] & 0xF0) | ((key.b[0] - 1) & 0xF));
                } else if (key.b[3] & 3) {
                    key.b[3] = static_cast<u8>(key.b[3] & 0xFC);
                } else {
                    break;
                }
            } while (static_cast<u32>(e.numLightConstants(key)) + 3u * static_cast<u32>(e.skin.boneLimit) > e.maxConstants);
            m.vertexShader = dev.GetShader(m, key, info);
            prevAmbient    = rwx::F(1.0 - rwx::D(m.ambient));
        }
        if (prevAmbient != m.ambient || prevDiffuse != m.diffuse) {                                       // 0x7C82BC
            const float a = rwx::F(rwx::D(m.ambient) / rwx::D(prevAmbient));
            const float d = rwx::F(rwx::D(m.diffuse) / rwx::D(prevDiffuse));
            dev.SetConst(4, c4, static_cast<unsigned>(ScaleLightColours(c4, key, a, d)));
            prevAmbient = m.ambient;
            prevDiffuse = m.diffuse;
        }
        if (info[0] != cacheColourReg || cachedColour != m.color) {                                       // 0x7C8310
            cacheColourReg = info[0];
            cachedColour   = m.color;
            float v[4];
            MaterialColourConst(v, m.color);
            dev.SetConst(info[0], v, 1);
        }
        if (info[1] != cacheFogReg) {                                                                     // 0x7C8342
            cacheFogReg = info[1];
            float v[4];
            FogConst(v, key.b[3] & 3, e.camFar, e.camFogPlane, e.fogDensity);
            dev.SetConst(info[1], v, 1);
        }
        // bone matrices
        if (rleCount) {                                                                                   // 0x7C836A: split skin, bones of this mesh
            cacheBoneReg    = info[4];
            const unsigned start = rleCount[0], size = rleCount[1];
            rleCount += 2;
            for (unsigned r = 0; r < size; r++) {
                const unsigned first = e.skin.rle[2 * (start + r)], n = e.skin.rle[2 * (start + r) + 1];
                for (unsigned j = 0; j < n; j++) {
                    const unsigned id = first + j;
                    dev.SetConst(info[4] + 3u * e.skin.remap[id], e.boneBuf + 12u * id, 3);
                }
            }
        } else if (cacheBoneReg != info[4]) {                                                             // 0x7C8418
            cacheBoneReg = info[4];
            const u32 numNodes = static_cast<u32>(e.bones.numNodes);
            if (numNodes > e.maxBones || static_cast<u32>(e.skin.numUsedBones) < numNodes) {
                // runs of consecutive used bone ids (0x7C84E0 when the hierarchy is larger than the constant budget: the registers are packed by position in the
                // used-bone list; 0x7C8456 otherwise: every bone keeps its own register, base + 3 * id)
                const bool   packed = numNodes > e.maxBones;
                const unsigned used = static_cast<unsigned>(e.skin.numUsedBones);
                unsigned s = 0;
                while (s < used) {
                    const unsigned startIdx = s;
                    while (s + 1 < used && e.skin.usedBones[s] + 1u == e.skin.usedBones[s + 1]) {
                        s++;
                    }
                    s++;
                    const unsigned id = e.skin.usedBones[startIdx];
                    dev.SetConst(info[4] + 3u * (packed ? startIdx : id), e.boneBuf + 12u * id, 3u * (s - startIdx));
                }
            } else {
                dev.SetConst(info[4], e.boneBuf, 3u * numNodes);                                          // 0x7C84C4: everything at once
            }
        }
        MeshRender(dev, m, e.indexBuffer, e.primType);                                                    // table[3]
        m.vertexShader = nullptr;
    }
}

} // namespace rwskin
NOTSA_CLANG_SSE2_END
