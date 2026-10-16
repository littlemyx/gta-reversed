# TESTFIX report (PC24 suite green)

Full `tools/standalone/run_all_tests.sh build/StandaloneRW` at the tip of reverse/interior_c (plus the commits below), all 41 tests ok.
Run 1 (full, built from the tree): 38 ok, 3 NORESULT (pool_oracle_test = parser, rw_raster_image_test = wined3d flake, script_oracle_g16_test = real crash).
After fixing the first and third and a re-run of the flake: all ok (table at the end).

## Items
| # | Symptom | Root cause | Fix (commit) |
|---|---------|-----------|--------------|
| 1 | 9 rw_* tests NOBUILD | `standalone/rw/camera.cpp` memlog (f63e33fd, d4a78ca3) used FindPlayerPed/CWorld/CPad/CClock/pools; the PCH-less shim tests do not have them | game-state part moved to `Fixups::LogGameStateTrace` (Fixups.cpp), no-op stub in tests/standalone/fixups_log_stub.cpp and rw_skin_pipeline_test.cpp (e8a8d734). rw_custom_pipelines_test has its own PCH: stale, deleted its cmake_pch.cxx.{pch,obj} once |
| 2 | physics_oracle_test: fault at EIP 0x400000 in SteerAICarWithPhysicsFollowPath | NOT the foreign CarCtrl.cpp hunk (that was NANTRACE logging in DragCarToPoint only) and not a port bug. The 16-bit CWorld scan code wraps at 0xFFFF after ~65k traffic sector scans (case 2518 of the full CCarCtrl:: run); the inline CWorld::AdvanceCurrentScanCode then calls CWorld::ClearScanCodes, which is not linked in this test (World.cpp) -> /FORCE:UNRESOLVED = jump to 0x400000. Found with breadcrumbs in the port (crash inside FindMaximumSpeedForThisCarInTraffic, EAX=0xB7CD78=ms_nCurrentScanCode, ECX=0xFFFF); the filtered run never reaches 65535 | `void CWorld::ClearScanCodes()` defined in physics_oracle_cd.inc through the exe (0x563470) (e869a888). Breadcrumb edits were reverted exactly (CarCtrl.cpp / physics_oracle_test.cpp restored from copies under the build mutex) |
| 3 | logic_oracle_test > 900 s | ~760 s run but near the cap under load | runner: `logic_oracle_test` TIMEOUT 3000 s (e8a8d734). The test itself is untouched (>=1000 cases/function) |
| 4 | logic_oracle_test 800 mismatches at the tip (coordinator item) | `CCarCtrl::ChooseCarModelToLoad` guard (238aca02 of p2e5-pop). Applied as hunk only via the index (`git apply --cached` + working tree), the foreign dirty hunks stayed untouched (cdbb35f1). After it: 224 functions, PC24 0/0. The ColSphere / PedGeometryAnalyser versions on main (c4ffe6bb, ea2eaa47) pass the oracle (CColSphere x4, ComputeClosestSurfacePoint x3, ComputeRouteRoundSphere: PC24 0), so a820efca / c7857a23 were NOT applied (p2e5 versions are float-based and differ from main's; main's are oracle-clean) | cdbb35f1 |
| 5 | x87_oracle_test NORESULT | summary says "67 rows," | runner regex accepts `functions|rows` and ", INFO rows excluded" (4c9d3671) |
| 6 | x87 PC24 2/2 on CQuaternion::Get_Euler (denormal case 1042) | `CQuaternion::Get(RwMatrix*)` 0x59C080: in the exe 2x/2y stay on the x87 stack, 2z is spilled, y2r and z2r stay UNROUNDED and enter right.y/right.z/up.x/at.x; the port rounded every product to float (denormal rounding differs by 1 ulp, x5 after the atan2 division) | rewritten per asm (4c9d3671); Get_Euler PC24 0 and PC53 428 -> 0 |
| 7 | pool_oracle_test NORESULT (x6 retries) | summary line not parsed ("N checks, ...: M mismatches" + PASS) | runner rule added |
| 8 | script_oracle_g16_test NORESULT: fault EIP 0x400000 at command 1672 | since f0896a29 `CTask::~CTask` is out of line (Task.cpp, not linked); the stack task of TASK_TOGGLE_PED_THREAT_SCANNER destroys through it | `CTask::~CTask() {}` stub in the test (39f7fdbd) |
| 9 | rw_raster_image_test NORESULT 6/6 in the full run | wined3d/MoltenVK device creation dies (exit 5 / hang, no output) while the host is loaded with other agents' builds; 3/3 manual runs PASS, re-run in the runner: ok | none (known flake) |
| 10 | fixed_oracle_test IdleCam | stale exe; after rebuild 16/16 ok | - |

## Final table (PC24; checks / failures / mismatches)
fixed_oracle 16/0/0, game_oracle 44/0/0, gameplay_oracle 311/0/0, logic_oracle 224/0/0, physics_oracle 107/0/0 (PC24 74 strict = 74 NaN-payload-only rows, excluded as in the runner), pool_oracle 1900009/0/0, review_oracle 42/0/0,
rw_atomic_clump 274, rw_custom_pipelines 137, rw_engine 93, rw_frame_camera 100, rw_im2d_im3d 117, rw_matfx_pipeline 273, rw_matfx_uvanim 151, rw_math_stream 139, rw_pipeline 541, rw_platform 55, rw_raster_image 325,
rw_renderstate 373, rw_rtanim_rtquat 4835, rw_skin_cpu_oracle 240000, rw_skin_hanim 166, rw_skin_hw_oracle 27800, rw_skin_pipeline 52, rw_skin_vs 8987, rw_strip 46, rw_texture 103, rw_world_geometry 166,
script_oracle (base, g13_15, g16, g17, g18, g19, g20_21, g22, g23, g24, g25_26) 80/47/47/17/51/56/31/75/58/70/58, world_oracle 116, x87_oracle 67 — all 0 failures, 0 mismatches. Wall time of the full run ~1 h 50 min (build included).

## Commits
e8a8d734 (rw_* build + logic timeout), cdbb35f1 (ChooseCarModelToLoad), 4c9d3671 (Quaternion::Get matrix + x87 parser), e869a888 (physics ClearScanCodes), 39f7fdbd (g16 CTask dtor), runner pool parser commit (last).

## Notes
* Foreign work (A2 globals codemod, Audio/…, BUG2 NanGuard) was dirty in the tree during the run; none of it was touched or committed. The run built with it and it compiled.
* Lesson for the brief: test targets link with /FORCE:UNRESOLVED, so every unresolved symbol that a newly out-of-line function/dtor introduces shows up only at run time as "EIP=00400000". Check `NOT ORACLE-ABLE ... EIP=00400000` first for a missing stub.
