#include "StdInc.h"

#include "Remote.h"

#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Heli.h"
#include "Entity/Vehicle/Plane.h"
#include "Scripts/TheScripts.h"
#include "Pools/Pools.h"

static_assert(offsetof(CVehicle, m_autoPilot) == 0x390);
static_assert(offsetof(CVehicle, vehicleFlags) == 0x428);
static_assert(offsetof(CPlayerInfo, m_nTimeOfRemoteVehicleExplosion) == 0xE0);

void CRemote::InjectHooks() {
    RH_ScopedClass(CRemote);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(GivePlayerRemoteControlledCar, 0x45AB10);
    RH_ScopedInstall(TakeRemoteControlledCarFromPlayer, 0x45AE80);
}

// 0x45AE80
void CRemote::TakeRemoteControlledCarFromPlayer(bool bCreateRemoteVehicleExplosion) {
    auto& pi = CWorld::Players[CWorld::PlayerInFocus];

    // NOTE: `m_pRemoteVehicle` isn't null-checked in the original
    if (pi.m_pRemoteVehicle->GetCreatedBy() == MISSION_VEHICLE) {
        pi.m_pRemoteVehicle->SetVehicleCreatedBy(RANDOM_VEHICLE);
        CTheScripts::MissionCleanUp.RemoveEntityFromList(*pi.m_pRemoteVehicle);
    }
    pi.m_pRemoteVehicle->vehicleFlags.bIsLocked = false;

    pi.m_nTimeOfRemoteVehicleExplosion   = CTimer::GetTimeInMS();
    pi.m_bAfterRemoteVehicleExplosion    = true;
    pi.m_bCreateRemoteVehicleExplosion   = bCreateRemoteVehicleExplosion;
    pi.m_bFadeAfterRemoteVehicleExplosion = true;
}

// 0x45AB10
void CRemote::GivePlayerRemoteControlledCar(CVector pos, float rotation, int16 modelId) {
    // NOTE: `operator new` returns null if the pool is full, the original then crashes below
    CVehicle* veh;
    if (CModelInfo::IsHeliModel(modelId)) {
        veh = new CHeli(modelId, MISSION_VEHICLE);
    } else if (CModelInfo::IsPlaneModel(modelId)) {
        veh = new CPlane(modelId, MISSION_VEHICLE);
    } else {
        veh = new CAutomobile(modelId, MISSION_VEHICLE, true);
    }

    // x87: the sum below is kept in extended precision until the float store
    bool isGroundZFound{};
    const float groundZ = CWorld::FindGroundZFor3DCoord({ pos.x, pos.y, pos.z + 2.f }, &isGroundZFound, nullptr); // 0x858CA0 = 2.0f
    const float height  = veh->GetDistanceFromCentreOfMassToBaseOfModel();
    pos.z               = (float)((double)height + (double)groundZ);

    veh->GetMatrix().SetRotateZOnly(rotation); // NOTE: the original doesn't check if `m_matrix` exists here
    if (veh->m_matrix) {
        veh->m_matrix->GetPosition() = pos;
    } else {
        veh->m_placement.m_vPosn = pos;
    }

    veh->SetStatus(STATUS_REMOTE_CONTROLLED);
    veh->vehicleFlags.bIsLocked = true;
    CCarCtrl::JoinCarWithRoadSystem(veh);

    auto& ap = veh->m_autoPilot;
    ap.m_nCarMission   = MISSION_NONE;
    ap.m_nTempAction   = TEMPACT_NONE;
    ap.m_nCarDrivingStyle = DRIVING_STYLE_STOP_FOR_CARS;
    ap.m_speed         = 9.f;
    ap.m_nCruiseSpeed  = 9;
    ap.m_nCurrentLane  = 0;
    ap.m_nNextLane     = 0;

    veh->vehicleFlags.bEngineOn = !veh->vehicleFlags.bEngineBroken;
    CWorld::Add(veh);

    if (auto* const playerVeh = FindPlayerVehicle()) {
        playerVeh->SetStatus(STATUS_FORCED_STOP);
    }

    auto& remote = CWorld::Players[CWorld::PlayerInFocus].m_pRemoteVehicle;
    remote = veh;
    veh->RegisterReference(reinterpret_cast<CEntity**>(&remote));

    TheCamera.TakeControl(veh, MODE_CAM_ON_A_STRING, eSwitchType::INTERPOLATION, 1);
    TheCamera.SetZoomValueCamStringScript(1);
}
