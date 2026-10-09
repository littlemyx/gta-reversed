// P2B-09 unit test: platform glue (standalone/rw/platform.cpp) on librw/D3D9.
//  1. _rwD3D9Device{Set,Get}RestoreCallback store / return the callback
//  2. the restore callback runs after every device Reset (multisampling change via ResetDeviceNow -> showRaster, video mode change, window
//     resize detected by RwCameraBeginUpdate), not otherwise, only after librw restored the device, and not once cleared
//  3. gamma: the device from RwD3D9GetCurrentD3DDevice() round-trips a ramp the way CGamma::SetGamma writes it; RwD3D9GetCaps() is the mutable
//     caps cache CGamma::Init edits
// Needs a D3D9 HAL adapter (works under Wine/wined3d). D3DResourceSystem is game code (pools in the data image) and has nothing to test here.
// Exit code 0 = all checks passed.
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>
#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { const bool ok_ = !!(c); std::printf("%-4s %s\n", ok_ ? "ok" : "FAIL", #c); if (!ok_) ++g_fail; } while (0)

static int  g_calls         = 0;
static bool g_deviceOkInCb  = true;
static bool g_targetsOkInCb = true;
static void RestoreCb() {
    ++g_calls;
    auto* dev = rw::d3d::d3ddevice;
    g_deviceOkInCb = g_deviceOkInCb && dev && dev->TestCooperativeLevel() == D3D_OK;
    IDirect3DSurface9* rt = nullptr;
    // librw restored the default render target / depth surface before we are called
    if (dev && SUCCEEDED(dev->GetRenderTarget(0, &rt))) {
        g_targetsOkInCb = g_targetsOkInCb && rt == rw::d3d::d3d9Globals.defaultRenderTarget;
        rt->Release();
    } else {
        g_targetsOkInCb = false;
    }
}
static void OtherCb() {}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    HWND wnd = CreateWindowA("STATIC", "rw_platform_test", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(wnd != nullptr);

    // ---- 1. callback storage (works without an engine)
    CHECK(_rwD3D9DeviceGetRestoreCallback() == nullptr);
    _rwD3D9DeviceSetRestoreCallback(OtherCb);
    CHECK(_rwD3D9DeviceGetRestoreCallback() == OtherCb);
    _rwD3D9DeviceSetRestoreCallback(_rwD3D9DeviceGetRestoreCallback()); // what CGame::InitialiseRenderWare does
    CHECK(_rwD3D9DeviceGetRestoreCallback() == OtherCb);
    _rwD3D9DeviceSetRestoreCallback(nullptr);
    CHECK(_rwD3D9DeviceGetRestoreCallback() == nullptr);
    CHECK(NotsaRwPlatform_DeviceResetCount() == 0);

    CHECK(RwEngineInit(nullptr, 0, 0) == TRUE);
    CHECK(RpAnisotPluginAttach() == TRUE && RpAnisotPluginAttach() == TRUE); // before Open; second call is a no-op
    RwEngineOpenParams params{wnd};
    CHECK(RwEngineOpen(&params) == TRUE);
    CHECK(RwEngineStart() == TRUE);
    const bool device = rw::d3d::d3ddevice != nullptr;
    std::printf("D3D9 device: %s\n", device ? "yes" : "NO (device-less run)");
    if (!device) {
        RwEngineClose(); RwEngineTerm();
        std::printf("FAILED (no device)\n");
        return 1;
    }
    auto* dev = static_cast<IDirect3DDevice9*>(RwD3D9GetCurrentD3DDevice());
    CHECK(dev == rw::d3d::d3ddevice);

    {   // anisotropy plugin attached: the per-texture level round-trips (default 1)
        rw::Texture* t = rw::Texture::create(nullptr);
        CHECK(t != nullptr);
        if (t) {
            CHECK(t->getMaxAnisotropy() == 1);
            t->setMaxAnisotropy(4);
            CHECK(t->getMaxAnisotropy() == 4);
            t->destroy();
        }
    }

    // ---- 2. restore callback
    _rwD3D9DeviceSetRestoreCallback(RestoreCb);
    const unsigned resets0 = NotsaRwPlatform_DeviceResetCount();
    g_calls = 0;
    CHECK(RwD3D9ChangeMultiSamplingLevels(1) == TRUE);              // ResetDeviceNow -> showRaster wrapper
    CHECK(g_calls == 1 && NotsaRwPlatform_DeviceResetCount() == resets0 + 1);
    CHECK(g_deviceOkInCb && g_targetsOkInCb);
    CHECK(RwD3D9ChangeMultiSamplingLevels(1) == TRUE);
    CHECK(g_calls == 2);

    CHECK(RwD3D9ChangeVideoMode(RwEngineGetCurrentVideoMode()) == TRUE); // video mode change path
    CHECK(g_calls == 3);

    // a window resize is picked up by Camera::beginUpdate (librw resets the device there)
    RwFrame*  frame = RwFrameCreate();
    RwCamera* cam   = RwCameraCreate();
    RwCameraSetFrame(cam, frame);
    rw::Raster* fb = rw::Raster::create(rw::d3d::d3d9Globals.present.BackBufferWidth, rw::d3d::d3d9Globals.present.BackBufferHeight, 0, rw::Raster::CAMERA);
    rw::Raster* zb = rw::Raster::create(rw::d3d::d3d9Globals.present.BackBufferWidth, rw::d3d::d3d9Globals.present.BackBufferHeight, 0, rw::Raster::ZBUFFER);
    CHECK(fb && zb);
    RwCameraSetRaster(cam, fb); RwCameraSetZRaster(cam, zb);
    RwCameraSetNearClipPlane(cam, 0.5f); RwCameraSetFarClipPlane(cam, 100.0f);
    const int before = g_calls;
    CHECK(RwCameraBeginUpdate(cam) == cam);                          // no reset: size unchanged
    RwCameraEndUpdate(cam);
    CHECK(g_calls == before);
    RECT r{};
    GetClientRect(wnd, &r);
    SetWindowPos(wnd, nullptr, 0, 0, 640 + 80, 480 + 40, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    RECT r2{};
    GetClientRect(wnd, &r2);
    std::printf("client %ldx%ld -> %ldx%ld\n", r.right, r.bottom, r2.right, r2.bottom);
    CHECK(r2.right != r.right || r2.bottom != r.bottom);
    const unsigned resets1 = NotsaRwPlatform_DeviceResetCount();
    CHECK(RwCameraBeginUpdate(cam) == cam);
    RwCameraEndUpdate(cam);
    CHECK(NotsaRwPlatform_DeviceResetCount() == resets1 + 1);
    CHECK(g_calls == before + 1);
    CHECK(rw::d3d::d3d9Globals.present.BackBufferWidth == (UINT)r2.right);

    // vsync flip through showRaster: a presentation interval change resets the device too
    CHECK(RwCameraShowRaster(cam, nullptr, rwRASTERFLIPWAITVSYNC) == cam);
    const int afterVsync = g_calls;
    CHECK(RwCameraShowRaster(cam, nullptr, rwRASTERFLIPWAITVSYNC) == cam);   // same interval: plain Present, no callback
    CHECK(g_calls == afterVsync);
    CHECK(RwCameraShowRaster(cam, nullptr, rwRASTERFLIPDONTWAIT) == cam);
    CHECK(g_calls == afterVsync + 1);

    // callback replaced / cleared
    _rwD3D9DeviceSetRestoreCallback(nullptr);
    const int calls = g_calls;
    CHECK(RwD3D9ChangeMultiSamplingLevels(1) == TRUE);
    CHECK(g_calls == calls);

    // ---- 3. gamma (CGamma::Init / SetGamma use exactly these calls)
    {
        D3DCAPS9* caps = (D3DCAPS9*)const_cast<void*>(RwD3D9GetCaps());
        CHECK(caps != nullptr);
        D3DCAPS9 devCaps{};
        CHECK(SUCCEEDED(dev->GetDeviceCaps(&devCaps)));
        const bool had = (caps->DeclTypes & D3DDTCAPS_DEC3N) != 0;
        caps->DeclTypes &= ~D3DDTCAPS_DEC3N;                                  // CGamma::Init edits the cache in place
        CHECK((((D3DCAPS9*)RwD3D9GetCaps())->DeclTypes & D3DDTCAPS_DEC3N) == 0);
        if (had) caps->DeclTypes |= D3DDTCAPS_DEC3N;
        std::printf("gamma caps: FULLSCREENGAMMA %d CANCALIBRATEGAMMA %d\n", !!(devCaps.Caps2 & D3DCAPS2_FULLSCREENGAMMA), !!(devCaps.Caps2 & D3DCAPS2_CANCALIBRATEGAMMA));

        D3DGAMMARAMP saved{}, table{}, back{};
        dev->GetGammaRamp(0, &saved);                                         // ms_SavedGamma
        const float level = 0.5f * 0.7f, power = 1.0f - level + 0.2f;
        for (int i = 0; i < 256; i++) {
            const auto g = static_cast<int16_t>(std::pow(float(i + 1) / 256.0f, power) * 65535.0f);
            table.red[i] = table.green[i] = table.blue[i] = (WORD)g;
        }
        dev->SetGammaRamp(0, D3DSGR_CALIBRATE, &table);
        dev->GetGammaRamp(0, &back);
        int diff = 0;
        for (int i = 0; i < 256; i++) diff += back.red[i] != table.red[i] || back.green[i] != table.green[i] || back.blue[i] != table.blue[i];
        std::printf("gamma ramp mismatches after round trip: %d (red[128] %u -> %u)\n", diff, table.red[128], back.red[128]);
        CHECK(diff == 0);
        dev->SetGammaRamp(0, D3DSGR_CALIBRATE, &saved);                       // restore (CGamma does this on exit)
        dev->GetGammaRamp(0, &back);
        diff = 0;
        for (int i = 0; i < 256; i++) diff += back.red[i] != saved.red[i];
        CHECK(diff == 0);
    }

    RwCameraSetRaster(cam, nullptr); RwCameraSetZRaster(cam, nullptr);
    fb->destroy(); zb->destroy();
    RwCameraSetFrame(cam, nullptr);
    RwCameraDestroy(cam); RwFrameDestroy(frame);

    CHECK(RwEngineStop() == TRUE);
    CHECK(RwEngineClose() == TRUE);
    CHECK(RwEngineTerm() == TRUE);
    DestroyWindow(wnd);
    std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "PASSED", g_fail);
    return g_fail;
}
