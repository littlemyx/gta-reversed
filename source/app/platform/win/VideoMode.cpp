#include "StdInc.h"

#include "VideoMode.h"
#include "WinPlatform.h"
#include "PostEffects.h"
#ifdef NOTSA_PORTABLE
#include <SDL3/SDL.h>
#include "portable/app_window.h"
#endif

// todo: "2014/12/07 [nick7]: is it related to streaming memory (45M default?)?"
#define GAME_FREE_VIDEO_MEM_REQUIRED (45 * 1024 * 1024)

#ifdef NOTSA_VIDEO_MODE_LOGS
    #define VIDEO_MODE_LOG(...) NOTSA_LOG_DEBUG(__VA_ARGS__)
#else
    #define VIDEO_MODE_LOG(...)
#endif

// 0x745AF0
char** GetVideoModeList() {
    if (gVideoModes) {
        if (gCurrentGpu == RwEngineGetCurrentSubSystem()) {
            return gVideoModes;
        }
        FreeVideoModeList();
    }
    gCurrentGpu = RwEngineGetCurrentSubSystem();

    uint32 numVidModes = RwEngineGetNumVideoModes();
    assert(numVidModes != -1 && "Failed to get Video Modes");

    gVideoModes = (char**)CMemoryMgr::Calloc(numVidModes, sizeof(char*));

    for (auto modeId = 0u; modeId < numVidModes; modeId++) {
        RwVideoMode vmi;
        RwEngineGetVideoModeInfo(&vmi, modeId);

        gVideoModes[modeId] = nullptr;
        if ((vmi.flags & rwVIDEOMODEEXCLUSIVE) == 0) {
            VIDEO_MODE_LOG("Unavailable video mode id={:02d}: {} X {} X {} [reason: video mode not exclusive]", modeId, videoMode.width, videoMode.height, videoMode.depth);
            continue;
        }

        if (vmi.width < APP_MINIMAL_WIDTH || vmi.height < APP_MINIMAL_HEIGHT) {
            VIDEO_MODE_LOG("Unavailable video mode id={:02d}: {} X {} X {} [reason: size]", modeId, videoMode.width, videoMode.height, videoMode.depth);
            continue;
        }

        float fRatio = float(vmi.height) / float(vmi.width);
        if (!IS_FULLSCREEN_RATIO(fRatio) && !IS_WIDESCREEN_RATIO(fRatio)) {
            VIDEO_MODE_LOG("Unavailable video mode id={:02d}: {} X {} X {} [reason: ratio {:0.2f}]", modeId, videoMode.width, videoMode.height, videoMode.depth, fRatio);
            continue;
        }

        if (vmi.width != APP_MINIMAL_WIDTH || vmi.height != APP_MINIMAL_HEIGHT) {
            if (s_OSStatus.VRAM.Avail - vmi.height * vmi.width * vmi.depth / 8 <= GAME_FREE_VIDEO_MEM_REQUIRED) {
                continue;
            }
        }

        gVideoModes[modeId] = (char*)CMemoryMgr::Calloc(100, sizeof(char));                                  // 100 chars
        sprintf_s(gVideoModes[modeId], 100 * sizeof(char), "%lu X %lu X %lu", vmi.width, vmi.height, vmi.depth); // rwsprintf

        VIDEO_MODE_LOG("Available video mode id={:02d}: {}", modeId, gVideoModes[modeId]);
    }

    return gVideoModes;
}

// 0x745A80
bool FreeVideoModeList() {
    auto numVidModes = RwEngineGetNumVideoModes();
    if (gVideoModes) {
        for (auto mode = 0; mode < numVidModes; mode++) {
            CMemoryMgr::Free(gVideoModes[mode]);
        }
        CMemoryMgr::Free(gVideoModes);
        gVideoModes = nullptr;
    }
    return true;
}

// 0x745C70
void SetVideoMode(int32 mode) {
    assert(mode < RwEngineGetNumVideoModes());

    VIDEO_MODE_LOG("Changing Video Mode to {} ({})", mode, gVideoModes ? gVideoModes[mode] : "unknown");

    RwD3D9ChangeVideoMode(mode);
#ifdef NOTSA_PORTABLE
    // MAC-W2: video modes are in drawable pixels; the engine sizes the window in points (HiDPI: twice too large). Re-size it so that pixels == the mode.
    if (PSGLOBAL(sdlWindow) && !(SDL_GetWindowFlags(PSGLOBAL(sdlWindow)) & SDL_WINDOW_FULLSCREEN)) {
        RwVideoMode vm{};
        if (RwEngineGetVideoModeInfo(&vm, mode) && !(vm.flags & rwVIDEOMODEEXCLUSIVE)) {
            SDL_SetWindowSize(PSGLOBAL(sdlWindow), notsa::portable::PointsFromPixels(vm.width), notsa::portable::PointsFromPixels(vm.height));
            SDL_SyncWindow(PSGLOBAL(sdlWindow));
        }
    }
#endif
#ifdef NOTSA_USE_SDL3
    // exe 0x7F8640 (RwD3D9ChangeVideoMode) resizes the window with SetWindowPos; the resulting WM_SIZE is SENT to the game's window procedure, i.e. it runs
    // synchronously inside the call: __MainWndProc 0x747F04 -> RsEventHandler(rsCAMERASIZE) -> CameraSize resizes the camera / z rasters to the new client size.
    // SDL owns the window procedure and only queues SDL_EVENT_WINDOW_RESIZED (nothing consumes it), so the camera raster was still the OLD size here, and
    // SetupBackBufferVertex below sized the post-effects front buffer for the previous mode (the pow2 of the old size: one mode step behind). The copy
    // RwRasterRenderFast(camera raster) into it then failed (D3DERR_INVALIDCALL) and the heat haze drew dark squares of an uninitialised texture (PLAYTEST-1 U2).
    if (RwInitialized) {
        RECT client{};
        if (GetClientRect(PSGLOBAL(window), &client) && client.right > 0 && client.bottom > 0) {
            RwRect wndsz{ .x = 0, .y = 0, .w = client.right, .h = client.bottom };
            RsEventHandler(rsCAMERASIZE, &wndsz);
        }
    }
#endif
    CPostEffects::SetupBackBufferVertex();
    RsGlobal.maximumWidth  = RwRasterGetWidth(RwCameraGetRaster(Scene.m_pRwCamera));
    RsGlobal.maximumHeight = RwRasterGetHeight(RwCameraGetRaster(Scene.m_pRwCamera));
}

// 0x745CA0
bool IsVideoModeExclusive() { // AKA isCurrentModeFullscreen
    RwVideoMode vmi;
    RwEngineGetVideoModeInfo(&vmi, gCurrentVideoMode);
    return vmi.flags & rwVIDEOMODEEXCLUSIVE;
}

void VideoModeInjectHooks() {
    RH_ScopedNamespaceName("VideoMode");
    RH_ScopedCategoryGlobal();

    RH_ScopedGlobalInstall(GetVideoModeList, 0x745AF0);
    RH_ScopedGlobalInstall(FreeVideoModeList, 0x745A80);
    RH_ScopedGlobalInstall(SetVideoMode, 0x745C70);
    RH_ScopedGlobalInstall(IsVideoModeExclusive, 0x745CA0);
}
