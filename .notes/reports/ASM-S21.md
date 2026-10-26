# ASM-S21 (PedSaveStructure.cpp, PlayerPed.cpp, PlayerSkin.cpp, 32 functions) - 2026-10-26 21:02 .. see hand-back
Result: 23 compared (3 FIXED in 3 commits, 20 MATCH), 0 UNSURE, 9 NOT REACHED (token budget; table in S21.result.tsv). Slice is PARTIAL.
Fixes (exe address, one line each):
- DisbandPlayerGroup (0x60A0A0): RemoveAllFollowers(true) runs always (port: only when there were 0 members); the Say needs >0 members and uses DISBAND_ONE (0x96) for a single member (port ATTACK_SINGLE 0x94).
- MakeChangesForNewWeapon (0x60B460): fire anim flags |= 9 (PLAYING | FINISH_AUTO_REMOVE; the port ANDed the two flags, a no-op); ammo in clip is a signed min of total ammo and the sign-extended word clip size.
- LOSBlockedBetweenPeds (0x60B550): the target point is entity2's neck/position (port used entity1 for both ends); on a hit the exe returns arg1 != arg2 (`cmp [esp+4Ch], esi; setne`), not hitEntity != entity2.
MATCH: CPedSaveStructure::Extract, CPlayerPed::Load/Save (byte-compatible; exe leaves Save padding as stack garbage, port writes 0), Deactivate/ReactivatePlayerPed, CanPlayerStartMission, ReApplyMoveAnims,
MakeGroupRespondToPlayerTakingDamage, TellGroupToStartFollowingPlayer, HandlePlayerBreath, SetPlayerMoveBlendRatio, SetInitialState, MakeChangesForNewWeapon(slot), EvaluateTarget,
EvaluateNeighbouringTarget, SetupPlayerPed, CPlayerSkin::Initialise/Shutdown/RenderFrontendSkinEdit/GetSkinTexture.
Notes: SetPlayerMoveBlendRatio min(2, v) differs from the exe only for NaN (not reachable). Load reads wanted level / chosen weapon as dword/uint8 where the exe reads a byte (equal for valid saves).
Consumer audit: no layout/signature change; LOSBlockedBetweenPeds callers: EvaluateTarget (this slice) and the exe's own callers; all three fixes are local to the function bodies.
Not reached (9): ProcessAnimGroups, SetRealMoveAnim, Compute3rdPersonMouseTarget, KeepAreaAroundPlayerClear, FindWeaponLockOnTarget, FindNextWeaponLockOnTarget, MakeThisPedJoinOurGroup, ProcessWeaponSwitch, ProcessControl.
Gate: see hand-back (Release build, no oracle target covers these D/S/C functions, soak).
