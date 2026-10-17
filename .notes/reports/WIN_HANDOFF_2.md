# Windows host — hand-over 2 (paused for a user reboot), 2026-10-17 ~12:00

Paused on the user's request (PC reboot). All agents stopped, game/build/census processes killed, `C:/gta-scratch/{gpu,build}.lock` removed.
HEAD of `reverse/interior_c` = 53fedc65 (pushed).

## Done in Windows session 2 (details: tail of `.notes/PHASE2_STATUS.md`, reports in `.notes/reports/`)
* Native StandaloneRelease build; toolchain verified (cl 19.51.36260 in toolset 14.51.36231 is correct).
* run_all_tests.sh 42/42 on the RTX 4080 (RW-GPU: skin VS flags 4 -> 2; three tests had Wine-specific expectations).
* BUG-QUIT: 4 shutdown UAF/bad-free fixes; librw f358ffd (VB/IB out-pointer nil-init).
* WINDOWED: `NOTSA_STANDALONE_WINDOWED=<W>x<H>`; soak.sh/census.py default 960x600 on Windows.
* WIN-PERF: cold start 72 s = vsync-locked Present ~78 ms/frame on this PC (original identical); ours less CPU/frame than the original. WIN_FPS.md.
* IDLE-LOCK (917b3c52): exe's 14 ms Idle() busy-wait ported; fps/hitches/intro time now match the original.
* Stream D: B07 verified, B09–B19 converted + verified, D1/D2/E1 done (2305 globals, 0 mismatches; census UNCLAIMED 17 -> 6).
* PLAY-KIT: desktop shortcut -> `C:\gta-play\play.cmd` (GPU lock owner USER-PLAY). Snapshot exe predates D-DE's commits -> refresh it.
* Rules: ninja -j4 BelowNormal; GPU lock `mkdir C:/gta-scratch/gpu.lock` around game runs/device tests; never git stash others' files; commit per file.

## Interrupted: phase C (C1..C6), uncommitted
Three agents were mid-work in the shared tree: D-C12 (C1 Tasks/PedGroups/Gangs/Inform/Attractors, C2 TempColModels/Matrix/OctTree), D-C34 (C3 Entity/Cam/Camera, C4 CStores/AnimBlendHierarchy/Fx/Furniture/Clothes), D-C56 (C5 Audio/Menu/Pad/HudColours/HandShaker/ControllerConfig, C6 Scripts/PathFind/RoadBlocks/Garages/CPlayerInfo).
* 77 modified files left in the working tree (NOT committed, state unknown/partly half-converted). Snapshot: branch `wip/phaseC-pause-2026-10-17` (88b9ad3a, pushed) and `C:/gta-scratch/phaseC_wip_2026-10-17.patch` (1059 lines).
* 10 of them got CRLF line endings (Garages.h, Glass.h, Pad.cpp, PathFind.cpp, TheScripts.h, StencilShadows.h, WeaponEffects.h, WeaponInfo.h, Weather.cpp, World.h) — normalise to LF before committing.
* No phase-C reports were written (`.notes/reports/D-C12.md`, `D-C34.md`, `D-C56.md` absent). `.notes/ZONES.md` holds their (uncommitted) claim lines — remove them.
* Scratch from these runs: C:/gta-scratch/c12_*, C:/gta-soak-out/c12, C:/gta-census-c12.

## Resume plan
1. Triage the 77 files: one sonnet agent per family reads the WIP diff of its family (C1+C2 / C3+C4 / C5+C6), finishes or reverts each object, then runs the phase-C acceptance (static_assert size, verifier 0 mismatches outside vptr dwords, option-OFF identity, Debug DLL compile, poison soak + census). Assign files by family so nobody touches another's; first agent to start normalises CRLF.
2. B20 (common.h + leftovers) alone, after C lands (forces full rebuilds).
3. F1 (raw casts / plugin::Call), F2 (hooks out of run build), F3 (delete DataImage/Fixups), G1 (CMake without GTASA_ORIGINAL_EXE), G2 (clean clone without the exe on disk).
4. Refresh `C:\gta-play` from a clean tree; user listen test still pending (crackle / shadows / stutter).
## Open bugs
* Native DirectShow intro video hangs on a black frame >3 min (play.cmd skips videos).
* One-off census AV at 0x016a5f26 right after the car cheat (unsymbolised); one-off null `new CDummyObject()` in CFileLoader::LoadObjectInstance (pool full?).
* GetComboFlagsLo pre-array read mapped to named neighbours — never exercised at run time.
* Unclaimed census pages left: 4 raw casts (F1) + 2 audio vtables (C5).
