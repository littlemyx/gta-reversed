# ASM-S15d (Swim::ProcessControlInput, LineUpPedWithCar x3)

Scope: .notes/asm_sweep/S15d.tsv, 4 rows. Compared 3 (2 MATCH, 1 FIXED), NOT_REACHED 1 (budget). Result table: S15d.result.tsv.

| Function | Verdict | Commit |
|---|---|---|
| Swim::ProcessControlInput (0x688A90) | FIXED (all 0x688A90-0x68961E compared, jump table 0x689624 decoded): underwater mouse turn subtracts the unrounded fpatan result from the heading (the exe does `fst`, not `fstp`); move-blend compare takes the assignment on a NaN difference; pow(0.95/0.92, ts) kept unrounded in the three damping multiplies (same pattern as S15c) | f73e09fc |
| LineUp GetLocalPositionToOpenCarDoor (0x64FC10) | MATCH, all 0x64FC10-0x6500E6 (anim-id tables from the exe byte table incl. the documented 0x64FCA2 bug, per-door seat arithmetic, locked-door blend, bike path) | - |
| LineUp GetPositionToOpenCarDoor (0x650A80) | MATCH | - |
| LineUp ProcessPed (0x6513A0) | NOT_REACHED (1309 asm lines, budget) | - |

Open: CTaskUtilityLineUpPedWithCar::ProcessPed (0x6513A0). Portable note: the pow multiplies use `(float)(pow(double) * (double)x)` as in S15c (no F24 helper is used anywhere yet).
