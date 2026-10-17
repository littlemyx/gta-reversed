# BUG-VIDEO - intro movies hang on a black frame (Windows host, 2026-10-17)

## Verdict
Root cause: **the DirectShow end-of-movie notification never reached the game.** With SDL the game window is an SDL window (`SDL_CreateWindow`, `PSGLOBAL(window)` = its HWND); the original window procedure `__MainWndProc` (WndProc.cpp, `case WM_GRAPHNOTIFY` = 0x74842A) is never installed, SDL's own window procedure swallows the `WM_GRAPHNOTIFY` (1037) that the graph posts to the owner window (`IMediaEventEx::SetNotifyWindow(window, WM_GRAPHNOTIFY)`, exe 0x747660 + 0x74778D). So `VideoPlayer::OnGraphNotify` (EC_COMPLETE -> `PLAYING_LOGO -> TITLE`, `PLAYING_INTRO -> FRONTEND_LOADING`) never ran: the movie played to the end and the game sat in `PLAYING_LOGO` on the last (black) frame, spinning in the message loop, until a key press. It was never DirectShow itself (it works natively; under Wine there was none, hence nobody saw it).
Fix (`source/app/platform/win/VideoPlayer/VideoPlayer.cpp`, `#ifdef NOTSA_USE_SDL3`): `SDL_SetWindowsMessageHook(GraphNotifyHook)` installed in `Play`; the hook runs on the game thread for every Win32 message SDL pulls from the queue and does what the original case does (`gGameState` PLAYING_INTRO/LOGO -> `OnGraphNotify()`), returning false (message consumed). Result: the intro plays Logo.mpg (15 s) then GTAtitles.mpg (88 s) and reaches the menu on exactly the original exe's timeline.

## Reproduction and stacks (before the fix)
Our exe, windowed 960x600 (`NOTSA_STANDALONE_WINDOWED`), `NOTSA_STANDALONE_MUTE=1` + the dsmute dsound proxy (the DirectShow audio renderer is NOT covered by the mute, see below), videos ON, cdb attached at 40 / 60 / 90 s (run `repro2`, matching pdb):
* `gstate` (0xC8D4C0) = **2 = GAME_STATE_PLAYING_LOGO** at 40, 60 and 90 s, graph pointers (0xC920D4..) live and constant. No log activity (nothing to log in that state).
* main thread: `MainLoop -> ProcessGameLogic -> notsa::SDLWrapper::ProcessEvents -> SDL_PollEvent -> SDL_PumpEventMaintenance -> SDL_UpdateJoysticks -> HIDAPI_UpdateDevices -> hid_read_timeout -> WaitForSingleObject` (the "1 busy core": the loop of the PLAYING state has no sleep, the original's PeekMessage loop does not either). Other threads idle (spdlog worker, thread-pool workers).
* Screenshots (window rect, `bv_shot.ps1`): the movie IS shown - Rockstar North/Games logo at 22 s, fading to black by 28 s (Logo.mpg is 15 s long, window phase 19 s after the splashes), then a black client area for ever. So not a rendering/z-order/owner problem; the video window child of the SDL window works (also in exclusive fullscreen, see below).
* The earlier first sample (`repro1`) used an exe whose pdb had been relinked meanwhile (bogus symbol names): ignore it.

## The original exe on this PC (`gta_sa_compact.exe`, silent: dsmute proxy + SFX/radio volume 0 in a prepared `gta_sa.set`, radio auto-select off, real movies, exclusive 800x600, no dpnhpast stub needed any more: the real `dpnhpast.dll` is in System32/SysWOW64 since 11:44)
`gstate` polled every 250 ms with ReadProcessMemory (`orig2`): 0->2 at 10.5 s, **2->4 at 26.2 s** (Logo 15 s done), **4->5 at 115.1 s** (GTAtitles 88 s done), 5->7 (frontend idle) at 117.0 s: both movies play to the end and the state machine advances by itself via WM_GRAPHNOTIFY, as the asm says. (Earlier sample run `orig1`: same, 40-126 s stuck in 4 only because the titles take 88 s.)

## After the fix (HEAD d10bbd28 + the fix, StandaloneRelease built in a private worktree C:/gta-scratch/wtv)
| run | what | gstate timeline (s) |
|---|---|---|
| fix1/fix2 | windowed 960x600, muted, videos on, 2 runs | 0->1 10.3, ->2 10.5, **2->3->4 25.9-26.2**, **4->5 114.9-115.2**, **5->7 116.0** (original 26.2 / 115.1 / 117.0); screenshots at 13 s (Rockstar Games logo), 40-100 s (GTAtitles frames) |
| fix3_skip | windowed, injected `key:space` at 14 s | 2->3 14.2, 3->5 14.5 (the injector holds the key 150 ms so the held key also skips the second movie; the check is "pad has input", same code as the original), 5->7 15.3 |
| fix4_excl / fix5 | **exclusive fullscreen** 800x600 (the user's play mode; run 45 s, desktop mode switched for that time) | 2->4 26.9 (EC_COMPLETE handled in exclusive mode too) |
| soak (3 min, `soak.sh`, windowed, keeps `SKIP_VIDEOS=1`) | HEAD+fix | RESULT OK, 22 markers, 0 errors, 0 game [error], quit via pause menu, atexit clean (25 fps in every phase = the ported 14 ms idle lock + limiter, not related) |
| play.cmd | copy in a data dir, `PLAY_MUTE=1 PLAY_WINDOW=960x600`, no `videos` argument | launcher banner "видеозаставки включены", state 2->3->4 at 5.9/6.2 s after attach |

## Silence (hard requirement for agent runs)
`NOTSA_STANDALONE_MUTE` only covers the audio engine's DirectSound buffers; the DirectShow graph has its own audio renderer, so the movie sound would have been audible. Deviation (run build only, `NOTSA_STANDALONE_RUN`, only with `NOTSA_STANDALONE_MUTE=1`): in `Play`, before `Run()`, `IBasicAudio::put_Volume(-10000)` on the graph; logged as `[mute] NOTSA_STANDALONE_MUTE: DirectShow audio renderer volume -100 dB for 'movies\Logo.mpg' (hr=0x0)` (also for GTAtitles; checked in `standalone.log`, the dsmute proxy was removed from the data dir for that run). Listed in `tools/standalone/standalone_deviations.tsv` and `docs/STANDALONE.md`. The real play build (MUTE off) does not touch the volume.

## Things checked and found NOT to be the cause
* z-order / owner / window styles: the video window (WS_CHILD of the SDL window, `put_MessageDrain`) shows the movie in windowed and plays/ends in exclusive mode.
* `Windowed` global (`if (!Windowed) Play`, `if (Windowed || ...) skip`): that is the exe's own "windowed" flag, not `NOTSA_STANDALONE_WINDOWED` (the emulation does not set it), so the windowed emu does not change the video path.
* the Play code itself: compared with the exe asm 0x747660 (UpdateWindow(hwnd), MultiByteToWideChar, CoInitialize, CoCreateInstance(CLSID_FilterGraph), vtbl+0x34 RenderFile, QI MediaControl/MediaEventEx/MediaSeeking/VideoWindow, put_Owner [+0x74], put_WindowStyle(0x46000000) [+0x24], SetNotifyWindow(window, 0x40D) [+0x34], 0x7466D0, Run [+0x1c], SetFocus): identical.
* Not ported / left as is: the `WM_ACTIVATEAPP` branch (pause the movie when the window is deactivated, resume on activation, 0x748087) is also dead with SDL (same cause); the movie keeps playing when the window loses focus. Cosmetic, not needed for the intro; can use the same hook if wanted.

## Files / commits
* worktree commits 1438666c (VideoPlayer.cpp), 16f9ee40 (play.cmd, docs/STANDALONE.md, standalone_deviations.tsv), cherry-picked onto reverse/interior_c.
* play.cmd: videos ON by default, `novideos` (or `PLAY_VIDEOS=0`) skips, `videos` accepted and ignored. `C:\gta-play`: play.cmd, README-ru.txt and the exe snapshot (+ original_data.bin/.json, data_pointers.bin from the same build) refreshed, GPU lock was free (not USER-PLAY) while copying; the old snapshot exe is `C:/gta-scratch/bv_play_old_gta_reversed.exe`.
* Tools (scratch, not committed): `C:/gta-scratch/bv_run.sh|bv_run2.sh|bv_run_orig2.sh|bv_poll2.ps1|bv_shot.ps1`.

## run_all_tests
`SKIP_BUILD=1 tools/standalone/run_all_tests.sh build/StandaloneRW` (private worktree build at HEAD + fix, Debug, `gta_reversed.exe` + all `*_test` targets built, `ninja -j4`, GPU lock held for the run): 41 tests, 0 failures, 0 mismatches, rc 0 (incl. all rw_* device tests on the RTX 4080).
The private worktree C:/gta-scratch/wtv was removed afterwards. Data dirs left for re-runs (hard links/junctions of the Steam folder, never rm -rf them): C:/gta-bv (our exe), C:/gta-bv-orig (original exe), C:/gta-bv-soak; outputs in C:/gta-bv-out.
