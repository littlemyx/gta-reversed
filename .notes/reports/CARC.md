# CARC report

Commit e32ff891 (CarCtrl.cpp, Automobile.cpp, tests/standalone/{physics_oracle_carc.inc (new), physics_oracle_test.cpp (+2 lines), world_oracle_env.inc}).

## Task 1: GetATanOfXY sites in CarCtrl.cpp (line numbers had drifted; all verified against the exe disassembly)
| Function | Exe VA | Change |
|---|---|---|
| FlyAIPlaneInCertainDirection | 0x423045 | curHeadingExt = Ext(); targetHeading = (float)ext / (float)(ext-2.0f) / (float)(2.0f+ext); headingDiff = ext - (double)targetHeading (st0 stays live through 0x4230A5 fsub) |
| SteerAIHeliFlyingAwayFromPlayer | 0x42ACF9 | (float)(Ext + pi) (fadd [0x858CB8] then fstp) |
| SteerAIHeliToKeepEntityInView | 0x42AF08 | the atan stays on the x87 stack across the sqrt/compare to 0x42AFED: wantedHeading = (float)(ext + pi/2) (0x858FE4) |
| SteerAICarWithPhysicsHeadingForTarget | 0x433479 | (float)(Ext - (double)heading) |
| SteerAICarWithPhysicsFollowPath | 0x435452 | (float)(Ext(bend) - (double)heading) |
| SteerAICarWithPhysicsFollowPath | 0x435499 | (float)((double)curDirAngle - Ext(next)) (fsubr) |
Oracle (StandaloneRW, 4000 cases): HeadingForTarget / FollowPath / FollowPath_Racing (existing cases) 0 PC24 mismatches before and after (the rounding rarely survives FindSpeedMultiplier's clamps, so they do not discriminate);
new case CCarCtrl::SteerAIHeliToKeepEntityInView 0x42AEB0 (fake CHeli + fake target, near path only, whole heli buffer compared): old code 2/2000 PC24 mismatches (heli+0x98C), new code 0/4000.
Not driveable here: SteerAIHeliFlyingAwayFromPlayer (FindPlayerCoors reads the exe's player array vs the harness stub), FlyAIPlaneInCertainDirection (world line-of-sight probes); checked by asm only.

## Task 2: CCullZones box tests
CZoneDef::IsPointWithin (0x72D850) already matches the exe (212244c5): minZ < z < maxZ via FCOMP+JP / test 0x41 (NaN fails), dx/dy float, dot products 0 <= d <= exact int32 limit (fild), NaN fails. Re-verified line by line against the disassembly; no change.
The only other box test in CullZones.cpp is the unused, unhooked IsPointWithinArbitraryArea (0x72DDE0): the asm is `fcomp 0; test ah,5; jnp -> false` per edge, final `jp -> true`, i.e. false only when the cross product is < 0, NaN passes every edge; the port's `< 0.0f` has identical NaN behaviour; no change.
New oracle cases (world_oracle_env.inc): "CZoneDef::IsPointWithin 0x72D850 (NaN / inf points)" (each of x/y/z NaN/qNaN/sNaN/+-inf/-0/in-zone) and "IsPointWithinArbitraryArea 0x72DDE0".
Verified through a temporary copy in physics_oracle_test (world_oracle_test cannot link now): 4000 cases each, 0 mismatches PC24 and PC53 (183 resp. 1643 true results). The permanent copy compiles (world_oracle_test.cpp.obj built) but world_oracle_test.exe does not link: Audio/Hardware/AEAudioChannel.h(90) `g_numSoundChannelsUsed` undeclared (D-stream WIP). Re-run `world_oracle_test CZoneDef IsPointWithinArbitraryArea` when it builds.

## Task 3: Automobile.cpp
CGameLogic::GameState is eGameLogicState; GAME_STATE_INITIAL -> GAMELOGIC_STATE_PLAYING at 3469 and 5864; 3712 (`|| CGameLogic::GameState`) -> `!= GAMELOGIC_STATE_PLAYING`. No other misuse in source/.

## Build / tests
* StandaloneRelease: CarCtrl.cpp.obj and Automobile.cpp.obj compiled, 0 errors (no full link by me).
* StandaloneRW physics_oracle_test: touched cases 0 PC24 mismatches. A full run (no filter) faults in a later FollowTest case with 'DBG-PORT' (EIP=0x400000 = call into an unresolved /FORCE-linked symbol; the uncommitted DBG edits in physics_oracle_cd.inc belong to another agent). The same cases pass with a name filter; not caused by this change.
* Note: edits to .inc files do not retrigger ninja; `touch` the including .cpp.
* Not touched: physics_oracle_cd.inc (foreign uncommitted edit), the 238aca02 hunk.
