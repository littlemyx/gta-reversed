/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "MissionCleanup.h"

#include "Garages.h"
#include "VehicleRecording.h"
#include "PostEffects.h"
#include "EntryExitManager.h"
#include "TheZones.h"
#include "Tasks/TaskTypes/TaskComplexUseMobilePhone.h"
#include "Scripts/Scripted2dEffects.h"
#include "DecisionMakers/DecisionMakerTypesFileLoader.h"
#include "LoadMonitor.h"
#include "Fx/FxSystem.h"

// Globals that have no declaration anywhere else in the code base (names are NOTSA)
static auto& s_CopsNeededCarDensityScale = StaticRef<float>(0x8A5B20); // Same global as the one in Population.cpp
static auto& s_CurDistForCam             = StaticRef<float>(0x8CCB84); // Same global as `gCurDistForCam` in Camera.cpp
static auto& s_Unk_B6EC2C                = StaticRef<bool>(0xB6EC2C);
static auto& s_Unk_B728E0                = StaticRef<int32>(0xB728E0);
static auto& s_Unk_BA18D9                = StaticRef<bool>(0xBA18D9);

void CMissionCleanup::InjectHooks() {
    RH_ScopedClass(CMissionCleanup);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Process, 0x468560);
    RH_ScopedInstall(FindFree, 0x4637C0);
    RH_ScopedInstall(CheckIfCollisionHasLoadedForMissionObjects, 0x4652D0);
}

CMissionCleanup::CMissionCleanup() {
    Init();
}

/* Initializes data
 * @addr 0x4637A0
 */
void CMissionCleanup::Init() {
    m_Count = 0;
    m_Objects.fill(tMissionCleanupEntity());
}

/* Performs a clean-up
 * @addr 0x468560
 */
void CMissionCleanup::Process() {
    const auto player = CWorld::Players[0].m_pPed; // Read directly (not via `FindPlayerPed`), as the original does

    if (CTheScripts::bScriptHasFadedOut) {
        CTheScripts::bScriptHasFadedOut = false;
        if (CWorld::Players[0].m_nPlayerState != PLAYERSTATE_HAS_BEEN_ARRESTED && CWorld::Players[0].m_nPlayerState != PLAYERSTATE_HAS_DIED) {
            TheCamera.Fade(0.5f, eFadeFlag::FADE_OUT);
            CPad::GetPad(0)->bPlayerSafe = false;
        }
    }

    CTrain::DisableRandomTrains(false);
    CTrain::ReleaseMissionTrains();
    CPlane::SwitchAmbientPlanes(true);
    CPopulation::PedDensityMultiplier = 1.0f;
    s_CopsNeededCarDensityScale       = 1.0f;

    if (CGangWars::bTrainingMission) {
        const auto disableRadarGangColors = !CGangWars::bGangWarsActive;
        CGangWars::bTrainingMission       = false;
        CTheZones::FillZonesWithGangColours(disableRadarGangColors);
    }

    CPopulation::m_AllRandomPedsThisType        = -1;
    CGangWars::bCanTriggerGangWarWhenOnAMission = false;
    CGangWars::ClearSpecificZonesToTriggerGangWar();
    CPopulation::m_bDontCreateRandomGangMembers = false;
    CPopulation::m_bOnlyCreateRandomGangMembers = false;
    CPopulation::m_bDontCreateRandomCops        = false;
    CGarages::NoResprays                        = false;
    CGarages::AllRespraysCloseOrOpen(true);
    CCullZones::bMilitaryZonesDisabled          = false;
    FindPlayerWanted(-1)->m_Multiplier          = 1.0f;
    CPickups::RemoveMissionPickUps();
    CRoadBlocks::ClearScriptRoadBlocks();
    CStreaming::DisableCopBikes(false);
    g_LoadMonitor.EnableAmbientCrime(); // 0xB72994
    CObject::bArea51SamSiteDisabled          = false;
    CObject::bAircraftCarrierSamSiteDisabled = true;
    ThePaths.ReleaseRequestedNodes();
    ThePaths.UnMarkAllRoadNodesAsDontWander();
    ThePaths.TidyUpNodeSwitchesAfterMission();
    CVehicleRecording::RemoveAllRecordingsThatArentUsed();
    TheCamera.SetWideScreenOff();
    TheCamera.m_nModeForTwoPlayersSeparateCars              = MODE_TWOPLAYER_SEPARATE_CARS;
    TheCamera.m_nModeForTwoPlayersSameCarShootingAllowed    = MODE_TWOPLAYER_IN_CAR_AND_SHOOTING;
    TheCamera.m_nModeForTwoPlayersSameCarShootingNotAllowed = MODE_BEHINDCAR;
    TheCamera.m_nModeForTwoPlayersNotBothInCar              = MODE_TWOPLAYER;
    TheCamera.m_bDisableFirstPersonInCar                    = false;
    TheCamera.m_bCinemaCamera                               = false; // 0xB6FD18
    TheCamera.InitialiseScriptableComponents();
    TheCamera.ResetDuckingSystem(nullptr);
    s_CurDistForCam                    = 1.0f;
    s_Unk_B6EC2C                       = false;
    CGameLogic::bScriptCoopGameGoingOn = false;
    CTheScripts::bDrawCrossHair        = eCrossHairType::NONE;
    CSpecialFX::bVideoCam              = false;
    CSpecialFX::bLiftCam               = false;
    CPostEffects::ScriptResetForEffects();
    CEntryExitManager::ms_bDisabled = false;
    if (CGame::currArea == AREA_CODE_NORMAL_WORLD) {
        CTimeCycle::StopExtraColour(false);
    }

    for (uint8 i = 0; i < 4; i++) {
        AudioEngine.ClearMissionAudio(i);
    }

    CWeather::ReleaseWeather();
    s_Unk_B728E0 = 99999;

    for (int32 slot = 0; slot < 10; slot++) {
        CStreaming::SetMissionDoesntRequireSpecialChar(slot);
    }

    CTheScripts::ClearAllSuppressedCarModels();
    CTheScripts::ForceRandomCarModel      = -1;
    CStreaming::ms_disableStreaming       = false;
    CHud::m_ItemToFlash                   = (eHudItem)0xFFFF; // Stored as a word
    CHud::bScriptDontDisplayRadar         = false;
    CHud::bScriptDontDisplayVehicleName   = false;
    CHud::bScriptDontDisplayAreaName      = false;
    CHud::bScriptForceDisplayWithCounters = false;
    FrontEndMenuManager.m_bMenuAccessWidescreen = false; // 0xBA677C
    CTheScripts::RadarZoomValue           = 0;
    CTheScripts::RadarShowBlipOnAllLevels = false;
    CTheScripts::HideAllFrontEndMapBlips  = false;
    C3dMarkers::ForceRender(false);
    CTheScripts::bDisplayHud                            = true;
    CTheScripts::fCameraHeadingWhenPlayerIsAttached     = 0.0f;
    CTheScripts::fCameraHeadingStepWhenPlayerIsAttached = 0.0f;
    CTheScripts::bEnableCraneRaise                      = true;
    CTheScripts::bEnableCraneLower                      = true;
    CTheScripts::bEnableCraneRelease                    = true;
    s_Unk_BA18D9                                        = false;
    CTheScripts::bUseMessageFormatting                  = false;
    CTheScripts::MessageCentre                          = 0;
    CTheScripts::MessageWidth                           = 0;
    CTheScripts::bDrawOddJobTitleBeforeFade             = true;
    CTheScripts::bDrawSubtitlesBeforeFade               = true;

    auto& group = CPedGroups::ms_groups[player->GetPlayerData()->m_nPlayerGroup];
    group.GetIntelligence().SetDefaultTaskAllocatorType(ePedGroupDefaultTaskAllocatorType::FOLLOW_LIMITED);
    group.GetIntelligence().SetGroupDecisionMakerType(eDecisionMakerType::UNKNOWN);
    group.GetMembership().SetSeparationRange(120.0f); // 0x86C6C0 (`ms_fPlayerGroupMaxSeparation`)
    player->GetPlayerData()->m_bGroupStuffDisabled = false;
    player->ForceGroupToAlwaysFollow(false);
    player->ForceGroupToNeverFollow(false);
    player->MakePlayerGroupReappear();
    player->GetPlayerData()->m_nScriptLimitToGangSize  = 99;
    player->m_fireDmgMult                              = 1.0f;
    CWorld::Players[0].m_PlayerData.m_nFadeDrunkenness = 1; // 0xB7CDDD
    CWorld::Players[0].m_PlayerData.m_nDrugLevel       = 0; // 0xB7CDDE
    player->EnablePedSpeech();
    player->EnablePedSpeechForScriptSpeech();

    CPad::GetPad(0)->SetDrunkInputDelay(0);
    CPad::GetPad(0)->bApplyBrakes                    = false;
    CPad::GetPad(0)->bDisablePlayerEnterCar          = false;
    CPad::GetPad(0)->bDisablePlayerDuck              = false;
    CPad::GetPad(0)->bDisablePlayerFireWeapon        = false;
    CPad::GetPad(0)->bDisablePlayerFireWeaponWithL1  = false;
    CPad::GetPad(0)->bDisablePlayerCycleWeapon       = false;
    CPad::GetPad(0)->bDisablePlayerJump              = false;
    CPad::GetPad(0)->bDisablePlayerDisplayVitalStats = false;
    CWorld::Players[0].m_bCanDoDriveBy               = true; // 0xB7CEEB

    // 0x634A40 - `CTaskComplexUseMobilePhone::Quit(CPed*)`, inlined here as the method has no declaration
    if (const auto task = player->GetTaskManager().FindTaskByType(TASK_PRIMARY_PRIMARY, TASK_COMPLEX_USE_MOBILE_PHONE)) {
        if (task->GetTaskType() == TASK_COMPLEX_USE_MOBILE_PHONE) {
            const auto phone = static_cast<CTaskComplexUseMobilePhone*>(task);
            if (!phone->m_bQuit) {
                phone->m_bQuit = true;
                phone->MakeAbortable(player, ABORT_PRIORITY_LEISURE, nullptr);
            }
        }
    }

    CVehicle::bDisableRemoteDetonation          = false;
    CVehicle::bDisableRemoteDetonationOnContact = false;
    CGameLogic::ClearSkip(true);
    if (CGameLogic::GameState != GAMELOGIC_STATE_BUSTED && CGameLogic::GameState != GAMELOGIC_STATE_WASTED) {
        if (FindPlayerPed(-1)->m_nPedState != PEDSTATE_DEAD && FindPlayerPed(-1)->m_nPedState != PEDSTATE_DIE) {
            CRestart::ClearRespawnPointForDurationOfMission();
        }
    }
    CTheScripts::RiotIntensity = 0;
    gFireManager.ClearAllScriptFireFlags();
    CTheScripts::StoreVehicleIndex     = -1;
    CTheScripts::StoreVehicleWasRandom = true;
    CTheScripts::UpsideDownCars.Init(); // The original inlines the reset of all 6 entries
    CTheScripts::StuckCars.Init();
    CStats::bShowUpdateStats                     = true;
    CEventGunShot::ms_fGunShotSenseRangeForRiot2 = -1.0f;
    CHud::m_fHelpMessageBoxWidth                 = 200.0f;
    CVehicle::ms_forceVehicleLightsOff           = false;

    if (!CTheScripts::bMiniGameInProgress) {
        if (!CWorld::Players[CWorld::PlayerInFocus].m_pRemoteVehicle) {
            TheCamera.Restore();
        }
        player->GetPlayerWanted()->m_bPoliceBackOff    = false;
        player->GetPlayerWanted()->m_bEverybodyBackOff = false;
        CWorld::Players[0].MakePlayerSafe(false, 10000.0f);
        CHud::SetHelpMessage(nullptr, true, false, false);
    }

    for (auto& entity : m_Objects) {
        if (entity.type == MISSION_CLEANUP_ENTITY_TYPE_EMPTY) {
            continue;
        }

        switch ((uint8)entity.type) {
        case MISSION_CLEANUP_ENTITY_TYPE_VEHICLE: {
            if (const auto veh = GetVehiclePool()->GetAtRef(entity.handle)) {
                CTheScripts::CleanUpThisVehicle(veh);
            }
            break;
        }
        case MISSION_CLEANUP_ENTITY_TYPE_PED: {
            if (const auto ped = GetPedPool()->GetAtRef(entity.handle)) {
                CTheScripts::CleanUpThisPed(ped);
            }
            break;
        }
        case MISSION_CLEANUP_ENTITY_TYPE_OBJECT: {
            if (const auto obj = GetObjectPool()->GetAtRef(entity.handle)) {
                CTheScripts::CleanUpThisObject(obj);
            }
            break;
        }
        case MISSION_CLEANUP_ENTITY_TYPE_PARTICLE: {
            const auto idx = CTheScripts::GetActualScriptThingIndex(entity.handle, SCRIPT_THING_EFFECT_SYSTEM);
            if (idx >= 0 && CTheScripts::ScriptEffectSystemArray[idx].m_pFxSystem) {
                CTheScripts::ScriptEffectSystemArray[idx].m_pFxSystem->Kill();
                CTheScripts::RemoveScriptEffectSystem(entity.handle);
            }
            break;
        }
        case MISSION_CLEANUP_ENTITY_TYPE_GROUP: {
            const auto idx = CTheScripts::GetActualScriptThingIndex(entity.handle, SCRIPT_THING_PED_GROUP);
            if (idx >= 0) {
                CPedGroups::RemoveGroup(idx);
            }
            break;
        }
        case MISSION_CLEANUP_ENTITY_TYPE_PED_QUEUE: { // NOTE: Actually a scripted 2D effect (see `SCRIPT_THING_2D_EFFECT`)
            const auto idx = CTheScripts::GetActualScriptThingIndex(entity.handle, SCRIPT_THING_2D_EFFECT);
            if (idx >= 0) {
                CScripted2dEffects::ms_activated[idx] = false;
            }
            break;
        }
        case MISSION_CLEANUP_ENTITY_TYPE_TASK_SEQUENCE: {
            const auto idx = CTheScripts::GetActualScriptThingIndex(entity.handle, SCRIPT_THING_SEQUENCE_TASK);
            if (idx >= 0) {
                auto& seq = CTaskSequences::ms_taskSequence[idx];
                if (seq.m_RefCnt == 0) {
                    seq.m_bFlushTasks = false;
                    seq.Flush();
                } else {
                    seq.m_bFlushTasks = true;
                }
                CTaskSequences::ms_bIsOpened[idx]                    = false;
                CTheScripts::ScriptSequenceTaskArray[idx].m_bUsed = false;
            }
            break;
        }
        case MISSION_CLEANUP_ENTITY_TYPE_DECISION_MAKER: {
            const auto idx = CTheScripts::GetActualScriptThingIndex(entity.handle, SCRIPT_THING_DECISION_MAKER);
            if (idx >= 0) {
                CDecisionMakerTypesFileLoader::UnloadDecisionMaker((eDecisionTypes)idx);
            }
            break;
        }
        case MISSION_CLEANUP_ENTITY_TYPE_SEARCHLIGHT: {
            if (CTheScripts::GetActualScriptThingIndex(entity.handle, SCRIPT_THING_SEARCH_LIGHT) >= 0) {
                CTheScripts::RemoveScriptSearchLight(entity.handle);
            }
            break;
        }
        case MISSION_CLEANUP_ENTITY_TYPE_CHECKPOINT: {
            if (CTheScripts::GetActualScriptThingIndex(entity.handle, SCRIPT_THING_CHECKPOINT) >= 0) {
                CTheScripts::RemoveScriptCheckpoint(entity.handle);
            }
            break;
        }
        case MISSION_CLEANUP_ENTITY_TYPE_TXD: {
            CTheScripts::RemoveScriptTextureDictionary();
            break;
        }
        default: // Types 6, 10 and everything above 13 (jump table default)
            break;
        }

        // NOTE: Both fields are re-read here, the clean-up calls above may have reset the entry already
        RemoveEntityFromList(entity.handle, (MissionCleanUpEntityType)entity.type);
    }

    // Forget all the acquaintances the missions (PED_TYPE_MISSION1..8) have set up with the other ped types
    for (int32 pedType = PED_TYPE_PLAYER1; pedType < PED_TYPE_MISSION1; pedType++) {
        for (int32 missionType = PED_TYPE_MISSION1; missionType <= PED_TYPE_MISSION8; missionType++) {
            for (AcquaintanceId id = 0; id < 5; id++) {
                CPedType::ClearPedTypeAsAcquaintance(id, (ePedType)pedType, CPedType::GetPedFlag((ePedType)missionType));
            }
        }
    }
    for (int32 missionType = PED_TYPE_MISSION1; missionType <= PED_TYPE_MISSION8; missionType++) {
        for (AcquaintanceId id = 0; id < 5; id++) {
            CPedType::ClearPedTypeAcquaintances(id, (ePedType)missionType);
        }
    }
}

/* Finds a free entity, returns NULL if no free entity can be found.
 * @addr 0x4637C0
 */
tMissionCleanupEntity* CMissionCleanup::FindFree() {
    for (auto& entity : m_Objects) {
        if (entity.type == MISSION_CLEANUP_ENTITY_TYPE_EMPTY) {
            return &entity;
        }
    }
    return nullptr;
}

/* Adds entity to list
 * @addr 0x4637E0
 */
void CMissionCleanup::AddEntityToList(int32 handle, MissionCleanUpEntityType type) {
    for (auto& entity : m_Objects) {
        if (entity.type == MissionCleanUpEntityType::MISSION_CLEANUP_ENTITY_TYPE_EMPTY) {
            entity.handle = handle;
            entity.type   = type;
            ++m_Count;
            return;
        }
    }
}

/* Remotes entity from list
 * @addr 0x4654B0
 */
void CMissionCleanup::RemoveEntityFromList(int32 handle, MissionCleanUpEntityType type) {
    for (auto& entity : m_Objects) {
        if (entity.type != type || entity.handle != handle) {
            continue;
        }

        switch (entity.type) {
        case MissionCleanUpEntityType::MISSION_CLEANUP_ENTITY_TYPE_VEHICLE: {
            auto* veh = GetVehiclePool()->GetAtRef(entity.handle);

            if (veh && veh->m_bIsStaticWaitingForCollision) {
                veh->m_bIsStaticWaitingForCollision = false;
                if (!veh->GetIsStatic()) {
                    veh->AddToMovingList();
                }
            }
            break;
        }
        case MissionCleanUpEntityType::MISSION_CLEANUP_ENTITY_TYPE_PED: {
            auto* ped = GetPedPool()->GetAtRef(entity.handle);

            if (ped && ped->m_bIsStaticWaitingForCollision) {
                ped->m_bIsStaticWaitingForCollision = false;
                if (!ped->GetIsStatic()) {
                    ped->AddToMovingList();
                }
            }
            break;
        }
        case MissionCleanUpEntityType::MISSION_CLEANUP_ENTITY_TYPE_OBJECT: {
            auto* obj = GetObjectPool()->GetAtRef(entity.handle);

            if (obj && obj->m_bIsStaticWaitingForCollision) {
                obj->m_bIsStaticWaitingForCollision = false;
                if (!obj->GetIsStatic()) {
                    obj->AddToMovingList();
                }
            }
            break;
        }
        default:
            break;
        }

        entity = tMissionCleanupEntity();
        --m_Count;
    }
}

/* Checks if collision has loaded for mission objects
 * @addr 0x4652D0
 */
void CMissionCleanup::CheckIfCollisionHasLoadedForMissionObjects() {
    ZoneScoped;

    // Common part of the vehicle / ped / object handling (all of them are the same up to the entity type)
    const auto ProcessEntity = [](CPhysical* entity) -> bool {
        if (!entity || !entity->m_bIsStaticWaitingForCollision) {
            return false;
        }
        if (!CColStore::HasCollisionLoaded(entity->GetPosition(), entity->GetAreaCode())) { // 0x410CE0
            return false;
        }
        if (!CIplStore::HaveIplsLoaded(entity->GetPosition(), entity->GetAreaCode())) { // 0x405600
            return false;
        }
        entity->m_bIsStaticWaitingForCollision = false;
        if (!entity->GetIsStatic()) { // 0x4633E0
            entity->AddToMovingList(); // 0x542800
        }
        return true;
    };

    for (const auto& entity : m_Objects) {
        switch ((uint8)entity.type) {
        case MISSION_CLEANUP_ENTITY_TYPE_VEHICLE: {
            const auto veh = GetVehiclePool()->GetAtRef(entity.handle);
            if (!ProcessEntity(veh)) {
                break;
            }
            switch (veh->m_nVehicleType) {
            case VEHICLE_TYPE_AUTOMOBILE:
            case VEHICLE_TYPE_TRAILER:
                static_cast<CAutomobile*>(veh)->PlaceOnRoadProperly(); // 0x6AF420
                break;
            case VEHICLE_TYPE_BIKE:
                static_cast<CBike*>(veh)->PlaceOnRoadProperly(); // 0x6BEEB0
                break;
            default:
                break;
            }
            break;
        }
        case MISSION_CLEANUP_ENTITY_TYPE_PED: {
            ProcessEntity(GetPedPool()->GetAtRef(entity.handle));
            break;
        }
        case MISSION_CLEANUP_ENTITY_TYPE_OBJECT: {
            ProcessEntity(GetObjectPool()->GetAtRef(entity.handle));
            break;
        }
        default:
            break;
        }
    }
}

// NOTSA
void CMissionCleanup::AddEntityToList(CObject& obj) {
    return AddEntityToList(GetObjectPool()->GetRef(&obj), MISSION_CLEANUP_ENTITY_TYPE_OBJECT);
}

// NOTSA
void CMissionCleanup::AddEntityToList(CPed& ped) {
    return AddEntityToList(GetPedPool()->GetRef(&ped), MISSION_CLEANUP_ENTITY_TYPE_PED);
}

// NOTSA
void CMissionCleanup::AddEntityToList(CVehicle& veh) {
    return AddEntityToList(GetVehiclePool()->GetRef(&veh), MISSION_CLEANUP_ENTITY_TYPE_VEHICLE);
}

// NOTSA
void CMissionCleanup::RemoveEntityFromList(CObject& obj) {
    return RemoveEntityFromList(GetObjectPool()->GetRef(&obj), MISSION_CLEANUP_ENTITY_TYPE_OBJECT);
}

void CMissionCleanup::RemoveEntityFromList(CPed& ped) {
    return RemoveEntityFromList(GetPedPool()->GetRef(&ped), MISSION_CLEANUP_ENTITY_TYPE_PED);
}

void CMissionCleanup::RemoveEntityFromList(CVehicle& veh) {
    return RemoveEntityFromList(GetVehiclePool()->GetRef(&veh), MISSION_CLEANUP_ENTITY_TYPE_VEHICLE);
}
