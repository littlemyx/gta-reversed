# ORACLE-W3-RENDER report

Agent ORACLE-W3-RENDER (rules v3), 2026-10-21 02:15 - ~02:45 WEST, worktree C:/gta-wt/oracle-w3-render (branch oracle-w3-render, base 8e88f3ef). New target `wave3_render_oracle_test` (W2 harness: deterministic arena, whole exe data image + a 16 KB fixture block compared after every case), rows in `tests/standalone/wave3_oracle_render.inc`, run with `W2_PORT_FAULT_IS_DIFF=1`.
**38 functions, PC24 0 mismatches, run time ~1 min 40 s** (PC53 1772 informational: x87 extended precision; the game runs PC24). Dev switch `W3R_DIV=n` divides the case counts.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 4685 | **4724** |
| Renderer.cpp / PostEffects.cpp / GrassRenderer.cpp / 2dEffect.cpp uncovered A+B | 10 / 9 / 9 / 9 | **0 / 0 / 0 / 0** |
| port bugs fixed (one commit each) | | **2** |

## Fixtures and stubs
Entities (6 CEntity/CPhysical blobs with random flags, model index 0..3 -> model info blobs through the exe's `ms_modelInfoPtrs`, CPlaceable matrix null or a fixture matrix), the five sector scan lists of one sector (0..3 nodes each, entities may repeat, scan code / area code mixed equal / off by one / random), the visible entity / LOD counters, the LOD render list pointers (inside `PC_Scratch`), RpLight blobs + all light colour globals (same block as ORACLE-W3-VIS), PostEffects flags + day-night balance globals, a CPed blob (dead / alive, death time near and far from now), a geometry blob with the 2dEffect plugin at a random offset (null / non-null effect entries), triangle vertices + out vector. Unlinked callees are stubbed on both sides and record (kind, pointers, float bits): `RpLightSetColor` 0x751A90, `CVisibilityPlugins::InsertEntityIntoSortedList` 0x734570, `CPointLights::GenerateLightsAffectingObject` 0x6FFBB0 / `RemoveLightsAffectingObject` 0x6FFFE0, `CCarFXRenderer::SetFxEnvMapLightMult` 0x5D5BA0, `CPhysical::GetLightingFromCol` 0x5447B0 (thiscall); app_light.cpp and PPTriPlantBuffer.cpp are linked for real.

## Port bugs fixed (rows failed pre-fix; counts = failing PC24 cases of the first run on the unfixed code, logs C:/gta-scratch/w3render/r_pre1.log / r2.log)
| function | asm | bug | pre-fix | commit |
|---|---|---|---|---|
| CPostEffects::InfraredVisionStoreAndSetLightsForHeatObjects | 0x701320 | exe scales the death-time difference by the float 0.0001f (0x858FC4: fild + fmul), the port divided by 10000.0f (float rounding of the heat colour) | 13/1500 | 2bd9f6bf |
| CEntity::IsInCurrentArea (inline; seen in CRenderer::ScanSectorList_ListModels 0x5535D0 / ListModelsVisible 0x553650) | 0x5536A1 | exe `movzx edx, al` zero-extends the area byte, the port compared the signed `eAreaCodesS8` (differs for codes >= 0x80; no real entity has one, fix is for faithfulness) | 84/1500 | 6cbe5dd1 |

## Rows that pass without a change
Renderer: RemoveVehiclePedLights, AddEntityToRenderList (stubbed InsertEntityIntoSortedList true / false, LOD-children / underwater / draw-last / distance-fade bits), GetLodRenderListBase, GetLodDontRenderListBase, ResetLodRenderLists, AddToLodRenderList, AddToLodDontRenderList, SetupLightingForEntity (scorched and normal path; PC24 0 incl. the float chain `(lighting*0.95+0.05)*generated`), SetupScanLists (sector coordinates incl. full int range), ScanSectorList_ListModels, ScanSectorList_ListModelsVisible (class C, bonus; 1228 of 3000 cases fault identically in port and exe inside CEntity::GetIsOnScreen with the fixture camera and are skipped);
PostEffects: FilterFX_StoreAndSetDayNightBalance, FilterFX_RestoreDayNightBalance, ScriptCCTVSwitch, ScriptResetForEffects, NightVisionSetLights, InfraredVisionSetLightsForDefaultObjects, InfraredVisionRestoreLightsForHeatObjects, SetSpeedFXManualSpeedCurrentFrame (NaN / inf / -0 included);
GrassRenderer: Initialise, Shutdown, GetPlantModelsTab / SetPlantModelsTab (type 0..5 and full u32), SetCloseFarAlphaDist, SetCurrentScanCode, SetGlobalCameraPos, SetGlobalWindBending, GenPointInTriangle;
2dEffect: Roadsign_GetNumLines / NumLetters / PaletteID FromFlags, RpGeometryGet2dFxCount, RpGeometryGet2dFxAtIndex, t2dEffectPluginConstructor / CopyConstructor, Rwt2dEffectPluginDataChunkWrite / GetSize callbacks.

## Observed, not changed
* ScriptCCTVSwitch 0x7011B0: the exe stores the argument byte as is, the port a normalised bool; differs only for a non-canonical bool (not 0/1), which no caller passes: the row uses 0/1.
* GenPointInTriangle 0x5DAB00 is the exe's inlined helper (void, eax is left holding v1); the port returns the out vector: return value not compared, the written vector is identical.
* PC53 differences (x87 extended precision) in SetupLightingForEntity, GenPointInTriangle, Infrared row: informational.

## X87-TRIG note: CPostEffects::SetupBackBufferVertex 0x7043D0 / ImmediateModeFilterStuffInitialize 0x703CC0 `std::bit_ceil` vs the exe (NOT FIXED, evidence only)
Disassembly of 0x703EB5..0x703F3A: for each raster side w the exe computes `n = _ftol(ln(w) / ln(2)) + 1` (fldln2; fild w; fyl2x; fdiv; truncating _ftol of the EXTENDED quotient) and the size `_ftol(pow(2.0, n))` = 2^n. That is the NEXT power of two **strictly above** w for exact powers of two as long as the extended quotient is exactly k (hand analysis: ln2*k rounded to 64 bits divided by ln2 rounds to exactly k for k=10, so w=1024 -> 2048), whereas the port's `std::bit_ceil(1024)` = 1024 (and bit_ceil(1)=1 vs exe 2). Non-power-of-two sizes (1000 -> 1024, 960 -> 1024, 600 -> 1024) agree. Both functions are class D (CRT pow, RW calls), so no row; consequence of the difference: front-buffer raster is half as big and fFrontBufferU2/V2 differ for 1024-, 512-, 256-wide/high back buffers. Next step (CONTINUE): add a naked-asm helper `ExeRasterSize(int w)` next to x87::ln (fldln2; fld 2.0; fyl2x; fldln2; fild; fyl2x; fdiv st(1); fnstcw/RC=trunc/fistp), check all w in 1..8192 against a host replay of the exe instruction sequence, then use it at PostEffects.cpp:94-95 and 310-311.

## Not covered
Class C/D/S of the four files (Render / Update / DrawTriPlants / AddTriPlant / Radiosity ... : RW rendering, file IO, heap); C2dEffect::Shutdown / t2dEffectPluginDestructor / Rwt2dEffectPluginDataChunkReadCallBack (D), PluginAttach / DestroyAtomic (C, RW plugin registration / frames).

## Tests run (worktree, final tree)
wave3_render_oracle_test 38 functions PC24 0, exit 0 (~1 min 40 s, log C:/gta-scratch/w3render/r3.log). Entity.h changed (IsInCurrentArea inline): the orchestrator's full run_all_tests covers the other targets that include it.
