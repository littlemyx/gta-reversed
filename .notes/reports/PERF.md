# PERF: walking CPU cost, early hitches, cheat car (agent PERF, 2026-10-11)

Repo reverse/interior_c. Exe under test: Release standalone (several relinks of the shared objects + my instrumented objects, nothing of it left in the tree), reference: `gta_sa_compact.exe` as `gta_sa.exe`. Same private prefix/data for both, silent (`NOTSA_STANDALONE_MUTE=1`, volume 0 in `gta_sa.set`), no thread suspension, scene script = new game, 25-30 s standing, 3x20 s walking (W held / strafe), limiter OFF for capacity comparisons. Host load average was 11-48 the whole time (other agents' games and builds), so only ratios inside one period are used. Tools: `.notes/reports/PERF_tools/` (proxy d3d9.dll with per-frame draw/state/lock/create counters and a hook engine for ORIGINAL-exe functions, QPC zone profiler for ours, per-thread CPU logger, oracle micro-benchmark patch).

## Commit
`6482ce3f` CCheat::AddToCheatString / VehicleCheat (fix c). Nothing else is committed; every other change was scratch (object-level, never in the shared tree).

## (c) JQNTDMH cheat car: FIXED, root cause = hash-table scan order
* `JQNTDMH` (Rancher) is stored in `m_aCheatHashKeys[19]` as key **0** (CRC of the reversed string is exactly 0), and slots 34, 59 (and 90/91 in the exe image) also hold 0. The exe (0x4384F6..0x43852F) hashes the typed string from its full length DOWN to 6 characters and stops at the FIRST slot whose key matches; the port hashed 6..29 characters ascending and took the LAST matching slot -> slot 59 = `PredatorCheat` (a boat), so "cheat activated" was shown and no Rancher came. The original's own misses in the FPS runs (~5 of 8) were not investigated; likely candidates are `LoadAllRequestedModels` not finishing the model in one call and the >50 vehicle rule below.
* Also fixed from the asm: `VehicleCheat` (0x43A0E2) returns without spawning when MORE than 50 vehicle pool slots are used (the port had the loop commented out); `m_bHasPlayerCheated` is set before the cheat function runs.
* Proof (same script, cheat typed 1 s after control, `vehs` = vehicles within 30 m of CJ read from the pool): pre-fix exe (HEAD Cheat.cpp linked into the same object set): before `[model 481]`, after `[model 481]` only (second vehicle appears later, out of range) ; fixed exe: after `[model 481 d=6.6] [model 505 d=5.4]` (model 505 = Rancher), twice (r/ and the pool exe). Compile check: Release flags via `ninja -t commands` line, 0 errors.

## (b) first-30-s hitches: NOT a port-specific lazy-resource problem
* Per-frame D3D counters (proxy, identical for both exes): textures/VB/IB/shaders created during gameplay = **0 per frame in all scenes in BOTH exes** (creation bursts only in the load->play transition: ours 650 in one frame, original 240-280), so ours does not create/upload at draw time. The exe's `D3DResourceSystem` recycling pool (not fed by librw) was tested anyway: a librw hook + pool build (patch in PERF_tools, exe `rp`) gave no change (stand 41.6 vs 39.7 fps, walk 35.7/30.2/30.4 vs 33.6/31.3/31.9) -> **not applied**.
* Frames >50 ms in the first 30 s of gameplay on non-saturated windows: ours 3, 11, 2, 2, 2, 1, 1, 4 (runs a10,a20,a31,a40,a50,a70,a90,a114), original 4, 0, 4 (a30,a60,a112); with load 30+ both exes show 24+. Hitch size 50-64 ms in both; ours is blocked inside `Draw*` (DrawPrimitiveUP/DrawIndexedPrimitiveUP: 20-39 ms in the frame), the original inside `VertexBuffer::Lock` (16-35 ms) = wined3d command-stream back-pressure in both, not shader/texture first use (no slow `Create*` event > 6 ms in any run). Per frame (a10 ours vs a3 orig): DIP 691 / 709, SetTexture 697 / 675, SetRenderState 502 / 503; ours additionally SetVertexDeclaration 287 vs 28 (librw creates one decl object per geometry, RW shares them), VS consts 28 vs 7, PS consts 21 vs 0, DPUP+DIPUP 53 vs 0 (Im2D/Im3D through the UP path; original uses DP on its VB ring). wined3d CS thread: ours 37-52 % of a core, original 28-45 %.

## (a) walking costs more CPU than the original: explained, not closed
**Measured gap (limiter off, back-to-back, host load ~15-25):** original stand 37.1 fps, walk 37.3 / 38.1 / 42.3; ours stand 39.7, walk 33.6 / 31.3 / 31.9 (-15..-20 %; standing equal or better). The FPS report's "CPU ms/frame" was misleading: with the limiter ON the main thread busy-waits in BOTH exes (86-97 % of a core at 25-30 fps), so only the limiter-off capacity compares.

**Where it goes (QPC zones in ours, QPC hooks on the same functions in the original, ms/frame, runs a60/a70 same period; incl.):**

| function | orig stand | orig walk | ours stand | ours walk |
|---|---|---|---|---|
| Idle (frame) | 24.6 | 24.6 | 26.9 | 34.7 |
| CGame::Process | 3.6 | 3.8 | 7.0 | 11.0 |
| CWorld::Process | 1.5 | 1.7 | 2.4 | 7.9 |
| CPhysical::ProcessCollision (calls) | 1.15 (11.7) | 1.23 (12.6) | 1.71 (12.1) | 5.74 (37.8) |
| CCollision::TestSphereTriangle (calls) | 1.11 (759) | 1.15 (783) | 1.45 (759) | 3.42 (1216) |
| CRenderer::ConstructRenderList | 1.20 | 1.21 | 1.77 | 4.09 |
| RenderScene | 8.6 | 9.3 | 10.7 | 10.7 |

Walking brings ~20 peds and ~13 cars into the moving list (pools: ours stand ped 2-6 / veh 5-7, walk ped 17-20 / veh 9-15; original stand ped 7-12 / veh 13-18, walk 11-21 / 9-15) and every one of them runs ProcessControl + ProcessCollision each frame, so the collision/ped code dominates; the frame time then feeds back into larger time steps (more collision sub-steps) which is why the effect compounds. The same walking population in the ORIGINAL (run a112, load 25: ProcessCollision 13 -> 38 calls, CWorld::Process 2.2 -> 6.8 ms, frame 36.6 -> 45.9 ms) shows the identical surge: it is workload, not a behavioural difference; the original just spends less per call.

**Per-call cost ours / original** (same workload, in-game): ProcessCollision 1.5-2.2x, CPed::ProcessControl 2.2x, CAutomobile::ProcessControl 2.1x, TestSphereTriangle 1.3x, ConstructRenderList 1.0-1.5x, CPlantMgr::Update 4x, CCamera::Process / ProcessLineOfSight 2-3x. Out-of-game, controlled (exe-oracle micro-benchmark, same inputs, same process, PC=24, `PERF_tools/oracle_bench.patch`): port / exe = 1.25 TestSphereTriangle, 1.45 TestLineTriangle, 1.35 ProcessSphereTriangle, 1.44 ProcessLineTriangle, 1.31 TestLineBox_DW, 1.21 ProcessSphereSphere, 1.19 CVector::Normalise, 1.07 Magnitude.

**Root cause: x87 under Rosetta.** Absolute cost of the exe's own TestSphereTriangle is 1.44 us per call (~4 ns per x87 instruction); the same port source compiled `/arch:SSE2` is 13 ns (100x), Process* primitives 30-40 ns (30x), Normalise 62 ns. The port's x87 is also more verbose than the MSVC 7 code of the exe (static count, TestSphereTriangle: exe 437 insts / 91 fld-fst-fstp / 6 fxch, port 478 / 177 / 35; ProcessSphereTriangle exe 622 / 127 / 8, port 687 / 322 / 47), hence 1.1-1.5x per call. Compile switches do nothing: `/GS-` (leaf functions have no cookie anyway), `/Ob3`, `/fp:fast` all within noise (1.25-1.48 unchanged). `_ftol2` is NOT the problem (our fldcw version 44 ns vs the exe's algorithm 82 ns per call under Rosetta).

**Not the cause (measured):** D3D resource creation/pooling (0 per frame, pool experiment no change), draw/state counts (equal), Tracy (compiled out), population size (similar once walking), frame-limiter spin (equal), wined3d CS thread (+10 % core, off the critical path).

**Port-side outliers worth a follow-up** (stand state, ours vs original, ms): CAudioEngine::Service 1.45-1.9 vs 0.5-0.7 (AUDIO's area), CPlantMgr::Update 0.76 vs 0.18, CCoronas::Update 1.4 vs 0.7, CCamera::Process 1.2 vs 0.6 (camera cone cast), CCollision::ProcessLineOfSight 1.3 vs 0.43; ShadowManager::Update 1.5-2.7 (SHADOW's).

**What would close the gap (not done, needs an owner decision, D-B04 owns Collision.cpp):**
1. Selective SSE2 for pure-float primitives. Because the game thread runs at PC=24, x87 float arithmetic equals single-precision SSE arithmetic (except exponent range), so a function that is written with `double` intermediates must be re-expressed in `float` and moved to a TU compiled `/arch:SSE2`. Prototype (scratch only, `PERF_tools/testspheretriangle_float_sse2.patch`): TestSphereTriangle as float, whole Collision.cpp `/arch:SSE2`: exe-oracle 100000 cases at PC=24 -> 0 regular mismatches, 5 "special input" ones (sphere radius = +inf); 1.4 us -> 14 ns per call. In-game that single function is 3.3 ms of the 36 ms walking frame (the Test* group ~4.5 ms) -> walking frame ~31 ms = original. Without the float rewrite (plain `/arch:SSE2`) 33 of 60000 TestSphereTriangle and thousands of Process*/DistToLine cases differ, so it must be per function, oracle-gated (`bench.exe -n 100000 <name>`).
2. clang-cl's x87 stackifier may emit far fewer fxch/spills than MSVC 14 (the C1/clang agents already build with it): measure with `PERF_tools/bv.sh`.
3. Share vertex declarations in librw (the exe does): 287 -> 28 SetVertexDeclaration per frame; expected gain small (CS thread only), unmeasurable in this noise, not done.

## Notes / cautions
* `QueryPerformanceCounter` values and wall FILETIMEs must be handled as int64 in game code: after D3D9 CreateDevice the main thread has x87 PC=24, so a `double` of a 1e13 tick count has a 128-256 ms ulp (my first profile was quantised to 256 ms because of this).
* The original freezes at LOADING / right after control in ~1 of 4 hooked runs; hooking a float-returning function (CCamera::CalculateGroundHeight, result in ST0) froze it reliably -> removed from the hook set.
* A "release" exe copied from `build/StandaloneRelease/bin` at 00:16 already contained my Cheat.cpp edit (shared tree), so a true pre-fix exe had to be linked from HEAD's Cheat.cpp.
* Zombie check done at the end: all my wine/winedbg/driver/queue processes killed, /tmp/perf* and the private prefix removed, no worktrees, no running agents of mine.
