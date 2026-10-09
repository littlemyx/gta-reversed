#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group25_26.hpp"

#include <bit>
#include <cstddef>

#include "World.h"
#include "GameLogic.h"
#include "ScriptsForBrains.h"
#include "3dMarkers.h"
#include "MenuManager.h"
#include "Hud.h"
#include "Messages.h"
#include "Text/Text.h"
#include "PedGroups.h"
#include "PlayerInfo.h"
#include "PlayerPed.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Entity/Ped/PlayerPed.h"
#include "TaskComplexFollowNodeRoute.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g25_26;

/*!
* Script commands of the exe's group processor for ids 2600..2699 (`CRunningScript::ProcessCommands2600To2699` @0x479DA0, switch base 2600,
* 39 cases: 2600..2638), ids 2602..2632 (S6-J, part c): the 16 vanilla commands of that range that had no handler of their own.
* (2638 is DO_DEBUG_STUFF in the PC build and already registered, `TRAP_READERS_script_missing.txt` lists its Xbox alias name.)
* Conventions: see Group25_26a.cpp.
*/

namespace {
static_assert(offsetof(CVehicle, m_fHealth) == 0x4C0);
static_assert(offsetof(CPlayerInfo, m_PlayerData) + offsetof(CPlayerPedData, m_nModelIndexOfLastBuildingShot) == 0xA0);
static_assert(offsetof(tUsedObject, nModelIndex) == 0x18 && sizeof(tUsedObject) == 0x1C);

//! 2602 IS_THIS_HELP_MESSAGE_BEING_DISPLAYED (case @0x479E75): text label (8) => compare flag. No CollectParameters.
//! text = TheText.Get(label) [0x6A0050]; if CHud::HelpMessageDisplayed() [0x588B50]: StringCopy(buf, text, 400) [0x69DB70];
//! InsertPlayerControlKeysInString(buf) [0x69E160]; result = StringCompare(buf, CHud::m_pHelpMessageToPrint [0xBAA480], GetStringLength(buf) [0x69DB50]) [0x69DBD0]
OpcodeResult IsThisHelpMessageBeingDisplayed(CRunningScript& S) {
    char label[16]{}; // NOTSA: zero-initialised (the exe leaves the 8 char label unterminated)
    S.ReadTextLabelFromScript(label, 8); // 0x463D50
    const GxtChar* const text = TheText.Get(label);
    bool                 result = false;
    if (CHud::HelpMessageDisplayed()) {
        GxtChar buf[400]{};
        CMessages::StringCopy(buf, text, 400);
        CMessages::InsertPlayerControlKeysInString(buf);
        result = CMessages::StringCompare(buf, CHud::m_pHelpMessageToPrint, (uint16)CMessages::GetStringLength(buf));
    }
    S.UpdateCompareFlag(result);
    return OR_CONTINUE;
}

//! 2606 TASK_FOLLOW_PATH_NODES_TO_COORD_WITH_RADIUS (case @0x479F3D): CollectParameters(7): ped, x, y, z, moveState, time, radius
//! time == -1 => the default 0xC350 (50000, the dword at 0x86FCAC); time == -2 => -1.
//! new CTaskComplexFollowNodeRoute(moveState, pos, radius, slowDownDist = 3.0 [0x86FCA4], heightChange = 2.0 [0x86FCA8], true, time, true) [0x66EA30]
void TaskFollowPathNodesToCoordWithRadius(eScriptCommands command, CRunningScript& S, int32 pedHandle, CVector pos, int32 moveState, int32 time, float radius) {
    if (time == -1) {
        time = 0xC350;
    } else if (time == -2) {
        time = -1;
    }
    S.GivePedScriptedTask(
        pedHandle,
        new CTaskComplexFollowNodeRoute{ (eMoveState)(uint32)moveState, pos, radius, 3.0f, 2.0f, true, time, true },
        command
    );
}

//! 2608 FIX_CAR (case @0x47A020): car (not null checked)  -- vehicle->Fix() (vtable +0xC8), then m_fHealth (+0x4C0) = 1000.0f (0x447A0000)
void FixCar(CVehicle& veh) {
    veh.Fix();
    veh.m_fHealth = std::bit_cast<float>(0x447A0000u);
}

//! 2609 SET_PLAYER_GROUP_TO_FOLLOW_NEVER (case @0x47A057): player, flag  -- FindPlayerPed(player)->ForceGroupToNeverFollow(flag != 0) [0x60C800] (no null check)
void SetPlayerGroupToFollowNever(int32 playerId, int32 flag) {
    FindPlayerPed(playerId)->ForceGroupToNeverFollow(flag != 0);
}

//! 2613 SET_UP_SKIP_FOR_VEHICLE_FINISHED_BY_SCRIPT (case @0x47A129): x, y, z, heading (float), vehicle
//! CGameLogic::SetUpSkip(pos, heading, false, vehicle, true) [0x4423C0]
void SetUpSkipForVehicleFinishedByScript(CVector pos, float heading, CVehicle& veh) {
    CGameLogic::SetUpSkip(pos, heading, false, &veh, true);
}

//! 2614 IS_SKIP_WAITING_FOR_SCRIPT_TO_FADE_IN (case @0x47A195): => compare flag = CGameLogic::IsSkipWaitingForScriptToFadeIn() [0x4416C0]
bool IsSkipWaitingForScriptToFadeIn() {
    return CGameLogic::IsSkipWaitingForScriptToFadeIn();
}

//! 2615 FORCE_ALL_VEHICLE_LIGHTS_OFF (case @0x47A1B1): flag  -- CVehicle::ms_forceVehicleLightsOff (0xC1CC18) = low byte of flag (raw byte store)
void ForceAllVehicleLightsOff(int32 flag) {
    *reinterpret_cast<uint8*>(&CVehicle::ms_forceVehicleLightsOff) = (uint8)flag;
}

//! 2618 IS_LAST_BUILDING_MODEL_SHOT_BY_PLAYER (case @0x47A1E7): player, model => compare flag
//! model < 0 (signed): model = CTheScripts::UsedObjectArray[-model].nModelIndex (stride 0x1C, field +0x18 [0xA44B88])
//! then CWorld::Players[player] +0xA0 (m_PlayerData.m_nModelIndexOfLastBuildingShot) == model
bool IsLastBuildingModelShotByPlayer(int32 playerId, int32 model) {
    if (model < 0) {
        model = CTheScripts::UsedObjectArray[0u - (uint32)model].nModelIndex;
    }
    return (int32)CWorld::Players[playerId].m_PlayerData.m_nModelIndexOfLastBuildingShot == model;
}

//! 2619 CLEAR_LAST_BUILDING_MODEL_SHOT_BY_PLAYER (case @0x47A23B): player  -- Players[player] +0xA0 (m_nModelIndexOfLastBuildingShot) = -1
void ClearLastBuildingModelShotByPlayer(int32 playerId) {
    CWorld::Players[playerId].m_PlayerData.m_nModelIndexOfLastBuildingShot = (uint32)-1;
}

//! 2622 GET_RANDOM_CHAR_IN_AREA_OFFSET_NO_SAVE (case @0x47A2CD): CollectParameters(6): x, y, z, dx, dy, dz => ped handle (-1 = none)
//! The box is (x - dx, y - dy, z - dz) .. (dx + x, dy + y, dz + z), each bound computed on the x87 stack and stored as float.
//! Peds from the last pool slot to the first (the first one that fits is taken): createdBy == 1 (game), !bRemoveFromWorld (+0x1C bit 11),
//! !bFadeOut (+0x470 bit 3), !IsPedDead [0x464D70], !bInVehicle (+0x46C bit 8), no ped group (CPedGroups::GetPedsGroup 0x5F7E80), and the position inside
//! the box: per axis `pos < min` (or NaN) rejects, `pos > max` (or NaN) rejects. NO_SAVE: no mission-ped marking, no last random ped id.
//! The result is the pool ref (CPool::GetRef 0x4442D0).
OpcodeResult GetRandomCharInAreaOffsetNoSave(CRunningScript& S) {
    S.CollectParameters(6);
    const float x = ScriptParams[0].fParam, y = ScriptParams[1].fParam, z = ScriptParams[2].fParam;
    const float dx = ScriptParams[3].fParam, dy = ScriptParams[4].fParam, dz = ScriptParams[5].fParam;
    const float minX = (float)((double)x - (double)dx);
    const float minY = (float)((double)y - (double)dy);
    const float minZ = (float)((double)z - (double)dz);
    const float maxX = (float)((double)dx + (double)x);
    const float maxY = (float)((double)dy + (double)y);
    const float maxZ = (float)((double)dz + (double)z);

    int32 result = -1;
    for (auto i = GetPedPool()->GetSize(); i-- > 0;) {
        CPed* const ped = GetPedPool()->GetAt(i);
        if (!ped) {
            continue;
        }
        if (ped->GetCreatedBy() != PED_GAME) {
            continue;
        }
        if (ped->m_bRemoveFromWorld || ped->bFadeOut) {
            continue;
        }
        if (S.IsPedDead(ped)) {
            continue;
        }
        if (ped->bInVehicle) {
            continue;
        }
        if (CPedGroups::GetPedsGroup(ped)) {
            continue;
        }
        const CVector pos = ped->GetPosition();
        if (!(pos.x >= minX) || !(pos.x <= maxX)) {
            continue;
        }
        if (!(pos.y >= minY) || !(pos.y <= maxY)) {
            continue;
        }
        if (!(pos.z >= minZ) || !(pos.z <= maxZ)) {
            continue;
        }
        result = GetPedPool()->GetRef(ped);
        break;
    }
    ScriptParams[0].iParam = result;
    S.StoreParameters(1);
    return OR_CONTINUE;
}

//! 2623 SET_SCRIPT_COOP_GAME (case @0x47A498): flag  -- CGameLogic::bScriptCoopGameGoingOn (0x96A8A8) = (flag != 0)
void SetScriptCoopGame(int32 flag) {
    CGameLogic::bScriptCoopGameGoingOn = flag != 0;
}

//! 2624 CREATE_USER_3D_MARKER (case @0x47A4B8): x, y, z, colour => marker slot  -- C3dMarkers::User3dMarkerSet(x, y, z, colour) [0x720FD0]
int32 CreateUser3dMarker(float x, float y, float z, int32 colour) {
    return C3dMarkers::User3dMarkerSet(x, y, z, (eHudColours)colour);
}

//! 2625 REMOVE_USER_3D_MARKER (case @0x47A50E): marker slot  -- C3dMarkers::User3dMarkerDelete(slot) [0x721090]
void RemoveUser3dMarker(int32 slot) {
    C3dMarkers::User3dMarkerDelete(slot);
}

//! 2630 SWITCH_OBJECT_BRAINS (case @0x47A5A0): brain id (byte), on  -- CTheScripts::ScriptsForBrains (0xA90CF0).SwitchAllObjectBrainsWithThisID(id, on != 0) [0x46A900]
void SwitchObjectBrains(int32 id, int32 on) {
    CTheScripts::ScriptsForBrains.SwitchAllObjectBrainsWithThisID((int8)(uint8)id, on != 0);
}

//! 2632 ALLOW_PAUSE_IN_WIDESCREEN (case @0x47A5F5): flag  -- FrontEndMenuManager.m_bMenuAccessWidescreen (0xBA677C) = (flag != 0)
void AllowPauseInWidescreen(int32 flag) {
    FrontEndMenuManager.m_bMenuAccessWidescreen = flag != 0;
}
}; // namespace

void notsa::script::commands::ported::g25_26::RegisterG26() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g26");

    REGISTER_COMMAND_HANDLER(COMMAND_IS_THIS_HELP_MESSAGE_BEING_DISPLAYED, IsThisHelpMessageBeingDisplayed);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_FOLLOW_PATH_NODES_TO_COORD_WITH_RADIUS, TaskFollowPathNodesToCoordWithRadius);
    REGISTER_COMMAND_HANDLER(COMMAND_FIX_CAR, FixCar);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_PLAYER_GROUP_TO_FOLLOW_NEVER, SetPlayerGroupToFollowNever);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_UP_SKIP_FOR_VEHICLE_FINISHED_BY_SCRIPT, SetUpSkipForVehicleFinishedByScript);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_SKIP_WAITING_FOR_SCRIPT_TO_FADE_IN, IsSkipWaitingForScriptToFadeIn);
    REGISTER_COMMAND_HANDLER(COMMAND_FORCE_ALL_VEHICLE_LIGHTS_OFF, ForceAllVehicleLightsOff);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_LAST_BUILDING_MODEL_SHOT_BY_PLAYER, IsLastBuildingModelShotByPlayer);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_LAST_BUILDING_MODEL_SHOT_BY_PLAYER, ClearLastBuildingModelShotByPlayer);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_RANDOM_CHAR_IN_AREA_OFFSET_NO_SAVE, GetRandomCharInAreaOffsetNoSave);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_SCRIPT_COOP_GAME, SetScriptCoopGame);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_USER_3D_MARKER, CreateUser3dMarker);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_USER_3D_MARKER, RemoveUser3dMarker);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_OBJECT_BRAINS, SwitchObjectBrains);
    REGISTER_COMMAND_HANDLER(COMMAND_ALLOW_PAUSE_IN_WIDESCREEN, AllowPauseInWidescreen);
}
