# WINDOWED - run build option NOTSA_STANDALONE_WINDOWED=<W>x<H> (Windows host, 2026-10-17)

Commits: 8a0e23a1 (engine.cpp/WinPs.cpp/WinMain.cpp/rwextra.h), d18807e9 (soak.sh, census.py, hostenv.sh), c5e13d67 (docs + deviations tsv). No librw change needed.

## Problem
Exclusive fullscreen game runs switch the desktop mode of the PC the user works on (seen 800x600 / 2560x1600 instead of 3840x2160) and leave it changed.

## Design (run build only, off by default; `#ifdef NOTSA_STANDALONE_RUN`)
* `source/standalone/rw/engine.cpp` `WindowedEmu`: after `BuildExeModeList` every exclusive librw mode loses `VIDEOMODEEXCLUSIVE` (so librw's `startD3D` creates a WINDOWED device whose back buffer is the window client area, and
  `ApplyModeToDevice`/`RwD3D9ChangeVideoMode` size the window to the mode instead of switching the display), while `RwEngineGetVideoModeInfo` still REPORTS them as exclusive: the game's `fullScreen`,
  `RsGlobal.maximumWidth/Height`, HUD scale and menu mode list are those of an ordinary exclusive W x H x 32 run. One synthetic W x H mode is appended at the END of the list (real mode indices, which gta_sa.set stores, do not move).
* `WinPs.cpp psSelectDevice` selects that mode (wins over the saved / default / searched mode). `WinMain.cpp`: the WS_POPUP restyle (`MultipleSubSystems || fullScreen`) is skipped, `ShowWindow` uses SW_SHOWNOACTIVATE
  (the user is working; `NOTSA_STANDALONE_WINDOWED_FOCUS=1` activates as usual, for SendInput drivers).
* Window: caption + sysmenu + minimise, no sizing border (fixed back buffer), client area exactly W x H (verified with GetClientRect, one DPI correction pass), centred on the primary work area, HWND_NOTOPMOST.
* Values: `WxH` (320x240..16384), `1`/`on` = 960x600, `0`/`off`/empty = off.
* Defaults: soak.sh (Windows host only, `960x600` = size the route's mouse coordinates assume; `${VAR-default}`, so empty/0 opts out) and census.py (calls soak.sh; also `setdefault`). Not set on the Wine hosts (their numbers stay comparable).
  hostenv.sh: `py` added as python candidate (python3/python are the Store stub here: the soak report step died with rc 49 before).
* NOT changed (WIN-PERF's untracked scripts, original exe has no windowed mode): fps_run.sh, startup_probe.py, perf_datadir.sh. WIN_FPS comparisons ours vs original must use the SAME mode: both exclusive (do not export the variable in that shell) or say so.
* Frame limiter / vsync: logic unchanged (librw IMMEDIATE, menu vsync toggles via Reset, software frame limiter). In a window Present goes through DWM: no tearing, vsync paced by the compositor; frame pacing differs slightly from exclusive.

## Verification
| check | result |
|---|---|
| StandaloneRelease build (-j4) | ok (rwextra.h touched -> full rebuild) |
| 3-min soak (route 393 s, idle 60 s), default windowed 960x600, GPU lock | RESULT OK, 0 errors, 21 markers menu..idle_end..quit_ask, 29-31 fps, screenshots 960x600 (HUD correct); process exited via atexit after the Return on QUIT (so `quit_sent` mark is not reached - first soak where the quit really works, BUG-QUIT) |
| desktop resolution (Win32_VideoController) before / after | 2560x1600 / 2560x1600 (D-B09 fullscreen runs had left 800x600 earlier) |
| 1280x720 boot to menu | log `window client 1280x720`, back-buffer screenshots 1280x720 |
| tools/standalone/run_all_tests.sh (StandaloneRW) | 42/42 ok, rc 0 (rw_engine_test, rw_platform_test and all rw_* included; engine.cpp changes are `NOTSA_STANDALONE_RUN`-only) |
Out dir of the soak: C:/gta-soak-out/windowed1 (report.txt, shots).

## Notes / not done
* Not tested: opt-out (`=0`, unchanged old path), user clicking another window mid-run (injected input is independent of focus; WM_KILLFOCUS pauses only the stream audio, WM_SETFOCUS would open the pause menu on a later refocus of the game window).
* `CMenuManager::CentreMousePointer` still `SetCursorPos`es the real cursor into the game window when the menu opens (same as before in fullscreen) - visible to the user once per menu open.
* A normal quit still writes gta_sa.set (video mode index = the one in prefs, the synthetic mode is not saved because prefs are left alone); soak.sh deletes the file as before.
