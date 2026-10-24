# MAC-FLOOP2 report (2026-10-24): systematic PC24 float semantics + record/replay loop 2
Commits (portable/main, pushed): f138c7e1 (x87d + tool v1), d0626213 / 88acc3d4 / 3d315dab (first batches), then X87R + tool v2 + Matrix.h/Vector2D.h, then 17 per-directory commits of game_sa (213 TUs in total) + IdleCam. Details RR_PLAN 11.8.

| item | result |
|---|---|
| Mechanism | `x87d` (source/Base.h): double on Win32 / float under NOTSA_PORTABLE; `X87R(expr)`: bare tokens on Win32 / `((x87d)(expr))` portable, for an op with a 53 bit operand (double global, call result, x87:: result, non-float literal). Parentheses and casts are NOT neutral on MSVC (/fp:precise spills; 14 TUs differed with `(expr)`), bare tokens are. F-loop 1 `#ifdef` blocks replaced. |
| Tool | tools/portable/pc24_scan.py: clang JSON AST over build/PortableMacRR/compile_commands.json, 14 s for 1179 TUs; TSV file:line / verdict / ops / expression / leaves / function / oracle (address comment vs tests/standalone); `--apply` rewrites AUTO + WRAP roots and clean double locals; idempotent (0 edits on re-run); `--only/--exclude`. |
| Census (whole game_sa, last scan) | AUTO 2105, WRAP 734, CHECK-C 41 (compound assign on a dirty double), LOCAL-AUTO 975, LOCAL-CHECK-D 209 (locals holding x87:: / pow / *Ext() results, kept double on purpose), SKIP-MACRO 11 |
| Applied | all of game_sa: 213 TUs changed (hot set CarCtrl, Stats, Population-side, Cam/Camera/IdleCam, Ped/PlayerPed, Physical, Automobile + all vehicles, World-side, Core/Vector/Matrix/Quaternion/Placeable, Collision, WaterLevel, PathFind, ...) + hand edits: Matrix.h, Vector2D.h, `PhysicalAddScaledByRecip(double r)`, Ped.cpp lambda return type |
| Not touched (another agent's uncommitted edits in them) | Group21b, Group23, 2dEffect.cpp/.h, CutsceneMgr, CutsceneObject, CustomCarEnvMapPipeline; Cam.cpp and TaskSimpleFight.cpp x87d edits are STAGED in the index together with that agent's staged hunks (they land with its commit) |
| MSVC neutrality proof | wine cl.exe on build/StandaloneReleaseRR (statehash variant) vs signatures taken before the first edit, `obj_code_sig.py` code + relocs: all 213 changed TUs identical; plus 39 unchanged TUs that inline Matrix.h identical (the 40th, CutsceneMgr.cpp, differs by the other agent's staged change). Reference exe unchanged (restored from a copy after the probe experiments). |
| Portable determinism | 3 runs of the 900-frame route: identical F lines (md5 52fb4331...). Smoke: play1 s1 walk 30 s OK, s8 OK before the global sweep; s2 (car drive) runs, then aborts in a static destructor at exit (IKChainManager_c::~, not gameplay; pre-existing class, MAC-BISECT1 area). No soak.sh (Windows tool). |

First divergent frame (exact / quantised), 900-frame route vs `win32-wine-walk900.trace`:
| | FLOOP1 | FLOOP2 |
|---|---|---|
| rng | 57/57 | 254/254 |
| pop | 0/0 | 256/256 |
| pl | 56/181 | 56/181 |
| cam | 0/57 | 181/182 |
| pd | 0/0 | 27/181 |
| vh | 0/0 | 27/200 |
| ob | 139/139 | 163/286 |
Idle frames (menu + new-game start, 225): 64 differing rrf/rrfe lines -> 0 of 451: N1, N2, N3 closed.
N1 root causes (bisected with Probe calls inside CAutomobile, Win32 variant relinked by hand, probes reverted): `PhysicalAddScaledByRecip(..., double r)` parameter (double product instead of float), `CVector::Normalise` and the Matrix.h `TransformVector/Point` inline chains. Tool blind spots that caused them: double parameters/returns of helpers, lambdas, header inline code (PCH hides header decls; a no-PCH dump is 7 GB).
Open: recorded frame 27 (idle 279) vehicle slot 8 collision response tv.x 1e-5 off; player exact from 56; RNG from 254. Coordinates in RR_PLAN 11.8.
Consumer audit: arithmetic-only changes (no layout / shared-state change); the portable runtime effect is every `double` chain of the game now rounding per operation like the exe's PC=24 x87.
Cleanup: no game / wine / ninja processes left (wineserver of the msvc prefix killed).
