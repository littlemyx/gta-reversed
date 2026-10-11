#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>

#include "World.h"
#include "Radar.h"
#include "Game.h"
#include "CarCtrl.h"
#include "CutsceneMgr.h"
#include "MissionCleanup.h"
#include "PedGroups.h"
#include "PedGeometryAnalyser.h" // CPointRoute
#include "SearchLight.h"
#include "Checkpoint.h"
#include "Weapon.h"
#include "Ragdoll/IKChainManager.h"
#include "TaskSequences.h"
#include "DecisionMakers/DecisionMakerTypes.h"
#include "DecisionMakers/DecisionMakerTypesFileLoader.h"
#include "Entity/Vehicle/Train.h"

#include "TaskSimpleTriggerLookAt.h"
#include "TaskComplexDiveFromAttachedEntityAndGetUp.h"
#include "TaskComplexSitDownThenIdleThenStandUp.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "TaskComplexDrivePointRoute.h"
#include "TaskSimpleCarSetTempAction.h"
#include "TaskComplexCarDriveMission.h"
#include "TaskComplexGoToPointAiming.h"
#include "TaskSimpleRunAnim.h"
#include "TaskComplexArrestPed.h"
#include "SeekEntity/TaskComplexSeekEntityStandard.h"
#include "SeekEntity/TaskComplexSeekEntityRadiusAngleOffset.h"
#include "game_sa/DetachedShared.h"
#line 39

// Registered by the central registration (Commands.hpp / RunningScript.cpp, orchestrator)
namespace notsa::script::commands::ported::g17b {
void RegisterHandlers();
}

using namespace notsa::script;

/*!
* Script commands ported from the exe's group processor g17 (CRunningScript::ProcessCommands1700To1799 @0x496E00) for the
* vanilla commands that had no handler of their own, part b: the TASK_* commands, searchlights, checkpoints, trains,
* LA riots / emergency services switches (44 commands, ids 1701..1764). The rest of g17 is in Group16_17c.cpp.
*
* Every handler below was written from the asm of the `case` in the group processor, NOT from the Ghidra decompilation.
* The case address is given in the comment above each handler. Common shape of the TASK_* commands:
* `CollectParameters(n)` -> `new CTaskXxx(...)` -> `CRunningScript::GivePedScriptedTask(ped, task, opcode)` (ped handle -1 =
* "add to the active sequence", handled inside `GivePedScriptedTask`; `opcode` = the command that is executing).
* The exe's `test eax, eax` after `operator new` is only the allocation-failure check and is not kept.
*
* The exe peeks at the output variable of the "create" commands (`CollectNextParameterWithoutIncreasingPC`) and feeds it to
* `GetActualScriptThingIndex` / `GetActualBlipArrayIndex`; the results are never used (dead), so that is omitted here (it would
* index the thing arrays with the garbage of a not yet written variable).
*/

namespace {
//! 0x8595EC: degrees -> radians (float)
constexpr float DEG_TO_RAD_F = std::bit_cast<float>(0x3C8EFA35u);

//! 0xC18D50: the route that FLUSH_ROUTE / EXTEND_ROUTE build and TASK_FOLLOW_POINT_ROUTE / TASK_DRIVE_POINT_ROUTE consume
CPointRoute& ScriptRoute() { return NOTSA_GLOBAL_EXPR(0xC18D50, (CPointRoute), notsa::shared::ScriptRoute); }

//! 0x8D237C: set by SET_NEXT_DESIRED_MOVE_STATE, consumed (and reset to PEDMOVE_RUN = 6) by the TASK_GOTO_CHAR_OFFSET family
eMoveState& NextDesiredMoveState() { return NOTSA_GLOBAL_EXPR(0x8D237C, (eMoveState), notsa::shared::NextDesiredMoveState); }

//! 1701 TASK_DIVE_FROM_ATTACHMENT_AND_GET_UP (case @0x496E61): ped, timeOnGround
//! CollectParameters(2); CTaskComplexDiveFromAttachedEntityAndGetUp(time) [0x492E20]
void TaskDiveFromAttachmentAndGetUp(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 time) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexDiveFromAttachedEntityAndGetUp{ time }, command);
}

//! 1704 TASK_GOTO_CHAR_OFFSET (case @0x496F5F): ped, target ped, time, radius, angle(deg)
//! CollectParameters(5); time < 0 => 50000 (0xC350). CTaskComplexSeekEntity<RadiusAngleOffset>(target, time, 1000, 1.0 (0x85A40C),
//! 2.0 (0x85A410), 2.0 (0x85A414), true, true) [0x493730]; then calc.radius = radius, calc.angle = (float)(angle * 0.017453292f (0x8595EC)),
//! task->moveState (+0x4C) = [0x8D237C] and [0x8D237C] = 6 (PEDMOVE_RUN)
void TaskGotoCharOffset(eScriptCommands command, CRunningScript& S, int32 pedHandle, CPed* target, int32 time, float radius, float angleDeg) {
    if (time < 0) { // `test edi, edi; jge`
        time = 50000;
    }
    const float angle = (float)((double)angleDeg * (double)DEG_TO_RAD_F); // `fmul` + `fstp dword`
    auto* const task  = new CTaskComplexSeekEntityRadiusAngleOffset{
        target, time, 1000, 1.0f, 2.0f, 2.0f, true, true,
        CEntitySeekPosCalculatorRadiusAngleOffset{ radius, angle }
    };
    task->SetMoveState(NextDesiredMoveState());
    NextDesiredMoveState() = PEDMOVE_RUN;
    S.GivePedScriptedTask(pedHandle, task, command);
}

//! 1705 TASK_LOOK_AT_COORD (case @0x49707D): ped, x, y, z, time
//! CollectParameters(5); time -1 => 20000, -2 => 999999 (0xF423F).
//! ped != -1: IKChainManager_c::LookAt [0x618970] right away (nothing is given to the ped);
//! ped == -1: CTaskSimpleTriggerLookAt [0x634440] appended to the active sequence [CTaskComplexSequence::AddTask 0x632D10]
void TaskLookAtCoord(int32 pedHandle, CVector pos, int32 time) {
    if (time == -1) {
        time = 20000;
    } else if (time == -2) {
        time = 999999;
    }
    if (pedHandle != -1) {
        g_ikChainMan.LookAt(
            "COMMAND_TASK_LOOK_AT_COORD", // 0x85A3F0
            GetPedPool()->GetAtRef(pedHandle),
            nullptr,
            time,
            BONE_UNKNOWN, // -1
            &pos,
            false,
            0.25f, // 0x3E800000
            500,
            6,
            true
        );
        return;
    }
    CTaskSequences::AddTaskToActiveSequence(
        new CTaskSimpleTriggerLookAt{ nullptr, time, BONE_UNKNOWN, RwV3d{ pos.x, pos.y, pos.z }, true, 0.25f, 1000, 3 }
    );
}

//! 1707 HIDE_CHAR_WEAPON_FOR_SCRIPTED_CUTSCENE (case @0x4971D1): ped, flag
//! CollectParameters(2); the ped is NOT null checked. flag != 0 => CPed::ReplaceWeaponForScriptedCutscene [0x5E6530];
//! else CPed::RemoveWeaponForScriptedCutscene [0x5E6550] and, if the ped is in a vehicle (pedFlags bit 8 at +0x46C),
//! CPed::RemoveWeaponWhenEnteringVehicle(0) [0x5E6370]
void HideCharWeaponForScriptedCutscene(CPed& ped, int32 flag) {
    if (flag != 0) {
        ped.ReplaceWeaponForScriptedCutscene();
        return;
    }
    ped.RemoveWeaponForScriptedCutscene();
    if (ped.bInVehicle) {
        ped.RemoveWeaponWhenEnteringVehicle(0);
    }
}

//! 1709 SET_GROUP_DECISION_MAKER (case @0x497281): group, decision maker
//! CollectParameters(2); group = GetActualScriptThingIndex(P0, 8) must be in [0, 8); the decision maker is -1 if P1 == -1, else
//! GetActualScriptThingIndex(P1, 7). CPedGroupIntelligence::SetGroupDecisionMakerType [0x5F7340] of ms_groups[group] (0xC09920 + 0x30)
void SetGroupDecisionMaker(int32 groupHandle, int32 dmHandle) {
    const auto group = CTheScripts::GetActualScriptThingIndex(groupHandle, SCRIPT_THING_PED_GROUP);
    if (group < 0 || group >= 8) {
        return;
    }
    int32 dm = -1;
    if (dmHandle != -1) {
        dm = CTheScripts::GetActualScriptThingIndex(dmHandle, SCRIPT_THING_DECISION_MAKER);
    }
    CPedGroups::ms_groups[group].GetIntelligence().SetGroupDecisionMakerType((eDecisionMakerType)dm);
}

//! 1710 LOAD_GROUP_DECISION_MAKER (case @0x4972E2): type => 1 handle
//! CollectParameters(1); GetGrpDMName(type) [0x600880]; LoadDecisionMaker(name, 1 (!), m_UsesMissionCleanup) [0x607D30];
//! handle = GetNewUniqueScriptThingIndex(id, 7); if cleanup: AddEntityToList(handle, 9 = DECISION_MAKER)
int32 LoadGroupDecisionMaker(CRunningScript& S, int32 type) {
    char name[1024];
    CDecisionMakerTypesFileLoader::GetGrpDMName(type, name);
    const auto id     = CDecisionMakerTypesFileLoader::LoadDecisionMaker(name, (eDecisionTypes)1, S.m_UsesMissionCleanup);
    const auto handle = CTheScripts::GetNewUniqueScriptThingIndex(id, SCRIPT_THING_DECISION_MAKER);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(handle, MISSION_CLEANUP_ENTITY_TYPE_DECISION_MAKER);
    }
    return handle;
}

//! 1712 TASK_SIT_DOWN (case @0x4973B9): ped, time
//! CollectParameters(2); time == -1 => 20000 (0x86DB28). CTaskComplexSitDownThenIdleThenStandUp(time, 0, 0) [0x631460]
void TaskSitDown(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 time) {
    if (time == -1) {
        time = 20000;
    }
    S.GivePedScriptedTask(pedHandle, new CTaskComplexSitDownThenIdleThenStandUp{ time, 0, 0 }, command);
}

//! 1713 CREATE_SEARCHLIGHT (case @0x497444): x1, y1, z1, x2, y2, z2, targetRadius, baseRadius => 1 handle
//! CollectParameters(8); CTheScripts::AddScriptSearchLight(start, null, target, targetRadius, baseRadius) [0x493000];
//! if cleanup: AddEntityToList(handle, 11 = SEARCHLIGHT)
int32 CreateSearchlight(CRunningScript& S, CVector start, CVector target, float targetRadius, float baseRadius) {
    const auto handle = (int32)CTheScripts::AddScriptSearchLight(start, nullptr, target, targetRadius, baseRadius);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(handle, MISSION_CLEANUP_ENTITY_TYPE_SEARCHLIGHT);
    }
    return handle;
}

//! 1714 DELETE_SEARCHLIGHT (case @0x497505): searchlight
//! CollectParameters(1); GetActualScriptThingIndex(P0, 2) < 0 => nothing. Else RemoveScriptSearchLight(P0) [0x493160];
//! if cleanup: RemoveEntityFromList(P0, 11) [0x4654B0]
void DeleteSearchlight(CRunningScript& S, int32 handle) {
    if (CTheScripts::GetActualScriptThingIndex(handle, SCRIPT_THING_SEARCH_LIGHT) < 0) {
        return;
    }
    CTheScripts::RemoveScriptSearchLight(handle);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.RemoveEntityFromList(handle, MISSION_CLEANUP_ENTITY_TYPE_SEARCHLIGHT);
    }
}

//! 1715 DOES_SEARCHLIGHT_EXIST (case @0x49755D): searchlight => cmp
//! CollectParameters(1); GetActualScriptThingIndex(P0, 2) >= 0
bool DoesSearchlightExist(int32 handle) {
    return CTheScripts::GetActualScriptThingIndex(handle, SCRIPT_THING_SEARCH_LIGHT) >= 0;
}

//! 1716 MOVE_SEARCHLIGHT_BETWEEN_COORDS (case @0x497598): searchlight, x1, y1, z1, x2, y2, z2, speed
//! CollectParameters(8); the function at 0x493360 (CSearchLight::SetPathBetween)
void MoveSearchlightBetweenCoords(int32 handle, CVector p1, CVector p2, float speed) {
    CSearchLight::SetPathBetween(handle, p1, p2, speed);
}

//! 1717 POINT_SEARCHLIGHT_AT_COORD (case @0x4975E5): searchlight, x, y, z, speed
//! CollectParameters(5); the function at 0x493480 (CSearchLight::SetTravelToPoint)
void PointSearchlightAtCoord(int32 handle, CVector point, float speed) {
    CSearchLight::SetTravelToPoint(handle, point, speed);
}

//! 1718 POINT_SEARCHLIGHT_AT_CHAR (case @0x49761E): searchlight, ped, speed
//! CollectParameters(3); the function at 0x493420 (CSearchLight::SetFollowEntity) with the ped (pool GetAtRef)
void PointSearchlightAtChar(int32 handle, CPed* ped, float speed) {
    CSearchLight::SetFollowEntity(handle, ped, speed);
}

//! 1721 HAS_CUTSCENE_LOADED (case @0x4976A2): => cmp
//! no parameters; CCutsceneMgr::ms_cutsceneLoadStatus (0xB5F84C) == 2 (LOADED)
bool HasCutsceneLoaded() {
    return CCutsceneMgr::ms_cutsceneLoadStatus == CCutsceneMgr::LoadStatus::LOADED;
}

//! 1722 TASK_TURN_CHAR_TO_FACE_COORD (case @0x4976C8): ped, x, y, z
//! CollectParameters(4); CTaskComplexTurnToFaceEntityOrCoord(coords, 0.5f (0x86FC7C), 0.2f (0x86FC80)) [0x66B910]
void TaskTurnCharToFaceCoord(eScriptCommands command, CRunningScript& S, int32 pedHandle, CVector coords) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexTurnToFaceEntityOrCoord{ coords, 0.5f, 0.2f }, command);
}

//! 1723 TASK_DRIVE_POINT_ROUTE (case @0x49776F): ped, vehicle, speed
//! CollectParameters(3); the vehicle is only looked up if the handle is >= 0 (else null). CTaskComplexDrivePointRoute(veh,
//! route 0xC18D50, speed, mode 2, carModel -1, targetRadius -1.0f, drivingStyle 0) [0x6433E0]
void TaskDrivePointRoute(eScriptCommands command, CRunningScript& S, int32 pedHandle, CVehicle* veh, float speed) {
    S.GivePedScriptedTask(
        pedHandle,
        new CTaskComplexDrivePointRoute{ veh, ScriptRoute(), speed, 2u, (eModelID)-1, -1.0f, (eCarDrivingStyle)0 },
        command
    );
}

//! 1724 FIRE_SINGLE_BULLET (case @0x4977E3): x1, y1, z1, x2, y2, z2, damage
//! CollectParameters(7); FireOneInstantHitRound(&start, &end, damage) [0x73AF00]
void FireSingleBullet(CVector start, CVector end, int32 damage) {
    FireOneInstantHitRound(start, end, damage);
}

//! 1725 IS_LINE_OF_SIGHT_CLEAR (case @0x497852): x1, y1, z1, x2, y2, z2, buildings, vehicles, peds, objects, dummies => cmp
//! CollectParameters(11); the 5 flags are normalised with `!= 0`; CWorld::GetIsLineOfSightClear(start, end, flags..., false, false) [0x56A490]
bool IsLineOfSightClear(CVector start, CVector end, int32 buildings, int32 vehicles, int32 peds, int32 objects, int32 dummies) {
    return CWorld::GetIsLineOfSightClear(start, end, buildings != 0, vehicles != 0, peds != 0, objects != 0, dummies != 0, false, false);
}

//! 1726 GET_CAR_ROLL (case @0x497908): vehicle => 1 float
//! CollectParameters(1); the vehicle is NOT null checked; CAutomobile::GetCarRoll [0x6A6010] (on whatever vehicle type it is: it only uses the matrix)
float GetCarRoll(CVehicle& veh) {
    return reinterpret_cast<CAutomobile&>(veh).GetCarRoll();
}

//! 1727 POINT_SEARCHLIGHT_AT_VEHICLE (case @0x497940): searchlight, vehicle, speed
//! CollectParameters(3); the function at 0x493420 (CSearchLight::SetFollowEntity) with the vehicle (pool GetAtRef)
void PointSearchlightAtVehicle(int32 handle, CVehicle* veh, float speed) {
    CSearchLight::SetFollowEntity(handle, veh, speed);
}

//! 1728 IS_VEHICLE_IN_SEARCHLIGHT (case @0x497978): searchlight, vehicle => cmp
//! CollectParameters(2); the function at 0x493900 (CSearchLight::IsSpottedEntity); the vehicle is NOT null checked
bool IsVehicleInSearchlight(int32 handle, CVehicle& veh) {
    return CSearchLight::IsSpottedEntity(handle, veh);
}

//! 1729 CREATE_SEARCHLIGHT_ON_VEHICLE (case @0x4979B4): vehicle, x1, y1, z1, x2, y2, z2, targetRadius, baseRadius => 1 handle
//! CollectParameters(9); AddScriptSearchLight(start = offset (P1..P3), vehicle, target (P4..P6), P7, P8) [0x493000]; cleanup as 1713
int32 CreateSearchlightOnVehicle(CRunningScript& S, CVehicle* veh, CVector start, CVector target, float targetRadius, float baseRadius) {
    const auto handle = (int32)CTheScripts::AddScriptSearchLight(start, veh, target, targetRadius, baseRadius);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(handle, MISSION_CLEANUP_ENTITY_TYPE_SEARCHLIGHT);
    }
    return handle;
}

//! 1730 TASK_GO_TO_COORD_WHILE_AIMING (case @0x497A65): ped, x, y, z, moveState, targetRadius, slowDownDist, aimAt ped, aimX, aimY, aimZ
//! CollectParameters(11); the aim-at ped is only looked up if the handle is >= 0 (else null).
//! CTaskComplexGoToPointAiming(moveState, movePos, aimAt, aimPos, targetRadius, slowDownDist) [0x668790]
void TaskGoToCoordWhileAiming(
    eScriptCommands command, CRunningScript& S, int32 pedHandle,
    CVector movePos, eMoveState moveState, float targetRadius, float slowDownDist, CPed* aimAt, CVector aimPos
) {
    S.GivePedScriptedTask(
        pedHandle,
        new CTaskComplexGoToPointAiming{ moveState, movePos, aimAt, aimPos, targetRadius, slowDownDist },
        command
    );
}

//! 1731 GET_NUMBER_OF_FIRES_IN_RANGE (case @0x497B75): x, y, z, radius => 1 int
//! CollectParameters(4); CFireManager::GetNumFiresInRange(&pos, radius) [0x5397F0]
int32 GetNumberOfFiresInRange(CVector pos, float radius) {
    return (int32)gFireManager.GetNumFiresInRange(pos, radius);
}

//! 1732 ADD_BLIP_FOR_SEARCHLIGHT (case @0x497BC7): searchlight => 1 blip
//! CollectParameters(1); CRadar::SetEntityBlip(6 = SPOTLIGHT, P0, 0, 3 = BOTH, scriptName) [0x5839A0]; ChangeBlipScale(blip, 3)
int32 AddBlipForSearchlight(int32 handle) {
    const auto blip = CRadar::SetEntityBlip(BLIP_SPOTLIGHT, handle, 0, BLIP_DISPLAY_BOTH);
    CRadar::ChangeBlipScale(blip, 3);
    return blip;
}

//! 1733 SKIP_TO_END_AND_STOP_PLAYBACK_RECORDED_CAR is in Group16_17c.cpp

//! 1735 TASK_CAR_TEMP_ACTION (case @0x497C50): ped, vehicle, action, time
//! CollectParameters(4); the vehicle is only looked up if the handle != -1 (else null). CTaskSimpleCarSetTempAction(veh, action, time) [0x63D6F0]
void TaskCarTempAction(eScriptCommands command, CRunningScript& S, int32 pedHandle, CVehicle* veh, int32 action, int32 time) {
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleCarSetTempAction{ veh, (eAutoPilotTempAction)action, (uint32)time }, command);
}

//! 1736 SET_LA_RIOTS (case @0x497CDB): flag
//! CollectParameters(1); gbLARiots (0xB72958) = (P0 != 0)
void SetLaRiots(int32 on) {
    gbLARiots = on != 0;
}

//! 1738 ATTACH_SEARCHLIGHT_TO_SEARCHLIGHT_OBJECT (case @0x497D54): searchlight, tower, housing, bulb, offX, offY, offZ
//! CollectParameters(7); the 3 objects are pool GetAtRef (0xB7449C); CTheScripts::AttachSearchlightToSearchlightObject(P0, tower, housing, bulb, offset) [0x4934F0]
void AttachSearchlightToSearchlightObject(int32 handle, CObject* tower, CObject* housing, CObject* bulb, CVector offset) {
    CTheScripts::AttachSearchlightToSearchlightObject(handle, tower, housing, bulb, offset);
}

//! 1744 SWITCH_EMERGENCY_SERVICES (case @0x497DC8): flag
//! CollectParameters(1); CCarCtrl::bAllowEmergencyServicesToBeCreated (0x8A5B28) = (P0 != 0)
void SwitchEmergencyServices(int32 on) {
    CCarCtrl::bAllowEmergencyServicesToBeCreated = on != 0;
}

//! 1749 CREATE_CHECKPOINT (case @0x497E54): type, x1, y1, z1, x2, y2, z2, radius => 1 handle
//! CollectParameters(8); CTheScripts::AddScriptCheckpoint(at (P1..P3), pointTo (P4..P6), radius, type) [0x4935A0];
//! if cleanup: AddEntityToList(handle, 12 = CHECKPOINT)
int32 CreateCheckpoint(CRunningScript& S, eCheckpointType type, CVector at, CVector pointTo, float radius) {
    const auto handle = (int32)CTheScripts::AddScriptCheckpoint(at, pointTo, radius, type);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(handle, MISSION_CLEANUP_ENTITY_TYPE_CHECKPOINT);
    }
    return handle;
}

//! 1750 DELETE_CHECKPOINT (case @0x497EF9): checkpoint
//! CollectParameters(1); GetActualScriptThingIndex(P0, 3) < 0 => nothing. Else RemoveScriptCheckpoint(P0) [0x4936C0];
//! if cleanup: RemoveEntityFromList(P0, 12)
void DeleteCheckpoint(CRunningScript& S, int32 handle) {
    if (CTheScripts::GetActualScriptThingIndex(handle, SCRIPT_THING_CHECKPOINT) < 0) {
        return;
    }
    CTheScripts::RemoveScriptCheckpoint(handle);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.RemoveEntityFromList(handle, MISSION_CLEANUP_ENTITY_TYPE_CHECKPOINT);
    }
}

//! 1751 SWITCH_RANDOM_TRAINS (case @0x497F51): flag
//! CollectParameters(1); CTrain::DisableRandomTrains(P0 == 0) [0x6F5DB0]
void SwitchRandomTrains(int32 on) {
    CTrain::DisableRandomTrains(on == 0);
}

//! 1752 CREATE_MISSION_TRAIN (case @0x497F85): type, x, y, z, direction => 1 handle (the first carriage)
//! CollectParameters(5); CTrain::CreateMissionTrain(pos, direction != 0, type, &first, null, -1, -1, true) [0x6F7550]
CVehicle* CreateMissionTrain(int32 type, CVector pos, int32 direction) {
    CTrain* first{};
    CTrain::CreateMissionTrain(pos, direction != 0, (uint32)type, &first, nullptr, -1, -1, true);
    return first;
}

//! 1753 DELETE_MISSION_TRAINS (case @0x498001): CTrain::RemoveMissionTrains() [0x6F6A20]
void DeleteMissionTrains() {
    CTrain::RemoveMissionTrains();
}

//! 1754 MARK_MISSION_TRAINS_AS_NO_LONGER_NEEDED (case @0x49800D): CTrain::ReleaseMissionTrains() [0x6F6B60]
void MarkMissionTrainsAsNoLongerNeeded() {
    CTrain::ReleaseMissionTrains();
}

//! 1755 DELETE_ALL_TRAINS (case @0x498019): CTrain::RemoveAllTrains() [0x6F6AA0]
void DeleteAllTrains() {
    CTrain::RemoveAllTrains();
}

//! 1756 SET_TRAIN_SPEED (case @0x498025): train, speed
//! CollectParameters(2); CTrain::SetTrainSpeed(train, speed) [0x6F5E20] (the vehicle is not type checked / null checked)
void SetTrainSpeed(CVehicle* train, float speed) {
    CTrain::SetTrainSpeed(static_cast<CTrain*>(train), speed);
}

//! 1757 SET_TRAIN_CRUISE_SPEED (case @0x49805A): train, speed
//! CollectParameters(2); CTrain::SetTrainCruiseSpeed(train, speed) [0x6F5E50]
void SetTrainCruiseSpeed(CVehicle* train, float speed) {
    CTrain::SetTrainCruiseSpeed(static_cast<CTrain*>(train), speed);
}

//! 1758 GET_TRAIN_CABOOSE (case @0x498090): train => 1 handle
//! CollectParameters(1); CTrain::FindCaboose(train) [0x6F5E70], stored as the vehicle pool ref [0x424160]
CVehicle* GetTrainCaboose(CVehicle* train) {
    return CTrain::FindCaboose(static_cast<CTrain*>(train));
}

//! 1761 TASK_CAR_MISSION (case @0x498101): ped, vehicle, target vehicle, mission, speed, drivingStyle
//! CollectParameters(6); both vehicles are only looked up if the handle is >= 0 (else null).
//! CTaskComplexCarDriveMission(veh, target, mission (P3), style (P5), speed (P4)) [0x63CC30]
void TaskCarMission(
    eScriptCommands command, CRunningScript& S, int32 pedHandle,
    CVehicle* veh, CVehicle* targetVeh, int32 mission, float speed, int32 style
) {
    S.GivePedScriptedTask(
        pedHandle,
        new CTaskComplexCarDriveMission{ veh, targetVeh, (eCarMission)mission, (eCarDrivingStyle)style, speed },
        command
    );
}

//! 1762 TASK_GO_TO_OBJECT (case @0x4981BE): ped, object, time, radius
//! CollectParameters(4); CTaskComplexSeekEntity<Standard>(obj, time, 1000, radius, 2.0f (0x859E30), 2.0f (0x859E34), true, true) [0x46AC10]
void TaskGoToObject(eScriptCommands command, CRunningScript& S, int32 pedHandle, CObject* obj, int32 time, float radius) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexSeekEntityStandard{ obj, time, 1000, radius, 2.0f, 2.0f, true, true }, command);
}

//! 1763 TASK_WEAPON_ROLL (case @0x498259): ped, right
//! CollectParameters(2); CTaskSimpleRunAnim(group 0, anim 0x3B (right) / 0x39 (left), blend 4.0f, false) [0x61A8B0]
void TaskWeaponRoll(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 right) {
    const auto animId = right != 0 ? ANIM_ID_CROUCH_ROLL_R : ANIM_ID_CROUCH_ROLL_L; // 0x3B : 0x39
    static_assert(ANIM_ID_CROUCH_ROLL_R == 0x3B && ANIM_ID_CROUCH_ROLL_L == 0x39);
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleRunAnim{ (AssocGroupId)0, animId, 4.0f, false }, command);
}

//! 1764 TASK_CHAR_ARREST_CHAR (case @0x4982EF): ped, target ped
//! CollectParameters(2); CTaskComplexArrestPed(target) [0x68B990]
void TaskCharArrestChar(eScriptCommands command, CRunningScript& S, int32 pedHandle, CPed* target) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexArrestPed{ target }, command);
}
}; // namespace

namespace notsa::script::commands::ported::g17b {
void RegisterHandlers() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g17b");

    REGISTER_COMMAND_HANDLER(COMMAND_TASK_DIVE_FROM_ATTACHMENT_AND_GET_UP, TaskDiveFromAttachmentAndGetUp);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_GOTO_CHAR_OFFSET, TaskGotoCharOffset);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_LOOK_AT_COORD, TaskLookAtCoord);
    REGISTER_COMMAND_HANDLER(COMMAND_HIDE_CHAR_WEAPON_FOR_SCRIPTED_CUTSCENE, HideCharWeaponForScriptedCutscene);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GROUP_DECISION_MAKER, SetGroupDecisionMaker);
    REGISTER_COMMAND_HANDLER(COMMAND_LOAD_GROUP_DECISION_MAKER, LoadGroupDecisionMaker);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_SIT_DOWN, TaskSitDown);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_SEARCHLIGHT, CreateSearchlight);
    REGISTER_COMMAND_HANDLER(COMMAND_DELETE_SEARCHLIGHT, DeleteSearchlight);
    REGISTER_COMMAND_HANDLER(COMMAND_DOES_SEARCHLIGHT_EXIST, DoesSearchlightExist);
    REGISTER_COMMAND_HANDLER(COMMAND_MOVE_SEARCHLIGHT_BETWEEN_COORDS, MoveSearchlightBetweenCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_POINT_SEARCHLIGHT_AT_COORD, PointSearchlightAtCoord);
    REGISTER_COMMAND_HANDLER(COMMAND_POINT_SEARCHLIGHT_AT_CHAR, PointSearchlightAtChar);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_CUTSCENE_LOADED, HasCutsceneLoaded);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_TURN_CHAR_TO_FACE_COORD, TaskTurnCharToFaceCoord);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_DRIVE_POINT_ROUTE, TaskDrivePointRoute);
    REGISTER_COMMAND_HANDLER(COMMAND_FIRE_SINGLE_BULLET, FireSingleBullet);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_LINE_OF_SIGHT_CLEAR, IsLineOfSightClear);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_ROLL, GetCarRoll);
    REGISTER_COMMAND_HANDLER(COMMAND_POINT_SEARCHLIGHT_AT_VEHICLE, PointSearchlightAtVehicle);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_VEHICLE_IN_SEARCHLIGHT, IsVehicleInSearchlight);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_SEARCHLIGHT_ON_VEHICLE, CreateSearchlightOnVehicle);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_GO_TO_COORD_WHILE_AIMING, TaskGoToCoordWhileAiming);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NUMBER_OF_FIRES_IN_RANGE, GetNumberOfFiresInRange);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_BLIP_FOR_SEARCHLIGHT, AddBlipForSearchlight);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_CAR_TEMP_ACTION, TaskCarTempAction);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_LA_RIOTS, SetLaRiots);
    REGISTER_COMMAND_HANDLER(COMMAND_ATTACH_SEARCHLIGHT_TO_SEARCHLIGHT_OBJECT, AttachSearchlightToSearchlightObject);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_EMERGENCY_SERVICES, SwitchEmergencyServices);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_CHECKPOINT, CreateCheckpoint);
    REGISTER_COMMAND_HANDLER(COMMAND_DELETE_CHECKPOINT, DeleteCheckpoint);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_RANDOM_TRAINS, SwitchRandomTrains);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_MISSION_TRAIN, CreateMissionTrain);
    REGISTER_COMMAND_HANDLER(COMMAND_DELETE_MISSION_TRAINS, DeleteMissionTrains);
    REGISTER_COMMAND_HANDLER(COMMAND_MARK_MISSION_TRAINS_AS_NO_LONGER_NEEDED, MarkMissionTrainsAsNoLongerNeeded);
    REGISTER_COMMAND_HANDLER(COMMAND_DELETE_ALL_TRAINS, DeleteAllTrains);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_TRAIN_SPEED, SetTrainSpeed);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_TRAIN_CRUISE_SPEED, SetTrainCruiseSpeed);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_TRAIN_CABOOSE, GetTrainCaboose);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_CAR_MISSION, TaskCarMission);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_GO_TO_OBJECT, TaskGoToObject);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_WEAPON_ROLL, TaskWeaponRoll);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_CHAR_ARREST_CHAR, TaskCharArrestChar);
}
}; // namespace notsa::script::commands::ported::g17b
