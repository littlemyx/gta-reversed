# RD-ARM - slice 1 of REPLAY_DIFF_PLAN.md (arm point, pins, runner)

Agent RD-ARM, rules v4, 2026-10-24 05:32 - ~07:00 WEST. Worktree C:/gta-wt/rd-arm (branch rd-arm), runs under C:/gta-scratch/rdd/rdarm*/.

## What was built
* Port side (from branch replay-diff-probe 1c3d24a5, reviewed line by line, then changed):
  * `GameRand.h`: the srand call is now `g_SrandHook(seed, file, line)` (a function pointer, null unless `NOTSA_LOCKSTEP_OUT` is set - no link dependency on Lockstep.cpp
    any more); `GameRand()` takes a defaulted `std::source_location` and calls `g_RandHook` (null unless pin bit 32 + `NOTSA_LOCKSTEP_RLOG`). With the env unset the only
    runtime cost is a null test per rand()/srand().
  * `Lockstep.cpp`: **arm point** = first Tick with a player ped. At the arm point: pin 8 re-seeds the RNG (0x2468ACE1), **pin 16 (new) jumps `CTimer::m_FrameCounter`
    FORWARD to the next multiple of 0x100000** and shifts the pinned clock anchor back by the same number of slots (clock stays continuous; a forward jump keeps every stored
    "frame stamp" in the past), the per-frame rand-call counter is zeroed. S lines now carry `file:line` of the srand call (the original's S lines carry the return address;
    rd_diff.py ignores that column). Pin 32 + `NOTSA_LOCKSTEP_RLOG=<n>`: `R fc file:line` for every rand() call of the first n traced frames (noise hunting).
  * Timer.cpp / WinPs.cpp hooks unchanged from the probe (inert without the env). Listed in `tools/standalone/standalone_deviations.tsv`.
* Original-exe side `rd_orig.c`: same arm logic (pin 8 + pin 16 with the same granule, `fc` read after the tick), `R fc ra` rand log (pin 32 + `RD_RLOG`). Built by build_orig.sh.
* `tools/standalone/replay_diff/run.sh [intro] --reps 3 --pin 31 --frames 3000`: runs o1 p1 o2 p2 o3 p3 (each leg takes gpu.lock itself, waits for any owner), retries a short
  run up to 2 times (logs kept in `failed/`), prints + writes `report.md` (A-A and A-B table, first-divergence detail), verifies by signature (file list+size+mtime+md5) that the
  Documents\GTA San Andreas User Files directory is bit-identical before/after (exit 2 if not). rd_diff.py gained `--max N` and a `SUMMARY` line.
* Safety fixes in the probe runners: the game is killed / Documents restored / lock released only while the script owns gpu.lock (the old trap would have killed another agent's game
  while waiting); `docs_backup` now happens after the lock is taken; `RD_MAXF` / `NOTSA_LOCKSTEP_MAXF` pass through.

## Result: intro scenario, pins 1+2+4+8+16 (=31), first 3000 traced frames, 3 repetitions each (final binaries)
| pair | equal prefix | equal frames | first divergence |
|---|---|---|---|
| o1-o2 / o1-o3 / o2-o3 | 91 / 92 / 91 | 91 / 92 / 91 | rng (rand-call count) |
| p1-p2 / p1-p3 / p2-p3 | 9 / 7 / 7 | 1841 / 7 / 7 | rng |
| o1-p1 / o2-p2 / o3-p3 | 1 / 1 / 1 | 1 / 1 / 1 | rng, ncalls 74 (orig) vs 317 (port) at frame 1 |

Earlier run with the pre-RLOG binaries (same pins): orig-orig prefixes 16/16/1509, port-port 12/4/4. The A-A noise is run-to-run variable (race with real time), so the acceptance
(3000 equal frames x 3 reps) is **NOT reached**: A-A is equal for 7-92 frames, not 3000.
Frame 0 (the arm frame) is identical orig-vs-orig, port-vs-port AND orig-vs-port in fc (1048576 on both: pin 16 works), tms, RNG seed, player pos/health, pools, script hash.

## First remaining divergences (with the R log, 5 orig runs / 2 port runs, 45 frames)
* orig-orig: first differing rand sequence at frames 10-18. The differing calls are **population spawns** (vehicle plate text `GeneratePlateText`, `PickRandomCar`,
  `GeneratePedCreationCoors`, then everything after it): whether a car/ped is created in frame N depends on whether the streamed model arrived by then (CdStream thread,
  plan row 7 = RD-NOISE "CdStream tick barrier"). One orig-orig run pair also differed by an audio rand (AEAudioHardware) in frame 10 (row 6).
* port-port: frame 19 (R log run): one extra `CGeneral::GetRandomNumber` (General.cpp:203, caller unknown - wrapper) in one of two runs, followed by peds/vehicles count at 15..18: same
  family (spawn timing), callers not resolved (a CGeneral wrapper hides them; RD-NOISE should capture the return address in the port hook).
* orig-port: frame 1: rand calls 74 vs 317 (unchanged lead from the probe; scr hash differs at frame 1, peds/vehs at 8, player pz at 88, px/py at 93).
* srand at frame 1509 differs in every pair: the pointer-derived seed of Entity.cpp:1056 (plan row 8).

## Gate (rules v4, env vars unset = inert)
* Release build (StandaloneRelease, rebased tree 70536578 minus this note): ok. Soak (NOVSYNC, 3 min route, env unset, pre-rebase tree = same code): RESULT OK, 0 errors, 33100 frames.
* run_all_tests: all 74 RW test targets BUILD ok on the rebased tree; the RUN phase was NOT completed (three run_all_tests instances shared the PC and the build lock for 40+ min; my own
  run was invalidated when I rebased mid-build). => **not landed on reverse/interior_c, pushed as branch origin/rd-arm** (fast-forward onto interior_c; needs the orchestrator gate).

## Not done / next
RD-NOISE (slice 2) is exactly this list: CdStream tick barrier (spawn timing), audio RNG partition, pointer seed, Grain/OS_TimeMS + time(). The port-port prefix (7-9 frames) is shorter than
the orig-orig one (91): look at what in the port runs earlier than in the original (frame 4-12 peds/vehs counts).
