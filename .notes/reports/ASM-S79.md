# ASM-S79 report

Base 776f6b8e1 (origin/reverse/interior_c). Result table: `.notes/asm_sweep/S79.result.tsv` (50 rows).

Verdicts: 43 MATCH, 7 FIXED, 0 UNSURE, 0 NOT_REACHED. Several "trivial" rows were not trivial (SetGroupData, SetBreakInfo, SetCutsceneAnim, IsTargetValid, SetObjectData, GetForMap, GetRoadBlockNodeInfo, GetNearestFreeAccident*, GetTextureFromTxdAndLoadNextTxd) and were traced in full.

| row | verdict | commit |
|---|---|---|
| BreakObject_c::SetGroupData | FIXED (rand-add association, RwV3dNormalize) | 0c49e4ec1 |
| CCutsceneMgr::SetCutsceneAnim | FIXED (clears IS_PLAYING; Start() set it) | 1723602d0 |
| CIdleCam::IsTargetValid | FIXED (NaN slerp-time arm) | 376a3bdbd |
| COnscreenTimer::SetClockBeepCountdownSecs | FIXED (4 x 0x40-byte stride walk) | 2e119cc46 |
| CAccidentManager::GetNearestFreeAccidentExceptThisOne | FIXED (squared distance compare) | f571c9f6c |
| GetAtomicWithName (ClothesBuilder) | FIXED (first match wins) | 0451bc813 |
| CEntryInfoNodePool 0x536D10 | FIXED (hook label GetAt -> New) | 604e9a300 |

## Notes

- SetBreakInfo: FramesToLive = 256 - ftol(rnd * -32) is the same as the port's 256 + int(rnd*32); colour/texcoord/render-info stores agree.
- Not changed (benign): CEscalator ctor/dtor (port zero-inits the whole matrix and runs ~EntityRef at exit), CPatrolRoute ctor (port zero-inits the points), CConversations::IsConversationAtNode (exe upper-cases the caller's string in place, port uses stricmp), CPool::GetAtRef bounds check.
- Gate: StandaloneRelease + StandaloneRW built after the last edit; wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=compact exe) 1142 functions, PC24 0 / PC53 0 mismatches; combat regress PASS=14; v6 pre-commit grep clean; PortableWin64 (x64 cl, real compile, no PCH) of BreakObject_c / CutsceneMgr / IdleCam / OnscreenTimer / AccidentManager / ClothesBuilder / Pools / InjectHooksMain: 0 errors.
