# ASM-S33 (WaterLevel.cpp; 26 rows)

Partial. 8 rows compared: 5 DEVIATION fixed, 3 MATCH; 18 NOT_REACHED (budget). Result table: S33.result.tsv.
GetWaterLevel itself is not in the slice (done earlier); its helpers GetWaterDepth and GetGroundLevel were done first and match.

| Function | Result |
|---|---|
| SetUpWaterFog 0x6EA9F0 | The exe returns without registering the fog when the water lookup succeeds with big == 0 and small == 0 (port registered it with z 0); `gbPlayerIsInsideWaterFog` is only ever set here (port assigned it every call, clearing it); height + z sum unrounded. |
| PreRenderWater 0x6EB710 (flow update) | step = `ts*0.001f` (port `ts/1000`), delta / sum unrounded, negative delta subtracts and zero / NaN add (port `copysign`). |
| ScanThroughBlocks 0x6E6D10 | scan points are `x*0.002f + 6` with the product unrounded (port `x/500 + 6`). GetCorners3D order / CWorldScan not compared. |
| SetCameraRange 0x6E9C80 | `(cam -/+ 48) * 0.5` goes to floor / ceil unrounded; the port rounded the difference to float first. |
| WaterLevelInitialise 0x6EAE80 | the block -> poly table (0xC21B70, 0x120 bytes) is cleared first (port left stale entries on re-init). The inlined loader reads every line of the real water.dat (301 quads, 6 triangles) the same as the port's stream parser; texture setup equal. |
| MATCH | Shutdown, GetWaterDepth, GetGroundLevel. |
| NOT_REACHED | all Render* / Split* / RenderBoatWakes / RenderWakeSegment / RenderWaterFog / RenderWater / RenderHighDetail* rows. |

## Tests
StandaloneRelease gta_reversed.exe rebuilt (0 errors). Pre-commit grep clean. wave2_oracle_test (RW_EXE_ORACLE and GTA_EXE = gta_sa_compact.exe): 1142 functions, 0 mismatches PC24 and PC53. gameplay_regress combat (-b worktree StandaloneRelease): PASS=14. Not run: soak, PortableWin64.
