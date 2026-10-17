#include "StdInc.h"

#include "VideoPlayer.h"

#include "dshow.h"
#pragma comment(lib, "strmiids.lib") // Class identifiers (CLSIDs) and interface identifiers (IIDs).

#ifdef NOTSA_USE_SDL3
#include <SDL3/SDL.h>
#endif
#ifdef NOTSA_STANDALONE_RUN
#include "Audio/Hardware/AEDirectSoundMute.h"
#endif

namespace VideoPlayer {

NOTSA_GLOBAL(pvGraphBuilder, 0xC920D4, (IGraphBuilder*), {});
NOTSA_GLOBAL(pvMediaControl, 0xC920D8, (IMediaControl*), {});
NOTSA_GLOBAL(pvMediaEvent, 0xC920DC, (IMediaEventEx*), {});
NOTSA_GLOBAL(pvVideoWindow, 0xC920E0, (IVideoWindow*), {});
NOTSA_GLOBAL(pvMediaSeeking, 0xC920E4, (IMediaSeeking*), {});

#ifdef NOTSA_USE_SDL3
//! NOTSA: with SDL the game window is an SDL window and the original `__MainWndProc` (WndProc.cpp) is never installed, so the WM_GRAPHNOTIFY that DirectShow posts to the
//! owner window (`IMediaEventEx::SetNotifyWindow` in Play) was swallowed by SDL's window procedure: EC_COMPLETE never reached `OnGraphNotify`, the state PLAYING_LOGO /
//! PLAYING_INTRO never ended by itself (the original advances to TITLE / FRONTEND_LOADING when the movie is over) and the game sat on the last/black frame until a key press.
//! SDL runs this hook for every Win32 message it pulls from the queue (SDL_PumpEvents on the game thread), before it dispatches the message: same thread, same time as the
//! original's window procedure case at 0x74842A.
static bool SDLCALL GraphNotifyHook(void* /*userdata*/, MSG* msg) {
    if (msg->message != WM_GRAPHNOTIFY || msg->hwnd != PSGLOBAL(window)) {
        return true;
    }
    switch (gGameState) { // 0x74842A
    case GAME_STATE_PLAYING_INTRO:
    case GAME_STATE_PLAYING_LOGO:
        if (pvMediaEvent) {
            OnGraphNotify();
        }
        break;
    }
    return false; // handled, SDL must not dispatch it
}
#endif

void InjectHooks() {
    RH_ScopedNamespace(VideoPlayer);
    RH_ScopedCategory("App");

    RH_ScopedInstall(Shutdown, 0x7467B0);
    RH_ScopedInstall(WaitState, 0x745F90);
    RH_ScopedInstall(OnGraphNotify, 0x745FB0);
    RH_ScopedInstall(UpdateWindow, 0x7466D0);
    RH_ScopedInstall(Release, 0x746740);
    RH_ScopedInstall(Play, 0x747660);
}

// 0x746740
void Release() {
#define RELEASE(p) if (p) { (p)->Release(); }  (p) = nullptr;

    RELEASE(pvMediaEvent);
    RELEASE(pvMediaSeeking);
    RELEASE(pvMediaControl);
    RELEASE(pvVideoWindow);
    RELEASE(pvGraphBuilder);

#undef RELEASE
}

// 0x7467B0
void Shutdown() {
    if (pvMediaControl) {
        pvMediaControl->Stop();
    }
    Release();
}

// 0x745F90
void WaitState(int32 state) {
    OAFilterState curState;
    do {
        pvMediaControl->GetState(10, &curState);
    }
    while (state != curState);
}

// 0x745FB0
void OnGraphNotify() {
    LONG code = 0;
    LONG_PTR param1 = 0, param2 = 0;

    while (!FAILED(pvMediaEvent->GetEvent(&code, &param1, &param2, 0))) {
        pvMediaEvent->FreeEventParams(code, param1, param2);
        if (code != EC_COMPLETE) {
            continue;
        }
        // Possibly advance game state
        ChangeGameStateTo([] {
            switch (gGameState) {
            case GAME_STATE_PLAYING_LOGO:  return GAME_STATE_TITLE;
            case GAME_STATE_PLAYING_INTRO: return GAME_STATE_FRONTEND_LOADING;
            }
            return (eGameState)gGameState; // No change
        }());
        pvMediaEvent->SetNotifyWindow(NULL, 0, 0);
    }
}

// 0x7466D0
void UpdateWindow() {
    RECT rect;
    GetClientRect(PSGLOBAL(window), &rect);
    HRESULT hr;

    if (FAILED(hr = pvVideoWindow->SetWindowPosition(rect.left, rect.top, rect.right, rect.bottom))) {
        NOTSA_LOG_DEBUG("FAILED(hr=0x{:x}) in pvVideoWindow->SetWindowPosition() failed/n", hr);
        return;
    }

    if (FAILED(hr = pvVideoWindow->put_MessageDrain((OAHWND) PSGLOBAL(window)))) {
        NOTSA_LOG_DEBUG("FAILED(hr=0x{:x}) in (pvVideoWindow->put_MessageDrain((OAHWND) PSGLOBAL(window)))\n", hr);
        return;
    }

    SetFocus(PSGLOBAL(window));
}

// 0x747660
void Play(int32 nCmdShow, const char* path) {
    UpdateWindow(PSGLOBAL(window));

#ifdef FIX_BUGS
    const auto size = MultiByteToWideChar(0, 0, path, -1, nullptr, 0);
    WCHAR* fileName = new WCHAR[size];
    MultiByteToWideChar(0, 0, path, -1, fileName, size);
#else
    // Buffer overflow may happen in MultiByteToWideChar.
    WCHAR fileName[256] = { 0 };
    MultiByteToWideChar(0, 0, path, -1, fileName, sizeof(fileName) - 1);
#endif
    HRESULT hr;

    if (FAILED(hr = CoInitialize(nullptr))) {
        NOTSA_LOG_DEBUG("FAILED(hr=0x{:x}) in CoInitialize(NULL)\n", hr);
        return;
    }

    if (FAILED(hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC, IID_IGraphBuilder, (void**)&pvGraphBuilder))) {
        NOTSA_LOG_DEBUG("FAILED(hr=0x{:x}) in CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC, IID_IGraphBuilder, (void **)&pvGraphBuilder)\n", hr);
        return;
    }

    if (FAILED(hr = pvGraphBuilder->RenderFile(&fileName[0], nullptr))) {
        NOTSA_LOG_DEBUG("FAILED(hr=0x{:x}) in pvGraphBuilder->RenderFile(&wFileName[0], NULL)\n", hr);
        return;
    }

    if (FAILED(hr = pvGraphBuilder->QueryInterface(IID_IMediaControl, (void**)&pvMediaControl))) {
        NOTSA_LOG_DEBUG("FAILED(hr=0x{:x}) in pvGraphBuilder->QueryInterface(IID_IMediaControl, (void **)&pvMediaControl)\n", hr);
        return;
    }

    if (FAILED(hr = pvGraphBuilder->QueryInterface(IID_IMediaEventEx, (void**)&pvMediaEvent))) {
        NOTSA_LOG_DEBUG("FAILED(hr=0x{:x}) in pvGraphBuilder->QueryInterface(IID_IMediaEventEx, (void **)&pvMediaEvent)\n", hr);
        return;
    }

    if (FAILED(hr = pvGraphBuilder->QueryInterface(IID_IMediaSeeking, (void**)&pvMediaSeeking))) {
        NOTSA_LOG_DEBUG("FAILED(hr=0x{:x}) in pvGraphBuilder->QueryInterface(IID_IMediaSeeking, (void **)&pMS)\n", hr);
        return;
    }

    if (FAILED(hr = pvGraphBuilder->QueryInterface(IID_IVideoWindow, (void**)&pvVideoWindow))) {
        NOTSA_LOG_DEBUG("FAILED(hr=0x{:x}) in pvGraphBuilder->QueryInterface(IID_IVideoWindow, (void **)&pvVideoWindow)\n", hr);
        return;
    }

    if (FAILED(hr = pvVideoWindow->put_Owner((OAHWND)PSGLOBAL(window)))) {
        NOTSA_LOG_DEBUG("FAILED(hr=0x{:x}) in pvVideoWindow->put_Owner((OAHWND) PSGLOBAL(window))\n", hr);
        return;
    }

    if (FAILED(hr = pvVideoWindow->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN))) {
        NOTSA_LOG_DEBUG("FAILED(hr=0x{:x}) in pvVideoWindow->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN)\n", hr);
        return;
    }

    if (FAILED(hr = pvMediaEvent->SetNotifyWindow((OAHWND)PSGLOBAL(window), WM_GRAPHNOTIFY, 0))) {
        NOTSA_LOG_DEBUG("FAILED(hr=0x{:x}) in pvMediaEvent->SetNotifyWindow((OAHWND)PSGLOBAL(window), WM_GRAPHNOTIFY, 0)\n", hr);
        return;
    }

#ifdef NOTSA_USE_SDL3
    SDL_SetWindowsMessageHook(GraphNotifyHook, nullptr); // see GraphNotifyHook
#endif

#ifdef NOTSA_STANDALONE_RUN
    // NOTSA (run build only): the DirectShow graph has its own audio renderer, which NOTSA_STANDALONE_MUTE (DirectSound buffers of the audio engine) does not reach.
    if (notsa::audio_mute::Enabled()) {
        IBasicAudio* audio = nullptr;
        if (SUCCEEDED(pvGraphBuilder->QueryInterface(IID_IBasicAudio, (void**)&audio)) && audio) {
            const auto hr = audio->put_Volume(-10000);
            notsa::standalone::Fixups::Log("[mute] NOTSA_STANDALONE_MUTE: DirectShow audio renderer volume -100 dB for '%s' (hr=0x%x)", path, (unsigned)hr);
            audio->Release();
        } else {
            notsa::standalone::Fixups::Log("[mute] NOTSA_STANDALONE_MUTE: '%s' has no IBasicAudio (no audio stream?)", path);
        }
    }
#endif

    UpdateWindow();

    if (FAILED(hr = pvMediaControl->Run())) {
        NOTSA_LOG_DEBUG("FAILED(hr=0x{:x}) in pvMediaControl->Run()\n", hr);
        return;
    }

    SetFocus(PSGLOBAL(window));

#ifdef FIX_BUGS
    delete[] fileName;
#endif
};

auto GetMediaControl() -> IMediaControl* {
    return pvMediaControl;
}

} // namespace VideoPlayer
