// P2B-02a unit test: RwEngine* shim over librw/D3D9. Init -> Open (hidden window) -> sub-systems / video modes -> Start -> Stop -> Close -> Term.
// Needs a D3D9 HAL adapter (works under Wine/wined3d). Exit code 0 = all checks passed.
#include "fakerw.h"
#include <cstdio>
#include <d3d9.h>

// engine.cpp calls these platform.cpp (P2B-09) hooks; this unit test does not link platform.cpp
void NotsaRwPlatform_OnEngineStarted() {}
void NotsaRwPlatform_OnEngineStopping() {}

static int g_fail = 0;
#define CHECK(c) do { const bool ok_ = !!(c); std::printf("%-4s %s\n", ok_ ? "ok" : "FAIL", #c); if (!ok_) ++g_fail; } while (0)

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    HWND wnd = CreateWindowA("STATIC", "rw_engine_test", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(wnd != nullptr);

    CHECK(RwEngineGetNumSubSystems() == -1);                        // not opened yet
    CHECK(RwEngineInit(nullptr, 0, 0) == TRUE && RwInitialized);
    CHECK(RwEngineInit(nullptr, 0, 0) == FALSE);                    // twice
    CHECK(RwEngineStart() == FALSE);                                // Init -> Start is not allowed
    RwEngineOpenParams params{nullptr};
    CHECK(RwEngineOpen(&params) == FALSE);                          // no window
    params.displayID = wnd;
    CHECK(RwEngineOpen(&params) == TRUE);
    CHECK(RwEngineInstance->stringFuncs.vecStrlen("abcd") == 4);

    const int numSS = RwEngineGetNumSubSystems();
    CHECK(numSS >= 1);
    for (int i = 0; i < numSS; i++) {
        RwSubSystemInfo ssi{};
        CHECK(RwEngineGetSubSystemInfo(&ssi, i) == &ssi);
        std::printf("subsystem %d: %s%s\n", i, ssi.name, i == RwEngineGetCurrentSubSystem() ? " (current)" : "");
    }
    CHECK(RwEngineGetSubSystemInfo(nullptr, 0) == nullptr);
    CHECK(RwEngineSetSubSystem(numSS) == FALSE);
    CHECK(RwEngineSetSubSystem(RwEngineGetCurrentSubSystem()) == TRUE);

    const int numVM = RwEngineGetNumVideoModes();
    CHECK(numVM >= 1);
    for (int i = 0; i < numVM && i < 12; i++) {
        RwVideoMode vm{};
        CHECK(RwEngineGetVideoModeInfo(&vm, i) == &vm);
        std::printf("videomode %d: %d x %d x %d  %d Hz  fmt 0x%04x  %s\n", i, vm.width, vm.height, vm.depth, vm.refRate, vm.format,
                    (vm.flags & rwVIDEOMODEEXCLUSIVE) ? "exclusive" : "windowed");
    }
    // 01r: the exe lists R5G6B5 (16 bit) modes first, then X8R8G8B8 / A2R10G10B10 (32 bit); mode 0 is the desktop as a windowed mode; entries are unique
    // per (width, height, format) and the raster format follows the exe's table (565 -> 0x0200, 888 -> 0x0600)
    {
        bool seen32 = false, order16After32 = false, dup = false, rasterOk = true;
        for (int i = 1; i < numVM; i++) {
            RwVideoMode vm{};
            RwEngineGetVideoModeInfo(&vm, i);
            if (vm.depth == 32) { seen32 = true; }
            if (vm.depth == 16 && seen32) { order16After32 = true; }
            if (vm.depth == 16 && vm.format != rwRASTERFORMAT565) { rasterOk = false; }
            for (int j = 1; j < i; j++) {
                RwVideoMode o{};
                RwEngineGetVideoModeInfo(&o, j);
                if (o.width == vm.width && o.height == vm.height && o.depth == vm.depth && o.format == vm.format) { dup = true; }
            }
            CHECK((vm.flags & rwVIDEOMODEEXCLUSIVE) != 0);
        }
        CHECK(!order16After32);
        CHECK(!dup);
        CHECK(rasterOk);
        RwVideoMode m0{};
        RwEngineGetVideoModeInfo(&m0, 0);
        CHECK((m0.flags & rwVIDEOMODEEXCLUSIVE) == 0);
    }
    RwVideoMode bad{};
    CHECK(RwEngineGetVideoModeInfo(&bad, numVM) == nullptr);
    CHECK(RwEngineGetCurrentVideoMode() == 0);
    CHECK(RwEngineSetVideoMode(0) == TRUE);                         // windowed desktop mode
    CHECK(RwEngineSetVideoMode(numVM) == FALSE);
    CHECK(RwD3D9GetCaps() != nullptr);
    std::printf("version 0x%X, DXT %d, max MSAA %u\n", RwEngineGetVersion(), RwD3D9DeviceSupportsDXTTexture(), RwD3D9EngineGetMaxMultiSamplingLevels());
    RwD3D9EngineSetMultiSamplingLevels(4);                          // selected before the start: the device is reset into a 4x back buffer by RwEngineStart (checked below)

    CHECK(RwEngineStart() == TRUE);
    CHECK(RwD3D9GetCurrentD3DDevice() != nullptr);
    {
        auto* dev0 = static_cast<IDirect3DDevice9*>(RwD3D9GetCurrentD3DDevice());
        IDirect3DSwapChain9* sc0 = nullptr;
        D3DPRESENT_PARAMETERS pp0{};
        if (SUCCEEDED(dev0->GetSwapChain(0, &sc0)) && sc0) { sc0->GetPresentParameters(&pp0); sc0->Release(); }
        std::printf("live multisample type after start with level 4 selected: %d\n", (int)pp0.MultiSampleType);
        CHECK(RwD3D9EngineGetMaxMultiSamplingLevels() < 4 || pp0.MultiSampleType == D3DMULTISAMPLE_4_SAMPLES);
    }
    CHECK(RwEngineInstance->dOpenDevice.zBufferFar > RwEngineInstance->dOpenDevice.zBufferNear);
    CHECK(RwEngineGetNumVideoModes() == numVM);                     // still readable while started
    CHECK(RwEngineSetVideoMode(0) == FALSE);                        // only before Start
    CHECK(RwD3D9ChangeVideoMode(0) == TRUE);                        // in-place device reset
    CHECK(RwD3D9ChangeMultiSamplingLevels(4) == TRUE);              // MSAA on, device reset
    CHECK(RwD3D9ChangeMultiSamplingLevels(1) == TRUE);

    // PLAYTEST-1 U17: the menu offers 1..min(4, max); every value must leave a healthy device and a swap chain whose multisample type is the exe's
    // choice: min(request, max) stepped down to the first plain type the runtime accepts for the display format (3 samples -> 2 on NVIDIA/AMD)
    {
        auto* dev = static_cast<IDirect3DDevice9*>(RwD3D9GetCurrentD3DDevice());
        IDirect3D9* d3d = nullptr;
        CHECK(dev && SUCCEEDED(dev->GetDirect3D(&d3d)) && d3d);
        D3DDISPLAYMODE dm{};
        D3DDEVICE_CREATION_PARAMETERS cp{};
        dev->GetCreationParameters(&cp);
        d3d->GetAdapterDisplayMode(cp.AdapterOrdinal, &dm);
        const auto maxL = RwD3D9EngineGetMaxMultiSamplingLevels();
        auto liveType = [&]() {
            IDirect3DSwapChain9* sc = nullptr;
            D3DPRESENT_PARAMETERS pp{};
            if (SUCCEEDED(dev->GetSwapChain(0, &sc)) && sc) { sc->GetPresentParameters(&pp); sc->Release(); }
            return pp;
        };
        const RwUInt32 reqs[] = {2u, 3u, 4u, 5u, 99u, 1u, 0u, 4u};
        for (RwUInt32 req : reqs) {
            std::printf("MSAA request %u (max %u)\n", req, maxL);
            CHECK(RwD3D9ChangeMultiSamplingLevels(req) == TRUE);
            CHECK(dev->TestCooperativeLevel() == D3D_OK);
            RwUInt32 want = req > maxL ? maxL : req;
            while (want >= 2 && FAILED(d3d->CheckDeviceMultiSampleType(cp.AdapterOrdinal, D3DDEVTYPE_HAL, dm.Format, TRUE, (D3DMULTISAMPLE_TYPE)want, nullptr))) { --want; }
            if (want < 2) { want = 0; }
            const auto pp = liveType();
            std::printf("  live multisample type %d quality %u (expected type %u)\n", (int)pp.MultiSampleType, (unsigned)pp.MultiSampleQuality, want);
            if (want) {
                CHECK((unsigned)pp.MultiSampleType == want);
            } else {
                CHECK(pp.MultiSampleType == D3DMULTISAMPLE_NONE);
            }
            CHECK(SUCCEEDED(dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0)));
            CHECK(SUCCEEDED(dev->Present(nullptr, nullptr, nullptr, nullptr)));
        }
        d3d->Release();
    }

    CHECK(RwEngineClose() == FALSE);                                // still started
    CHECK(RwEngineStop() == TRUE);
    CHECK(RwEngineTerm() == FALSE);                                 // still open
    CHECK(RwEngineClose() == TRUE);
    CHECK(RwEngineTerm() == TRUE && !RwInitialized);
    CHECK(RwEngineInit(nullptr, 0, 0) == TRUE && RwEngineTerm() == TRUE); // re-init cycle
    DestroyWindow(wnd);
    std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "PASSED", g_fail);
    return g_fail;
}
