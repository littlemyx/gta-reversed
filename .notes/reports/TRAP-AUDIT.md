# TRAP-AUDIT - port-only faults hidden by the W2 harness (2026-10-20, rules v3, 21:56 - ~23:30)

Switch: `W2_PORT_FAULT_IS_DIFF=1` (env, every wave2/wave3 row; default off). `w2::BothA` then counts a port-only fault at EIP 0x400000 as a mismatch instead of a skipped fixture
limit, and `Run` prints per row `TRAPAUDIT <row>: port-only(0x400000) N, both M, exe-only K, unreachable-hits U of 2*cases; ret=<return address of the faulting call>` (resolve with the linker .map).
With the switch on, `NOTSA_UNREACHABLE` in the wave3 targets behaves like the run build (`Fixups::UnreachableHit`: record the site, fall through; Base.h `W2_UNREACHABLE_RUNBUILD`),
so the port continues exactly as in the game and the normal comparison finds any difference to the exe. Switch off: the old behaviour (fault at 0x400000, skipped).

## Result
26 targets run (wave2_oracle_test + 25 wave3_*; wave3_coll uses the game_oracle machinery, which never skips a fault, so it has nothing to hide).
43 rows had port-only faults at 0x400000 (17 NOTSA_UNREACHABLE, 26 call into a translation unit the test does not link). Nothing is a port bug:

* **17 NOTSA_UNREACHABLE rows**: the Debug test build traps there (MessageBox / exit, = a fault at 0x400000 in the harness); the run build logs the site and falls through. With the
  fall-through the port equals the exe on every hit case: PC24 0 in all 17 rows (audio GetVoice 40/1200 hits, events Compute*Response 24-168 hits each, Pickup::FindStringForTextIndex 38/80,
  FollowPointRoute 3 rows 20-52/120, wave2 DestroyCarMelee/HitByGun/Hit/RoadRage 24-110/120). UNREACHABLE in the test build only; no fix.
* **26 rows, port calls a function of an unlinked TU** (CMemoryMgr::MoveMemory, CEntity::TransformFromObjectSpace -> TransformPoint, CPedType::GetPedFlag, CRadar::ClearBlipForEntity,
  CHudColours::GetIntColour, CPad::GetHorn ...): the exe has the code, the port's callee is not in the test image. Not a trap - a fixture limit, but the part of the function after the call
  is not compared in those cases (2-95 % of the cases of a row). Follow-up (not done): link the callee TU or add a 3-line stub per callee to compare the rest.

All 17 UNREACHABLE rows were checked on the pass with fall-through; the 26 unlinked-callee rows are fixture limits and unchanged. FIXED: 0 (the only real hidden trap, CShopping::FindItem /
GetItemIndex, was fixed by ORACLE-W3-GP2-2 at 0x49AD20 / 0x49AB10). UNREACHABLE/FIXTURE: 43.
Caveat: wave2_oracle_test dies in its ControlSubTask rows (STATUS_INVALID_EXCEPTION_HANDLER after a first-chance AV) when built with the fall-through mode, so wave2 is excluded from
`W2_UNREACHABLE_RUNBUILD`; its 4 UNREACHABLE rows were run on their own with the fall-through build (PC24 0), the other wave2 rows only with the plain switch.

Switch off (default): wave2_oracle_test, wave3_anim/audio/camera/events/gameplay/tasks PC24 0 (the targets with port-only rows; the other targets differ from before only by counters). With the switch on, wave3_* PC24 = only the unlinked-callee rows above (port-only counted as mismatch), everything else 0; wave3_ai/carenv/coll(PC24 0 excl. NaN-payload)/entity/fx/fxs/gp2/ik/input/interior/misc/models/scripts/stream/vis/world have no port-only rows.

## Table
| target | row | port-only cases | cause | site / callee | verdict |
|---|---|---|---|---|---|
| wave2 | CTaskComplexDestroyCar::CreateFirstSubTask 0x62DA90 | 90/120 | unlinked callee | CWeapon::IsTypeMelee | FIXTURE (callee TU not linked into the test; not a trap) |
| wave2 | CTaskComplexDestroyCarArmed::CreateFirstSubTask 0x62E0A0 | 104/120 | unlinked callee | CWeapon::GetWeaponInfo | FIXTURE (callee TU not linked into the test; not a trap) |
| wave2 | CTaskComplexDestroyCarMelee::CreateNextSubTask 0x62DC20 | 40/120 | NOTSA_UNREACHABLE | TaskComplexDestroyCarMelee.cpp:118 TaskComplexDestroyCarMelee.cpp:221 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| wave2 | CTaskComplexEnterCarAsDriverTimed::ControlSubTask 0x63AEE0 | 8/120 | unlinked callee | ?0CTaskSimpleCarSetPedInAsDriver | FIXTURE (callee TU not linked into the test; not a trap) |
| wave2 | CTaskComplexHitByGunResponse::CreateFirstSubTask 0x631E50 | 24/120 | NOTSA_UNREACHABLE | TaskComplexHitByGunResponse.cpp:43 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| wave2 | CTaskComplexHitResponse::CreateFirstSubTask 0x638040 | 24/120 | NOTSA_UNREACHABLE | TaskComplexHitResponse.cpp:41 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| wave2 | CTaskComplexPartnerChat::MakeAbortable 0x682C60 | 30/120 | unlinked callee | CAEPedSpeechAudioEntity::ReleasePedConversation | FIXTURE (callee TU not linked into the test; not a trap) |
| wave2 | CTaskComplexRoadRage::CreateNextSubTask 0x62E270 | 110/120 | NOTSA_UNREACHABLE | (wave2: checked by filtered run) | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| wave2 | CTaskComplexUseMobilePhone::MakeAbortable 0x634930 | 48/120 | unlinked callee | CWeaponInfo::GetWeaponInfo | FIXTURE (callee TU not linked into the test; not a trap) |
| wave2 | CTaskSimpleCarFallOut::FinishAnimFallOutCB 0x648EE0 | 106/120 | unlinked callee | CDamageManager::SetDoorClosed_Component | FIXTURE (callee TU not linked into the test; not a trap) |
| wave2 | CTaskSimpleCarWaitForDoorNotToBeInUse::ProcessPed 0x646C90 | 100/120 | unlinked callee | CCarEnterExit::IsCarDoorInUse | FIXTURE (callee TU not linked into the test; not a trap) |
| anim | CAnimBlendSequence::MoveMemory 0x4D1150 | 122/200 | unlinked callee | CMemoryMgr::MoveMemory | FIXTURE (callee TU not linked into the test; not a trap) |
| anim | CAnimBlendHierarchy::MoveMemory 0x4CF800 | 164/200 | unlinked callee | CMemoryMgr::MoveMemory | FIXTURE (callee TU not linked into the test; not a trap) |
| audio | CAEPedSpeechAudioEntity::GetVoice 0x4E3CD0 | 40/1200 | NOTSA_UNREACHABLE | AEPedSpeechAudioEntity.cpp:429 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| camera | CCam::GetCoreDataForDWCineyCamMode 0x517130 | 236/1200 | unlinked callee | TransformPoint | FIXTURE (callee TU not linked into the test; not a trap) |
| events | CPedAcquaintanceScanner::WantsToRiotAgainst 0x603AF0 | 32/600 | unlinked callee | CPedType::GetPedFlag | FIXTURE (callee TU not linked into the test; not a trap) |
| events | CEventHandler::ComputeHighAngerAtPlayerResponse 0x4BAC10 | 36/300 | NOTSA_UNREACHABLE | EventHandler.cpp:1463 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| events | CEventHandler::ComputeLowAngerAtPlayerResponse 0x4BAAD0 | 36/300 | NOTSA_UNREACHABLE | EventHandler.cpp:1693 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| events | CEventHandler::ComputeLowHealthResponse 0x4BA990 | 26/200 | NOTSA_UNREACHABLE | EventHandler.cpp:1708 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| events | CEventHandler::ComputeReallyLowHealthResponse 0x4BAA30 | 26/200 | NOTSA_UNREACHABLE | EventHandler.cpp:2747 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| events | CEventHandler::ComputePedThreatBadlyLitResponse 0x4B9C90 | 24/200 | NOTSA_UNREACHABLE | EventHandler.cpp:2306 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| events | CEventHandler::ComputeSeenCopResponse 0x4BC050 | 80/240 | NOTSA_UNREACHABLE | EventHandler.cpp:2818 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| events | CEventHandler::ComputeSexyPedResponse 0x4B99F0 | 24/200 | NOTSA_UNREACHABLE | EventHandler.cpp:2865 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| events | CEventHandler::ComputeSpecialResponse 0x4BB800 | 168/600 | NOTSA_UNREACHABLE | EventHandler.cpp:3033 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| gameplay | CPedGroupMembership::RemoveMember-ByMemIdx 0x5F80D0 | 182/400 | unlinked callee | CRadar::ClearBlipForEntity | FIXTURE (callee TU not linked into the test; not a trap) |
| gameplay | CPickup::FindStringForTextIndex 0x455540 | 38/80 | NOTSA_UNREACHABLE | Pickup.cpp:72 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| gameplay | CGangWars::CanPlayerStartAGangWarHere 0x443F80 | 102/120 | unlinked callee | CTheZones::GetNavigationZone | FIXTURE (callee TU not linked into the test; not a trap) |
| gameplay | CGarages::Init_AfterRestart 0x448B60 | 118/120 | unlinked callee | CAEDoorAudioEntity::Reset | FIXTURE (callee TU not linked into the test; not a trap) |
| gameplay | CGarages::StoreCarInNearestImpoundingGarage 0x44A3C0 | 90/120 | unlinked callee | CVehicleModelInfo::GetNumRemaps | FIXTURE (callee TU not linked into the test; not a trap) |
| ped | CPed::FlagToDestroyWhenNextProcessed 0x5E7B70 | 2/120 | unlinked callee | CRadar::ClearBlipForEntity | FIXTURE (callee TU not linked into the test; not a trap) |
| tasks | CTaskComplexFollowPointRoute::GetLastWaypoint 0x669340 | 52/120 | NOTSA_UNREACHABLE | TaskComplexFollowPointRoute.cpp:182 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| tasks | CTaskComplexFollowPointRoute::GetNextWaypoint 0x669420 | 52/120 | NOTSA_UNREACHABLE | TaskComplexFollowPointRoute.cpp:212 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| tasks | CTaskComplexFollowPointRoute::CalcBlendRatio 0x66E710 | 18/120 | NOTSA_UNREACHABLE | TaskComplexFollowPointRoute.cpp:182 | UNREACHABLE in test build only; run-build fall-through = exe (PC24 0) |
| tasks | CTaskComplexPolicePursuit::PersistPursuit 0x68BDC0 | 12/120 | unlinked callee | CCullZones::NoPolice | FIXTURE (callee TU not linked into the test; not a trap) |
| tasks | CTaskComplexWander::ScanForBlockedNode- 0x671EF0 | 60/120 | unlinked callee | CVehicleScanner::GetClosestVehicleInRange | FIXTURE (callee TU not linked into the test; not a trap) |
| tasks | CTaskComplexWanderStandard::WillChat 0x66AED0 | 16/120 | unlinked callee | CPedType::GetPedFlag | FIXTURE (callee TU not linked into the test; not a trap) |
| ui | CHud::SetHelpMessageStatUpdate 0x588D40 | 404/3000 | unlinked callee | AsciiToGxtChar | FIXTURE (callee TU not linked into the test; not a trap) |
| ui | CHud::SetHelpMessageWithNumber 0x588E30 | 1540/3000 | unlinked callee | GxtCharStrcpy | FIXTURE (callee TU not linked into the test; not a trap) |
| ui | CRadar::AddBlipToLegendList 0x5859F0 | 170/4000 | unlinked callee |  | FIXTURE (callee TU not linked into the test; not a trap) |
| vehicle | CAutomobile::BoostJumpControl 0x6A3A60 | 84/120 | unlinked callee | CPad::HornJustDown | FIXTURE (callee TU not linked into the test; not a trap) |
| vehicle | CBmx::LaunchBunnyHopCB 0x6C0390 | 32/120 | unlinked callee | CStats::GetFatAndMuscleModifier | FIXTURE (callee TU not linked into the test; not a trap) |
| vehicle | CVehicle::ActivateBomb 0x6D24F0 | 18/120 | unlinked callee | CGarages::TriggerMessage | FIXTURE (callee TU not linked into the test; not a trap) |
| vehicle | CVehicle::ProcessSirenAndHorn 0x6E0950 | 16/120 | unlinked callee | CPad::GetHorn | FIXTURE (callee TU not linked into the test; not a trap) |

## Callees (LINK-CALLEES, 2026-10-20/21, rules v3, 23:25 - ~00:50)
All 26 unlinked-callee rows now run to the end: `W2_PORT_FAULT_IS_DIFF=1` port-only faults 0 in every affected target (full runs of wave2 rows, anim, camera, events, gameplay, ped, tasks, ui, vehicle: PC24 0;
remaining TRAPAUDIT lines are `both`/`exe-only` faults (the exe faults too: fixture limits) and the 17 UNREACHABLE rows). Fix = link the callee TU (`target_sources(... PRIVATE ...)` after the target in
source/CMakeLists.txt) or forward the call to the exe inside the test (2 cases). Port-only before -> after (cases of 2*N):
| target | row | callee | how | port-only |
|---|---|---|---|---|
| wave2 | DestroyCar::CreateFirstSubTask / DestroyCarArmed::CreateFirstSubTask | CWeapon::IsTypeMelee / GetWeaponInfo | link Weapon.cpp, WeaponInfo.cpp | 90 / 104 -> 0 |
| wave2 | EnterCarAsDriverTimed::ControlSubTask | CTaskSimpleCarSetPedInAsDriver ctor | link TaskSimpleCarSetPedInAsDriver.cpp | 8 -> 0 |
| wave2 | PartnerChat::MakeAbortable / UseMobilePhone::MakeAbortable | CAEPedSpeechAudioEntity::ReleasePedConversation / CWeaponInfo::GetWeaponInfo | link AEPedSpeechAudioEntity.cpp / WeaponInfo.cpp | 30 / 48 -> 0 |
| wave2 | CarFallOut::FinishAnimFallOutCB / CarWaitForDoorNotToBeInUse::ProcessPed | CDamageManager::SetDoorClosed_Component / CCarEnterExit::IsCarDoorInUse | link DamageManager.cpp, CarEnterExit.cpp | 106 / 100 -> 0 |
| anim | CAnimBlendSequence / Hierarchy::MoveMemory | CMemoryMgr::MoveMemory | forward to exe 0x72F500 (MemoryMgr.cpp collides with the harness allocator: LNK2005) | 122 / 164 -> 0 |
| camera | CCam::GetCoreDataForDWCineyCamMode | TransformPoint (via CEntity::TransformFromObjectSpace) | link common.cpp | 236 -> 0 |
| events / tasks | WantsToRiotAgainst / WillChat | CPedType::GetPedFlag | link PedType.cpp | 32 / 16 -> 0 |
| tasks | PersistPursuit / ScanForBlockedNode- | CCullZones::NoPolice / CVehicleScanner::GetClosestVehicleInRange | link CullZones.cpp, VehicleScanner.cpp | 12 / 60 -> 0 (18 `both` remain: exe faults too) |
| gameplay / ped | RemoveMember-ByMemIdx / FlagToDestroyWhenNextProcessed | CRadar::ClearBlipForEntity | link Radar.cpp | 182 / 2 -> 0 |
| gameplay | CanPlayerStartAGangWarHere / StoreCarInNearestImpoundingGarage | CTheZones::GetNavigationZone / CVehicleModelInfo::GetNumRemaps | link TheZones.cpp, VehicleModelInfo.cpp | 102 / 90 -> 0 |
| gameplay | CGarages::Init_AfterRestart | CAEDoorAudioEntity::Reset -> CAESoundManager::CancelSoundsOwnedByAudioEntity | link AEDoorAudioEntity.cpp + forward to exe 0x4EFCD0 | 118 -> 0 |
| ui | SetHelpMessageStatUpdate / SetHelpMessageWithNumber / AddBlipToLegendList | AsciiToGxtChar, GxtCharStrcpy / InsertPlayerControlKeysInString -> CControllerConfigManager / CHudColours::GetIntColour | link GxtChar.cpp, ControllerConfigManager.cpp, HudColours.cpp | 404 / 1540 / 170 -> 0 |
| vehicle | BoostJumpControl / LaunchBunnyHopCB / ActivateBomb / ProcessSirenAndHorn | CPad::HornJustDown, GetHorn / CStats::GetFatAndMuscleModifier / CGarages::TriggerMessage | link Pad.cpp, Stats.cpp, Garages.cpp | 84 / 32 / 18 / 16 -> 0 |

Now-compared code found 3 real port differences (one commit each; pre-fix = PC24 mismatches of the row, post-fix 0):
* `CHud::SetHelpMessageStatUpdate` 0x588D40: the sign char is '+' for every non-zero state byte (0x588E00-0x588E19), the port tested `== STAT_UPDATE_INCREASE` - 71/1500.
* `CMessages::InsertPlayerControlKeysInString` 0x69E160: the port was a rewrite (strstr / string_view lookup, unknown tags copied as text, no zero-fill behind the result); ported the exe algorithm
  (59-entry name table at ControlsManager+0x224, unknown tag: the 4 chars `~k~~` dropped, result copied back + zero-filled to 400 bytes) - row SetHelpMessageWithNumber 188/1500.
* `CGangWars::CanPlayerStartAGangWarHere` 0x443F80: compares with `&ZoneInfoArray[navZone.m_ZoneInfoIndex]` (no index-0 nullptr, no bounds check; the port went through GetZoneInfo(const CZone*) / a
  take-view subscript) - new row "(real zone infos)" 167/200 (+49/60 of the generated row), mostly the Debug-only subscript assert.
Not changed: other `CTheZones::GetZoneInfo(const CZone*)` users (GangWars.cpp:106/693/710/1187, Radar.cpp:1705, ZoneCommands.cpp:19) have the same index-0 nullptr/bounds deviation from the exe asm if the
exe code at those sites is unchecked - not oracle-covered, follow-up. Harness: `_CrtSetDebugFillThreshold(0)` in main (Debug CRT 0xFE fill of sprintf_s tails), gameplay fixture sets TotalNumberOfNavigationZones.
Run times with the switch on (3 targets in parallel, nice 15): ui 677 s (the largest target), wave3 camera 377 s, gameplay 72 s, vehicle 157 s, ped 62 s, tasks 44 s, events 49 s.
