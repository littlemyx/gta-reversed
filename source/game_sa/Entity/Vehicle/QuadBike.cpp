#include "StdInc.h"

#include "QuadBike.h"
#include "VehicleRecording.h"
#include "ControllerConfigManager.h"

NOTSA_GLOBAL(bDoQuadDamping, 0x8D3450, (bool), { true }); // true
NOTSA_GLOBAL(QUAD_HBSTEER_ANIM_MULT, 0x8D3454, (float), { -0.4f }); // -0.4f
NOTSA_GLOBAL(vecQuadResistance, 0x8D3458, (CVector), { 0.995f, 0.995f, 1.0f }); // { 0.995f, 0.995f, 1.0f }

namespace {
// Same as in `Bike.cpp` - replicates the operation order of the original (inlined `CMatrix::Multiply3x3`)

// 0x59C790
CVector TransformVectorOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return CVector{
        (u.x * v.z + f.x * v.y) + r.x * v.x,
        (u.y * v.z + r.y * v.x) + f.y * v.y,
        (u.z * v.z + r.z * v.x) + f.z * v.y
    };
}

// 0x59C810
CVector InverseTransformVectorOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return CVector{
        (r.y * v.y + r.z * v.z) + v.x * r.x,
        (f.y * v.y + f.x * v.x) + f.z * v.z,
        (u.y * v.y + u.x * v.x) + u.z * v.z
    };
}

// Constants used by `ProcessControl`
constexpr float QUAD_PITCH_DAMP_COEF      = 0.995f; // 0x871ABC - Also used as the pitch damping when the front wheels are in the air
constexpr float QUAD_PITCH_DAMP_COEF_AIR  = 0.5f;   // 0x871AC0 - Used if neither wheelie, nor stoppie
constexpr float QUAD_ROLL_DAMP_COEF       = 1.0f;   // 0x871AC4
constexpr float QUAD_ROLL_DAMP_COEF_AIR   = 1.0f;   // 0x871AC8
}

void CQuadBike::InjectHooks() {
    RH_ScopedVirtualClass(CQuadBike, 0x871ae8, 71);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(Constructor, 0x6CE370);
    RH_ScopedVMTInstall(Fix, 0x6CE2B0);
    RH_ScopedVMTInstall(GetRideAnimData, 0x6CDC90);
    RH_ScopedVMTInstall(PreRender, 0x6CEAD0);
    RH_ScopedVMTInstall(ProcessAI, 0x6CE460);
    RH_ScopedVMTInstall(ProcessControl, 0x6CDCC0);
    RH_ScopedVMTInstall(ProcessControlInputs, 0x6CE020);
    RH_ScopedVMTInstall(ProcessDrivingAnims, 0x6CE280);
    RH_ScopedVMTInstall(ProcessSuspension, 0x6CE270);
    RH_ScopedVMTInstall(ResetSuspension, 0x6CDCB0);
    RH_ScopedVMTInstall(SetupDamageAfterLoad, 0x6CE340);
    RH_ScopedVMTInstall(SetupSuspensionLines, 0x6CDCA0);
}

// 0x6CE370
CQuadBike::CQuadBike(int32 modelIndex, eVehicleCreatedBy createdBy) :
    CAutomobile(modelIndex, createdBy, false)
{
    m_pHandling = gHandlingDataMgr.GetBikeHandlingPointer(GetVehicleModelInfo()->m_nHandlingId);
    m_nVehicleSubType = VEHICLE_TYPE_QUAD;

    { // unused
        field_9A8[0] = 1.f;
        field_9A8[1] = 0.f;
        field_9A8[2] = 0.f;
        field_9A8[3] = 0.0f; // see SetupSuspensionLines
    }

    CQuadBike::SetupSuspensionLines();

    m_nQuadFlags &= ~1u; // useless
    m_fSteerAngle = 0.0f;
}

CQuadBike* CQuadBike::Constructor(int32 modelIndex, eVehicleCreatedBy createdBy) {
    this->CQuadBike::CQuadBike(modelIndex, createdBy);
    return this;
}

// 0x6CE2B0
void CQuadBike::Fix() {
    m_damageManager.ResetDamageStatus(); // 0x6CE2BC
    for (auto i = 2; i < 6; i++) { // from DOOR_LEFT_FRONT to MAX_DOORS
        m_damageManager.SetDoorStatus(static_cast<eDoors>(i), eDoorStatus::DAMSTATE_NOTPRESENT);
    }
    vehicleFlags.bIsDamaged = false;
    RpClumpForAllAtomics(GetRpClump(), CVehicleModelInfo::HideAllComponentsAtomicCB, (void*)2); // TODO Use RpAtomicVisibility::VISIBILITY_DAM instead of `2` here
    for (auto i = 0; i < 4; i++) {
        m_damageManager.SetWheelStatus((eCarWheel)i, eCarWheelStatus::WHEEL_STATUS_OK);
    }
}

// 0x6CDC90
CRideAnimData* CQuadBike::GetRideAnimData() {
    return &m_sRideAnimData;
}

// 0x6CEAD0
void CQuadBike::PreRender() {
    CAutomobile::PreRender();

    auto mi = GetVehicleModelInfo();
    CVector wheelPos{};
    mi->GetWheelPosn(CAR_WHEEL_REAR_LEFT, wheelPos, false);
    SetTransmissionRotation(
        m_aCarNodes[QUAD_REAR_AXLE],
        m_wheelPosition[CAR_WHEEL_REAR_LEFT],
        m_wheelPosition[CAR_WHEEL_REAR_RIGHT],
        wheelPos,
        false
    );

    CVector wheelFrontLeftPos{};
    mi->GetWheelPosn(CAR_WHEEL_FRONT_LEFT, wheelFrontLeftPos, false);

    // Original code saves position of each matrix (because calls to SetRotation set the pos. to 0), then restores it
    // We just use SetRotateYOnly which doesn't modify the position

    CMatrix mat;

    if (m_aCarNodes[QUAD_SUSPENSION_LF]) {
        mat.Attach(RwFrameGetMatrix(m_aCarNodes[QUAD_SUSPENSION_LF]), false);
        mat.SetRotateYOnly(x87::atan2(m_wheelPosition[CAR_WHEEL_FRONT_LEFT] - wheelFrontLeftPos.z, fabs(wheelFrontLeftPos.x)));
        mat.UpdateRW();
    }

    if (m_aCarNodes[QUAD_SUSPENSION_RF]) {
        mat.Attach(RwFrameGetMatrix(m_aCarNodes[QUAD_SUSPENSION_RF]), false);
        mat.SetRotateYOnly(-x87::atan2(m_wheelPosition[eCarWheel::CAR_WHEEL_FRONT_RIGHT] - wheelFrontLeftPos.z, fabs(wheelFrontLeftPos.x)));
        mat.UpdateRW();
    }

    if (m_aCarNodes[QUAD_HANDLEBARS]) {
        mat.Attach(RwFrameGetMatrix(m_aCarNodes[QUAD_HANDLEBARS]), false);
        mat.SetRotateZOnly(QUAD_HBSTEER_ANIM_MULT * m_sRideAnimData.AnimLeanLeft);
        mat.UpdateRW();
    }
}

// 0x6CDCC0
void CQuadBike::ProcessControl() {
    if (GetStatus() == STATUS_PLAYER && bDoQuadDamping) {
        // Damp the angular velocity of the quad (and keep it from tipping over)
        const auto& mat = GetMatrix();

        const auto turnLocal = InverseTransformVectorOriginal(mat, m_vecTurnSpeed);

        float pitchCoef = QUAD_PITCH_DAMP_COEF;
        float rollCoef  = QUAD_ROLL_DAMP_COEF;

        x87d pitchDamp = vecQuadResistance.x; // Note: the original keeps this in the FPU (extended precision)
        float  rollDamp  = vecQuadResistance.y;

        const auto wheelRatios = m_fWheelsSuspensionCompression;
        if (wheelRatios[0] == 1.0f && wheelRatios[2] == 1.0f) { // Front wheels in the air => wheelie
            if ((wheelRatios[1] < 1.0f || wheelRatios[3] < 1.0f) && GetForward().z > 0.0f) {
                const auto error = std::fabs(m_pHandling->m_fWheelieAng - GetForward().z) * 0.25f;
                pitchDamp = (x87d)vecQuadResistance.x - (x87d)std::min(error, 0.07f);
            } else {
                pitchDamp = QUAD_PITCH_DAMP_COEF;
                rollCoef  = QUAD_ROLL_DAMP_COEF_AIR;
            }
        } else if (m_WheelCounts[1] == 1.0f && m_WheelCounts[3] == 1.0f) { // Front wheels on the ground, rear ones in the air => stoppie
            if (GetForward().z < 0.0f) {
                const auto error = std::fabs(m_pHandling->m_fStoppieAng - GetForward().z) * 0.3f;
                pitchDamp = ((x87d)std::min(error, 0.1f) + (x87d)0.9f) * (x87d)vecQuadResistance.x;
            }
        } else {
            pitchCoef = QUAD_PITCH_DAMP_COEF_AIR;
        }

        const auto pitchDampF = (float)(pitchDamp / ((x87d)turnLocal.x * (x87d)turnLocal.x * (x87d)pitchCoef + (x87d)1.0));
        const auto rollDampF  = (float)((x87d)rollDamp / ((x87d)turnLocal.y * (x87d)turnLocal.y * (x87d)rollCoef + (x87d)1.0));

        const auto pitchFactor = (float)x87::pow((double)pitchDampF, (double)CTimer::GetTimeStep());
        const auto rollFactor  = x87::pow((double)rollDampF, (double)CTimer::GetTimeStep());

        const auto pitchDelta = (float)((x87d)turnLocal.x * (x87d)pitchFactor - (x87d)turnLocal.x);
        const auto rollDelta  = X87R((double)turnLocal.y * rollFactor) - (x87d)turnLocal.y; // Stays in the FPU

        const auto turnMass = m_fTurnMass;
        const auto up       = GetUp();

        {
            const auto fx = (float)((x87d)up.x * -(x87d)1.0 * rollDelta);
            const auto fz = (float)((x87d)(float)((x87d)up.z * -(x87d)1.0) * rollDelta);
            ApplyTurnForce(
                CVector{
                    (float)((x87d)fx * (x87d)turnMass),
                    (float)((x87d)up.y * -(x87d)1.0 * rollDelta * (x87d)turnMass),
                    (float)((x87d)fz * (x87d)turnMass)
                },
                TransformVectorOriginal(mat, m_vecCentreOfMass) + GetRight()
            );
        }
        {
            const auto fz = (float)((x87d)pitchDelta * (x87d)up.z);
            ApplyTurnForce(
                CVector{
                    (float)((x87d)pitchDelta * (x87d)up.x * (x87d)turnMass),
                    (float)((x87d)pitchDelta * (x87d)up.y * (x87d)turnMass),
                    (float)((x87d)fz * (x87d)turnMass)
                },
                TransformVectorOriginal(mat, m_vecCentreOfMass) + GetForward()
            );
        }
    }

    CAutomobile::ProcessControl(); // 0x6B1880
}

// 0x6CE460
bool CQuadBike::ProcessAI(uint32& extraHandlingFlags) {
    if (GetStatus() != STATUS_PLAYER) {
        return CAutomobile::ProcessAI(extraHandlingFlags);
    }

    GetColModel(); // Called, but not used?

    m_autoPilot.carCtrlFlags.bHonkAtCar = false;
    m_autoPilot.carCtrlFlags.bHonkAtPed = false;
    const auto recID = m_autoPilot.m_vehicleRecordingId;
    if (recID >= 0 && !CVehicleRecording::bUseCarAI[recID]) {
        return false;
    }
    m_vecCentreOfMass = m_pHandlingData->m_vecCentreOfMass;
    if (m_pDriver && m_pDriver->IsPlayer()) {
        PruneReferences();
        if (m_pDriver->m_nPedState == PEDSTATE_ARRESTED ||
            m_pDriver->GetTaskManager().HasAnyOf<TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN, TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT, TASK_COMPLEX_CAR_QUICK_BE_DRAGGED_OUT>()
        ) {
            vehicleFlags.bIsHandbrakeOn = true;
            m_BrakePedal = 1.0f;
            m_GasPedal = 0.0f;
        } else {
            ProcessControlInputs((uint8)m_pDriver->m_nPedType);
            CPad* pad = m_pDriver->AsPlayer()->GetPadFromPlayer();

            float fTurnForcePerTimeStep = 0.0f;
            const float fLeanDirection = DotProduct(m_vecTurnSpeed, m_matrix->GetRight());
            if (m_sRideAnimData.LeanFwd >= 0.0f || fLeanDirection >= m_pHandling->m_fLeanBakCOM) {
                // Lean forward
                if (m_sRideAnimData.LeanFwd > 0.0f) {
                    if (fLeanDirection > -m_pHandling->m_fLeanFwdCOM) {
                        if (m_nNumContactWheels) {
                            if (m_BrakePedal > 0.0f) {
                                fTurnForcePerTimeStep =
                                    m_pHandling->m_fLeanFwdForce * m_fTurnMass * m_sRideAnimData.LeanFwd * std::min(0.1f, m_vecMoveSpeed.Magnitude());
                            }
                        } else {
                            fTurnForcePerTimeStep = m_fTurnMass * m_sRideAnimData.LeanFwd * 0.0015f;
                        }
                        const float fTurnForce = -CTimer::GetTimeStep() * fTurnForcePerTimeStep;
                        ApplyTurnForce(m_matrix->GetUp() * fTurnForce, m_vecCentreOfMass + m_matrix->GetForward());
                    }
                }
            } else {
                // Lean back
                if (m_nNumContactWheels) {
                    if (m_BrakePedal == 0.0f && !vehicleFlags.bIsHandbrakeOn) {
                        fTurnForcePerTimeStep =
                            m_pHandling->m_fLeanBakForce * m_fTurnMass * m_sRideAnimData.LeanFwd * std::min(0.1f, m_vecMoveSpeed.Magnitude());
                    }
                } else {
                    fTurnForcePerTimeStep = m_fTurnMass * m_sRideAnimData.LeanFwd * 0.0015f;
                }
                const float fTurnForce = -CTimer::GetTimeStep() * fTurnForcePerTimeStep;
                ApplyTurnForce(m_matrix->GetUp() * fTurnForce, m_vecCentreOfMass + m_matrix->GetForward());
            }

            // 0x6CE7D9: only when ALL four wheels are in the air (the exe compares each wheel ratio with 1.0)
            if (m_fWheelsSuspensionCompression[0] == 1.0f && m_fWheelsSuspensionCompression[1] == 1.0f &&
                m_fWheelsSuspensionCompression[2] == 1.0f && m_fWheelsSuspensionCompression[3] == 1.0f && pad) {
                float steeringLeftRightProgress = (float)pad->GetSteeringLeftRight() / 128.0f;
                if (CCamera::m_bUseMouse3rdPerson && fabs(steeringLeftRightProgress) < 0.05f) {
                    steeringLeftRightProgress = std::clamp(CPad::NewMouseControllerState.m_AmountMoved.x * ExeRecip(100.0f), -1.5f, 1.5f);
                }
                constexpr float AIR_TURN_SPEED_LIMIT = 0.03f; // 0x871AD0
                if (vehicleFlags.bIsHandbrakeOn) {
                    // 0x6CE88D: yaw - the force is applied at the front
                    const float fTurnSpeed_Dot_MatUp = DotProduct(m_vecTurnSpeed, m_matrix->GetUp());
                    if (fTurnSpeed_Dot_MatUp < AIR_TURN_SPEED_LIMIT && steeringLeftRightProgress < 0.0f ||
                        fTurnSpeed_Dot_MatUp > -AIR_TURN_SPEED_LIMIT && steeringLeftRightProgress > 0.0f)
                    {
                        const float fTurnForce = CTimer::GetTimeStep() * m_fTurnMass * 0.0015f * steeringLeftRightProgress;
                        ApplyTurnForce(m_matrix->GetRight() * fTurnForce, m_vecCentreOfMass + m_matrix->GetForward());
                    }
                } else if (!pad->GetAccelerate()) { // 0x6CE95F: `jne skip` - the roll assist works only while NOT accelerating
                    const float fTurnSpeed_Dot_MatFwd = DotProduct(m_vecTurnSpeed, m_matrix->GetForward());
                    if (fTurnSpeed_Dot_MatFwd < AIR_TURN_SPEED_LIMIT && steeringLeftRightProgress < 0.0f ||
                        fTurnSpeed_Dot_MatFwd > -AIR_TURN_SPEED_LIMIT && steeringLeftRightProgress > 0.0f)
                    {
                        const float fTurnForce = CTimer::GetTimeStep() * m_fTurnMass * 0.0015f * steeringLeftRightProgress;
                        ApplyTurnForce(m_matrix->GetRight() * fTurnForce, m_vecCentreOfMass + m_matrix->GetUp());
                    }
                }
            }
            const auto fValue = x87::pow(X87W((x87d)m_pHandling->m_fDesLean), X87W((x87d)CTimer::GetTimeStep())); // TODO: Name this variable properly; exe 0x6CEA48: kept unrounded, every op below rounds
            const float fSteerTerm = m_pHandling->m_fFullAnimLean * m_fSteerAngle / DegreesToRadians(m_pHandlingData->m_fSteeringLock);
            m_sRideAnimData.LeanAngle = (float)(X87R(X87R(fValue * (x87d)m_sRideAnimData.LeanAngle) - X87R((x87d)fSteerTerm * X87R((x87d)1.0 - fValue))));

            DoDriveByShootings();

            if (CCheat::IsActive(CHEAT_CARS_ON_WATER)) {
                Remove();
            }
        }

    }
    return false;
}

// 0x6CE020
void CQuadBike::ProcessControlInputs(uint8 playerNum) {
    CAutomobile::ProcessControlInputs(playerNum);

    CPad* pad = CPad::GetPad(playerNum);
    if (!CCamera::m_bUseMouse3rdPerson || !m_bEnableMouseSteering) {
        m_sRideAnimData.LeanFwd += (float(-pad->GetSteeringUpDown()) / 128.0f - m_sRideAnimData.LeanFwd) * CTimer::GetTimeStep() * ExeRecip(5.0f);
    } else {
        if (CPad::NewMouseControllerState.m_AmountMoved.IsZero() &&
            (std::fabs(m_fRawSteerAngle) <= 0.0f || m_nLastControlInput != eControllerType::MOUSE || pad->IsSteeringInAnyDirection())
        ) {
            if (pad->GetSteeringUpDown() || m_nLastControlInput != eControllerType::MOUSE) {
                m_nLastControlInput = eControllerType::KEYBOARD;
                m_sRideAnimData.LeanFwd += (float(-pad->GetSteeringUpDown()) / 128.0f - m_sRideAnimData.LeanFwd) * CTimer::GetTimeStep() * ExeRecip(5.0f);
            }
        } else {
            m_nLastControlInput = eControllerType::MOUSE;
            if (!pad->NewState.m_bVehicleMouseLook) {
                m_sRideAnimData.LeanFwd += CPad::NewMouseControllerState.m_AmountMoved.y * -0.0035f; // 0x6CE15A: constant at 0x871AD8 is -0.0035
            }
            if (pad->NewState.m_bVehicleMouseLook || std::fabs(m_sRideAnimData.LeanFwd) < 0.35f) {
                m_sRideAnimData.LeanFwd = (float)(X87R(x87::pow(X87W((x87d)0.98f), X87W((x87d)CTimer::GetTimeStep())) * (x87d)m_sRideAnimData.LeanFwd)); // exe 0x6CE197: pow kept unrounded, the multiply rounds
            }
        }
    }
    m_sRideAnimData.LeanFwd = std::clamp(m_sRideAnimData.LeanFwd, -1.0f, 1.0f);
    if (pad->DisablePlayerControls) {
        m_sRideAnimData.LeanFwd = 0.0f;
    }
}

// 0x6CE280
void CQuadBike::ProcessDrivingAnims(CPed* driver, bool blend) {
    if (!m_bOffscreen) { // see CBike::ProcessDrivingAnims
        CBike::ProcessRiderAnims(driver, this, &m_sRideAnimData, m_pHandling, 0);
    }
}

// 0x6CE270
void CQuadBike::ProcessSuspension() {
    CAutomobile::ProcessSuspension();
}

// 0x6CDCB0
void CQuadBike::ResetSuspension() {
    CAutomobile::ResetSuspension();
}

// 0x6CE340
void CQuadBike::SetupDamageAfterLoad() {
    if (m_aCarNodes[QUAD_BODY_FRONT]) {
        CAutomobile::SetBumperDamage(FRONT_BUMPER, false);
    }
    if (m_aCarNodes[QUAD_BODY_REAR]) {
        CAutomobile::SetDoorDamage(DOOR_BONNET, false);
    }
}

// 0x6CDCA0
void CQuadBike::SetupSuspensionLines() {
    field_9A8[0] = 1.0f;
    CAutomobile::SetupSuspensionLines();
}
