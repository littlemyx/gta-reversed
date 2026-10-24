# MAC-FLOOP1 report (2026-10-24): F-loop iteration 1
Commit 4e0ff8bc (portable/main). Details: RR_PLAN.md section 11.7.

| first divergent frame (exact / quantised) | before | after D1+D3 | after D2 (final) |
|---|---|---|---|
| rng | 0 / 0 | 57 / 57 | 57 / 57 |
| pl (player) | 0 / 181 | 0 / 181 | 56 / 181 |
| cam | 0 / 57 | 0 / 57 | 0 / 57 |
| pop, pd, vh | 0 / 0 | 0 / 0 | 0 / 0 |
| ob | 139 / 139 | 139 / 139 | 139 / 139 |
Frame-0 detail diff fields vs Win32: 88 -> 86.

Fixes (all NOTSA_PORTABLE-guarded; wine cl.exe A/B CarCtrl.cpp.obj + Stats.cpp.obj: code+relocs bit-identical, so Win32 unchanged and the reference trace stays valid):
* D1 ChooseModel total/shares: double chain -> float chain (PC24 rounds each partial sum): 8 -> 9 as in Win32; first RNG divergence moved from the first recorded frame to frame 57.
* D3 GenerateOneRandomCar density checks: same class, float chains.
* D2 CStats::GetFatAndMuscleModifier: `(double)569*(double)0.001f*176` is one rounding; the exe rounds per op -> player health 100.14399719 now equal.
* C-8: librw was built without -ffp-contract=off (game lib already had it everywhere, no fast-math anywhere): added to source/CMakeLists.txt (portable only).
Self-determinism of the new portable build: 2 detail runs byte-identical.
Open (RR_PLAN 11.7): N1 vehicle micro-velocity noise from idle frame 4; N2 RNG/loaded-model-set differences at new-game start (idle 168..175, re-converging at the harness reseed) -> ped 9 model and stmem; N3 camera/ped 1-10 ulp. Localisation recipe: NOTSA_RR_LOG_FRAMES/LOG_ENTS on both sides (rrf/rrfe lines), 11 s Win32 runs.
Consumer audit: ChooseModel/GenerateOneRandomCar/GetFatAndMuscleModifier have no layout/shared-state change (arithmetic only). No gameplay-affecting soak run (time); effect = portable arithmetic only.
No background tasks, wine or game processes left.
