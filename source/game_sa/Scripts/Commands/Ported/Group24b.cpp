#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group24.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>

#include "Pad.h"
#include "World.h"
#include "GameLogic.h"
#include "Darkel.h"
#include "Hud.h"
#include "Population.h"
#include "PostEffects.h"
#include "Stats.h"
#include "Checkpoint.h"
#include "Checkpoints.h"
#include "EntryExitManager.h"
#include "MissionCleanup.h"
#include "FireManager.h"
#include "PathFind.h"
#include "VehicleRecording.h"
#include "LoadedCarGroup.h"
#include "Audio/AudioEngine.h"
#include "Core/KeyGen.h"
#include "Fx/Fx.h"
#include "Streaming.h"
#include "VisibilityPlugins.h"
#include "Models/ModelInfo.h"
#include "Models/ClumpModelInfo.h"
#include "Models/VehicleModelInfo.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Object/Object.h"
#include "DamageManager.h"
#include "Pools/Pools.h"
#include "Scripts/RunningScript.h"
#include "Tasks/TaskTypes/TaskSimpleSetCharIgnoreWeaponRangeFlag.h"
#include "Tasks/TaskTypes/TaskSimpleHoldEntity.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g24;
using namespace notsa::script::commands::ported::g09_12;

/*!
* Script commands ported from the exe's group processor for ids 2400..2499 (`CRunningScript::ProcessCommands2400To2499` @0x478000, switch base 2400,
* jump table @0x479AFC) for the vanilla commands that had no handler of their own (S6-I, g24b: 2449..2499). See Group24a.cpp.
*/

// Raw offsets the exe uses directly in these cases
static_assert(offsetof(CPad, bDisablePlayerCycleWeapon) == 0x11D);
static_assert(offsetof(CPed, m_pVehicle) == 0x58C && offsetof(CVehicle, m_nVehicleType) == 0x590);
static_assert(offsetof(CPed, m_nBodypartToRemove) == 0x754);
static_assert(offsetof(CPhysical, m_fContactSurfaceBrightness) == 0x12C);
static_assert(offsetof(CVehicleModelInfo, m_nVehicleClass) == 0x4D);
static_assert(offsetof(CVehicle, m_nDoorLock) == 0x4F8 && offsetof(CAutomobile, m_damageManager) == 0x5A0);
static_assert(offsetof(CRunningScript, m_UsesMissionCleanup) == 0xC6 && offsetof(CRunningScript, m_IsTextBlockOverride) == 0xC8);
static_assert(offsetof(CCheckpoint, m_ID) == 4 && sizeof(tScriptCheckpoint) == 8 && offsetof(tScriptCheckpoint, m_Checkpoint) == 4);
static_assert(STAT_RESPECT_MISSION_TOTAL == 228 && STAT_RESPECT_MISSION == 224);
static_assert(sizeof(eCrossHairType) == 4);
static_assert(TASK_SIMPLE_HOLD_ENTITY == 0x133 && ABORT_PRIORITY_URGENT == 1);
static_assert(VEHICLE_APPEARANCE_AUTOMOBILE == 1 && VEHICLE_APPEARANCE_BIKE == 2);

namespace {
//! 0x4082C0 (`CVector::Magnitude`) is not needed here; the angled area test below inlines the extended precision normalisations

//! 0x477DA0 (unreversed): is `p` (z ignored) inside the angled rectangle (x1, y1) - (x2, y2) with the given width? The 2nd edge is perpendicular to the first one.
//! corners: angle = GetRadianAngleBetweenPoints(x1, y1, x2, y2) + pi/2 [0x858FE4] folded into [0, 2pi] [0x858CBC] (x87 compares); C = (x2 + sin * w, y2 - cos * w),
//! D = (x1 + sin * w, y1 - cos * w) (the cos * w product is rounded to float first). AB = B - A, AD = D - A; |AB|, |AD| are stored as floats; AB is normalised
//! (inverse length in extended precision; a non-positive squared length leaves AB.x = 1.0f and AB.y as it is), dotAB = rel . AB must be in [0, |AB|]; then AD is normalised
//! the same way (0x44E480: in place, float stores; non-positive squared length => x = 1.0f only) and dotAD in [0, |AD|] (NaN => false).
//! (The debug highlight call 0x486990 is only done if the byte 0x859CF8 is set; it is 0.)
bool IsPointInAngledAreaOriginal(const CVector& p, float x1, float y1, float x2, float y2, float width) {
    constexpr double PI_HALF = (double)1.5707963705062866f; // 0x858FE4
    constexpr double TWO_PI  = (double)6.2831854820251465f; // 0x858CBC
    double angle = CGeneral::GetRadianAngleBetweenPointsExt(x1, y1, x2, y2) + PI_HALF; // 0x53CBE0, keeps the extended result
    while (angle < 0.0) { // fcom 0.0; test ah, 5; jp (exits the loop if not less)
        angle += TWO_PI;
    }
    while (angle > TWO_PI) { // fcom 2pi; test ah, 0x41; jne
        angle -= TWO_PI;
    }
    double sn, cs;
    {
        // fsin / fcos of the extended angle
        const double a = angle;
        double       s, c;
        __asm {
            fld   qword ptr [a]
            fld   st(0)
            fsin
            fstp  qword ptr [s]
            fcos
            fstp  qword ptr [c]
        }
        sn = s;
        cs = c;
    }
    const float cosW = (float)(-(cs * (double)width)); // fchs; fst dword
    const float dx   = (float)(sn * (double)width + (double)x1);
    const float dy   = (float)((double)cosW + (double)y1);

    // AB
    const float  abx = x2 - x1;
    const double aby = (double)y2 - (double)y1; // stays in the FPU
    // AD
    const float adx = dx - x1;
    const float ady = dy - y1;
    const double abSq = aby * aby + (double)abx * abx;
    const float  abLen = (float)std::sqrt(abSq);
    const double adSq = (double)ady * ady + (double)adx * adx;
    const float  adLen = (float)std::sqrt(adSq);
    const float  relx = p.x - x1;
    const float  rely = p.y - y1;

    double nx, ny;
    if (abSq > 0.0 || std::isnan(abSq)) { // fcom 0.0; test ah, 0x41; jp -> normalise
        const double inv  = 1.0 / std::sqrt(abSq);
        const float  invF = (float)inv; // fst dword [temp]
        nx = inv * (double)abx;
        ny = aby * (double)invF;
    } else {
        nx = 1.0;
        ny = aby;
    }
    const double dotAB = ny * (double)rely + (double)relx * nx;
    if (dotAB < 0.0 || std::isnan(dotAB)) { // fcom 0.0; test ah, 1; jne
        return false;
    }
    if (!(dotAB <= (double)abLen)) { // fcomp; test ah, 0x41; jp
        return false;
    }

    // 0x44E480: CVector2D::Normalise (in place)
    float adnx = adx, adny = ady;
    {
        const double sq = (double)adny * adny + (double)adnx * adnx; // fld [ecx+4]; fld [ecx]; ...
        if (sq > 0.0 || std::isnan(sq)) {
            const double inv = 1.0 / std::sqrt(sq);
            adnx = (float)(inv * (double)adnx);
            adny = (float)(inv * (double)ady);
        } else {
            adnx = 1.0f;
        }
    }
    const double dotAD = (double)rely * adny + (double)relx * adnx;
    if (dotAD < 0.0 || std::isnan(dotAD)) {
        return false;
    }
    return dotAD <= (double)adLen;
}

//! 2449 PAUSE_CURRENT_BEAT_TRACK (case @0x478DA5): pause (low byte)  -- AudioEngine.PauseBeatTrack(pause) [0x507200]
void PauseCurrentBeatTrack(int32 pause) {
    AudioEngine.PauseBeatTrack((uint8)pause != 0);
}

//! 2450 SET_PLAYER_CYCLE_WEAPON_BUTTON (case @0x478DC7): pad, enable  -- CPad::GetPad(pad)->bDisablePlayerCycleWeapon (+0x11D) = (enable == 0)
void SetPlayerCycleWeaponButton(int32 padId, int32 enable) {
    CPad::GetPad(padId)->bDisablePlayerCycleWeapon = (enable == 0); // 0x53FB70
}

//! 2452 MARK_ROAD_NODE_AS_DONT_WANDER (case @0x478E13): x, y, z  -- ThePaths.MarkRoadNodeAsDontWander(x, y, z) [0x450560]
void MarkRoadNodeAsDontWander(float x, float y, float z) {
    ThePaths.MarkRoadNodeAsDontWander(x, y, z);
}

//! 2453 UNMARK_ALL_ROAD_NODES_AS_DONT_WANDER (case @0x478E41): ThePaths.UnMarkAllRoadNodesAsDontWander() [0x44D400]
void UnMarkAllRoadNodesAsDontWander() {
    ThePaths.UnMarkAllRoadNodesAsDontWander();
}

//! 2454 SET_CHECKPOINT_HEADING (case @0x478E52): checkpoint, heading  -- idx = GetActualScriptThingIndex(handle, SCRIPT_THING_CHECKPOINT); if valid and the slot's checkpoint (+4) is set:
//! CCheckpoints::SetHeading(checkpoint->m_ID, heading) [0x722970]
void SetCheckpointHeading(int32 handle, float heading) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(handle, SCRIPT_THING_CHECKPOINT); // 0x4839A0
    if (idx < 0) {
        return;
    }
    const auto* const cp = CTheScripts::ScriptCheckpointArray[idx].m_Checkpoint; // 0xA44074 + idx * 8
    if (!cp) {
        return;
    }
    CCheckpoints::SetHeading(cp->m_ID, heading);
}

//! 2455 SET_MISSION_RESPECT_TOTAL (case @0x478EA4): value  -- CStats::SetStatValue(STAT_RESPECT_MISSION_TOTAL (0xE4), (float)value) [0x55A070]
void SetMissionRespectTotal(int32 value) {
    CStats::SetStatValue(STAT_RESPECT_MISSION_TOTAL, (float)value);
}

//! 2456 AWARD_PLAYER_MISSION_RESPECT (case @0x478ECB): value  -- CStats::IncrementStat(STAT_RESPECT_MISSION (0xE0), (float)value) [0x55C180]
void AwardPlayerMissionRespect(int32 value) {
    CStats::IncrementStat(STAT_RESPECT_MISSION, (float)value);
}

//! 2458 SET_CAR_COLLISION (case @0x478EF2): car, flag  -- m_bUsesCollision (+0x1C bit 0) and physicalFlags.bApplyGravity (+0x40 bit 1) = (flag != 0)
void SetCarCollision(CVehicle& veh, int32 flag) {
    if (flag != 0) {
        veh.m_nFlags |= 1;
        veh.m_nPhysicalFlags |= 2;
    } else {
        veh.m_nFlags &= ~1u;
        veh.m_nPhysicalFlags &= ~2u;
    }
}

//! 2459 CHANGE_PLAYBACK_TO_USE_AI (case @0x478F43): car  -- CVehicleRecording::ChangeCarPlaybackToUseAI(car) [0x45A360]
void ChangePlaybackToUseAI(CVehicle& veh) {
    CVehicleRecording::ChangeCarPlaybackToUseAI(&veh);
}

//! 2461 IS_NIGHT_VISION_ACTIVE (case @0x478F9C): => compare flag: CPostEffects::m_bNightVision (0xC402B8)
bool IsNightVisionActive() {
    return CPostEffects::m_bNightVision;
}

//! 2462 SET_CREATE_RANDOM_COPS (case @0x478FB2): flag  -- CPopulation::m_bDontCreateRandomCops (0xC0FCB4) = (flag == 0)
void SetCreateRandomCops(int32 flag) {
    CPopulation::m_bDontCreateRandomCops = flag == 0;
}

//! 2463 TASK_SET_IGNORE_WEAPON_RANGE_FLAG (case @0x478FD2): ped, flag (byte)  -- handle != -1: the ped's bIgnoreWeaponRange (+0x478 bit 9) = bit 0 of the flag byte, immediately;
//! handle == -1: GivePedScriptedTask(-1, new CTaskSimpleSetCharIgnoreWeaponRangeFlag(flag byte) [0x474620], command) [0x465C20] (sequence task building)
void TaskSetIgnoreWeaponRangeFlag(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, int32 flag) {
    if (pedHandle != -1) {
        GetPedPool()->GetAtRef(pedHandle)->bIgnoreWeaponRange = ((uint8)flag & 1) != 0; // 0x404910
        return;
    }
    // NOTSA: the exe passes the zero extended byte, the constructor takes a bool (only differs for byte values with bit 0 clear but non zero)
    auto* const task = new CTaskSimpleSetCharIgnoreWeaponRangeFlag((uint8)flag != 0);
    S.GivePedScriptedTask(-1, task, (int32)cmd);
}

//! 2464 TASK_PICK_UP_SECOND_OBJECT (case @0x47905C): CRunningScript::ScriptTaskPickUpObject(command) [0x46AF50] (no CollectParameters before)
void TaskPickUpSecondObject(CRunningScript& S, eScriptCommands cmd) {
    S.ScriptTaskPickUpObject((int32)cmd);
}

//! 2465 DROP_SECOND_OBJECT (case @0x47906B): ped, flag  -- if the ped's secondary task in slot 0 (TASK_SECONDARY_ATTACK) is a TASK_SIMPLE_HOLD_ENTITY (0x133, virtual +0x10):
//! task->DropEntity(ped, flag != 0) [0x6930F0] then task->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr) (virtual +0x18)
void DropSecondObject(CPed& ped, int32 flag) {
    auto& taskMgr = ped.GetIntelligence()->m_TaskMgr;
    auto* const t0 = taskMgr.GetTaskSecondary(TASK_SECONDARY_ATTACK); // 0x681810
    if (!t0) {
        return;
    }
    if (taskMgr.GetTaskSecondary(TASK_SECONDARY_ATTACK)->GetTaskType() != TASK_SIMPLE_HOLD_ENTITY) {
        return;
    }
    auto* const task = static_cast<CTaskSimpleHoldEntity*>(taskMgr.GetTaskSecondary(TASK_SECONDARY_ATTACK));
    task->DropEntity(&ped, flag != 0);
    task->MakeAbortable(&ped, ABORT_PRIORITY_URGENT, nullptr);
}

//! 2466 REMOVE_OBJECT_ELEGANTLY (case @0x479107): object  -- CTheScripts::CleanUpThisObject(obj) [0x4866C0] (null allowed); if the object exists: all atomics of its clump get
//! CClumpModelInfo::SetAtomicRendererCB(atomic, CVisibilityPlugins::RenderFadingClumpCB) [RpClumpForAllAtomics 0x749B70] and objectFlags |= 0x400000 (bFadingIn);
//! then (object or not) if the script uses mission cleanup: RemoveEntityFromList(handle, 3 (object))
void RemoveObjectElegantly(CRunningScript& S, int32 handle) {
    auto* const obj = GetObjectPool()->GetAtRef(handle); // 0x465040
    CTheScripts::CleanUpThisObject(obj);
    if (obj) {
        if (auto* const clump = obj->GetRpClump()) {
            RpClumpForAllAtomics(clump, CClumpModelInfo::SetAtomicRendererCB, (void*)CVisibilityPlugins::RenderFadingClumpCB);
        }
        obj->objectFlags.bFadingIn = true;
    }
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.RemoveEntityFromList(handle, MISSION_CLEANUP_ENTITY_TYPE_OBJECT); // 0x4654B0
    }
}

//! 2467 DRAW_CROSSHAIR (case @0x47917C): flag  -- CTheScripts::bDrawCrossHair (0xA44490, dword) = (flag != 0)
void DrawCrosshair(int32 flag) {
    CTheScripts::bDrawCrossHair = (eCrossHairType)(flag != 0 ? 1u : 0u);
}

//! 2470 SHOW_BLIPS_ON_ALL_LEVELS (case @0x479203): flag  -- CTheScripts::RadarShowBlipOnAllLevels (0xA444A2) = (flag != 0)
void ShowBlipsOnAllLevels(int32 flag) {
    CTheScripts::RadarShowBlipOnAllLevels = flag != 0;
}

//! 2471 SET_CHAR_DRUGGED_UP (case @0x479222): ped, flag  -- bDruggedUp (+0x478 bit 10) = (flag != 0)
void SetCharDruggedUp(CPed& ped, int32 flag) {
    ped.bDruggedUp = flag != 0;
}

//! 2472 IS_CHAR_HEAD_MISSING (case @0x47926D): ped => compare flag: the ped exists && bRemoveHead (+0x46D bit 7) && m_nBodypartToRemove (+0x754) == 2
bool IsCharHeadMissing(CPed* ped) {
    if (!ped) {
        return false;
    }
    return ped->bRemoveHead && ped->m_nBodypartToRemove == 2;
}

//! 2473 GET_HASH_KEY (case @0x4792AF): text label (15 chars, no CollectParameters) => CKeyGen::GetUppercaseKey(label) [0x53CF30]
uint32 GetHashKey(CRunningScript& S) {
    char label[16]{}; // NOTSA: zero initialised (the exe's buffer is not, and ReadTextLabelFromScript does not terminate)
    S.ReadTextLabelFromScript(label, 15); // 0x463D50
    return CKeyGen::GetUppercaseKey(label);
}

//! 2475 RANDOM_PASSENGER_SAY (case @0x47931F): car, context (low word)  -- passenger = PickRandomPassenger() [0x6D2A10]; if there is one: Say(context, 0, 1.0f, false, false, false) [0x5EFFE0]
void RandomPassengerSay(CVehicle& veh, int32 context) {
    if (auto* const passenger = veh.PickRandomPassenger()) {
        passenger->Say((eGlobalSpeechContext)(uint16)context, 0, 1.0f, false, false, false);
    }
}

//! 2476 HIDE_ALL_FRONTEND_BLIPS (case @0x47936D): flag  -- CTheScripts::HideAllFrontEndMapBlips (0xA444A1) = (flag != 0)
void HideAllFrontendBlips(int32 flag) {
    CTheScripts::HideAllFrontEndMapBlips = flag != 0;
}

//! 2478 IS_CHAR_IN_ANY_TRAIN (case @0x4793B0): ped => compare flag: bInVehicle (+0x46D bit 0) && ped->m_pVehicle->m_nVehicleType (+0x590) == 6 (train)
bool IsCharInAnyTrain(CPed& ped) {
    return ped.bInVehicle && ped.m_pVehicle->m_nVehicleType == VEHICLE_TYPE_TRAIN;
}

//! 2479 SET_UP_SKIP_AFTER_MISSION (case @0x479401): x, y, z, heading  -- CGameLogic::SetUpSkip(pos, heading, afterMission = true, vehicle = null, finishedByScript = false) [0x4423C0]
void SetUpSkipAfterMission(CVector pos, float heading) {
    CGameLogic::SetUpSkip(pos, heading, true, nullptr, false);
}

//! 2480 SET_VEHICLE_IS_CONSIDERED_BY_PLAYER (case @0x47945B): car, flag  -- bConsideredByPlayer (+0x42E bit 5) = (flag != 0)
void SetVehicleIsConsideredByPlayer(CVehicle& veh, int32 flag) {
    veh.vehicleFlags.bConsideredByPlayer = flag != 0;
}

//! 2482 GET_RANDOM_CAR_MODEL_IN_MEMORY (case @0x47949F): flag => model (-1 if none), vehicle class (model info +0x4D, movsx byte; -1 if none)
//! model = m_AppropriateLoadedCars.PickRandomCar(false, flag != 0) [0x611C50]
OpcodeResult GetRandomCarModelInMemory(CRunningScript& S, int32 flag) {
    const auto model = (int32)CPopulation::m_AppropriateLoadedCars.PickRandomCar(false, flag != 0);
    StoreArg(&S, model);
    if (model == -1) {
        StoreArg(&S, -1);
    } else {
        StoreArg(&S, (int32)(int8)(uint8)static_cast<CVehicleModelInfo*>(CModelInfo::ms_modelInfoPtrs[model])->m_nVehicleClass);
    }
    return OR_CONTINUE;
}

//! 2483 GET_CAR_DOOR_LOCK_STATUS (case @0x479500): car => m_nDoorLock (+0x4F8)
int32 GetCarDoorLockStatus(CVehicle& veh) {
    return (int32)veh.m_nDoorLock;
}

//! 2484 SET_CLOSEST_ENTRY_EXIT_FLAG (case @0x479536): x, y, radius, flag, enable  -- CEntryExitManager::SetEntryExitFlagWithIndex(FindNearestEntryExit((x, y), radius, -1) [0x43F4B0], flag, enable != 0) [0x43EF90]
void SetClosestEntryExitFlag(CVector2D pos, float radius, int32 flag, int32 enable) {
    const auto index = CEntryExitManager::FindNearestEntryExit(pos, radius, -1);
    CEntryExitManager::SetEntryExitFlagWithIndex(index, (uint32)flag, enable != 0);
}

//! 2488 ADD_BLOOD (case @0x47966F): x, y, z, dirx, diry, dirz, count, ped  -- g_fx.AddBlood(pos, dir, count, ped->m_fContactSurfaceBrightness (+0x12C)) [0x49EB00]. The ped is not null checked.
void AddBlood(CVector pos, CVector dir, int32 count, CPed& ped) {
    g_fx.AddBlood(pos, dir, count, ped.m_fContactSurfaceBrightness);
}

//! 2489 DISPLAY_CAR_NAMES (case @0x4796EC): flag  -- CHud::bScriptDontDisplayVehicleName (0xBAA3F9) = (flag == 0)
void DisplayCarNames(int32 flag) {
    CHud::bScriptDontDisplayVehicleName = flag == 0;
}

//! 2491 IS_CAR_DOOR_DAMAGED (case @0x47972B): car, door => compare flag: CDamageManager::GetDoorStatus(door) != 0 [0x6C2230]
bool IsCarDoorDamaged(CVehicle& veh, int32 door) {
    // The damage manager is at +0x5A0 (CAutomobile::m_damageManager); the vehicle is not checked to be an automobile
    return (int32)static_cast<CAutomobile&>(veh).m_damageManager.GetDoorStatus((eDoors)door) != 0;
}

//! 2493 SET_MINIGAME_IN_PROGRESS (case @0x4797B6): flag  -- flag != 0: script->+0xC8 = true, bMiniGameInProgress (0xA444A8) = true, bDisplayNonMiniGameHelpMessages (0xA444A7) = false; flag == 0: the opposite
void SetMinigameInProgress(CRunningScript& S, int32 flag) {
    const bool on = flag != 0;
    S.m_IsTextBlockOverride                       = on;
    CTheScripts::bMiniGameInProgress              = on;
    CTheScripts::bDisplayNonMiniGameHelpMessages  = !on;
}

//! 2494 IS_MINIGAME_IN_PROGRESS (case @0x479800): => compare flag: CTheScripts::bMiniGameInProgress (0xA444A8)
bool IsMinigameInProgress() {
    return CTheScripts::bMiniGameInProgress;
}

//! 2495 SET_FORCE_RANDOM_CAR_MODEL (case @0x479817): model  -- CTheScripts::ForceRandomCarModel (0xA4448C) = model
void SetForceRandomCarModel(int32 model) {
    CTheScripts::ForceRandomCarModel = model;
}

//! 2496 GET_RANDOM_CAR_OF_TYPE_IN_ANGLED_AREA_NO_SAVE (case @0x479831): x1, y1, x2, y2, width, model => vehicle handle (-1 if none)
//! The vehicle pool is scanned from the LAST slot to the first, until a match is found: an automobile / bike (GetVehicleAppearance 1 or 2) whose model is `model`
//! (or `model` < 0 = any), that CanBeDeleted(), and whose position (matrix or placement) is inside the angled area [0x477DA0]; => the vehicle's pool ref [0x424160]
int32 GetRandomCarOfTypeInAngledAreaNoSave(float x1, float y1, float x2, float y2, float width, int32 model) {
    auto* const pool = GetVehiclePool();
    int32       result = -1;
    for (int32 i = (int32)pool->GetSize(); i != 0 && result == -1;) {
        --i;
        auto* const veh = pool->GetAt(i); // 0x41CC10
        if (!veh) {
            continue;
        }
        if (veh->GetVehicleAppearance() != VEHICLE_APPEARANCE_AUTOMOBILE && veh->GetVehicleAppearance() != VEHICLE_APPEARANCE_BIKE) { // 0x6D1080
            continue;
        }
        if (veh->m_nModelIndex != model && model >= 0) {
            continue;
        }
        if (!veh->CanBeDeleted()) { // 0x6D1180
            continue;
        }
        const CVector& pos = veh->GetPosition();
        if (IsPointInAngledAreaOriginal(pos, x1, y1, x2, y2, width)) {
            result = pool->GetRef(veh);
        }
    }
    return result;
}

//! 2498 FAIL_KILL_FRENZY (case @0x47994B): CDarkel::FailKillFrenzy() [0x43DC60]
void FailKillFrenzy() {
    CDarkel::FailKillFrenzy();
}

//! 2499 IS_COP_VEHICLE_IN_AREA_3D_NO_SAVE (case @0x479957): x1, y1, z1, x2, y2, z2 => compare flag: any vehicle (all pool slots are scanned, no early out) that IsLawEnforcementVehicle()
//! [0x6D2370] and isn't model 430 (predator), whose position (matrix or placement) lies in the box (the pairs are sorted first: swapped only if a > b; NaN => outside)
bool IsCopVehicleInArea3DNoSave(CVector a, CVector b) {
    SortPair(a.x, b.x);
    SortPair(a.y, b.y);
    SortPair(a.z, b.z);
    auto* const pool = GetVehiclePool();
    bool        found = false;
    for (int32 i = 0; i < (int32)pool->GetSize(); ++i) {
        auto* const veh = pool->GetAt(i);
        if (!veh || !veh->IsLawEnforcementVehicle() || veh->m_nModelIndex == 0x1AE) {
            continue;
        }
        const CVector& p = veh->GetPosition();
        if (p.x >= a.x && p.x <= b.x && p.y >= a.y && p.y <= b.y && p.z >= a.z && p.z <= b.z) {
            found = true;
        }
    }
    return found;
}
}; // namespace

void notsa::script::commands::ported::g24::RegisterG24b() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g24b");

    REGISTER_COMMAND_HANDLER(COMMAND_PAUSE_CURRENT_BEAT_TRACK, PauseCurrentBeatTrack);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_PLAYER_CYCLE_WEAPON_BUTTON, SetPlayerCycleWeaponButton);
    REGISTER_COMMAND_HANDLER(COMMAND_MARK_ROAD_NODE_AS_DONT_WANDER, MarkRoadNodeAsDontWander);
    REGISTER_COMMAND_HANDLER(COMMAND_UNMARK_ALL_ROAD_NODES_AS_DONT_WANDER, UnMarkAllRoadNodesAsDontWander);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHECKPOINT_HEADING, SetCheckpointHeading);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_MISSION_RESPECT_TOTAL, SetMissionRespectTotal);
    REGISTER_COMMAND_HANDLER(COMMAND_AWARD_PLAYER_MISSION_RESPECT, AwardPlayerMissionRespect);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_COLLISION, SetCarCollision);
    REGISTER_COMMAND_HANDLER(COMMAND_CHANGE_PLAYBACK_TO_USE_AI, ChangePlaybackToUseAI);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_NIGHT_VISION_ACTIVE, IsNightVisionActive);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CREATE_RANDOM_COPS, SetCreateRandomCops);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_SET_IGNORE_WEAPON_RANGE_FLAG, TaskSetIgnoreWeaponRangeFlag);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_PICK_UP_SECOND_OBJECT, TaskPickUpSecondObject);
    REGISTER_COMMAND_HANDLER(COMMAND_DROP_SECOND_OBJECT, DropSecondObject);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_OBJECT_ELEGANTLY, RemoveObjectElegantly);
    REGISTER_COMMAND_HANDLER(COMMAND_DRAW_CROSSHAIR, DrawCrosshair);
    REGISTER_COMMAND_HANDLER(COMMAND_SHOW_BLIPS_ON_ALL_LEVELS, ShowBlipsOnAllLevels);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_DRUGGED_UP, SetCharDruggedUp);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CHAR_HEAD_MISSING, IsCharHeadMissing);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_HASH_KEY, GetHashKey);
    REGISTER_COMMAND_HANDLER(COMMAND_RANDOM_PASSENGER_SAY, RandomPassengerSay);
    REGISTER_COMMAND_HANDLER(COMMAND_HIDE_ALL_FRONTEND_BLIPS, HideAllFrontendBlips);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CHAR_IN_ANY_TRAIN, IsCharInAnyTrain);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_UP_SKIP_AFTER_MISSION, SetUpSkipAfterMission);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_VEHICLE_IS_CONSIDERED_BY_PLAYER, SetVehicleIsConsideredByPlayer);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_RANDOM_CAR_MODEL_IN_MEMORY, GetRandomCarModelInMemory);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_DOOR_LOCK_STATUS, GetCarDoorLockStatus);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CLOSEST_ENTRY_EXIT_FLAG, SetClosestEntryExitFlag);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_BLOOD, AddBlood);
    REGISTER_COMMAND_HANDLER(COMMAND_DISPLAY_CAR_NAMES, DisplayCarNames);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_DOOR_DAMAGED, IsCarDoorDamaged);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_MINIGAME_IN_PROGRESS, SetMinigameInProgress);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_MINIGAME_IN_PROGRESS, IsMinigameInProgress);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_FORCE_RANDOM_CAR_MODEL, SetForceRandomCarModel);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_RANDOM_CAR_OF_TYPE_IN_ANGLED_AREA_NO_SAVE, GetRandomCarOfTypeInAngledAreaNoSave);
    REGISTER_COMMAND_HANDLER(COMMAND_FAIL_KILL_FRENZY, FailKillFrenzy);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_COP_VEHICLE_IN_AREA_3D_NO_SAVE, IsCopVehicleInArea3DNoSave);
}
