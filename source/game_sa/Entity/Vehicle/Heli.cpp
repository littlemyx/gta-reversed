/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"
#include "WindModifiers.h"

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
    plugin::CallMethod<0x6C6D30, CHeli*, CEntity*, uint8>(this, damager, bHideExplosion);
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
    plugin::CallMethod<0x6C5420, CHeli*>(this);
}

// 0x6C7050
void CHeli::ProcessControl() {
    plugin::CallMethod<0x6C7050, CHeli*>(this);
}
