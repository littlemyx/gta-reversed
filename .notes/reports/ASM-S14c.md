# ASM-S14c (6 CTaskSimplePlayerOnFoot functions) - 2026-10-28
Result: 6 compared: 1 FIXED (1 commit), 5 MATCH, 0 UNSURE rows (two x87-extended-intermediate notes), 0 NOT REACHED. Table: S14c.result.tsv.
Fix:
- 589ad106 PlayIdleAnimations (0x6872F8-0x687348, 0x687497-0x6874AB): pad->SetTouched test used bCrouchWhenShooting, exe tests byte ped+0x478 bit1 = bUsingMobilePhone (13th flag byte); the "is looking" gate is `test 0xc` = either bIsLooking or bIsRestoringLook (port required both); the 20000 ms delta since the last idle anim is a signed compare (port unsigned wrapped when gLastTouchTimeDelta > delta, e.g. block unloaded when touched).
MATCH: PlayerControlDucked, PlayerControlZeldaWeapon, PlayerControlFighter, PlayerControlZelda, ProcessPed (branch polarity, constants, dot/sum orders, ExeRecip(60), move-state 4/6/7, anim ids, fight commands 3-6/0xF-0x12, timer ftol all compared to the exe).
Notes (not changed): Zelda and Fighter keep step=ts*0.07 / diffs in x87 extended in places where the port rounds to float (could differ by 1 ulp of m_fMoveBlendRatio / fight movement).
Consumers: fix is local to PlayIdleAnimations (reads only); no shared state or signature changed.
Gate: Release build OK; wave2_oracle_test not run (no touched oracle row: only CTaskSimplePlayerOnFoot::Destructor is covered, unchanged); soak 3 min NOVSYNC RESULT: OK (out C:/gta-scratch/asm-s14c/soak). Dumps: C:/gta-scratch/asm-s14c/.
