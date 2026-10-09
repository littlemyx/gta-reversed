#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group22.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>

#include "World.h"
#include "Population.h"
#include "MissionCleanup.h"
#include "PedGroups.h"
#include "GangWars.h"
#include "Shopping.h"
#include "EntryExit.h"
#include "EntryExitManager.h"
#include "TimeCycle.h"
#include "PostEffects.h"
#include "3dMarkers.h"
#include "StreamedScripts.h"
#include "Streaming.h"
#include "CutsceneMgr.h"
#include "DamageManager.h"
#include "PedAttractorManager.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Ped/PlayerPed.h"
#include "Events/EventAttractor.h"
#include "Models/PedModelInfo.h"
#include "Plugins/TwoDEffectPlugin/2dEffect.h"
#include "TaskTypes/TaskComplexUseEffect.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g22;

/*!
* Script commands ported from the exe's group processor g22 (`0x474900`, switch base 2200, jump table @0x476140), ids 2200..2249
* for the vanilla commands that had no handler of their own (S6-G, part 1). Written from the asm of each `case` (see `.notes/S6G_TABLE.md`).
* Idioms: `CPed&` / `CVehicle&` parser arguments are `GetAtRef(handle)` without a null check (exactly like the exe);
* `_ftol2` (0x821B40) = low dword of the int64 truncation (NaN / out of range => 0);
* the x87 stack values (`double`) are kept unrounded where the exe keeps them in a register (the standalone build runs the FPU at PC=24).
*/

namespace {
static_assert(offsetof(CPed, m_nPedType) == 0x598);
static_assert(offsetof(CPed, m_fMaxHealth) == 0x544);
static_assert(offsetof(CPed, m_pEnex) == 0x78C);
static_assert(offsetof(CPedModelInfo, m_nPedFlags) == 0x32);

//! 0x6002F0 (unnamed in the symbol table; used by TASK_USE_CLOSEST_MAP_ATTRACTOR): finds the closest 2D effect of type `EFFECT_ATTRACTOR` whose
//! attractor type is `attractorType`, around `pos`. Outputs (both cleared first) the effect and the entity owning it; returns whether one was found.
//! Candidates: buildings + objects (FindObjectsInRange, 3D, max 0x400); `model == -1` accepts every entity, else the model index (int16 @+0x22) must match;
//! `exclude` (the effect the ped is already using) is skipped; `name` (if not null) must equal the effect's script name (_stricmp);
//! `checkFreeSlot`: the attractor manager must have an empty slot. The ranking is the SQUARED distance (starts at 1e8 = 0x4CBEBC20), accumulated as
//! ((dz*dz + dy*dy) + dx*dx) in extended precision and compared unrounded, then stored as float.
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
        CEntity* const entity     = entities[i];
        const int32    entityMdl  = (int32)(int16)entity->m_nModelIndex;
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

// ============================================================================ g22 (ProcessCommands2200To2299 @0x474900), part 1

//! 2200 ENABLE_CRANE_CONTROLS (case @0x47493E): raise, lower, release => bytes 0xA44496 / 0xA44495 / 0xA44494 (each `!= 0`)
void EnableCraneControls(int32 raise, int32 lower, int32 release) {
    CTheScripts::bEnableCraneRaise   = raise != 0;   // 0xA44496
    CTheScripts::bEnableCraneLower   = lower != 0;   // 0xA44495
    CTheScripts::bEnableCraneRelease = release != 0; // 0xA44494
}

//! 2206 GET_RANDOM_CHAR_IN_SPHERE_ONLY_DRUGS_BUYERS (case @0x474A04): x, y, z, radius => handle (-1 = none)
//! Per ped (after createdBy == game, !bRemoveFromWorld, !bFadeOut): !IsPedDead, the ped MODEL's flag word (CPedModelInfo +0x32) has bit 0 set, no ped group.
int32 GetRandomCharInSphereOnlyDrugsBuyers(CRunningScript& S, float x, float y, float z, float radius) {
    return FindRandomCharInSphere(S, { x, y, z }, radius, [](CRunningScript& s, CPed* ped) {
        if (s.IsPedDead(ped)) { // 0x464D70
            return false;
        }
        const auto* const mi = static_cast<CPedModelInfo*>(CModelInfo::ms_modelInfoPtrs[(int32)(int16)ped->m_nModelIndex]); // no type check
        if (!(mi->m_nPedFlags & 1)) {
            return false;
        }
        return CPedGroups::GetPedsGroup(ped) == nullptr; // 0x5F7E80
    });
}

//! 2207 GET_PED_TYPE (case @0x474BD4): char => ped type (+0x598), no null check
int32 GetPedType(CPed& ped) {
    return (int32)ped.m_nPedType;
}

//! 2208 TASK_USE_CLOSEST_MAP_ATTRACTOR (case @0x474C0A): char (-1 = sequence), radius, model (0 => -1, negative => UsedObjectArray), x, y, z (only used for char == -1),
//! text label (8, the attractor's script name) => compare flag (an attractor was found).
//! Looks the closest free attractor up (0x6002F0) around the ped (or the coordinates). Not found: compare flag false.
//! Found, script is an EXTERNAL one (m_ExternalType != -1): the script is moved to the idle list and shut down, the ped gets a CEventAttractor
//! (task id TASK_COMPLEX_USE_EFFECT) in its event group and the attractor as "effect in use"; the handler returns OR_INTERRUPT (no compare flag update).
//! Otherwise: GivePedScriptedTask(char, new CTaskComplexUseEffect(effect, entity), cmd); compare flag true.
OpcodeResult TaskUseClosestMapAttractor(CRunningScript& S, eScriptCommands command) {
    S.CollectParameters(6);
    const int32 pedHandle = ScriptParams[0].iParam;
    const float radius    = ScriptParams[1].fParam;
    int32       model     = ScriptParams[2].iParam;
    CVector     pos{ ScriptParams[3].fParam, ScriptParams[4].fParam, ScriptParams[5].fParam };

    char name[16]{};
    S.ReadTextLabelFromScript(name, 8); // 0x463D50

    CPed* ped = nullptr;
    if (pedHandle != -1) {
        ped = GetPedPool()->GetAtRef(pedHandle); // no null check
        pos = ped->GetPosition();
    }
    C2dEffect* const inUse = ped ? ped->GetIntelligence()->GetEffectInUse() : nullptr; // 0x6018D0

    if (model == 0) {
        model = -1;
    } else if (model < 0) {
        model = CTheScripts::UsedObjectArray[-model].nModelIndex; // 0xA44B88 + 0x1C * -model
    }

    C2dEffect* effect = nullptr;
    CEntity*   entity = nullptr;
    FindClosestScriptedAttractor(pos, radius, model, PED_ATTRACTOR_TRIGGER_SCRIPT, name, true, inUse, effect, entity); // the return value is not used, `outEffect` is tested
    if (!effect) {
        S.UpdateCompareFlag(false);
        return OR_CONTINUE;
    }

    if (S.m_ExternalType != -1) {
        S.RemoveScriptFromList(&CTheScripts::pActiveScripts); // 0xA8B42C
        S.AddScriptToList(&CTheScripts::pIdleScripts);        // 0xA8B428
        S.ShutdownThisScript();                               // 0x465AA0

        CEventAttractor event{ reinterpret_cast<C2dEffectPedAttractor*>(effect), entity, true, TASK_COMPLEX_USE_EFFECT }; // 0x4AF350, task id word (+0xE) = 0xE9
        ped->GetIntelligence()->m_eventGroup.Add(&event, true); // 0x4AB420 (no null check on `ped`: char == -1 crashes the exe here)
        ped->GetIntelligence()->SetEffectInUse(effect);         // 0x6018E0
        return OR_INTERRUPT;
    }

    S.GivePedScriptedTask(pedHandle, new CTaskComplexUseEffect{ reinterpret_cast<C2dEffectPedAttractor*>(effect), entity }, (int32)command); // 0x6321F0, 0x465C20
    S.UpdateCompareFlag(true);
    return OR_CONTINUE;
}

//! 2211 CAN_TRIGGER_GANG_WAR_WHEN_ON_A_MISSION (case @0x474E88): flag => byte 0x96AB93
void CanTriggerGangWarWhenOnAMission(int32 flag) {
    CGangWars::bCanTriggerGangWarWhenOnAMission = flag != 0;
}

//! 2212 CONTROL_MOVABLE_VEHICLE_PART (case @0x474EA8): car, angle => CAutomobile::UpdateMovingCollision(angle) [0x6A1460] (no type check; the angle is the raw dword)
//! (the exe leaves the second parameter on the stack for the callee: `push angle; push car; GetAtRef; call UpdateMovingCollision`)
void ControlMovableVehiclePart(CVehicle& veh, float angle) {
    static_cast<CAutomobile&>(veh).UpdateMovingCollision(angle);
}

//! 2213 WINCH_CAN_PICK_VEHICLE_UP (case @0x474ED7): car, flag => +0x42D bit 0x10 (`bWinchCanPickMeUp`)
void WinchCanPickVehicleUp(CVehicle& veh, int32 flag) {
    veh.vehicleFlags.bWinchCanPickMeUp = flag != 0;
}

//! 2214 OPEN_CAR_DOOR_A_BIT (case @0x474F22): car, door, ratio  -- as 1623 OPEN_CAR_DOOR with the ratio instead of 1.0f:
//! node = GetCarNodeIndexFromDoor(door) [0x6C26F0]; nothing if the door is missing (virtual slot 38) or `CAutomobile::m_aCarNodes[node]` (+0x648, no type check) is null;
//! else OpenDoor(nullptr, node, door, ratio, true) (slot 27)
void OpenCarDoorABit(CVehicle& veh, eDoors door, float ratio) {
    const auto node = CDamageManager::GetCarNodeIndexFromDoor(door);
    if (veh.IsDoorMissing(door)) {
        return;
    }
    if (!static_cast<CAutomobile&>(veh).m_aCarNodes[node]) {
        return;
    }
    veh.OpenDoor(nullptr, node, door, ratio, true);
}

//! 2215 IS_CAR_DOOR_FULLY_OPEN (case @0x474F8C): car, door => compare flag: virtual slot 33 (`IsDoorFullyOpenU32`, 0x6A63E0 for CAutomobile) called with the NODE index
//! (CAR_DOOR_* 8..11 map to the eDoors versions, everything else gives false)
bool IsCarDoorFullyOpen(CVehicle& veh, eDoors door) {
    const auto node = CDamageManager::GetCarNodeIndexFromDoor(door);
    return veh.IsDoorFullyOpenU32((uint32)node);
}

//! 2216 SET_ALWAYS_DRAW_3D_MARKERS (case @0x474FD9): flag => C3dMarkers::ForceRender(flag != 0) [0x722870]
void SetAlwaysDraw3dMarkers(int32 flag) {
    C3dMarkers::ForceRender(flag != 0);
}

//! 2217 STREAM_SCRIPT (case @0x47500D): script id (low 16 bits)
//! idx = (int16) StreamedScripts.GetProperIndexFromIndexUsedByScript(id) [0x470810] (written back to ScriptParams[0]); RequestModel(RESOURCE_ID_SCM + idx, STREAMING_MISSION_REQUIRED)
//! (no check for idx == -1)
void StreamScript(int32 id) {
    const int32 idx = (int32)CTheScripts::StreamedScripts.GetProperIndexFromIndexUsedByScript((int16)(uint16)id);
    ScriptParams[0].iParam = idx; // the exe leaves the index in the parameter buffer
    CStreaming::RequestModel(SCMToModelId(idx), STREAMING_MISSION_REQUIRED); // 0x6676 + idx, flags 4
}

//! 2219 HAS_STREAMED_SCRIPT_LOADED (case @0x475049): script id (low 16 bits) => compare flag: the streaming info of RESOURCE_ID_SCM + idx has load state 1 (LOADED)
//! (the info array is indexed with idx without a check, -1 included)
bool HasStreamedScriptLoaded(int32 id) {
    const int32 idx = (int32)CTheScripts::StreamedScripts.GetProperIndexFromIndexUsedByScript((int16)(uint16)id);
    ScriptParams[0].iParam = idx;
    return CStreaming::GetInfo(SCMToModelId(idx)).m_LoadState == LOADSTATE_LOADED;
}

//! 2220 SET_GANG_WARS_TRAINING_MISSION (case @0x475085): flag => byte 0x96AB91
void SetGangWarsTrainingMission(int32 flag) {
    CGangWars::bTrainingMission = flag != 0;
}

//! 2221 SET_CHAR_HAS_USED_ENTRY_EXIT (case @0x4750A5): char, x, y, radius
//! ee = GetInSlot(FindNearestEntryExit((x, y), radius, -1)) [0x43EF00] (no null check); `link` = ee->m_pLink ? ee->m_pLink : ee, and when there IS a link
//! its own link pointer is set to `ee` (a side effect). ped.areaCode (+0x2F) = link.area; ped.m_pEnex (+0x78C) = link.area != 0 ? ee : null.
//! If the ped is a player: the entry/exit stack is reset to 1 entry (`ee`) and the extra colour of `link` is started (sky colour > 0: colour - 1) or stopped.
void SetCharHasUsedEntryExit(CPed& ped, float x, float y, float radius) {
    static_assert(offsetof(CEntryExit, m_nArea) == 0x32 && offsetof(CEntryExit, m_nSkyColor) == 0x33 && offsetof(CEntryExit, m_pLink) == 0x38);
    const auto idx = CEntryExitManager::FindNearestEntryExit(CVector2D{ x, y }, radius, -1); // 0x43F4B0
    auto* const ee = CEntryExitManager::GetInSlot(idx);
    CEntryExit* link = ee;
    if (ee->m_pLink) {
        link          = ee->m_pLink;
        link->m_pLink = ee;
    }
    ped.SetAreaCode((eAreaCodes)(uint8)link->m_nArea); // byte store to +0x2F
    ped.m_pEnex = link->m_nArea != 0 ? ee : nullptr;
    if (!ped.IsPlayer()) { // 0x5DF8F0
        return;
    }
    CEntryExitManager::ms_entryExitStackPosn = 0;
    CEntryExitManager::AddEntryExitToStack(ee); // 0x43E410
    if (link->m_nSkyColor > 0) {
        CTimeCycle::StartExtraColour((int32)link->m_nSkyColor - 1, false); // 0x55FEC0
    } else {
        CTimeCycle::StopExtraColour(false); // 0x55FF20
    }
}

//! 2223 SET_CHAR_MAX_HEALTH (case @0x475170): char, health (int) => float at +0x544
void SetCharMaxHealth(CPed& ped, int32 health) {
    ped.m_fMaxHealth = (float)health; // fild + fstp
}

//! 2225 SET_NIGHT_VISION (case @0x4751A5): flag => CPostEffects::ScriptNightVisionSwitch(flag != 0) [0x701120]
void SetNightVision(int32 flag) {
    CPostEffects::ScriptNightVisionSwitch(flag != 0);
}

//! 2226 SET_INFRARED_VISION (case @0x4751D9): flag => CPostEffects::ScriptInfraredVisionSwitch(flag != 0) [0x701140]
void SetInfraredVision(int32 flag) {
    CPostEffects::ScriptInfraredVisionSwitch(flag != 0);
}

//! 2228..2233 IS_{GLOBAL,LOCAL}_VAR_BIT_SET_{CONST,VAR,LVAR} (shared case @0x475244): value, bit => compare flag: `(1 << (bit & 31)) & value != 0`
//! (`shl cl`: the shift count is masked to 5 bits; the value comes from CollectParameters so every variable kind is handled by it)
bool IsVarBitSet(int32 value, int32 bit) {
    return (((uint32)1 << (bit & 31)) & (uint32)value) != 0;
}

//! 2234..2236 SET_GLOBAL_VAR_BIT_*, 2237..2239 SET_LOCAL_VAR_BIT_* (cases @0x475274 / @0x4752A2): the variable pointer is taken FIRST
//! (GetPointerToScriptVariable, the argument is ignored), then CollectParameters(1) => bit; `*var |= 1 << (bit & 31)`
template<eScriptVariableType VarType>
OpcodeResult SetVarBit(CRunningScript& S) {
    auto* const var = S.GetPointerToScriptVariable(VarType);
    S.CollectParameters(1);
    var->uParam |= (uint32)1 << (ScriptParams[0].iParam & 31);
    return OR_CONTINUE;
}

//! 2240..2242 CLEAR_GLOBAL_VAR_BIT_*, 2243..2245 CLEAR_LOCAL_VAR_BIT_* (cases @0x4752CC / @0x4752FC): as above with `*var &= ~(1 << (bit & 31))`
template<eScriptVariableType VarType>
OpcodeResult ClearVarBit(CRunningScript& S) {
    auto* const var = S.GetPointerToScriptVariable(VarType);
    S.CollectParameters(1);
    var->uParam &= ~((uint32)1 << (ScriptParams[0].iParam & 31));
    return OR_CONTINUE;
}

//! 2246 SET_CHAR_CAN_BE_KNOCKED_OFF_BIKE (case @0x47532C): char, value => the 2 bit field `CantBeKnockedOffBike` (dword +0x474, bits 27..28) = value & 3
void SetCharCanBeKnockedOffBike(CPed& ped, int32 value) {
    ped.CantBeKnockedOffBike = (uint8)(value & 3);
}

//! 2247 SET_CHAR_COORDINATES_DONT_WARP_GANG (case @0x47536F): char, x, y, z => CRunningScript::SetCharCoordinates(ped, (x, y, z), warpGang = false, offset = true) [0x464DC0]
void SetCharCoordinatesDontWarpGang(CRunningScript& S, CPed& ped, CVector pos) {
    S.SetCharCoordinates(ped, pos, false, true);
}

//! 2248 ADD_PRICE_MODIFIER (case @0x4753B0): key (raw dword), price => CShopping::AddPriceModifier(key, price) [0x49BDD0]
void AddPriceModifier(uint32 key, int32 price) {
    CShopping::AddPriceModifier(key, price);
}

//! 2249 REMOVE_PRICE_MODIFIER (case @0x4753D6): key => CShopping::RemovePriceModifier(key) [0x49ACD0]
void RemovePriceModifier(uint32 key) {
    CShopping::RemovePriceModifier(key);
}
}; // namespace

void notsa::script::commands::ported::g22::RegisterG22a() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g22a");

    REGISTER_COMMAND_HANDLER(COMMAND_ENABLE_CRANE_CONTROLS, EnableCraneControls);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_RANDOM_CHAR_IN_SPHERE_ONLY_DRUGS_BUYERS, GetRandomCharInSphereOnlyDrugsBuyers);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_PED_TYPE, GetPedType);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_USE_CLOSEST_MAP_ATTRACTOR, TaskUseClosestMapAttractor);
    REGISTER_COMMAND_HANDLER(COMMAND_CAN_TRIGGER_GANG_WAR_WHEN_ON_A_MISSION, CanTriggerGangWarWhenOnAMission);
    REGISTER_COMMAND_HANDLER(COMMAND_CONTROL_MOVABLE_VEHICLE_PART, ControlMovableVehiclePart);
    REGISTER_COMMAND_HANDLER(COMMAND_WINCH_CAN_PICK_VEHICLE_UP, WinchCanPickVehicleUp);
    REGISTER_COMMAND_HANDLER(COMMAND_OPEN_CAR_DOOR_A_BIT, OpenCarDoorABit);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_DOOR_FULLY_OPEN, IsCarDoorFullyOpen);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ALWAYS_DRAW_3D_MARKERS, SetAlwaysDraw3dMarkers);
    REGISTER_COMMAND_HANDLER(COMMAND_STREAM_SCRIPT, StreamScript);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_STREAMED_SCRIPT_LOADED, HasStreamedScriptLoaded);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GANG_WARS_TRAINING_MISSION, SetGangWarsTrainingMission);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_HAS_USED_ENTRY_EXIT, SetCharHasUsedEntryExit);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_MAX_HEALTH, SetCharMaxHealth);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_NIGHT_VISION, SetNightVision);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_INFRARED_VISION, SetInfraredVision);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_GLOBAL_VAR_BIT_SET_CONST, IsVarBitSet);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_GLOBAL_VAR_BIT_SET_VAR, IsVarBitSet);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_GLOBAL_VAR_BIT_SET_LVAR, IsVarBitSet);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_LOCAL_VAR_BIT_SET_CONST, IsVarBitSet);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_LOCAL_VAR_BIT_SET_VAR, IsVarBitSet);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_LOCAL_VAR_BIT_SET_LVAR, IsVarBitSet);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GLOBAL_VAR_BIT_CONST, SetVarBit<VAR_GLOBAL>);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GLOBAL_VAR_BIT_VAR, SetVarBit<VAR_GLOBAL>);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GLOBAL_VAR_BIT_LVAR, SetVarBit<VAR_GLOBAL>);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_LOCAL_VAR_BIT_CONST, SetVarBit<VAR_LOCAL>);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_LOCAL_VAR_BIT_VAR, SetVarBit<VAR_LOCAL>);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_LOCAL_VAR_BIT_LVAR, SetVarBit<VAR_LOCAL>);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_GLOBAL_VAR_BIT_CONST, ClearVarBit<VAR_GLOBAL>);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_GLOBAL_VAR_BIT_VAR, ClearVarBit<VAR_GLOBAL>);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_GLOBAL_VAR_BIT_LVAR, ClearVarBit<VAR_GLOBAL>);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_LOCAL_VAR_BIT_CONST, ClearVarBit<VAR_LOCAL>);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_LOCAL_VAR_BIT_VAR, ClearVarBit<VAR_LOCAL>);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_LOCAL_VAR_BIT_LVAR, ClearVarBit<VAR_LOCAL>);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_CAN_BE_KNOCKED_OFF_BIKE, SetCharCanBeKnockedOffBike);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_COORDINATES_DONT_WARP_GANG, SetCharCoordinatesDontWarpGang);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_PRICE_MODIFIER, AddPriceModifier);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_PRICE_MODIFIER, RemovePriceModifier);
}
