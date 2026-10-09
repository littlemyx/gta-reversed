#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group09_12.hpp"

#include "World.h"
#include "Game.h"
#include "Radar.h"
#include "Coronas.h"
#include "Collision/ColStore.h"
#include "RoadBlocks.h"
#include "Streaming.h"
#include "Timecycle.h"
#include "UserDisplay.h"
#include "Population.h"
#include "Ropes.h"
#include "Pathfind.h"
#include "MissionCleanup.h"
#include "Animation/AnimManager.h"
#include "Animation/AnimBlock.h"
#include "Models/VehicleModelInfo.h"
#include "Tasks/PedScriptedTaskRecord.h"
#include "Tasks/TaskTypes/TaskSimpleDuckToggle.h"
#include "Tasks/TaskTypes/TaskSimpleStandStill.h"
#include "Tasks/TaskTypes/TaskComplexUseSwatRope.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Bike.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g09_12;

/*!
* Script commands ported from the exe's group processor for ids 1200..1299 (`CRunningScript::ProcessCommands1200To1299` @0x48B590)
* for the vanilla commands that had no handler of their own (S6-B, g12). Written from the asm of each `case`, see `.notes/S6B_TABLE.md`.
*/

namespace {
//! 1209 GET_CLOSEST_STRAIGHT_ROAD (case @0x48B7B5): x, y, z, minDist, maxDist => 7 values (node A pos, node B pos, distance) + compare flag
//! No ground-Z lookup here. If the pair isn't found (or node B can't be resolved) all 7 values are 0.
OpcodeResult GetClosestStraightRoad(CRunningScript& S, CVector pos, float minDist, float maxDist) {
    CNodeAddress nodeA{}, nodeB{};
    nodeA.m_wAreaId = 0xFFFF; // the exe only presets the two area ids
    nodeB.m_wAreaId = 0xFFFF;
    float dist{};
    ThePaths.FindNodePairClosestToCoors(pos, 0, &nodeA, &nodeB, &dist, minDist, maxDist, true, true, false);

    bool found = false;
    CVector posA{}, posB{};
    if (nodeA.m_wAreaId != 0xFFFF) {
        posA = ThePaths.FindNodeCoorsForScript(nodeA, nullptr);
        posB = ThePaths.FindNodeCoorsForScript(nodeB, &found);
    }
    if (found) {
        StoreArg(&S, MultiRet<float, float, float, float, float, float, float>{ posA.x, posA.y, posA.z, posB.x, posB.y, posB.z, dist });
    } else {
        StoreArg(&S, MultiRet<float, float, float, float, float, float, float>{});
    }
    S.UpdateCompareFlag(found);
    return OR_CONTINUE;
}

//! 1210 SET_CAR_FORWARD_SPEED (case @0x48B900): car, speed  -- move speed = forward vector * (speed / 60) (0x859044); heli handling on an automobile also gets its rotor spinning
void SetCarForwardSpeed(CVehicle& veh, float speed) {
    const float scaled = speed * 0.016666668f; // 0x859044
    const auto& fwd = veh.m_matrix->GetForward(); // (the exe doesn't check the matrix)
    veh.m_vecMoveSpeed = CVector{ fwd.x * scaled, fwd.y * scaled, fwd.z * scaled };
    if (veh.m_pHandlingData->m_bIsHeli && veh.m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
        static_cast<CAutomobile&>(veh).m_fHeliRotorSpeed = 0.22f; // 0x3E6147AE (+0x84C)
    }
}

//! 1211 SET_AREA_VISIBLE (case @0x48B98D): area
void SetAreaVisible(int32 area) {
    CGame::currArea = (eAreaCodes)area; // 0xB72914
    CStreaming::RemoveBuildingsNotInArea((eAreaCodes)area);
}

//! 1213 MARK_CAR_AS_CONVOY_CAR (case @0x48B9B0): car, flag  -- +0x42B bit 8
void MarkCarAsConvoyCar(CVehicle& veh, int32 flag) {
    veh.vehicleFlags.bPartOfConvoy = flag != 0;
}

//! 1216 CREATE_SCRIPT_ROADBLOCK (case @0x48B9FB): x1, y1, z1, x2, y2, z2, gangRoadblock
void CreateScriptRoadblock(CVector cornerA, CVector cornerB, int32 isGangRoadBlock) {
    CRoadBlocks::RegisterScriptRoadBlock(cornerA, cornerB, isGangRoadBlock != 0);
}

//! 1217 CLEAR_ALL_SCRIPT_ROADBLOCKS (case @0x48BA8B)
void ClearAllScriptRoadblocks() {
    CRoadBlocks::ClearScriptRoadBlocks();
}

//! 1230 ADD_SHORT_RANGE_SPRITE_BLIP_FOR_COORD (case @0x48BC0A): x, y, z, sprite => blip handle
//! (the exe also peeks the output variable and calls the pure `CRadar::GetActualBlipArrayIndex` on it, the result is unused)
int32 AddShortRangeSpriteBlipForCoord(CRunningScript& S, float x, float y, float z, int32 sprite) {
    z = GroundZIfAuto(x, y, z);
    const auto blip = CRadar::SetShortRangeCoordBlip(BLIP_COORD, { x, y, z }, (eBlipColour)5, (eBlipDisplay)3, S.m_szName); // type 4, colour 5, display 3
    CRadar::SetBlipSprite(blip, (eRadarSprite)sprite);
    return blip;
}

//! 1232 SET_HELI_ORIENTATION (case @0x48BCC6): heli, angle (degrees; `(angle + 90) * pi/180`, wrapped into [0, 2pi])
void SetHeliOrientation(CVehicle& heli, float angle) {
    float a = (angle + 90.0f) * 0.017453292f; // 0x85991C, 0x8595EC
    if (a < 0.0f) { // 0x858B50
        do {
            a += 6.2831855f; // 0x858CBC
        } while (a < 0.0f);
    }
    if (a > 6.2831855f) {
        do {
            a -= 6.2831855f;
        } while (a > 6.2831855f);
    }
    static_cast<CAutomobile&>(heli).SetHeliOrientation(a);
}

//! 1234 PLANE_GOTO_COORDS (case @0x48BDA3): plane, x, y, z, altitudeMin, altitudeMax
void PlaneGotoCoords(CVehicle& plane, float x, float y, float z, float altitudeMin, float altitudeMax) {
    static_cast<CAutomobile&>(plane).TellPlaneToGoToCoors(x, y, z, altitudeMin, altitudeMax);
}

//! 1235 GET_NTH_CLOSEST_CAR_NODE (case @0x48BDED): x, y, z, n => x, y, z + compare flag (0, 0, 0 if not found)
OpcodeResult GetNthClosestCarNode(CRunningScript& S, float x, float y, float z, int32 n) {
    z = GroundZIfAuto(x, y, z);
    const auto node = ThePaths.FindNthNodeClosestToCoors({ x, y, z }, 0, 999999.88f, false, true, n - 1, false, false, nullptr);
    bool found{};
    const CVector pos = ThePaths.FindNodeCoorsForScript(node, &found);
    if (found) {
        StoreArg(&S, MultiRet<float, float, float>{ pos.x, pos.y, pos.z });
    } else {
        StoreArg(&S, MultiRet<float, float, float>{});
    }
    S.UpdateCompareFlag(found);
    return OR_CONTINUE;
}

//! 1237 DRAW_WEAPONSHOP_CORONA (case @0x48BEF2): x, y, z, size, type, flare, r, g, b
void DrawWeaponshopCorona(CRunningScript& S, float x, float y, float z, float size, int32 type, int32 flare, int32 r, int32 g, int32 b) {
    z = GroundZIfAuto(x, y, z);
    const CVector pos{ x, y, z };
    CCoronas::RegisterCorona(
        ScriptThingIdFromIP(S), nullptr,
        (uint8)r, (uint8)g, (uint8)b, 255,
        pos, size, 150.0f, // 0x43160000
        (eCoronaType)(uint8)type, (eCoronaFlareType)(uint8)flare, (eCoronaReflType)1, (eCoronaLOSCheck)0, (eCoronaTrail)0,
        0.0f, false,
        0.2f, false, // 0x3E4CCCCD
        15.0f, false, false // 0x41700000
    );
}

//! 1238 SET_ENABLE_RC_DETONATE_ON_CONTACT (case @0x48BFC3): enable  -- `bDisableRemoteDetonationOnContact = !enable`
void SetEnableRCDetonateOnContact(int32 enable) {
    CVehicle::bDisableRemoteDetonationOnContact = enable == 0;
}

//! 1241 SET_OBJECT_RECORDS_COLLISIONS (case @0x48C07B): object, flag  -- physical flags bit 0x10000000
void SetObjectRecordsCollisions(CObject& obj, int32 flag) {
    obj.physicalFlags.bCanBeCollidedWith = flag != 0;
}

//! 1242 HAS_OBJECT_COLLIDED_WITH_ANYTHING (case @0x48C0C3): object => compare flag  (`CPhysical::m_nNumEntitiesCollided` +0xB9 > 0)
bool HasObjectCollidedWithAnything(CObject& obj) {
    return obj.m_nNumEntitiesCollided > 0;
}

//! 1247 SET_HELI_STABILISER (case @0x48C165): heli, flag  -- +0x42B bit 0x10
void SetHeliStabiliser(CVehicle& veh, int32 flag) {
    veh.vehicleFlags.bHeliMinimumTilt = flag != 0;
}

//! 1248 SET_CAR_STRAIGHT_LINE_DISTANCE (case @0x48C1B0): car, distance (byte)  -- autopilot +0x3DD
void SetCarStraightLineDistance(CVehicle& veh, int32 distance) {
    veh.m_autoPilot.m_nStraightLineDistance = (uint8)distance;
}

//! 1249 POP_CAR_BOOT (case @0x48C1DD): car (any vehicle type, the exe calls CAutomobile::PopBoot directly)
void PopCarBoot(CVehicle& veh) {
    static_cast<CAutomobile&>(veh).PopBoot();
}

//! 1252 REQUEST_COLLISION (case @0x48C244): x, y  -- `CColStore::RequestCollision({x, y, 0}, CGame::currArea)`
void RequestCollision(float x, float y) {
    CColStore::RequestCollision(CVector{ x, y, 0.0f }, (eAreaCodes)CGame::currArea);
}

//! 1253 LOCATE_OBJECT_2D / 1254 LOCATE_OBJECT_3D (case @0x48C28F, shared)
OpcodeResult LocateObject(CRunningScript& S, eScriptCommands command) {
    S.LocateObjectCommand((int32)command);
    return OR_CONTINUE;
}

//! 1255 IS_OBJECT_IN_WATER (case @0x48C29E): object (may be invalid) => compare flag  -- physical flags bit 0x100 (submerged in water)
bool IsObjectInWater(CObject* obj) {
    return obj && obj->physicalFlags.bSubmergedInWater;
}

//! 1257 IS_OBJECT_IN_AREA_2D / 1258 IS_OBJECT_IN_AREA_3D (case @0x48C2E9, shared)
OpcodeResult IsObjectInArea(CRunningScript& S, eScriptCommands command) {
    S.ObjectInAreaCheckCommand((int32)command);
    return OR_CONTINUE;
}

//! 1259 TASK_TOGGLE_DUCK (case @0x48C2F8): ped, mode  -- `GivePedScriptedTask(ped, new CTaskSimpleDuckToggle(mode), command)`
OpcodeResult TaskToggleDuck(CRunningScript& S, eScriptCommands command, int32 pedHandle, int32 mode) {
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleDuckToggle{ (CTaskSimpleDuckToggle::eMode)mode }, (int32)command);
    return OR_CONTINUE;
}

//! 1261 REQUEST_ANIMATION (case @0x48C351): name (16 chars)
OpcodeResult RequestAnimation(CRunningScript& S) {
    char name[24]{};
    S.ReadTextLabelFromScript(name, 16);
    const auto block = CAnimManager::GetAnimationBlockIndex((const char*)name);
    CStreaming::RequestModel(IFPToModelId(block), STREAMING_MISSION_REQUIRED); // 0x63E7 + block, flags 4
    CTheScripts::ScriptResourceManager.AddToResourceManager(block, RESOURCE_TYPE_ANIMATION, &S);
    return OR_CONTINUE;
}

//! 1262 HAS_ANIMATION_LOADED (case @0x48C391): name (16 chars) => compare flag  (the exe doesn't check for an unknown block)
OpcodeResult HasAnimationLoaded(CRunningScript& S) {
    char name[24]{};
    S.ReadTextLabelFromScript(name, 16);
    S.UpdateCompareFlag(CAnimManager::GetAnimationBlock((const char*)name)->IsLoaded == true);
    return OR_CONTINUE;
}

//! 1263 REMOVE_ANIMATION (case @0x48C3D5): name (16 chars)
OpcodeResult RemoveAnimation(CRunningScript& S) {
    char name[24]{};
    S.ReadTextLabelFromScript(name, 16);
    const auto block = CAnimManager::GetAnimationBlockIndex((const char*)name);
    if (CTheScripts::ScriptResourceManager.RemoveFromResourceManager(block, RESOURCE_TYPE_ANIMATION, &S)) {
        CStreaming::SetMissionDoesntRequireAnim(block); // 0x48B570 (adds 0x63E7 and tail-calls SetMissionDoesntRequireModel)
    }
    return OR_CONTINUE;
}

//! 1265 IS_CAR_WAITING_FOR_WORLD_COLLISION (case @0x48C45C): car => compare flag  -- entity flags bit 0x40000
bool IsCarWaitingForWorldCollision(CVehicle& veh) {
    return veh.m_bIsStaticWaitingForCollision;
}

//! 1271 DISPLAY_NTH_ONSCREEN_COUNTER_WITH_STRING (case @0x48C52F): var, type, line (1-based), label
OpcodeResult DisplayNthOnscreenCounterWithString(CRunningScript& S) {
    const uint16 varId = S.GetIndexOfGlobalVariable();
    const int32  type = Read<int32>(&S);
    const int32  line = Read<int32>(&S);
    char key[24]{};
    S.ReadTextLabelFromScript(key, 8);
    (void)TheText.Get(key); // result unused in the exe
    CUserDisplay::OnscnTimer.AddCounter(varId, (eOnscreenCounter)(uint16)type, key, (uint16)(line - 1));
    return OR_CONTINUE;
}

//! 1273 SET_EXTRA_COLOURS (case @0x48C65A): colour (1-based), flag
void SetExtraColours(int32 colour, int32 flag) {
    CTimeCycle::StartExtraColour(colour - 1, flag != 0);
}

//! 1274 CLEAR_EXTRA_COLOURS (case @0x48C69D): flag
void ClearExtraColours(int32 flag) {
    CTimeCycle::StopExtraColour(flag != 0);
}

//! The dword of a float in memory, read as an integer (no x87 load, so signalling NaNs survive)
uint32 RawBits(const float& f) {
    return *reinterpret_cast<const volatile uint32*>(&f);
}

//! 1276 GET_WHEELIE_STATS (case @0x48C6D1): player => 6 values (car 2 wheels time/dist, bike wheelie time/dist, bike stoppie time/dist); they are reset to 0 afterwards
//! (the exe moves raw dwords, so the floats are returned as their bit patterns: a float round trip would quiet signalling NaNs)
MultiRet<uint32, uint32, uint32, uint32, uint32, uint32> GetWheelieStats(int32 playerIdx) {
    auto& pi = CWorld::Players[playerIdx]; // used as is, the index isn't validated
    const MultiRet<uint32, uint32, uint32, uint32, uint32, uint32> res{
        pi.m_nBestCarTwoWheelsTimeMs, RawBits(pi.m_fBestCarTwoWheelsDistM),
        pi.m_nBestBikeWheelieTimeMs, RawBits(pi.m_fBestBikeWheelieDistM),
        pi.m_nBestBikeStoppieTimeMs, RawBits(pi.m_fBestBikeStoppieDistM)
    };
    pi.m_nBestCarTwoWheelsTimeMs = 0;
    pi.m_fBestCarTwoWheelsDistM = 0.0f;
    pi.m_nBestBikeWheelieTimeMs = 0;
    pi.m_fBestBikeWheelieDistM = 0.0f;
    pi.m_nBestBikeStoppieTimeMs = 0;
    pi.m_fBestBikeStoppieDistM = 0.0f;
    return res;
}

//! 1278 BURST_CAR_TYRE (case @0x48C768): car, tyre  -- virtual `CVehicle::BurstTyre(tyre, true)`; for bikes 2 -> 0 and 3 -> 1
void BurstCarTyre(CVehicle& veh, int32 tyre) {
    if (veh.m_nVehicleType == VEHICLE_TYPE_BIKE) {
        if (tyre == 2) {
            tyre = 0;
        } else if (tyre == 3) {
            tyre = 1;
        }
    }
    veh.BurstTyre((uint8)tyre, true);
}

//! 1283 CREATE_SWAT_ROPE (case @0x48C863): pedType, model, x, y, z => ped handle
//! Creates a ped hanging from a swat rope (task `CTaskComplexUseSwatRope`), standing still as the default task; no ground-Z lookup.
CPed* CreateSwatRope(CRunningScript& S, eScriptCommands command, ePedType pedType, eModelID model, CVector pos) {
    const int32 ropeId = CRopes::CreateRopeForSwatPed(pos); // 0x558D10
    CPed* const ped = CPopulation::AddPed(pedType, model, pos, true);
    if (notsa::IsFixBugs() && !ped) { // BUG: the original goes on with a null ped (crash) if it couldn't be created
        return nullptr;
    }

    const auto swatTask = new CTaskComplexUseSwatRope{ (uint32)ropeId };
    ped->GetTaskManager().SetTask(swatTask, TASK_PRIMARY_PRIMARY, false);
    CPedScriptedTaskRecord::ms_scriptedTasks[CPedScriptedTaskRecord::GetVacantSlot()].Set(ped, (int32)command, swatTask);
    ped->SetCharCreatedBy(PED_MISSION);
    ped->bAllowMedicsToReviveMe = false; // bit 31 of the ped flags dword at +0x46C

    ped->GetTaskManager().SetTask(new CTaskSimpleStandStill{ 999999, true, false, 8.0f }, TASK_PRIMARY_DEFAULT, false);
    CTheScripts::ClearSpaceForMissionEntity(pos, ped);
    if (S.m_UsesMissionCleanup) {
        ped->m_bIsStaticWaitingForCollision = true;
    }
    CPopulation::ms_nTotalMissionPeds++;

    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(CPools::GetPedRef(ped), MISSION_CLEANUP_ENTITY_TYPE_PED);
    }
    return ped; // (stored as the pool ref by the parser)
}

//! 1286 SET_CAR_MODEL_COMPONENTS (case @0x48C9F4): model (unused), comp1, comp2  -- `CVehicleModelInfo::ms_compsToUse[0..1]` (bytes at 0x8A6458/9)
void SetCarModelComponents(int32 model, int32 comp1, int32 comp2) {
    CVehicleModelInfo::ms_compsToUse[0] = (int8)comp1;
    CVehicleModelInfo::ms_compsToUse[1] = (int8)comp2;
}

//! 1288 CLOSE_ALL_CAR_DOORS (case @0x48CA1C): car (any type, the exe calls CAutomobile::CloseAllDoors directly)
void CloseAllCarDoors(CVehicle& veh) {
    static_cast<CAutomobile&>(veh).CloseAllDoors();
}

//! 1294 SORT_OUT_OBJECT_COLLISION_WITH_CAR (case @0x48CB16): object, car  -- `object->m_pEntityIgnoredCollision (+0x128) = car` (car may be an invalid handle -> null)
void SortOutObjectCollisionWithCar(CObject& obj, CVehicle* car) {
    obj.m_pEntityIgnoredCollision = car;
}

//! 1295 GET_MAX_WANTED_LEVEL (case @0x48CB51) => max wanted level (0x8CDEE4)
int32 GetMaxWantedLevel() {
    return StaticRef<int32>(0x8CDEE4);
}
}; // namespace

static_assert(offsetof(CPlayerInfo, m_nBestCarTwoWheelsTimeMs) == 0x118 && offsetof(CPlayerInfo, m_fBestBikeStoppieDistM) == 0x12C);
static_assert(offsetof(CPhysical, m_pEntityIgnoredCollision) == 0x128 && offsetof(CPhysical, m_nNumEntitiesCollided) == 0xB9);
static_assert(offsetof(CAutomobile, m_fHeliRotorSpeed) == 0x84C);
static_assert(offsetof(CVehicle, m_matrix) == 0x14 && offsetof(CVehicle, m_pHandlingData) == 0x384);

void notsa::script::commands::ported::g09_12::RegisterG12() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g12");

    REGISTER_COMMAND_HANDLER(COMMAND_GET_CLOSEST_STRAIGHT_ROAD, GetClosestStraightRoad);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_FORWARD_SPEED, SetCarForwardSpeed);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_AREA_VISIBLE, SetAreaVisible);
    REGISTER_COMMAND_HANDLER(COMMAND_MARK_CAR_AS_CONVOY_CAR, MarkCarAsConvoyCar);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_SCRIPT_ROADBLOCK, CreateScriptRoadblock);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_ALL_SCRIPT_ROADBLOCKS, ClearAllScriptRoadblocks);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_SHORT_RANGE_SPRITE_BLIP_FOR_COORD, AddShortRangeSpriteBlipForCoord);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_HELI_ORIENTATION, SetHeliOrientation);
    REGISTER_COMMAND_HANDLER(COMMAND_PLANE_GOTO_COORDS, PlaneGotoCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NTH_CLOSEST_CAR_NODE, GetNthClosestCarNode);
    REGISTER_COMMAND_HANDLER(COMMAND_DRAW_WEAPONSHOP_CORONA, DrawWeaponshopCorona);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ENABLE_RC_DETONATE_ON_CONTACT, SetEnableRCDetonateOnContact);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_RECORDS_COLLISIONS, SetObjectRecordsCollisions);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_OBJECT_COLLIDED_WITH_ANYTHING, HasObjectCollidedWithAnything);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_HELI_STABILISER, SetHeliStabiliser);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_STRAIGHT_LINE_DISTANCE, SetCarStraightLineDistance);
    REGISTER_COMMAND_HANDLER(COMMAND_POP_CAR_BOOT, PopCarBoot);
    REGISTER_COMMAND_HANDLER(COMMAND_REQUEST_COLLISION, RequestCollision);
    REGISTER_COMMAND_HANDLER(COMMAND_LOCATE_OBJECT_2D, LocateObject);
    REGISTER_COMMAND_HANDLER(COMMAND_LOCATE_OBJECT_3D, LocateObject);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_OBJECT_IN_WATER, IsObjectInWater);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_OBJECT_IN_AREA_2D, IsObjectInArea);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_OBJECT_IN_AREA_3D, IsObjectInArea);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_TOGGLE_DUCK, TaskToggleDuck);
    REGISTER_COMMAND_HANDLER(COMMAND_REQUEST_ANIMATION, RequestAnimation);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_ANIMATION_LOADED, HasAnimationLoaded);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_ANIMATION, RemoveAnimation);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_WAITING_FOR_WORLD_COLLISION, IsCarWaitingForWorldCollision);
    REGISTER_COMMAND_HANDLER(COMMAND_DISPLAY_NTH_ONSCREEN_COUNTER_WITH_STRING, DisplayNthOnscreenCounterWithString);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_EXTRA_COLOURS, SetExtraColours);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_EXTRA_COLOURS, ClearExtraColours);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_WHEELIE_STATS, GetWheelieStats);
    REGISTER_COMMAND_HANDLER(COMMAND_BURST_CAR_TYRE, BurstCarTyre);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_SWAT_ROPE, CreateSwatRope);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_MODEL_COMPONENTS, SetCarModelComponents);
    REGISTER_COMMAND_HANDLER(COMMAND_CLOSE_ALL_CAR_DOORS, CloseAllCarDoors);
    REGISTER_COMMAND_HANDLER(COMMAND_SORT_OUT_OBJECT_COLLISION_WITH_CAR, SortOutObjectCollisionWithCar);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_MAX_WANTED_LEVEL, GetMaxWantedLevel);
}
