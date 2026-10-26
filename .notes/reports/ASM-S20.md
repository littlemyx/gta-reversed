# ASM-S20 - CivilianPed / CopPed / EmergencyPed / Ped.cpp (75 functions)

Rules v5; worktree C:/gta-wt/asm-s20; start 19:50, end see hand-back. Result table: `.notes/asm_sweep/S20.result.tsv`.

| verdict | count |
|---|---|
| MATCH | 42 |
| FIXED (one commit each) | 12 |
| UNSURE | 0 |
| NOT_REACHED (stopped at the token budget; all are the 130..400+ line functions) | 21 |

## Fixes (exe address -> change)
- CPed::PedCanPickUpPickUp 0x455560: asks CPedIntelligence::FindTaskByType (4 primary slots), not active task + secondaries.
- CPed::AddGogglesModel 0x5E3AA4: removes existing goggles first (port only had a Release-no-op assert).
- CPed::RemoveWeaponWhenEnteringVehicle 0x5E63E8->0x5E6461: SMG slot without Uzi/Tec9/MP5 (no jetpack) removes the weapon model; port did nothing.
- CPed::SetRadioStation 0x5DFDC5: `rand() < 0x3FFF` -> radio1 (port `<=`).
- CPed::CreateDeadPedMoney 0x45913C: drops money for count >= 10 (port `> 10`).
- CPed::GetWalkAnimSpeed 0x5E04FB: no-translation branch reads Trans.y (+0x18, stride 20), port Rot.imag.y (practically unreachable).
- CPed::DettachPedFromEntity 0x5E7F48: DIE force is forward * -4 with z += 4.0 (0x858B90).
- CPed::AttachPedToEntity 0x5E7D93: player path calls CPlayerPed::MakeChangesForNewWeapon(weapon) after m_nChosenWeapon.
- CPed::GiveObjectToPedToHold 0x5E43C6: drop of the held entity (HOLD/PICKUP/PUTDOWN/GoPickUp) is independent of the HOLD-task lookup.
- CPed::CreateDeadPedWeaponPickups 0x459315: ClearWeapons() after the pickup loop (not on early returns).
- CPed::SetModelIndex 0x5E4934..0x5E49A0: money = base roll first (1st rand), then the 3% 400 roll (2nd rand); the port rolled the 400 first and consumed one rand fewer on a hit (RNG stream shift).
- CPed::OurPedCanSeeThisEntity 0x5E1719: LOS ray starts at own position + 1.0 z.

## Notes
- Consumer audit: all fixes are local to the function body (no shared-state layout/meaning change). SetModelIndex shifts the global rand() stream by one call on 3% of spawns (now equals the exe). CreateDeadPedWeaponPickups now empties the dead ped's weapons as the exe (caller TaskSimpleDead does not).
- Ignored (documented in tsv): GiveWeapon ammo cap signed vs unsigned (ammo >= 2^31), DoesLOSBulletHitPed NaN, AttachPedToBike return value (exe returns non-zero on success, port void).
- Not reached (21): CPed ctor/dtor, ShoulderBoneRotation, DoFootLanded, ProcessBuoyancy, PositionPedOutOfCollision, PositionAnyPedOutOfCollision, SetPedPositionInCar, DeadPedMakesTyresBloody, MakeTyresMuddySectorList, SetMoveAnim, SpecialEntityPre/CalcCollision*, Render, PreRenderAfterTest, ProcessEntityCollision, PlayFootSteps, ProcessControl, KillPedWithCar, Save, Load (plus the 401+ line ones that exceed the cap).
- Gate: Release build of the worktree (see hand-back); no oracle target covers these functions (none touched); soak below.
