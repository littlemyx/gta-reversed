# ASM-S49 (Game.cpp, GameLogic.cpp, GangWars.cpp) - 2026-10-29
Result: 40 rows: 19 FIXED (17 commits), 11 MATCH, 10 NOT_REACHED (budget), 0 UNSURE. Table in S49.result.tsv.
Order followed: GangWars attack/defend state machine + GameLogic restart/passtime/skip first, Game init/shutdown not reached.
Fixes (exe address, one line each):
- bd121512d CGameLogic::PassTime (0x4414C0): weekday wraps 7->1 once per day (port added all days at once), days stat +1.0 per day, 32-bit hour/day arithmetic.
- d3f92d79c FindCityClosestToPoint (0x441B70): cities tried LS, LV, SF; NaN keeps LS.
- 6d1d7585d StopPlayerMovingFromDirection (0x441290): NaN z stays NaN (std::max gave 0).
- 2cdc10124 CalcDistanceToForbiddenTrainCrossing (0x4418E0): third crossing is (2770,277) not 2270; the three tests are independent (3rd runs for stat <= 1); 3D dot with dir.z=-point.z and out.z=-point.z (the 2D variant is kept under FIX_BUGS).
- cdfb36171 DoWeaponStuffAtStartOf2PlayerGame (0x4428B0): stores (not restores) player 1's weapons; player 2 gets player 1's chosen weapon (port self-assigned).
- 75212b6d2 IsPlayerAllowedToGoInThisDirection (0x441E10): final test was (p1 && dot>=0) || dot<=0, i.e. always false for player 1; exe: p1 dot<0, p2 dot>0; dot summed z,y,x.
- 90aa64386 IsPlayerUse2PlayerControls (0x442020): focus NONE(2) returns true, otherwise ped != Players[focus] (port tested PLAYER2).
- d24f1c706 RestorePlayerStuffDuringResurrection (0x442060): vehicle reference cleaned for &m_pVehicle (port passed a local copy), fire pointer cleared after Extinguish, water distance 99999.9 (0x47C34FF3).
- 9b492f60c UpdateSkip (0x442480): in-progress state waits while now <= SkipTimer.
- 4ddb4e50c CalculateTimeTillNextAttack (0x443DB0): own multiply chain, not the lerp.
- 1020c28b6 DoStuffWhenPlayerVictorious (0x446400): TimeTillNextAttack = max(t-240000, 30000) (port min).
- e425a0122 EndGangWar (0x4464C0): attack branch for any non-zero State2 (port only WAR_NOTIFIED).
- c9f21a0be CheerVictory (0x444040): "ELS" label (port "ELF" never matched and asserted).
- 534b99407 MakeEnemyGainInfluenceInZone / MakePlayerGainInfluenceInZone (0x445FD0 / 0x445E80): lost-territory stat needs "controlled before"; 32-bit density sum.
- a7d220cff PickZoneToAttack (0x443B00): nested loops (each enemy zone x all nav zones in natural order, next enemy zone when the player is too close).
- 408242e10 StartDefensiveGangWar (0x444300): blip script name "CODEGW1".
- 7dd171e9a StartOffensiveGangWar (0x446050): Gang1/Gang2/density sum exclude Grove with the exe runner-up logic; training flag 0x96AB91 zeroes WarFerocity (port tested bIsPlayerOnAMission); inverted 0.4/0.5 territory test (dead in vanilla: stat < 0).
- e8963472b ReleasePedsInAttackWave (0x445C30): backwards pool walk and pool ref (idx<<8|flags) for ClearBlipForEntity (port: bare index, blips with non-zero flags byte were never cleared).
MATCH: Remove2ndPlayerIfPresent, SortOutStreamingAndMemory, SkipCanBeActivated, Store/RestorePedsWeapons, Clear/SetSpecificZones, ClearTheStreets, TellGangMembersTo, StrengthenPlayerInfluenceInZone, SetGangWarsActive.
Open (NOT_REACHED): CGangWars::CreateAttackWave (0x444810) and CreateDefendingGroup (0x4453D0) - the biggest state-machine pieces, highest value next; CGameLogic::Update (0x442AD0), Save, Load; CGame Init1/Init3/InitialiseRenderWare/InitialiseCoreDataAfterRW/CameraDestroy.
Side notes (not rows of this slice): CGameLogic::SetUpSkip (0x4423C0) calls CEntity::RegisterReference(SkipVehicle) without assigning the `vehicle` argument first - worth a look. CGangWars::MakeEnemyGainInfluenceInZone keeps the port's null-zone guard (the exe would crash).
Trailer uses "Claude Sonnet 5.5" (the real model), not the brief's "Opus 5.5".
Gate: StandaloneRelease build OK, PortableWin64 compile of GameLogic.cpp + GangWars.cpp clean (generators copied from the asm-s47 build), v6 pre-commit grep clean; wave2_oracle_test (RW_EXE_ORACLE + GTA_EXE = compact exe): 1142 functions, 0 mismatches (PC24 0/0, PC53 0/0); combat gameplay_regress PASS=14.
