#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "World.h"
#include "Hud.h"
#include "HudColours.h"
#include "Darkel.h"
#include "PostEffects.h"
#include "MenuSystem.h"
#include "Streaming.h"
#include "StreamedScripts.h"
#include "ScriptsForBrains.h"
#include "Weather.h"
#include "WaterLevel.h"
#include "Garage.h"
#include "Garages.h"
#include "PedGroups.h"
#include "PedAttractorManager.h"
#include "Explosion.h"
#include "Pickups.h"
#include "Stats.h"
#include "GameLogic.h"
#include "EntryExit.h"
#include "ModelIndices.h"
#include "Door.h"
#include "Fx/Fx.h"
#include "Fx/FxPrtMult.h"
#include "Audio/AudioEngine.h"
#include "Audio/AudioZones.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Plane.h"
#include "Entity/Object/Object.h"
#include "Plugins/TwoDEffectPlugin/2dEffect.h"

namespace notsa::script::commands::ported::g23 { void RegisterHandlers(); }

using namespace notsa::script;

/*!
* Script commands ported from the exe's per-100 group processor CRunningScript::ProcessCommands2300To2399
* (@0x4762D0, switch base 2300 (`lea eax,[edi-0x8FC]; cmp eax,0x63`), jump table 0x477BFC...) for the vanilla commands that had no handler of
* their own: 58 commands of ids 2300..2399 (S6-H, group g23). See `.notes/S6H_TABLE.md`.
*
* Every handler below was written from the asm of the `case` in the group processor, NOT from the Ghidra decompilation.
* Conventions (see Group19.cpp):
*  - `CPed&` / `CVehicle&` / `CObject&` parser arguments are exactly `GetAtRef(handle)` (the exe has no null check there);
*    pointers are used where the exe tests the result;
*  - `_ftol2` is not needed in this group; flags are `!= 0` of the raw dword;
*  - handlers that read a text label BEFORE the parameters, or that touch stale `ScriptParams`, are written as raw `OpcodeResult(CRunningScript&)`.
*/

namespace {
static_assert(offsetof(CObject, m_nObjectFlags) == 0x140);
static_assert(offsetof(CObject, m_nLastWeaponDamage) == 0x148);
static_assert(offsetof(CPhysical, m_nPhysicalFlags) == 0x40);
static_assert(offsetof(CPhysical, m_vecTurnSpeed) == 0x50);
static_assert(offsetof(CPhysical, m_fMass) == 0x8C && offsetof(CPhysical, m_fTurnMass) == 0x90);
static_assert(offsetof(CPed, m_pEnex) == 0x78C);
static_assert(offsetof(CPlane, m_fLandingGearStatus) == 0x9CC);
static_assert(offsetof(CAutomobile, m_damageManager) == 0x5A0 && offsetof(CAutomobile, m_doors) == 0x5B8 && sizeof(CDoor) == 0x18);
static_assert(offsetof(CAutomobile, m_swingingChassis) == 0x70C);
static_assert(offsetof(CDoor, m_doorState) == 0xB && offsetof(CDoor, m_angle) == 0xC);
static_assert(offsetof(CGarage, m_nFlags) == 0x4E);
static_assert(offsetof(CPedGroup, m_bMembersEnterLeadersVehicle) == 4 && offsetof(CPedGroup, m_groupMembership) == 8 && sizeof(CPedGroup) == 0x2D4);
static_assert(offsetof(tScriptSearchlight, m_bClipIfColliding) == 1 && sizeof(tScriptSearchlight) == 0x7C);
static_assert(sizeof(CStreamedScripts::CStreamedScriptInfo) == 0x20 && offsetof(CStreamedScripts::CStreamedScriptInfo, m_NumberOfUsers) == 4);
static_assert(offsetof(CEntryExit, m_recEntrance) == 8 && offsetof(CEntryExit, m_fEntranceZ) == 0x18 && offsetof(CEntryExit, m_vecExitPos) == 0x20);
static_assert(offsetof(CEntryExit, m_fExitAngle) == 0x2C && offsetof(CEntryExit, m_pLink) == 0x38);
static_assert(SCRIPT_THING_PED_GROUP == 8 && SCRIPT_THING_SEARCH_LIGHT == 2);
static_assert(STAT_TOTAL_SNAPSHOTS == 0xE8 && STAT_TOTAL_HORSESHOES == 0xF2 && STAT_TOTAL_OYSTERS == 0xF4);
static_assert(PICKUP_SNAPSHOT == 0x14);
static_assert(RESOURCE_ID_SCM == 0x6676);

//! 0x8595EC (degrees -> radians as a float)
constexpr float DEG_TO_RAD = std::bit_cast<float>(0x3C8EFA35u);

//! A float as the raw dword the exe copies around (`mov eax, [mem]` / `mov [mem], eax`)
int32 RawF(const float& f) { // (by reference + memcpy: a float passed by value would travel through the x87 stack, which quiets a signalling NaN)
    int32 v;
    std::memcpy(&v, &f, sizeof(v));
    return v;
}

//! `fld dword; fstp dword` quiets a signalling NaN; used where the exe moves a float through the x87 stack instead of copying the dword
int32 RawFViaX87(float f) {
    uint32 u = std::bit_cast<uint32>(f);
    if ((u & 0x7F800000u) == 0x7F800000u && (u & 0x007FFFFFu) != 0) {
        u |= 0x00400000u;
    }
    return (int32)u;
}

//! Inlined `CVehicle::SetEngineOn(bool)` (0x41BDD0): `test byte [veh + 0x42D], 2` (bEngineBroken) => engine off, else bit 4 of 0x428 = (arg & 1)
void VehSetEngineOn(CVehicle& veh, bool on) {
    if (veh.vehicleFlags.bEngineBroken) {
        veh.vehicleFlags.bEngineOn = false;
    } else {
        veh.vehicleFlags.bEngineOn = on;
    }
}

//! The decode of a script model handle used by ALLOCATE_STREAMED_SCRIPT_TO_OBJECT: `test eax, eax; jge keep; else UsedObjectArray[-handle].nModelIndex` (0xA44B88 - handle * 0x1C)
int32 ModelFromHandleSigned(int32 handle) {
    if (handle >= 0) {
        return handle;
    }
    return CTheScripts::UsedObjectArray[-handle].nModelIndex;
}

//! 0x6002F0 (unnamed in the symbol table; also used by TASK_USE_CLOSEST_MAP_ATTRACTOR in g22): finds the closest 2D effect of type `EFFECT_ATTRACTOR` whose
//! attractor type is `attractorType`, around `pos`. Outputs (both cleared first) the effect and the entity owning it; returns whether one was found.
//! Candidates: buildings + objects (FindObjectsInRange, 3D, max 0x400); `model == -1` accepts every entity, else the model index (int16 @+0x22) must match;
//! `exclude` is skipped; `name` (if not null) must equal the effect's script name (_stricmp); `checkFreeSlot`: the attractor manager must have an empty slot.
//! The ranking is the SQUARED distance (starts at 1e8 = 0x4CBEBC20), accumulated as ((dz*dz + dy*dy) + dx*dx) in extended precision and compared
//! unrounded, then stored as float.
bool FindClosestScriptedAttractor(
    const CVector& pos,
    float          radius,
    int32          model,
    int32          attractorType,
    const char*    name,
    bool           checkFreeSlot,
    C2dEffect*     exclude,
    C2dEffect*&    outEffect,
    CEntity*&      outEntity
) {
    outEffect = nullptr;
    outEntity = nullptr;

    CEntity* entities[0x400];
    int16    count = -1;
    float    best  = std::bit_cast<float>(0x4CBEBC20u);
    bool     found = false;
    CWorld::FindObjectsInRange(pos, radius, false, &count, 0x400, entities, true, false, false, true, false); // 0x564A20

    for (int32 i = 0; i < (int32)count; i++) {
        CEntity* const entity    = entities[i];
        const int32    entityMdl = (int32)(int16)entity->m_nModelIndex;
        if (model != -1 && model != entityMdl) {
            continue;
        }
        CBaseModelInfo* const mi = CModelInfo::ms_modelInfoPtrs[entityMdl];
        for (int32 j = 0; j < (int32)mi->m_n2dfxCount; j++) { // the count byte is re-read on every iteration
            C2dEffect* const fx = mi->Get2dEffect(j); // 0x4C4C70
            if (exclude && exclude == fx) {
                continue;
            }
            if (fx->m_Type != EFFECT_ATTRACTOR) {
                continue;
            }
            if ((int32)(uint8)fx->pedAttractor.m_nAttractorType != attractorType) { // movzx
                continue;
            }
            if (name && _stricmp(name, fx->pedAttractor.m_szScriptName) != 0) { // 0x8229B6
                continue;
            }
            if (checkFreeSlot && !GetPedAttractorManager()->HasEmptySlot(reinterpret_cast<C2dEffectPedAttractor*>(fx), entity)) { // 0x5EBB00
                continue;
            }
            const CVector w  = entity->TransformFromObjectSpace(fx->m_Pos); // 0x5334F0
            const double  dx = (double)pos.x - (double)w.x;
            const double  dy = (double)pos.y - (double)w.y;
            const double  dz = (double)pos.z - (double)w.z;
            const double  sq = (dz * dz + dy * dy) + dx * dx;
            if (sq < (double)best) { // fcom + `test ah, 5; jp skip`: only when strictly less (ordered)
                best      = (float)sq;
                outEffect = fx;
                outEntity = entity;
                found     = true;
            }
        }
    }
    return found;
}

//! Shared tail of CREATE_SNAPSHOT_PICKUP / CREATE_HORSESHOE_PICKUP / CREATE_OYSTER_PICKUP (case @0x4778AA, selected by `cmd - 0x958`):
//! `CStats::IncrementStat(stat, 1.0f)`, then z <= -100.0f (0x859014, ordered: NaN keeps z) => FindGroundZForCoord(x, y) + 0.5f (0x858B8C),
//! then `CPickups::GenerateNewOne(pos, model (word), type, 0, 0, false, nullptr)` => the pickup handle (the raw int in eax).
int32 CreateCollectiblePickup(float x, float y, float z, eStats stat, uint16 model, ePickupType type) {
    CStats::IncrementStat(stat, 1.0f); // 0x55C180
    if (z <= -100.0f) {
        z = (float)((double)CWorld::FindGroundZForCoord(x, y) + (double)0.5f); // 0x569660
    }
    return CPickups::GenerateNewOne({ x, y, z }, (uint32)model, type, 0, 0, false, nullptr).num; // 0x456F20
}

// ============================================================================ g23 (ProcessCommands2300To2399 @0x4762D0)

//! 2301 SET_HEATHAZE_EFFECT (case @0x4762F8): flag => `CPostEffects::ScriptHeatHazeFXSwitch(flag != 0)` (0x701160)
void SetHeathazeEffect(int32 flag) {
    CPostEffects::ScriptHeatHazeFXSwitch(flag != 0);
}

//! 2302 IS_HELP_MESSAGE_BEING_DISPLAYED (case @0x476323): => compare flag `CHud::HelpMessageDisplayed()` (0x588B50)
bool IsHelpMessageBeingDisplayed() {
    return CHud::HelpMessageDisplayed();
}

//! 2303 HAS_OBJECT_BEEN_DAMAGED_BY_WEAPON (case @0x476346): object, weapon type => compare flag
//! No object => false. Weapon type 0x38 / 0x39 (ANYMELEE / ANYWEAPON) => `CDarkel::CheckDamagedWeaponType((int8)obj->+0x148, type)` (0x43D9E0), else
//! `(int8)lastWeaponDamage == type` (sign-extended byte compared with the raw dword).
bool HasObjectBeenDamagedByWeapon(CObject* obj, int32 weaponType) {
    if (!obj) {
        return false;
    }
    const int32 last = (int32)(int8)obj->m_nLastWeaponDamage; // movsx
    if (weaponType == 0x38 || weaponType == 0x39) {
        return CDarkel::CheckDamagedWeaponType((eWeaponType)last, (eWeaponType)weaponType);
    }
    return last == weaponType;
}

//! 2304 CLEAR_OBJECT_LAST_WEAPON_DAMAGE (case @0x4763BB): object => byte +0x148 = 0xFF (nothing for a missing object)
void ClearObjectLastWeaponDamage(CObject* obj) {
    if (obj) {
        obj->m_nLastWeaponDamage = 0xFF;
    }
}

//! 2308 GET_HUD_COLOUR (case @0x47644C): colour index (byte) => r, g, b, a (4 ints, each a byte of `CHudColours::GetRGB` 0x58FEA0)
MultiRet<int32, int32, int32, int32> GetHudColour(int32 index) {
    const CRGBA c = HudColour.GetRGB((eHudColours)(uint8)index);
    return { (int32)c.r, (int32)c.g, (int32)c.b, (int32)c.a };
}

//! 2309 LOCK_DOOR (case @0x4764AF): object, flag (no null check)
//! flag != 0 => `CObject::LockDoor()` (0x59F5C0). Otherwise: physical flags &= ~0xC (bDisableCollisionForce, bCollidable), turn speed (+0x50) = 0,
//! then virtual slot 4 (`SetIsStatic(true)`).
void LockDoor(CObject& obj, int32 lock) {
    if (lock != 0) {
        obj.LockDoor();
        return;
    }
    obj.m_nPhysicalFlags &= 0xFFFFFFF3u;
    obj.m_vecTurnSpeed = CVector{ 0.0f, 0.0f, 0.0f };
    obj.SetIsStatic(true);
}

//! 2310 SET_OBJECT_MASS (case @0x47652F): object, mass (raw dword => +0x8C). No null check.
void SetObjectMass(CObject& obj, float mass) {
    obj.m_fMass = mass;
}

//! 2311 GET_OBJECT_MASS (case @0x476563): object => raw dword of +0x8C
int32 GetObjectMass(CObject& obj) {
    return RawF(obj.m_fMass);
}

//! 2312 SET_OBJECT_TURN_MASS (case @0x4765A0): object, mass (raw dword => +0x90)
void SetObjectTurnMass(CObject& obj, float mass) {
    obj.m_fTurnMass = mass;
}

//! 2313 GET_OBJECT_TURN_MASS (case @0x4765D5): object => raw dword of +0x90
int32 GetObjectTurnMass(CObject& obj) {
    return RawF(obj.m_fTurnMass);
}

//! 2318 SET_ACTIVE_MENU_ITEM (case @0x476659): menu id (byte), item (byte, passed as int8) => `CMenuSystem::SetActiveMenuItem` (0x5820C0)
void SetActiveMenuItem(int32 menu, int32 item) {
    CMenuSystem::SetActiveMenuItem((MenuId)(uint8)menu, (int8)(uint8)item);
}

//! 2319 MARK_STREAMED_SCRIPT_AS_NO_LONGER_NEEDED (case @0x47668A): script index (word)
//! `GetProperIndexFromIndexUsedByScript((int16)idx)` (0x470810), sign-extended; `CStreaming::SetMissionDoesntRequireModel(index + RESOURCE_ID_SCM)` (0x4700E0 -> 0x409C90).
//! (The exe also leaves the sign-extended index in ScriptParams[0]: a dead store.)
void MarkStreamedScriptAsNoLongerNeeded(int32 scmIndex) {
    const int16 proper = CTheScripts::StreamedScripts.GetProperIndexFromIndexUsedByScript((int16)(uint16)scmIndex);
    CStreaming::SetMissionDoesntRequireModel(SCMToModelId((int32)proper));
}

//! 2320 REMOVE_STREAMED_SCRIPT (case @0x4766C5): script index (word) => same index decode, `CStreaming::RemoveModel(index + RESOURCE_ID_SCM)` (0x40C1C0 -> 0x4089A0)
void RemoveStreamedScript(int32 scmIndex) {
    const int16 proper = CTheScripts::StreamedScripts.GetProperIndexFromIndexUsedByScript((int16)(uint16)scmIndex);
    CStreaming::RemoveModel(SCMToModelId((int32)proper));
}

//! 2322 SET_MESSAGE_FORMATTING (case @0x476701): flag, centre (word), width (word)
//! bUseMessageFormatting (0xA44B66) = flag != 0, MessageCentre (0xA44B64) = (uint16)centre, MessageWidth (0xA44B60) = (uint16)width.
void SetMessageFormatting(int32 flag, int32 centre, int32 width) {
    CTheScripts::bUseMessageFormatting = (flag != 0);
    CTheScripts::MessageCentre         = (uint16)centre;
    CTheScripts::MessageWidth          = (uint16)width;
}

//! 2323 START_NEW_STREAMED_SCRIPT (case @0x476742): script index (word), then the new script's parameters (read from the IP afterwards)
//! `StreamedScripts.StartNewStreamedScript((int16)GetProperIndexFromIndexUsedByScript(idx))` (0x470890) => `ReadParametersForNewlyStartedScript(newScript)` (0x464500).
//! A script that isn't loaded gives a null script and the exe crashes in the latter, exactly like here.
void StartNewStreamedScript(CRunningScript& S, int32 scmIndex) {
    const int16 proper = CTheScripts::StreamedScripts.GetProperIndexFromIndexUsedByScript((int16)(uint16)scmIndex);
    CRunningScript* const newScript = CTheScripts::StreamedScripts.StartNewStreamedScript((int32)proper);
    S.ReadParametersForNewlyStartedScript(newScript);
}

//! 2325 SET_WEATHER_TO_APPROPRIATE_TYPE_NOW (case @0x47679F): `CWeather::SetWeatherToAppropriateTypeNow()` (0x72A790)
void SetWeatherToAppropriateTypeNow() {
    CWeather::SetWeatherToAppropriateTypeNow();
}

//! 2326 WINCH_CAN_PICK_OBJECT_UP (case @0x4767B2): object, flag => object flags (+0x140) bit 0x40000 (bCanBeAttachedToMagnet) = flag != 0
void WinchCanPickObjectUp(CObject& obj, int32 flag) {
    if (flag != 0) {
        obj.m_nObjectFlags |= 0x40000u;
    } else {
        obj.m_nObjectFlags &= 0xFFFBFFFFu;
    }
}

//! 2327 SWITCH_AUDIO_ZONE (case @0x476810): text label (8; read BEFORE the parameter), flag => `CAudioZones::SwitchAudioZone(label, flag != 0)` (0x508320)
//! (the exe passes its unterminated 8 byte buffer; here the rest is zero)
OpcodeResult SwitchAudioZone(CRunningScript& S) {
    char name[16]{};
    S.ReadTextLabelFromScript(name, 8);
    S.CollectParameters(1);
    CAudioZones::SwitchAudioZone(name, ScriptParams[0].iParam != 0);
    return OR_CONTINUE;
}

//! 2328 SET_CAR_ENGINE_ON (case @0x47684E): car, flag => `CVehicle::SetEngineOn(flag != 0)` (0x41BDD0): the engine can't be turned on if it's broken
void SetCarEngineOn(CVehicle& veh, int32 flag) {
    VehSetEngineOn(veh, flag != 0);
}

//! 2329 SET_CAR_LIGHTS_ON (case @0x47688A): car, flag => bLightsOn (byte +0x428 bit 6) = flag != 0
void SetCarLightsOn(CVehicle& veh, int32 flag) {
    veh.vehicleFlags.bLightsOn = (flag != 0);
}

//! 2332 GET_USER_OF_CLOSEST_MAP_ATTRACTOR (case @0x4768EC): x, y, z, radius, model (0 => -1, negative => UsedObjectArray), text label (8; read AFTER the parameters) => char handle (-1 = none)
//! `FindClosestScriptedAttractor(pos, radius, model, 5 (script attractor type), label, false, nullptr, &effect, &entity)` (0x6002F0): no effect => -1;
//! else `GetPedAttractorManager()->GetPedUsingEffect(effect, entity)` (0x5EBE50): no ped => -1, else the ped handle (0x4442D0).
int32 GetUserOfClosestMapAttractor(CRunningScript& S, float x, float y, float z, float radius, int32 model) {
    char name[16]{};
    S.ReadTextLabelFromScript(name, 8);

    if (model == 0) {
        model = -1;
    } else if (model < 0) {
        model = ModelFromHandleSigned(model);
    }

    const CVector pos{ x, y, z };
    C2dEffect*    effect = nullptr;
    CEntity*      entity = nullptr;
    FindClosestScriptedAttractor(pos, radius, model, 5, name, false, nullptr, effect, entity); // the return value is not used, `effect` is tested
    if (!effect) {
        return -1;
    }
    CPed* const user = GetPedAttractorManager()->GetPedUsingEffect(reinterpret_cast<C2dEffectPedAttractor*>(effect), entity); // 0x5EBE50
    if (!user) {
        return -1;
    }
    return GetPedPool()->GetRef(user); // 0x4442D0
}

//! 2335 GET_PLANE_UNDERCARRIAGE_POSITION (case @0x476BA1): plane => raw dword of +0x9CC (CPlane::m_fLandingGearStatus). No null check.
int32 GetPlaneUndercarriagePosition(CVehicle& veh) {
    return RawF(static_cast<CPlane&>(veh).m_fLandingGearStatus);
}

//! 2339 SWITCH_AMBIENT_PLANES (case @0x476C9B): flag => `CPlane::SwitchAmbientPlanes(flag != 0)` (0x6CCC50)
void SwitchAmbientPlanes(int32 flag) {
    CPlane::SwitchAmbientPlanes(flag != 0);
}

//! 2340 SET_DARKNESS_EFFECT (case @0x476CC6): flag, alpha => `CPostEffects::ScriptDarknessFilterSwitch(flag != 0, alpha)` (0x701170)
void SetDarknessEffect(int32 flag, int32 alpha) {
    CPostEffects::ScriptDarknessFilterSwitch(flag != 0, alpha);
}

//! 2342 GET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT (case @0x476D10): script index (word) => `m_aScripts[proper].m_NumberOfUsers` (byte, zero-extended)
//! No bounds check on the index (a "not found" index of -1 reads the memory in front of the array, like the exe).
int32 GetNumberOfInstancesOfStreamedScript(int32 scmIndex) {
    const int16 proper = CTheScripts::StreamedScripts.GetProperIndexFromIndexUsedByScript((int16)(uint16)scmIndex);
    return (int32)CTheScripts::StreamedScripts.m_aScripts.data()[proper].m_NumberOfUsers;
}

//! 2344 ALLOCATE_STREAMED_SCRIPT_TO_RANDOM_PED (case @0x476D3F): script index (word), model (word), priority (word)
//! `ScriptsForBrains.AddNewScriptBrain((int16)proper, (int16)model, priority, 0 (random ped), -1, -1.0f)` (0x46A930)
void AllocateStreamedScriptToRandomPed(int32 scmIndex, int32 model, int32 priority) {
    const int16 proper = CTheScripts::StreamedScripts.GetProperIndexFromIndexUsedByScript((int16)(uint16)scmIndex);
    CTheScripts::ScriptsForBrains.AddNewScriptBrain(proper, (int16)(uint16)model, (uint16)priority, 0, -1, -1.0f);
}

//! 2345 ALLOCATE_STREAMED_SCRIPT_TO_OBJECT (case @0x476D99): script index (word), model (signed: negative => UsedObjectArray), priority (word), radius (raw float dword), grouping id (byte)
//! `AddNewScriptBrain((int16)proper, (int16)model, priority, 1 (object), groupingId, radius)`
void AllocateStreamedScriptToObject(int32 scmIndex, int32 model, int32 priority, float radius, int32 groupingId) {
    const int16 proper = CTheScripts::StreamedScripts.GetProperIndexFromIndexUsedByScript((int16)(uint16)scmIndex);
    CTheScripts::ScriptsForBrains.AddNewScriptBrain(proper, (int16)ModelFromHandleSigned(model), (uint16)priority, 1, (int8)(uint8)groupingId, radius);
}

//! 2347 GET_GROUP_MEMBER (case @0x476E09): group handle, member id => char handle (-1 = none)
//! `idx = CTheScripts::GetActualScriptThingIndex(group, 8)` (0x4839A0; no range check), `ms_groups[idx].m_groupMembership.GetMember(id)` (0x5F69B0), pool GetRef (0x4442D0).
CPed* GetGroupMember(int32 groupHandle, int32 memberId) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(groupHandle, SCRIPT_THING_PED_GROUP);
    return CPedGroups::ms_groups.data()[idx].GetMembership().GetMember(memberId);
}

//! 2350 GET_WATER_HEIGHT_AT_COORDS (case @0x476E6E): x, y, with-waves flag => height (-1000.0f = 0xC47A0000 when there is no water)
//! flag != 0 => `CWaterLevel::GetWaterLevel(x, y, 0.0f, &out, true, nullptr)` (0x6EB690), else `GetWaterLevelNoWaves({x, y, 0}, &out, nullptr, nullptr)` (0x6E8580).
float GetWaterHeightAtCoords(float x, float y, int32 withWaves) {
    float out = 0.0f;
    bool  ok;
    if (withWaves != 0) {
        ok = CWaterLevel::GetWaterLevel(x, y, 0.0f, out, 1, nullptr);
    } else {
        ok = CWaterLevel::GetWaterLevelNoWaves(CVector{ x, y, 0.0f }, &out, nullptr, nullptr);
    }
    if (!ok) {
        out = std::bit_cast<float>(0xC47A0000u); // -1000.0f
    }
    return out;
}

//! 2361 ATTACH_CAR_TO_OBJECT (case @0x477175): car, object, offset (3), rotation in degrees (3)
//! Each rotation component = (float)(deg * 0.017453292f (0x8595EC)); `car->AttachEntityToEntity(object, offset, rotation)` (0x54D570). No null checks.
void AttachCarToObject(CVehicle& veh, CObject& obj, float ox, float oy, float oz, float rx, float ry, float rz) {
    const CVector rot{
        (float)((double)rx * (double)DEG_TO_RAD),
        (float)((double)ry * (double)DEG_TO_RAD),
        (float)((double)rz * (double)DEG_TO_RAD)
    };
    veh.AttachEntityToEntity(&obj, CVector{ ox, oy, oz }, rot);
}

//! 2362 SET_GARAGE_RESPRAY_FREE (case @0x477239): text label (8; read BEFORE the parameter), flag
//! `CGarages::GetGarageNumberByName(label)` (0x447680, sign-extended int16): negative => nothing, else the garage flags byte (+0x4E) bit 7 (m_bRespraysAlwaysFree) = flag != 0.
OpcodeResult SetGarageRespray(CRunningScript& S) {
    char name[16]{};
    S.ReadTextLabelFromScript(name, 8);
    S.CollectParameters(1);
    const int32 idx = (int32)CGarages::GetGarageNumberByName(name);
    if (idx < 0) {
        return OR_CONTINUE;
    }
    CGarages::aGarages[idx].m_bRespraysAlwaysFree = (ScriptParams[0].iParam != 0);
    return OR_CONTINUE;
}

//! 2363 SET_CHAR_BULLETPROOF_VEST (case @0x47729D): char, flag => bit 0 of the dword at +0x478 (bHasBulletProofVest) = flag != 0. No null check.
void SetCharBulletproofVest(CPed& ped, int32 flag) {
    ped.bHasBulletProofVest = (flag != 0);
}

//! 2368 SET_GROUP_FOLLOW_STATUS (case @0x477323): group handle, flag => `idx = GetActualScriptThingIndex(group, 8)`; 0 <= idx < 8 => ms_groups[idx] byte +4 (m_bMembersEnterLeadersVehicle) = flag != 0
void SetGroupFollowStatus(int32 groupHandle, int32 flag) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(groupHandle, SCRIPT_THING_PED_GROUP);
    if (idx < 0 || idx >= 8) {
        return;
    }
    CPedGroups::ms_groups[idx].m_bMembersEnterLeadersVehicle = (flag != 0);
}

//! 2369 SET_SEARCHLIGHT_CLIP_IF_COLLIDING (case @0x477372): searchlight handle, flag => `idx = GetActualScriptThingIndex(handle, 2)`; idx >= 0 (no upper bound check) => light[idx].m_bClipIfColliding = flag != 0
void SetSearchlightClipIfColliding(int32 searchlightHandle, int32 flag) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(searchlightHandle, SCRIPT_THING_SEARCH_LIGHT);
    if (idx < 0) {
        return;
    }
    CTheScripts::ScriptSearchLightArray.data()[idx].m_bClipIfColliding = (flag != 0);
}

//! 2374 SET_CHAR_USES_UPPERBODY_DAMAGE_ANIMS_ONLY (case @0x47744C): char, flag => bit 2 of the dword at +0x478 (bUpperBodyDamageAnimsOnly) = flag != 0
void SetCharUsesUpperbodyDamageAnimsOnly(CPed& ped, int32 flag) {
    ped.bUpperBodyDamageAnimsOnly = (flag != 0);
}

//! 2375 SET_CHAR_SAY_CONTEXT (case @0x47749A): char, context (word) => the sign-extended int16 result of `CPed::Say(ctx, 0, 1.0f, false, false, false)` (0x5EFFE0)
int32 SetCharSayContext(CPed& ped, int32 context) {
    return (int32)ped.Say((eGlobalSpeechContext)(uint16)context, 0, 1.0f, false, false, false);
}

//! 2376 ADD_EXPLOSION_VARIABLE_SHAKE (case @0x4774DA): x, y, z, type, camera shake
//! `CExplosion::AddExplosion(nullptr, nullptr, type, pos, 0, 1 (sound), shake, 0 (visible))` (0x736A50)
void AddExplosionVariableShake(float x, float y, float z, int32 type, float shake) {
    CExplosion::AddExplosion(nullptr, nullptr, (eExplosionType)type, CVector{ x, y, z }, 0, 1, shake, 0);
}

//! 2377 ATTACH_MISSION_AUDIO_TO_CHAR (case @0x477545): slot (byte, minus 1), char (may be null) => `AudioEngine.AttachMissionAudioToPed((uint8)(slot - 1), ped)` (0x507310)
void AttachMissionAudioToChar(int32 slot, CPed* ped) {
    AudioEngine.AttachMissionAudioToPed((uint8)(slot - 1), ped);
}

//! 2378 UPDATE_PICKUP_MONEY_PER_DAY (case @0x477584): pickup handle, money (word) => `CPickups::UpdateMoneyPerDay(handle, (uint16)money)` (0x455680)
void UpdatePickupMoneyPerDay(int32 pickup, int32 money) {
    CPickups::UpdateMoneyPerDay(tPickupReference{ pickup }, (uint16)money);
}

//! 2379 GET_NAME_OF_ENTRY_EXIT_CHAR_USED (case @0x4775B3): char => string variable
//! The char's `m_pEnex` (+0x78C): the destination (fetched with `GetPointerToScriptVariable(VAR_GLOBAL)` AFTER the char) gets the NUL terminated name copied
//! byte by byte from the start of the entry/exit (the 8 byte name is not guaranteed to be terminated: the copy runs on, like the exe); no enex => "".
void GetNameOfEntryExitCharUsed(CRunningScript& S, CPed& ped) {
    char* dst = (char*)S.GetPointerToScriptVariable(VAR_GLOBAL);
    if (const char* src = reinterpret_cast<const char*>(ped.m_pEnex)) {
        char c;
        do {
            c      = *src++;
            *dst++ = c;
        } while (c);
    } else {
        *dst = '\0';
    }
}

//! 2380 GET_POSITION_OF_ENTRY_EXIT_CHAR_USED (case @0x47760C): char => x, y, z, angle (radians)
//! enex = ped->m_pEnex. With an enex that has a link (+0x38): the exit position (+0x20, raw dwords) and exit angle (+0x2C) * 0.017453292f. Without a link: the centre of the
//! entrance rectangle ((right + left) * 0.5f, (top + bottom) * 0.5f, +0x18) (0x43E090) and angle 0. Without an enex the exe skips the writes and STILL calls
//! `StoreParameters(4)` => it stores whatever stale values ScriptParams[0..3] hold (ScriptParams[0] = the char handle): kept (hence the raw handler).
OpcodeResult GetPositionOfEntryExitCharUsed(CRunningScript& S) {
    S.CollectParameters(1);
    CPed* const ped = GetPedPool()->GetAtRef(ScriptParams[0].iParam); // no null check
    if (CEntryExit* const enex = ped->m_pEnex) {
        int32 x, y, z;
        float angle = 0.0f;
        if (enex->m_pLink) {
            x     = RawF(enex->m_vecExitPos.x);
            y     = RawF(enex->m_vecExitPos.y);
            z     = RawF(enex->m_vecExitPos.z);
            angle = (float)((double)enex->m_fExitAngle * (double)DEG_TO_RAD);
        } else {
            const auto& r = enex->m_recEntrance; // CRect: left, bottom, right, top (+8, +0xC, +0x10, +0x14)
            x = RawF((float)(((double)r.right + (double)r.left) * (double)0.5f));
            y = RawF((float)(((double)r.top + (double)r.bottom) * (double)0.5f));
            z = RawF(enex->m_fEntranceZ);
        }
        ScriptParams[0].iParam = x;
        ScriptParams[1].iParam = y;
        ScriptParams[2].iParam = z;
        ScriptParams[3].iParam = RawF(angle);
    }
    S.StoreParameters(4);
    return OR_CONTINUE;
}

//! 2381 IS_CHAR_TALKING (case @0x4776C3): char => compare flag `CPed::GetPedTalking()` (0x5EFF50)
bool IsCharTalking(CPed& ped) {
    return ped.GetPedTalking();
}

//! 2384 SET_UP_SKIP (case @0x47776C): x, y, z, angle => `CGameLogic::SetUpSkip(pos, angle, false, nullptr, false)` (0x4423C0)
void SetUpSkip(float x, float y, float z, float angle) {
    CGameLogic::SetUpSkip(CVector{ x, y, z }, angle, false, nullptr, false);
}

//! 2385 CLEAR_SKIP (case @0x4777CE): `CGameLogic::ClearSkip(false)` (0x441560)
void ClearSkip() {
    CGameLogic::ClearSkip(false);
}

//! 2386 PRELOAD_BEAT_TRACK (case @0x4777E6): track id (word, as int16) => `AudioEngine.PreloadBeatTrack` (0x507F40)
void PreloadBeatTrack(int32 trackId) {
    AudioEngine.PreloadBeatTrack((int16)(uint16)trackId);
}

//! 2387 GET_BEAT_TRACK_STATUS (case @0x477811): => int (sign-extended byte of `AudioEngine.GetBeatTrackStatus()` 0x507170)
int32 GetBeatTrackStatus() {
    return (int32)AudioEngine.GetBeatTrackStatus();
}

//! 2388 PLAY_BEAT_TRACK (case @0x477823): `AudioEngine.PlayPreloadedBeatTrack(false)` (0x507180)
void PlayBeatTrack() {
    AudioEngine.PlayPreloadedBeatTrack(false);
}

//! 2389 STOP_BEAT_TRACK (case @0x47783D): `AudioEngine.StopBeatTrack()` (0x5071A0)
void StopBeatTrack() {
    AudioEngine.StopBeatTrack();
}

//! 2390 FIND_MAX_NUMBER_OF_GROUP_MEMBERS (case @0x477855): => `CStats::FindMaxNumberOfGroupMembers()` (0x559A50)
int32 FindMaxNumberOfGroupMembers() {
    return CStats::FindMaxNumberOfGroupMembers();
}

//! 2391 VEHICLE_DOES_PROVIDE_COVER (case @0x47785F): car, flag => bDoesProvideCover (byte +0x42E bit 2) = flag != 0
void VehicleDoesProvideCover(CVehicle& veh, int32 flag) {
    veh.vehicleFlags.bDoesProvideCover = (flag != 0);
}

//! 2392 CREATE_SNAPSHOT_PICKUP (case @0x4778AA, cmd 0x958): x, y, z => pickup handle. Stat 0xE8, model MI_PICKUP_CAMERA (0x8CD5D8, word), pickup type 0x14 (PICKUP_SNAPSHOT)
int32 CreateSnapshotPickup(float x, float y, float z) {
    return CreateCollectiblePickup(x, y, z, STAT_TOTAL_SNAPSHOTS, (uint16)ModelIndices::MI_PICKUP_CAMERA, PICKUP_SNAPSHOT);
}

//! 2393 CREATE_HORSESHOE_PICKUP (case @0x4778AA, cmd 0x959): stat 0xF2, model MI_HORSESHOE (0x8CD754), pickup type 3
int32 CreateHorseshoePickup(float x, float y, float z) {
    return CreateCollectiblePickup(x, y, z, STAT_TOTAL_HORSESHOES, (uint16)ModelIndices::MI_HORSESHOE, (ePickupType)3);
}

//! 2394 CREATE_OYSTER_PICKUP (case @0x4778AA, cmd 0x95A): stat 0xF4, model MI_OYSTER (0x8CD750), pickup type 3
int32 CreateOysterPickup(float x, float y, float z) {
    return CreateCollectiblePickup(x, y, z, STAT_TOTAL_OYSTERS, (uint16)ModelIndices::MI_OYSTER, (ePickupType)3);
}

//! 2395 HAS_OBJECT_BEEN_UPROOTED (case @0x4779A7): object => compare flag `!obj->GetIsStatic()` (0x4633E0; `neg al; sbb al, al; inc al`). No null check.
bool HasObjectBeenUprooted(CObject& obj) {
    return !obj.GetIsStatic();
}

//! 2396 ADD_SMOKE_PARTICLE (case @0x4779EC): pos (3), velocity (3), r, g, b, a, size, life (12 floats)
//! `FxPrtMult_c mults(r, g, b, a, size, 1.0f, life)` (0x4AB290); `g_fx.m_SmokeHuge (0xA9AE20)->AddParticle(pos, vel, 0.0f, mults, -1.0f, 1.2f (0x3F99999A), 0.6f (0x3F19999A), false)` (0x4AA440). No null check on the system.
void AddSmokeParticle(float px, float py, float pz, float vx, float vy, float vz, float r, float g, float b, float a, float size, float life) {
    static_assert(offsetof(Fx_c, m_SmokeHuge) == 0xA9AE20 - 0xA9AE00);
    const CVector     pos{ px, py, pz };
    const CVector     vel{ vx, vy, vz };
    const FxPrtMult_c mults{ r, g, b, a, size, 1.0f, life };
    g_fx.m_SmokeHuge->AddParticle(pos, vel, 0.0f, mults, -1.0f, std::bit_cast<float>(0x3F99999Au), std::bit_cast<float>(0x3F19999Au), false);
}

//! 2397 IS_CHAR_STUCK_UNDER_CAR (case @0x477AA6): char => compare flag bit 3 of the dword at +0x478 (bStuckUnderCar). No null check.
bool IsCharStuckUnderCar(CPed& ped) {
    return ped.bStuckUnderCar;
}

//! 2398 CONTROL_CAR_DOOR (case @0x477AE5): car, door, status, ratio
//! The ratio is read as an INTEGER (`fild dword`) and stored as a float. door < 6 (signed): `if (ratio >= 0) m_doors[door].Open(ratio)` (CDoor array at +0x5B8, stride 0x18, 0x6F4790),
//! `m_damageManager.SetDoorStatus(door, status)` (0x6C21C0), `SetDoorDamage(door, false)` (0x6B1600). Otherwise (the swinging chassis door at +0x70C, whatever the vehicle type): `if (ratio >= 0) Open(ratio)` and the door state byte (+0x717) = (uint8)status.
//! Raw handler: the parameter has to be read as a raw dword whatever literal type the script uses.
OpcodeResult ControlCarDoor(CRunningScript& S) {
    S.CollectParameters(4);
    CVehicle* const veh    = GetVehiclePool()->GetAtRef(ScriptParams[0].iParam); // no null check
    const int32     door   = ScriptParams[1].iParam;
    const int32     status = ScriptParams[2].iParam;
    const double    ratioD = (double)ScriptParams[3].iParam; // fild
    const float     ratio  = (float)ratioD;                  // fst
    if (door < 6) {
        auto* const car = static_cast<CAutomobile*>(veh);
        if (!(ratioD < 0.0)) { // `test ah, 1; jne skip` after fcomp 0.0: skipped for st0 < 0 (and unordered)
            car->m_doors.data()[door].Open(ratio);
        }
        car->m_damageManager.SetDoorStatus((eDoors)door, (eDoorStatus)status);
        car->SetDoorDamage((eDoors)door, false);
    } else {
        auto* const car = static_cast<CAutomobile*>(veh);
        if (!(ratioD < 0.0)) {
            car->m_swingingChassis.Open(ratio);
        }
        *(reinterpret_cast<uint8*>(car) + 0x717) = (uint8)status; // m_swingingChassis.m_doorState
    }
    return OR_CONTINUE;
}

//! 2399 GET_DOOR_ANGLE_RATIO (case @0x477B8C): car, door => float (the door's current angle, CDoor +0xC)
//! door < 6 (signed): `m_doors[door].m_angle` through the x87 stack (fld/fstp: a signalling NaN gets quieted); otherwise the swinging chassis door's angle (+0x718) as a raw dword.
int32 GetDoorAngleRatio(CVehicle& veh, int32 door) {
    if (door < 6) {
        return RawFViaX87(static_cast<CAutomobile&>(veh).m_doors.data()[door].m_angle);
    }
    return RawF(static_cast<CAutomobile&>(veh).m_swingingChassis.m_angle);
}
} // namespace

void notsa::script::commands::ported::g23::RegisterHandlers() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g23");

    REGISTER_COMMAND_HANDLER(COMMAND_SET_HEATHAZE_EFFECT, SetHeathazeEffect);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_HELP_MESSAGE_BEING_DISPLAYED, IsHelpMessageBeingDisplayed);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_OBJECT_BEEN_DAMAGED_BY_WEAPON, HasObjectBeenDamagedByWeapon);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_OBJECT_LAST_WEAPON_DAMAGE, ClearObjectLastWeaponDamage);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_HUD_COLOUR, GetHudColour);
    REGISTER_COMMAND_HANDLER(COMMAND_LOCK_DOOR, LockDoor);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_MASS, SetObjectMass);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_OBJECT_MASS, GetObjectMass);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_TURN_MASS, SetObjectTurnMass);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_OBJECT_TURN_MASS, GetObjectTurnMass);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ACTIVE_MENU_ITEM, SetActiveMenuItem);
    REGISTER_COMMAND_HANDLER(COMMAND_MARK_STREAMED_SCRIPT_AS_NO_LONGER_NEEDED, MarkStreamedScriptAsNoLongerNeeded);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_STREAMED_SCRIPT, RemoveStreamedScript);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_MESSAGE_FORMATTING, SetMessageFormatting);
    REGISTER_COMMAND_HANDLER(COMMAND_START_NEW_STREAMED_SCRIPT, StartNewStreamedScript);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_WEATHER_TO_APPROPRIATE_TYPE_NOW, SetWeatherToAppropriateTypeNow);
    REGISTER_COMMAND_HANDLER(COMMAND_WINCH_CAN_PICK_OBJECT_UP, WinchCanPickObjectUp);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_AUDIO_ZONE, SwitchAudioZone);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_ENGINE_ON, SetCarEngineOn);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_LIGHTS_ON, SetCarLightsOn);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_USER_OF_CLOSEST_MAP_ATTRACTOR, GetUserOfClosestMapAttractor);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_PLANE_UNDERCARRIAGE_POSITION, GetPlaneUndercarriagePosition);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_AMBIENT_PLANES, SwitchAmbientPlanes);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_DARKNESS_EFFECT, SetDarknessEffect);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT, GetNumberOfInstancesOfStreamedScript);
    REGISTER_COMMAND_HANDLER(COMMAND_ALLOCATE_STREAMED_SCRIPT_TO_RANDOM_PED, AllocateStreamedScriptToRandomPed);
    REGISTER_COMMAND_HANDLER(COMMAND_ALLOCATE_STREAMED_SCRIPT_TO_OBJECT, AllocateStreamedScriptToObject);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_GROUP_MEMBER, GetGroupMember);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_WATER_HEIGHT_AT_COORDS, GetWaterHeightAtCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_ATTACH_CAR_TO_OBJECT, AttachCarToObject);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GARAGE_RESPRAY_FREE, SetGarageRespray);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_BULLETPROOF_VEST, SetCharBulletproofVest);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GROUP_FOLLOW_STATUS, SetGroupFollowStatus);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_SEARCHLIGHT_CLIP_IF_COLLIDING, SetSearchlightClipIfColliding);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_USES_UPPERBODY_DAMAGE_ANIMS_ONLY, SetCharUsesUpperbodyDamageAnimsOnly);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_SAY_CONTEXT, SetCharSayContext);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_EXPLOSION_VARIABLE_SHAKE, AddExplosionVariableShake);
    REGISTER_COMMAND_HANDLER(COMMAND_ATTACH_MISSION_AUDIO_TO_CHAR, AttachMissionAudioToChar);
    REGISTER_COMMAND_HANDLER(COMMAND_UPDATE_PICKUP_MONEY_PER_DAY, UpdatePickupMoneyPerDay);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NAME_OF_ENTRY_EXIT_CHAR_USED, GetNameOfEntryExitCharUsed);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_POSITION_OF_ENTRY_EXIT_CHAR_USED, GetPositionOfEntryExitCharUsed);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CHAR_TALKING, IsCharTalking);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_UP_SKIP, SetUpSkip);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_SKIP, ClearSkip);
    REGISTER_COMMAND_HANDLER(COMMAND_PRELOAD_BEAT_TRACK, PreloadBeatTrack);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_BEAT_TRACK_STATUS, GetBeatTrackStatus);
    REGISTER_COMMAND_HANDLER(COMMAND_PLAY_BEAT_TRACK, PlayBeatTrack);
    REGISTER_COMMAND_HANDLER(COMMAND_STOP_BEAT_TRACK, StopBeatTrack);
    REGISTER_COMMAND_HANDLER(COMMAND_FIND_MAX_NUMBER_OF_GROUP_MEMBERS, FindMaxNumberOfGroupMembers);
    REGISTER_COMMAND_HANDLER(COMMAND_VEHICLE_DOES_PROVIDE_COVER, VehicleDoesProvideCover);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_SNAPSHOT_PICKUP, CreateSnapshotPickup);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_HORSESHOE_PICKUP, CreateHorseshoePickup);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_OYSTER_PICKUP, CreateOysterPickup);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_OBJECT_BEEN_UPROOTED, HasObjectBeenUprooted);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_SMOKE_PARTICLE, AddSmokeParticle);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CHAR_STUCK_UNDER_CAR, IsCharStuckUnderCar);
    REGISTER_COMMAND_HANDLER(COMMAND_CONTROL_CAR_DOOR, ControlCarDoor);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_DOOR_ANGLE_RATIO, GetDoorAngleRatio);
}
