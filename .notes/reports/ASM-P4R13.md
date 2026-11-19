# ASM-P4R13: inclusive vs strict range/box tests
Settled P4R11c UNSURE: the 12 draws are in ControlSubTask (0x662A10), not CreateNextSubTask: exe 11 GetRandomNumberInRange sites + rand(), port 9 + the SaySomethingRandom lambda duplicated + rand(): MATCH, no missing/extra draw.
All CBoundingBox / CBox / CRect box tests checked are consistent with the exe (inclusive; only NaN differs in CWaterLevel::SetUpWaterFog and a few `>=`/`<=` radius tests - ignored).
Fixed (strict compare in exe, port inclusive): CTaskComplexKillCriminal ProcessEvent (dist^2 < 625), CTaskComplexWalkAlongsidePed (dist2D^2 < 4), CTaskComplexFollowNodeRoute::MakeAbortable (speed^2 < 0.015625); CTaskComplexFollowPointRoute: fsqrt vs dot instead of mag <= dot^2.
Not reached: Bmx, GangWars, RoadBlocks, Population, PlayerPed, GameLogic, CarCtrl hits (see result.tsv). Details: .notes/asm_sweep/P4R13.result.tsv.
