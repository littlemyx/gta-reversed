/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"
#include <numbers>
#include "WindModifiers.h"
#include "Shadows.h"
#include "CarCtrl.h"
#include "CullZones.h"
#include "InterestingEvents.h"
#include "Ropes.h"
#include "FireManager.h"

void CHeli::InjectHooks() {
    RH_ScopedVirtualClass(CHeli, 0x871680, 71);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(InitHelis, 0x6C4560);
    RH_ScopedInstall(AddHeliSearchLight, 0x6C45B0);
    RH_ScopedInstall(Pre_SearchLightCone, 0x6C4650);
    RH_ScopedInstall(Post_SearchLightCone, 0x6C46E0);
    RH_ScopedInstall(SwitchPoliceHelis, 0x6C4800);
    RH_ScopedInstall(RenderAllHeliSearchLights, 0x6C7C50);
    RH_ScopedInstall(TestSniperCollision, 0x6C6890);
    RH_ScopedVMTInstall(Render, 0x6C4400);
    RH_ScopedVMTInstall(Fix, 0x6C4530);
    RH_ScopedVMTInstall(BurstTyre, 0x6C4330);
    RH_ScopedVMTInstall(SetUpWheelColModel, 0x6C4320);
    RH_ScopedVMTInstall(ProcessControlInputs, 0x6C4830);
    RH_ScopedVMTInstall(ProcessFlyingCarStuff, 0x6C4E60);
    RH_ScopedVMTInstall(PreRender, 0x6C5420);
    RH_ScopedVMTInstall(BlowUpCar, 0x6C6D30);
    RH_ScopedVMTInstall(ProcessControl, 0x6C7050);
}

// 0x6C4190
CHeli::CHeli(int32 modelIndex, eVehicleCreatedBy createdBy) : CAutomobile(modelIndex, createdBy, true) {
    m_nVehicleSubType = VEHICLE_TYPE_HELI;

    m_fLeftRightSkid           = 0.0f;
    m_fSteeringUpDown          = 0.0f;
    m_fSteeringLeftRight       = 0.0f;
    m_fAccelerationBreakStatus = 0.0f;

    field_99C = 0;
    m_fRotorZ = 0;
    m_fSecondRotorZ = 0;

    m_fMinAltitude = 10.0f;
    m_fMaxAltitude = 10.0f;

    field_9AC = 10.0f;
    field_9B4 = 0;

    m_nHeliFlags = m_nHeliFlags & 0xFC;
    m_fSearchLightIntensity = 0.0f;
    physicalFlags.bDontCollideWithFlyers = true;

    if (modelIndex == MODEL_HUNTER) {
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        m_doors[DOOR_LEFT_FRONT].Init((3.0f * PI) / 10.0f, 0.0f, DOOR_AXIS_NEG_X, DOOR_AXIS_Y, DOOR_EXTRA_BASED);
    }

    m_nNumSwatOccupants = 4;
    m_aSwatState.fill(0);

    m_nSearchLightTimer = CTimer::GetTimeInMS();

    m_aSearchLightHistoryX.fill(0.0f);
    m_aSearchLightHistoryY.fill(0.0f);

    m_nShootTimer = 0;
    m_nPoliceShoutTimer = CTimer::GetTimeInMS();

    vehicleFlags.bNeverUseSmallerRemovalRange = true; // 0x6C42BD
    m_autoPilot.m_ucHeliTargetDist2 = 10;

    m_ppGunflashFx = nullptr;
    m_nFiringMultiplier = 16;

    field_9B8 = 0;
    m_bSearchLightEnabled = false;
    field_A14 = CGeneral::GetRandomNumberInRange(2.f, 8.f);
}

// 0x6C4340
CHeli::~CHeli() {
    if (m_ppGunflashFx) {
        for (auto i = 0; i < CVehicle::GetPlaneNumGuns(); i++) {
            if (auto& fx = m_ppGunflashFx[i]) {
                fx->Kill();
                g_fxMan.DestroyFxSystem(fx);
            }
        }
        delete[] m_ppGunflashFx;
        m_ppGunflashFx = nullptr;
    }

    m_vehicleAudio.Terminate();
}

// 0x6C4560
void CHeli::InitHelis() {
    std::ranges::fill(pHelis, nullptr);
    for (auto& light : HeliSearchLights) {
        light.Init();
    }
    NumberOfSearchLights = 0;
    bPoliceHelisAllowed = true;
}

// 0x6C45B0
void CHeli::AddHeliSearchLight(const CVector& origin, const CVector& target, float targetRadius, float power, uint32 coronaIndex, uint8 unknownFlag, uint8 drawShadow) {
    auto& light = HeliSearchLights[NumberOfSearchLights];

    light.m_vecOrigin     = origin;
    light.m_vecTarget     = target;
    light.m_fTargetRadius = targetRadius;
    light.m_fPower        = power;
    light.m_nCoronaIndex  = coronaIndex;
    light.field_24        = unknownFlag;
    light.m_bDrawShadow   = drawShadow;

    NumberOfSearchLights += 1;
}

// 0x6C4640
void CHeli::PreRenderAlways() {
    // NOP
}

// 0x6C4650
void CHeli::Pre_SearchLightCone() {
    ZoneScoped;

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,          RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,             RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,            RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,        RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,            RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,            RWRSTATE(rwSHADEMODEGOURAUD));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION,    RWRSTATE(rwALPHATESTFUNCTIONGREATEREQUAL));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(0));
}

// 0x6C46E0
void CHeli::Post_SearchLightCone() {
    ZoneScoped;

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,         RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,          RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,             RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,            RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATECULLMODE,             RWRSTATE(rwCULLMODECULLBACK));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION,    RWRSTATE(rwALPHATESTFUNCTIONGREATER));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(2u));
}

// 0x6C4750
void CHeli::SpecialHeliPreRender() {
    // NOP
}

// 0x6C4760
CVector CHeli::FindSwatPositionRelativeToHeli(int32 swatNumber) {
    CVector result;
    ((void(__thiscall*)(CHeli*, CVector*, int32))0x6C4760)(this, &result, swatNumber);
    return result;

    switch ( swatNumber ) {
    case 0:
        return { -1.2f, -1.0f, -0.5f };
    case 1:
        return { 1.2f,  -1.0f, -0.5f };
    case 2:
        return { -1.2f, 1.0f,  -0.5f };
    case 3:
        return { 1.2f,  1.0f,  -0.5f };
    default:
        return { 0.0f,  0.0f,  0.0f  };
    }
}

// 0x6C4800
void CHeli::SwitchPoliceHelis(bool enable) {
    bPoliceHelisAllowed = enable;
}

// 0x6C58E0
void CHeli::SearchLightCone(int32 coronaIndex,
                            CVector origin,
                            CVector target,
                            float targetRadius,
                            float power,
                            uint8 unknownFlag,
                            uint8 drawShadow,
                            CVector& useless0,
                            CVector& useless1,
                            CVector& useless2,
                            bool a11,
                            float baseRadius,
                            float a13,
                            float a14,
                            float a15
) {
    ((void(__cdecl*)(int32, CVector, CVector, float, float, uint8, uint8, CVector&, CVector&, CVector&, bool, float, float, float, float))0x6C58E0)(coronaIndex, origin, target, targetRadius, power, unknownFlag, drawShadow, useless0, useless1, useless2, a11, baseRadius, a13, a14, a15);
}

// 0x6C6520
CHeli* CHeli::GenerateHeli(CPed* target, bool newsHeli) {
    return ((CHeli * (__cdecl*)(CPed*, bool))0x6C6520)(target, newsHeli);
}

// 0x6C6890
void CHeli::TestSniperCollision(CVector* origin, CVector* target) {
    CVector point = *target - *origin;

    if (point.z >= point.Magnitude() / 2.0f)
        return;

    for (auto& heli : pHelis) {
        if (!heli || heli->physicalFlags.bBulletProof)
            continue;

        const auto mat = (CMatrix*)heli->m_matrix;
        if (CCollision::DistToLine(*origin, *target, mat->TransformPoint({ -0.43f, 1.49f, 1.5f })) < 0.8f) {
            heli->m_fRotationBalance = (float)(CGeneral::GetRandomNumber() < pow(2, 14) - 1) * 0.1f - 0.05f; // 2^14 - 1 = 16383 [-0.05, 0.05]
            heli->BlowUpCar(FindPlayerPed(), false);
            heli->m_nNumSwatOccupants = 0;
        };
    }
}

// 0x6C69C0
bool CHeli::SendDownSwat() {
    return ((bool(__thiscall*)(CHeli*))0x6C69C0)(this);
}

// 0x6C79A0
void CHeli::UpdateHelis() {
    ZoneScoped;

    ((void(__cdecl*)())0x6C79A0)();
}

// 0x6C7C50
void CHeli::RenderAllHeliSearchLights() {
    ZoneScoped;

    for (auto& light : HeliSearchLights) {
        SearchLightCone(
            light.m_nCoronaIndex,
            light.m_vecOrigin,
            light.m_vecTarget,
            light.m_fTargetRadius,
            light.m_fPower,
            light.field_24,
            light.m_bDrawShadow,
            light.m_vecUseless[0],
            light.m_vecUseless[1],
            light.m_vecUseless[2],
            false,
            0.05f,
            0.0f,
            0.0f,
            1.0f
        );
    }
}

// 0x6C6D30
void CHeli::BlowUpCar(CEntity* damager, bool bHideExplosion) {
    if (!vehicleFlags.bCanBeDamaged) {
        return;
    }

    const auto isRCHeli = m_nModelIndex == MODEL_RCRAIDER || m_nModelIndex == MODEL_RCGOBLIN;

    // 0x6C6D42 - Non-player helis crash and burn first
    if (GetStatus() != STATUS_PLAYER && m_autoPilot.m_nCarMission != MISSION_HELI_CRASH_AND_BURN && !isRCHeli) {
        m_autoPilot.m_nCarMission = MISSION_HELI_CRASH_AND_BURN; // Not `SetCarMission`, but the same thing
        m_fHealth = 0.0f;
        return;
    }

    if (damager == FindPlayerPed() || damager == FindPlayerVehicle()) { // 0x6C6D7A
        auto& playerInfo = FindPlayerInfo();
        playerInfo.m_nHavocCaused += 20;
        playerInfo.m_fCurrentChaseValue += 10.0f; // 0x85862C
        CStats::IncrementStat(STAT_COST_OF_PROPERTY_DAMAGED, (float)(rand() % 6000 + 4000));
    }

    if (m_nModelIndex == MODEL_VCNMAV) {
        CWanted::UseNewsHeliInAdditionToPolice = false;
    }

    if (GetStatus() == STATUS_PLAYER) { // 0x6C6DFA
        m_bUsesCollision = false; // `m_nFlags &= 0xFFFFFF7E` (bits 0 and 7)
        m_bIsVisible     = false;
        ResetMoveSpeed();
        ResetTurnSpeed();
    }

    SetStatus(STATUS_WRECKED);
    physicalFlags.bRenderScorched = true;
    m_nTimeWhenBlowedUp = CTimer::GetTimeInMS();
    CVisibilityPlugins::SetClumpForAllAtomicsFlag(GetRpClump(), eAtomicComponentFlag::ATOMIC_PIPE_NO_EXTRA_PASSES); // 0x6C6E3D
    m_damageManager.FuckCarCompletely(false);

    if (!isRCHeli) { // 0x6C6E51
        SetBumperDamage(FRONT_BUMPER, false);
        SetBumperDamage(REAR_BUMPER, false);
        SetDoorDamage(DOOR_BONNET, false);
        SetDoorDamage(DOOR_BOOT, false);
        SetDoorDamage(DOOR_LEFT_FRONT, false);
        SetDoorDamage(DOOR_RIGHT_FRONT, false);
        SetDoorDamage(DOOR_LEFT_REAR, false);
        SetDoorDamage(DOOR_RIGHT_REAR, false);
        SpawnFlyingComponent(CAR_WHEEL_LF, 1);

        // BUG: The original doesn't check if the node exists
        if (notsa::IsFixBugs() ? m_aCarNodes[HELI_WHEEL_LF] != nullptr : true) {
            RpAtomic* atomic = nullptr;
            RwFrameForAllObjects(m_aCarNodes[HELI_WHEEL_LF], GetCurrentAtomicObjectCB, &atomic);
            if (atomic) {
                RpAtomicSetFlags(atomic, 0);
            }
        }
    }

    m_nBombOnBoard = 0; // 0x6C6EEB
    m_fHealth      = 0.0f;
    m_wBombTimer   = 0;

    TheCamera.CamShake(0.4f, GetPosition());
    KillPedsInVehicle();

    m_nOverrideLights          = NO_CAR_LIGHT_OVERRIDE; // 0x6C6F49
    vehicleFlags.bEngineOn     = false;
    vehicleFlags.bLightsOn     = false;
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
        isRCHeli ? EXPLOSION_RC_VEHICLE : EXPLOSION_AIRCRAFT,
        GetPosition(),
        0,
        1,
        -1.0f,
        0
    );
}

// 0x6C4530
void CHeli::Fix() {
    m_damageManager.ResetDamageStatus();
    SetupDamageAfterLoad();
}

// 0x6C4330
bool CHeli::BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect) {
    return false;
}

// 0x6C4320
bool CHeli::SetUpWheelColModel(CColModel* wheelCol) {
    return false;
}

// 0x6C4830
void CHeli::ProcessControlInputs(uint8 playerNum) {
    const auto pad = CPad::GetPad(playerNum);

    m_fAccelerationBreakStatus = (float)((int32)pad->GetAccelerate() - (int32)pad->GetBrake()) * (1.0f / 255.0f); // 0x859A3C

    // 0x6C4A4E / 0x6C4952
    const auto SteerWithPad = [&] {
        m_nLastControlInput  = eControllerType::KEYBOARD;
        m_fSteeringUpDown    = (float)(int32)pad->GetSteeringUpDown() * (1.0f / 128.0f);
        m_fSteeringLeftRight = (float)(-(int32)pad->GetSteeringLeftRight()) * (1.0f / 128.0f);
    };

    if (!CCamera::m_bUseMouse3rdPerson || !m_bEnableMouseFlying) {
        SteerWithPad();
    } else {
        const auto& mouseMoved = CPad::NewMouseControllerState.m_AmountMoved;

        bool useMouse = false; // 0x6C4993
        if (mouseMoved.x != 0.0f || mouseMoved.y != 0.0f) {
            useMouse = true;
        } else if (   (std::abs(m_fSteeringLeftRight) > 0.0f || std::abs(m_fSteeringUpDown) > 0.0f)
                   && m_nLastControlInput == eControllerType::MOUSE
                   && pad->GetSteeringLeftRight() == 0
                   && pad->GetSteeringUpDown() == 0
        ) {
            useMouse = true;
        }

        if (useMouse) {
            m_nLastControlInput = eControllerType::MOUSE;
            if (pad->NewState.m_bVehicleMouseLook == 0) {
                m_fSteeringLeftRight = (float)((double)m_fSteeringLeftRight - (double)mouseMoved.x * (double)0.0025f); // 0x871674
                m_fSteeringUpDown    = (float)((double)mouseMoved.y * (double)0.0025f + (double)m_fSteeringUpDown);
            }
            if (std::abs(m_fSteeringLeftRight) < 0.5f) {
                m_fSteeringLeftRight = (float)(std::pow(0.98, (double)CTimer::GetTimeStep()) * (double)m_fSteeringLeftRight); // 0x87167C
            }
            if (std::abs(m_fSteeringUpDown) < 0.5f) {
                m_fSteeringUpDown = (float)(std::pow(0.98, (double)CTimer::GetTimeStep()) * (double)m_fSteeringUpDown);
            }
        } else if (pad->GetSteeringLeftRight() != 0 || pad->GetSteeringUpDown() != 0 || m_nLastControlInput != eControllerType::MOUSE) { // 0x6C492E
            SteerWithPad();
        } // else: keep the mouse values as they are
    }

    m_fSteeringUpDown    = std::clamp(m_fSteeringUpDown, -1.0f, 1.0f); // 0x6C4A96
    m_fSteeringLeftRight = std::clamp(m_fSteeringLeftRight, -1.0f, 1.0f);

    m_fLeftRightSkid = (float)pad->GetLookRight();
    if (pad->GetLookLeft()) {
        m_fLeftRightSkid = -1.0f;
    }

    // 0x6C4B4F - Horn: levels the helicopter out
    if (pad->GetHorn() && GetUp().z > 0.0f) {
        m_fLeftRightSkid = 0.0f;

        const auto flying = m_pFlyingHandlingData;
        const auto& speed = m_vecMoveSpeed;

        auto pitchDir = CrossProduct(CVector{ 0.0f, 0.0f, 1.0f }, GetRight());
        pitchDir.Normalise();
        const double pitch = ((double)pitchDir.y * speed.y + (double)pitchDir.z * speed.z + (double)pitchDir.x * speed.x) * (double)flying->m_fPitchStab;
        m_fSteeringUpDown = (float)std::clamp(pitch, -2.0, 2.0);

        auto rollDir = CrossProduct(GetForward(), CVector{ 0.0f, 0.0f, 1.0f });
        rollDir.Normalise();
        const double roll = ((double)rollDir.y * speed.y + (double)rollDir.z * speed.z + (double)rollDir.x * speed.x) * (double)flying->m_fRollStab;
        m_fSteeringLeftRight = (float)std::clamp(roll, -2.0, 2.0);
    }

    m_fSteerAngle = 0.0f; // 0x6C4D75
    m_BrakePedal  = 1.0f;
    m_GasPedal    = 0.0f;
    vehicleFlags.bIsHandbrakeOn = false;

    if (pad->DisablePlayerControls) {
        FindPlayerPed()->KeepAreaAroundPlayerClear();

        const double mag = std::sqrt((double)m_vecMoveSpeed.x * m_vecMoveSpeed.x + (double)m_vecMoveSpeed.y * m_vecMoveSpeed.y + (double)m_vecMoveSpeed.z * m_vecMoveSpeed.z);
        if (mag > (double)0.28f) { // 0x871254
            const double scale = (double)0.28f / mag;
            m_vecMoveSpeed.x = (float)(scale * m_vecMoveSpeed.x);
            m_vecMoveSpeed.y = (float)(scale * m_vecMoveSpeed.y);
            m_vecMoveSpeed.z = (float)(scale * m_vecMoveSpeed.z);
        }
    }

    if (m_fHealth < 250.0f) { // 0x6C4E1A
        m_fAccelerationBreakStatus = -0.1f;
        m_fLeftRightSkid = (float)((double)m_fLeftRightSkid + 0.5);
    }
}

// 0x6C4400
void CHeli::Render() {
    auto* mi = GetVehicleModelInfo();
    m_nTimeTillWeNeedThisCar = CTimer::GetTimeInMS() + 3000;
    mi->SetVehicleColour(m_nPrimaryColor, m_nSecondaryColor, m_nTertiaryColor, m_nQuaternaryColor);

    auto staticRotor = m_aCarNodes[HELI_STATIC_ROTOR];
    RpAtomic* data = nullptr;
    if (staticRotor) {
        RwFrameForAllObjects(staticRotor, GetCurrentAtomicObjectCB, &data);
        if (data)
            CVehicle::SetComponentAtomicAlpha(data, 255);
    }

    auto staticRotor2 = m_aCarNodes[HELI_STATIC_ROTOR2];
    data = nullptr;
    if (staticRotor2) {
        RwFrameForAllObjects(staticRotor2, GetCurrentAtomicObjectCB, &data);
        if (data)
            CVehicle::SetComponentAtomicAlpha(data, 255);
    }

    auto movingRotor = m_aCarNodes[HELI_MOVING_ROTOR];
    data = nullptr;
    if (movingRotor) {
        RwFrameForAllObjects(movingRotor, GetCurrentAtomicObjectCB, &data);
        if (data)
            CVehicle::SetComponentAtomicAlpha(data, 0);
    }

    auto movingRotor2 = m_aCarNodes[HELI_MOVING_ROTOR2];
    data = nullptr;
    if (movingRotor2) {
        RwFrameForAllObjects(movingRotor2, GetCurrentAtomicObjectCB, &data);
        if (data)
            CVehicle::SetComponentAtomicAlpha(data, 0);
    }

    CEntity::Render(); // exactly CEntity
}

// 0x6C4550
void CHeli::SetupDamageAfterLoad() {
    vehicleFlags.bIsDamaged = false;
}

// 0x6C4E60
void CHeli::ProcessFlyingCarStuff() {
    const auto isRCHeli = [this] { return m_nModelIndex == MODEL_RCRAIDER || m_nModelIndex == MODEL_RCGOBLIN; };

    const auto status = GetStatus();
    if (status != STATUS_PLAYER && status != STATUS_REMOTE_CONTROLLED && status != STATUS_PHYSICS) {
        if (!m_pHandlingData->m_bIsHeli) { // 0x6C4E93
            return;
        }

        vehicleFlags.bEngineOn = false;

        // Rotor spins down
        const double decay = (double)CTimer::GetTimeStep() * (double)0.00055f; // 0x8717B0
        if (decay < m_fHeliRotorSpeed) {
            m_nFakePhysics = 0;
            m_fHeliRotorSpeed = (float)((double)m_fHeliRotorSpeed - decay);
        } else {
            m_fHeliRotorSpeed = 0.0f;
        }
    } else {
        // 0x6C4EF8 - Rotor spins up
        if (m_fHeliRotorSpeed < 0.22f && !physicalFlags.bSubmergedInWater) { // 0x8717AC
            m_fHeliRotorSpeed += isRCHeli() ? 0.003f : 0.001f; // 0x859CD8, 0x858CDC
        }

        if (m_fHeliRotorSpeed > 0.15f) { // 0x858FCC
            const auto isFloatingOnWater = physicalFlags.bTouchingWater && (m_nModelIndex == MODEL_SEASPAR || m_nModelIndex == MODEL_LEVIATHN);
            if (vehicleFlags.bIsRCVehicle) {
                FlyingControl(FLIGHT_MODEL_RC, m_fLeftRightSkid, m_fSteeringUpDown, m_fSteeringLeftRight, m_fAccelerationBreakStatus);
            } else if (   !(m_nNumContactWheels >= 4 || isFloatingOnWater)
                       || m_fAccelerationBreakStatus > 0.0 // 0x859EF8 (double 0.0)
                       || std::abs(m_vecMoveSpeed.x) > 0.02f // 0x858B38
                       || std::abs(m_vecMoveSpeed.y) > 0.02f
                       || std::abs(m_vecMoveSpeed.z) > 0.02f
            ) {
                FlyingControl(FLIGHT_MODEL_HELI, m_fLeftRightSkid, m_fSteeringUpDown, m_fSteeringLeftRight, m_fAccelerationBreakStatus);
            }
        }

        // 0x6C501D - Rotor blades
        if (m_fHeliRotorSpeed > 0.015f && m_aCarNodes[HELI_STATIC_ROTOR]) { // 0x8717A8
            auto* const rotorFrame = m_aCarNodes[HELI_STATIC_ROTOR];
            CMatrix rotorMat{ &rotorFrame->modelling, false };
            // NOTSA: The original also constructs a second, never used, local CMatrix here

            RpAtomic* atomic = nullptr;
            RwFrameForAllObjects(rotorFrame, GetCurrentAtomicObjectCB, &atomic);
            if (atomic) {
                const float radius = RpAtomicGetBoundingSphere(atomic)->radius;
                if (radius > 0.1f) { // 0x858B1C
                    float damageMult = 1.0f;
                    if (isRCHeli()) {
                        damageMult = 0.9f;
                    } else if (m_nModelIndex == MODEL_SPARROW || m_nModelIndex == MODEL_SEASPAR) {
                        damageMult = 0.8f;
                    } else if (m_nModelIndex == MODEL_HUNTER) {
                        damageMult = 0.5f;
                    }
                    if (GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_REMOTE_CONTROLLED) {
                        DoBladeCollision(rotorMat.GetPosition(), GetMatrix(), -3, radius, damageMult); // 0x6C5135
                    }
                }
            }

            // 0x6C513A - Wind
            const auto statusNow = GetStatus();
            if ((statusNow == STATUS_PLAYER || statusNow == STATUS_PHYSICS) && m_fHeliRotorSpeed > 0.0075f) { // 0x8717A4
                const double power = (double)m_fHeliRotorSpeed * (double)6.666667f; // 0x866FBC
                CWindModifiers::RegisterOne(GetPosition(), 1, 1.0 < power ? 1.0f : (float)power);
            } else if (statusNow == STATUS_SIMPLE) {
                CWindModifiers::RegisterOne(GetPosition(), 1, 1.0f);
            }
        }
    }

    // 0x6C5200 - Blade sound
    if (   !isRCHeli()
        && m_fHeliRotorSpeed < 0.154f // 0x8717A0
        && m_fHeliRotorSpeed > 0.0044f // 0x87179C
        && m_aCarNodes[HELI_STATIC_ROTOR]
    ) {
        const auto& pos    = GetPosition();
        const auto& camPos = TheCamera.GetPosition();

        const float  dz  = camPos.z - pos.z;
        const float  dy  = camPos.y - pos.y;
        const double dxe = (double)camPos.x - (double)pos.x; // Not rounded to float on the x87 stack
        const float  dx  = (float)dxe;

        const double distSq = dxe * dx + (double)dy * dy + (double)dz * dz;
        if (distSq < 400.0 && std::abs((double)m_fPropRotate - (double)m_wheelRotation[1]) > (double)0.5235988f) { // 0x85A700, 0x858F20
            CMatrix rotorMat{ &m_aCarNodes[HELI_STATIC_ROTOR]->modelling, false };
            // NOTSA: The original also constructs a second, never used, local CMatrix here

            const auto& right = rotorMat.GetRight();
            const auto& mat   = GetMatrix();
            const CVector bladeDir{
                (float)((double)mat.GetUp().x * right.z + (double)mat.GetForward().x * right.y + (double)mat.GetRight().x * right.x),
                (float)((double)mat.GetUp().y * right.z + (double)mat.GetRight().y * right.x + (double)mat.GetForward().y * right.y),
                (float)((double)mat.GetUp().z * right.z + (double)mat.GetRight().z * right.x + (double)mat.GetForward().z * right.y),
            };

            const double dist    = std::sqrt((double)(float)distSq);
            const double invDist = 1.0 / (dist < (double)0.01f ? (double)0.01f : dist); // 0x858C58 (clamp: min 0.01)
            const double dot     = (double)bladeDir.z * (invDist * dz) + (double)bladeDir.y * (invDist * dy) + (double)bladeDir.x * (dx * invDist);
            if (std::abs(dot) > (double)0.95f) { // 0x858EF0
                m_vehicleAudio.AddAudioEvent(AE_HELI_BLADE, 0.0f); // 0x6C53CB
                m_fPropRotate = m_wheelRotation[1];
            }
        }
    }
}

// 0x6C5420
void CHeli::PreRender() {
    CVehicle::PreRender(); // 0x6D6480

    const auto mi = GetVehicleModelInfo();
    CMatrix    rotorMat{}; // Re-attached to each of the rotor frames below

    // 0x6C545D - Search light
    if (m_bSearchLightEnabled && m_fSearchLightIntensity > 0.0f && CClock::GetIsTimeInRange(19, 6)) {
        const auto origin = GetMatrix().TransformPoint({ 0.0f, 3.5f, -0.3f }); // 0x59C890
        AddHeliSearchLight(
            origin,
            m_vecSearchLightTarget,
            20.0f,
            m_fSearchLightIntensity,
            reinterpret_cast<uint32>(this) + 11, // Corona index
            1,
            1
        );
    }

    // NOTSA: `GetColModel()` (0x535300) was called here, the result is unused

    // 0x6C5506 - Wheel positions
    if (vehicleFlags.bVehicleColProcessed) {
        DoBurstAndSoftGroundRatios();

        for (auto i = 0; i < 4; i++) {
            const double t = 1.0 - (double)m_aSuspensionSpringLength[i] / (double)m_aSuspensionLineLength[i];
            const float  v = (float)(((double)m_fWheelsSuspensionCompression[i] - t) / (1.0 - t));

            CVector wheelPos;
            mi->GetWheelPosn(i, wheelPos, true);

            double wheelZ = (double)wheelPos.z + (double)m_pHandlingData->m_fSuspensionUpperLimit;
            if (v > 0.0f) {
                wheelZ -= (double)v * (double)m_aSuspensionSpringLength[i];
            }

            const double curZ = m_wheelPosition[i];
            if (!(wheelZ > curZ)) {
                if (!physicalFlags.bAddMovingCollisionSpeed || !handlingFlags.bLowRider) {
                    wheelZ = (wheelZ - curZ) * (double)0.75f + curZ; // 0x858F34
                }
            }
            m_wheelPosition[i] = (float)wheelZ;
        }
    }

    UpdateWheelMatrix(4, 1);
    UpdateWheelMatrix(7, 1);
    UpdateWheelMatrix(2, 1);
    UpdateWheelMatrix(5, 1);

    if (!(m_nModelIndex == MODEL_RCRAIDER || m_nModelIndex == MODEL_RCGOBLIN)) {
        DoHeliDustEffect(1.0f, 1.0f);
    }

    // 0x6C55E4 - Main rotor angle
    constexpr float TWO_PI_F = 2.0f * std::numbers::pi_v<float>; // 0x858CBC
    {
        const auto isBigRotor = m_nModelIndex == MODEL_SPARROW
                             || m_nModelIndex == MODEL_SEASPAR
                             || m_nModelIndex == MODEL_MAVERICK
                             || m_nModelIndex == MODEL_VCNMAV
                             || m_nModelIndex == MODEL_POLMAV;
        const double step = isBigRotor
            ? (double)CTimer::GetTimeStep() * (double)m_fHeliRotorSpeed * (double)1.66f // 0x8D33A0
            : (double)CTimer::GetTimeStep() * (double)m_fHeliRotorSpeed;
        m_fRotorZ = (float)((double)m_fRotorZ - step);
        if (m_fRotorZ < -TWO_PI_F) { // 0x863234
            double angle = (double)m_fRotorZ + (double)TWO_PI_F;
            while (angle < -(double)TWO_PI_F) {
                angle += (double)TWO_PI_F;
            }
            m_fRotorZ = (float)angle;
        }
    }

    // 0x6C568A - Second rotor angle
    {
        double step = (double)CTimer::GetTimeStep() * (double)m_fHeliRotorSpeed;
        if (m_nModelIndex == MODEL_LEVIATHN) {
            step = step + step;
        } else {
            step = step * (double)2.3f; // 0x858F54
        }
        m_fSecondRotorZ = (float)((double)m_fSecondRotorZ - step);
        if (m_fSecondRotorZ > TWO_PI_F) {
            double angle = m_fSecondRotorZ;
            do {
                angle -= (double)TWO_PI_F;
            } while (angle > (double)TWO_PI_F);
            m_fSecondRotorZ = (float)angle;
        }
    }

    // 0x6C56E9 - Apply the rotation to the rotor frames (keeping their position)
    const auto RotateRotor = [&](eHeliNodes node, float angle, bool aroundZ) {
        const auto frame = m_aCarNodes[node];
        if (!frame) {
            return;
        }
        rotorMat.Attach(&frame->modelling, false);
        const auto pos = rotorMat.GetPosition();
        if (aroundZ) {
            rotorMat.SetRotateZ(angle);
        } else {
            rotorMat.SetRotateX(angle);
        }
        rotorMat.GetPosition().x += pos.x;
        rotorMat.GetPosition().y += pos.y;
        rotorMat.GetPosition().z += pos.z;
        rotorMat.UpdateRW();
    };
    RotateRotor(HELI_STATIC_ROTOR,  m_fRotorZ,       true);
    RotateRotor(HELI_MOVING_ROTOR,  m_fRotorZ,       true);
    RotateRotor(HELI_STATIC_ROTOR2, m_fSecondRotorZ, false);
    RotateRotor(HELI_MOVING_ROTOR2, m_fSecondRotorZ, false);

    CShadows::StoreShadowForVehicle(this, VEH_SHD_HELI); // 0x6C589D
}

// 0x6C7050
void CHeli::ProcessControl() {
    CAutomobile::ProcessControl(); // 0x6B1880

    if (!vehicleFlags.bEngineOn && m_pDustParticle) {
        m_pDustParticle->Kill();
        m_pDustParticle       = nullptr;
        m_heliDustFxTimeConst = 0.0f;
    }

    // 0x6C7085 - Toggle the search light
    if (CPad::GetPad(m_pDriver && m_pDriver->m_nPedType == PED_TYPE_PLAYER2 ? 1 : 0)->HornJustDown()) {
        m_bSearchLightEnabled = !m_bSearchLightEnabled;
    }

    bool     searchLightOn = false; // 0x6C70C7
    bool     shootAtTarget = false;
    CEntity* target        = nullptr;

    if (physicalFlags.bRenderScorched || CCullZones::PlayerNoRain()) {
        m_fSearchLightIntensity = 0.0f; // 0x6C77E6
    } else {
        if (   m_autoPilot.m_nCarMission == MISSION_HELI_POLICE_BEHAVIOUR
            && (   !FindPlayerVehicle()
                || (FindPlayerVehicle()->m_nVehicleSubType != VEHICLE_TYPE_HELI && FindPlayerVehicle()->m_nVehicleSubType != VEHICLE_TYPE_PLANE)
            )
        ) {
            searchLightOn = true;
            shootAtTarget = true;
            target        = FindPlayerEntity();
        } else if (m_autoPilot.m_nCarMission == MISSION_HELI_FOLLOW_ENTITY && m_autoPilot.m_TargetEntity && (m_nHeliFlags & 2)) {
            searchLightOn = true;
            target        = m_autoPilot.m_TargetEntity;
        } else if (GetStatus() == STATUS_PLAYER && m_nModelIndex == MODEL_POLMAV && m_bSearchLightEnabled) {
            searchLightOn = true;
        }

        if (physicalFlags.bSubmergedInWater) { // 0x6C7195
            searchLightOn = false;
            shootAtTarget = false;
        }

        m_bSearchLightEnabled = searchLightOn;

        if (searchLightOn) { // 0x6C71AB
            // Position and speed of whatever the light is following
            CVector targetPos;
            CVector targetVel;
            if (target) {
                targetPos = target->GetPosition();
                targetVel = static_cast<CPhysical*>(target)->m_vecMoveSpeed;
            } else {
                // Look at the ground in front of the heli
                const auto up = GetUpVector(); // 0x50E420
                const CVector offset{ up.x * -30.0f, up.y * -30.0f, up.z * -30.0f }; // 0x859CE4

                const auto  fwd = GetForwardVector(); // 0x41CCB0
                const auto& pos = GetPosition();
                targetPos.x = (float)((double)fwd.x * 10.0 + (double)pos.x + (double)offset.x); // 0x85862C
                targetPos.y = (float)((double)(float)(fwd.y * 10.0f) + (double)pos.y + (double)offset.y);
                targetPos.z = (float)((double)(float)((float)(fwd.z * 10.0f) + pos.z) + (double)offset.z);
                targetVel   = m_vecMoveSpeed;
            }

            // 0x6C72AE - Record the (predicted) target position once per second
            const uint32 now = CTimer::GetTimeInMS();
            int32        timeSinceLastRecord = (int32)(now - m_nSearchLightTimer);
            if (timeSinceLastRecord > 1000) {
                const double fx = (double)targetVel.x * 100.0; // 0x858628
                const double fy = (double)targetVel.y * 100.0;

                int32 numRecords = (int32)((uint32)(timeSinceLastRecord - 1001) / 1000u) + 1;
                timeSinceLastRecord -= numRecords * 1000;
                do {
                    for (auto i = (int32)m_aSearchLightHistoryX.size() - 1; i > 0; i--) {
                        m_aSearchLightHistoryX[i] = m_aSearchLightHistoryX[i - 1];
                        m_aSearchLightHistoryY[i] = m_aSearchLightHistoryY[i - 1];
                    }
                    m_nSearchLightTimer += 1000;
                    m_aSearchLightHistoryX[0] = (float)(fx + (double)targetPos.x);
                    m_aSearchLightHistoryY[0] = (float)((double)targetPos.y + fy);
                } while (--numRecords != 0);
            }

            // 0x6C7352 - Interpolate between the recorded positions
            const double blend    = (double)timeSinceLastRecord * (double)0.001f; // 0x858CDC
            const double blendInv = 1.0 - blend;
            m_vecSearchLightTarget.z = targetPos.z;
            const float  curX = (float)(blend * m_aSearchLightHistoryX[1] + blendInv * m_aSearchLightHistoryX[2]);
            m_vecSearchLightTarget.x = curX;
            const double curY = blend * m_aSearchLightHistoryY[1] + blendInv * m_aSearchLightHistoryY[2];
            m_vecSearchLightTarget.y = (float)curY;

            // 0x6C73A8 - Light intensity falls off with the distance
            {
                const auto& pos = GetPosition();
                const double dy   = curY - (double)pos.y;
                const double dx   = (double)curX - (double)pos.x; // Not rounded to float on the x87 stack
                const double dist = std::sqrt(dy * dy + dx * dx);
                if (dist > (double)60.0f) { // 0x858B34
                    m_fSearchLightIntensity = 0.0f;
                } else {
                    const float distF = (float)dist;
                    if (distF < 40.0f) { // 0x858A10
                        m_fSearchLightIntensity = 1.0f;
                    } else {
                        m_fSearchLightIntensity = (float)(1.0 - ((double)distF - (double)40.0f) * (double)0.05f); // 0x858C28
                    }
                }
            }

            const float  dxToTarget = (float)((double)targetPos.x - (double)curX);
            const double dyToTarget = (double)targetPos.y - curY;
            if (m_fSearchLightIntensity < 0.9f || dyToTarget * dyToTarget + (double)dxToTarget * dxToTarget > 49.0) { // 0x858C20, 0x8717C4
                m_nShootTimer             = now;
                m_nTimeForMinigunFiring   = now;
            } else if (now > m_nPoliceShoutTimer) {
                m_nPoliceShoutTimer = (uint32)(rand() & 0xFFF) + 4500 + now;
            }

            // 0x6C74B7 - Police heli shooting at the target
            if (shootAtTarget) {
                int32 interval;
                switch ((uint32)FindPlayerPed()->GetPlayerWanted()->m_WantedLevel) { // 0x6C74E5
                case 0:
                case 1:
                case 2: interval = 999999; break;
                case 3: interval = 10000; break;
                case 4: interval = 5000; break;
                case 5: interval = 3500; break;
                case 6: interval = 2000; break;
                default: interval = std::bit_cast<int32>(dxToTarget); break; // NOTSA: The original uses a leftover stack value here (can't happen, max wanted level is 6)
                }

                if (FindPlayerPed()->GetPlayerWanted()->m_WantedLevel != eWantedLevel::WANTED_CLEAN) {
                    AudioEngine.SayPedless(AE_SPEECH_PED, CTX_GLOBAL_POLICE_HELICOPTER, this, 0, 1.0f, false, false, false); // 0x6C7547
                }

                if (CCullZones::NoPolice()) {
                    interval /= 2;
                }

                if (target != FindPlayerPed()) {
                    interval = 5000;
                }

                if (FindPlayerWanted()->PoliceBackOff()) { // 0x6C7585
                    m_nShootTimer           = CTimer::GetTimeInMS();
                    m_nTimeForMinigunFiring = CTimer::GetTimeInMS();
                } else {
                    const auto origin = GetMatrix().TransformPoint({ 0.0f, 3.5f, -1.0f }); // 0x59C890

                    const uint32 shootTime = m_nShootTimer + (uint32)interval;
                    if (CTimer::GetTimeInMS() > shootTime && CTimer::GetPreviousTimeInMS() <= shootTime) {
                        if (!CWorld::GetIsLineOfSightClear(origin, targetPos, true, false, false, false, false, false, false)) {
                            m_nShootTimer           = CTimer::GetTimeInMS();
                            m_nTimeForMinigunFiring = CTimer::GetTimeInMS();
                        }
                    }

                    if (CTimer::GetTimeInMS() > m_nShootTimer + (uint32)interval && CTimer::GetTimeInMS() > m_nTimeForMinigunFiring) { // 0x6C760F
                        CVector shotTarget = targetPos;
                        shotTarget.x = (float)((double)((rand() & 0xFF) - 0x80) * (double)0.02f + (double)shotTarget.x); // 0x858B38
                        shotTarget.y = (float)((double)((rand() & 0xFF) - 0x80) * (double)0.02f + (double)shotTarget.y);

                        CVector dir{
                            targetPos.x - origin.x,
                            targetPos.y - origin.y,
                            targetPos.z - origin.z,
                        };
                        dir.Normalise();

                        // 3.0f = 0x858B3C
                        const float  dx3f   = (float)((double)dir.x * 3.0);
                        const double dy3    = (double)dir.y * 3.0;
                        const double dz3    = (double)dir.z * 3.0;
                        const float  dz3f   = (float)dz3;
                        shotTarget.x = (float)((double)dx3f + (double)shotTarget.x);
                        shotTarget.y = (float)(dy3 + (double)shotTarget.y);
                        shotTarget.z = (float)((double)shotTarget.z + dz3);

                        const CVector start{
                            (float)((double)dx3f + (double)origin.x),
                            (float)((double)origin.y + dy3),
                            (float)((double)origin.z + (double)dz3f),
                        };

                        FireOneInstantHitRound(start, shotTarget, 20); // 0x6C7773
                        AudioEngine.ReportWeaponEvent(AE_WEAPON_FIRE, WEAPON_M4, this); // 0x6C7788

                        m_nTimeForMinigunFiring = CTimer::GetTimeInMS() + (CGeneral::GetRandomNumberInRange(0.0f, 1.0f) >= 0.15f ? 150u : 400u); // 0x8D33A4
                    }
                }
            }
        }
    }

    // 0x6C77EC - Dropping the SWAT team
    if (m_autoPilot.m_nCarMission == MISSION_HELI_POLICE_BEHAVIOUR && m_nNumSwatOccupants != 0) {
        SendDownSwat();
        g_InterestingEvents.Add(CInterestingEvents::ZELDICK_OCCUPATION, this);
    }

    for (auto i = 0; i < (int32)m_aSwatState.size(); i++) { // 0x6C7824
        auto& state = m_aSwatState[i];
        if (state == 0) {
            continue;
        }

        state--;

        const auto ropeId = reinterpret_cast<uint32>(this) + i; // The rope is identified by `this + i`
        const auto swatOffset = FindSwatPositionRelativeToHeli(i);
        CRopes::RegisterRope(ropeId, 8, GetMatrix().TransformPoint(swatOffset), false, 0, 0, nullptr, 20000);

        if (state == 0) {
            const auto swatOffset2 = FindSwatPositionRelativeToHeli(i);
            const CVector v{ swatOffset2.x * 0.05f, swatOffset2.y * 0.05f, swatOffset2.z * 0.05f }; // 0x858C28
            const auto& mat = GetMatrix();
            // 0x59C790 (Multiply3x3)
            const auto rotated = CVector{
                (float)((double)mat.GetUp().x * v.z + (double)mat.GetForward().x * v.y + (double)mat.GetRight().x * v.x),
                (float)((double)mat.GetUp().y * v.z + (double)mat.GetRight().y * v.x + (double)mat.GetForward().y * v.y),
                0.0f, // z isn't used
            };
            CRopes::SetSpeedOfTopNode(ropeId, rotated);
        }
    }

    UpdateWinch();
    ProcessWeapons();

    if (g_InterestingEvents.m_b1) { // 0xC0B184
        float chance = (float)((double)CTimer::GetTimeStep() * (double)0.02f * (double)0.1f); // 0x858B38, 0x858B1C
        if (shootAtTarget) {
            chance += chance;
        }
        if ((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL < (double)chance) { // 0x858C7C
            g_InterestingEvents.Add(CInterestingEvents::INTERESTING_EVENT_21, this);
        }
    }
}
