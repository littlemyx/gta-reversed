# IDLE-LOCK - the exe's 14 ms `Idle()` frame lock ported verbatim (Windows host, 2026-10-17)

## Original (oracle `gta_sa_compact.exe`, capstone)
```
53E920 sub esp,8 ; 53E923 call 561A80 (GetCurrentTimeInCycles) ; 53E928 call 561A40 (GetCyclesPerMillisecond)   <- results discarded
53E930 call 561A80 ; mov esi,eax ; call 561A40 ; mov ecx,eax ; xor edx,edx ; mov eax,esi ; div ecx           ; now_ms = cycles / cyclesPerMs   (unsigned 32)
53E944 sub eax,[B72CA8] ; cmp eax,0Eh ; jb 53E930                                                           ; spin while (now_ms - g_LastTickTime) < 14   (unsigned)
53E94F call 561A80 ; ... ; div ecx ; mov [B72CA8],eax                                                      ; g_LastTickTime = GetCurrentTimeInCycles()/GetCyclesPerMillisecond()
53E968 call 561B10 (CTimer::Update) ...
```
Only `Idle` references 0xB72CA8 (byte scan of the exe: 0x53E946, 0x53E964). All uint32; initial value 0.

## Port (`source/app/app_game.cpp`, `Idle`)
```cpp
NOTSA_GLOBAL_LOCAL(g_LastTickTime, 0xB72CA8, (uint32), {});   // function-local like `version_number`; address mode = StaticRef at 0xB72CA8
uint32 nowMs;
do { nowMs = CTimer::GetCurrentTimeInCycles() / CTimer::GetCyclesPerMillisecond(); } while (nowMs - g_LastTickTime < 14);
g_LastTickTime = CTimer::GetCurrentTimeInCycles() / CTimer::GetCyclesPerMillisecond();
```
The old commented-out block ("FPS lock. Limits to 26 frames per second") is gone. The two discarded leading calls are side-effect free and omitted. No run-time switch, no `#if`: always on in standalone and in the DLL build (there the hooked `Idle` replaces the exe's, which has the lock, so DLL == original too). `docs/STANDALONE_FIDELITY.md` has a line for it; `standalone_deviations.tsv` needs none (no deviation).

## Measurements (800x600 exclusive, d3d9px Present log, `fps_run.sh`; tags il1_* in C:\gta-perf\runs)
| case | before (WIN-PERF t5/t6/r1 etc.) | after |
|---|---|---|
| limiter OFF, uncapped (foot / drive / walk) | ours 342-366 / 272-529 / 382-583 fps, p50 1.7-3.7 ms; original 123-132 fps (15.2 + 1.2 ms Present pairs) | ours 117.2 / 107.9 / 128.9 fps (p50 3.75 / 4.26 / 2.28, p95 18.1 / 18.7 / 16.2 ms = same 14-16 ms + short frame pairing as the original) |
| new game -> player control (`skipintro`) | ours: 5.0 s ok in most runs, but TIMEOUT (120 s, black "Francis INTL." text) in t7 at high fps | ours 5016 ms ok; original (same run il1_orig_off) 5015 ms ok |
| limiter ON (vsync neutralised), foot_30 / drive_30 / walk_30 | ours 29.5-30.0 fps, p99 38-40 ms, ~0-9 hitches/min (table C of WIN_FPS.md); original 24.9-25.0 fps, p99 81-83 ms, 70-88 hitches/min | ours 25.03 / 25.05 / 25.00 fps, p50 33.8, p99 82.5 / 81.3 / 81.6 ms, 68-75 hitches >50 ms/min = the original's table-C numbers |

Reading: the "limiter smoother in ours" finding of WIN-PERF was an artefact of the missing lock; with the exe's lock the limiter pacing is the original's (limiter + 14 ms lock interplay: ~25 fps, 80 ms frames), so "limiter ON still ~30 fps" does NOT hold any more - 25 fps is the original's behaviour (Mac FPS.md: original ~26).
Uncapped ours (108-129 fps) is slightly below the original's 123-132 in drive because there ours' frames are longer than 14 ms in pairs; no tuning (CPU-bound variation, not the lock).

## Soak and tests
* `soak.sh build/StandaloneRelease 3` (windowed 960x600, limiter on, GPU lock): RESULT OK, errors 0, 9800 frames, peak committed 547 MB; fps per phase 24.4-25.1 (31 in the menu->new game load), consistent with the 25 fps above.
* `run_all_tests.sh` (build/StandaloneRW, 42 tests, GPU lock around the run): 42/42 ok, 0 failures, 0 mismatches (log C:/gta-scratch/idlelock_tests.log).
