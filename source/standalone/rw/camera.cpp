// P2B-02b: RwCamera* on top of librw (rw::Camera, D3D9 backend).
//   RwCamera{Create,Destroy,BeginUpdate,EndUpdate,Clear,ShowRaster,FrustumTestSphere,SetNearClipPlane,SetFarClipPlane,SetProjection,SetViewWindow},
//   RwD3D9SetStencilClear, RwD3D9CameraAttachWindow.
// Function classes (see .notes/P2B_SHIM_PLAN.md): D = direct, A = adapter, W = written here because librw has nothing equivalent.
//
// What the game reads from a camera and where it comes from:
//   frustumPlanes[0..5] {plane,closestX/Y/Z}  librw buildPlanes() runs from the camera's sync callback (Frame::syncDirty, called by
//                                             BeginUpdate): far, near, right, top, left, bottom = RW's order (FxManager reads [2..5]).
//   viewMatrix                                librw cameraSync(): the RW "world -> clip" matrix CCamera::m_mViewMatrix attaches to.
//   zScale/zShift                             librw calczShiftScale() (RW's formula, from the device z range) on Create/SetNear/SetFar/SetProjection.
//   recipViewWindow                           NOT a librw field (WindowedMode.cpp was its only reader and is not built): RwCameraGetRecipViewWindow()
//                                             in rwextra.h computes 1/viewWindow on demand.
//   D3D view / projection                     RW uploaded D3DTS_VIEW / D3DTS_PROJECTION in BeginUpdate for the fixed-function pipelines; librw only
//                                             fills cam->devView / cam->devProj (its shaders read them), so BeginUpdate here uploads them too. The
//                                             matrices are bit-identical to what _rwD3D9CameraBeginUpdate built (checked in WindowedMode.cpp).
// Only needs fakerw + librw + the CRT (also built by the PCH-less unit test tests/standalone/rw_frame_camera_test.cpp).
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

// librw's D3D9 device internals (d3ddevice, d3d9Globals)
#include <src/d3d/rwd3dimpl.h>

#include "camera_sync.h"   // 01r: the exe's camera sync callback 0x7EE5A0 (lifted from the asm, bit-exact vs the exe), replaces librw's cameraSync

#include <cassert>
#include <crtdbg.h>
#include <Windows.h>
#include "standalone/Fixups.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace rw { void calczShiftScale(Camera* cam); } // camera.cpp (librw): zScale/zShift from the device z range, not in a header (superseded by CalcZShiftScale below)

static_assert(rwSPHEREOUTSIDE == rw::Camera::SPHEREOUTSIDE && rwSPHEREBOUNDARY == rw::Camera::SPHEREBOUNDARY && rwSPHEREINSIDE == rw::Camera::SPHEREINSIDE);
static_assert(rwPERSPECTIVE == rw::Camera::PERSPECTIVE && rwPARALLEL == rw::Camera::PARALLEL);
static_assert(rwRASTERFLIPWAITVSYNC == rw::Raster::FLIPWAITVSYNCH);

namespace {
using rw::d3d::d3d9Globals;

RwUInt32 s_StencilClear = 0; // RwD3D9SetStencilClear (the exe's StencilClearValue, default 0)

// 01r: sync callback of every camera (0x7EE5A0 ported bit-exactly in camera_sync.h): view matrix, frustum corners, planes (table _rwInvSqrt), bound box
void NotsaCameraSyncCB(rw::ObjectWithFrame* obj) {
    rwx::CameraSync(reinterpret_cast<rw::Camera*>(obj));
}

// 01r: the exe's zScale / zShift (0x7EE200), evaluated as the asm does. librw's calczShiftScale is algebraically the same for the perspective case but
// shrinks the device range differently (it updates N before using it for F); here delta = (F - N) * 1e-4f is taken once. n, f are 1/near, 1/far for a
// perspective camera, near, far for a parallel one; zScale = (F' - N') / (f - n), zShift = 0.5 * ((F' + N') - (n + f) * zScale) with F' = float(F - delta).
void CalcZShiftScale(RwCamera* cam) {
    if (!rw::engine) {
        return;
    }
    const double dNear = rw::engine->device.zNear, dFar = rw::engine->device.zFar;
    double       n, f;
    if (cam->projection == rw::Camera::PARALLEL) {
        n = cam->nearPlane;
        f = cam->farPlane;
    } else {
        f = 1.0 / static_cast<double>(cam->farPlane);
        n = 1.0 / static_cast<double>(cam->nearPlane);
    }
    const double delta = (dFar - dNear) * static_cast<double>(0.0001f);
    const float  far2  = static_cast<float>(dFar - delta);
    const double near2 = delta + dNear;
    const double zs    = (static_cast<double>(far2) - near2) / (f - n);
    cam->zScale        = static_cast<float>(zs);
    cam->zShift        = static_cast<float>(0.5 * ((static_cast<double>(far2) + near2) - (n + f) * zs));
}

bool DepthFormatHasStencil(D3DFORMAT f) {
    return f == D3DFMT_D24S8 || f == D3DFMT_D24X4S4 || f == D3DFMT_D24FS8 || f == D3DFMT_D15S1;
}

// Same "release video memory, Reset(), restore" dance librw only runs from showRaster()/beginUpdate(): see ResetDeviceNow() in engine.cpp (02a).
// showRaster() re-creates the device as soon as the requested presentation interval differs from present.PresentationInterval and never
// dereferences the raster, so the interval is flipped on purpose and the original one is requested.
bool ResetDeviceNow() {
    if (!rw::d3d::d3ddevice || !rw::engine) {
        return false;
    }
    auto&      present = d3d9Globals.present;
    const bool vsync   = present.PresentationInterval == D3DPRESENT_INTERVAL_ONE;
    present.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_ONE;
    rw::engine->device.showRaster(reinterpret_cast<rw::Raster*>(1), vsync ? rw::Raster::FLIPWAITVSYNCH : 0);
    return rw::d3d::d3ddevice->TestCooperativeLevel() == D3D_OK;
}
} // namespace

// D (+ zScale/zShift init, librw leaves the whole camera uninitialised past the fields create() sets): a new camera has no frame, no
// rasters and RW's defaults (view window 1x1, near 0.05, far 10, fog 5, perspective).
RwCamera* RwCameraCreate(void) {
    RwCamera* cam = rw::Camera::create();
    if (cam) {
        cam->originalSync = NotsaCameraSyncCB;   // librw's world plugin wraps the sync callback (worldCameraSync -> originalSync): replace the inner one
        CalcZShiftScale(cam);
    }
    return cam;
}

// D: librw asserts the camera is neither in a world nor in a clump (RW reports an error and fails); the frame is detached.
RwBool RwCameraDestroy(RwCamera* camera) {
    if (!camera || camera->world || camera->clump) {
        return FALSE;
    }
    camera->destroy();
    if (RwEngineInstance->curCamera == camera) {
        RwEngineInstance->curCamera = nullptr;
    }
    return TRUE;
}

// A: Camera::beginUpdate = syncDirty (LTMs, camera frustum/view matrix), device beginUpdate (view/proj into devView/devProj, window-size
// reset, render surfaces, viewport, BeginScene) + the fixed-function view/projection upload RW did. The exe (0x7EF370) sets `curCamera` first, syncs the
// dirty frames, calls the device's begin-update and returns NULL if that failed (curCamera stays set). It does NOT touch `renderFrame` ("Camera display
// count", the env-map pipeline's once-per-frame key): the only writer in the whole exe is the world's render callback 0x750430 (inc word [engine + 8] at
// 0x750453), reached through RpWorldRender 0x74F570, which the game never calls (no call/jmp/data reference anywhere in the exe) - SA renders through its
// own CRenderer -> RpAtomicRender. So renderFrame stays at its initial 0 for the whole run (the env-map guard `RenderFrame != renderFrame` never fires).
RwCamera* RwCameraBeginUpdate(RwCamera* camera) {
    if (!camera || !camera->getFrame() || !camera->frameBuffer || !rw::d3d::d3ddevice) {
        return nullptr;
    }
    RwEngineInstance->curCamera = camera;
    camera->beginUpdate();
    rw::d3d::d3ddevice->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&camera->devView));
    rw::d3d::d3ddevice->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX*>(&camera->devProj));
    return camera;
}

// 01r: RpWorldRender's counter bump (exe 0x750453, the world render callback 0x750430)
void NotsaRwBumpRenderFrame() {
    RwEngineInstance->renderFrame++;
}

// 0x74F570: RW's world render. The one observable effect without BSP sectors (librw has none) is the exe's render callback 0x750430, which makes the world
// current and increments renderFrame (the env-map pipeline's once-per-frame key). The game never calls this function (no reference anywhere in the exe);
// it lives here, next to the counter, so that it links wherever the camera does (world.cpp is built without rwglobals.cpp in several unit tests).
RpWorld* RpWorldRender(RpWorld* world);
RpWorld* RpWorldRender(RpWorld* world) {
    NotsaRwBumpRenderFrame();
    return world;
}

// D: EndScene; RW clears curCamera outside an update.
RwCamera* RwCameraEndUpdate(RwCamera* camera) {
    if (!camera || !rw::d3d::d3ddevice) {
        return nullptr;
    }
    camera->endUpdate();
    RwEngineInstance->curCamera = nullptr;
    return camera;
}

// A: Camera::clear + the stencil clear VALUE (librw always clears stencil to 0). The rwCAMERACLEAR{IMAGE,Z,STENCIL} bits are the
// D3DCLEAR_{TARGET,ZBUFFER,STENCIL} bits, z is cleared to 1.0. IDirect3DDevice9::Clear fails as a whole when D3DCLEAR_STENCIL is
// requested for a depth format without stencil, so the bit is dropped then (RW checked the format too).
RwCamera* RwCameraClear(RwCamera* camera, RwRGBA* colour, RwInt32 clearMode) {
    if (!camera || !camera->frameBuffer || !rw::d3d::d3ddevice) {
        return nullptr;
    }
    RwUInt32 mode = static_cast<RwUInt32>(clearMode) & (rwCAMERACLEARIMAGE | rwCAMERACLEARZ | rwCAMERACLEARSTENCIL);
    if ((mode & rwCAMERACLEARSTENCIL) && !DepthFormatHasStencil(d3d9Globals.present.AutoDepthStencilFormat)) {
        mode &= ~static_cast<RwUInt32>(rwCAMERACLEARSTENCIL);
    }
    RwRGBA black{0, 0, 0, 0};
    camera->clear(colour ? colour : &black, mode); // sets the render surfaces + viewport, clears with stencil value 0
    if ((mode & rwCAMERACLEARSTENCIL) && s_StencilClear != 0) {
        rw::d3d::d3ddevice->Clear(0, nullptr, D3DCLEAR_STENCIL, 0, 1.0f, s_StencilClear); // same surfaces / viewport as the clear above
    }
    return camera;
}

// A: Raster::show on the camera's frame buffer = Present. RW's `pDev` was the destination HWND; librw presents to the device window
// (the only window in the standalone build) and ignores it. flags: rwRASTERFLIPWAITVSYNC == Raster::FLIPWAITVSYNCH.
#ifdef NOTSA_STANDALONE_RUN
// S5: NOTSA_STANDALONE_SCREENSHOT=<k> writes frame_<n>.bmp (back buffer, 24 bit) into the current directory before every k-th Present (k>=1, first 30 files)
static void ShimDumpBackBuffer() {
    static int  s_Every = -2;
    static int  s_Frame = 0, s_Written = 0;
    if (s_Every == -2) {
        const char* e = std::getenv("NOTSA_STANDALONE_SCREENSHOT");
        s_Every = e ? (std::atoi(e) > 0 ? std::atoi(e) : 1) : -1;
    }
    if (s_Every < 0 || s_Written >= 30 || (s_Frame++ % s_Every) != 0) {
        return;
    }
    IDirect3DDevice9* dev = rw::d3d::d3ddevice;
    IDirect3DSurface9 *bb = nullptr, *sys = nullptr;
    if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) {
        return;
    }
    D3DSURFACE_DESC d{};
    bb->GetDesc(&d);
    if (SUCCEEDED(dev->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr)) && SUCCEEDED(dev->GetRenderTargetData(bb, sys))) {
        D3DLOCKED_RECT lr{};
        if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
            char name[64];
            std::snprintf(name, sizeof(name), "frame_%d.bmp", s_Written++);
            if (FILE* f = std::fopen(name, "wb")) {
                const uint32_t rowBytes = (d.Width * 3 + 3) & ~3u, imgSize = rowBytes * d.Height;
                uint8_t        hdr[54] = { 'B', 'M' };
                auto           put32   = [&](int o, uint32_t v) { std::memcpy(hdr + o, &v, 4); };
                put32(2, 54 + imgSize); put32(10, 54); put32(14, 40); put32(18, d.Width); put32(22, d.Height);
                hdr[26] = 1; hdr[28] = 24; put32(34, imgSize);
                std::fwrite(hdr, 1, 54, f);
                std::vector<uint8_t> row(rowBytes, 0);
                for (int y = (int)d.Height - 1; y >= 0; y--) {
                    const uint8_t* src = (const uint8_t*)lr.pBits + (size_t)y * lr.Pitch;
                    for (UINT x = 0; x < d.Width; x++) {
                        if (d.Format == D3DFMT_R5G6B5) {
                            const uint16_t v = ((const uint16_t*)src)[x];
                            row[x * 3 + 0] = (uint8_t)((v & 31) * 255 / 31); row[x * 3 + 1] = (uint8_t)(((v >> 5) & 63) * 255 / 63); row[x * 3 + 2] = (uint8_t)(((v >> 11) & 31) * 255 / 31);
                        } else {
                            row[x * 3 + 0] = src[x * 4 + 0]; row[x * 3 + 1] = src[x * 4 + 1]; row[x * 3 + 2] = src[x * 4 + 2];
                        }
                    }
                    std::fwrite(row.data(), 1, rowBytes, f);
                }
                std::fclose(f);
            }
            sys->UnlockRect();
        }
    }
    if (sys) sys->Release();
    bb->Release();
}
#endif

RwCamera* RwCameraShowRaster(RwCamera* camera, void* /*pDev*/, RwUInt32 flags) {
    if (!camera || !camera->frameBuffer || !rw::d3d::d3ddevice) {
        return nullptr;
    }
#ifdef NOTSA_STANDALONE_RUN
    ShimDumpBackBuffer();
    if (std::getenv("NOTSA_STANDALONE_MEMLOG")) {
        static int n = 0;
        if (++n % 100 == 0) {
            _CrtMemState st;
            _CrtMemCheckpoint(&st);
            size_t priv = 0;
            MEMORY_BASIC_INFORMATION mbi;
            for (char* a = nullptr; VirtualQuery(a, &mbi, sizeof(mbi)) && (uintptr_t)a < 0x7FFE0000; a += mbi.RegionSize) {
                if (mbi.State == MEM_COMMIT) priv += mbi.RegionSize;
            }
            notsa::standalone::Fixups::Log("memlog frame %d: crt heap %lu bytes in %lu blocks, committed %lu KB", n, (unsigned long)(st.lSizes[1] + st.lSizes[2]), (unsigned long)(st.lCounts[1] + st.lCounts[2]), (unsigned long)(priv / 1024));
        }
    }
    if (std::getenv("NOTSA_STANDALONE_NOPRESENT")) { // S5 diagnostics: skip Present (leak hunting)
        return camera;
    }
#endif
    camera->showRaster(flags);
    return camera;
}

// 0x7EE2D0: 6 planes in RW's order (far, near, right, top, left, bottom), distance = (n.y*c.y + n.x*c.x) + n.z*c.z - plane distance (x87 order);
// OUTSIDE (0) if it exceeds the radius for any plane, else BOUNDARY (1) if it is above -radius for any plane, else INSIDE (2). NaN counts as inside.
// Planes are those of the last sync (BeginUpdate / syncDirty).
RwFrustumTestResult RwCameraFrustumTestSphere(const RwCamera* camera, const RwSphere* sphere) {
    int res = rwSPHEREINSIDE;
    for (int i = 0; i < 6; i++) {
        const auto&  pl = camera->frustumPlanes[i].plane;
        const double d  = ((double)pl.normal.y * sphere->center.y + (double)pl.normal.x * sphere->center.x) + (double)pl.normal.z * sphere->center.z - (double)pl.distance;
        if (d > (double)sphere->radius) {
            return rwSPHEREOUTSIDE;
        }
        if (d > -(double)sphere->radius) {
            res = rwSPHEREBOUNDARY;
        }
    }
    return static_cast<RwFrustumTestResult>(res);
}

// D: also refreshes zScale/zShift (librw) and marks the camera's frame dirty so the frustum is rebuilt on the next sync.
RwCamera* RwCameraSetNearClipPlane(RwCamera* camera, RwReal nearClip) {
    if (!camera) {
        return nullptr;
    }
    camera->setNearPlane(nearClip);
    CalcZShiftScale(camera);
    return camera;
}

RwCamera* RwCameraSetFarClipPlane(RwCamera* camera, RwReal farClip) {
    if (!camera) {
        return nullptr;
    }
    camera->setFarPlane(farClip);
    CalcZShiftScale(camera);
    return camera;
}

// A: + zScale/zShift recompute (they depend on the projection type; librw only does that in setNear/setFar).
RwCamera* RwCameraSetProjection(RwCamera* camera, RwCameraProjection projection) {
    if (!camera || (projection != rwPERSPECTIVE && projection != rwPARALLEL)) {
        return nullptr;
    }
    camera->setProjection(projection);
    CalcZShiftScale(camera);
    return camera;
}

// D: recipViewWindow is derived on demand (RwCameraGetRecipViewWindow); the frame is marked dirty so frustum + view matrix are rebuilt.
RwCamera* RwCameraSetViewWindow(RwCamera* camera, const RwV2d* viewWindow) {
    if (!camera || !viewWindow || viewWindow->x == 0.0f || viewWindow->y == 0.0f) {
        return nullptr;
    }
    camera->setViewWindow(viewWindow);
    return camera;
}

// W: RW's StencilClearValue (exe global 0xC97C44), the value RwCameraClear clears the stencil buffer to (StencilShadows sets 0).
void RwD3D9SetStencilClear(RwUInt32 stencilClear) {
    s_StencilClear = stencilClear;
}

// W: attach the camera's swap chain to another window (WindowedMode.cpp used it when switching between its main and top-level windows;
// librw has no API). Only the single device window exists in the standalone build, so attaching to it (or to null) is a no-op; another
// HWND becomes the device window and the device is reset in place (windowed: the back buffer takes the new client size).
RwBool RwD3D9CameraAttachWindow(void* camera, void* hwnd) {
    if (!camera || !rw::d3d::d3ddevice) {
        return FALSE;
    }
    HWND wnd = static_cast<HWND>(hwnd);
    if (!wnd || wnd == d3d9Globals.window) {
        return TRUE;
    }
    if (!IsWindow(wnd)) {
        return FALSE;
    }
    const HWND oldWindow = d3d9Globals.window;
    d3d9Globals.window                  = wnd;
    d3d9Globals.present.hDeviceWindow   = wnd;
    if (d3d9Globals.present.Windowed) {
        d3d9Globals.present.BackBufferWidth  = 0;
        d3d9Globals.present.BackBufferHeight = 0;
    }
    if (!ResetDeviceNow()) {
        d3d9Globals.window                = oldWindow;
        d3d9Globals.present.hDeviceWindow = oldWindow;
        ResetDeviceNow();
        return FALSE;
    }
    return TRUE;
}
#endif
