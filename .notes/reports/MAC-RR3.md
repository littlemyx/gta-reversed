# MAC-RR3 report (2026-10-24)
Result: first Win32 state-hash trace produced (Wine on the Mac) and diffed against the portable trace. Details: RR_PLAN.md section 11 (11.4 diff, 11.5 divergence log).

| item | result |
|---|---|
| Win32 variant | `-DGTASA_STATEHASH=ON` builds StandaloneRelease (build/StandaloneReleaseRR, 17 MB exe); pins: virtual QPC (oswrapper_win.cpp), inline CdStreamRead (CdStreamInfo.cpp), shared zeroing, NOAUDIO on both sides |
| OFF neutrality | build.ninja identical (current vs HEAD vs pre-RR base worktrees); wine cl.exe A/B 37 TUs: 36 bit-identical, app_game.cpp differs only in one `__LINE__` immediate |
| Win32 self-determinism | 3+3 runs identical (3 byte-identical files), 55 s per 900-frame run |
| Portable self-determinism (NOAUDIO) | 3+3 runs byte-identical |
| Pre-history | gf / CTimer ms / game clock equal in all 900 frames |
| First divergence | recorded frame 0, exact+quantised (rng, pop, pd/vh/ob counts); per-subsystem first frames: pl 0/181, cam 0/57, ob 131/131 (exact/quantised) |
| Cause (class F) | `CCarCtrl::ChooseModel` `total`: identical float inputs, portable double sum -> 8, Win32 PC=24 rounding -> 9; first RNG-divergent draw = 20th of the 100 start-up GenerateOneRandomCar calls of idle frame 1; second: player health 100.14400482 vs 100.14399719 |
| Open | Windows-host confirmation (PORTABLE_SYNC.md); Rosetta PC=24 fidelity unproven; Win32 thread-local rand not needed with audio off |

How it was localised (no fix applied): RNG state is equal at the menu and at ~25 probe points through CGame::Initialise (70 000 draws) and the first CGame::Process up to GenerateRandomCars; probes `RR_MARK` / `RR_STAGE` / `RR_AUX` (reverted, method in RR_PLAN 11.5) narrowed it to the call, the stage (ChooseRandomCarModel) and the inputs of ChooseModel.
Runs: `tools/portable/rr_run_win32.sh` (new; retries two Wine start-up flakes; gamedir must be a short path), `tools/portable/rr_run.sh` with `NOTSA_RR_NOAUDIO=1`. Traces in `.notes/rr_traces/`.
Changed: source/CMakeLists.txt, portable/statehash.{cpp,h}, oswrapper/oswrapper_win.cpp, game_sa/CdStreamInfo.cpp, game_sa/Entity/Entity.cpp + Tasks/TaskTypes/TaskSimpleHoldEntity.cpp (F6 clang-only diagnostics narrowed to NOTSA_PORTABLE), standalone/rw/rtanim.cpp (RR variant skips the pointer stores too).
Not touched: MAC-PLAY1 files. No background tasks, wine or game processes left (checked).
