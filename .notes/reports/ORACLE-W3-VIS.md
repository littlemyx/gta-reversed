# ORACLE-W3-VIS report

Agent ORACLE-W3-VIS (rules v3), 2026-10-20 12:51 - 13:32 WEST, worktree C:/gta-wt/oracle-w3-vis (branch oracle-w3-vis, base 54db0151). New target `wave3_vis_oracle_test` (W2 harness: deterministic arena, whole exe data image + a 16 KB fixture block compared after every case), rows in `tests/standalone/wave3_oracle_vis.inc`.
**63 functions, PC24 0 mismatches, run time 3 min 7 s** (PC53 1647 informational: x87 extended precision; the game runs PC24). Development switch `W3V_DIV=n` divides the case counts. `wave3_models_oracle_test` (links VisibilityPlugins.cpp): 95 functions, PC24 0.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 4462 | **4525** |
| VisibilityPlugins.cpp uncovered A / B | 12 / 27 | **0 / 0** |
| app_light.cpp uncovered A / B | 1 / 22 | **0 / 0** |
| port bugs fixed (one commit each) | | **8** (+1 hook address) |

## Fixtures
RW plugin objects (atomic / clump / frame) as raw blobs with the plugin offsets `ms_*PluginOffset` randomised per case (0x40..0x7C), matrices with game-range floats mixed with NaN / inf / denormal / huge, entity + model info + col model blobs (draw distance, far clip, LOD multiplier, alpha, lod entity), the five alpha link lists + the weapon-ped list over 6-node pools with random used / free split (full lists included, sorted distances with ties), the camera + frame in BOTH RenderWare layouts for SetRenderWareCamera (exe layout and librw layout, pointer results normalised to "points at the frame's position"), RpLight blobs + all light colour globals + CTimer / CClock / CTimeCycle / CPostEffects globals for the lights. The private members of CVisibilityPlugins are reached by the explicit-instantiation access trick (functions) and StaticRef at the exe addresses (data). Port function pointers stored in fixture data (list node callbacks, clump visibility callback) are mapped to the exe addresses before the compare.
RenderWare is not run: the exe's `RpLightSetColor` (0x751A90) and `RpClumpForAllAtomics` (0x749B70) are `oracle::Patch`ed to recording stubs, the port's fakerw entry points of the same names are defined in the test with the same bodies (the clump stub also runs the callback on up to two fixture atomics). Recorded: order, light pointer and the four colour floats of every `RpLightSetColor`; callback (mapped), clump and data of every `RpClumpForAllAtomics`.

## Port bugs fixed (rows failed pre-fix; counts = failing PC24 cases of the first full run on the unfixed code, log C:/gta-scratch/w3vis/r_full_pre.log)
| function | asm | bug | pre-fix | commit |
|---|---|---|---|---|
| CVisibilityPlugins::GetDistanceSquaredFromCamera(RwV3d*) (+ the RwFrame* overload, same code shape) | 0x732CC0 / 0x732C80 | exe sums (z*z + y*y) + x*x; port x*x+y*y+z*z (float rounding) | 338/1500 | f84048fc |
| CVisibilityPlugins::GetDotProductWithCameraVector | 0x7326D0 | exe sums each of its dot products as (y*y' + x*x') + z*z' | 632/4000 | c62bc2bc |
| CVisibilityPlugins::InsertEntityIntoSortedList | 0x734570 | exe: `movsx` entity model index vs `movzx` MI_GRASSHOUSE; the port compared zero-extended (differs for index 0xFFFF == the default MI_GRASSHOUSE) | 196/2000 | dc00cac8 |
| CVisibilityPlugins::CalculateFadingAtomicAlpha | 0x732500 | min(drawDist, radius) is `(dd < R) ? dd : R` in the exe (a NaN draw distance gives the radius); `std::min(dd, R)` keeps the NaN | 1/4000 | 3e9c033b |
| Light::SetFlashyColours, SetFlashyColours_Mild | 0x735AB0 / 0x735B40 | exe copies only r,g,b of DirectionalLightColourForFrame; the port copied the alpha too (recorded RpLightSetColor alpha + global differ) | 506/1000 each | a64a12a4 |
| Light::SetAmbientColoursToIndicateRoadGroup | 0x735C90 | table index is `idx % 7` (idiv); the port used `idx & 8` (`& std::size`): only idx 0 and 8.. ever matched | 311/400 | a0fae8ce |
| Light::SetLightColoursForPedsCarsAndObjects | 0x735D90 | green / blue ambient add m_BrightnessAddedToAmbientGreen (0xB79E34) / Blue (0xB79E30); the port added the Red one to all three | 1399/1500 | ea90a464 |
| Light::GetDayNightBalance | 0x6FAB30 | the two ramps multiply by the float 1/60 (0x859044), the port divided by 60 | 105/3000 | 8bcf0ed5 |
| hook address (not a behaviour bug) | 0x732307 -> 0x7322F0 | SetClumpForAllAtomicsFlag was hooked on the bare `ret` that follows the function (0x732307); the function starts at 0x7322F0 | row passes | 249b02a5 |

## Rows that pass without a change
Plugin ctors / copy ctors / dtors (Atomic, Clump, Frame), DefaultVisibilityCB, SetAtomicFlag, ClearAtomicFlag, SetAtomicId, SetUserValue, SetModelInfoIndex, GetAtomicId, GetUserValue, GetModelInfoIndex, GetModelInfo (ids -1..63 and random 15 bit), Get/SetFrameHierarchyId, Get/SetClumpAlpha, GetClumpModelInfo, SetClumpForAllAtomicsFlag, ClearClumpForAllAtomicsFlag (class C, covered as a bonus), setClumpForAllAtomicsFlagCB, clearClumpForAllAtomicsFlagCB, SetAlphaCB, InitAlphaAtomicList, InitAlphaEntityList, Insert{Object,EntityIntoUnderwater,AtomicIntoReallyDrawLast,EntityIntoReallyDrawLast}List (sorted insert, ties, NaN distances, full pools), AddWeaponPedForPC, SetRenderWareCamera (squared LOD / fade distances via std::powf match the exe at PC24, both camera layouts);
Light: Activate / DeActivateDirectional, LightsEnable, WorldReplaceScorchedLightsWithNormal, RemoveExtraDirectionalLights, SetAmbientAndDirectionalColours, ReSetAmbientAndDirectionalColours, SetBrightMarkerColours, SetDirectionalColours, SetFullAmbient, both SetAmbientColours, the four infrared / night-vision light setters.

## Observed, not changed
* WorldReplaceNormalLightsWithScorched 0x7357E0: the exe builds the `RwRGBAReal` on its stack and leaves the alpha uninitialised (RpLightSetColor copies it into the light), the port zeroes it; the recorded alpha is not compared (commit 8b74e261). Nothing reads the alpha of a light colour.
* InsertEntityIntoSortedList: the sign-extension difference needs a 16 bit model index >= 0x8000 (the invalid index 0xFFFF); no rendered entity has one, the fix is for faithfulness.
* PC53 (CRT default precision) differs in SetRenderWareCamera (powf), GetDistanceSquaredFromCamera, GetDotProduct, SetLightColoursForPedsCarsAndObjects: x87 extended intermediate precision, informational.

## Not covered (VisibilityPlugins.cpp / app_light.cpp)
Class C render callbacks and RW-calling functions (RenderEntity, RenderFading*, Render*CB, RenderVehicle*, RenderWeaponCB, SetupVehicleVariables, IsAtomicVisible, FrustumSphereCB, GetDistanceSquaredFromCamera(RwFrame*) [fixed from the asm, no row], SetClumpModelInfo, VehicleVisibilityCB, IsClumpVisible, PluginAttach, SetTextureCB, LightsCreate / LightsDestroy / SetLightsWithTimeOfDayColour); class D Initialise / Shutdown (heap); class S RenderEntity.

## Tests run (worktree, final tree)
wave3_vis_oracle_test 63 functions PC24 0, exit 0 (3 min 7 s, log C:/gta-scratch/w3vis/r5.log); wave3_models_oracle_test (the only other target linking VisibilityPlugins.cpp; app_light.cpp is linked by no other target) 95 functions PC24 0, exit 0. The orchestrator runs the full run_all_tests.
