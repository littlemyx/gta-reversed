# ORACLE-W3-GAMEPLAY report

Agent ORACLE-W3-GAMEPLAY (rules v2), worktree C:/gta-wt/oracle-w3-gp, branch oracle-w3-gp. Scope: exe-oracle rows, classes A then B, for the gameplay-rule root files of game_sa.

| | before | after |
|---|---:|---:|
| game_sa root files covered | 855 | **976** (+121) |
| uncovered A / B in the root files | 396 / 611 | **346 / 540** |
| exe-oracle coverage, whole game | 3037 | **3158** |
| new target | | `wave3_gameplay_oracle_test` (W2 harness, `W2_ONLY W3_GP_ONLY`); generator `tests/standalone/tools/gen_w3_gp.py` (110 generated rows) + 11 hand rows in `wave3_oracle_gp_fix.inc` |
| committed test | | 121 rows, PC24 0 mismatches, PC53 0, exits rc 0 |
| port bugs fixed | | **8 commits** (one per function, asm address in the subject) |

Files of the slice (uncovered A / B before): Clock (0 / 3), Darkel (1 / 8), GangWars (1 / 12), Garage (6 / 15), Garages (8 / 10), PedGroup (4 / 1), PedGroupIntelligence (8 / 0), PedGroupMembership (0 / 7), PedGroups (3 / 1), PedStats (0 / 2), Pickup (3 / 1), Pickups (5 / 2), PlayerInfo (9 / 2), Population (5 / 1), Stats (2 / 6), WeaponEffects (3 / 3), WeaponInfo (1 / 0), Weapon (0 / 2), Weather (0 / 5). Wanted.cpp was already fully covered.

## Fixture
`W3GpCase(r, d, "<class>", sizeof(this obj), ents, port, exe)` = the W2 case (garbage `this`, entity pointers -> reference-world entities, whole exe data image compared and restored) plus `GpFix(cls)` (wave3_oracle_gp_fix.inc): WireWorld (every reference-world entity gets a matrix and a shared model with a collision model and spheres, handling id in table range, random handling flags), WireIntel (empty CPedIntelligence for member peds), sane CGarage fields and the 50 `CGarages::aGarages`, CPedGroups members, CGangWars zone counters. Generator additions: class from the `RH_ScopedClass` line (root files are installed after `RH_ScopedCategoryGlobal`, which the inventory reads as "no class"), CrossHairId, `const CPed&`, CZoneInfo buffer, garage-id arguments 0..49, `CDarkel::ReadStatus` compared on AX (`mov ax,[Status]`), CStoredCar header override (the port defines CStoredCar in both StoredCar.h and Garage.h).

## Port bugs fixed (each row failed on the pre-fix code; counts = PC24 mismatches of 60 / 100 / 400 cases)
| function | asm | bug | before |
|---|---|---|---|
| CWeaponEffects::ClearCrossHairsImmediately 0x742CC0 | loop stores time-100, active=0 | called ClearCrossHair (time+400, still active) | 60/60 |
| CPedGroup::FindDistanceToNearestMember 0x5FB0A0 | out pointer written for every follower, `fcom` minimum starting at 1e10 | returned the truly nearest follower | 62/100 |
| CPedGroupMembership::RemoveAllFollowers 0x5FB190 (+ CPedGroup 0x5FB7D0) | `cmp esi,7` skips slot 7 only | `IsLeader(mem)` also skipped duplicates of the leader | 14/60, 22/60 |
| CGarage::BuildRotatedDoorMatrix 0x4479F0 | sin/cos `fstp dword`, angle unrounded in ST0, cross product rounds only the differences | double sin/cos, rounded products | 26/60 (1 ulp) |
| CPopulation::FindPedRaceFromName 0x5B6D40 | NUL maps to `continue`, reads the 2nd char | returned RACE_DEFAULT at once | 4/60 |
| CPopulation::AddExistingPedInCar 0x611560 | `mov eax,[esp+4]; ret` | NOTSA_UNREACHABLE trap | every call |
| CGarages::TriggerMessage 0x447B80 | arg2 -> 0x96C010, arg4 -> 0x96C00C; plain strcpy; `ja` on the end time | numbers swapped, strcpy_s (0xFE fill, 7-char limit), `>=` | 382/400 |
| CGarages::ChangeGarageType 0x4476D0 | writes the global garage | `auto garage = GetGarage()` copied the 0xD8 bytes | 55/60 |

## Not bugs
`CWeather::FindWeatherTypesList` / `CPickup::FindStringForTextIndex` return pointers into data: hand rows compare the content (the exe region lists are 64 BYTES, the port's are 64 int16 entries; values equal). `CPlayerInfo::CancelPlayerEnteringCars` is `ret 4` in the exe (stdcall) but static (cdecl) in the port: the hand row calls the exe as stdcall.

## Not covered (details and next steps in ORACLE-W3-GAMEPLAY-state.md)
12 generated functions that fault in both (fixture missing) are in `tools/w3_gp_skip.tsv`; 2 hand rows (CPlayerInfo::EvaluateCarPosition 0x56DAD0, CPedGroupMembership::From 0x5F7FE0) removed because they fault in both; 3 constructors; CPickup::GetPosn (returns CVector); ModifyStringLabelForControlSetting (global, no class). Class C / S / D functions of the same files are a different slice.
Trailer: `Co-Authored-By: Claude Opus 5.5` as the brief says.
