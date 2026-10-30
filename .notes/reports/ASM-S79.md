# ASM-S79 report

Base 776f6b8e1 (origin/reverse/interior_c). Result table: `.notes/asm_sweep/S79.result.tsv` (50 rows).

Verdicts: 43 MATCH, 7 FIXED, 0 UNSURE, 0 NOT_REACHED. Several "trivial" rows were not trivial (SetGroupData, SetBreakInfo, SetCutsceneAnim, IsTargetValid, SetObjectData, GetForMap, GetRoadBlockNodeInfo, GetNearestFreeAccident*, GetTextureFromTxdAndLoadNextTxd) and were traced in full.

| row | verdict | commit |
|---|---|---|
| BreakObject_c::SetGroupData | FIXED (rand-add association, RwV3dNormalize) | cddd2db86 |
| CCutsceneMgr::SetCutsceneAnim | FIXED (clears IS_PLAYING; Start() set it) | ab5e06209 |
| CIdleCam::IsTargetValid | FIXED (NaN slerp-time arm) | 035b782ba |
| COnscreenTimer::SetClockBeepCountdownSecs | FIXED (4 x 0x40-byte stride walk) | e47e777b0 |
| CAccidentManager::GetNearestFreeAccidentExceptThisOne | FIXED (squared distance compare) | 97050b5a9 |
| GetAtomicWithName (ClothesBuilder) | FIXED (first match wins) | 19d247eb0 |
| CEntryInfoNodePool 0x536D10 | FIXED (hook label GetAt -> New) | e6854d552 |

## Notes

- SetBreakInfo: FramesToLive = 256 - ftol(rnd * -32) is the same as the port's 256 + int(rnd*32); colour/texcoord/render-info stores agree.
- Not changed (benign): CEscalator ctor/dtor (port zero-inits the whole matrix and runs ~EntityRef at exit), CPatrolRoute ctor (port zero-inits the points), CConversations::IsConversationAtNode (exe upper-cases the caller's string in place, port uses stricmp), CPool::GetAtRef bounds check.
- Gate: StandaloneRelease + StandaloneRW built after the last edit; wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=compact exe) 1142 functions, PC24 0 / PC53 0 mismatches; combat regress PASS=14; v6 pre-commit grep clean; PortableWin64 (x64 cl, real compile, no PCH) of BreakObject_c / CutsceneMgr / IdleCam / OnscreenTimer / AccidentManager / ClothesBuilder / Pools / InjectHooksMain: 0 errors.
