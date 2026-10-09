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

#include <cassert>

namespace rw { void calczShiftScale(Camera* cam); } // camera.cpp (librw): zScale/zShift from the device z range, not in a header (superseded by CalcZShiftScale below)

static_assert(rwSPHEREOUTSIDE == rw::Camera::SPHEREOUTSIDE && rwSPHEREBOUNDARY == rw::Camera::SPHEREBOUNDARY && rwSPHEREINSIDE == rw::Camera::SPHEREINSIDE);
static_assert(rwPERSPECTIVE == rw::Camera::PERSPECTIVE && rwPARALLEL == rw::Camera::PARALLEL);
static_assert(rwRASTERFLIPWAITVSYNC == rw::Raster::FLIPWAITVSYNCH);

namespace {
using rw::d3d::d3d9Globals;

RwUInt32 s_StencilClear = 0; // RwD3D9SetStencilClear (the exe's StencilClearValue, default 0)

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
// dirty frames, calls the device's begin-update and returns NULL if that failed (curCamera stays set). It does NOT touch `renderFrame`: that counter
// ("Camera display count", read by the env-map pipeline as its once-per-frame key) is incremented by RpWorldRender (0x750453).
static bool s_worldRenderBumpsFrame = false; // set by NotsaRwBumpRenderFrame(): once RpWorldRender (world.cpp, not this file) bumps it, BeginUpdate must not
RwCamera* RwCameraBeginUpdate(RwCamera* camera) {
    if (!camera || !camera->getFrame() || !camera->frameBuffer || !rw::d3d::d3ddevice) {
        return nullptr;
    }
    RwEngineInstance->curCamera = camera;
    if (!s_worldRenderBumpsFrame) {
        RwEngineInstance->renderFrame++; // fallback until world.cpp calls NotsaRwBumpRenderFrame(): one bump per begin-update, close to once per frame
    }
    camera->beginUpdate();
    rw::d3d::d3ddevice->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(&camera->devView));
    rw::d3d::d3ddevice->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX*>(&camera->devProj));
    return camera;
}

// 01r: to be called by the RpWorldRender shim (world.cpp) exactly where the exe increments RwEngineInstance->renderFrame (0x750453)
void NotsaRwBumpRenderFrame() {
    s_worldRenderBumpsFrame = true;
    RwEngineInstance->renderFrame++;
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
RwCamera* RwCameraShowRaster(RwCamera* camera, void* /*pDev*/, RwUInt32 flags) {
    if (!camera || !camera->frameBuffer || !rw::d3d::d3ddevice) {
        return nullptr;
    }
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
