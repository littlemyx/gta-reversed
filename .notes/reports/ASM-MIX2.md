# ASM-MIX2 - slices S29b (Object/HandObject) and S21b (PlayerPed), 2026-10-27
S29b: 5 functions: 1 MATCH (CHandObject::Render), 4 FIXED; S21b: 0 of 9 reached (token budget spent on S29b), all NOT_REACHED. Tables: S29b.result.tsv, S21b.result.tsv.
Fixes (exe range, commit):
- CObject::SpecialEntityCalcCollisionSteps 0x5A0646-0x5A0695: liftable/smallbox/fence part edge uses the smallest bbox extent (port: largest); step counts *6.6666665f/*3.3333333f; speed2*ts*ts - 1bf19088
- CObject::ProcessSamSiteBehaviour 0x5A08C1: altitude gate this.z - 2 <= target.z (port: target.z + 2) - 838b33b4
- CObject::ObjectDamage 0x5A0F99.. / 0x5A10FF: destroy paths set only the static bit (not virtual SetIsStatic); fx type 1 skipped when destroyed - 9c183b22
- CObject::ProcessControl 0x5A21C9 / 0x5A2685-0x5A2719 / 0x5A289D: dumper without fork-angle test; RC bomb force *turnMass and pow(1-|v|^2*0.2, ts) (args were swapped); door settle: not stuck (port: not BIGBuilding) - a08b8942
Consumer audit: no layout or signature changes; ObjectDamage no longer clears bDoorHitEndStop on destroyed objects (flag read only by door code, object is static and invisible by then).
Not reached: ProcessAnimGroups, SetRealMoveAnim, Compute3rdPersonMouseTarget, KeepAreaAroundPlayerClear, FindWeaponLockOnTarget, FindNextWeaponLockOnTarget, MakeThisPedJoinOurGroup, ProcessWeaponSwitch, PlayerPed::ProcessControl.
Side notes: CObject::SetIsStatic(true) at Object.cpp lines ~326/759/783 (other functions) is the same virtual-vs-bit pattern; not checked against their asm.
Gate: Release build of the worktree OK (clean full build + incremental); no oracle target covers Object files; soak.sh 3 min NOVSYNC: RESULT: OK.
