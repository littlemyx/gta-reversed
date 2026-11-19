# ASM-P4R2ac

Part A: d56535783 (CPhysical::ApplyTurnSpeed) CONFIRMED against exe 0x542E20-0x542FD6 (call argument order of 0x59C730 / 0x59C790, flag bits, reset branch, position update).

Part B: 11 of the 12 open P4R2ab rows compared (Fx rows and the "no verdict yet" sweep NOT_REACHED, budget): 8 MATCH, 3 FIXED.

| fix | commit |
|---|---|
| CAutomobile::ProcessAutoBusDoors: right door ratio 1-(t+500)/500 when open (port passed 0) | 7d4d45c30 |
| CAutomobile::ScanForCrimes: dist^2 sum order (z+x)+y | b9c8b1d19 |
| CWeapon::Update: anim-less reload audio skipped when rload == 0 | c7642d1d3 |

Note: the exe runs with the FPU at PC=24, so "unrounded" differences are float-vs-x87 noise; only control flow / operand order was judged.
Rows: .notes/asm_sweep/P4R2ac.result.tsv
