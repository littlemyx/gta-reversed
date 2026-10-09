#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group20_21.hpp"

#include <bit>
#include <cstddef>

#include "World.h"
#include "Timer.h"
#include "Darkel.h"
#include "StuntJumpManager.h"
#include "ModelIndices.h"
#include "PatrolRoute.h"
#include "PedIntelligence.h"
#include "Models/ModelInfo.h"
#include "Entity/Object/Object.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Plane.h"
#include "Entity/Vehicle/Train.h" // MarkSurroundingEntitiesForCollisionWithTrain
#include "TaskSequences.h"
#include "TaskManager.h"
#include "TaskComplexSequence.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleSlideToCoord.h"
#include "TaskComplexFollowPatrolRoute.h"
#include "TaskComplexPartnerGreet.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g09_12;
using namespace notsa::script::commands::ported::g20_21;

/*!
* Script commands of the exe's group processor for ids 2000..2099 (`CRunningScript::ProcessCommands2000To2099`), ids 2045..2086
* (S6-F, part 20b): the 25 vanilla commands of that range that had no handler of their own.
* Written from the asm of each `case` (the case address is in the comment above each handler).
* Convention: `CollectParameters(n)` of the exe => the handler arguments; where the exe copies the raw dwords of `ScriptParams`
* (and the kind of the value matters, e.g. floats of the ground-Z / x87 sequences) the handler reads `ScriptParams` itself.
*/

namespace {
//! 0x8595EC: degrees -> radians (float)
constexpr float DEG_TO_RAD_F = std::bit_cast<float>(0x3C8EFA35u);

// Raw offsets the asm relies on
static_assert(offsetof(CPed, m_nWeaponSkill) == 0x72C);
static_assert(offsetof(CPed, m_nFightingStyle) == 0x72D);
static_assert(offsetof(CPed, m_nAllowedAttackMoves) == 0x72E);
static_assert(sizeof(eFightingStyle) == 1 && sizeof(eWeaponSkill) == 1);
static_assert(offsetof(CPedIntelligence, m_nEventId) == 0xD1);
static_assert(offsetof(CVehicle, handlingFlags) == 0x38C);
static_assert(offsetof(CVehicle, vehicleFlags) == 0x428);
static_assert(offsetof(CVehicle, m_nVehicleSubType) == 0x594);
static_assert(offsetof(CAutomobile, m_fHeliRotorSpeed) == 0x84C);
static_assert(offsetof(CPlane, m_fPropSpeed) == 0x9C4);
static_assert(offsetof(CPhysical, m_vecMoveSpeed) == 0x44);
static_assert(TASK_SIMPLE_FALL == 0xCF);

//! 0x4119D0 - `v / s`. x87: the reciprocal is kept in extended precision for the Z, and rounded to float for X and Y
CVector DivideOriginal(const CVector& v, float s) {
    const double recip  = 1.0 / (double)s;
    const double recipF = (double)(float)recip;
    return {
        (float)((double)v.x * recipF),
        (float)((double)v.y * recipF),
        (float)((double)v.z * recip),
    };
}

//! 0xC18DB8: the patrol route the script commands (EXTEND_PATROL_ROUTE, ...) build, consumed by TASK_FOLLOW_PATROL_ROUTE
CPatrolRoute& ScriptPatrolRoute() { return StaticRef<CPatrolRoute>(0xC18DB8); }

//! 2045 DOES_GROUP_EXIST (case @0x4731CE): group => compare flag
//! `CTheScripts::GetActualScriptThingIndex(handle, 8 [SCRIPT_THING_PED_GROUP])` in [0, 8) (signed)
bool DoesGroupExist(int32 groupHandle) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(groupHandle, SCRIPT_THING_PED_GROUP); // 0x4839A0
    return idx >= 0 && idx < 8;
}

//! 2046 GIVE_MELEE_ATTACK_TO_CHAR (case @0x47320D): ped, fighting style, moves  (the ped isn't null checked)
//! style: compared (zero extended byte vs the full dword) with +0x72D; if different: +0x72D = (uint8)style, +0x72E (allowed moves) = 0.
//! moves (switch on `moves - 1` as unsigned, jump table @0x474520): 1 => moves = 0, 2 => |= 1, 3 => |= 2, 4 => |= 4, 5 => |= 8, 6 => |= 0xF, else nothing
void GiveMeleeAttackToChar(CPed& ped, int32 style, int32 moves) {
    if ((int32)(uint8)ped.m_nFightingStyle != style) {
        ped.m_nFightingStyle      = (eFightingStyle)(int8)(uint8)style; // `mov [eax + 0x72D], cl`
        ped.m_nAllowedAttackMoves = 0;
    }
    switch ((uint32)(moves - 1)) {
    case 0: ped.m_nAllowedAttackMoves = 0; break;
    case 1: ped.m_nAllowedAttackMoves |= 1; break;
    case 2: ped.m_nAllowedAttackMoves |= 2; break;
    case 3: ped.m_nAllowedAttackMoves |= 4; break;
    case 4: ped.m_nAllowedAttackMoves |= 8; break;
    case 5: ped.m_nAllowedAttackMoves |= 0xF; break;
    default: break;
    }
}

//! 2047 SET_CAR_HYDRAULICS (case @0x4732B1): car, on  (the car isn't null checked)
//! on != 0 => CVehicle::AddVehicleUpgrade(MI_HYDRAULICS) [0x6E3290] (result ignored), else CVehicle::RemoveVehicleUpgrade(MI_HYDRAULICS) [0x6DF930]
//! MI_HYDRAULICS = `movzx word [0x8CD76C]`
void SetCarHydraulics(CVehicle& veh, int32 on) {
    const int32 model = (int32)ModelIndices::MI_HYDRAULICS.get_underlying();
    if (on != 0) {
        veh.AddVehicleUpgrade(model);
    } else {
        veh.RemoveVehicleUpgrade(model);
    }
}

//! 2048 IS_2PLAYER_GAME_GOING_ON (case @0x4732FE): no params => compare flag = CGameLogic::IsCoopGameGoingOn() [0x441390]
bool Is2PlayerGameGoingOn() {
    return CGameLogic::IsCoopGameGoingOn();
}

//! 2051 DOES_CAR_HAVE_HYDRAULICS (case @0x473350): car => compare flag  (the car isn't null checked)
//! `subtype (+0x594) == 0 [VEHICLE_TYPE_AUTOMOBILE] && handlingFlags (+0x38C) & 0x20000 [bHydraulicInst]`
bool DoesCarHaveHydraulics(CVehicle& veh) {
    return veh.m_nVehicleSubType == VEHICLE_TYPE_AUTOMOBILE && veh.handlingFlags.bHydraulicInst;
}

//! 2052 TASK_CHAR_SLIDE_TO_COORD_AND_PLAY_ANIM (case @0x4733A6)
//! CollectParameters(6): ped, x, y, z, heading (degrees), speed;  2 text labels: anim name (24), anim group (16);
//! CollectParameters(6): blendDelta (float), b, c, d, e, time.
//! Builds a sequence { CTaskComplexGoToPointAndStandStill(walk, pos, 0.5, 2.0), CTaskSimpleSlideToCoord(pos, heading, speed, anim, group, flags, blend, runInSeq, time) }
//! heading = (float)(heading * 0.017453292f), speed < 0 => 0.1f (NaN stays NaN)
//! slide flags: 0x10; 0x12 if (b != 0 || time > 0); |= 0x40 if c != 0; |= 0x80 if d != 0; |= 8 if e == 0
//! runInSeq = CTaskSequences::ms_iActiveSequence [0x8D2E98] >= 0; time = (time > 0) ? time : -1 (signed test)
void TaskCharSlideToCoordAndPlayAnim(eScriptCommands command, CRunningScript& S) {
    S.CollectParameters(6); // 0x464080
    const int32   pedHandle = ScriptParams[0].iParam;
    const CVector pos{ ScriptParams[1].fParam, ScriptParams[2].fParam, ScriptParams[3].fParam };
    const float   heading   = (float)((double)ScriptParams[4].fParam * (double)DEG_TO_RAD_F); // x87 product, stored as float
    float         speed     = ScriptParams[5].fParam;
    if (speed < 0.0f) { // 0x858B50
        speed = 0.1f;
    }

    char animName[24]{}; // NOTSA: zero-initialised (the exe leaves the 8-char label unterminated)
    char animGroup[16]{};
    S.ReadTextLabelFromScript(animName, sizeof(animName));  // 0x463D50
    S.ReadTextLabelFromScript(animGroup, sizeof(animGroup));

    S.CollectParameters(6);
    const float blendDelta = ScriptParams[0].fParam;
    const int32 b          = ScriptParams[1].iParam;
    const int32 c          = ScriptParams[2].iParam;
    const int32 d          = ScriptParams[3].iParam;
    const int32 e          = ScriptParams[4].iParam;
    const int32 time       = ScriptParams[5].iParam;

    uint32 flags = 0x10;
    if (b != 0 || time > 0) {
        flags = 0x12;
    }
    if (c != 0) {
        flags |= 0x40;
    }
    if (d != 0) {
        flags |= 0x80;
    }
    if (e == 0) {
        flags |= 8;
    }
    const bool runInSequence = CTaskSequences::ms_iActiveSequence >= 0; // 0x8D2E98

    auto* const seq = new CTaskComplexSequence{}; // 0x632BD0
    seq->AddTask(new CTaskComplexGoToPointAndStandStill{ PEDMOVE_WALK, pos, 0.5f, 2.0f, false, false }); // 0x668120, 0x632D10; (0x86FC84, 0x86FC88)
    seq->AddTask(new CTaskSimpleSlideToCoord{ pos, heading, speed, animName, animGroup, flags, blendDelta, runInSequence, (uint32)(time > 0 ? time : -1) }); // 0x66C450
    S.GivePedScriptedTask(pedHandle, seq, command);
}

//! 2054 GET_TOTAL_NUMBER_OF_PEDS_KILLED_BY_PLAYER (case @0x4735FD): player => count
int32 GetTotalNumberOfPedsKilledByPlayer(int32 playerId) {
    return CDarkel::FindTotalPedsKilledByPlayer(playerId); // 0x43D6E0
}

//! 2058 GET_LEVEL_DESIGN_COORDS_FOR_OBJECT (case @0x473617): object, index => x, y, z  (the object isn't null checked)
//! CEntity::FindTriggerPointCoors [0x533380] (zero vector if there is no such trigger point)
CVector GetLevelDesignCoordsForObject(CObject& obj, int32 index) {
    CVector out;
    obj.FindTriggerPointCoors(&out, index);
    return out;
}

//! 2062 GET_CHAR_HIGHEST_PRIORITY_EVENT (case @0x473679): ped => event id  (the ped isn't null checked)
//! zero extended byte at CPedIntelligence+0xD1 (m_nEventId)
int32 GetCharHighestPriorityEvent(CPed& ped) {
    return (int32)ped.GetIntelligence()->m_nEventId;
}

//! 2064 GET_PARKING_NODE_IN_AREA (case @0x4736A6): x1, y1, z1, x2, y2, z2 => x, y, z
//! Each axis pair is sorted (swap only if a > b); CPathFind::FindParkingNodeInArea(minX, maxX, minY, maxY, minZ, maxZ) [0x4513F0] on ThePaths (0x96F050)
CVector GetParkingNodeInArea(float x1, float y1, float z1, float x2, float y2, float z2) {
    SortPair(x1, x2);
    SortPair(y1, y2);
    SortPair(z1, z2);
    return ThePaths.FindParkingNodeInArea(x1, x2, y1, y2, z1, z2);
}

//! 2066 TASK_PLAY_ANIM_NON_INTERRUPTABLE (case @0x473831): no CollectParameters here, CRunningScript::PlayAnimScriptCommand(command) [0x470150] reads everything
void TaskPlayAnimNonInterruptable(eScriptCommands command, CRunningScript& S) {
    S.PlayAnimScriptCommand(command);
}

//! 2068 ADD_STUNT_JUMP (case @0x473840): 16 params: start centre (xyz), start size (xyz), end centre, end size, camera (xyz), reward
//! The boxes are min = centre - size, max = centre + size (each component computed on the x87 stack, stored as float);
//! CBoundingBox::SetMinMax [0x470100], CStuntJumpManager::AddOne(start, end, camera, reward) [0x49CB40]
void AddStuntJump(CVector startCentre, CVector startSize, CVector endCentre, CVector endSize, CVector camera, int32 reward) {
    const auto MakeBox = [](const CVector& c, const CVector& s) {
        const CVector min{
            (float)((double)c.x - (double)s.x),
            (float)((double)c.y - (double)s.y),
            (float)((double)c.z - (double)s.z)
        };
        const CVector max{
            (float)((double)c.x + (double)s.x),
            (float)((double)c.y + (double)s.y),
            (float)((double)c.z + (double)s.z)
        };
        CBoundingBox box;
        box.SetMinMax(min, max); // 0x470100
        return box;
    };
    const CBoundingBox start = MakeBox(startCentre, startSize);
    const CBoundingBox end   = MakeBox(endCentre, endSize);
    CStuntJumpManager::AddOne(start, end, camera, reward);
}

//! 2069 SET_OBJECT_COORDINATES_AND_VELOCITY (case @0x473A01): object, x, y, z  (the object isn't null checked)
//! z <= -100 => ground Z (CWorld::FindGroundZForCoord 0x569660). Move speed = (newPos - curPos) / timeStep (0x40FE60, 0x4119D0; timeStep = 0xB7CB5C),
//! every component clamped to [-1, 1] (strict compares: NaN is kept); MarkSurroundingEntitiesForCollisionWithTrain(pos, 25.0f, obj, false) [0x6F6640];
//! obj->Teleport(pos, false) (vtable +0x38); CTheScripts::ClearSpaceForMissionEntity(pos, obj) [0x486B00]
void SetObjectCoordinatesAndVelocity(CObject& obj, float x, float y, float z) {
    z = GroundZIfAuto(x, y, z);
    const CVector pos{ x, y, z };

    auto& speed = obj.m_vecMoveSpeed;
    speed       = DivideOriginal(pos - obj.GetPosition(), CTimer::ms_fTimeStep);
    for (float* v : { &speed.x, &speed.y, &speed.z }) {
        if (*v < -1.0f) { // 0x858C1C
            *v = -1.0f;
        }
        if (*v > 1.0f) { // 0x858624
            *v = 1.0f;
        }
    }

    MarkSurroundingEntitiesForCollisionWithTrain(pos, 25.0f, &obj, false); // 0x41C80000
    obj.Teleport(pos, false);
    CTheScripts::ClearSpaceForMissionEntity(pos, &obj);
}

//! 2070 SET_CHAR_KINDA_STAY_IN_SAME_PLACE (case @0x473BF5): ped, on  (the ped isn't null checked)
//! 0x470070: sets bit 22 of the flags dword at +0x46C (bKindaStayInSamePlace) to `on != 0`
void SetCharKindaStayInSamePlace(CPed& ped, int32 on) {
    ped.bKindaStayInSamePlace = on != 0;
}

//! 2071 TASK_FOLLOW_PATROL_ROUTE (case @0x473C38): ped, moveState, mode
//! CTaskComplexFollowPatrolRoute((int16)moveState, route 0xC18DB8, (int16)mode, 0.5 [0x86FD08], 5.0 [0x86FD0C]) [0x674930]
void TaskFollowPatrolRoute(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 moveState, int32 mode) {
    S.GivePedScriptedTask(
        pedHandle,
        new CTaskComplexFollowPatrolRoute{ (int16)moveState, &ScriptPatrolRoute(), (CTaskComplexFollowPatrolRoute::eMode)(int16)mode, 0.5f, 5.0f },
        command
    );
}

//! 2072 IS_CHAR_IN_AIR (case @0x473CB3): ped => compare flag  (the ped isn't null checked)
//! bIsInTheAir (+0x46C bit 9) => true; bIsStanding (bit 0) => false; neither bKnockedUpIntoAir (+0x470 bit 4) nor bKnockedOffBike (bit 31) => false;
//! else: simplest active task of the ped exists and is TASK_SIMPLE_FALL (0xCF = 207; NOT TASK_SIMPLE_IN_AIR)
bool IsCharInAir(CPed& ped) {
    if (ped.bIsInTheAir) {
        return true;
    }
    if (ped.bIsStanding) {
        return false;
    }
    if (!ped.bKnockedUpIntoAir && !ped.bKnockedOffBike) {
        return false;
    }
    auto& tm = ped.GetIntelligence()->GetTaskManager();
    if (!tm.GetSimplestActiveTask()) { // 0x6819D0
        return false;
    }
    return tm.GetSimplestActiveTask()->GetTaskType() == TASK_SIMPLE_FALL;
}

//! 2073 GET_CHAR_HEIGHT_ABOVE_GROUND (case @0x473D38): ped => height  (the ped isn't null checked)
//! pos.z - CWorld::FindGroundZFor3DCoord(pos, 0, 0) [0x5696C0], subtracted on the x87 stack and stored as float
float GetCharHeightAboveGround(CPed& ped) {
    const CVector pos = ped.GetPosition();
    const float   groundZ = CWorld::FindGroundZFor3DCoord(pos, nullptr, nullptr);
    return (float)((double)pos.z - (double)groundZ);
}

//! 2074 SET_CHAR_WEAPON_SKILL (case @0x473DA3): ped, skill  (the ped isn't null checked) -- only the low byte is stored at +0x72C
void SetCharWeaponSkill(CPed& ped, int32 skill) {
    ped.m_nWeaponSkill = (eWeaponSkill)(uint8)skill;
}

//! 2077 SET_CAR_ENGINE_BROKEN (case @0x473E51): car, broken  (the car isn't null checked)
//! broken != 0 => bEngineBroken (+0x42D bit 1) = 1 and bEngineOn (+0x428 bit 4) = 0, else only bEngineBroken = 0
void SetCarEngineBroken(CVehicle& veh, int32 broken) {
    if (broken != 0) {
        veh.vehicleFlags.bEngineBroken = true;
        veh.vehicleFlags.bEngineOn     = false;
    } else {
        veh.vehicleFlags.bEngineBroken = false;
    }
}

//! 2078 IS_THIS_MODEL_A_BOAT (case @0x473EA8): model (raw id, not translated) => compare flag. CModelInfo::IsBoatModel [0x4C5A70]
bool IsThisModelABoat(int32 model) {
    return CModelInfo::IsBoatModel(model);
}

//! 2079 IS_THIS_MODEL_A_PLANE (case @0x473EE1): model (raw id) => compare flag. CModelInfo::IsPlaneModel [0x4C5B30]
bool IsThisModelAPlane(int32 model) {
    return CModelInfo::IsPlaneModel(model);
}

//! 2080 IS_THIS_MODEL_A_HELI (case @0x473F19): model (raw id) => compare flag. CModelInfo::IsHeliModel [0x4C5B00]
bool IsThisModelAHeli(int32 model) {
    return CModelInfo::IsHeliModel(model);
}

//! 2083 TASK_GREET_PARTNER (case @0x473F8A): ped A, ped B, distance multiplier (float), handshake type (int)
//! A gets CTaskComplexPartnerGreet("COMMAND_TASK_GREET_PARTNER" [0x859E8C], B, leader = true, mult, type, (0,0,0)) [0x684210], then B gets the same with A and leader = false.
//! (both peds are resolved with GetAtRef up front, not null checked; the tasks are given in this order: A, B)
void TaskGreetPartner(eScriptCommands command, CRunningScript& S, ScriptEntity<CPed> pedA, ScriptEntity<CPed> pedB, float distanceMultiplier, int32 handShakeType) {
    S.GivePedScriptedTask(
        pedA.h,
        new CTaskComplexPartnerGreet{ "COMMAND_TASK_GREET_PARTNER", pedB.e, true, distanceMultiplier, handShakeType, CVector{ 0.0f, 0.0f, 0.0f } },
        command
    );
    S.GivePedScriptedTask(
        pedB.h,
        new CTaskComplexPartnerGreet{ "COMMAND_TASK_GREET_PARTNER", pedA.e, false, distanceMultiplier, handShakeType, CVector{ 0.0f, 0.0f, 0.0f } },
        command
    );
}

//! 2085 SET_HELI_BLADES_FULL_SPEED (case @0x4740DC): vehicle  (not null checked)
//! subtype (+0x594) 3 [heli] => CAutomobile::m_fHeliRotorSpeed (+0x84C) = 0x3E6147AE (0.22f); subtype 4 [plane] => CPlane::m_fPropSpeed (+0x9C4) = 0x3E3851EC (0.18f)
void SetHeliBladesFullSpeed(CVehicle& veh) {
    if (veh.m_nVehicleSubType == VEHICLE_TYPE_HELI) {
        static_cast<CAutomobile&>(veh).m_fHeliRotorSpeed = std::bit_cast<float>(0x3E6147AEu);
    } else if (veh.m_nVehicleSubType == VEHICLE_TYPE_PLANE) {
        static_cast<CPlane&>(veh).m_fPropSpeed = std::bit_cast<float>(0x3E3851ECu);
    }
}

//! 2086 DISPLAY_HUD (case @0x474128): on  => CTheScripts::bDisplayHud (0xA444A0) = on != 0
void DisplayHud(int32 on) {
    CTheScripts::bDisplayHud = on != 0;
}
}; // namespace

void notsa::script::commands::ported::g20_21::RegisterG20b() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g20b");

    REGISTER_COMMAND_HANDLER(COMMAND_DOES_GROUP_EXIST, DoesGroupExist);
    REGISTER_COMMAND_HANDLER(COMMAND_GIVE_MELEE_ATTACK_TO_CHAR, GiveMeleeAttackToChar);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_HYDRAULICS, SetCarHydraulics);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_2PLAYER_GAME_GOING_ON, Is2PlayerGameGoingOn);
    REGISTER_COMMAND_HANDLER(COMMAND_DOES_CAR_HAVE_HYDRAULICS, DoesCarHaveHydraulics);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_CHAR_SLIDE_TO_COORD_AND_PLAY_ANIM, TaskCharSlideToCoordAndPlayAnim);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_TOTAL_NUMBER_OF_PEDS_KILLED_BY_PLAYER, GetTotalNumberOfPedsKilledByPlayer);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_LEVEL_DESIGN_COORDS_FOR_OBJECT, GetLevelDesignCoordsForObject);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CHAR_HIGHEST_PRIORITY_EVENT, GetCharHighestPriorityEvent);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_PARKING_NODE_IN_AREA, GetParkingNodeInArea);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_PLAY_ANIM_NON_INTERRUPTABLE, TaskPlayAnimNonInterruptable);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_STUNT_JUMP, AddStuntJump);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_COORDINATES_AND_VELOCITY, SetObjectCoordinatesAndVelocity);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_KINDA_STAY_IN_SAME_PLACE, SetCharKindaStayInSamePlace);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_FOLLOW_PATROL_ROUTE, TaskFollowPatrolRoute);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CHAR_IN_AIR, IsCharInAir);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CHAR_HEIGHT_ABOVE_GROUND, GetCharHeightAboveGround);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_WEAPON_SKILL, SetCharWeaponSkill);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_ENGINE_BROKEN, SetCarEngineBroken);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_THIS_MODEL_A_BOAT, IsThisModelABoat);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_THIS_MODEL_A_PLANE, IsThisModelAPlane);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_THIS_MODEL_A_HELI, IsThisModelAHeli);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_GREET_PARTNER, TaskGreetPartner);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_HELI_BLADES_FULL_SPEED, SetHeliBladesFullSpeed);
    REGISTER_COMMAND_HANDLER(COMMAND_DISPLAY_HUD, DisplayHud);
}
