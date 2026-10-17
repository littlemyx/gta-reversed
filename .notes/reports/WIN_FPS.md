# WIN_FPS - native frame times, original exe vs ours (Windows host, 2026-10-17, agent WIN-PERF)

Host: Ryzen 7 7700X, RTX 4080 (driver 32.0.15.1714), Windows 11, desktop 2560x1600@60. Ours = `build/StandaloneRelease` (tip 40c660d2, MSVC x86), original = repo-root `gta_sa_compact.exe` (v1.0 compact, copied as `gta_sa.exe` into a private data dir `C:\gta-perf\orig`; never the Steam v3.0 exe).
Both run the same route in the same exclusive 800x600 X8R8G8B8 mode (set file DisplayVideoMode=4, radio auto-select off), through the same tools: `tools/standalone/windows/{d3d9px,dsmute,inputdrv.c,fps_run.sh,fps_scenes.txt,frame_stats.py}`.

## Method (how it differs from the Mac FPS.md, and why)
* Frame time = interval between `IDirect3DDevice9::Present` calls, QPC timestamps from a proxy `d3d9.dll` (`d3d9px`, works for both exes). It also (a) caps the frame rate by sleeping (`cap:N` from the script: 0 = uncapped, 30), (b) can neutralise vsync (`D3D9PX_NOVSYNC=1`: the real CreateDevice/Reset get a copy of the parameters with PresentationInterval=IMMEDIATE and SwapEffect=DISCARD; the caller's struct is untouched), (c) takes back-buffer screenshots on request.
* Input: `inputdrv.exe` (SendInput scancodes + ReadProcessMemory of the game state; refuses to send keys unless the game window is the foreground window), identical script for both exes (menu by mouse hover + RETURN, intro skipped with ESC while the cutscene flag is set, cheat JQNTDMH + F to enter the car). Scenes: `foot` = standing at the new-game spawn (30 s of which uncapped 15 s / capped 20 s), `drive` = cheat car driven W 15+15 s, `walk` = W held 15+15 s on foot. Each scene is measured twice: `_u` = uncapped (proxy cap 0), `_30` = proxy cap 30 fps.
* Both exes run with the game's own limiter OFF (set file byte 0xB35) unless stated, silent (ours: `NOTSA_STANDALONE_MUTE=1`; the original: dsound proxy `dsmute` that pins every DirectSound buffer to DSBVOLUME_MIN and answers GetVolume with the requested value, volumes 0, empty `movies` dir). dsmute.log was not collected (see "Honest caveats").
* Why not the game's limiter + vsync like the Mac: on this PC every vsync-locked Present in EXCLUSIVE fullscreen takes 78 ms (12.7 Hz) - see WIN-PERF.md section 1. The game ties its limiter to vsync (`psCameraShowRaster`: WAITVSYNC when `m_bPrefsFrameLimiter` or a loading screen is active), so limiter ON = 12.7 fps here for both exes; the numbers below therefore use vsync-free presents.

## Results (p50 / p99 / max in ms; 3 runs each for limiter OFF, values per run joined by `/`)
### A. Game limiter OFF, proxy cap 30 fps  (= the game's normal frame rate, no vsync; the comparable table)
| scene | exe | fps | p50 | p99 | max | hitches >50 ms /min | CPU ms/frame |
|---|---|---|---|---|---|---|---|
| foot (Grove spawn, 20 s) | ours | 30.0 x3 | 33.33 | 44.1 / 43.8 / 33.4 | 52.2 / 52.6 / 33.8 | 3 / 3 / 0 | 7.6 / 5.7 / 5.0 |
| foot | original | 30.0 x3 | 33.33 | 33.4 x3 | 33.8 / 33.9 / 33.6 | 0 x3 | 7.8 / 6.8 / 6.8 |
| drive (cheat car, 15 s) | ours | 30.0 x3 | 33.33 | 41.7 / 33.4 / 33.4 | 47.0 / 33.7 / 33.4 | 0 x3 | 6.4 / 4.2 / 4.2 |
| drive | original | 30.0 x3 | 33.33 | 33.4 x3 | 33.5 / 33.4 / 33.7 | 0 x3 | 9.1 / 8.8 / 9.2 |
| walk (W held, 15 s) | ours | 30.0 x3 | 33.33 | 39.6 / 33.5 / 33.4 | 47.0 / 33.7 / 33.7 | 0 x3 | 7.1 / 4.1 / 3.9 |
| walk | original | 30.0 x3 | 33.33 | 33.4 / 33.6 / 39.2 | 33.6 / 33.8 / 41.6 | 0 x3 | 8.3 / 7.8 / 9.8 |
Draw calls per frame are equal within noise (ours 695-870, original 780-900). CPU ms/frame = process CPU time / frames (all threads incl. audio/streaming).
**Reading:** at the normal 30 fps neither exe is anywhere near its CPU limit (frame cost 2-4 ms of a 33 ms budget); both hold the cap with no hitches except ONE port-only effect: the first 20-30 s after the new-game spawn (foot scene) show 1 frame of 52 ms and p99 44 ms in 2 of 3 runs for ours, 0 for the original = streaming/model conversion cost of the librw shim (Mac FPS.md saw the same "first 30 s" effect). Ours uses ~35-50 % LESS CPU per frame than the original (4-7.6 vs 6.8-9.8 ms): native SSE-compiled MSVC x86 vs the exe's x87 code - the Wine/Rosetta "ours 15-30 % slower walking" does not exist natively.

### B. Game limiter OFF, uncapped (CPU-bound speed)
| scene | exe | fps | p50 | p99 | max | CPU ms/frame |
|---|---|---|---|---|---|---|
| foot | ours | 342 / 345 / 366 | 2.97 / 2.87 / 2.83 | 3.9 / 3.8 / 4.0 | 7.6 / 20.8 / 4.8 | 4.2 / 4.1 / 3.8 |
| foot | original | 125 / 123 / 128 | 1.8 / 1.7 / 2.0 (alternating 15.2 + 1.2) | 15.9 / 15.9 / 16.2 | 17.2 / 16.6 / 17.3 | 8.7 / 8.9 / 8.5 |
| drive | ours | 272 / 396 / 529 | 3.7 / 2.3 / 1.9 | 4.7 / 4.4 / 2.3 | 9.3 / 27.4 / 3.9 | 5.1 / 3.5 / 2.5 |
| drive | original | 124 / 127 / 130 | 1.8 / 1.9 / 2.0 | 16.0 / 16.3 / 16.4 | 16.7 / 18.1 / 17.9 | 8.7 / 8.6 / 8.4 |
| walk | ours | 382 / 583 / 543 | 2.2 / 1.7 / 1.8 | 4.5 / 2.2 / 2.3 | 8.3 / 3.8 / 3.5 | 3.5 / 2.3 / 2.4 |
| walk | original | 126 / 132 / 123 | 1.9 / 2.0 / 3.3 | 16.0 / 16.6 / 18.1 | 17.6 / 17.1 / 19.4 | 8.5 / 8.2 / 9.4 |
**The original is NOT CPU-bound here: it is held at ~125 fps by a hard frame lock in `Idle()` (0x53E920, disassembled from the oracle): `do { now = CTimer::GetCurrentTimeInCycles()/GetCyclesPerMillisecond(); } while (now - g_LastTickTime(0xB72CA8) < 14);` - an unconditional 14 ms busy-wait per Idle (pairs of presents 15.2 + 1.2 ms), independent of the limiter setting.** The port has this block commented out (`source/app/app_game.cpp:292`, comment "FPS lock. Limits to 26 frames per second"), so ours runs uncapped: 270-580 fps. That is a behavioural difference vs the exe, harmless at the normal 30 fps limiter, but with the limiter OFF on a fast PC the port runs at 1000-2000 fps (menu-less map views) and then the new-game intro never progresses (black "Francis INTL. Airport" text screen for 100+ s while the game logic runs; seen with frame times 0.4-1.2 ms, t4-t6 runs, cutscene flag never set) - the original cannot reach those rates. Recommendation: decision for the orchestrator - either port the 14 ms lock verbatim behind the standalone/limiter-off path (exact to the exe, burns a core like the exe) or fix the high-fps intro stall separately. NOT changed by me (game logic, outside my zone).

### C. Game limiter ON (game's own 26/30 fps limiter), vsync neutralised by the proxy (1 run each)
| exe | fps (all scenes) | p50 | p99 | max | hitches >50 ms /min | CPU ms/frame |
|---|---|---|---|---|---|---|
| ours | 29.5-30.0 | 33.3-33.4 | 37.9-40.0 (one 75.7) | 40-76 | 0-9 | 19-35 (the limiter busy-waits) |
| original | 24.9-25.0 | 33.4-37.1 | 48-83 | 50-86 | 4-88 | 28-39 |
The original's own limiter settles at ~25 fps with a 80 ms hitch every few frames (p99 81-83 ms, 70-88 hitches/min in the foot_30/drive_30/walk_30 windows); ours holds 30 fps with p99 38-40 ms. (Mac FPS.md: original ~26 fps.) The port's limiter is not the exe's (different code path) - ours is smoother than the original on this host.

## Standing / walking / driving vs the Wine numbers
Wine (Mac): standing parity, walking ours 15-30 % slower, hitch storms under host load. Native: no sign of either (A: ours less CPU than the original in all three scenes, equal frame pacing; B: ours 2.2-4.7x the original's lock-limited rate). Remaining port-only cost: the one-off ~50 ms frame while the area around the spawn streams in (foot scene, 2 of 3 runs).

## Honest caveats
* The user's PC was in normal use (other agents' soaks/tests share the GPU lock but not the CPU; a Windows toast stole the foreground once and made two early runs invalid - the guard aborted them). Run-to-run spread in B (ours drive 272-529 fps) shows the noise; A is robust.
* `dsmute.log` was not collected for the original (it is written at DLL detach, the process is killed by taskkill), so "silent" for the original rests on the proxy + volume 0 + empty movies dir + radio auto-select off; two early diagnostic runs of the original (before the proxy existed: the hang at `LoadLibrary("dpnhpast.dll")` run was silent because it never got that far; the first run that reached gstate 4 ran ~30 s of the intro movie path) were not guaranteed silent.
* Limiter ON measurements are single runs.
* The original crashes with 0xC0000094 (integer divide by zero at 0x4F1464 = `CAEStreamThread::Service`, stream thread) when a car enter starts a radio stream: the Steam install ships silent 4429-byte stub radio tracks (decoder length 0) - same defect as Mac S7/UBF notes; ours turns a zero-length stream into a failed load (a8f07257). Worked around for the oracle runs with radio auto-select OFF in the set file.
* The original refuses to start on this host without a stub `dpnhpast.dll` next to it: `LoadLibraryA("dpnhpast.dll")` hangs forever in `LdrpCheckComponentOnDemandEtwEvent` (optional Windows feature "DirectPlay" not installed; the stub is an empty DLL in the private data dir, no system change).
