// P2B-24c: the numerics of the exe's CPU skinning path (RenderWare 3.6 D3D9 skin pipeline, node body 0x7C7B90, branch "no hardware skin").
/*
    The skin node (0x7C7B90) skins on the CPU when `skin+0x20` (the "hardware skin" flag written by the resentry creation 0x7C8D60 as `hierarchy && 0x7C8A00(atomic)`,
    i.e. HW T&L && VS/PS >= 1.1 && the skin's bone budget fits the constant registers) is ZERO and the atomic has a hierarchy: it computes the bone matrices into a
    global matrix array, runs one of eight kernels over every vertex (positions + normals into the locked vertex buffer) and then draws with the stock fixed-function
    callbacks (0x7C85B0). This header holds the numerics shared by the glue (pipeline_skin_cpu.cpp) and the exe-oracle test (rw_skin_cpu_oracle_test.cpp).

    Ported from gta_sa_compact.exe (all .text):
      0x7CB390 + 0x7CAA30 (tail)   SSE bone matrices (4x4 per bone, 64 bytes, the pad column zeroed)    BuildBonesSse
      0x7CA850 + 0x7CA6C0          x87 bone matrices (the product is stored transposed: 3 coefficients + translation per output axis)   BuildBonesX87
      0x7CAAD0 (1 weight, 1 bone)  / 0x7CAB80 (1 weight, several bones) / 0x7CAF50 (2 weights) / 0x7CAC60 (3-4 weights)    Skin1BoneSse / Skin1WeightSse / Skin2Sse / Skin4Sse
      0x7CA330 / 0x7CA240 / 0x7CA410 / 0x7C9DA0   the x87 twins (lifted by tools/standalone/lift_x87.py -> pipeline_skin_cpu_lifted.inc)
      the dispatch of 0x7C7B90 (0x7C7E44..0x7C7FC3: by [0xC980A4] SSE flag, skin->numWeights, skin->numUsedBones)     SkinVertices
    Float discipline: x87 intermediates are `double` (lifted code), SSE code is written with SSE intrinsics in the order of the asm (operand order of every add / mul kept:
    the first operand is the destination register of the asm instruction, which matters only for NaN payloads).
*/
#pragma once
#include "fakerw.h"
#include "rwmath_exact.h"
#include <emmintrin.h>
#include <xmmintrin.h>
NOTSA_CLANG_SSE2_BEGIN

#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace rwskincpu {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;

//--------------------------------------------------------------------------------------------------
// harness for the lifted x87 code (same conventions as rwskin::lift in pipeline_skin_core.h)
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
#include "pipeline_skin_cpu_lifted.inc"
#undef MEMABS

// calls a lifted cdecl function with `args` on a private emulated stack (the callee reads [esp + 4 * k] exactly like the asm)
inline Ret Call(Ret (*fn)(R), std::initializer_list<R> args) {
    alignas(16) static u8 stack[0x400];
    R esp = reinterpret_cast<R>(stack + sizeof(stack) - 0x40);
    esp -= 4 * static_cast<R>(args.size());
    R at = esp;
    for (R a : args) {
        WU32(at, static_cast<u32>(a));
        at += 4;
    }
    esp -= 4; // return address slot
    WU32(esp, 0);
    return fn(esp);
}
} // namespace lift

//--------------------------------------------------------------------------------------------------
// bone matrices
//--------------------------------------------------------------------------------------------------
struct BoneSource {
    u32                     hierFlags    = 0;       // RpHAnimHierarchy flags: 2 = no matrices (use the node frames' LTMs), 0x4000 = local space matrices
    int                     numNodes     = 0;       // hierarchy +4: the number of matrices produced
    const RwMatrix*         skinToBone   = nullptr; // inverse bind matrices (skin +0xC), numNodes entries
    const RwMatrix*         hierMatrices = nullptr; // hierarchy +8 (flags & 2 == 0)
    const RwMatrix* const*  nodeLTM      = nullptr; // LTM of every node frame (hierarchy +0x10 -> node info +0xC -> frame LTM; flags & 2)
    const RwMatrix*         atomicLTM    = nullptr; // LTM of the atomic's frame (0x7F0990 of atomic +4)
};
constexpr u32 kHierNoMatrices = 0x2, kHierLocalSpace = 0x4000;

// 0x7CB390 (hierarchy flavours as in the HW twin 0x7C78A0) into `out` (numNodes * 64 bytes, 16-byte aligned, consecutive RwMatrix slots), then the tail of 0x7CAA30:
// the four pad dwords of every matrix are zeroed (the kernels load whole rows with movaps and ignore lane 3).
inline void BuildBonesSse(float* out, const BoneSource& s) {
    RwMatrix t1, w;
    RwMatrix* slot = reinterpret_cast<RwMatrix*>(out);
    if (s.hierFlags & kHierNoMatrices) {
        rwx::Invert(&w, s.atomicLTM);                                      // 0x7F2070
        for (int i = 0; i < s.numNodes; i++) {
            rwx::Multiply(&t1, &s.skinToBone[i], s.nodeLTM[i]);            // 0x7F18B0
            rwx::Multiply(&slot[i], &t1, &w);
        }
    } else if (s.hierFlags & kHierLocalSpace) {
        for (int i = 0; i < s.numNodes; i++) {
            rwx::Multiply(&slot[i], &s.skinToBone[i], &s.hierMatrices[i]);
        }
    } else {
        rwx::Invert(&w, s.atomicLTM);
        for (int i = 0; i < s.numNodes; i++) {
            rwx::Multiply(&t1, &s.skinToBone[i], &s.hierMatrices[i]);
            rwx::Multiply(&slot[i], &t1, &w);
        }
    }
    for (int i = 0; i < s.numNodes; i++) {                                 // 0x7CAA89..0x7CAA9D
        float* m = out + 16 * i;
        m[3] = 0.0f; m[7] = 0.0f; m[11] = 0.0f; m[15] = 0.0f;
    }
}

// 0x7CA6C0(dst, a, b): the transposed product of the x87 path
inline void MultiplyT87(float* dst, const RwMatrix* a, const RwMatrix* b) {
    lift::Call(lift::mulT_7CA6C0, {reinterpret_cast<lift::R>(dst), reinterpret_cast<lift::R>(a), reinterpret_cast<lift::R>(b)});
}

// 0x7CA850: like 0x7CB390 but the last multiplication is 0x7CA6C0 (only 12 floats of every 64-byte slot are written, the x87 kernels read nothing else)
inline void BuildBonesX87(float* out, const BoneSource& s) {
    RwMatrix t1, w;
    if (s.hierFlags & kHierNoMatrices) {
        rwx::Invert(&w, s.atomicLTM);
        for (int i = 0; i < s.numNodes; i++) {
            rwx::Multiply(&t1, &s.skinToBone[i], s.nodeLTM[i]);
            MultiplyT87(out + 16 * i, &t1, &w);
        }
    } else if (s.hierFlags & kHierLocalSpace) {
        for (int i = 0; i < s.numNodes; i++) {
            MultiplyT87(out + 16 * i, &s.skinToBone[i], &s.hierMatrices[i]);
        }
    } else {
        rwx::Invert(&w, s.atomicLTM);
        for (int i = 0; i < s.numNodes; i++) {
            rwx::Multiply(&t1, &s.skinToBone[i], &s.hierMatrices[i]);
            MultiplyT87(out + 16 * i, &t1, &w);
        }
    }
}

//--------------------------------------------------------------------------------------------------
// SSE kernels. `dstNrm` / `srcNrm`: normals are skinned only when srcNrm != nullptr (0x7CAB36 / 0x7CACFA: `test`), dstNrm is then written (rotation part only).
// `m` / `mats`: 16-byte aligned 4x4 matrices of 64 bytes (rows = RwMatrix right / up / at / pos).
//--------------------------------------------------------------------------------------------------
namespace detail {
struct Rows { __m128 r0, r1, r2, r3; };

inline Rows LoadRows(const float* m) {
    return {_mm_load_ps(m), _mm_load_ps(m + 4), _mm_load_ps(m + 8), _mm_load_ps(m + 12)}; // movaps
}
inline void Store3(float* p, __m128 v) { // movss [p], v; shufps 0xE5; movss [p+4]; movhlps; movss [p+8]
    alignas(16) float t[4];
    _mm_store_ps(t, v);
    p[0] = t[0]; p[1] = t[1]; p[2] = t[2];
}
// pos = ((z * r2 + r3) + (y * r1 + x * r0)); the three splats come from three movss + shufps 0
inline void XformPos(float* dst, const float* src, const Rows& r) {
    __m128 x = _mm_set1_ps(src[0]), y = _mm_set1_ps(src[1]), z = _mm_set1_ps(src[2]);
    x = _mm_mul_ps(x, r.r0);
    y = _mm_mul_ps(y, r.r1);
    z = _mm_mul_ps(z, r.r2);
    y = _mm_add_ps(y, x);
    z = _mm_add_ps(z, r.r3);
    z = _mm_add_ps(z, y);
    Store3(dst, z);
}
// normal = (z * r2) + (y * r1 + x * r0)
inline void XformNrm(float* dst, const float* src, const Rows& r) {
    __m128 x = _mm_set1_ps(src[0]), y = _mm_set1_ps(src[1]), z = _mm_set1_ps(src[2]);
    x = _mm_mul_ps(x, r.r0);
    y = _mm_mul_ps(y, r.r1);
    z = _mm_mul_ps(z, r.r2);
    y = _mm_add_ps(y, x);
    z = _mm_add_ps(z, y);
    Store3(dst, z);
}
inline Rows ScaledRows(const float* m, __m128 w) { // movaps + mulps xmm, [weight splat]
    const Rows r = LoadRows(m);
    return {_mm_mul_ps(r.r0, w), _mm_mul_ps(r.r1, w), _mm_mul_ps(r.r2, w), _mm_mul_ps(r.r3, w)};
}
inline Rows AddRows(const Rows& a, const Rows& b) { // addps a, b  (a = destination)
    return {_mm_add_ps(a.r0, b.r0), _mm_add_ps(a.r1, b.r1), _mm_add_ps(a.r2, b.r2), _mm_add_ps(a.r3, b.r3)};
}
inline u32 FloatBits(float f) { u32 b; std::memcpy(&b, &f, 4); return b; }
inline const float* Mat(const float* mats, u32 index) { return mats + 16 * index; } // shl 6 on bytes
} // namespace detail

// 0x7CAAD0: all vertices through one matrix (numWeights == 1 and a single used bone)
inline void Skin1BoneSse(u32 count, const float* m, u8* dstPos, const float* srcPos, u8* dstNrm, const float* srcNrm, u32 stride) {
    const detail::Rows r = detail::LoadRows(m);
    for (; count; count--) {
        detail::XformPos(reinterpret_cast<float*>(dstPos), srcPos, r);
        srcPos += 3;
        dstPos += stride;
        if (srcNrm) {
            detail::XformNrm(reinterpret_cast<float*>(dstNrm), srcNrm, r);
            srcNrm += 3;
            dstNrm += stride;
        }
    }
}

// 0x7CAB80: one weight per vertex: the matrix of bone `indices[4 * i] & 0xFF`
inline void Skin1WeightSse(u32 count, const u8* indices, const float* mats, u8* dstPos, const float* srcPos, u8* dstNrm, const float* srcNrm, u32 stride) {
    for (; count; count--) {
        const detail::Rows r = detail::LoadRows(detail::Mat(mats, *indices));
        indices += 4;
        detail::XformPos(reinterpret_cast<float*>(dstPos), srcPos, r);
        srcPos += 3;
        dstPos += stride;
        if (srcNrm) {
            detail::XformNrm(reinterpret_cast<float*>(dstNrm), srcNrm, r);
            srcNrm += 3;
            dstNrm += stride;
        }
    }
}

// 0x7CAF50: two weights (weights[0] bits >= 1.0f bits as unsigned = rigid on bone 0; else w0 * M0 + w1 * M1)
inline void Skin2Sse(u32 count, const float* weights, const u8* indices, const float* mats, u8* dstPos, const float* srcPos, u8* dstNrm, const float* srcNrm, u32 stride) {
    using namespace detail;
    for (; count; count--, weights += 4, indices += 4) {
        Rows r;
        if (FloatBits(weights[0]) >= 0x3F800000u) {                         // cmp eax, [0x882DE4] (1.0f); jb = blend
            r = LoadRows(Mat(mats, indices[0]));
        } else {
            const Rows a = ScaledRows(Mat(mats, indices[0]), _mm_set1_ps(weights[0]));
            const Rows b = ScaledRows(Mat(mats, indices[1]), _mm_set1_ps(weights[1]));
            r = AddRows(b, a);                                              // addps xmm4, xmm0 ...
        }
        XformPos(reinterpret_cast<float*>(dstPos), srcPos, r);
        srcPos += 3;
        dstPos += stride;
        if (srcNrm) {
            XformNrm(reinterpret_cast<float*>(dstNrm), srcNrm, r);
            srcNrm += 3;
            dstNrm += stride;
        }
    }
}

// 0x7CAC60: three or four weights; bones 2 / 3 are added only while their weights are > 0 (ucomiss against 0.0f at 0x882DE8; NaN skips)
inline void Skin4Sse(u32 count, const float* weights, const u8* indices, const float* mats, u8* dstPos, const float* srcPos, u8* dstNrm, const float* srcNrm, u32 stride) {
    using namespace detail;
    for (; count; count--, weights += 4, indices += 4) {
        Rows r;
        if (FloatBits(weights[0]) >= 0x3F800000u) {
            r = LoadRows(Mat(mats, indices[0]));
        } else {
            const Rows a = ScaledRows(Mat(mats, indices[0]), _mm_set1_ps(weights[0]));
            const Rows b = ScaledRows(Mat(mats, indices[1]), _mm_set1_ps(weights[1]));
            r = AddRows(b, a);
            if (weights[2] > 0.0f) {
                r = AddRows(r, ScaledRows(Mat(mats, indices[2]), _mm_set1_ps(weights[2])));
                if (weights[3] > 0.0f) {
                    r = AddRows(r, ScaledRows(Mat(mats, indices[3]), _mm_set1_ps(weights[3])));
                }
            }
        }
        XformPos(reinterpret_cast<float*>(dstPos), srcPos, r);
        srcPos += 3;
        dstPos += stride;
        if (srcNrm) {
            XformNrm(reinterpret_cast<float*>(dstNrm), srcNrm, r);
            srcNrm += 3;
            dstNrm += stride;
        }
    }
}

//--------------------------------------------------------------------------------------------------
// x87 kernels (lifted). Argument order = the exe's cdecl order.
//--------------------------------------------------------------------------------------------------
inline lift::R P(const void* p) { return reinterpret_cast<lift::R>(p); }

inline void Skin1Bone87(u32 count, const float* m, u8* dstPos, const float* srcPos, u8* dstNrm, const float* srcNrm, u32 stride) {
    lift::Call(lift::w1b_7CA330, {count, P(m), P(dstPos), P(srcPos), P(dstNrm), P(srcNrm), stride});
}
inline void Skin1Weight87(u32 count, const u8* indices, const float* mats, u8* dstPos, const float* srcPos, u8* dstNrm, const float* srcNrm, u32 stride) {
    lift::Call(lift::w1n_7CA240, {count, P(indices), P(mats), P(dstPos), P(srcPos), P(dstNrm), P(srcNrm), stride});
}
inline void Skin2X87(u32 count, const float* weights, const u8* indices, const float* mats, u8* dstPos, const float* srcPos, u8* dstNrm, const float* srcNrm, u32 stride) {
    lift::Call(lift::w2_7CA410, {count, P(weights), P(indices), P(mats), P(dstPos), P(srcPos), P(dstNrm), P(srcNrm), stride});
}
inline void Skin4X87(u32 count, const float* weights, const u8* indices, const float* mats, u8* dstPos, const float* srcPos, u8* dstNrm, const float* srcNrm, u32 stride) {
    lift::Call(lift::w4_7C9DA0, {count, P(weights), P(indices), P(mats), P(dstPos), P(srcPos), P(dstNrm), P(srcNrm), stride});
}

//--------------------------------------------------------------------------------------------------
// the dispatch of 0x7C7B90 (0x7C7E44 .. 0x7C7FC3)
//--------------------------------------------------------------------------------------------------
struct SkinInput {
    u32          numVertices = 0;
    int          numWeights  = 0;       // skin +0x10
    int          numUsedBones = 0;      // skin +4
    const u8*    usedBones   = nullptr; // skin +8
    const float* weights     = nullptr; // skin +0x18 (float4 per vertex)
    const u8*    indices     = nullptr; // skin +0x14 (4 bytes per vertex)
    const float* srcPos      = nullptr; // morph target 0 vertices
    const float* srcNrm      = nullptr; // morph target 0 normals when the geometry has them, else nullptr
};

// `mats`: the output of BuildBonesSse (sse = true) or BuildBonesX87 (sse = false). dstNrm is ignored when in.srcNrm == nullptr.
inline void SkinVertices(bool sse, const SkinInput& in, const float* mats, u8* dstPos, u8* dstNrm, u32 stride) {
    const u32 n = in.numVertices;
    if (sse) {
        if (in.numWeights > 2) {
            Skin4Sse(n, in.weights, in.indices, mats, dstPos, in.srcPos, dstNrm, in.srcNrm, stride);
        } else if (in.numWeights > 1) {
            Skin2Sse(n, in.weights, in.indices, mats, dstPos, in.srcPos, dstNrm, in.srcNrm, stride);
        } else if (in.numUsedBones == 1) {
            Skin1BoneSse(n, detail::Mat(mats, in.usedBones[0]), dstPos, in.srcPos, dstNrm, in.srcNrm, stride);
        } else {
            Skin1WeightSse(n, in.indices, mats, dstPos, in.srcPos, dstNrm, in.srcNrm, stride);
        }
    } else {
        if (in.numWeights > 2) {
            Skin4X87(n, in.weights, in.indices, mats, dstPos, in.srcPos, dstNrm, in.srcNrm, stride);
        } else if (in.numWeights > 1) {
            Skin2X87(n, in.weights, in.indices, mats, dstPos, in.srcPos, dstNrm, in.srcNrm, stride);
        } else if (in.numUsedBones == 1) {
            Skin1Bone87(n, detail::Mat(mats, in.usedBones[0]), dstPos, in.srcPos, dstNrm, in.srcNrm, stride);
        } else {
            Skin1Weight87(n, in.indices, mats, dstPos, in.srcPos, dstNrm, in.srcNrm, stride);
        }
    }
}

} // namespace rwskincpu
NOTSA_CLANG_SSE2_END
