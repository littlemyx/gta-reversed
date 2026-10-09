#pragma once
/*
    01r camera sync: the exe's RwCamera sync callback 0x7EE5A0 (view matrix, frustum corners / planes / bound box) and RwMatrixOptimize 0x7F17E0, ported
    mechanically from the x87 code by tools/standalone/lift_x87.py (camera_sync_lifted.inc, generated) and verified bit-for-bit against the exe's own
    machine code by the oracle in tests/standalone/rw_math_stream_test.cpp.

    Why not librw's cameraSync: it builds the view matrix with a different expression order, normalises the frustum planes with exact sqrt instead of the
    table-based _rwInvSqrt and computes the frustum corners / plane distances differently; the plane coefficients feed RwCameraFrustumTestSphere (entity
    culling), where ~1e-4 differences flip edge cases.

    The lifted code works on the exe's RwCamera layout (RW 3.6: projection +0x14, viewMatrix +0x20, viewWindow +0x68, recipViewWindow +0x70, viewOffset +0x78,
    near +0x80, far +0x84, frustum planes +0x94, bound box +0x10C, corners +0x124; the frame's LTM at frame + 0x50). librw's Camera has another layout, so
    NotsaCameraSync() marshals into a scratch buffer of the exe's layout and copies the results back.

    Header-only: included by camera.cpp (the sync callback of every shim camera) and by the unit test.
*/
#include <cstdint>
#include <cstring>
#include <cmath>

#include "rwmath_exact.h"

namespace rwx::lift {
using R = uintptr_t;
struct Ret { R eax; double st0; };

inline uint32_t RU32(R a) { uint32_t v; std::memcpy(&v, reinterpret_cast<const void*>(a), 4); return v; }
inline uint32_t RU16(R a) { uint16_t v; std::memcpy(&v, reinterpret_cast<const void*>(a), 2); return v; }
inline uint32_t RU8(R a) { return *reinterpret_cast<const uint8_t*>(a); }
inline float    RF32(R a) { float v; std::memcpy(&v, reinterpret_cast<const void*>(a), 4); return v; }
inline double   RF64(R a) { double v; std::memcpy(&v, reinterpret_cast<const void*>(a), 8); return v; }
inline void WU32(R a, uint32_t v) { std::memcpy(reinterpret_cast<void*>(a), &v, 4); }
inline void WU16(R a, uint32_t v) { const uint16_t w = static_cast<uint16_t>(v); std::memcpy(reinterpret_cast<void*>(a), &w, 2); }
inline void WU8(R a, uint32_t v) { *reinterpret_cast<uint8_t*>(a) = static_cast<uint8_t>(v); }
inline void WF64(R a, double v) { std::memcpy(reinterpret_cast<void*>(a), &v, 8); }
inline bool PARITY8(uint32_t v) { v &= 0xFFu; v ^= v >> 4; v ^= v >> 2; v ^= v >> 1; return (v & 1u) == 0; }
inline double FromBits64(uint64_t u) { double d; std::memcpy(&d, &u, 8); return d; }

// the RW matrix plugin's default tolerances (RwMatrixOpen 0x7F16C0: normal, orthogonal, identity = 0.01f); the lifted RwMatrixOptimize reads them through
// the engine globals 0xC97B24 (engine) + 0xC979BC (plugin offset) + 0xC, which are pointed at this array.
inline float* MatrixTolerances() { static float t[3] = {rwx::FromBits(0x3C23D70Au), rwx::FromBits(0x3C23D70Au), rwx::FromBits(0x3C23D70Au)}; return t; }
inline uint32_t MemAbs(uint32_t addr) {
    switch (addr) {
    case 0xC97B24: return static_cast<uint32_t>(reinterpret_cast<R>(MatrixTolerances()) - 0xCu);
    case 0xC979BC: return 0u;
    default: std::abort();
    }
}
#define MEMABS(a) (::rwx::lift::MemAbs(a))

// 0x808F60 RwBBoxCalculate(out, vertices, n): out[0] = maximum, out[1] = minimum, both start at vertices[0]; the maximum is replaced when it is strictly
// below the vertex (ordered compare), the minimum when strictly above (NaN components never replace)
inline R RwxBBoxCalc(R esp) {
    float*       out = reinterpret_cast<float*>(RU32(esp));
    const float* v   = reinterpret_cast<const float*>(RU32(esp + 4));
    const uint32_t n = RU32(esp + 8);
    for (int k = 0; k < 3; k++) { out[k] = v[k]; out[3 + k] = v[k]; }
    v += 3;
    for (uint32_t i = 1; i < n; i++, v += 3) {
        for (int k = 0; k < 3; k++) if (out[3 + k] > v[k]) out[3 + k] = v[k];   // 0x808FA2..: minimum (second vector)
        for (int k = 0; k < 3; k++) if (out[k] < v[k]) out[k] = v[k];           // maximum
    }
    return reinterpret_cast<R>(out);
}
// 0x7EDD90 RwV3dTransformPoints(out, in, n, matrix) -> out (plugin dispatch to 0x7ED670)
inline R RwxTransformPoints(R esp) {
    RwV3d*          out = reinterpret_cast<RwV3d*>(RU32(esp));
    const RwV3d*    in  = reinterpret_cast<const RwV3d*>(RU32(esp + 4));
    const int32_t   n   = static_cast<int32_t>(RU32(esp + 8));
    const RwMatrix* m   = reinterpret_cast<const RwMatrix*>(RU32(esp + 12));
    for (int32_t i = 0; i < n; i++) rwx::TransformPoint(&out[i], &in[i], m);
    return reinterpret_cast<R>(out);
}

#include "camera_sync_lifted.inc"
#undef MEMABS
} // namespace rwx::lift

namespace rwx {
// exe layout offsets of RwCamera
namespace camoff {
constexpr int kProjection = 0x14, kView = 0x20, kViewWindow = 0x68, kRecip = 0x70, kOffset = 0x78, kNear = 0x80, kFar = 0x84, kPlanes = 0x94, kBBox = 0x10C, kCorners = 0x124, kSize = 0x1A0;
}

// Runs the exe's sync callback on a camera in the exe's layout: cam[kFrame] must point at a frame whose LTM is at +0x50 (here: `frameBuf`, size >= 0x90).
inline void CameraSyncExeLayout(uint8_t* cam) {
    alignas(16) uint8_t stack[0x400];
    lift::R esp = reinterpret_cast<lift::R>(stack + 0x3F0);
    esp -= 4; lift::WU32(esp, static_cast<uint32_t>(reinterpret_cast<lift::R>(cam)));
    esp -= 4;                                                          // return address slot
    (void)lift::cam_sync(esp);
}

// 1/viewWindow as RwCameraSetViewWindow 0x7EE410 computes it (fld1; fdiv; fstp dword)
inline float CameraRecip(float v) { return F(1.0 / D(v)); }
} // namespace rwx

// librw adapter (camera.cpp assigns this as the camera's sync callback). `RwCamera` = rw::Camera.
#ifdef NOTSA_RW_LIBRW
namespace rwx {
inline void CameraSync(rw::Camera* cam) {
    alignas(16) uint8_t buf[camoff::kSize] = {};
    alignas(16) uint8_t frame[0x90] = {};
    auto F32 = [&](int off) -> float& { return *reinterpret_cast<float*>(buf + off); };
    const rw::Matrix* ltm = &cam->getFrame()->ltm;
    std::memcpy(frame + 0x50, ltm, 0x40);
    *reinterpret_cast<uint32_t*>(buf + 4) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(frame));
    *reinterpret_cast<int32_t*>(buf + camoff::kProjection) = cam->projection;
    F32(camoff::kViewWindow) = cam->viewWindow.x;       F32(camoff::kViewWindow + 4) = cam->viewWindow.y;
    F32(camoff::kRecip) = CameraRecip(cam->viewWindow.x); F32(camoff::kRecip + 4) = CameraRecip(cam->viewWindow.y);
    F32(camoff::kOffset) = cam->viewOffset.x;           F32(camoff::kOffset + 4) = cam->viewOffset.y;
    F32(camoff::kNear) = cam->nearPlane;                F32(camoff::kFar) = cam->farPlane;
    CameraSyncExeLayout(buf);
    std::memcpy(&cam->viewMatrix, buf + camoff::kView, 0x40);      // pads included: they are not read by anybody
    static_assert(sizeof(rw::FrustumPlane) == 0x14 && sizeof(rw::BBox) == 0x18 && sizeof(rw::V3d) == 0xC);
    std::memcpy(cam->frustumPlanes, buf + camoff::kPlanes, 6 * 0x14);
    std::memcpy(&cam->frustumBoundBox, buf + camoff::kBBox, 0x18);
    std::memcpy(cam->frustumCorners, buf + camoff::kCorners, 8 * 0xC);
}
} // namespace rwx
#endif
