# ASM-S11b (S11b, S12b and S33f ran in one agent; full text in ASM-S12b.md)
Result: S12b 6 of 10 compared in full (1 FIXED, 5 MATCH), 4 NOT_REACHED (CTaskSimpleGangDriveBy x4); S11b 0 of 9 (all NOT_REACHED); S33f 0 of 1 (NOT_REACHED). Stopped at the ~110k-token landing line. Tables: S12b/S11b/S33f.result.tsv.
Fix:
- 765d7537 CTaskSimpleDead::ProcessPed (0x630600-0x630F0F): bloody-footprint radius after 7 s is 0.75000006f (exe data 0x85B900 = 0x3F400001), the port used 0.75f; blood puddle in-water test is `z <= level` (exe fcomp + jp arm: z > level and NaN skip the flag), the port's `!(z > level)` set it for NaN.
MATCH (full asm read): CTaskSimpleFight::LoadMeleeData (file parse incl. scanf formats, 1/30 scale, defaults, 13 combos / 7 hit offsets), FightHitPed, FightStrike, StartAnim, ProcessPed (0x629920; commit aadd863c only respells 3 bare `(double)` casts as x87d, no behaviour change).
Open: GangDriveBy FireGun 0x627CC0, ProcessAIPed 0x627600, ProcessAimIK 0x628350, ProcessPed 0x62D3B0; all of S11b (CarSetPedInAsDriver/Passenger/SlowDraggedOut ProcessPed, CTaskSimpleClimb x6); S33f RenderBoatWakes 0x6ED9A0 (+ CBoat::RenderWakePoints).
Note: the remaining `(double)` casts in TaskSimpleFight.cpp IsTargetInRange/GetAvailableComboSet (lines ~436-529) predate this slice and were not touched (outside the compared range).
Consumer audit: no layout/signature change.
Gate: see the final line of the handback (builds, wave2_oracle_test, combat regress, PortableWin64 compile).
