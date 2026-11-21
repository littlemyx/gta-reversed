# ASM-Z16: land Z15, review the Z15 fixes, continue the P4_ORACLE_ONLY gameplay list (oracle: gta_sa_compact.exe via exedis)

## A. Z15 landing
Z15 (3 source fixes 8a260b264, 90278fd2a, fccfb76e9 plus docs) was pushed together with the Z16 commits after one non-STATEHASH StandaloneRelease link (buildslot.2, owner Z16).

## B. Independent review of the Z15 fixes (C:/gta-scratch/review_results.tsv)
| sha | function | verdict |
|---|---|---|
| 8a260b264 | CCoverPoint::Update / CanBeRemoved (0x699850) | CONFIRMED (AllocateMatrix 0x54F560 + 0x54F1B0 when entity+0x14 is null, then up.z) |
| 90278fd2a | CPed::SetLook(float) (0x5E79FE) | CONFIRMED (`and 0xFFFFFFF7 ; or 4` = bIsRestoringLook off, bIsLooking on) |
| fccfb76e9 | CPlayerPed::MakePlayerGroupDisappear (0x60A478) | CONFIRMED (`and [m+0x1C],0xFFFFFF7E` clears bit 0 and bit 7) |

## C. P4_ORACLE_ONLY gameplay rows (no previous asm verdict)
Result: .notes/asm_sweep/Z16.result.tsv. 3 FIXED, 1 DIFF-KEPT, rest MATCH.

| fix | exe | what |
|---|---|---|
| GetWeaponRadiusOnScreen | 0x609CD0 | radius = (counter*0.5+1) * (rangeProg * accuracyProg): the exe multiplies range*accuracy first (float rounding order) |
| CTaskComplexFleePoint::SetFleePosition | 0x65B7B0 | the new flee position is also taken when only safeDistance differs (x, y, z and m_safeDist are all compared); the port compared the position only, so a changed safe distance was ignored |
| CTaskSimpleCarWaitForDoorNotToBeInUse::SetPedPosition | 0x646CC0 | a null m_veh skips SetPedPositionInCar |

DIFF-KEPT: CPed::IsPointerValid (0x5E4220) - the exe checks only the pool index range and the collision list / player, the port's IsObjectValid also rejects a free slot; no gameplay effect.

Compared (MATCH): player-ped helpers (RemovePlayerPed, CheatWantedLevel, PickWeaponAllowedFor2Player), CTaskSimpleFight combo getters, duck/hold-entity helpers, follow-node-route waypoint getters, event AffectsPed / ReportCriminalEvent family (crime ids 5, 8, 10, 11), CEventKnockOffBike::From, kill-ped-on-foot-melee ranges, HitHurtsPed, walk-round-fire, medic target pos, group allocators, ~20 FinishAnim callbacks (field offsets and flag bits checked against headers), CVehicle winch/rope/firing-rate/upgrade helpers, CPlane/CBike/CBoat small ones, CObject rope helpers, CMatrix/CRect helpers, the weather/time/toggle cheats, CEntity::IsEntityPointerValid, CVehicleModelInfo::ChooseVehicleColour / passenger count from door count.

Remaining oracle-only gameplay candidates (not compared): CCheat vehicle-spawn and skill cheats, Interior_c / InteriorGroup_c / InteriorManager_c setup helpers, CVehicleModelInfo component-rule helpers (IsValidCompRule, CountCompsInRule, GetListOfComponentsNotUsedByRules), CTaskComplexPartnerDeal/Shove::GetPartnerSequence (PartnerChat has x87 audit comments, not instruction-compared).

## Build
StandaloneRelease (non-STATEHASH, ninja -j4, nice) in C:/gta-wt/p4r2d/build/StandaloneRelease under buildslot.2 (owner Z16): see the note below.
