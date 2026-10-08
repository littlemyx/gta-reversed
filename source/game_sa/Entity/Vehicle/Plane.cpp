#include "StdInc.h"

#include "Plane.h"
#include "CarCtrl.h"
#include "FireManager.h"
#include "Heli.h"
#include "Game.h"
#include "CutsceneMgr.h"
#include "Weather.h"
#include "PopCycle.h"
#include "GameLogic.h"
#include "PedGroups.h"
#include "PedGroup.h"
#include "EventDanger.h"
#include "Streaming.h"
#include "Darkel.h"
#include "Explosion.h"
#include "TheScripts.h"
#include "Fx.h"

auto& HARRIER_NOZZLE_ROTATERATE = StaticRef<float>(0x8D33DC);       // 25.0f
auto& PLANE_DAMAGE_WAVE_COUNTER_VAR = StaticRef<float>(0x8D33E0);   // 0.75f
auto& PLANE_DAMAGE_THRESHHOLD = StaticRef<float>(0x8D33E4);         // 500.0f
auto& PLANE_DAMAGE_SCALE_MASS = StaticRef<float>(0x8D33E8);         // 10000.0f
auto& PLANE_DAMAGE_DESTROY_THRESHHOLD = StaticRef<float>(0x8D33EC); // 5000.0f
auto& vecRCBaronGunPos = StaticRef<CVector>(0x8D33F0);            // <0.0f, 0.45f, 0.0f>
auto& PLANE_HYDRA_NOZZLE_ACCEL_MULT = StaticRef<float>(0x8D341C);   // 0.25f
auto& PLANE_AILERON_PANEL_MULT = StaticRef<float>(0x8D3420);        // 10.0f
auto& PLANE_AILERON_PANEL_AMPL = StaticRef<float>(0x8D3424);        // 0.002f
auto& PLANE_AILERON_DAMAGE_AMPL = StaticRef<float>(0x8D3428);       // 0.05f
auto& PLANE_ELEVATOR_PANEL_MULT = StaticRef<float>(0x8D342C);       // 10.0f
auto& PLANE_ELEVATOR_PANEL_AMPL = StaticRef<float>(0x8D3430);       // 0.002f
auto& PLANE_ELEVATOR_DAMAGE_AMPL = StaticRef<float>(0x8D3434);      // 0.05f
auto& PLANE_RUDDER_PANEL_MULT = StaticRef<float>(0x8D3438);         // 10.0f
auto& PLANE_RUDDER_PANEL_AMPL = StaticRef<float>(0x8D343C);         // 0.002f
auto& PLANE_RUDDER_DAMAGE_AMPL = StaticRef<float>(0x8D3440);        // 0.05f
auto& PLANE_PROP_DAMAGE_MULT = StaticRef<float>(0x8D3444);          // 0.2f
auto& PLANE_PROP_DAMAGE_WAVE_PERIOD = StaticRef<int32>(0x8D3448);   // 2500
auto& PLANE_PROP_DAMAGE_BASE = StaticRef<float>(0x8D344C);          // 0.8f

void CPlane::InjectHooks() {
    RH_ScopedVirtualClass(CPlane, 0x871948, 71);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(Constructor, 0x6C8E20);
    RH_ScopedInstall(InitPlaneGenerationAndRemoval, 0x6CAD90);
    RH_ScopedVMTInstall(SetUpWheelColModel, 0x6C9140);
    RH_ScopedVMTInstall(BurstTyre, 0x6C9150);
    RH_ScopedVMTInstall(PreRender, 0x6C94A0, { .Reversed = false });
    RH_ScopedVMTInstall(Render, 0x6CAB70);
    RH_ScopedInstall(IsAlreadyFlying, 0x6CAB90);
    RH_ScopedVMTInstall(Fix, 0x6CABB0);
    RH_ScopedVMTInstall(SetupDamageAfterLoad, 0x6CAC10);
    RH_ScopedInstall(SetGearUp, 0x6CAC20);
    RH_ScopedInstall(SetGearDown, 0x6CAC70);
    RH_ScopedVMTInstall(OpenDoor, 0x6CACB0);
    RH_ScopedVMTInstall(ProcessControl, 0x6C9260);
    RH_ScopedVMTInstall(ProcessControlInputs, 0x6CADD0);
    RH_ScopedVMTInstall(ProcessFlyingCarStuff, 0x6CB7C0);
    RH_ScopedVMTInstall(VehicleDamage, 0x6CC4B0);
    RH_ScopedInstall(CountPlanesAndHelis, 0x6CCA50);
    RH_ScopedInstall(AreWeInNoPlaneZone, 0x6CCAA0);
    RH_ScopedInstall(AreWeInNoBigPlaneZone, 0x6CCBB0);
    RH_ScopedInstall(SwitchAmbientPlanes, 0x6CCC50);
    RH_ScopedVMTInstall(BlowUpCar, 0x6CCCF0);
    RH_ScopedInstall(FindPlaneCreationCoors, 0x6CD090);
    RH_ScopedInstall(DoPlaneGenerationAndRemoval, 0x6CD2F0);
}

// 0x6C8E20
CPlane::CPlane(int32 modelIndex, eVehicleCreatedBy createdBy) : CAutomobile(modelIndex, createdBy, true) {
    m_nVehicleSubType = VEHICLE_TYPE_PLANE;

    m_fLeftRightSkid               = 0.0f;
    m_fSteeringUpDown              = 0.0f;
    m_fSteeringLeftRight           = 0.0f;
    m_fAccelerationBreakStatus     = 0.0f;
    m_fAccelerationBreakStatusPrev = 1.0f;
    m_fPropSpeed                   = 0.0f;
    field_9C8                      = 0.0f;
    m_fLandingGearStatus           = 0.0f;
    field_9A0                      = 0;
    m_planeCreationHeading         = 0.0f;
    m_planeHeading                 = 0.0f;
    m_planeHeadingPrev             = 0.0f;
    m_maxAltitude                  = 15.0f;
    m_altitude                     = 25.0f;
    m_minAltitude                  = 20.0f;
    m_forwardZ                     = 0;
    m_nStartedFlyingTime           = 0;
    m_fSteeringFactor              = 0.0f;

    if (m_nModelIndex != MODEL_VORTEX)
        physicalFlags.bDontCollideWithFlyers = true;

    m_nExtendedRemovalRange = 255;
    vehicleFlags.bNeverUseSmallerRemovalRange = true;
    vehicleFlags.bIsBig = true;

    auto& leftDoor = m_doors[DOOR_LEFT_FRONT];
    switch (modelIndex) {
    case MODEL_HYDRA:
    case MODEL_RUSTLER:
    case MODEL_CROPDUST:
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        leftDoor.Init((3.0f * PI) / 5.0f, 0.0f, DOOR_AXIS_NEG_X, DOOR_AXIS_Y, DOOR_EXTRA_BASED);
        break;
    case MODEL_SHAMAL:
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        leftDoor.Init(-((3.0f * PI) / 4.0f), 0.0f, DOOR_AXIS_Z, DOOR_AXIS_Y, DOOR_EXTRA_BASED);
        rwObjectSetFlags(GetFirstObject(m_aCarNodes[PLANE_WHEEL_LF]), 0);
        break;
    case MODEL_NEVADA:
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        leftDoor.Init(-TWO_PI / 5.0f, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
        break;
    case MODEL_VORTEX:
        if (m_panels[FRONT_LEFT_PANEL].m_nFrameId == (uint16)-1)
            m_panels[FRONT_LEFT_PANEL].SetPanel(PLANE_GEAR_L, 1, -0.25f);
        break;
    case MODEL_STUNT:
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        leftDoor.Init((3.0f * PI) / 5.0f, 0.0f, DOOR_AXIS_NEG_X, DOOR_AXIS_Y, DOOR_EXTRA_BASED);
        rwObjectSetFlags(GetFirstObject(m_aCarNodes[PLANE_WHEEL_LB]), 0);
        rwObjectSetFlags(GetFirstObject(m_aCarNodes[PLANE_WHEEL_RB]), 0);
        break;
    }

    CVector modelPos, localPos;
    for (auto wheelId = 0; wheelId < 4; wheelId++) {
        GetVehicleModelInfo()->GetWheelPosn(wheelId, modelPos, false);
        GetVehicleModelInfo()->GetWheelPosn(wheelId, localPos, true);
        m_wheelPosition[wheelId] = m_wheelPosition[wheelId] - modelPos.z + localPos.z;
    }

    m_planeDamageWave = 0;
    m_pGunParticles = nullptr;
    m_nFiringMultiplier = 16;
    field_9DC = 0;
    field_9E0 = 0;
    m_apJettrusParticles.fill(nullptr);

    m_pSmokeParticle = nullptr;

    if (m_nModelIndex == MODEL_HYDRA)
        m_wMiscComponentAngle = HARRIER_NOZZLE_ROTATE_LIMIT;

    m_bSmokeEjectorEnabled = false;
}

// 0x6C9160
CPlane::~CPlane() {
    if (m_pGunParticles) {
        for (auto i = 0; i < CVehicle::GetPlaneNumGuns(); i++) {
            if (auto& particle = m_pGunParticles[i]) {
                particle->Kill();
                g_fxMan.DestroyFxSystem(particle);
            }
        }
        delete[] m_pGunParticles;
        m_pGunParticles = nullptr;
    }

    for (auto particle : m_apJettrusParticles) {
        if (particle) {
            FxSystem_c::KillAndClear(particle);
        }
    }

    FxSystem_c::SafeKillAndClear(m_pSmokeParticle);

    m_vehicleAudio.Terminate();
}

// 0x6CAD90
void CPlane::InitPlaneGenerationAndRemoval() {
    GenPlane_Status = 0;
    GenPlane_LastTimeGenerated = 0;
    GenPlane_Active = true;
}

// 0x6CCCF0
void CPlane::BlowUpCar(CEntity* damager, bool bHideExplosion) {
    if (!vehicleFlags.bCanBeDamaged) {
        return;
    }

    if (GetStatus() != STATUS_PLAYER && m_autoPilot.m_nCarMission != MISSION_PLANE_CRASH_AND_BURN && m_nModelIndex != MODEL_RCBARON) {
        m_autoPilot.m_nCarMission = MISSION_PLANE_CRASH_AND_BURN; // Not `SetCarMission`, but the same thing
        m_fHealth = 0.0f;
        return;
    }

    if (damager == FindPlayerPed() || damager == FindPlayerVehicle()) {
        auto& playerInfo = FindPlayerInfo();
        playerInfo.m_nHavocCaused += 20;
        playerInfo.m_fCurrentChaseValue += 10.0f;
        CStats::IncrementStat(STAT_COST_OF_PROPERTY_DAMAGED, (float)(CGeneral::GetRandomNumber() % 6000 + 4000));
    }

    if (GetStatus() == STATUS_PLAYER) {
        if (m_pDriver) {
            m_pDriver->bDontRender = true;
        }
        for (auto& passenger : m_apPassengers) {
            if (passenger) {
                passenger->bDontRender = true;
            }
        }
        m_bUsesCollision = false; // `m_nFlags &= 0xFFFFFF7E` (bits 0 and 7)
        m_bIsVisible     = false;
        ResetMoveSpeed();
        ResetTurnSpeed();
    }

    SetStatus(STATUS_WRECKED);
    physicalFlags.bRenderScorched = true;
    m_nTimeWhenBlowedUp = CTimer::GetTimeInMS();
    CVisibilityPlugins::SetClumpForAllAtomicsFlag(GetRpClump(), eAtomicComponentFlag::ATOMIC_PIPE_NO_EXTRA_PASSES);
    m_damageManager.FuckCarCompletely(false);

    if (m_nModelIndex != MODEL_RCBARON) {
        SetBumperDamage(FRONT_BUMPER, false);
        SetBumperDamage(REAR_BUMPER, false);
        SetDoorDamage(DOOR_BONNET, false);
        SetDoorDamage(DOOR_BOOT, false);
        SetDoorDamage(DOOR_LEFT_FRONT, false);
        SetDoorDamage(DOOR_RIGHT_FRONT, false);
        SetDoorDamage(DOOR_LEFT_REAR, false);
        SetDoorDamage(DOOR_RIGHT_REAR, false);
        SpawnFlyingComponent(CAR_WHEEL_LF, 1);

        if (m_aCarNodes[CAR_WHEEL_LF]) {
            if (const auto atomic = GetCurrentAtomicObject(m_aCarNodes[CAR_WHEEL_LF])) {
                RpAtomicSetFlags(atomic, 0);
            }
        }
    }

    m_nBombOnBoard = 0;
    m_fHealth      = 0.0f;
    m_wBombTimer   = 0;

    TheCamera.CamShake(0.4f, GetPosition());
    KillPedsInVehicle();

    m_nOverrideLights          = NO_CAR_LIGHT_OVERRIDE;
    vehicleFlags.bEngineOn     = false;
    vehicleFlags.bLightsOn     = false;
    m_bSmokeEjectorEnabled     = false;
    vehicleFlags.bSirenOrAlarm = false;
    autoFlags.bTaxiLight       = false;

    if (vehicleFlags.bIsAmbulanceOnDuty) {
        vehicleFlags.bIsAmbulanceOnDuty = false;
        CCarCtrl::NumAmbulancesOnDuty--;
    }

    if (vehicleFlags.bIsFireTruckOnDuty) {
        vehicleFlags.bIsFireTruckOnDuty = false;
        CCarCtrl::NumFireTrucksOnDuty--;
    }

    ChangeLawEnforcerState(false);
    gFireManager.StartFire(this, damager, 0.8f, 1, 7000, 0);
    CDarkel::RegisterCarBlownUpByPlayer(*this, 0);
    CExplosion::AddExplosion(
        this,
        damager,
        m_nModelIndex == MODEL_RCBARON ? EXPLOSION_RC_VEHICLE : EXPLOSION_AIRCRAFT,
        GetPosition(),
        0,
        1,
        -1.0f,
        0
    );
}

// 0x6CABB0
void CPlane::Fix() {
    m_damageManager.ResetDamageStatus();
    if (m_pHandlingData->m_bNoDoors) {
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_NOTPRESENT);
        m_damageManager.SetDoorStatus(DOOR_RIGHT_FRONT, DAMSTATE_NOTPRESENT);
        m_damageManager.SetDoorStatus(DOOR_LEFT_REAR, DAMSTATE_NOTPRESENT);
        m_damageManager.SetDoorStatus(DOOR_RIGHT_REAR, DAMSTATE_NOTPRESENT);
    }
    SetupDamageAfterLoad();
}

// 0x6CACB0
void CPlane::OpenDoor(CPed* ped, int32 componentId, eDoors door, float doorOpenRatio, bool playSound) {
    CAutomobile::OpenDoor(ped, componentId, door, doorOpenRatio, playSound);

    if (m_nModelIndex != MODEL_STUNT)
        return;

    // Unfinished code R*, which removed in Android
    if (false) // byte_C1CAFC
    {
        CMatrix matrix(RwFrameGetMatrix(m_aCarNodes[componentId]), false);
        const auto y = m_doors[door].m_angle - m_doors[door].m_prevAngle + matrix.GetPosition().y;
        matrix.SetTranslate({matrix.GetPosition().x, y, matrix.GetPosition().z});
        matrix.UpdateRW();
    }
}

// 0x6CAC10
void CPlane::SetupDamageAfterLoad() {
    vehicleFlags.bIsDamaged = false;
}

// 0x6CC4B0
void CPlane::VehicleDamage(float damageIntensity, eVehicleCollisionComponent component, CEntity* damager, CVector* vecCollisionCoors, CVector* vecCollisionDirection, eWeaponType weapon) {
    float destroyThreshold = PLANE_DAMAGE_DESTROY_THRESHHOLD;
    float damageThreshold  = PLANE_DAMAGE_THRESHHOLD;
    float particleScale    = 0.333f;
    bool  bWeaponDamage    = false;

    if (m_nModelIndex == MODEL_VORTEX) {
        CAutomobile::VehicleDamage(damageIntensity, component, damager, vecCollisionCoors, vecCollisionDirection, weapon);
        return;
    }

    if (!vehicleFlags.bCanBeDamaged) {
        return;
    }

    if (GetStatus() != STATUS_PLAYER && physicalFlags.bInvulnerable && m_pDamageEntity != FindPlayerPed() && m_pDamageEntity != FindPlayerVehicle()) {
        return;
    }

    if (damageIntensity == 0.0f) { // Damage from collision
        damageIntensity = m_fDamageIntensity;

        const auto massScale = m_fMass / PLANE_DAMAGE_SCALE_MASS;
        particleScale        = 1.0f;
        damageThreshold      = std::max(massScale, 1.0f) * PLANE_DAMAGE_THRESHHOLD;
        vecCollisionCoors    = &m_vecLastCollisionPosn;
        destroyThreshold     = massScale * PLANE_DAMAGE_DESTROY_THRESHHOLD;

        if (damageIntensity <= 0.0f) {
            return;
        }
        if (physicalFlags.bCollisionProof) {
            return;
        }
        if (m_pDamageEntity && m_pDamageEntity->GetIsTypePed()) {
            return;
        }
    } else { // Damage from a weapon
        bool bDamagedDueToFireOrExplosionOrBullet = false;
        if (!CanVehicleBeDamaged(damager, weapon, bDamagedDueToFireOrExplosionOrBullet)) {
            return;
        }
        if (weapon != WEAPON_ROCKET && weapon != WEAPON_ROCKET_HS && weapon != WEAPON_FREEFALL_BOMB && weapon != WEAPON_EXPLOSION) {
            return;
        }
        bWeaponDamage   = true;
        damageIntensity = 0.0f;
    }

    float damage = damageIntensity * m_pHandlingData->m_fCollisionDamageMultiplier;
    const float oldHealth = m_fHealth;
    if (this == FindPlayerVehicle()) {
        if (vehicleFlags.bTakeLessDamage) {
            damage *= 0.16666667f;
        } else {
            damage *= 0.5f;
        }
    } else if (vehicleFlags.bTakeLessDamage) {
        damage *= 0.083333336f;
    } else if (!m_pDamageEntity || m_pDamageEntity != FindPlayerVehicle()) {
        damage *= 0.25f;
    } else {
        damage *= 0.6666667f;
    }

    m_fHealth -= damage;
    if (m_fHealth <= 0.0f && oldHealth > 0.0f) {
        m_fHealth = 1.0f;
    }

    if (damageIntensity > destroyThreshold || (m_fHealth <= 1.0f && oldHealth > 0.0f)) {
        BlowUpCar(this, false); // NOTE: The vehicle itself is passed as the damager
        return;
    }

    if (damageIntensity <= damageThreshold && !bWeaponDamage) {
        return;
    }
    if (GetStatus() == STATUS_WRECKED) {
        return;
    }
    if (m_fMass <= 1000.0f) {
        return;
    }

    // Find the component that is the closest to the collision
    const auto hitPosLocal = m_matrix->InverseTransformVector(*vecCollisionCoors - GetPosition());

    float closestDist = 1000.0f;
    int32 closestNode = -1;
    for (int32 i = PLANE_STATIC_PROP; i <= PLANE_MISC_B; i++) {
        const auto node = m_aCarNodes[i];
        if (!node) {
            continue;
        }
        const auto distSq = (RwFrameGetMatrix(node)->pos - hitPosLocal).SquaredMagnitude();
        if (distSq < closestDist * closestDist) {
            closestDist = std::sqrt(distSq);
            closestNode = i;
        }
    }
    if (closestNode <= -1) {
        return;
    }

    if (closestNode == PLANE_RUDDER && (CGeneral::GetRandomNumber() & 1)) {
        if (m_aCarNodes[PLANE_ELEVATOR_L] && (CGeneral::GetRandomNumber() & 1)) {
            closestNode = PLANE_ELEVATOR_L;
        } else if (m_aCarNodes[PLANE_ELEVATOR_R]) {
            closestNode = PLANE_ELEVATOR_R;
        }
    }

    if (m_damageManager.ProgressAeroplaneDamage(closestNode)) {
        if (m_damageManager.GetAeroplaneCompStatus(closestNode) == 1 && closestNode >= PLANE_RUDDER && closestNode <= PLANE_GEAR_R) {
            // BUG: Original code checks 4 panels, but there are only 3 (the 4th one overlaps with `m_swingingChassis`)
            const auto numPanels = notsa::IsFixBugs() ? m_panels.size() : 4u;
            for (auto i = 0u; i < numPanels; i++) {
                auto& panel = (&m_panels[0])[i];
                if (panel.m_nFrameId == (uint16)-1) {
                    panel.SetPanel(closestNode, 0, -0.02f);
                    break;
                }
                if (panel.m_nFrameId == closestNode) {
                    break;
                }
            }
        } else if (m_damageManager.GetAeroplaneCompStatus(closestNode) == 2) {
            SetComponentVisibility(m_aCarNodes[closestNode], eAtomicComponentFlag::ATOMIC_DAMAGED);
        }
    }

    dmgDrawCarCollidingParticles(*vecCollisionCoors, particleScale * damageIntensity, weapon);

    const CVector nodePos = RwFrameGetMatrix(m_aCarNodes[closestNode])->pos;

    if (m_pSmokeParticle) {
        m_pSmokeParticle->Kill();
        m_pSmokeParticle = nullptr;
    }

    if (const auto objMat = GetModellingMatrix()) {
        m_pSmokeParticle = g_fxMan.CreateFxSystem("fire_med", nodePos, objMat, false);
        if (m_pSmokeParticle) {
            m_pSmokeParticle->Play();
            m_pSmokeParticle->SetVelAdd(-m_vecMoveSpeed * 5.0f);
            m_pSmokeParticle->SetLocalParticles(true);
            m_nSmokeTimer = CGeneral::GetRandomNumberInRange(2000, 4000);
        }
    }
}

// 0x6CAB90
void CPlane::IsAlreadyFlying() {
    m_nStartedFlyingTime = CTimer::GetTimeInMS() - 20000;
}

// 0x6CAC20
void CPlane::SetGearUp() {
    m_fLandingGearStatus = 1.0f;
    m_fAirResistance = m_pHandlingData->m_fDragMult / 1000.0f / 2.0f * m_pFlyingHandlingData->m_fGearUpR;
    m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_LEFT,  WHEEL_STATUS_MISSING);
    m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_LEFT,   WHEEL_STATUS_MISSING);
    m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_RIGHT, WHEEL_STATUS_MISSING);
    m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_RIGHT,  WHEEL_STATUS_MISSING);
}

// 0x6CAC70
void CPlane::SetGearDown() {
    m_fLandingGearStatus = 0.0f;
    m_fAirResistance = m_pHandlingData->m_fDragMult / 1000.0f / 2.0f;
    m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_LEFT,  WHEEL_STATUS_OK);
    m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_LEFT,   WHEEL_STATUS_OK);
    m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_RIGHT, WHEEL_STATUS_OK);
    m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_RIGHT,  WHEEL_STATUS_OK);
}

// 0x6CCA50
uint32 CPlane::CountPlanesAndHelis() {
    uint32 counter = 0;
    for (auto& vehicle : GetVehiclePool()->GetAllValid()) {
        if (vehicle.IsSubHeli() || vehicle.IsSubPlane()) {
            counter++;
        }
    }
    return counter;
}

// 0x6CCAA0
bool CPlane::AreWeInNoPlaneZone() {
    const auto& camPos = TheCamera.GetPosition();
    constexpr CVector vec1 = { -1073.0f, -675.0f, 50.0f };

    return DistanceBetweenPoints(vec1, camPos) < 200.0f ||
           camPos.x > -2743.0f && camPos.x < -2626.0f && camPos.y > 1300.0f && camPos.y < 2200.0f || // todo: Is point inside
           camPos.x > -1668.0f && camPos.x < -1122.0f && camPos.y > 541.0f && camPos.y < 1118.0f;
}

// 0x6CCBB0
bool CPlane::AreWeInNoBigPlaneZone() {
    // untested
    const auto& camPos = TheCamera.GetPosition();
    return DistanceBetweenPoints2D({ +1522.0f, -1237.0f }, camPos) < 800.0f ||
           DistanceBetweenPoints2D({ -1836.0f, +659.0f }, camPos) < 800.0f;
}

// 0x6CCC50
void CPlane::SwitchAmbientPlanes(bool enable) {
    if (GenPlane_Active && !enable) {
        // Remove all ambient (randomly generated) planes and helis
        for (auto i = GetVehiclePool()->GetSize(); i; i--) {
            auto* const veh = GetVehiclePool()->GetAt(i - 1);
            if (!veh) {
                continue;
            }
            if ((veh->IsSubHeli() || veh->IsSubPlane()) && veh->m_nCreatedBy == RANDOM_VEHICLE) {
                CWorld::Remove(veh);
                delete veh;
            }
        }
    }
    GenPlane_Active = enable;
}

// 0x6CD090
void CPlane::FindPlaneCreationCoors(CVector* outCoors, CVector* outTargetCoors, float* outPlaneOrientation, float* outFlightHeight, bool isBigPlane) {
    const float distFromPlayer = isBigPlane ? 340.0f : 140.0f;
    const float baseHeight     = isBigPlane ? 200.0f : 25.0f;

    for (int32 i = 0; i < 500; i += 10) {
        const float angle = (float)(CGeneral::GetRandomNumber() % 360) * 0.017453292f /* DegreesToRadians(1.0f) */;

        *outCoors = FindPlayerCoors();
        *outFlightHeight = (float)((CGeneral::GetRandomNumber() & 0xF) + i) + baseHeight;
        outCoors->x = std::cos(angle) * distFromPlayer + outCoors->x;
        outCoors->y = std::sin(angle) * distFromPlayer + outCoors->y;
        outCoors->z += *outFlightHeight;

        *outTargetCoors = FindPlayerCoors();
        const float targetDist = (float)(CGeneral::GetRandomNumber() & 0x1F) + 20.0f;
        const auto& camFwd = TheCamera.m_mCameraMatrix.GetForward(); // 0xB6F9AC
        const float dx = camFwd.x * targetDist;
        const float dy = camFwd.y * targetDist;
        const float dz = camFwd.z * targetDist;
        outTargetCoors->x = dx + outTargetCoors->x;
        outTargetCoors->y = dy + outTargetCoors->y;
        outTargetCoors->z = dz + outTargetCoors->z;
        outTargetCoors->z += *outFlightHeight;

        *outPlaneOrientation = CGeneral::GetATanOfXY(outTargetCoors->x - outCoors->x, outTargetCoors->y - outCoors->y);

        const CVector lineEnd = (*outTargetCoors - *outCoors) + *outTargetCoors;

        CWorld::AdvanceCurrentScanCode();

        // NOTSA: `box` is passed uninitialized by the original code (default-constructed here)
        CColBox      box{};
        CColSphere   sphere;
        sphere.Set(15.0f, *outCoors, SURFACE_DEFAULT, 0, tColLighting{0xFF});
        if (CWorld::GetIsLineOfSightClear(*outCoors, lineEnd, true, false, false, false, false, false, false)
            && !CCollision::CheckCameraCollisionBuildings((int32)outCoors->x, (int32)outCoors->y, box, sphere, sphere, sphere)
        ) {
            break;
        }
    }

    *outFlightHeight += 20.0f;
    outTargetCoors->z += 20.0f;
}

// 0x6CD2F0
void CPlane::DoPlaneGenerationAndRemoval() {
    if ((CTimer::GetFrameCounter() & 31) != 30) {
        return;
    }

    if (!GenPlane_Active) {
        return;
    }

    const auto numPlanesAndHelis = CountPlanesAndHelis();

    // Sets the vehicle's orientation like the code at 0x6C4130 (the same code is shared with another function by the compiler)
    const auto SetHeadingFast = [](CVehicle* veh, float heading) {
        auto& mat = *veh->m_matrix;
        mat.GetRight()   = CVector{ std::sin(heading), -std::cos(heading), 0.0f };
        mat.GetForward() = CVector{ std::cos(heading),  std::sin(heading), 0.0f };
        mat.GetUp()      = CVector{ 0.0f, 0.0f, 1.0f };
    };

    // Inlined `CVehicle::SetEngineOn(true)` (0x41BDD0): the engine is forced off if it is broken
    const auto TurnEngineOn = [](CVehicle* veh) {
        veh->vehicleFlags.bEngineOn = !veh->vehicleFlags.bEngineBroken;
    };

    const auto FinishGeneration = [] {
        GenPlane_LastTimeGenerated = CTimer::GetTimeInMS();
        GenPlane_Status = 0;
    };

    if (GenPlane_Status != 0) {
        if (GenPlane_Status != 1) {
            return;
        }

        if (!CStreaming::IsModelLoaded(GenPlane_ModelIndex)) {
            CStreaming::RequestModel(GenPlane_ModelIndex, STREAMING_KEEP_IN_MEMORY);
            return;
        }

        if (GenPlane_ModelIndex == MODEL_POLMAV) {
            CVector coors, targetCoors;
            float   heading, height;
            FindPlaneCreationCoors(&coors, &targetCoors, &heading, &height, false);

            // BUG: Original code doesn't check for allocation failure
            auto* const heli = new CHeli(GenPlane_ModelIndex, RANDOM_VEHICLE);
            heli->SetPosn(coors);
            SetHeadingFast(heli, heading);
            CCarCtrl::JoinCarWithRoadSystem(heli);
            heli->m_fHeliRotorSpeed = 0.22f;
            heli->SetStatus(STATUS_PHYSICS);
            CWorld::Add(heli);

            heli->m_fMinAltitude = height;
            heli->m_autoPilot.m_nCarMission = MISSION_HELI_FLYINDIRECTION;
            heli->field_9B4 = heading;
            heli->m_fMaxAltitude = height;
            heli->SetVelocity(CVector{ std::cos(heading) * 0.6666667f, std::sin(heading) * 0.6666667f, 0.0f });
            heli->vehicleFlags.bNeverUseSmallerRemovalRange = true;
            heli->m_nExtendedRemovalRange = 255;
            heli->m_autoPilot.m_nCruiseSpeed = 50;
            TurnEngineOn(heli);

            if (CGeneral::GetRandomNumber() % 2 == 0) {
                const auto& heliPos = heli->GetPosition();
                if (auto* const target = CWorld::FindUnsuspectingTargetCar(heliPos, FindPlayerCoors())) {
                    heli->m_autoPilot.m_TargetEntity = target;
                    heli->m_autoPilot.m_nCarMission  = MISSION_HELI_FOLLOW_ENTITY;
                    target->RegisterReference(reinterpret_cast<CEntity**>(&heli->m_autoPilot.m_TargetEntity));
                    heli->m_nHeliFlags |= 2;
                    heli->m_autoPilot.m_nCarCtrlFlags |= 0xA0; // bDoTargetCatchupCheck | bHeliFollowTarget
                }
            } else { // `% 2 == 1`
                const auto& heliPos = heli->GetPosition();
                if (auto* const target = CWorld::FindUnsuspectingTargetPed(heliPos, FindPlayerCoors())) {
                    heli->m_autoPilot.m_nCarMission  = MISSION_HELI_FOLLOW_ENTITY;
                    // NOTSA: The field is declared as `CVehicle*`, but the heli can follow peds too (original code does the same)
                    heli->m_autoPilot.m_TargetEntity = reinterpret_cast<CVehicle*>(target);
                    target->RegisterReference(reinterpret_cast<CEntity**>(&heli->m_autoPilot.m_TargetEntity));
                    heli->m_nHeliFlags |= 2;
                    heli->m_autoPilot.m_nCarCtrlFlags |= 0xA0; // bDoTargetCatchupCheck | bHeliFollowTarget

                    auto* const group = CPedGroups::GetPedsGroup(target);
                    if (!group || group->m_bIsMissionGroup) {
                        if (target->IsPlayer() || target->IsCreatedByMission() || target->m_nPedType == PED_TYPE_COP) {
                            FinishGeneration();
                            return;
                        }
                        CEventDanger event{ heli, 200.0f };
                        event.m_TaskId = TASK_COMPLEX_SMART_FLEE_ENTITY; // 0x38F
                        target->GetEventGroup().Add(&event, false);
                    } else {
                        CEventDanger event{ heli, 200.0f };
                        event.m_TaskId = TASK_GROUP_FLEE_THREAT; // 0x5E1
                        group->m_groupIntelligence.AddEvent(&event);
                    }
                }
            }
        } else {
            const bool isBigPlane = GenPlane_ModelIndex == MODEL_AT400 || GenPlane_ModelIndex == MODEL_ANDROM;

            CVector coors, targetCoors;
            float   heading, height;
            FindPlaneCreationCoors(&coors, &targetCoors, &heading, &height, isBigPlane);

            // BUG: Original code doesn't check for allocation failure
            auto* const plane = new CPlane(GenPlane_ModelIndex, RANDOM_VEHICLE);
            plane->SetPosn(coors);
            plane->m_planeCreationHeading = heading;
            SetHeadingFast(plane, heading);
            CCarCtrl::JoinCarWithRoadSystem(plane);
            plane->SetStatus(STATUS_PHYSICS);
            CWorld::Add(plane);

            plane->m_planeHeading     = heading;
            plane->m_minAltitude      = height;
            plane->m_autoPilot.m_nCarMission = MISSION_PLANE_FLYINDIRECTION;
            plane->m_planeHeadingPrev = heading;
            plane->m_maxAltitude      = height;
            plane->SetVelocity(CVector{ std::cos(heading) * 0.6666667f, std::sin(heading) * 0.6666667f, 0.0f });
            plane->vehicleFlags.bNeverUseSmallerRemovalRange = true;

            if (!isBigPlane && GenPlane_ModelIndex != MODEL_HYDRA) {
                plane->m_nExtendedRemovalRange = 150;
            } else {
                plane->m_nExtendedRemovalRange = 360;
                if (isBigPlane) {
                    plane->m_bUsesCollision = false;
                }
            }

            if (GenPlane_ModelIndex == MODEL_HYDRA) {
                plane->m_autoPilot.m_nCarMission = MISSION_PLANE_ATTACK_PLAYER_POLICE;
                plane->m_wMiscComponentAngle = 0;
            }

            plane->SetGearUp();
            TurnEngineOn(plane);
            CStreaming::SetModelIsDeletable(GenPlane_ModelIndex);
            CStreaming::SetModelTxdIsDeletable(GenPlane_ModelIndex);
        }

        FinishGeneration();
        return;
    }

    // Should we generate a plane? (Status == 0)

    if (CGame::currArea != AREA_CODE_NORMAL_WORLD || numPlanesAndHelis > 4 || AreWeInNoPlaneZone() || CCutsceneMgr::ms_cutsceneProcessing) {
        GenPlane_LastTimeGenerated = CTimer::GetTimeInMS();
        return;
    }

    const auto wantedLevel = (uint32)FindPlayerWanted()->GetWantedLevel();
    if (const auto* const playerVeh = FindPlayerVehicle()) {
        if (wantedLevel > 3 && (playerVeh->IsSubPlane() || playerVeh->IsSubHeli())
            && playerVeh->m_nModelIndex != MODEL_AT400
            && playerVeh->m_nModelIndex != MODEL_ANDROM
            && CTimer::GetTimeInMS() > GenPlane_LastTimeGenerated + 60'000
        ) {
            // Count Hydras that are currently attacking the player
            int32 numAttackingPlanes = 0;
            for (auto i = GetVehiclePool()->GetSize(); i; i--) {
                if (const auto* const veh = GetVehiclePool()->GetAt(i - 1)) {
                    if (veh->m_autoPilot.m_nCarMission == MISSION_PLANE_ATTACK_PLAYER_POLICE) {
                        numAttackingPlanes++;
                    }
                }
            }
            if (numAttackingPlanes < 1 || (numAttackingPlanes < 2 && wantedLevel > 4)) {
                GenPlane_ModelIndex = MODEL_HYDRA;
                GenPlane_Status     = 1;
            }
        }
    }

    if (numPlanesAndHelis > 0) {
        return;
    }
    if (GenPlane_Status != 0) {
        return;
    }

    const auto IsPlayerFlying = []() {
        const auto* const veh = FindPlayerVehicle();
        return veh && (veh->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI || veh->GetVehicleAppearance() == VEHICLE_APPEARANCE_PLANE);
    };

    const auto IsWeatherOK = []() {
        return CWeather::OldWeatherType != WEATHER_SANDSTORM_DESERT && CWeather::NewWeatherType != WEATHER_SANDSTORM_DESERT;
    };

    if ((CWeather::WeatherRegion == WEATHER_REGION_DEFAULT || CWeather::WeatherRegion == WEATHER_REGION_DESERT) && IsWeatherOK()) {
        if (!IsPlayerFlying() && CTimer::GetTimeInMS() > GenPlane_LastTimeGenerated + 120'000) {
            switch (CGeneral::GetRandomNumber() & 7) {
            case 0:
            case 1:
            case 2:
            case 7:
                GenPlane_ModelIndex = MODEL_RUSTLER;
                break;
            case 3:
            case 4:
                GenPlane_ModelIndex = MODEL_CROPDUST;
                break;
            case 5:
            case 6:
                GenPlane_ModelIndex = MODEL_BEAGLE;
                break;
            }
            GenPlane_Status = 1;
            return;
        }
        if (GenPlane_Status != 0) {
            return;
        }
    }

    if (CPopCycle::m_bCurrentZoneIsGangArea) {
        if (!CTheScripts::IsPlayerOnAMission() && CTimer::GetTimeInMS() > GenPlane_LastTimeGenerated + 350'000) {
            GenPlane_ModelIndex = MODEL_POLMAV;
            GenPlane_Status     = 1;
            return;
        }
        if (GenPlane_Status != 0) {
            return;
        }
    }

    if (CGameLogic::LaRiotsActiveHere()) {
        GenPlane_Status     = 1;
        GenPlane_ModelIndex = MODEL_POLMAV;
        return;
    }

    if (GenPlane_Status == 0
        && CWeather::WeatherRegion != WEATHER_REGION_DEFAULT
        && CWeather::WeatherRegion != WEATHER_REGION_DESERT
        && !CPopCycle::m_bCurrentZoneIsGangArea
        && IsWeatherOK()
        && CTimer::GetTimeInMS() > GenPlane_LastTimeGenerated + 200'000
        && !AreWeInNoBigPlaneZone()
        && !IsPlayerFlying()
    ) {
        const auto rnd = CGeneral::GetRandomNumber();
        GenPlane_ModelIndex = (rnd & 1) == 0 ? MODEL_AT400 : MODEL_ANDROM;
        GenPlane_Status     = 1;
    }
}

// 0x6C9140
bool CPlane::SetUpWheelColModel(CColModel* wheelCol) {
    return false;
}

// 0x6C9150
bool CPlane::BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect) {
    return false;
}

// 0x6C94A0
void CPlane::PreRender() {
    plugin::CallMethod<0x6C94A0, CPlane*>(this);
}

// 0x6CAB70
void CPlane::Render() {
    m_nTimeTillWeNeedThisCar = CTimer::GetTimeInMS() + 3000;
    CVehicle::Render();
}

// 0x6C9260
void CPlane::ProcessControl() {
    if (GetStatus() == STATUS_PLAYER && (m_nModelIndex == MODEL_CROPDUST || m_nModelIndex == MODEL_STUNT)) {
        const auto padNum = m_pDriver && m_pDriver->m_nPedType == PED_TYPE_PLAYER2 ? 1 : 0;
        if (CPad::GetPad(padNum)->IsRightShockPressed()) {
            m_bSmokeEjectorEnabled = !m_bSmokeEjectorEnabled;
        }
    }

    if (m_bSmokeEjectorEnabled) {
        if (!vehicleFlags.bEngineOn || vehicleFlags.bIsDrowning || !m_pDriver) {
            m_bSmokeEjectorEnabled = false;
        }
    }

    if (m_nModelIndex == MODEL_SKIMMER) {
        for (auto i = 0; i < 4; i++) {
            m_damageManager.SetWheelStatus((eCarWheel)i, WHEEL_STATUS_MISSING);
        }
    }

    CAutomobile::ProcessControl();

    m_vehicleAudio.m_DoCountStalls = (int16)field_9A0;
    field_9A0 = 0;

    ProcessWeapons();

    if (m_nModelIndex == MODEL_VORTEX) {
        m_WheelStates.fill(WHEEL_STATE_NORMAL);
    }

    if (m_pSmokeParticle) {
        m_nSmokeTimer += (int32)(CTimer::GetTimeStep() * 0.02f * -1000.0f); // `-CTimer::GetTimeStepInMS()`

        RwMatrix mat;
        m_pSmokeParticle->GetCompositeMatrix(&mat);

        const CVector velocity = -m_vecMoveSpeed * 5.0f;
        const FxPrtMult_c prtMult{ 0.0f, 0.0f, 0.0f, 0.2f, 1.0f, 1.0f, 0.1f };
        g_fx.m_SmokeHuge->AddParticle(mat.pos, velocity, 0.0f,  prtMult, -1.0f, 1.2f, 0.6f, false);
        g_fx.m_SmokeHuge->AddParticle(mat.pos, velocity, 0.05f, prtMult, -1.0f, 1.2f, 0.6f, false);

        if (m_nSmokeTimer < 1 || vehicleFlags.bIsDrowning) {
            m_pSmokeParticle->Kill();
            m_pSmokeParticle = nullptr;
        }
    }
}

// 0x6CADD0
void CPlane::ProcessControlInputs(uint8 playerNum) {
    const auto pad = CPad::GetPad(playerNum);
    const auto& fwd = GetForward();

    // Speed along the forward vector
    const float forwardSpeed = m_vecMoveSpeed.z * fwd.z + m_vecMoveSpeed.y * fwd.y + m_vecMoveSpeed.x * fwd.x;

    // Smoothly move steering values towards the pad input
    const auto UpdateSteeringFromPad = [&] {
        m_nLastControlInput = eControllerType::KEYBOARD;
        m_fSteeringLeftRight = ((float)pad->GetSteeringLeftRight() * (1.0f / 128.0f) - m_fSteeringLeftRight) * CTimer::GetTimeStep() * 0.2f + m_fSteeringLeftRight;
        m_fSteeringUpDown    = ((float)pad->GetSteeringUpDown()    * (1.0f / 128.0f) - m_fSteeringUpDown)    * CTimer::GetTimeStep() * 0.2f + m_fSteeringUpDown;
    };

    //> 0x6CAE01 - Steering (Pitch/Yaw)
    if (!CCamera::m_bUseMouse3rdPerson || !m_bEnableMouseFlying) {
        UpdateSteeringFromPad();
    } else {
        const auto& mouseMoved = CPad::NewMouseControllerState.m_AmountMoved;

        bool bMouseControl = false;
        if (mouseMoved.x == 0.0f && mouseMoved.y == 0.0f) {
            if ((std::abs(m_fSteeringLeftRight) > 0.0f || std::abs(m_fSteeringUpDown) > 0.0f) && m_nLastControlInput == eControllerType::MOUSE) {
                if (pad->GetSteeringLeftRight() == 0 && pad->GetSteeringUpDown() == 0) {
                    bMouseControl = true;
                }
            }
            if (!bMouseControl) {
                if (pad->GetSteeringLeftRight() == 0 && pad->GetSteeringUpDown() == 0 && m_nLastControlInput == eControllerType::MOUSE) {
                    // Nothing to do, keep the current values (jumps to 0x6CB0C7)
                    goto clampSteering;
                }
                UpdateSteeringFromPad();
                goto clampSteering;
            }
        }

        // 0x6CAF6B - Mouse control
        m_nLastControlInput = eControllerType::MOUSE;
        if (pad->NewState.m_bVehicleMouseLook == 0) {
            m_fSteeringLeftRight = mouseMoved.x * 0.0015f + m_fSteeringLeftRight;
            m_fSteeringUpDown    = mouseMoved.y * 0.0015f + m_fSteeringUpDown;
        }
        if (std::abs(m_fSteeringLeftRight) < 0.15f) {
            m_fSteeringLeftRight = std::pow(0.99f, CTimer::GetTimeStep()) * m_fSteeringLeftRight;
        }
        if (std::abs(m_fSteeringUpDown) < 0.15f) {
            m_fSteeringUpDown = std::pow(0.99f, CTimer::GetTimeStep()) * m_fSteeringUpDown;
        }
    }

clampSteering:
    m_fSteeringLeftRight = std::clamp(m_fSteeringLeftRight, -1.0f, 1.0f);
    m_fSteeringUpDown    = std::clamp(m_fSteeringUpDown, -1.0f, 1.0f);

    //> 0x6CB176 - Roll
    float rollChange;
    if (m_nModelIndex == MODEL_VORTEX) {
        rollChange = ((float)pad->GetSteeringLeftRight() * (1.0f / 128.0f) - m_fLeftRightSkid) * CTimer::GetTimeStep();
    } else {
        const float lookRight = (float)pad->GetLookRight();
        if (pad->GetLookLeft()) {
            rollChange = (-1.0f - m_fLeftRightSkid) * CTimer::GetTimeStep();
        } else {
            rollChange = (lookRight - m_fLeftRightSkid) * CTimer::GetTimeStep();
        }
    }
    m_fLeftRightSkid = std::clamp(rollChange * 0.2f + m_fLeftRightSkid, -1.0f, 1.0f);

    //> 0x6CB260 - Steer angle
    const float speedFactor = std::max(1.0f - forwardSpeed, 0.1f);
    m_fSteerAngle = -(m_pHandlingData->m_fSteeringLock * 0.017453292f * m_fLeftRightSkid * speedFactor);

    if (m_nModelIndex == MODEL_VORTEX) {
        if (pad->GetHandBrake()) {
            m_fSteerAngle    *= 1.5f;
            m_fLeftRightSkid *= 1.5f;
        }
    }

    //> 0x6CB2E4 - Acceleration
    m_fAccelerationBreakStatus = (float)(pad->GetAccelerate() - pad->GetBrake()) * (1.0f / 255.0f);

    //> 0x6CB329 - Landing gear
    if (pad->IsRightShockPressed()
        && m_fWheelsSuspensionCompression[0] == 1.0f
        && m_fWheelsSuspensionCompression[1] == 1.0f
        && m_fWheelsSuspensionCompression[2] == 1.0f
        && m_fWheelsSuspensionCompression[3] == 1.0f
        && m_pFlyingHandlingData->m_fGearUpR < 1.0f
        && m_nModelIndex != MODEL_RCBARON
    ) {
        if (m_fLandingGearStatus == 0.0f) { // Gear is down, start retracting it
            for (auto i = 0; i < 4; i++) {
                m_damageManager.SetWheelStatus((eCarWheel)i, WHEEL_STATUS_MISSING);
            }
            m_fLandingGearStatus = CTimer::GetTimeStep() * 0.02f + m_fLandingGearStatus;
        } else if (m_fLandingGearStatus == 1.0f) { // Gear is up, start extending it
            m_fLandingGearStatus = CTimer::GetTimeStep() * 0.02f - 1.0f;
        } else { // In the middle of an animation, reverse it
            m_fLandingGearStatus *= -1.0f;
        }
    } else {
        if (m_fLandingGearStatus < 0.0f) { // Extending
            m_fLandingGearStatus = CTimer::GetTimeStep() * 0.02f + m_fLandingGearStatus;
            if (m_fLandingGearStatus >= 0.0f) {
                SetGearDown(); // This was inlined
            }
        } else if (m_fLandingGearStatus > 0.0f && m_fLandingGearStatus < 1.0f) { // Retracting
            m_fLandingGearStatus = CTimer::GetTimeStep() * 0.02f + m_fLandingGearStatus;
            if (m_fLandingGearStatus >= 1.0f) {
                SetGearUp();
            }
        }
    }

    //> 0x6CB4F3 - Hydra's nozzles
    if (m_nModelIndex == MODEL_HYDRA) {
        if (std::abs((float)pad->GetCarGunUpDown()) > 10.0f) {
            m_wMiscComponentAnglePrev = m_wMiscComponentAngle;

            // NOTE: Original code evaluates this expression twice (same result)
            const auto change = (int16)(int32)((float)pad->GetCarGunUpDown() * CTimer::GetTimeStep() * HARRIER_NOZZLE_ROTATERATE * (1.0f / 128.0f));
            if ((int32)change + (int32)m_wMiscComponentAngle < 0) {
                m_wMiscComponentAngle = 0;
            } else {
                m_wMiscComponentAngle = (uint16)(change + m_wMiscComponentAngle);
            }
            if ((int32)m_wMiscComponentAngle > (int32)(int16)HARRIER_NOZZLE_ROTATE_LIMIT) {
                m_wMiscComponentAngle = HARRIER_NOZZLE_ROTATE_LIMIT;
            }
        }
    }

    //> 0x6CB5EA - Brakes
    vehicleFlags.bIsHandbrakeOn = false;
    m_GasPedal = 0.0f;

    if (forwardSpeed > 0.0f && pad->GetBrake() != 0) {
        if (forwardSpeed > 0.35f) {
            m_BrakePedal = (float)pad->GetBrake() * (1.0f / 510.0f);
        } else {
            m_BrakePedal = (float)pad->GetBrake() * (1.0f / 255.0f);
        }
    } else if (m_vecMoveSpeed.Magnitude() < 0.1f && pad->GetBrake() < 10 && pad->GetAccelerate() < 10) {
        m_BrakePedal = 0.5f;
    } else {
        m_BrakePedal = 0.0f;
    }

    // Player's controls are disabled => stop the plane
    if (pad->DisablePlayerControls) {
        m_BrakePedal = 1.0f;
        vehicleFlags.bIsHandbrakeOn = true;
        m_GasPedal = 0.0f;
        FindPlayerPed()->KeepAreaAroundPlayerClear();

        const auto speed = m_vecMoveSpeed.Magnitude();
        if (speed > 0.28f) {
            const auto scale = 0.28f / speed;
            m_vecMoveSpeed.x = scale * m_vecMoveSpeed.x;
            m_vecMoveSpeed.y = scale * m_vecMoveSpeed.y;
            m_vecMoveSpeed.z = scale * m_vecMoveSpeed.z;
        }
    }
}

// 0x6CB7C0
void CPlane::ProcessFlyingCarStuff() {
    constexpr float RAND_RECIP = 1.0f / 32767.0f; // 0x858C7C
    const auto Rand01 = []() { return (float)CGeneral::GetRandomNumber() * RAND_RECIP; };

    const float timeStep = CTimer::GetTimeStep();
    if (timeStep <= 0.0f) {
        return;
    }

    // Update damage wave counter
    {
        const float waveMax = PLANE_DAMAGE_WAVE_COUNTER_VAR + 1.0f;
        const float waveMin = 1.0f - PLANE_DAMAGE_WAVE_COUNTER_VAR;
        const float wave    = (waveMax - waveMin) * Rand01() + waveMin;
        const auto  timeMS  = (uint32)(int32)(CTimer::GetTimeStep() * 0.02f * 1000.0f); // `GetTimeStepInMS()`
        m_planeDamageWave += (int32)((float)timeMS * wave);
    }

    const float speedFactor = std::min(m_vecMoveSpeed.Magnitude() * 3.0f, 1.0f);

    float roll  = m_fLeftRightSkid;
    float pitch = m_fSteeringUpDown;
    float yaw   = m_fSteeringLeftRight;

    // Damaged plane starts to spin
    if (m_fHealth < 250.0f && GetStatus() == STATUS_PLAYER) {
        m_damageManager.SetAeroplaneCompStatus(PLANE_STATIC_PROP, DAMSTATE_DAMAGED);
        m_damageManager.SetAeroplaneCompStatus(PLANE_MOVING_PROP, DAMSTATE_DAMAGED);

        roll += 0.5f;
        if (std::abs(GetRoll()) < 2.3561945f) {
            yaw += 0.75f;
        }
        if (std::abs(GetRoll()) > 1.5707964f) {
            pitch += 0.5f;
        }
    }

    if (m_nModelIndex != MODEL_RCBARON) {
        for (int32 node = PLANE_STATIC_PROP; node < PLANE_NUM_NODES; node++) { // 12..24
            const int32 status = m_damageManager.GetAeroplaneCompStatus(node);
            if (!m_aCarNodes[node] || status <= 0) {
                continue;
            }

            // Find the bouncing panel of this component
            // BUG: Original code checks 4 panels, but there are only 3 (the 4th one overlaps with `m_swingingChassis`)
            int32 panelIdx = -1;
            const int32 numPanels = notsa::IsFixBugs() ? (int32)m_panels.size() : 4;
            for (int32 i = 0; i < numPanels; i++) {
                if ((int16)(&m_panels[0])[i].m_nFrameId == node) {
                    panelIdx = i;
                    break;
                }
            }

            float particleSize = 0.0f;
            switch (node) {
            case PLANE_STATIC_PROP:
            case PLANE_MOVING_PROP:
            case PLANE_STATIC_PROP2:
            case PLANE_MOVING_PROP2: {
                if (m_fAccelerationBreakStatus > 0.0f) {
                    const float maxVal = PLANE_PROP_DAMAGE_BASE + 1.0f;
                    const float minVal = 1.0f - PLANE_PROP_DAMAGE_BASE;
                    const float rnd    = Rand01();
                    float wave = (float)(uint32)m_planeDamageWave;
                    float period = (float)(uint32)PLANE_PROP_DAMAGE_WAVE_PERIOD;
                    const float sine   = std::sin(wave * TWO_PI / period);
                    m_fAccelerationBreakStatus = ((((maxVal - minVal) * rnd + minVal) * (sine - 1.0f)) * (float)status * (float)status) * timeStep * PLANE_PROP_DAMAGE_MULT + m_fAccelerationBreakStatus;
                }
                particleSize = (float)status * 0.5f;
                break;
            }
            case PLANE_RUDDER: {
                const float s = (float)status;
                m_fLeftRightSkid = (1.0f - s * 0.2f) * m_fLeftRightSkid;
                const float lo = -PLANE_RUDDER_DAMAGE_AMPL;
                const float rnd = Rand01();
                const float s2 = (float)(status * status);
                m_fLeftRightSkid = ((PLANE_RUDDER_DAMAGE_AMPL - lo) * rnd + lo) * s2 * timeStep * speedFactor + m_fLeftRightSkid;
                roll = m_fLeftRightSkid;
                if (panelIdx >= 0) {
                    auto& panel = (&m_panels[0])[panelIdx];
                    const float plo = -PLANE_RUDDER_PANEL_AMPL;
                    const float prnd = Rand01();
                    panel.m_vecPos.y = ((PLANE_RUDDER_PANEL_AMPL - plo) * prnd + plo) * s2 * timeStep * speedFactor + panel.m_vecPos.y;
                    const float t = PLANE_RUDDER_PANEL_MULT * panel.m_vecRotation.y;
                    if (node == PLANE_AILERON_L) { // Never true
                        roll += t;
                    } else {
                        roll -= t;
                    }
                }
                particleSize = s * 0.2f;
                break;
            }
            case PLANE_ELEVATOR_L:
            case PLANE_ELEVATOR_R: {
                const float s = (float)status;
                m_fSteeringUpDown = (1.0f - s * 0.2f) * m_fSteeringUpDown;
                const float lo = -PLANE_ELEVATOR_DAMAGE_AMPL;
                const float rnd = Rand01();
                const float s2 = (float)(status * status);
                m_fSteeringUpDown = ((PLANE_ELEVATOR_DAMAGE_AMPL - lo) * rnd + lo) * s2 * timeStep * speedFactor + m_fSteeringUpDown;
                pitch = m_fSteeringUpDown;
                if (panelIdx >= 0) {
                    auto& panel = (&m_panels[0])[panelIdx];
                    const float plo = -PLANE_ELEVATOR_PANEL_AMPL;
                    const float prnd = Rand01();
                    panel.m_vecPos.y = ((PLANE_ELEVATOR_PANEL_AMPL - plo) * prnd + plo) * s2 * timeStep * speedFactor + panel.m_vecPos.y;
                    const float t = PLANE_ELEVATOR_PANEL_MULT * panel.m_vecRotation.y;
                    if (node == PLANE_AILERON_L) { // Never true
                        pitch += t;
                    } else {
                        pitch -= t;
                    }
                }
                particleSize = s * 0.15f;
                break;
            }
            case PLANE_AILERON_L:
            case PLANE_AILERON_R: {
                const float s = (float)status;
                m_fSteeringLeftRight = (1.0f - s * 0.2f) * m_fSteeringLeftRight;
                const float lo = -PLANE_AILERON_DAMAGE_AMPL;
                const float rnd = Rand01();
                const float s2 = (float)(status * status);
                m_fSteeringLeftRight = ((PLANE_AILERON_DAMAGE_AMPL - lo) * rnd + lo) * s2 * timeStep * speedFactor + m_fSteeringLeftRight;
                yaw = m_fSteeringLeftRight;
                if (panelIdx >= 0) {
                    auto& panel = (&m_panels[0])[panelIdx];
                    const float plo = -PLANE_AILERON_PANEL_AMPL;
                    const float prnd = Rand01();
                    panel.m_vecPos.y = ((PLANE_AILERON_PANEL_AMPL - plo) * prnd + plo) * s2 * timeStep * speedFactor + panel.m_vecPos.y;
                    const float t = PLANE_AILERON_PANEL_MULT * panel.m_vecRotation.y;
                    if (node == PLANE_AILERON_L) {
                        yaw += t;
                    } else {
                        yaw -= t;
                    }
                }
                particleSize = s * 0.25f;
                break;
            }
            default:
                continue;
            }

            // Smoke from damaged component
            if (particleSize > 0.0f && !vehicleFlags.bIsDrowning && (speedFactor > 0.3f || (CGeneral::GetRandomNumber() & 7) == 0)) {
                const CVector& camPos = TheCamera.GetPosition();
                const CVector& pos    = GetPosition();
                const float dx = pos.x - camPos.x;
                const float dy = pos.y - camPos.y;
                const float dz = pos.z - camPos.z;
                if ((dx * dx + dy * dy + dz * dz < 6400.0f || GetStatus() == STATUS_PLAYER) && m_aCarNodes[node]) {
                    CVector particlePos = m_matrix->TransformPoint(RwFrameGetMatrix(m_aCarNodes[node])->pos);

                    FxPrtMult_c prtMult{ 0.0f, 0.0f, 0.0f, 0.2f, 1.0f, 1.0f, particleSize };

                    CVector vel{
                        m_vecMoveSpeed.x * 0.25f * 50.0f,
                        m_vecMoveSpeed.y * 0.25f * 50.0f,
                        m_vecMoveSpeed.z * 0.25f * 50.0f
                    };
                    vel.x = (Rand01() * 0.20000005f + 0.9f) * vel.x;
                    vel.y = (Rand01() * 0.20000005f + 0.9f) * vel.y;
                    vel.z = (Rand01() * 0.20000005f + 0.9f) * vel.z;
                    prtMult.m_fLife = Rand01(); // Overwrites the life set above

                    const float lo = -particleSize;
                    particlePos.x = ((particleSize - lo) * Rand01() + lo) + particlePos.x;
                    particlePos.y = ((particleSize - lo) * Rand01() + lo) + particlePos.y;
                    particlePos.z = ((particleSize - lo) * Rand01() + lo) + particlePos.z;

                    g_fx.m_SmokeHuge->AddParticle(particlePos, vel, 0.0f, prtMult, -1.0f, 1.2f, 0.6f, false);
                }
            }
        }
    }

    //> 0x6CC0EA - Propeller
    const auto status = GetStatus();
    if (status != STATUS_PLAYER && status != STATUS_REMOTE_CONTROLLED && status != STATUS_PHYSICS) {
        const float propDecay = CTimer::GetTimeStep() * 0.001f;
        if (m_fPropSpeed <= propDecay) {
            m_fPropSpeed = 0.0f;
            vehicleFlags.bEngineOn = false;
            return;
        }
        if (m_fPropSpeed > PLANE_STD_PROP_SPEED) {
            m_nFakePhysics = 0;
            m_fPropSpeed = m_fPropSpeed - CTimer::GetTimeStep() * 0.003f;
            return;
        }
        m_nFakePhysics = 0;
        m_fPropSpeed = m_fPropSpeed - propDecay;
        return;
    }

    float targetPropSpeed = PLANE_STD_PROP_SPEED;
    if (m_fAccelerationBreakStatus > 0.0f) {
        targetPropSpeed = (PLANE_MAX_PROP_SPEED - PLANE_STD_PROP_SPEED) * m_fAccelerationBreakStatus + PLANE_STD_PROP_SPEED;
    } else if (m_fAccelerationBreakStatus < 0.0f) {
        targetPropSpeed = (PLANE_STD_PROP_SPEED - PLANE_MIN_PROP_SPEED) * m_fAccelerationBreakStatus + PLANE_STD_PROP_SPEED;
    }

    if (status == STATUS_PLAYER || status == STATUS_REMOTE_CONTROLLED) {
        const auto flightModel = m_nModelIndex != MODEL_RCBARON ? FLIGHT_MODEL_PLANE : FLIGHT_MODEL_BARON;
        if (HeightAboveCeiling(GetPosition().z, flightModel) > 0.0f) {
            const float ceilingFactor = std::max(HeightAboveCeiling(GetPosition().z, flightModel) * 0.02f, 0.0f);
            targetPropSpeed = ceilingFactor * targetPropSpeed;
            m_fAccelerationBreakStatus = std::max(m_fAccelerationBreakStatus - HeightAboveCeiling(GetPosition().z, flightModel) * 0.04f, -1.0f);
        }
    }

    const bool bDrowning = vehicleFlags.bIsDrowning;
    m_fPropSpeed = (targetPropSpeed - m_fPropSpeed) * CTimer::GetTimeStep() * PLANE_ROC_PROP_SPEED + m_fPropSpeed;
    if (bDrowning) {
        m_fPropSpeed = 0.0f;
        m_fAccelerationBreakStatus = 0.0f;
        vehicleFlags.bEngineOn = false;
    }

    if (m_nModelIndex == MODEL_RCBARON) {
        if (!bDrowning && vehicleFlags.bEngineOn) {
            FlyingControl(FLIGHT_MODEL_BARON, roll, pitch, yaw, m_fAccelerationBreakStatus);
        }
        return;
    }

    if (!vehicleFlags.bEngineOn) {
        return;
    }
    if (!(m_fPropSpeed > PLANE_MIN_PROP_SPEED) && !(m_vecMoveSpeed.SquaredMagnitude() > 0.05)) {
        return;
    }

    if (m_nModelIndex == MODEL_HYDRA && GetStatus() == STATUS_PLAYER && (int32)m_wMiscComponentAngle >= (int32)(int16)HARRIER_NOZZLE_SWITCH_LIMIT) {
        // Nozzles are down => use Hunter's flying handling
        const auto savedFlyingHandling = m_pFlyingHandlingData;
        m_pFlyingHandlingData = gHandlingDataMgr.GetFlyingPointer(static_cast<uint8>(CModelInfo::GetVehicleModelInfo(MODEL_HUNTER)->m_nHandlingId));
        if (m_fAccelerationBreakStatus > 0.0 || (m_nNumContactWheels < 4 && !physicalFlags.bTouchingWater)) {
            FlyingControl(FLIGHT_MODEL_HELI, roll, pitch, -yaw, PLANE_HYDRA_NOZZLE_ACCEL_MULT * m_fAccelerationBreakStatus);
        }
        m_pFlyingHandlingData = savedFlyingHandling;
        return;
    }

    FlyingControl(FLIGHT_MODEL_PLANE, roll, pitch, yaw, m_fAccelerationBreakStatus);
}
