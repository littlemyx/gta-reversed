# ASM-S99 (CVehicleModelInfo / CWeaponModelInfo, phase 3 tier B) - 2026-10-30
Result: 39 rows compared against the exe, 35 MATCH, 4 FIXED. Table: .notes/asm_sweep/S99.result.tsv (asm range, note, fix commit per row).

Fixes (all in source/game_sa/Models/VehicleModelInfo.cpp):
- SetAnimFile 0x4C7670 (940200d5a): the "null" test is the CRT _stricmp (0x8229B6), the port used strcmp.
- PreprocessHierarchy 0x4C8E60 (20dbb43f2): the first clone of the main wheel atomic gets SetAtomicRenderCallback(RenderWheelAtomicCB) (exe 0x4C9215); the port skipped it.
- SetEnvironmentMapCB 0x4C8840 (c028c5395): the disable test compares the whole data dword with 0xFFFF, not its low word.
- LoadVehicleColours 0x5B6890 (f07e98790): red/green/blue sscanf slots persist across carcols lines (exe frame slots), so a partially parsed line (malachite poly) keeps the previous green/blue; removed the port-only 10-blank cap on leading whitespace.

MATCH (35): CreateInstance, SetClump, DeleteRwObject, SetAtomicRenderCallbacks and all SetAtomicRendererCB variants, SetVehicleComponentFlags, GetWheelPosn, ChooseComponent/ChooseSecondComponent (+ global ChooseComponent, jump table 0x4C802C decoded), SetRenderPipelines, SetEditableMaterials(CB), env-map helpers, AssignRemapTxd, SetupCommonData, SetDirtTextures, LoadVehicleUpgrades, LoadEnvironmentMaps, CWeaponModelInfo::SetClump, and the rest (see table).

Tests (worktree C:/gta-wt/s99 at the fix tip):
- wave2_oracle_test (StandaloneRW) vs gta_sa_compact.exe: 1142 functions, mismatches PC24 0 / 0, PC53 0 / 0.
- gameplay_regress.sh -b StandaloneRelease combat missions: combat PASS=14, missions PASS=11, TOTAL 25 PASS.
- PortableWin64 flags (cl /Zs, x64) on VehicleModelInfo.cpp: compiles clean. FileLoader.cpp not touched.
- Not run: soak (not requested for this slice; code paths are vehicle model load / colours only).
