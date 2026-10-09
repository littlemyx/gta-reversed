#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>

#include <bit>
#include <cstdint>

#include "World.h"
#include "General.h"
#include "CarEnterExit.h"
#include "PedGeometryAnalyser.h" // CPointRoute
#include "Ragdoll/IKChainManager.h"
#include "TaskSequences.h"
#include "TaskManager.h"

#include "TaskSimplePause.h"
#include "TaskSimpleStandStill.h"
#include "TaskComplexFallAndGetUp.h"
#include "TaskComplexJump.h"
#include "TaskSimpleTired.h"
#include "TaskComplexDie.h"
#include "TaskSimpleTriggerLookAt.h"
#include "TaskSimpleSay.h"
#include "TaskSimpleShakeFist.h"
#include "TaskSimpleAffectSecondaryBehaviour.h"
#include "TaskSimpleCower.h"
#include "TaskSimpleHandsUp.h"
#include "TaskSimpleDuck.h"
#include "TaskSimpleUseAtm.h"
#include "TaskSimpleScratchHead.h"
#include "TaskSimpleLookAbout.h"
#include "TaskComplexEnterCarAsPassengerTimed.h"
#include "TaskComplexEnterCarAsDriverTimed.h"
#include "TaskComplexLeaveCar.h"
#include "TaskComplexLeaveCarAndFlee.h"
#include "TaskComplexLeaveAnyCar.h"
#include "TaskComplexDriveToPoint.h"
#include "TaskComplexDriveWander.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskComplexGoToPointAndStandStillTimed.h"
#include "TaskSimpleAchieveHeading.h"
#include "TaskComplexFollowPointRoute.h"
#include "SeekEntity/TaskComplexSeekEntityStandard.h"
#include "TaskComplexFleeEntity.h"
#include "TaskComplexFleePoint.h"
#include "TaskComplexSmartFleePoint.h"
#include "TaskComplexSmartFleeEntity.h"
#include "TaskComplexWanderStandard.h"
#include "TaskComplexFollowNodeRoute.h"
#include "TaskComplexGoToPointAnyMeans.h"
#include "TaskComplexGoToPointShooting.h"
#include "TaskSimpleGunControl.h"
#include "TaskSimpleSetStayInSamePlace.h"

// Registered by Group13_15.cpp (owner of the central registration)
namespace notsa::script::commands::ported::g13_15 {
void RegisterTaskHandlers();
}

using namespace notsa::script;

/*!
* TASK_* script commands of the exe's group processors g14 (0x48EAA0, ids 1403..1499) and g15 (0x490DB0, ids 1500..1593)
* that had no handler of their own (S6-C part T): 41 commands.
*
* Every handler was written from the asm of the `case` (address in the comment above each handler), NOT from the Ghidra
* decompilation. Common shape: `CollectParameters(n)` -> `new CTaskXxx(...)` -> `CRunningScript::GivePedScriptedTask(ped, task, opcode)`
* (ped handle -1 = "add to the active sequence", handled inside `GivePedScriptedTask`; `opcode` = the command that is executing).
* The exe's `test eax, eax` after `operator new` is only the allocation-failure check and is not kept.
*
* Idioms:
*  - time parameters: -1 => default (20000 = 0x4E20 unless noted), -2 => "forever" (0x7FFFFFFF unless noted)
*  - pool handles: `CPed*` / `CVehicle*` parser arguments are exactly `GetAtRef(handle)` (-1 -> null);
*    where the exe tests `handle >= 0` (signed) before `GetAtRef` the handle is read as `int32` and tested the same way
*/

namespace {
//! 0xC18D50: the route that FLUSH_ROUTE / EXTEND_ROUTE build and TASK_FOLLOW_POINT_ROUTE consumes
CPointRoute& ScriptRoute() { return StaticRef<CPointRoute>(0xC18D50); }

//! 0x8D237C: set by SET_NEXT_DESIRED_MOVE_STATE, consumed (and reset to PEDMOVE_RUN = 6) by TASK_ENTER_CAR_AS_DRIVER/PASSENGER
eMoveState& NextDesiredMoveState() { return StaticRef<eMoveState>(0x8D237C); }

//! 0xC18CF0: `fEntityPosChangeThreshold`, 0xC18DB4: radius used by TASK_GO_TO_COORD_ANY_MEANS (both read as plain floats)
float& EntityPosChangeThreshold() { return StaticRef<float>(0xC18CF0); }
float& GoToAnyMeansRadius() { return StaticRef<float>(0xC18DB4); }

//! x87-compare idiom `fcom -100.0f; test ah, 0x41; jp skip`: the ground Z is looked up only when `z <= -100.0f` (ordered).
float GroundZIfAuto(float x, float y, float z) {
    if (z <= -100.0f) { // 0x859014
        return CWorld::FindGroundZForCoord(x, y);
    }
    return z;
}

//! 0x8595EC: degrees -> radians (float)
constexpr float DEG_TO_RAD_F = std::bit_cast<float>(0x3C8EFA35u);

//! 0x48FB0E TASK_PAUSE: ped, time
//! CollectParameters(2); CTaskSimplePause(time) [0x48E750]
void TaskPause(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 time) {
    S.GivePedScriptedTask(pedHandle, new CTaskSimplePause{ time }, command);
}

//! 0x48FB82 TASK_STAND_STILL: ped, time
//! CollectParameters(2); time -1 => 20000 (0x86DB24), -2 => (999999 = 0xF423F, looped); CTaskSimpleStandStill [0x62F310]
void TaskStandStill(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 time) {
    CTask* task;
    if (time == -1) {
        task = new CTaskSimpleStandStill{ 20000, false, false, 8.0f };
    } else if (time == -2) {
        task = new CTaskSimpleStandStill{ 999999, true, false, 8.0f };
    } else {
        task = new CTaskSimpleStandStill{ time, false, false, 8.0f };
    }
    S.GivePedScriptedTask(pedHandle, task, command);
}

//! 0x48FC9D TASK_FALL_AND_GET_UP: ped, dir, time
//! CollectParameters(3); CTaskComplexFallAndGetUp(dir, time) [0x678700]
void TaskFallAndGetUp(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 dir, int32 time) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexFallAndGetUp{ dir, time }, command);
}

//! 0x48FCE7 TASK_JUMP: ped, bool
//! CollectParameters(2); CTaskComplexJump(OK) [0x67A030]; byte [task + 0x10] = (flag != 0)
void TaskJump(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 flag) {
    auto* const task = new CTaskComplexJump{ CTaskComplexJump::eForceClimb::OK };
    static_assert(offsetof(CTaskComplexJump, m_UsePlayerLaunchForce) == 0x10);
    task->m_UsePlayerLaunchForce = flag != 0;
    S.GivePedScriptedTask(pedHandle, task, command);
}

//! 0x48FD4A TASK_TIRED: ped, time
//! CollectParameters(2); CTaskSimpleTired(time) [0x630F20]
void TaskTired(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 time) {
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleTired{ (uint32)time }, command);
}

//! 0x48FD8D TASK_DIE: ped
//! CollectParameters(1); CTaskComplexDie(0, 0, 0xF, 4.0f, 0.0f, false, false, 0, false) [0x630040]
void TaskDie(eScriptCommands command, CRunningScript& S, int32 pedHandle) {
    S.GivePedScriptedTask(
        pedHandle,
        new CTaskComplexDie{
            WEAPON_UNARMED,
            (AssocGroupId)0,
            (AnimationId)0xF,
            4.0f,
            0.0f,
            false,
            false,
            eDirection::FORWARD,
            false
        },
        command
    );
}

//! 0x48FE0F TASK_LOOK_AT_CHAR: ped, target, time
//! CollectParameters(3); time -1 => 20000, -2 => -1.
//! ped != -1: IKChainManager_c::LookAt [0x618970] right away (nothing is given to the ped);
//! ped == -1: CTaskSimpleTriggerLookAt [0x634440] appended to the active sequence [CTaskComplexSequence::AddTask 0x632D10]
void TaskLookAtChar(int32 pedHandle, CPed* target, int32 time) {
    if (time == -1) {
        time = 20000;
    } else if (time == -2) {
        time = -1;
    }
    if (pedHandle != -1) {
        g_ikChainMan.LookAt(
            "COMMAND_TASK_LOOK_AT_CHAR", // 0x85A2EC
            GetPedPool()->GetAtRef(pedHandle),
            target,
            time,
            BONE_HEAD, // 5
            nullptr,
            false,
            0.125f,
            500,
            6,
            true
        );
        return;
    }
    CTaskSequences::AddTaskToActiveSequence(
        new CTaskSimpleTriggerLookAt{ target, time, BONE_HEAD, RwV3d{ 0.0f, 0.0f, 0.0f }, true, 0.125f, 1000, 3 }
    );
}

//! 0x48FF19 TASK_LOOK_AT_VEHICLE: ped, vehicle, time
//! CollectParameters(3); time -1 => 20000, -2 => 0x7FFFFFFF. Same as TASK_LOOK_AT_CHAR, but bone -1 and a vehicle target
void TaskLookAtVehicle(int32 pedHandle, CVehicle* target, int32 time) {
    if (time == -1) {
        time = 20000;
    } else if (time == -2) {
        time = 0x7FFFFFFF;
    }
    if (pedHandle != -1) {
        g_ikChainMan.LookAt(
            "COMMAND_TASK_LOOK_AT_VEHICLE", // 0x85A2CC
            GetPedPool()->GetAtRef(pedHandle),
            target,
            time,
            BONE_UNKNOWN, // -1
            nullptr,
            false,
            0.125f,
            500,
            6,
            true
        );
        return;
    }
    CTaskSequences::AddTaskToActiveSequence(
        new CTaskSimpleTriggerLookAt{ target, time, BONE_UNKNOWN, RwV3d{ 0.0f, 0.0f, 0.0f }, true, 0.125f, 1000, 3 }
    );
}

//! 0x490006 TASK_SAY: ped, speech context
//! CollectParameters(2); CTaskSimpleSay(ctx, -1) [0x48E360].
//! ped != -1: ped's secondary task SAY (2) [CTaskManager::SetTaskSecondary 0x681B60];
//! ped == -1: CTaskSimpleAffectSecondaryBehaviour(true, SAY, task) [0x691270] appended to the active sequence
void TaskSay(int32 pedHandle, int32 context) {
    auto* const task = new CTaskSimpleSay{ (eGlobalSpeechContext)context, -1 };
    if (pedHandle != -1) {
        auto* const ped = GetPedPool()->GetAtRef(pedHandle);
        assert(ped);
        ped->GetTaskManager().SetTaskSecondary(task, TASK_SECONDARY_SAY);
        return;
    }
    CTaskSequences::AddTaskToActiveSequence(new CTaskSimpleAffectSecondaryBehaviour{ true, TASK_SECONDARY_SAY, task });
}

//! 0x4900B2 TASK_SHAKE_FIST: ped
//! CollectParameters(1); CTaskSimpleShakeFist() [0x690B80]; same secondary-task logic as TASK_SAY with PARTIAL_ANIM (4)
void TaskShakeFist(int32 pedHandle) {
    auto* const task = new CTaskSimpleShakeFist{};
    if (pedHandle != -1) {
        auto* const ped = GetPedPool()->GetAtRef(pedHandle);
        assert(ped);
        ped->GetTaskManager().SetTaskSecondary(task, TASK_SECONDARY_PARTIAL_ANIM);
        return;
    }
    CTaskSequences::AddTaskToActiveSequence(new CTaskSimpleAffectSecondaryBehaviour{ true, TASK_SECONDARY_PARTIAL_ANIM, task });
}

//! 0x490143 TASK_COWER: ped
//! CollectParameters(1); CTaskSimpleCower() [0x48DE70]
void TaskCower(eScriptCommands command, CRunningScript& S, int32 pedHandle) {
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleCower{}, command);
}

//! 0x49017F TASK_HANDS_UP: ped, time
//! CollectParameters(2); time -1 => 20000, -2 => 0x7FFFFFFF; CTaskSimpleHandsUp(time) [0x48E970]
void TaskHandsUp(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 time) {
    if (time == -1) {
        time = 20000;
    } else if (time == -2) {
        time = 0x7FFFFFFF;
    }
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleHandsUp{ (uint32)time }, command);
}

//! 0x4901D8 TASK_DUCK: ped, time
//! CollectParameters(2); CTaskSimpleDuck(0, length, -1) [0x691FC0]:
//!   -2 => length 0, -1 => length 20000 (word 0x870A8C), else length = time
void TaskDuck(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 time) {
    CTask* task;
    if (time == -2) {
        task = new CTaskSimpleDuck{ (eDuckControlType)0, 0, -1 };
    } else if (time == -1) {
        task = new CTaskSimpleDuck{ (eDuckControlType)0, (uint16)20000, -1 };
    } else {
        task = new CTaskSimpleDuck{ (eDuckControlType)0, (uint16)time, -1 };
    }
    S.GivePedScriptedTask(pedHandle, task, command);
}

//! 0x49029B TASK_USE_ATM: ped
//! CollectParameters(1); CTaskSimpleUseAtm() [0x48DFE0]
void TaskUseAtm(eScriptCommands command, CRunningScript& S, int32 pedHandle) {
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleUseAtm{}, command);
}

//! 0x4902D7 TASK_SCRATCH_HEAD: ped
//! CollectParameters(1); CTaskSimpleScratchHead() [0x48DF30]
void TaskScratchHead(eScriptCommands command, CRunningScript& S, int32 pedHandle) {
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleScratchHead{}, command);
}

//! 0x490313 TASK_LOOK_ABOUT: ped, time
//! CollectParameters(2); time -1 => 20000, -2 => 0x7FFFFFFF; CTaskSimpleLookAbout(time) [0x48E0A0]
void TaskLookAbout(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 time) {
    if (time == -1) {
        time = 20000;
    } else if (time == -2) {
        time = 0x7FFFFFFF;
    }
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleLookAbout{ (uint32)time }, command);
}

//! 0x49036C TASK_ENTER_CAR_AS_PASSENGER: ped, vehicle, time, seat
//! CollectParameters(4); door = (seat != -1) ? CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(veh, seat) [0x64F190] : 0;
//! CTaskComplexEnterCarAsPassengerTimed(veh, door, time, true) [0x63B030] with time -2 => -1, -1 => 20000 (0x86E69C);
//! then task->moveState (+0x1C) = [0x8D237C] and [0x8D237C] = 6 (PEDMOVE_RUN)
void TaskEnterCarAsPassenger(eScriptCommands command, CRunningScript& S, int32 pedHandle, CVehicle* veh, int32 time, int32 seat) {
    int32 door = 0;
    if (seat != -1) {
        door = CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(veh, seat);
    }
    if (time == -2) {
        time = -1;
    } else if (time == -1) {
        time = 20000;
    }
    auto* const task = new CTaskComplexEnterCarAsPassengerTimed{ veh, (uint32)door, (uint32)time, true };
    task->SetMoveState(NextDesiredMoveState());
    NextDesiredMoveState() = PEDMOVE_RUN;
    S.GivePedScriptedTask(pedHandle, task, command);
}

//! 0x49046B TASK_ENTER_CAR_AS_DRIVER: ped, vehicle, time
//! CollectParameters(3); CTaskComplexEnterCarAsDriverTimed(veh, time) [0x63AD70] with time -2 => -1, -1 => 20000 (0x86E698);
//! then task->moveState (+0x14) = [0x8D237C] and [0x8D237C] = 6 (PEDMOVE_RUN)
void TaskEnterCarAsDriver(eScriptCommands command, CRunningScript& S, int32 pedHandle, CVehicle* veh, int32 time) {
    if (time == -2) {
        time = -1;
    } else if (time == -1) {
        time = 20000;
    }
    auto* const task = new CTaskComplexEnterCarAsDriverTimed{ veh, time };
    task->m_moveState = NextDesiredMoveState();
    NextDesiredMoveState() = PEDMOVE_RUN;
    S.GivePedScriptedTask(pedHandle, task, command);
}

//! 0x490554 TASK_LEAVE_CAR: ped, vehicle
//! CollectParameters(2); CTaskComplexLeaveCar(veh, 0, 0, true, false) [0x63B8C0]
void TaskLeaveCar(eScriptCommands command, CRunningScript& S, int32 pedHandle, CVehicle* veh) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexLeaveCar{ veh, 0, 0, true, false }, command);
}

//! 0x4905AC TASK_LEAVE_CAR_AND_FLEE: ped, vehicle, x, y, z
//! CollectParameters(5); CTaskComplexLeaveCarAndFlee(veh, &(x, y, z), 0, 0, false) [0x63BF90]
void TaskLeaveCarAndFlee(eScriptCommands command, CRunningScript& S, int32 pedHandle, CVehicle* veh, float x, float y, float z) {
    const CVector fleePoint{ x, y, z };
    S.GivePedScriptedTask(pedHandle, new CTaskComplexLeaveCarAndFlee{ veh, fleePoint, TARGET_DOOR_FRONT_LEFT, 0, false }, command);
}

//! 0x490649 TASK_CAR_DRIVE_TO_COORD: ped, vehicle, x, y, z, speed, arg4, model mode, driving style
//! CollectParameters(9); veh = (handle >= 0 (signed)) ? GetAtRef : null; z = (z <= -100.0f) ? FindGroundZForCoord(x, y) : z;
//! model mode 0 => -1, 1 => 0x19F, else raw; CTaskComplexDriveToPoint(veh, &(x, y, z), speed, arg4, model, -1.0f, style) [0x63CE00]
void TaskCarDriveToCoord(
    eScriptCommands command,
    CRunningScript& S,
    int32           pedHandle,
    int32           vehHandle,
    float           x,
    float           y,
    float           z,
    float           speed,
    int32           arg4,
    int32           modelMode,
    int32           drivingStyle
) {
    CVehicle* veh = nullptr;
    if (vehHandle >= 0) {
        veh = GetVehiclePool()->GetAtRef(vehHandle);
    }
    const CVector point{ x, y, GroundZIfAuto(x, y, z) };
    if (modelMode == 0) {
        modelMode = -1;
    } else if (modelMode == 1) {
        modelMode = 0x19F;
    }
    S.GivePedScriptedTask(
        pedHandle,
        new CTaskComplexDriveToPoint{ veh, point, speed, arg4, (eModelID)modelMode, -1.0f, (eCarDrivingStyle)drivingStyle },
        command
    );
}

//! 0x490762 TASK_CAR_DRIVE_WANDER: ped, vehicle, speed, driving style
//! CollectParameters(4); veh = (handle >= 0 (signed)) ? GetAtRef : null; CTaskComplexCarDriveWander(veh, style, speed) [0x63CB10]
void TaskCarDriveWander(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 vehHandle, float speed, int32 drivingStyle) {
    CVehicle* veh = nullptr;
    if (vehHandle >= 0) {
        veh = GetVehiclePool()->GetAtRef(vehHandle);
    }
    S.GivePedScriptedTask(pedHandle, new CTaskComplexCarDriveWander{ veh, (eCarDrivingStyle)drivingStyle, speed }, command);
}

//! 0x4907CE TASK_GO_STRAIGHT_TO_COORD: ped, x, y, z, move state, time
//! CollectParameters(6); radius 0.5f (0x86FC84), move state radius 2.0f (0x86FC88);
//!  time -2 => CTaskComplexGoToPointAndStandStill(ms, &pt, 0.5, 2.0, false, true) [0x668120]
//!  time -1 => CTaskComplexGoToPointAndStandStillTimed(ms, &pt, 0.5, 2.0, 20000 (0x86FC8C)) [0x6685E0]
//!  else    => CTaskComplexGoToPointAndStandStillTimed(ms, &pt, 0.5, 2.0, time)
void TaskGoStraightToCoord(eScriptCommands command, CRunningScript& S, int32 pedHandle, float x, float y, float z, int32 moveState, int32 time) {
    const CVector point{ x, y, z };
    CTask*        task;
    if (time == -2) {
        task = new CTaskComplexGoToPointAndStandStill{ (eMoveState)moveState, point, 0.5f, 2.0f, false, true };
    } else if (time == -1) {
        task = new CTaskComplexGoToPointAndStandStillTimed{ (eMoveState)moveState, point, 0.5f, 2.0f, 20000 };
    } else {
        task = new CTaskComplexGoToPointAndStandStillTimed{ (eMoveState)moveState, point, 0.5f, 2.0f, time };
    }
    S.GivePedScriptedTask(pedHandle, task, command);
}

//! 0x49090B TASK_ACHIEVE_HEADING: ped, heading (degrees)
//! CollectParameters(2); heading * 0.017453292f (0x8595EC, x87 product stored as float);
//! CTaskSimpleAchieveHeading(heading, 0.5f (0x86FC7C), 0.2f (0x86FC80)) [0x667E20]
void TaskAchieveHeading(eScriptCommands command, CRunningScript& S, int32 pedHandle, float heading) {
    const float rad = heading * DEG_TO_RAD_F;
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleAchieveHeading{ rad, 0.5f, std::bit_cast<float>(0x3E4CCCCDu) }, command);
}

//! 0x490981 FLUSH_ROUTE: no parameters; the number of route entries (dword at 0xC18D50) = 0
void FlushRoute() {
    ScriptRoute().Clear();
}

//! 0x490992 EXTEND_ROUTE: x, y, z
//! CollectParameters(3); appends (x, y, z) unless the route (8 entries) is full [0x48E180]
void ExtendRoute(float x, float y, float z) {
    ScriptRoute().AddUnlessFull(CVector{ x, y, z });
}

//! 0x4909D4 TASK_FOLLOW_POINT_ROUTE: ped, move state, mode
//! CollectParameters(3); CTaskComplexFollowPointRoute(ms, route (0xC18D50), mode, 0.5f (0x86FC98), 5.0f (0x86FC9C), false, true, true) [0x671510]
void TaskFollowPointRoute(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 moveState, int32 mode) {
    S.GivePedScriptedTask(
        pedHandle,
        new CTaskComplexFollowPointRoute{
            (eMoveState)moveState,
            ScriptRoute(),
            (CTaskComplexFollowPointRoute::Mode)mode,
            0.5f,
            5.0f,
            false,
            true,
            true
        },
        command
    );
}

//! 0x490A55 TASK_GOTO_CHAR: ped, target, time, radius
//! CollectParameters(4); time -2 => -1, -1 => 50000 (0xC350);
//! CTaskComplexSeekEntity<Standard>(target, time, 1000, radius, 2.0f (0x859E30), 2.0f (0x859E34), true, true) [0x46AC10]
void TaskGotoChar(eScriptCommands command, CRunningScript& S, int32 pedHandle, CPed* target, int32 time, float radius) {
    if (time == -2) {
        time = -1;
    } else if (time == -1) {
        time = 50000;
    }
    S.GivePedScriptedTask(pedHandle, new CTaskComplexSeekEntityStandard{ target, time, 1000, radius, 2.0f, 2.0f, true, true }, command);
}

//! 0x490AE6 TASK_FLEE_POINT: ped, x, y, z, radius, time
//! CollectParameters(6); CTaskComplexFleePoint(&(x, y, z), true, radius, time) [0x65B390]
void TaskFleePoint(eScriptCommands command, CRunningScript& S, int32 pedHandle, float x, float y, float z, float radius, int32 time) {
    const CVector point{ x, y, z };
    S.GivePedScriptedTask(pedHandle, new CTaskComplexFleePoint{ point, true, radius, time }, command);
}

//! 0x490B7E TASK_FLEE_CHAR: ped, target, safe distance (int, converted with fild), time
//! CollectParameters(4); CTaskComplexFleeEntity(target, true, (float)dist, time, 1000 (0x86F664), 1.0f (0x86F668)) [0x65B930]
void TaskFleeChar(eScriptCommands command, CRunningScript& S, int32 pedHandle, CPed* target, int32 safeDistance, int32 time) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexFleeEntity{ target, true, (float)safeDistance, time, 1000, 1.0f }, command);
}

//! 0x490DEE TASK_SMART_FLEE_POINT: ped, x, y, z, safe distance, time
//! CollectParameters(6); CTaskComplexSmartFleePoint(&(x, y, z), true, safeDist, time) [0x65BD20]
void TaskSmartFleePoint(eScriptCommands command, CRunningScript& S, int32 pedHandle, float x, float y, float z, float safeDistance, int32 time) {
    const CVector point{ x, y, z };
    S.GivePedScriptedTask(pedHandle, new CTaskComplexSmartFleePoint{ point, true, safeDistance, time }, command);
}

//! 0x490E63 TASK_SMART_FLEE_CHAR: ped, target, safe distance (int, converted with fild), time
//! CollectParameters(4); CTaskComplexSmartFleeEntity(target, true, (float)dist, time, 1000 (0x86F678), [0xC18CF0]) [0x65C430]
void TaskSmartFleeChar(eScriptCommands command, CRunningScript& S, int32 pedHandle, CPed* target, int32 safeDistance, int32 time) {
    S.GivePedScriptedTask(
        pedHandle,
        new CTaskComplexSmartFleeEntity{ target, true, (float)safeDistance, time, 1000, EntityPosChangeThreshold() },
        command
    );
}

//! 0x490EF7 TASK_WANDER_STANDARD: ped
//! CollectParameters(1); CTaskComplexWanderStandard(4 (WALK), GetRandomNumberInRange(0, 8) [0x407180], true) [0x48E4F0]
void TaskWanderStandard(eScriptCommands command, CRunningScript& S, int32 pedHandle) {
    auto* const task = new CTaskComplexWanderStandard{ PEDMOVE_WALK, (uint8)CGeneral::GetRandomNumberInRange(0, 8), true };
    S.GivePedScriptedTask(pedHandle, task, command);
}

//! 0x4911EF TASK_FOLLOW_PATH_NODES_TO_COORD: ped, x, y, z, move state, time
//! CollectParameters(6); time -1 => 50000 (0x86FCAC), -2 => -1;
//! CTaskComplexFollowNodeRoute(ms, &pt, 0.5f (0x86FCA0), 3.0f (0x86FCA4), 2.0f (0x86FCA8), true, time, true) [0x66EA30]
void TaskFollowPathNodesToCoord(eScriptCommands command, CRunningScript& S, int32 pedHandle, float x, float y, float z, int32 moveState, int32 time) {
    if (time == -1) {
        time = 50000;
    } else if (time == -2) {
        time = -1;
    }
    const CVector point{ x, y, z };
    S.GivePedScriptedTask(
        pedHandle,
        new CTaskComplexFollowNodeRoute{ (eMoveState)moveState, point, 0.5f, 3.0f, 2.0f, true, time, true },
        command
    );
}

//! 0x4912F4 TASK_GO_TO_COORD_ANY_MEANS: ped, x, y, z, move state, vehicle
//! CollectParameters(6); veh = (handle >= 0 (signed)) ? GetAtRef : null;
//! CTaskComplexGoToPointAnyMeans(ms, &pt, veh, [0xC18DB4], -1) [0x66B790]
void TaskGoToCoordAnyMeans(eScriptCommands command, CRunningScript& S, int32 pedHandle, float x, float y, float z, int32 moveState, int32 vehHandle) {
    CVehicle* veh = nullptr;
    if (vehHandle >= 0) {
        veh = GetVehiclePool()->GetAtRef(vehHandle);
    }
    const CVector point{ x, y, z };
    S.GivePedScriptedTask(pedHandle, new CTaskComplexGoToPointAnyMeans{ moveState, point, veh, GoToAnyMeansRadius(), -1 }, command);
}

//! 0x4913F5 TASK_PLAY_ANIM: no CollectParameters here, CRunningScript::PlayAnimScriptCommand(command) [0x470150] reads everything
void TaskPlayAnim(eScriptCommands command, CRunningScript& S) {
    S.PlayAnimScriptCommand(command);
}

//! 0x491FFF TASK_LEAVE_CAR_IMMEDIATELY: ped, vehicle
//! CollectParameters(2); CTaskComplexLeaveCar(veh, 0, 0, false, false) [0x63B8C0]
void TaskLeaveCarImmediately(eScriptCommands command, CRunningScript& S, int32 pedHandle, CVehicle* veh) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexLeaveCar{ veh, 0, 0, false, false }, command);
}

//! 0x49277A TASK_LEAVE_ANY_CAR: ped
//! CollectParameters(1); CTaskComplexLeaveAnyCar(0, true, false) [0x421150]
void TaskLeaveAnyCar(eScriptCommands command, CRunningScript& S, int32 pedHandle) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexLeaveAnyCar{ 0, true, false }, command);
}

//! 0x49283D TASK_AIM_GUN_AT_CHAR: ped, target, time
//! CollectParameters(3); CTaskSimpleGunControl(target, null, null, 0 (NONE), 1, time) [0x61F3F0] (null vectors => zero vectors)
void TaskAimGunAtChar(eScriptCommands command, CRunningScript& S, int32 pedHandle, CPed* target, int32 time) {
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleGunControl{ target, CVector{}, CVector{}, eGunCommand::NONE, 1, time }, command);
}

//! 0x4928AD TASK_GO_TO_COORD_WHILE_SHOOTING: ped, x, y, z, move state, radius, slow down dist, target
//! CollectParameters(8); CTaskComplexGoToPointShooting(ms, &(x, y, z), target, (0, 0, 0), radius, slowDown) [0x668C70]
void TaskGoToCoordWhileShooting(
    eScriptCommands command,
    CRunningScript& S,
    int32           pedHandle,
    float           x,
    float           y,
    float           z,
    int32           moveState,
    float           radius,
    float           slowDownDist,
    CPed*           target
) {
    const CVector point{ x, y, z };
    S.GivePedScriptedTask(
        pedHandle,
        new CTaskComplexGoToPointShooting{ (eMoveState)moveState, point, target, CVector{ 0.0f, 0.0f, 0.0f }, radius, slowDownDist },
        command
    );
}

//! 0x492975 TASK_STAY_IN_SAME_PLACE: ped, bool
//! CollectParameters(2); CTaskSimpleSetStayInSamePlace(flag != 0) [0x62F590];
//! ped != -1: a stack task, ProcessPed(ped) [0x62F5E0] right away; ped == -1: heap task appended to the active sequence
void TaskStayInSamePlace(int32 pedHandle, int32 flag) {
    if (pedHandle != -1) {
        auto* const ped = GetPedPool()->GetAtRef(pedHandle);
        CTaskSimpleSetStayInSamePlace task{ flag != 0 };
        task.ProcessPed(ped);
        return;
    }
    CTaskSequences::AddTaskToActiveSequence(new CTaskSimpleSetStayInSamePlace{ flag != 0 });
}
}; // namespace

void notsa::script::commands::ported::g13_15::RegisterTaskHandlers() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g13-15 tasks");

    // g14
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_PAUSE, TaskPause);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_STAND_STILL, TaskStandStill);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_FALL_AND_GET_UP, TaskFallAndGetUp);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_JUMP, TaskJump);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_TIRED, TaskTired);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_DIE, TaskDie);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_LOOK_AT_CHAR, TaskLookAtChar);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_LOOK_AT_VEHICLE, TaskLookAtVehicle);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_SAY, TaskSay);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_SHAKE_FIST, TaskShakeFist);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_COWER, TaskCower);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_HANDS_UP, TaskHandsUp);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_DUCK, TaskDuck);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_USE_ATM, TaskUseAtm);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_SCRATCH_HEAD, TaskScratchHead);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_LOOK_ABOUT, TaskLookAbout);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_ENTER_CAR_AS_PASSENGER, TaskEnterCarAsPassenger);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_ENTER_CAR_AS_DRIVER, TaskEnterCarAsDriver);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_LEAVE_CAR, TaskLeaveCar);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_LEAVE_CAR_AND_FLEE, TaskLeaveCarAndFlee);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_CAR_DRIVE_TO_COORD, TaskCarDriveToCoord);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_CAR_DRIVE_WANDER, TaskCarDriveWander);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_GO_STRAIGHT_TO_COORD, TaskGoStraightToCoord);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_ACHIEVE_HEADING, TaskAchieveHeading);
    REGISTER_COMMAND_HANDLER(COMMAND_FLUSH_ROUTE, FlushRoute);
    REGISTER_COMMAND_HANDLER(COMMAND_EXTEND_ROUTE, ExtendRoute);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_FOLLOW_POINT_ROUTE, TaskFollowPointRoute);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_GOTO_CHAR, TaskGotoChar);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_FLEE_POINT, TaskFleePoint);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_FLEE_CHAR, TaskFleeChar);

    // g15
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_SMART_FLEE_POINT, TaskSmartFleePoint);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_SMART_FLEE_CHAR, TaskSmartFleeChar);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_WANDER_STANDARD, TaskWanderStandard);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_FOLLOW_PATH_NODES_TO_COORD, TaskFollowPathNodesToCoord);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_GO_TO_COORD_ANY_MEANS, TaskGoToCoordAnyMeans);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_PLAY_ANIM, TaskPlayAnim);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_LEAVE_CAR_IMMEDIATELY, TaskLeaveCarImmediately);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_LEAVE_ANY_CAR, TaskLeaveAnyCar);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_AIM_GUN_AT_CHAR, TaskAimGunAtChar);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_GO_TO_COORD_WHILE_SHOOTING, TaskGoToCoordWhileShooting);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_STAY_IN_SAME_PLACE, TaskStayInSamePlace);
}
