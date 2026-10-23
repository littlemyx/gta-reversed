# ORACLE-W3-MISC report

Agent ORACLE-W3-MISC (rules v3), 2026-10-20 17:54 - 18:15 WEST, worktree C:/gta-wt/oracle-w3-misc (branch oracle-w3-misc, base a8b507e2). New target `wave3_misc_oracle_test` (W2 harness: deterministic arena, whole exe data image + a 8 KB fixture block compared after every case), rows in `tests/standalone/wave3_oracle_misc.inc`.
**32 functions, PC24 0 mismatches, run time ~1 min 55 s** (PC53 0 as well). Development switch `W3M_DIV=n` divides the case counts.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 4550 | **4581** |
| Localisation.cpp uncovered (A/B) | 17 (GermanGame was covered) | **0** |
| TagManager.cpp uncovered (A/B + IsTag) | 14 | **SetupAtomic (B), RenderTagForPC (C), Save, Load (S)** |
| port bugs fixed (one commit each) | | **4** |

## Fixtures
Localisation: the three bool globals 0xB9B7EC/ED/EE (0 / 1) and FrontEndMenuManager's language byte 0xBA67CC (0..6 the menu values, 7..255 as a damaged gta_sa.set could give), via the exe addresses on both sides; `Metric` is also checked for the language-byte layout (static_assert offsetof(CMenuManager, m_nPrefsLanguage) == 0xBA67CC - 0xBA6748).
TagManager: the tag array at 0xA9A8C0 (all 150 slots valid, `ms_numTags` 0..13 / 149 / 150, tag entities distinct or with repeats, alphas at the 227/228/229 boundary mixed with random), `ms_numTagged` consistent and inconsistent; 12 raw CEntity blobs (position in the placement or in a CMatrix reached through m_matrix, RpAtomic blob or none, model index 0 = a valid tag model info), the atomics with the three CVisibilityPlugins plugin offsets randomised (0x40..0x7C), the exe's `ms_modelInfoPtrs` entries 0..3 pointing to fake model infos whose vtable slots GetRwModelType / AsAtomicModelInfoPtr are host stubs (IsTag, a class C function, is covered this way), TheCamera wide-screen flag, area rects around the entity positions with entities exactly on the borders and game-range floats mixed with NaN / inf / denormal / huge. `CGarages::TriggerMessage` (0x447B80) is `oracle::Patch`ed to a recording stub; the port's symbol of the same name is defined in the test TU with the same body (compared: key string hash, arguments, call count). Entities not in the tag array are not used for the functions where the exe dereferences address 4 (GetAlpha/SetAlpha of an unknown atomic-less entity).

## Port bugs fixed (rows failed pre-fix; counts = failing PC24 cases of the first full run on the unfixed code, log C:/gta-scratch/w3misc_run1.log + w3misc_run.log)
| function | asm | bug | pre-fix | commit |
|---|---|---|---|---|
| CTagManager::ShutdownForRestart | 0x49CC60 | exe clears Alpha of the first ms_numTags entries; port cleared all 150 | 140/150 | fde4b475 |
| CTagManager::AddTag | 0x49CC90 | exe stores Entity + Alpha byte; port assigned a tTagDesc temporary (3 padding bytes of stack garbage written into the tag array image) | 300/300 | 13d50c4e |
| CTagManager::FindTagDesc (users GetAlpha 0x49CF90, SetAlpha 0x49CEC0) | 0x49CCB0 | exe scans last to first (the LAST entry of a repeated entity wins); port first to last | 37/1500 (+49/1500, 216/2500 in the users) | 68ed92a0 |
| CTagManager::SetAlphaInArea | 0x49CFE0 | exe sets the tag's Alpha for every tag in the area and guards only the atomic's SetUserValue; port skipped the whole tag (alpha and counters) when the entity had no atomic (streamed-out tag) | 1404/3000 | 327f1197 |

Rows that matched without a fix: all 18 Localisation functions, Init, GetTagPos, IsTag, SetAlpha/GetAlpha(RpAtomic), GetPercentageTagged (incl. 0 tags = NaN/inf -> 0x80000000), UpdateNumTagged, GetPercentageTaggedInArea (float and integer percentage agree on every case, also with 0 tags in the area).

## Skips
- CTagManager::SetupAtomic 0x49CE10 (B): reads RpGeometry / RpMaterial through the RenderWare layout; the port is compiled against the librw layout, the exe's fields are at other offsets (read by hand: pipeline store, flags |= 0x40, SetUserValue(0) - equal).
- CTagManager::RenderTagForPC 0x49CE40 (C): calls the atomic's render callback through the RW object.
- CTagManager::Save 0x5D3D60 / Load 0x5D3DA0 (S): CGenericGameStorage work buffer (file IO state); Load stores ms_numTags into the buffer instead of loading (kept, noted in the source).
- CTagManager::GetNearestTag was already covered (script_oracle_g19_test).
- Not reproducible by a fixture: IsTag's `movsx` of the model index (the exe sign-extends m_nModelIndex, the port reads it unsigned) matters only for index >= 0x8000, impossible with 20000 model slots.

## Second pass (ORACLE-W3-MISC2, rules v4, 2026-10-23 18:32 - 20:0x WEST, worktree C:/gta-wt/oracle-w3-misc2)
Recount at the start: Grass (all 12 A/B covered), 2dEffect (all A/B covered), PostEffects / Renderer A/B were already done by W3-RENDER; what was left: platform.cpp / WinPs.cpp (A/B), CustomBuildingRenderer (B) and the C-class Renderer functions.
| | before | after |
|---|---:|---:|
| whole game covered | 4815 | **4838** (+23; +1 is Shadows.cpp RenderExtraPlayerShadows, an over-approximation: the address is only Patch()ed) |
| platform.cpp | 0/30 | 11 |
| WinPs.cpp | 0/22 | 3 |
| CustomBuildingRenderer.cpp | 0/7 | 2 |
| Renderer.cpp | 11/35 | 17 |
Rows (tests/standalone/wave3_oracle_misc2.inc in wave3_misc_oracle_test, 46 functions, PC24 0; tests/standalone/wave3_oracle_render.inc TestW3RenderRenderer2 in wave3_render_oracle_test): RsKeyFromScanCode, RsPathGetSeparator, RsRegisterImageLoader, RsSetDebug, psPathGetSeparator, psInstallFileSystem, psGetMemoryFunctions, RsInputDeviceAttach, Rs{Keyboard,Mouse,Pad}EventHandler (recording handler stub), rsPreInitCommandLine (eax thunk), CCustomBuildingRenderer::UpdateDayNightBalanceParam / Update, CRenderer::ShouldModelBeStreamed, ScanSectorList_RequestModels (+ ScanPtrList_RequestModels 0x555680), RenderOneRoad, PreRender, ProcessLodRenderLists. Stubs: CStreaming::RequestModel 0x4087E0 / CHeli::SpecialHeliPreRender / CShadows::RenderExtraPlayerShadows (render target links Streaming.cpp, so /FORCE:MULTIPLE lets the TU's recording definition win); entity / model-info fake vtables.
### Port bugs (one commit each)
| function | exe | bug | pre-fix | commit |
|---|---|---|---|---|
| CRenderer::ProcessLodRenderLists + CEntity::CanLodChildrenRender | 0x553770 | do-while written as `while(false)` (whole phase 2 skipped); `int8 != 128` always true | 1522/4000 (118 after the loop fix alone) | 8591eef0 |
| CRenderer::ShouldModelBeStreamed | 0x554EB0 | fcompp + `test ah,0x41; jne` passes on NaN (`!(mag > sum)`), port `<=`; sum order dz,dy,dx | 2/5000 + 3/4000 (ScanSectorList_RequestModels), both edits together | d4e2e9aa |
| CRenderer::SetupEntityVisibility | 0x554230 | draw-last test used m_bWasPostponed (bit 6) instead of m_bDrawLast (bit 14), two sites | no row (asm only) | adef2d25 |
| CRenderer::RenderEverythingBarRoads | 0x553AA0 | alpha peds (type 3, clump alpha != 255) not inserted into the sorted list | no row (asm only) | d38c4c81 |
The last two were found by a read-only asm-vs-source helper (SetupEntityVisibility / SetupMapEntityVisibility / SetupBigBuildingVisibility / ScanSectorList / ScanBigBuildingList / RenderEverythingBarRoads / RenderFirstPersonVehicle; the other five reported no deviation) and re-verified by me against the asm (0x5543D6 / 0x554523 `test ah,0x40`; 0x553B28-0x553BA9). No row because the render states go through the RW engine table (0xC97B24) and SetupEntityVisibility needs camera / mirror / area / time-model fixtures. Remaining low-priority note from the helper: SetupEntityVisibility / SetupBigBuildingVisibility / RenderEverythingBarRoads distance sums (dz2+dy2+dx2 order) differ from DistanceBetweenPoints in the last float bit.
### Skips
platform.cpp RsTerminate / psTerminate (SDL3 build: port deletes RsGlobal.ps, exe `ret`), RsPathnameCreate / psPathnameCreate / Destroy (allocator behind the RW engine table), RsRwTerminate, VideoMode / VideoPlayer / WinInput / WinMain (DirectInput / DirectShow / window class D), psDebugMessageHandler 0x745540 (the exe function is a (type, msg) RwDebug handler that calls OutputDebugStringA twice; the port's function has another meaning, not called by the port itself), CustomBuildingRenderer::IsCBPCPipelineAttached / AtomicSetup (RW atomic layout), CarFXRenderer thin wrappers (RW), Renderer RenderFadingIn* / RenderRoads / RenderOneNonRoad / RenderEverythingBarRoads / ScanSectorList / SetupMapEntityVisibility (RW engine table / large C closure).
### Game check
NOT done by the agent: the orchestrator's gate held build.lock with a 40-minute Release build from 19:04 to ~19:45 (this agent's Release build could not start, a full Release build in a fresh worktree does not fit the 100 min cap). The fixes are render / LOD-list code (ProcessLodRenderLists also calls CStreaming::RequestModel), so soak.sh + gameplay_regress should be run by the gate on the landed tree. The final tree (Entity.h included) was rebuilt and both targets re-run: wave3_misc_oracle_test 46 functions PC24 0, wave3_render_oracle_test 43 functions PC24 0 (PC53 informational).
