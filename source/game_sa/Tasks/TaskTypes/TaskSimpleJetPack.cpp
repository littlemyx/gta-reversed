#include "StdInc.h"

#include "TaskSimpleJetPack.h"
#include "TaskSimplePlayerOnFoot.h"

constexpr CVector JETPACK_POS_OFFSET   = { 0.1f, 0.08f, 0.0f };
constexpr CVector JETPACK_ROT_AXIS     = { 0.0f, 1.0f, 0.0f };
constexpr CVector JETBALL_OFFSET       = { 0.f, 0.f, -0.15f };

constexpr float MAGIC = 0.008f; // == 1 / 125 

constexpr float THRUST_NOMINAL         = 0.8f;
constexpr float THRUST_FULL            = 0.6f;
constexpr float THRUST_STRAFE          = 0.3f;
constexpr float THRUST_STOP            = 0.5f;
constexpr float THRUST_MAX_ANGLE       = std::bit_cast<float>(0x3FA78D36u); // 0x8D2F48 (75 degrees, NOT 1.309f)
constexpr float THRUST_MOVE_DAMPING    = 0.02f;

constexpr float JETPACK_TURN_RATE      = -0.05f;
constexpr float JETPACK_ANGLE_RATE     = 0.9f;

constexpr float LEG_SWING_MAX_ANGLE    = std::bit_cast<float>(0x3F490FDBu); // 0x8D2F58 (PI / 4, NOT 0.7854f)
constexpr float LEG_SWING_DELTA_V_MULT = -0.2f;
constexpr float LEG_SWING_GRAVITY_MULT = 0.01f;
constexpr float LEG_SWING_DAMP_FRAC    = 0.98f;

void CTaskSimpleJetPack::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleJetPack, 0x8705c4, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x67B4E0);
    RH_ScopedInstall(Destructor, 0x67E600);

    RH_ScopedInstall(RenderJetPack, 0x67F6A0);

    RH_ScopedInstall(ProcessAnims, 0x67B5C0);
    RH_ScopedInstall(DropJetPack, 0x67B660);
    RH_ScopedInstall(ApplyRollAndPitch, 0x67B720);
    RH_ScopedInstall(DoJetPackEffect, 0x67B7F0);
    RH_ScopedInstall(StopJetPackEffect, 0x67BA10);
    RH_ScopedInstall(ProcessControlInput, 0x67E7B0);
    RH_ScopedInstall(ProcessThrust, 0x67EF20);

    RH_ScopedVMTInstall(Clone, 0x67C690);
    RH_ScopedVMTInstall(GetTaskType, 0x67B5B0);
    RH_ScopedVMTInstall(MakeAbortable, 0x67E6E0);
    RH_ScopedVMTInstall(ProcessPed, 0x6801F0);
}

// 0x67B4E0
CTaskSimpleJetPack::CTaskSimpleJetPack(const CVector* targetPos, float cruiseHeight, int32 hoverTime, CEntity* targetEntity) :
    m_HoverTime{ hoverTime },
    m_CruiseHeight{ cruiseHeight },
    m_TargetEntity{ targetEntity }
{
    CEntity::SafeRegisterRef(m_TargetEntity);

    if (targetPos)
        m_TargetPos = *targetPos;
}

CTaskSimpleJetPack::CTaskSimpleJetPack(const CTaskSimpleJetPack& o) :
    CTaskSimpleJetPack{ &o.m_TargetPos, o.m_CruiseHeight, o.m_HoverTime, o.m_TargetEntity }
{
}

// 0x67E600
CTaskSimpleJetPack::~CTaskSimpleJetPack() {
    if (m_JetPackClump) {
        CVisibilityPlugins::GetClumpModelInfo(m_JetPackClump)->RemoveRef();
        if (const auto a = GetFirstAtomic(m_JetPackClump)) {
            if (RpSkinGeometryGetSkin(RpAtomicGetGeometry(a))) {
                RpClumpForAllAtomics(m_JetPackClump, AtomicRemoveAnimFromSkinCB, nullptr);
            }
        }
        RpClumpDestroy(m_JetPackClump);
    }
    StopJetPackEffect();
    CEntity::SafeCleanUpRef(m_TargetEntity);
}

// 0x67E6E0
bool CTaskSimpleJetPack::MakeAbortable(class CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (event) {
        if (event->GetEventType() == EVENT_DAMAGE) {
            if (ped->m_fHealth > 0.f || (!ped->bIsStanding && !ped->physicalFlags.bSubmergedInWater)) {
                return false;
            }
        }

        if (ped->IsPlayer() && event->GetEventType() == EVENT_ON_FIRE) {
            return true;
        }

        if (event->GetEventPriority() < 63) {
            return false;
        }
    }

    if (m_bSwitchedWeapons && ped->IsPlayer()) {
        ped->ReplaceWeaponWhenExitingVehicle();
        m_bSwitchedWeapons = false;
    }

    m_bIsFinished = true;

    StopJetPackEffect();
    ped->GetAE().TurnOffJetPack();

    return true;
}

// 0x6801F0
bool CTaskSimpleJetPack::ProcessPed(CPed* ped) {
    if (m_bIsFinished) {
        if (m_bSwitchedWeapons && ped->IsPlayer()) {
            ped->ReplaceWeaponWhenExitingVehicle();
        }
        StopJetPackEffect();
        return true;
    }

    if (!m_bSwitchedWeapons) {
        if (ped->IsPlayer()) {
            ped->RemoveWeaponWhenEnteringVehicle(true);
        }
        m_bSwitchedWeapons = true; // exe 0x68025F: set also for non-players
    }

    ped->bCheckColAboveHead = true;

    if (ProcessAnims(ped)) {
        if (ped->m_fHealth >= 1.f) {
            if (ped->IsPlayer()) {
                ProcessControlInput(ped->AsPlayer());
            }
        } else {
            m_ThrustStop   = false;
            m_ThrustFwd    = 0;
            m_ThrustAngle  = 0.f;
            // exe 0x6801FF..0x6802F1: strafe += (rand() & 1) ? +1 : -1, then clamped to [-3, 3] (the port replaced the strafe by +-1 through a precedence slip, and used DoCoinFlip's rand() >= 16383 with the opposite sign)
            m_ThrustStrafe = std::clamp(m_ThrustStrafe + ((CGeneral::GetRandomNumber() & 1) ? 1.f : -1.f), -3.f, 3.f);

            ped->m_fAimingRotation +=
                X87R(x87::sin((float)CTimer::m_snTimeInMilliseconds * 0.0015707964f) * CTimer::GetTimeStep()) * JETPACK_TURN_RATE * CGeneral::GetRandomNumberInRange(0.f, 1.f); // TODO: Magic number

            if (ped->GetPlayerData()) {
                ped->GetPlayerData()->m_fMoveBlendRatio = 0.f;
            }
        }
        ProcessThrust(ped);
        if (m_ThrustFwd > 0 || std::abs(m_ThrustStrafe) > 0.1f) {
            ped->bIsStanding = false;
        }
    }

    return false;
}

// 0x67F6A0
void CTaskSimpleJetPack::RenderJetPack(CPed* ped) {
    if (m_JetPackClump && !m_bIsFinished) {
        const auto jp  = RpClumpGetFrame(m_JetPackClump);

        // Update JetPack matrix
        {
            const auto jpMat = RwFrameGetMatrix(jp);
            *jpMat = RpHAnimHierarchyGetMatrixArray(GetAnimHierarchyFromSkinClump(ped->GetRpClump()))[PED_NODE_LEFT_ARM]; // TODO/NOTE: Not sure abt the enum
            RwMatrixTranslate(jpMat, &JETPACK_POS_OFFSET, rwCOMBINEPRECONCAT);
            RwMatrixRotate(jpMat, &JETPACK_ROT_AXIS, 90.f, rwCOMBINEPRECONCAT);
        }

        auto currHeading = ped->m_fAimingRotation;
        if (currHeading > m_PrevHeading + PI) { // 0x67F72C: strict compares
            currHeading -= TWO_PI;
        } else if (currHeading < m_PrevHeading - PI) { // 0x67F748
            currHeading += TWO_PI;
        }
        const auto headingDelta = std::clamp(currHeading - m_PrevHeading, -0.2f, 0.2f);
        const float headingDeltaDeg = RadiansToDegrees(headingDelta) * 10.f; // 0x67F78A: (delta * 57.29578) * 10

        const auto ProcessJetBall = [&](const char* jbFrameName, bool bLeftSide) {
            const auto jb = CClumpModelInfo::GetFrameFromName(m_JetPackClump, jbFrameName);
            if (!jb) {
                return;
            }
            const auto RotateJB = [&](float x, float y) {
                const auto  jbMat = RwFrameGetMatrix(jb);
                const RwV3d jbPos = *RwMatrixGetPos(jbMat);
                RwMatrixRotate(jbMat, &CPedIK::XaxisIK, x, rwCOMBINEREPLACE);
                RwMatrixRotate(jbMat, &CPedIK::YaxisIK, y, rwCOMBINEPRECONCAT);
                RwV3dAssign(RwMatrixGetPos(jbMat), &jbPos); // Changing the rotation also changes the position I assume, so restore it
            };
            if (m_ThrustStop) {
                const auto t = (float)CTimer::GetTimeInMS() / 79.577469f; // TODO: Magic number
                RotateJB(
                    X87R(x87::sin(t) * (bLeftSide ? 45.f : -45.f)),
                    X87R(x87::cos(t) * 45.f)
                );
            } else {
                const auto rotY = m_ThrustStrafe * 45.f;
                // 0x67F921 (jetball1): ThrustAngle * 57.29578 + headingDeltaDeg; 0x67F999 (jetball2): ThrustAngle * 57.29578 - headingDeltaDeg
                const float rotXRaw = bLeftSide
                    ? RadiansToDegrees(m_ThrustAngle) + headingDeltaDeg
                    : RadiansToDegrees(m_ThrustAngle) - headingDeltaDeg;
                RotateJB(
                    std::clamp(rotXRaw, -90.f, 90.f),
                    bLeftSide ? std::max(0.f, rotY) : std::min(0.f, rotY)
                );
            }
        };

        ProcessJetBall("jetball1", true);
        ProcessJetBall("jetball2", false);

        RwFrameUpdateObjects(jp);
        RpClumpRender(m_JetPackClump);
        DoJetPackEffect(ped);
    } else {
        StopJetPackEffect();
    }
    m_PrevHeading = ped->m_fCurrentRotation;
}

// 0x67EF20
void CTaskSimpleJetPack::ProcessThrust(CPed* ped) {
    const auto ApplyThrust = [&](CVector dir, float mult) {
        ped->ApplyMoveForce(dir * (MAGIC * ped->m_fMass * CTimer::GetTimeStep() * mult));
    };

    const CVector prevPedVel = ped->GetMoveSpeed();

    if (m_ThrustStop) {
        ApplyThrust(ped->GetUp(), 1.f);

        const auto pedSpdSq = ped->GetMoveSpeed().SquaredMagnitude();
        auto stopForce = CTimer::GetTimeStep() * THRUST_STOP * MAGIC;
        if (sq(stopForce) > pedSpdSq) {
            stopForce *= 0.75f;
        }
        if (sq(stopForce) <= pedSpdSq) {
            ped->ApplyMoveForce(-ped->GetMoveSpeed() / std::sqrt(pedSpdSq) * stopForce);
        } else {
            ped->ResetMoveSpeed();
        }
    } else {
        ApplyThrust(ped->GetUp(), m_ThrustFwd >= 0 ? THRUST_NOMINAL : THRUST_NOMINAL * 0.5f);

        if (m_ThrustFwd > 0) { // 0x67F1B7
            const auto thrustUp = ped->GetUp() * x87::cos(m_ThrustAngle);

            auto thrustFwd = ped->GetForward() * -x87::sin(m_ThrustAngle);
            thrustFwd.z    = std::min(0.75f, thrustFwd.z);

            const auto heightMult = std::clamp(1.f - (ped->GetPosition().z - (TheCamera.CalculateGroundHeight(eGroundHeightType::ENTITY_BB_TOP) + 100.f)) * 0.05f, 0.f, 1.f); // 0x67F28C fmul [0x858C28]=0.05f: the exe fades over 20 units (the port divided by 100)
            ApplyThrust(thrustFwd + thrustUp, THRUST_FULL * heightMult);
        }

        if (m_ThrustStrafe != 0.f) { // 0x67F36F
            ApplyThrust(ped->GetRight(), m_ThrustStrafe * THRUST_STRAFE);
        }
    }

    const auto swingDampingFactor = std::pow(LEG_SWING_DAMP_FRAC, CTimer::GetTimeStep());
    const auto CalculateSwing = [&](CVector dir, float& speed, float& angle) {
        if (!ped->bWasStanding && !ped->bIsStanding) {
            const auto deltaVelocity      = ped->GetMoveSpeed() - m_PrevVelocity;   // Delta since last frame
            const auto sumAppliedVelocity = ped->GetMoveSpeed() - prevPedVel;       // Total velocity applied this frame
            speed -= (deltaVelocity + sumAppliedVelocity).Dot(dir) * LEG_SWING_DELTA_V_MULT;
        }
        speed -= X87R(x87::sin(angle) * CTimer::GetTimeStep()) * LEG_SWING_GRAVITY_MULT;
        speed *= swingDampingFactor;

        angle = std::clamp(angle + speed * CTimer::GetTimeStep(), -LEG_SWING_MAX_ANGLE, LEG_SWING_MAX_ANGLE);
    };
    CalculateSwing(ped->GetForward(), m_LegSwingFwdSpeed,  m_LegSwingFwd);
    CalculateSwing(ped->GetRight(),   m_LegSwingSideSpeed, m_LegSwingSide);

    m_LegTwist *= swingDampingFactor;

    ped->SetVelocity(ped->GetMoveSpeed() * std::pow(1.f - ped->GetMoveSpeed().Magnitude() * THRUST_MOVE_DAMPING, CTimer::GetTimeStep()));
    m_PrevVelocity = ped->GetMoveSpeed();

    if (!m_bIsFinished) {
        ped->GetAE().TurnOnJetPack();
        ped->GetAE().UpdateJetPack((float)m_ThrustFwd, m_ThrustAngle);
    }
}

// 0x67B5C0
bool CTaskSimpleJetPack::ProcessAnims(CPed* ped) {
    if (CStreaming::IsModelLoaded(MODEL_JETPACK)) {
        if (!m_JetPackClump) {
            const auto mi = CModelInfo::GetModelInfo(MODEL_JETPACK);
            m_JetPackClump = reinterpret_cast<RpClump*>(mi->CreateInstance());
            mi->AddRef();

            assert(m_JetPackClump);
        }
    } else {
        CStreaming::RequestModel(MODEL_JETPACK, STREAMING_KEEP_IN_MEMORY);
    }

    if (!m_JetPackClump) {
        return false;
    }

    if (ped->bIsStanding) {
        if (ped->IsPlayer()) {
            ped->AsPlayer()->SetRealMoveAnim();
        }
    } else {
        CAnimManager::BlendAnimation(ped->GetRpClump(), ped->m_nAnimGroup, ANIM_ID_IDLE, 4.f);
        ped->SetMoveState(PEDMOVE_STILL);
    }

    return true;
}

// 0x67E7B0
void CTaskSimpleJetPack::ProcessControlInput(CPlayerPed* player) {
    const auto pad = player->GetPadFromPlayer();
    const auto wi  = &player->GetActiveWeapon().GetWeaponInfo(player);

    if (wi->m_nWeaponFire == WEAPON_FIRE_INSTANT_HIT && wi->flags.bAimWithArm) { // 0x67E7E5
        if (const auto tPlayerOnFoot = notsa::dyn_cast_if_present<CTaskSimplePlayerOnFoot>(player->GetTaskManager().GetTaskPrimary(TASK_PRIMARY_DEFAULT))) {
            tPlayerOnFoot->ProcessPlayerWeapon(player);
        }
    }

    if (player->bIsStanding && player->IsPedStandingInPlace() && pad->GetExitVehicle()) { // 0x67E829
        DropJetPack(player);
        return;
    }

    const auto walkUpDown = (float)pad->GetPedWalkUpDown();
    const auto walkLeftRight = (float)pad->GetPedWalkLeftRight();
    const auto padMoveMag = std::sqrt(sq(walkUpDown) + sq(walkLeftRight)) * ExeRecip(60.f);

    const auto InterpolateThrustAngle = [&, this]{
        // exe (0x67E938): `p = pow(0.9, timestep)` stays on the x87 stack; result = (1 - p) * (walkUpDown * (1/128)) * 75deg + thrustAngle * p
        // (NOT common.h's `lerp`: this is the `from * (1 - t) + to * t` form with the multiplications in this order)
        const double p = std::pow((double)JETPACK_ANGLE_RATE, (double)CTimer::GetTimeStep());
        m_ThrustAngle = (float)(((X87R(1.0 - p)) * (x87d)(walkUpDown / 128.f)) * (x87d)THRUST_MAX_ANGLE + X87R((double)m_ThrustAngle * p));
    };

    const auto UpdateStrafe = [this](float step, float clampTo = 1.f) {
        m_ThrustStrafe += step * CTimer::GetTimeStep();
        m_ThrustStrafe = step >= 0.f
            ? std::min(m_ThrustStrafe, +clampTo)  // Upper clamp
            : std::max(m_ThrustStrafe, -clampTo); // Lower clamp
    };

    if (!CCamera::m_bUseMouse3rdPerson || player->bIsStanding) {
        if (player->m_pTargetedObject || player->bIsStanding || pad->GetTarget()) { // 0x67EA51
            if (padMoveMag > 0.f) { // Inverted
                // 0x67EB33..0x67EB5F: LimitRadianAngle(GetRadianAngleBetweenPoints(0, 0, -x, y) - TheCamera.m_fOrientation [0xB6F178])
                player->m_fAimingRotation = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(0.f, 0.f, -walkLeftRight, walkUpDown) - TheCamera.m_fOrientation);
                m_ThrustAngle = THRUST_MAX_ANGLE * padMoveMag * -0.3515625f; // TODO: Magic
            } else {
                player->GetPlayerData()->m_fMoveBlendRatio = 0.f;
            }
        } else if (walkLeftRight != 0.f) { // 0x67EAA1
            player->m_fAimingRotation = CGeneral::LimitRadianAngle(player->m_fAimingRotation + JETPACK_TURN_RATE * CTimer::GetTimeStep() * walkLeftRight / 128.f);
            InterpolateThrustAngle();
        }
    } else { // 0x67E8D1
        // 0x67E8F5..0x67E932: fpatan(-x', y') of the active cam front, with (x', y') = -(x, y) (fmul by -1.0 [0x858C1C]) unless the camera looks forward
        // (NOT `Heading() - PI`: that differs in range - it yields -pi where the exe yields +pi - and in rounding)
        {
            const auto& front = TheCamera.GetActiveCam().m_vecFront;
            const bool  fwd   = TheCamera.GetLookDirection() == LOOKING_DIRECTION_FORWARD;
            player->m_fAimingRotation = (float)x87::atan2((double)(fwd ? -front.x : front.x), (double)(fwd ? front.y : -front.y));
        }
        InterpolateThrustAngle();

        if (walkLeftRight < 0) {
            UpdateStrafe(-0.1f);
        } else if (walkLeftRight > 0) {
            UpdateStrafe(+0.1f);
        } else if (m_ThrustStrafe > 0.f) {
            UpdateStrafe(-0.05f);
        } else if (m_ThrustStrafe < 0.f) {
            UpdateStrafe(+0.05f);
        }
    }

    if (player->m_pTargetedObject && !player->bIsStanding) {
        const auto playerToTargetedObj       = player->GetPosition() - player->m_pTargetedObject->GetPosition();
        const auto playerToTargetedObj2D     = CVector2D{playerToTargetedObj};
        const auto playerToTargetedObjDist2D = playerToTargetedObj2D.Magnitude();
        if (playerToTargetedObjDist2D < std::abs(playerToTargetedObj.z) * 0.2f + 3.f) {
            player->ApplyMoveForce(CVector{
                  playerToTargetedObj2D / std::min(0.1f, playerToTargetedObjDist2D)
                * (1.f - playerToTargetedObjDist2D / (walkLeftRight / 128.f))
                * 3.f
                * MAGIC
                * THRUST_FULL
            });
        }
    }

    bool bThrustUpdated = false;
    if (!CCamera::m_bUseMouse3rdPerson || player->bIsStanding) { // 0x67ED23
        if (bThrustUpdated |= pad->GetLookBehindForCar()) { // 0x67EDA6
            m_ThrustStop   = true;
            m_ThrustFwd    = 0;
            m_ThrustStrafe = 0.f;
        } else if (!pad->GetTarget()) {
            if (bThrustUpdated |= pad->GetLookLeft()) {
                UpdateStrafe(-0.1f);
            }else if (bThrustUpdated |= pad->GetLookRight()) {
                UpdateStrafe(+0.1f);
            }
        }
    }
    if (!bThrustUpdated) {
        m_ThrustStop = false;
        m_ThrustFwd  = pad->GetBrake() ? -1 : pad->GetAccelerate() || pad->GetPedWalkUpDown() != 0;
        if (m_ThrustStrafe > 0.f) {
            UpdateStrafe(-0.05f, 0.f);
        } else if (m_ThrustStrafe < 0.f) {
            UpdateStrafe(+0.05f, 0.f);
        }
    }

    // 0x67EEA9
    auto& br = player->GetPlayerData()->m_fMoveBlendRatio;
    if (padMoveMag <= 0.f) {
        br = 0.f;
    } else {
        const auto delta = CTimer::GetTimeStep() * 0.07f;
        br = padMoveMag - br <= delta
            ? padMoveMag
            : br + delta;
    }
}

// 0x67B720
void CTaskSimpleJetPack::ApplyRollAndPitch(CPed* ped) {
    if (ped->bIsStanding || ped->bWasStanding) {
        return;
    }
    for (const auto nodeId : { PED_NODE_LEFT_LEG, PED_NODE_RIGHT_LEG }) { // Rotate legs according to current swing values
        const auto q = &ped->m_apBones[nodeId]->KeyFrame->q;
        RtQuatRotate(q, &CPedIK::ZaxisIK, RadiansToDegrees(m_LegSwingFwd), rwCOMBINEPOSTCONCAT);
        RtQuatRotate(q, &CPedIK::YaxisIK, RadiansToDegrees(m_LegSwingSide), rwCOMBINEPOSTCONCAT);
    }
    ped->bUpdateMatricesRequired = true;
}

// 0x67B660
void CTaskSimpleJetPack::DropJetPack(CPed* ped) {
    if (!m_bIsFinished) {
        CVector pickupPos;
        CPickups::CreatePickupCoorsCloseToCoors(ped->GetPosition(), pickupPos);
        CPickups::GenerateNewOne(pickupPos, MODEL_JETPACK, PICKUP_ONCE_TIMEOUT_SLOW, 0, 0, true, nullptr);
    }
    MakeAbortable(ped);
    m_ThrustStop   = 0;
    m_ThrustFwd    = 0;
    m_ThrustStrafe = 0.f;
}

// 0x67B7F0
void CTaskSimpleJetPack::DoJetPackEffect(CPed* ped) {
    // 0x67B7F0: both fx systems are created first (each creation resets the key time to 0), then the key time is
    // advanced, then both fx get their matrix/offset/const time
    const auto CreateFx = [&, this](FxSystem_c*& fx) {
        if (ped->GetRpClump() && !fx) {
            if (fx = g_fxMan.CreateFxSystem("jetpack", CVector{}, RwFrameGetMatrix(RpClumpGetFrame(ped->GetRpClump())))) {
                fx->Play();
                fx->SetLocalParticles(true);
                fx->CopyParentMatrix();
                m_FxKeyTime = 0.f;
            }
        }
    };
    CreateFx(m_ThrusterFX[0]);
    CreateFx(m_ThrusterFX[1]);

    const auto SetupFx = [&, this](FxSystem_c* fx, const char* jbFrameName) {
        if (fx) {
            fx->SetMatrix(RwFrameGetLTM(CClumpModelInfo::GetFrameFromName(m_JetPackClump, jbFrameName)));
            fx->SetOffsetPos(JETBALL_OFFSET);
        }
    };
    SetupFx(m_ThrusterFX[0], "jetball1");
    SetupFx(m_ThrusterFX[1], "jetball2");

    // When pressing the brake button (space) you're going down faster.
    // But the flame is blue, even though the audio for the blue flame isn't playing.
    // So, going by the logic "blue == thrust is generated", "orange == no thrust generated"
    // I conclude it this is a bug.
    m_FxKeyTime = (notsa::IsFixBugs() ? m_ThrustFwd == 1 : m_ThrustFwd != 0) || m_ThrustStrafe != 0.f // Simplified... TODO: Use frame delta time here
        ? std::min(m_FxKeyTime + 0.1f, 1.f)
        : std::max(m_FxKeyTime - 0.1f, 0.f);

    for (const auto fx : m_ThrusterFX) {
        if (fx) {
            fx->SetConstTime(true, m_FxKeyTime);
        }
    }
}

// 0x67BA10
void CTaskSimpleJetPack::StopJetPackEffect() {
    for (auto& fx : m_ThrusterFX) {
        if (fx) {
            fx->Kill();
            fx = nullptr;
        }
    }
}
