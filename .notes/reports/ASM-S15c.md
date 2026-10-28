# ASM-S15c (UseGun::ProcessPed, Swim x2, LineUpPedWithCar x3)

Scope: .notes/asm_sweep/S15c.tsv, 6 rows. Compared 2 (1 MATCH, 1 FIXED), NOT_REACHED 4 (budget). Result table: S15c.result.tsv.

| Function | Verdict | Commit |
|---|---|---|
| UseGun::ProcessPed (0x62A380) | MATCH, all 0x62A380-0x62AF3E: IK reset, weapon-info init/change, attack-button decay, arm-blocked and pistol-whip aborts, per-command anim handling (reload, whip hit + aim rotation, aim/fire/burst, continuous fire, twin pistols, expand, breakout), aim-skip angle test (115/-130 deg, dead-ped bonus, up-dot), AimGun/AbortIK choice, SetMoveAnim gate, NextCmd clear; all signed/NaN compares and x87 chains agree | - |
| Swim::ProcessSwimmingResistance (0x68A1D0) | FIXED: underwater-sprint vector op order ((cos*fwd)*shift.y; z = sin*y + (fwd + right.z*x), then +0.01); drag keeps pow(0.9,ts) unrounded; GetWaterLevel probed at the ped's own z (port added ts*speed.z); 0.785398185 (was 0.7854); state-changer growth ts*0.002 (was ts*0.02*0.1); 4/pi constant; unknown swim states fall through to the drag instead of returning | 9a77e30b |

Open (not compared): Swim::ProcessControlInput (0x688A90), LineUpPedWithCar GetLocalPositionToOpenCarDoor (0x64FC10), GetPositionToOpenCarDoor (0x650A80), ProcessPed (0x6513A0).
Tests: wave2_oracle_test 1142 functions, PC24 0/0, PC53 0/0 (the swim fix is not covered by an oracle row); combat regress PASS=14.
