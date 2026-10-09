#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group25_26.hpp"

#include <bit>
#include <cstddef>

#include "World.h"
#include "Population.h"
#include "GangWars.h"
#include "MenuSystem.h"
#include "MenuManager.h"
#include "Font.h"
#include "Messages.h"
#include "Text/Text.h"
#include "PlayerPedData.h"
#include "PedIntelligence.h"
#include "Models/ModelInfo.h"
#include "Entity/Object/Object.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Train.h"
#include "TaskComplexSignalAtPed.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g25_26;

/*!
* Script commands of the exe's group processor for ids 2500..2599 (`CRunningScript::ProcessCommands2500To2599` @0x47A760, switch base 2500),
* ids 2556..2595 (S6-J, part b): the vanilla commands of that range that had no handler of their own. See Group25_26a.cpp for the conventions.
*/

namespace {
static_assert(offsetof(CVehicle, handlingFlags) == 0x38C);
static_assert(offsetof(CVehicle, m_nTertiaryColor) == 0x436 && offsetof(CVehicle, m_nPrimaryColor) == 0x434 && offsetof(CVehicle, m_nSecondaryColor) == 0x435);
static_assert(sizeof(tScriptSearchlight) == 0x7C && offsetof(tScriptSearchlight, m_bEnableShadow) == 2);

//! 2556 IS_OBJECT_INTERSECTING_WORLD (case @0x47B509): object => compare flag = object->TestCollision(false) (vtable +0x34)
bool IsObjectIntersectingWorld(CObject& obj) {
    return obj.TestCollision(false);
}

//! 2557 GET_STRING_WIDTH (case @0x47B543): text label (8) => width. No CollectParameters.
//! CFont::GetStringWidth(TheText.Get(label), full = true, scriptText = true) [0x71A0E0] -> _ftol2 of the float returned in ST0
OpcodeResult GetStringWidth(CRunningScript& S) {
    char label[16]{}; // NOTSA: zero-initialised (the exe leaves the 8 char label unterminated)
    S.ReadTextLabelFromScript(label, 8); // 0x463D50
    const float width = CFont::GetStringWidth(TheText.Get(label), true, true); // 0x6A0050, 0x71A0E0
    ScriptParams[0].iParam = Ftol((double)width);
    S.StoreParameters(1);
    return OR_CONTINUE;
}

//! 2558 RESET_VEHICLE_HYDRAULICS (case @0x47B587): car  -- if handlingFlags (+0x38C) & 0x20000 [bHydraulicInst] and the type (+0x590) is 0 [automobile]:
//! CAutomobile::m_wMiscComponentAngle (+0x86C) = 0
void ResetVehicleHydraulics(CVehicle& veh) {
    if (!veh.handlingFlags.bHydraulicInst) {
        return;
    }
    if (veh.m_nVehicleType != VEHICLE_TYPE_AUTOMOBILE) {
        return;
    }
    static_cast<CAutomobile&>(veh).m_wMiscComponentAngle = 0;
}

//! 2561 IS_THIS_MODEL_A_CAR (case @0x47B619): model (raw id, not translated) => compare flag. CModelInfo::IsCarModel [0x4C5AA0]
bool IsThisModelACar(int32 model) {
    return CModelInfo::IsCarModel(model);
}

//! 2562 SWITCH_ON_GROUND_SEARCHLIGHT (case @0x47B63A): searchlight handle, flag
//! idx = GetActualScriptThingIndex(handle, 2 [SEARCH_LIGHT]); idx >= 0 (signed; no upper bound check) => ScriptSearchLightArray[idx] (stride 0x7C, 0xA94D68)
//! byte +2 (m_bEnableShadow) = low byte of flag (raw byte store)
void SwitchOnGroundSearchlight(int32 handle, int32 flag) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(handle, SCRIPT_THING_SEARCH_LIGHT);
    if (idx < 0) {
        return;
    }
    *reinterpret_cast<uint8*>(&CTheScripts::ScriptSearchLightArray[idx].m_bEnableShadow) = (uint8)flag;
}

//! 2563 IS_GANG_WAR_FIGHTING_GOING_ON (case @0x47B672): => compare flag = CGangWars::GangWarFightingGoingOn() [0x443AC0]
bool IsGangWarFightingGoingOn() {
    return CGangWars::GangWarFightingGoingOn();
}

//! 2566 IS_NEXT_STATION_ALLOWED (case @0x47B698): train (vehicle pool handle) => compare flag = CTrain::IsNextStationAllowed(train) [0x6F7260]
bool IsNextStationAllowed(CVehicle& veh) {
    return CTrain::IsNextStationAllowed(static_cast<CTrain*>(&veh));
}

//! 2568 GET_STRING_WIDTH_WITH_NUMBER (case @0x47B6EB): text label (8), then CollectParameters(1): number => width
//! text = TheText.Get(label) [0x6A0050]; CMessages::InsertNumberInString(text, number, -1 x5, out) [0x69DE90]; width = _ftol2(CFont::GetStringWidth(out, true, true)) [0x71A0E0]
OpcodeResult GetStringWidthWithNumber(CRunningScript& S) {
    char label[16]{}; // NOTSA: zero-initialised
    S.ReadTextLabelFromScript(label, 8); // 0x463D50
    const GxtChar* const text = TheText.Get(label);
    S.CollectParameters(1);
    GxtChar out[256]{};
    CMessages::InsertNumberInString(text, ScriptParams[0].iParam, -1, -1, -1, -1, -1, out);
    const float width = CFont::GetStringWidth(out, true, true);
    ScriptParams[0].iParam = Ftol((double)width);
    S.StoreParameters(1);
    return OR_CONTINUE;
}

//! 2572 IS_PLAYER_USING_JETPACK (case @0x47B84C): player => compare flag = Players[player].m_pPed->GetIntelligence()->GetTaskJetPack() != null [0x601110]
bool IsPlayerUsingJetpack(int32 playerId) {
    return CWorld::Players[playerId].m_pPed->GetIntelligence()->GetTaskJetPack() != nullptr;
}

//! 2575 HAS_LANGUAGE_CHANGED (case @0x47B8AD): => compare flag = FrontEndMenuManager.HasLanguageChanged() [0x573CD0]
bool HasLanguageChanged() {
    return FrontEndMenuManager.HasLanguageChanged();
}

//! 2577 SET_EXTRA_CAR_COLOURS (case @0x47B8FA): car, colour3, colour4  -- bytes +0x436 (m_nTertiaryColor), +0x437 (m_nQuaternaryColor)
void SetExtraCarColours(CVehicle& veh, int32 colour3, int32 colour4) {
    veh.m_nTertiaryColor   = (uint8)colour3;
    veh.m_nQuaternaryColor = (uint8)colour4;
}

//! 2579 MANAGE_ALL_POPULATION (case @0x47B977): CPopulation::ManageAllPopulation() [0x6160A0]
void ManageAllPopulation() {
    CPopulation::ManageAllPopulation();
}

//! 2581 HAS_CAR_BEEN_RESPRAYED (case @0x47B9AF): car => compare flag = bHasBeenResprayed (+0x42F bit 0); the flag is reset when it was set
bool HasCarBeenResprayed(CVehicle& veh) {
    if (!veh.vehicleFlags.bHasBeenResprayed) {
        return false;
    }
    veh.vehicleFlags.bHasBeenResprayed = false;
    return true;
}

//! 2586 TASK_PLAY_ANIM_SECONDARY (case @0x47BB08): no CollectParameters here, CRunningScript::PlayAnimScriptCommand(command) [0x470150] reads everything
void TaskPlayAnimSecondary(eScriptCommands command, CRunningScript& S) {
    S.PlayAnimScriptCommand(command);
}

//! 2589 TASK_HAND_GESTURE (case @0x47BBF7): ped A, ped B
//! A gets CTaskComplexSignalAtPed(B [CPedPool::GetAtRef 0x404910], -1, false) [0x660A30]
void TaskHandGesture(eScriptCommands command, CRunningScript& S, ScriptEntity<CPed> pedA, ScriptEntity<CPed> pedB) {
    S.GivePedScriptedTask(pedA.h, new CTaskComplexSignalAtPed{ pedB.e, -1, false }, command);
}

//! 2593 IMPROVE_CAR_BY_CHEATING (case @0x47BCE8): car, flag  -- bUseCarCheats (+0x42F bit 1) = bit 0 of flag (`shl dl, 1; xor; and 2`)
void ImproveCarByCheating(CVehicle& veh, int32 flag) {
    veh.vehicleFlags.bUseCarCheats = (flag & 1) != 0;
}

//! 2594 CHANGE_CAR_COLOUR_FROM_MENU (case @0x47BD27): menu (byte), car, which, grid index (byte)
//! colour = CMenuSystem::GetCarColourFromGrid(menu, gridIndex) [0x5822B0]; which == 1 => primary colour (+0x434) = colour, else secondary (+0x435)
void ChangeCarColourFromMenu(int32 menu, CVehicle& veh, int32 which, int32 gridIndex) {
    const uint8 colour = CMenuSystem::GetCarColourFromGrid((MenuId)(uint8)menu, (uint8)gridIndex);
    if (which == 1) {
        veh.m_nPrimaryColor = colour;
    } else {
        veh.m_nSecondaryColor = colour;
    }
}

//! 2595 HIGHLIGHT_MENU_ITEM (case @0x47BD82): menu (byte), item (byte), bought (byte) => CMenuSystem::HighlightOneItem(menu, item, bought) [0x581C10]
//! (the exe stores the raw byte of `bought` into m_abRowAlreadyBought, here it is the bool (bought != 0))
void HighlightMenuItem(int32 menu, int32 item, int32 bought) {
    CMenuSystem::HighlightOneItem((MenuId)(uint8)menu, (uint8)item, (uint8)bought != 0);
}
}; // namespace

void notsa::script::commands::ported::g25_26::RegisterG25b() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g25b");

    REGISTER_COMMAND_HANDLER(COMMAND_IS_OBJECT_INTERSECTING_WORLD, IsObjectIntersectingWorld);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_STRING_WIDTH, GetStringWidth);
    REGISTER_COMMAND_HANDLER(COMMAND_RESET_VEHICLE_HYDRAULICS, ResetVehicleHydraulics);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_THIS_MODEL_A_CAR, IsThisModelACar);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_ON_GROUND_SEARCHLIGHT, SwitchOnGroundSearchlight);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_GANG_WAR_FIGHTING_GOING_ON, IsGangWarFightingGoingOn);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_NEXT_STATION_ALLOWED, IsNextStationAllowed);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_STRING_WIDTH_WITH_NUMBER, GetStringWidthWithNumber);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_PLAYER_USING_JETPACK, IsPlayerUsingJetpack);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_LANGUAGE_CHANGED, HasLanguageChanged);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_EXTRA_CAR_COLOURS, SetExtraCarColours);
    REGISTER_COMMAND_HANDLER(COMMAND_MANAGE_ALL_POPULATION, ManageAllPopulation);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_CAR_BEEN_RESPRAYED, HasCarBeenResprayed);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_PLAY_ANIM_SECONDARY, TaskPlayAnimSecondary);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_HAND_GESTURE, TaskHandGesture);
    REGISTER_COMMAND_HANDLER(COMMAND_IMPROVE_CAR_BY_CHEATING, ImproveCarByCheating);
    REGISTER_COMMAND_HANDLER(COMMAND_CHANGE_CAR_COLOUR_FROM_MENU, ChangeCarColourFromMenu);
    REGISTER_COMMAND_HANDLER(COMMAND_HIGHLIGHT_MENU_ITEM, HighlightMenuItem);
}
