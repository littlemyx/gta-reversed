/*
* This file contains WinMain and related functions
*/

#include "StdInc.h"

#ifdef NOTSA_USE_SDL3
#include <SDL3/SDL.h>
#include "SDLWrapper.hpp"
#endif

#include "LoadingScreen.h"
#ifdef NOTSA_STATEHASH
#include "portable/statehash.h"
#endif
#include "ControllerConfigManager.h"
#include "Gamma.h"

#include "VideoPlayer.h"
#include "VideoMode.h"
#include "WinPlatform.h"

#include "extensions/Configs/FastLoader.hpp"
#ifdef NOTSA_PORTABLE
#include <cfenv>
#include "portable/app_platform.h"
#include "portable/app_window.h"
#include "portable/app_pad.h"
#endif

#ifndef NOTSA_USE_SDL3
#include "WinInput.h"
#include "WndProc.h"
#endif

#ifdef NOTSA_STANDALONE
#include <InjectHooksMain.h>
#include <extensions/CommandLine.h>
#include <extensions/debug.hpp>
#ifdef NOTSA_STANDALONE_RUN
#include <standalone/ImageDiag.h>
#include <standalone/Fixups.h>
#endif
#endif

#include <toolsmenu/UIRenderer.h>
#ifdef NOTSA_STANDALONE_RUN
#include <float.h>
#endif

constexpr auto NO_FOREGROUND_PAUSE = true;

// 0x747300
char* getDvdGamePath() {
#ifdef NOTSA_PORTABLE
    return notsa::portable::FindDvdGamePath(); // /Volumes/GTA_SAN_ANDREAS/ if such a volume is mounted
#else
    SetErrorMode(SEM_FAILCRITICALERRORS);

    size_t bufferSize = GetLogicalDriveStringsA(0, nullptr);
    const auto drivesBuffer = new char[bufferSize];

    GetLogicalDriveStringsA(bufferSize, drivesBuffer);
    assert(bufferSize > 0);

    char drivePath[8] = {};
    char volumeName[MAX_PATH + 1] = {};

    for (const auto* drive = drivesBuffer; *drive; drive += strlen(drive) + 1) {
        strcpy_s(drivePath, sizeof(drivePath), drive);

        if (GetDriveTypeA(drivePath) != DRIVE_CDROM) {
            continue;
        }

        if (!GetVolumeInformationA(drivePath, volumeName, sizeof(volumeName), nullptr, nullptr, nullptr, nullptr, 0)) {
            continue;
        }

        if (strcmp(volumeName, "GTA_SAN_ANDREAS") == 0) {
            const auto result = new char[strlen(drivePath) + 1];
            strcpy_s(result, strlen(drivePath) + 1, drivePath);
            delete[] drivesBuffer;
            return result;
        }
    }

    delete[] drivesBuffer;
    return nullptr;
#endif
}

// 0x746870
void MessageLoop() {
#ifdef NOTSA_USE_SDL3
    notsa::SDLWrapper::ProcessEvents();
#else
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE | PM_NOYIELD)) {
        if (msg.message == WM_QUIT) {
            RsGlobal.quit = true;
        } else {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
#endif
}

#ifndef NOTSA_USE_SDL3
// 0x7486A0
bool Win32_InitApplication(HINSTANCE hInstance) {
    WNDCLASS windowClass      = { 0 };
    windowClass.style         = CS_BYTEALIGNWINDOW;
    windowClass.lpfnWndProc   = __MainWndProc;
    windowClass.hInstance     = hInstance;
    windowClass.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    windowClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = APP_CLASS;
    return RegisterClass(&windowClass);
}

// 0x745560
auto Win32_InitInstance(HINSTANCE hInstance) {
    RECT rect = { 0, 0, RsGlobal.maximumWidth, RsGlobal.maximumHeight };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);

    return CreateWindowEx(
        0,
        APP_CLASS,
        RsGlobal.appName,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );
}
#endif

// 0x7468E0
bool IsAlreadyRunning() {
#ifdef NOTSA_PORTABLE
    return !notsa::portable::AcquireSingleInstance();
#else
    CreateEvent(nullptr, false, true, APP_CLASS);
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }

    const auto fgwnd = FindWindow(APP_CLASS, RsGlobal.appName);
    SetForegroundWindow(fgwnd != INVALID_HANDLE_VALUE ? fgwnd : PSGLOBAL(window));
    
    return true;
#endif
};

// 0x746060
bool IsForegroundApp() {
    return ForegroundApp;
}

// Code from winmain, 0x748DCF
bool ProcessGameLogic(INT nCmdShow) {
    ZoneScoped;
    if (RsGlobal.quit || FrontEndMenuManager.m_bStartGameLoading) {
        return false;
    }

#ifdef NOTSA_USE_SDL3
    notsa::SDLWrapper::ProcessEvents();
#else
    // Process Window messages
    if (MSG msg; PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE | PM_NOYIELD)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        return true;
    }
#endif

    // Game is in background
    if (!NO_FOREGROUND_PAUSE && !ForegroundApp) {
        if (isForeground) {
            isForeground = false;
        }
        Sleep(100);
        return true;
    }

    // TODO: Move this out from here (It's not platform specific at all)
    switch (gGameState) {
    case GAME_STATE_INITIAL: {
        ZoneScopedN("INITIAL");

        const auto ProcessSplash = [](eLoadingLogo id) {
            CLoadingScreen::LoadSplashes(true, id);
            CLoadingScreen::Init(true, true);
            CLoadingScreen::DoPCTitleFadeIn();
            CLoadingScreen::DoPCTitleFadeOut();
            CLoadingScreen::Shutdown();
        };
        if (!g_FastLoaderConfig.NoEAX) {
            ProcessSplash(eLoadingLogo::EAX);
        }
        if (!g_FastLoaderConfig.NoNVidia) {
            ProcessSplash(eLoadingLogo::NVIDIA);
        }
        ChangeGameStateTo(GAME_STATE_LOGO);
        break;
    }
    case GAME_STATE_LOGO: {
        ZoneScopedN("LOGO");

        if (!g_FastLoaderConfig.NoLogo) {
            if (!Windowed) {
                VideoPlayer::Play(nCmdShow, "movies\\Logo.mpg");
            }
        }
        ChangeGameStateTo(g_FastLoaderConfig.NoLogo ? GAME_STATE_TITLE : GAME_STATE_PLAYING_LOGO);
        break;
    }
    case GAME_STATE_PLAYING_LOGO:
    case GAME_STATE_PLAYING_INTRO: { // 0x748B17
        ZoneScopedN("PLAYING_LOGO & PLAYING_INTRO");

        CPad::UpdatePads();
        auto* pad = CPad::GetPad();
#ifdef NOTSA_STANDALONE_RUN
        // No DirectShow under Wine: VideoPlayer::Play fails and nothing ends the "playing" state (the original needs a key press then). Test hook:
        static const bool s_SkipVideos = std::getenv("NOTSA_STANDALONE_SKIP_VIDEOS") != nullptr;
#else
        constexpr bool s_SkipVideos = false;
#endif
        if (   Windowed
            || s_SkipVideos
            || ControlsManager.GetJoyButtonJustDown()
            || pad->NewState.CheckForInput()
            || CPad::IsMouseLButtonPressed()
            || CPad::IsEnterJustPressed()
            || pad->IsStandardKeyJustPressed(VK_SPACE)
            || CPad::IsMenuKeyJustPressed()
            || CPad::IsTabJustPressed()
        ) {
            ChangeGameStateTo([] {
                switch (gGameState) {
                case GAME_STATE_PLAYING_LOGO:  return GAME_STATE_TITLE;
                case GAME_STATE_PLAYING_INTRO: return GAME_STATE_FRONTEND_LOADING;
                default:                       NOTSA_UNREACHABLE_RET(GAME_STATE_TITLE);
                }
                return GAME_STATE_TITLE; // run build: NOTSA_UNREACHABLE only logs and returns (C4715); dead code where it is __assume(false)
            }());
        }
        break;
    }
    case GAME_STATE_TITLE: {
        ZoneScopedN("TITLE");

        if (!g_FastLoaderConfig.NoTitleOrIntro) {
            VideoPlayer::Shutdown();
            VideoPlayer::Play(nCmdShow, FrontEndMenuManager.GetMovieFileName());
        }
        ChangeGameStateTo(g_FastLoaderConfig.NoTitleOrIntro ? GAME_STATE_FRONTEND_LOADING : GAME_STATE_PLAYING_INTRO);
        break;
    }
    case GAME_STATE_FRONTEND_LOADING: {
        ZoneScopedN("FRONTEND_LOADING");

        VideoPlayer::Shutdown();
        CLoadingScreen::Init(true, false);
        if (!g_FastLoaderConfig.NoCopyright) {
            CLoadingScreen::DoPCTitleFadeIn();
        }
        if (!CGame::InitialiseEssentialsAfterRW()) {
            RsGlobal.quit = true;
        }
        CGame::InitialiseCoreDataAfterRW();
        ChangeGameStateTo(GAME_STATE_FRONTEND_LOADED);
#ifdef NOTSA_PORTABLE
        notsa::portable::Unsupported("GPU capability query (anisotropic filtering assumed supported: GL3 core on macOS has EXT_texture_filter_anisotropic)"); // W-A4
        anisotropySupportedByGFX = true;
#else
        anisotropySupportedByGFX = (((const D3DCAPS9*)RwD3D9GetCaps())->RasterCaps & D3DPRASTERCAPS_ANISOTROPY) != 0; // todo: func
#endif

#ifdef NOTSA_WINDOWED_MODE
        RwCameraClear(Scene.m_pRwCamera, &gColourTop, rwCAMERACLEARZ);
#endif

        break;
    }
    case GAME_STATE_FRONTEND_LOADED: {
        ZoneScopedN("FRONTEND_LOADED");

        FrontEndMenuManager.m_bActivateMenuNextFrame = true;
        FrontEndMenuManager.m_bMainMenuSwitch = true;
        if (IsVMNotSelected) {
            FrontEndMenuManager.m_nPrefsVideoMode = FrontEndMenuManager.m_nDisplayVideoMode = gCurrentVideoMode;
            IsVMNotSelected = false;
        }
        ChangeGameStateTo(GAME_STATE_FRONTEND_IDLE);
        if (g_FastLoaderConfig.NoCopyright) {
            CLoadingScreen::SkipCopyrightSplash();
        } else {
            CLoadingScreen::DoPCTitleFadeOut();
        }
        break;
    }
    case GAME_STATE_FRONTEND_IDLE: { // 0x748CB2
        ZoneScopedN("FRONTEND_IDLE");

#ifdef NOTSA_PORTABLE
        const struct { int showCmd; } wndpl{ (SDL_GetWindowFlags(PSGLOBAL(sdlWindow)) & SDL_WINDOW_MINIMIZED) ? SW_SHOWMINIMIZED : SW_SHOWNORMAL };
#else
        WINDOWPLACEMENT wndpl{ .length = sizeof(WINDOWPLACEMENT) };
        VERIFY(GetWindowPlacement(PSGLOBAL(window), &wndpl));
#endif
        if (g_FastLoaderConfig.ShouldLoadSaveGame()) {
            RsEventHandler(rsFRONTENDIDLE, nullptr); // We need to still run the frontend processing once because it has some important stuff
            if ((GetAsyncKeyState(g_FastLoaderConfig.SkipSaveGameLoadKey) & 0xF000) == 0) {
                g_FastLoaderConfig.StartGame(g_FastLoaderConfig.SaveGameToLoad); // Load game
            }
            g_FastLoaderConfig.TriedLoadingSaveGame = true;
        } else if (g_FastLoaderConfig.RenderAtAllTimes || wndpl.showCmd != SW_SHOWMINIMIZED) {
            RsEventHandler(rsFRONTENDIDLE, nullptr);
        }
        if (FrontEndMenuManager.m_bMenuActive && !FrontEndMenuManager.m_bLoadingData) {
            break;
        }
        ChangeGameStateTo(GAME_STATE_LOADING_STARTED);
        if (!FrontEndMenuManager.m_bLoadingData) {
            break;
        }
        NOTSA_SWCFALLTHRU; // Fall down and start loading
    }
    case GAME_STATE_LOADING_STARTED: {
        ZoneScopedN("LOADING_STARTED");

        if (!g_FastLoaderConfig.NoLoadingTune) {
            AudioEngine.StartLoadingTune();
        }

        InitialiseGame();
        ChangeGameStateTo(GAME_STATE_IDLE);
        FrontEndMenuManager.m_bMainMenuSwitch = false;

        AudioEngine.InitialisePostLoading();

        break;
    }
    case GAME_STATE_IDLE: { // 0x748AA1
        ZoneScopedN("IDLE");

        if (!RwInitialized) {
            break;
        }

        auto v9_1 = 1000.0f / (float)RsGlobal.frameLimit;
        auto v9_2 = (float)CTimer::GetCurrentTimeInCycles() / (float)CTimer::GetCyclesPerMillisecond();
#ifdef NOTSA_STATEHASH
        if (notsa::rr::Active()) { // RR: the frame-stepped virtual clock only moves inside Idle, so the frame-limiter wait below would never end; every iteration is a frame
            RsEventHandler(rsIDLE, (void*)true);
            break;
        }
#endif
        if (!FrontEndMenuManager.m_bPrefsFrameLimiter && CReplay::Mode != eReplayMode::MODE_PLAYBACK && !AudioEngine.IsBeatInfoPresent() || v9_1 < v9_2) {
            RsEventHandler(rsIDLE, (void*)true);
        }
        break;
    }
    }

    if (!isForeground) {
        isForeground = true;
    }

    return true;
}

// Code from winmain, 0x7489FB
void MainLoop(INT nCmdShow) {
    bool isNewGameFirstTime = true;
    while (true) {
        RwInitialized = true;

        // Useless, and annoying
        //RwV2d pos{ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f };
        //RsMouseSetPos(&pos);

        gamma.Init();

        // Game logic main loop
        while (ProcessGameLogic(nCmdShow)) {
            FrameMark;
        }

        // 0x748DDA
        RwInitialized = false;
        FrontEndMenuManager.UnloadTextures();
        if (!FrontEndMenuManager.m_bStartGameLoading) {
            break;
        }

        // load game
        CCheat::ResetCheats();
        CTimer::Stop();

        if (FrontEndMenuManager.m_bLoadingData) {
            CGame::ShutDownForRestart();
            CGame::InitialiseWhenRestarting();
            FrontEndMenuManager.m_bLoadingData = false;
        } else if (isNewGameFirstTime) {
            CTimer::Stop();
            ChangeGameStateTo(
                FrontEndMenuManager.m_nGameState != 1
                    ? GAME_STATE_LOADING_STARTED
                    : GAME_STATE_FRONTEND_LOADED
            );
        } else {
            CCheat::ResetCheats();
            CGame::ShutDownForRestart();
            CTimer::Stop();
            CGame::InitialiseWhenRestarting();
        }

        isNewGameFirstTime = false;
        FrontEndMenuManager.m_nGameState = 0;
        FrontEndMenuManager.m_bStartGameLoading = false;
    }

}

// 0x748710
INT WINAPI NOTSA_WinMain(HINSTANCE instance, HINSTANCE hPrevInstance, LPSTR cmdLine, INT nCmdShow) {
#ifndef NOTSA_PORTABLE
    SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0u, nullptr, 2);
#endif
    if (IsAlreadyRunning()) {
        return false;
    }

    if (rsEVENTERROR == RsEventHandler(rsINITIALIZE, nullptr)) {
        return false;
    }

#ifdef NOTSA_USE_SDL3
    if (!SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC)) {
        NOTSA_UNREACHABLE("Failed to initialize SDL: {}", SDL_GetError());
        return -1;
    }
#else
    if (!Win32_InitApplication(instance)) {
        return false;
    }
#endif

    char** argv = __argv;
    int    argc = __argc;
    for (int i = 0; i < argc; i++) {
        RsEventHandler(rsPREINITCOMMANDLINE, argv[i]);
    }

    // Dirty fix for crash in crt when trying to free cmd line args
    // Yeah we leak memory, but who cares
    // Crash was in `uninitialize_allocated_io_buffers` (`C:\Program Files (x86)\Windows Kits\10\Source\<Win 10 SDK Version>\ucrt\internal\initialization.cpp`)
#ifndef NOTSA_PORTABLE
    __argc  = 0;
    __argv  = nullptr;
    __wargv = nullptr;
#endif

#ifdef NOTSA_USE_SDL3
    SDL_Window* sdlWnd = SDL_CreateWindow(
        APP_CLASS,
        APP_DEFAULT_WIDTH, APP_DEFAULT_HEIGHT,
        SDL_WINDOW_RESIZABLE  //| SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS// | SDL_WINDOW_MOUSE_GRABBED | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS
#ifdef NOTSA_PORTABLE
        | SDL_WINDOW_OPENGL // R-3a: librw's gl3 backend adopts this window (gl3device.cpp startSDL3: GL context with depth 24 / stencil 8 on it)
        | notsa::portable::HiDpiWindowFlag() // MAC-W2: drawable in pixels (NOTSA_STANDALONE_HIDPI=0 keeps the 1x backing store)
#endif
    );
    PSGLOBAL(sdlWindow) = sdlWnd;
#ifdef NOTSA_PORTABLE
    if (sdlWnd) {
        notsa::portable::OnWindowCreated(sdlWnd); // MAC-W2: log sizes, hide the system cursor, windowed default size in points
    }
#endif
#ifdef NOTSA_PORTABLE
    PSGLOBAL(window) = (HWND)sdlWnd; // no Win32 window: the HWND slot carries the SDL_Window* (portable/app_win32_compat.h)
#else
    PSGLOBAL(window) = (HWND)(SDL_GetPointerProperty(SDL_GetWindowProperties(sdlWnd), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL)); // NOTE/TODO: Hacky, but required due to RW
#endif
#else
    PSGLOBAL(window) = Win32_InitInstance(instance);
#endif

    if (!PSGLOBAL(window)) {
        return false;
    }
    PSGLOBAL(instance) = instance; // Not used anywhere, just set here

    // 0x7487CF
#ifndef NOTSA_USE_SDL3
    VERIFY(WinInput::Initialise());
#endif
#ifdef NOTSA_PORTABLE
    notsa::portable::PadInit(); // MAC-W2: connected gamepads -> AllValidWinJoys (ReinitControls adds the default joypad bindings when pad 1 is attached)
#endif
    ControlsManager.ReinitControls();

    // 0x748847
    if (RsEventHandler(rsRWINITIALIZE, PSGLOBAL(window)) == rsEVENTERROR) {
#ifdef NOTSA_PORTABLE
        SDL_DestroyWindow(PSGLOBAL(sdlWindow));
#else
        DestroyWindow(PSGLOBAL(window));
#endif
        RsEventHandler(rsTERMINATE, nullptr);
        return false;
    }

    // 0x7488EE
    for (auto i = 0; i < argc; i++) {
        RsEventHandler(rsCOMMANDLINE, argv[i]);
    }

#ifndef NOTSA_USE_SDL3
#ifdef NOTSA_STANDALONE_RUN
    // NOTSA_STANDALONE_WINDOWED: keep the captioned window the video mode set up (engine.cpp WindowedEmuPlaceWindow), the client area is the back buffer
    if (!NotsaRwWindowedRun() && (MultipleSubSystems || PSGLOBAL(fullScreen))) {
#else
    if (MultipleSubSystems || PSGLOBAL(fullScreen)) {
#endif
        SetWindowLongPtr(PSGLOBAL(window), GWL_STYLE, (LONG_PTR)WS_POPUP);
        SetWindowPos(PSGLOBAL(window), nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
#endif

    RwRect rect{ 0, 0, RsGlobal.maximumWidth, RsGlobal.maximumHeight };
    RsEventHandler(rsCAMERASIZE, &rect);

#ifdef NOTSA_PORTABLE
    SDL_DisableScreenSaver(); // = SPI_SETSCREENSAVEACTIVE 0; the power-off / low-power timeouts and the sticky-keys accessibility flag are Windows-only and not touched
#else
    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 0u, nullptr, 2u);
    SystemParametersInfo(SPI_SETPOWEROFFACTIVE, 0u, nullptr, 2u);
    SystemParametersInfo(SPI_SETLOWPOWERACTIVE, 0u, nullptr, 2u);
    STICKYKEYS pvParam { .cbSize = sizeof(STICKYKEYS) };
    SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &pvParam, 2u);
    STICKYKEYS pvParam1 = { .cbSize = sizeof(STICKYKEYS), .dwFlags = SKF_TWOKEYSOFF };
    SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &pvParam1, 2u);

    UpdateWindow(PSGLOBAL(window));
#endif

#ifdef NOTSA_USE_SDL3
    SDL_SetWindowFocusable(sdlWnd, true);
    SDL_SetWindowPosition(sdlWnd, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_RaiseWindow(sdlWnd);
#else
#ifdef NOTSA_STANDALONE_RUN
    // NOTSA_STANDALONE_WINDOWED: the user works on this PC - do not steal the focus (NOTSA_STANDALONE_WINDOWED_FOCUS=1: activate as usual, e.g. for SendInput drivers)
    static const bool s_WindowedFocus = std::getenv("NOTSA_STANDALONE_WINDOWED_FOCUS") != nullptr && std::getenv("NOTSA_STANDALONE_WINDOWED_FOCUS")[0] == '1';
    ShowWindow(PSGLOBAL(window), NotsaRwWindowedRun() && !s_WindowedFocus ? SW_SHOWNOACTIVATE : nCmdShow);
#else
    ShowWindow(PSGLOBAL(window), nCmdShow);
#endif
#endif

    // 0x748995
    CFileMgr::SetDirMyDocuments();
    if (auto* file = CFileMgr::OpenFile("gta_sa.set", "rb")) {
        if (!ControlsManager.LoadSettings(file)) {
            ControlsManager.ReinitControls();
        }
        CFileMgr::CloseFile(file);
    }
    CFileMgr::SetDir("");

#ifndef NOTSA_PORTABLE
    SetErrorMode(SEM_FAILCRITICALERRORS);
#endif

    notsa::ui::UIRenderer::CreateInstance();

    // 0x7489FB
    MainLoop(nCmdShow);

    notsa::ui::UIRenderer::DestroyInstance();

    // if game is loaded, shut it down
    if (gGameState == GAME_STATE_IDLE) {
        CGame::Shutdown();
    }

    // now quit 0x748E75
    AudioEngine.Shutdown();
    FreeVideoModeList();
    RsEventHandler(rsRWTERMINATE, nullptr);
#ifdef NOTSA_PORTABLE
    notsa::portable::PadShutdown();
    SDL_DestroyWindow(PSGLOBAL(sdlWindow));
    RsEventHandler(rsTERMINATE, nullptr);
    // (argv is the process's own array here, not a CRT allocation: not freed)
    SDL_ShowCursor();

    SDL_EnableScreenSaver();
#else
    DestroyWindow(PSGLOBAL(window));
    RsEventHandler(rsTERMINATE, nullptr);
    free(argv);
    ShowCursor(true);

    SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &pvParam, 2u);
    SystemParametersInfo(SPI_SETPOWEROFFACTIVE, 1u, nullptr, 2u); // TODO: GUID_VIDEO_POWERDOWN_TIMEOUT
    SystemParametersInfo(SPI_SETLOWPOWERACTIVE, 1u, nullptr, 2u);
    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 1u, nullptr, 2u);
    // nullsub_0x72F3C0()
    SetErrorMode(0);
#endif

    return 0; // Msg.wParam
}

#ifdef NOTSA_STANDALONE
INT WINAPI WinMain(HINSTANCE instance, HINSTANCE hPrevInstance, LPSTR cmdLine, INT nCmdShow) {
    CommandLine::Load(__argc, __argv);
    if (CommandLine::s_WaitForDebugger) {
        notsa::debug::WaitForDebugger();
    }

    notsa::debug::DisplayConsole();
    notsa::debug::LoadSymbols();
    notsa::Logging::CreateInstance();
    notsa::ScopeGuard cleanupLogging{ [] {
        notsa::Logging::DestroyInstance();
        notsa::debug::UnloadSymbols();
    } };

#ifdef NOTSA_STANDALONE_DUMP_HOOKS_ONLY
    ReversibleHooks::RHManager::CreateInstance();
    notsa::ScopeGuard cleanupRH{ [] {
        ReversibleHooks::RHManager::DestroyInstance();
    }};

    NOTSA_LOG_INFO("Dumping hooks only, no memory writing will be performed");

    InjectHooksMain(); // This will also end up calling the CLI to dump the hooks

    return 0;
#else // NOTSA_STANDALONE_RUN (P2a, see .notes/P2A_DESIGN.md; F3: image-less)
    // F3: the run build has no original data image and no original code to patch (nothing is mapped at the original addresses, no original_data.* is read).
    notsa::standalone::Fixups::InstallDiagnostics();

    // D7: the original's CRT runs x87 libm (VS2005); the 32-bit UCRT would otherwise route sin/cos/pow/... to its SSE2 implementations on any SSE2 CPU
#ifndef NOTSA_PORTABLE
    _set_SSE2_enable(0);
#endif // (portable: arm64 has no x87/SSE2 libm split; bit-exact libm behaviour is stream F's)
    notsa::standalone::Fixups::LogFpuState("WinMain");
#ifdef NOTSA_PORTABLE
    std::feclearexcept(FE_ALL_EXCEPT); // start without sticky exception flags
#else
    _clearfp(); // the CRT's _FPinit ends with fnclex: start without sticky exception flags
#endif

    // The hook macros are no-ops here (reversiblehooks/NoHookRegistry.h): InjectHooksMain only runs the non-hook part (script command handlers, logger, CLI)
    ReversibleHooks::RHManager::CreateInstance();
    notsa::ScopeGuard cleanupRH{ [] {
        ReversibleHooks::RHManager::DestroyInstance();
    }};
    InjectHooksMain();

#ifdef NOTSA_PORTABLE
    void LoadConfigurations(); LoadConfigurations(); // gta-reversed.ini; dllmain.cpp (where the DLL build loads it) is not part of the portable target, defined at the end of this file
#endif
    NOTSA_VERIFY_GLOBALS_BOOT(); // A2: boot dump for tools/standalone/verify_globals.py (nothing unless NOTSA_VERIFY_GLOBALS)
    NOTSA_IMAGE_DIAG_ARM();      // A3: dev-only stray-access census / poison over the mapped original image (nothing unless GTASA_IMAGE_DIAG and the env variable is set)
    return NOTSA_WinMain(instance, hPrevInstance, cmdLine, nCmdShow);
#endif
}
#endif

void InjectWinMainStuff() {
    RH_ScopedCategory("Win");
    RH_ScopedNamespaceName("Win");

    RH_ScopedGlobalInstall(IsForegroundApp, 0x746060);
    RH_ScopedGlobalInstall(IsAlreadyRunning, 0x7468E0);

    // Unhooking these 2 after the game has started will do nothing
    RH_ScopedGlobalInstall(NOTSA_WinMain, 0x748710, {.Locked = true});
    RH_ScopedGlobalInstall(MessageLoop, 0x746870, {.Locked = true});

#ifndef NOTSA_USE_SDL3
    RH_ScopedGlobalInstall(Win32_InitApplication, 0x7486A0);
    RH_ScopedGlobalInstall(Win32_InitInstance, 0x745560, {.Locked = true});
#endif
}

#ifdef NOTSA_PORTABLE
// Portable build: the body of dllmain.cpp's LoadConfigurations() (same INI file, same configs); dllmain.cpp is excluded from the portable target.
// Placed at the end of the file so that no __LINE__ above it moves.
#include "extensions/Configs/Miscellaneous.hpp"
void LoadConfigurations() {
    g_ConfigurationMgr.Load("gta-reversed.ini");
    g_FastLoaderConfig.Load();
    g_MiscConfig.Load();
}
#endif
