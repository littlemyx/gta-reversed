# PT2-INTERIOR — playtest-2 U15 (police station: all cops spawn on top of CJ), U16 (nobody reacts to the wanted level inside)

## Summary
Three port bugs, all asm-proven; none was a regression of the stream-D / zero-fill changes (bisect not needed: each was wrong since the original port, checked against the exe).

| # | finding | root cause | fix commit |
|---|---|---|---|
| U15 | interior peds stacked above CJ | `CPopulation::PopulateInterior` (0x616470) passed the PLAYER position (`pos`, z += 0.9 per ped) to `AddPed`; the exe passes the generated interior node (`nodePos`, z + 0.9: 0x616579 `fld [esp+0x30]; fadd [0x858C20]; fstp [esp+0x38]`, `push &[esp+0x28]` -> AddPed 0x612710). The "NOTE/BUG: they just keep adding it up" comment was the symptom of the misreading. | a6370488 |
| U16 (a) | AI gunmen (cops) never hit anything | `IKChain_c::IsFacingTarget` (0x617E60): dir = normalize(offsetPosWS - **root bone position** (bone+0x6C)), the port subtracted the transformed effector -> dot with targetPos almost never >= 0.95 -> `bGunReachedTarget` false -> `CWeapon::FireInstantHit` AI branch skips the line-of-sight test (`if (ped->m_pedIK.bGunReachedTarget \|\| arg6)`) -> hitEntity null. | 17198205 (+ exe-oracle row) |
| U16 (b) | same symptom, second cause | `CTaskSimpleUseGun::FireGun` (0x61EB10): `if (m_pedIK.bUseArm) bGunReachedTarget = IKChainManager::IsFacingTarget(...)` (0x61EC2F..59, flag bit 2); port had it inverted (`!bUseArm`) and lacked the off-screen branch (0x61EB9B: `if (bUseArm) bGunReachedTarget = true`). | 0577bd60 |

U16 as the user saw it: the cops DO switch to pursuit/arrest/kill tasks (task chain 1103>1101>1000>1002>1020 in the probe, ammo drops), but **no shot ever hit** (591 of 591 shots with gunReached=0 in one 60 s run, player at 10-12 m in plain view), so "nobody reacts". This is also the on-foot half of U7 (shots at the player do no damage): PT-COMBAT fixed the in-car half (CTaskSimpleCarDrive flag), the on-foot cop/gang gunmen were still harmless.

## Repro (scripted, run build, NOTSA_STANDALONE_PROBE=1)
`C:/gta-scratch/pti_probe/`: route_int.txt (new game, HESOYAM, `cmd:tp:1554.95:-1675:15.8` = LSPD entry marker (ENEX POLICE1 -> interior area 6 at 246.8,63.9,1003.6), wait, `cmd:wanted:3`), route_int_w.txt (wanted first, then enter), run_probe.sh; original exe: orig_int.txt / orig_int_w.txt + orig_probe_pti.py (ReadProcessMemory: player pos/area/hp/wanted; teleport by memory write 10 s after control + typed `turnuptheheat`), run_orig.sh. Probe now logs `pedpos` (x10 position, area code, currArea) for up to 20 peds.

| build | interior ped placement (player at 246.7,64.0,1003.6) | cops vs wanted player (3 stars, standing at the entry marker) |
|---|---|---|
| ac70ea8a (user's) | peds at (246.7,64.0) with z 1004.3, 1005.0, 1006.3, 1006.6, 1008.6, 1011.2 (d = 1..7 m: stacked above CJ) | pursuit tasks, some hits at 1-5 m (they stand on him) |
| + U15 fix | peds spread over the interior nodes (x 239..266, y 63..77, z 999..1005) | cops at the corridor door, 12 m, 49 shots in 60 s, **0 hits**, armour 100 untouched |
| + IKChain fix only | same | 6 of 18 shots pass the LOS test; second scenario still 0 of 552 (FireGun inverted) |
| + FireGun fix (final) | same | 22/22 and 10/11 shots hit; armour 100 -> 91 -> 83 -> 75 -> 0, busted ~15 s after the wanted level |
| original exe | (ped list not readable by the poller: its pool scan returns 0 peds even outdoors, pre-existing PT-COMBAT limitation) | player killed (hp 100 -> 0, state 55) 25-45 s after getting 2 stars, both scenarios |

Not reproduced: "nobody reacts" in the sense of idle cops (the cops reacted in every run of the user's build too); the no-hit cause explains the user's observation.

## Observations, not changed
- Interior peds have area code 0 while `currArea` = 6 in our build (`CEntity::CEntity` 0x532A90 sets 0, `AddPed` does not set it; I found no exe code that sets it for ambient peds either). Two far cops (d ~ 50 m, other interior rooms) sit in `TASK_WAIT_FOR_MATCHING_AREA_CODES` (1307). Could not compare with the original (ped list unreadable); rendering and AI of the near cops are unaffected.
- The user's playtest-2 log.log ends with a second crash (09:46, `CTrailer::ProcessEntityCollision` -> `CPhysical::ProcessCollisionSectorList` lambda, access violation); not in this task's scope (crash-20261018-094618.log).

## Verification
- Release build (StandaloneRelease) ok; `run_all_tests.sh` in the worktree 44/44 ok (new row `IKChain_c::IsFacingTarget 0x617E60`: 20000 cases, 4066 true results, 0 mismatches at PC24 and PC53; with the old port code: 3840 mismatches). RwV3dTransformVector / RwV3dNormalize (0x7EDDC0 / 0x7ED9B0 dispatch through the uninitialised RW table) are redirected to the same host maths on both sides, so the row compares the IK logic only. `PopulateInterior` and `FireGun` are not oracle-able (random placement, world / weapon calls); verified by the scripted repro above.
- Soak 3 min: see PHASE2_STATUS line.
