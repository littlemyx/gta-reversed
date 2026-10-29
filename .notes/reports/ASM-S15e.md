# ASM-S15e (CTaskUtilityLineUpPedWithCar::ProcessPed 0x6513A0)

Result table: .notes/asm_sweep/S15e.result.tsv. Verdict FIXED, PARTIAL on the arithmetic tails.

- Decision logic compared against all of 0x6513A0-0x652790: heading / flipped-vehicle rotation (door idx 8, 9, 18), the three anim-id jump tables (0x6527B0, 0x6528BC, 0x6528D0) decoded and every AnimationId value checked against the port enum, animProgress / zBlend per anim (AT400 override, locked-door flag bit 5), door position (type 2 = ped position, second GetPositionToOpenCarDoor(1.0) skipped for bikes / type 2), FindZCoorForPed with the +1 / -0.5 / origZ fallback, boat upside-down +1, gravity 0.008f, bike Z path (getout 1-progress, getin 2*progress capped at 1), timed offset + rotation blend with the 2pi wrap, getin / getout quaternion roles (from / to) and the t<=0 / t>=1 shortcuts, final SetMatrix vs SetPosn + SetOrientation (animProgress<=0.2, flipped, bike, quad). All agree.
- Fixed (35e7ffc0): the exe's `jne` / `jp` after `test ah,0x41 / 5` are also taken on NaN. The ramp `cmp<=0`, RampDown `crit<=0`, the two `critAbs>=10` tests and the bike getin `t2<=1` now take the same branch as the exe on NaN (no change for finite values).
- PARTIAL (not read line by line): slerp math 0x651EBC-0x651FD6 / 0x652337-0x652451 (sin polynomial and the from/to combine were spot-checked and agree) and quat->matrix 0x651FD7-0x6521CF / 0x652452-0x65263F (same shape as the annotated port).
- PC=24 note: x87d is float arithmetic per op, so `1.f - progress()` etc. need no change.
