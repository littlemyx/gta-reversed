#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group20_21.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

#include "World.h"
#include "Radar.h"
#include "Birds.h"
#include "GangWars.h"
#include "Shopping.h"
#include "Clock.h"
#include "CarCtrl.h"
#include "PedGroups.h"
#include "PlayerInfo.h"
#include "PedClothesDesc.h"
#include "UserDisplay.h"
#include "VehicleRecording.h"
#include "Interior/InteriorManager_c.h"
#include "Core/KeyGen.h"
#include "Entity/Object/Object.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Models/ModelInfo.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g09_12;
using namespace notsa::script::commands::ported::g20_21;

/*!
* Script commands ported from the exe's group processor for ids 2100..2199 (S6-F, slice D: ids 2150..2199) for the
* vanilla commands that had no handler of their own. Written from the asm of each `case`, NOT from the decompilation.
*/

namespace {
static_assert(offsetof(CEntity, m_nFlags) == 0x1C);
static_assert(offsetof(CPhysical, m_nPhysicalFlags) == 0x40);
static_assert(offsetof(CPhysical, m_fAirResistance) == 0x98);
static_assert(offsetof(CObject, m_nObjectFlags) == 0x140);
static_assert(offsetof(CVehicle, m_pHandlingData) == 0x384);
static_assert(offsetof(CVehicle, m_fDirtLevel) == 0x4B0);
static_assert(offsetof(tHandlingData, m_fDragMult) == 0x10);
static_assert(offsetof(CPed, m_fTurretAngleA) == 0x778);
static_assert(offsetof(CPed, m_fTurretAngleB) == 0x77C);
static_assert(offsetof(CPlayerInfo, m_PlayerData) + offsetof(CPlayerPedData, m_pPedClothesDesc) == 0x8);
static_assert(sizeof(CPlayerInfo) == 0x190);
static_assert(sizeof(CPedGroup) == 0x2D4);
static_assert(std::bit_cast<uint32>(0.017453292f) == 0x3C8EFA35); // 0x8595EC
static_assert(BLIP_DISPLAY_BOTH == 3 && BLIP_CHAR == 2);

//! `entity->GetPosition()` the way the exe reads it: matrix pos (+0x30) when there is a matrix, else the placement (+4)
CVector EntityPos(const CEntity& e) {
    return e.m_matrix ? e.m_matrix->GetPosition() : e.m_placement.m_vPosn;
}

//! The inlined distance of the closest-object searches: every difference is stored as float, then CVector::Magnitude (0x4082C0)
float DistanceTo(const CEntity& e, float x, float y, float z) {
    const CVector p = EntityPos(e);
    const CVector d{ p.x - x, p.y - y, p.z - z };
    return d.Magnitude();
}

//! 2150 GET_CLOSEST_STEALABLE_OBJECT (case @0x4715B8): x, y, z, radius => object handle (-1 if none)
//! CWorld::FindObjectsInRange(pos, radius, 2D, ..., max 16, objects only) [0x564A20], then the closest object (type 4, objectFlags 0x2000
//! = bIsLiftable) whose distance is < 2 * radius (strict, NaN fails) wins. No compare flag.
int32 GetClosestStealableObject(CVector pos, float radius) {
    CEntity* found[16];
    int16    numFound{};
    CWorld::FindObjectsInRange(pos, radius, true, &numFound, 16, found, false, false, false, true, false); // 0x564A20
    CObject* closest{};
    float    best = radius + radius; // fadd st0, st0
    for (uint32 i = 0; i < (uint32)(int32)numFound; i++) {
        auto* const e = found[i];
        if (e->GetType() != ENTITY_TYPE_OBJECT) { // (+0x36 & 7) == 4
            continue;
        }
        auto* const obj = static_cast<CObject*>(e);
        if (!(obj->m_nObjectFlags & 0x2000)) {
            continue;
        }
        const float dist = DistanceTo(*e, pos.x, pos.y, pos.z);
        if (dist < best) { // fcom + `test ah, 5; jp`
            best    = dist;
            closest = obj;
        }
    }
    return closest ? GetObjectPool()->GetRef(closest) : -1; // 0x465070
}

//! 2151 IS_PROCEDURAL_INTERIOR_ACTIVE (case @0x47173A): group => compare flag  (InteriorManager_c::IsGroupActive 0x598280)
bool IsProceduralInteriorActive(int32 group) {
    return g_interiorMan.IsGroupActive(group) != 0;
}

//! 2161 SWITCH_START / 2162 SWITCH_CONTINUED (shared case @0x47176C)
//! START: CollectParameters(4): value, numEntries (stored as 2 * numEntries, 16 bit), hasDefault (!= 0), defaultLabel; then CollectParameters(14) = 7 (value, label) pairs.
//! CONTINUED: CollectParameters(18) = 9 pairs. When fewer values than a full batch are left, only those are added (and the counter is zeroed).
//! When all entries were read, the jump table is searched and the IP is set to the label (UseSwitchJumpTable 0x4703C0, UpdatePC 0x464DA0).
OpcodeResult SwitchImpl(CRunningScript& S, bool isStart) {
    uint8 numToCollect;
    if (isStart) {
        S.CollectParameters(4);
        CTheScripts::ValueToCheckInSwitchStatement       = ScriptParams[0].iParam;
        CTheScripts::NumberOfEntriesStillToReadForSwitch = (uint16)(ScriptParams[1].iParam * 2);
        CTheScripts::SwitchDefaultExists                 = ScriptParams[2].iParam != 0;
        CTheScripts::SwitchDefaultAddress                = ScriptParams[3].iParam;
        numToCollect                                     = 14;
    } else {
        numToCollect = 18;
    }
    const uint16 batch = numToCollect;
    S.CollectParameters(batch);

    const auto JumpToSwitchLabel = [&] {
        int32 label{};
        CTheScripts::UseSwitchJumpTable(label);
        S.UpdatePC(label);
    };

    auto& stillToRead = CTheScripts::NumberOfEntriesStillToReadForSwitch;
    if (stillToRead > batch) { // `jbe` (unsigned)
        for (uint32 i = 0; i < numToCollect; i += 2) {
            CTheScripts::AddToSwitchJumpTable(ScriptParams[i].iParam, ScriptParams[i + 1].iParam); // 0x470390
        }
        stillToRead = (uint16)(stillToRead - batch);
        if (stillToRead != 0) {
            return OR_CONTINUE;
        }
    } else {
        const uint32 n = stillToRead;
        for (uint32 i = 0; i < n; i += 2) {
            CTheScripts::AddToSwitchJumpTable(ScriptParams[i].iParam, ScriptParams[i + 1].iParam);
        }
        stillToRead = 0;
    }
    JumpToSwitchLabel();
    return OR_CONTINUE;
}
OpcodeResult SwitchStart(CRunningScript& S) { return SwitchImpl(S, true); }
OpcodeResult SwitchContinued(CRunningScript& S) { return SwitchImpl(S, false); }

//! 2163 REMOVE_CAR_RECORDING (case @0x47188A): file number  (CVehicleRecording::RemoveRecordingFile 0x45A0A0)
void RemoveCarRecording(int32 fileNumber) {
    CVehicleRecording::RemoveRecordingFile(fileNumber);
}

//! 2165 SET_OBJECT_ONLY_DAMAGED_BY_PLAYER (case @0x471916): object, flag  -- physical flags (+0x40) bit 0x400000 (bInvulnerable); the handle isn't checked
void SetObjectOnlyDamagedByPlayer(CObject& obj, int32 flag) {
    obj.physicalFlags.bInvulnerable = flag != 0;
}

//! 2166 CREATE_BIRDS (case @0x47195E): 8 ints: x1, y1, z1, x2, y2, z2, count, biome  (ints are converted to float with fild)
//! CBirds::CreateNumberOfBirds(start, target, count, biome, false) [0x711EF0]
void CreateBirds(int32 x1, int32 y1, int32 z1, int32 x2, int32 y2, int32 z2, int32 count, int32 biome) {
    CBirds::CreateNumberOfBirds(
        CVector{ (float)x1, (float)y1, (float)z1 },
        CVector{ (float)x2, (float)y2, (float)z2 },
        count,
        (eBirdsBiome)biome,
        false
    );
}

//! 2168 SET_VEHICLE_DIRT_LEVEL (case @0x471A00): vehicle, level  -- raw dword to +0x4B0 (m_fDirtLevel), the handle isn't checked
void SetVehicleDirtLevel(CVehicle& veh, float level) {
    veh.m_fDirtLevel = level;
}

//! 2169 SET_GANG_WARS_ACTIVE (case @0x471A2D): active  (CGangWars::SetGangWarsActive 0x446570)
void SetGangWarsActive(int32 active) {
    CGangWars::SetGangWarsActive(active != 0);
}

//! 2170 IS_GANG_WAR_GOING_ON (case @0x471A61): => compare flag  (CGangWars::GangWarGoingOn 0x443AA0)
bool IsGangWarGoingOn() {
    return CGangWars::GangWarGoingOn();
}

//! 2171 GIVE_PLAYER_CLOTHES_OUTSIDE_SHOP (case @0x471A70): player, texture (16 chars), model (16 chars), part
//! CWorld::Players[player] is NOT range checked; players[player].m_PlayerData.m_pPedClothesDesc (+8)->SetTextureAndModel(texture, model, part) [0x5A8080],
//! then CShopping::SetPlayerHasBought(CKeyGen::GetUppercaseKey(texture)) [0x49B610]   (the key is of the TEXTURE label)
//! NOTSA: the label buffers are zero initialised with one extra NUL (the exe's are uninitialised and read 16 chars without terminator)
void GivePlayerClothesOutsideShop(CRunningScript& S) {
    const auto playerIdx = Read<int32>(&S);
    auto&      player    = (&CWorld::Players[0])[playerIdx];
    char       texture[17]{};
    char       model[17]{};
    S.ReadTextLabelFromScript(texture, 16);
    S.ReadTextLabelFromScript(model, 16);
    const auto part = Read<int32>(&S);
    player.m_PlayerData.m_pPedClothesDesc->SetTextureAndModel(texture, model, (eClothesTexturePart)part);
    CShopping::SetPlayerHasBought(CKeyGen::GetUppercaseKey(texture));
}

//! 2172 CLEAR_LOADED_SHOP (case @0x471AF2): no params  (CShopping::RemoveLoadedShop 0x49AE30)
void ClearLoadedShop() {
    CShopping::RemoveLoadedShop();
}

//! 2173 SET_GROUP_SEQUENCE (case @0x471AFE): group, sequence
//! group = GetActualScriptThingIndex(h, 8 = ped group); if 0 <= group < 8: CPedGroups::ms_groups[group] + 0x2CC (= m_groupIntelligence.m_TaskSeqId, private)
//! = (sequence == -1) ? -1 : GetActualScriptThingIndex(sequence, 4 = sequence task)
void SetGroupSequence(int32 groupHandle, int32 sequenceHandle) {
    const auto group = CTheScripts::GetActualScriptThingIndex(groupHandle, SCRIPT_THING_PED_GROUP); // 0x4839A0
    if (group < 0 || group >= 8) {
        return;
    }
    int32 seq = -1;
    if (sequenceHandle != -1) {
        seq = CTheScripts::GetActualScriptThingIndex(sequenceHandle, SCRIPT_THING_SEQUENCE_TASK);
    }
    auto* const taskSeqId = reinterpret_cast<int32*>(reinterpret_cast<uint8*>(&CPedGroups::ms_groups[group]) + 0x2CC);
    *taskSeqId = seq;
}

//! 2180 REGISTER_ATTRACTOR_SCRIPT_BRAIN_FOR_CODE_USE (case @0x471C8F): streamed script index, brain name (8 chars)
//! idx = CStreamedScripts::GetProperIndexFromIndexUsedByScript((uint16)index) [0x470810], sign extended 16 bit; then
//! CScriptsForBrains::AddNewStreamedScriptBrainForCodeUse((uint16)idx, name, 5) [0x46A9C0]
//! NOTSA: the name buffer gets a terminating NUL (the exe's 8 byte buffer is passed to strcpy unterminated)
void RegisterAttractorScriptBrainForCodeUse(CRunningScript& S) {
    const auto  scriptIndex = Read<int32>(&S);
    const int32 properIdx   = CTheScripts::StreamedScripts.GetProperIndexFromIndexUsedByScript((int16)(uint16)scriptIndex); // movsx
    char        name[9]{};
    S.ReadTextLabelFromScript(name, 8);
    CTheScripts::ScriptsForBrains.AddNewStreamedScriptBrainForCodeUse((int16)(uint16)properIdx, name, 5);
}

//! 2183 SET_HEADING_LIMIT_FOR_ATTACHED_CHAR (case @0x471CE9): ped, direction (16 bit), limit (degrees)
//! ped+0x778 (uint16) = direction; ped+0x77C = limit * 0.017453292f (0x8595EC); the handle isn't checked
void SetHeadingLimitForAttachedChar(CPed& ped, int32 direction, float limit) {
    ped.m_fTurretAngleA = (uint16)direction;
    ped.m_fTurretAngleB = limit * 0.017453292f;
}

//! 2184 ADD_BLIP_FOR_DEAD_CHAR (case @0x471D2B): char, <blip var> => 1 blip  (same as ADD_BLIP_FOR_CHAR: SetEntityBlip(BLIP_CHAR, h, 1, both) [0x5839A0], scale 3 [0x583CC0])
//! The exe also peeks the output variable (GetActualBlipArrayIndex(CollectNextParameterWithoutIncreasingPC()), pure).
int32 AddBlipForDeadChar(CRunningScript& S, int32 charHandle) {
    CRadar::GetActualBlipArrayIndex(S.CollectNextParameterWithoutIncreasingPC());
    const auto blip = CRadar::SetEntityBlip(BLIP_CHAR, charHandle, 1, BLIP_DISPLAY_BOTH);
    CRadar::ChangeBlipScale(blip, 3);
    return blip;
}

//! 2186 TASK_PLAY_ANIM_WITH_FLAGS (case @0x471E0C): no CollectParameters here, CRunningScript::PlayAnimScriptCommand(command) [0x470150] reads everything
void TaskPlayAnimWithFlags(eScriptCommands command, CRunningScript& S) {
    S.PlayAnimScriptCommand(command);
}

//! 2187 SET_VEHICLE_AIR_RESISTANCE_MULTIPLIER (case @0x471E1B): vehicle, multiplier
//! No handling data => nothing. drag = handling->m_fDragMult (+0x10); if drag > 0.01 (0x858C58): veh->m_fAirResistance (+0x98) = drag / 1000 (0x859E80) * 0.5 (0x858B8C)
//! (stored as float), else = drag (raw); then veh->m_fAirResistance = multiplier * veh->m_fAirResistance. The vehicle handle isn't checked.
void SetVehicleAirResistanceMultiplier(CVehicle& veh, float mult) {
    const auto* const h = veh.m_pHandlingData;
    if (!h) {
        return;
    }
    if (h->m_fDragMult > 0.01f) {
        veh.m_fAirResistance = (float)((double)h->m_fDragMult / (double)1000.0f * (double)0.5f);
    } else {
        veh.m_fAirResistance = h->m_fDragMult;
    }
    veh.m_fAirResistance = (float)((double)mult * (double)veh.m_fAirResistance);
}

//! 2188 SET_CAR_COORDINATES_NO_OFFSET (case @0x471EA5): car, x, y, z  => CCarCtrl::SetCoordsOfScriptCar(car, x, y, z, 0, 0) [0x4342A0]; the handle isn't checked
void SetCarCoordinatesNoOffset(CVehicle& veh, CVector pos) {
    CCarCtrl::SetCoordsOfScriptCar(&veh, pos.x, pos.y, pos.z, 0, 0);
}

//! 2189 SET_USES_COLLISION_OF_CLOSEST_OBJECT_OF_TYPE (case @0x471EE8): x, y, z, radius, model, flag
//! z <= -100 => z = CWorld::FindGroundZForCoord(x, y). CWorld::FindObjectsOfTypeInRange(model, pos, radius, 2D, ..., max 16, buildings+objects+dummies) [0x564C70];
//! the closest (distance < 2 * radius, strict) of ANY of the found entities gets m_bUsesCollision (flags +0x1C bit 1) = flag != 0. No compare flag.
void SetUsesCollisionOfClosestObjectOfType(CVector pos, float radius, Model model, int32 flag) {
    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);
    CEntity* found[16];
    int16    numFound{};
    CWorld::FindObjectsOfTypeInRange((uint32)model.value, pos, radius, true, &numFound, 16, found, true, false, false, true, true); // 0x564C70
    CEntity* closest{};
    float    best = radius + radius;
    for (int32 i = 0; i < (int32)numFound; i++) { // `test ebx, ebx; jle` + 16 bit counter < count
        const float dist = DistanceTo(*found[i], pos.x, pos.y, pos.z);
        if (dist < best) {
            best    = dist;
            closest = found[i];
        }
    }
    if (closest) {
        closest->m_bUsesCollision = flag != 0;
    }
}

//! 2190 SET_TIME_ONE_DAY_FORWARD (case @0x47207D): no params  (CClock::OffsetClockByADay(1) 0x52D0B0)
void SetTimeOneDayForward() {
    CClock::OffsetClockByADay(1);
}

//! 2192 SET_TIMER_BEEP_COUNTDOWN_TIME (case @0x47208E): <global var>, seconds
//! The variable is read first (GetIndexOfGlobalVariable), then CollectParameters(1); COnscreenTimer::SetClockBeepCountdownSecs(var, secs) [0x44CEE0]
void SetTimerBeepCountdownTime(CRunningScript& S) {
    const uint16 varId = S.GetIndexOfGlobalVariable();
    const auto   secs  = Read<int32>(&S);
    CUserDisplay::OnscnTimer.SetClockBeepCountdownSecs(varId, secs);
}

//! 2195 ATTACH_TRAILER_TO_CAB (case @0x4720BB): trailer, cab  -- if both exist: trailer->SetTowLink(cab, true)  (vtable slot 0xF4 = 61)
void AttachTrailerToCab(CVehicle* trailer, CVehicle* cab) {
    if (trailer && cab) {
        trailer->SetTowLink(cab, true);
    }
}

//! 2199 IS_VEHICLE_TOUCHING_OBJECT (case @0x472100): vehicle, object => compare flag
//! false when the vehicle doesn't exist; otherwise vehicle->GetHasCollidedWith(object) [0x543540] (the object isn't checked for null)
bool IsVehicleTouchingObject(CVehicle* veh, CObject* obj) {
    return veh && veh->GetHasCollidedWith(obj);
}
}; // namespace

void notsa::script::commands::ported::g20_21::RegisterG21b() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g21b");

    REGISTER_COMMAND_HANDLER(COMMAND_GET_CLOSEST_STEALABLE_OBJECT, GetClosestStealableObject);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_PROCEDURAL_INTERIOR_ACTIVE, IsProceduralInteriorActive);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_START, SwitchStart);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_CONTINUED, SwitchContinued);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_CAR_RECORDING, RemoveCarRecording);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_ONLY_DAMAGED_BY_PLAYER, SetObjectOnlyDamagedByPlayer);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_BIRDS, CreateBirds);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_VEHICLE_DIRT_LEVEL, SetVehicleDirtLevel);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GANG_WARS_ACTIVE, SetGangWarsActive);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_GANG_WAR_GOING_ON, IsGangWarGoingOn);
    REGISTER_COMMAND_HANDLER(COMMAND_GIVE_PLAYER_CLOTHES_OUTSIDE_SHOP, GivePlayerClothesOutsideShop);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_LOADED_SHOP, ClearLoadedShop);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GROUP_SEQUENCE, SetGroupSequence);
    REGISTER_COMMAND_HANDLER(COMMAND_REGISTER_ATTRACTOR_SCRIPT_BRAIN_FOR_CODE_USE, RegisterAttractorScriptBrainForCodeUse);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_HEADING_LIMIT_FOR_ATTACHED_CHAR, SetHeadingLimitForAttachedChar);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_BLIP_FOR_DEAD_CHAR, AddBlipForDeadChar);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_PLAY_ANIM_WITH_FLAGS, TaskPlayAnimWithFlags);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_VEHICLE_AIR_RESISTANCE_MULTIPLIER, SetVehicleAirResistanceMultiplier);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_COORDINATES_NO_OFFSET, SetCarCoordinatesNoOffset);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_USES_COLLISION_OF_CLOSEST_OBJECT_OF_TYPE, SetUsesCollisionOfClosestObjectOfType);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_TIME_ONE_DAY_FORWARD, SetTimeOneDayForward);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_TIMER_BEEP_COUNTDOWN_TIME, SetTimerBeepCountdownTime);
    REGISTER_COMMAND_HANDLER(COMMAND_ATTACH_TRAILER_TO_CAB, AttachTrailerToCab);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_VEHICLE_TOUCHING_OBJECT, IsVehicleTouchingObject);
}
