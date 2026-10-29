# ASM-S12c (CTaskSimpleGangDriveBy, 4 rows) - agent ASM-S12c, 2026-10-29

Result: 4 of 4 compared in full: 1 FIXED, 3 MATCH. Table: .notes/asm_sweep/S12c.result.tsv.
- ProcessPed 0x62D3B0 (315deda9): the "ped dying" abort test is `fcomp 1.0 / test ah,5 / jnp`, i.e. health < 1.0 OR NaN aborts; the port's `health < 1.f` let NaN through. Now `!(health >= 1.f)`. The remaining bare `(double)` compares (cur-time windows 0.3 / 0.2667 / 0.4333, speed^2 0.5 / 0.01) are respelled float / x87d (exact, no behaviour change). Everything else (anim-group choice per vehicle type, window-open flags, jump table 0x62D9C0, burst count, attack timer, Say contexts 0x1F/0x1E) matches.
- FireGun 0x627CC0, ProcessAIPed 0x627600 (both jump tables decoded), ProcessAimIK 0x628350: MATCH (all float constants taken from the exe data, NaN arms and operand orders checked).
Open: none in this slice. Remaining `(double)`/`double` in GetVehicleHeading / aimHeading hold genuine x87:: (53 bit) results; left as is.
Gate: StandaloneRelease + StandaloneRW built; wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=compact exe) 1142 functions, mismatches PC24 0 / PC53 0; combat regress PASS=14; v6 pre-commit grep clean; PortableWin64 (x64 cl, real compile, subshell) of TaskSimpleGangDriveBy / TaskSimpleCarSetPedSlowDraggedOut / TaskSimpleClimb: 0 errors.
