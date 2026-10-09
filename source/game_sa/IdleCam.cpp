#include "StdInc.h"

#include "IdleCam.h"
#include "InterestingEvents.h"
#include "HandShaker.h"
#include "Fx/FxFtol.h"

auto& gIdleCam = StaticRef<CIdleCam>(0xB6FDA0);
auto& gbCineyCamProcessedOnFrame = StaticRef<uint32>(0xB6EC40);

void CIdleCam::InjectHooks() {
    RH_ScopedClass(CIdleCam);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(GetLookAtPositionOnTarget, 0x50EAE0);
    RH_ScopedInstall(Init, 0x50E6D0);
    RH_ScopedInstall(Reset, 0x50A160);
    RH_ScopedInstall(ProcessIdleCamTicker, 0x50A200);
    RH_ScopedInstall(SetTarget, 0x50A280);
    RH_ScopedInstall(FinaliseIdleCamera, 0x50E760);
    RH_ScopedInstall(SetTargetPlayer, 0x50EB50);
    RH_ScopedInstall(IsTargetValid, 0x517770);
    RH_ScopedInstall(ProcessTargetSelection, 0x517870);
    RH_ScopedInstall(ProcessSlerp, 0x5179E0);
    RH_ScopedInstall(ProcessFOVZoom, 0x517BF0);
    RH_ScopedInstall(Run, 0x51D3E0);
    RH_ScopedInstall(Process, 0x522C80);
    RH_ScopedInstall(IdleCamGeneralProcess, 0x50E690);
}

// 0x517760
CIdleCam::CIdleCam() {
    Init();
}

// 0x50E6D0
void CIdleCam::Init() {
    m_DistStartFOVZoom                         = 15.0f;
    m_ZoomNearest                              = 15.0f;
    m_SlerpDuration                            = 2000.0f;
    m_TimeControlsIdleForIdleToKickIn          = 90000.0f;
    m_TimeToConsiderNonVisibleEntityAsOccluded = 3000.0f;
    m_DistTooClose                             = 4.0f;
    m_DistTooFar                               = 80.0f;
    m_DegreeShakeIdleCam                       = 1.0f;
    m_ShakeBuildUpTime                         = 3000.0f;
    m_ZoomFarthest                             = 70.0f;
    m_DurationFOVZoom                          = 1000.0f;
    m_TargetLOSFramestoReject                  = 14;
    m_TimeBeforeNewZoomIn                      = 12000.0f;
    m_TimeMinimumToLookAtSomething             = 5000.0f;
    m_IncreaseMinimumTimeFactorForZoomedIn     = 2.0f;
    m_LastTimePadTouched                       = 0;
    m_IdleTickerFrames                         = 0;
    Reset(false);
}

// 0x50A160
void CIdleCam::Reset(bool resetControls) {
    m_PositionToSlerpFrom.Reset();
    m_LastIdlePos.Reset();
    m_TimeSlerpStarted               = -1.0f;
    m_TimeIdleCamStarted             = -1.0f;
    m_TimeLastTargetSelected         = -1.0f;
    m_ZoomFrom                       = -1.0f;
    m_ZoomTo                         = -1.0f;
    m_TimeZoomStarted                = -1.0f;
    m_TimeTargetEntityWasLastVisible = -1.0f;
    m_TimeLastZoomIn                 = -1.0f;
    m_Target                         = 0;
    m_ZoomState                      = eIdleCamZoomState::ZOOMED_OUT;
    m_nForceAZoomOut                 = 0;
    m_CurFOV                         = 70.0f;
    m_TargetLOSCounter               = 0;
    m_bHasZoomedIn                   = 0;
    m_SlerpTime                      = 1.0f;

    if (resetControls) {
        CPad::GetPad()->LastTimeTouched = CTimer::GetTimeInMS();
    }
}

// 0x50A200
void CIdleCam::ProcessIdleCamTicker() {
    if (m_LastTimePadTouched == CPad::GetPad(0)->LastTimeTouched) {
        m_IdleTickerFrames += notsa::detail::Ftol(CTimer::ms_fTimeStep * 0.02f * 1000.0f); // 0x858B38, 0x858C4C (NOT `ts * 20`)
    } else {
        m_LastTimePadTouched = CPad::GetPad(0)->LastTimeTouched;
        m_IdleTickerFrames   = 0;
    }
}

// inlined
bool CIdleCam::IsItTimeForIdleCam() {
    // Always false in Android
    return static_cast<float>(m_IdleTickerFrames) > m_TimeControlsIdleForIdleToKickIn;
}

// wrong name?
// 0x50E690
void CIdleCam::IdleCamGeneralProcess() {
    if (!IsItTimeForIdleCam()) {
        g_InterestingEvents.m_b1 = false;
    }

    if (TheCamera.GetActiveCam().m_nMode != MODE_FOLLOWPED) {
        g_InterestingEvents.m_b1    = false;
        gIdleCam.m_IdleTickerFrames = 0;
    }
}

// 0x50EAE0
void CIdleCam::GetLookAtPositionOnTarget(const CEntity* target, CVector& outPos) {
    outPos = target->GetPosition();
    if (target->GetIsTypePed()) {
        switch (target->AsPed()->m_nPedType) {
        case PED_TYPE_CIVFEMALE:
        case PED_TYPE_PROSTITUTE:
            outPos.z += 0.1f;
            break;
        default:
            outPos.z += 0.5f;
            break;
        }
    }
}

// 0x517BF0
void CIdleCam::ProcessFOVZoom(float time) {
    const auto curTimeMs = static_cast<float>(CTimer::GetTimeInMS());

    float zoomNearest  = m_ZoomNearest;
    bool  shouldZoomIn = false;
    if (m_Target) {
        CVector lookAtPos;
        GetLookAtPositionOnTarget(m_Target, lookAtPos);

        // exe: x87, term order z, y, x
        const double dx   = (double)m_Cam->m_vecSource.x - (double)lookAtPos.x;
        const double dy   = (double)m_Cam->m_vecSource.y - (double)lookAtPos.y;
        const double dz   = (double)m_Cam->m_vecSource.z - (double)lookAtPos.z;
        const double dist = std::sqrt(dz * dz + dy * dy + dx * dx);

        if (m_Target->GetType() == ENTITY_TYPE_PED) {
            const auto pedType = m_Target->AsPed()->m_nPedType;
            if (pedType == PED_TYPE_PROSTITUTE || pedType == PED_TYPE_CIVFEMALE) {
                shouldZoomIn = true;
                zoomNearest *= 0.5f;
                if (dist < 8.0) {
                    m_nForceAZoomOut = true;
                }
            }
        }
        if (dist > (double)m_DistStartFOVZoom) {
            shouldZoomIn = true;
        }
    }

    if (time >= 1.0f) {
        const auto prevState = m_ZoomState;

        bool  applyZoomTo = false;
        bool  keepStart   = false; // Don't touch `m_TimeZoomStarted` and `m_ZoomFrom`
        float newZoomTo{};

        if (shouldZoomIn) {
            if (m_TimeBeforeNewZoomIn < curTimeMs - m_TimeLastZoomIn) {
                bool isLOSClear = true;
                if (m_Target) {
                    const auto oldIgnore  = CWorld::pIgnoreEntity;
                    CWorld::pIgnoreEntity = m_Target;

                    CVector lookAtPos;
                    GetLookAtPositionOnTarget(m_Target, lookAtPos);
                    isLOSClear = CWorld::GetIsLineOfSightClear(m_Cam->m_vecSource, lookAtPos, true, false, false, true, false, false, true);

                    CWorld::pIgnoreEntity = oldIgnore;
                }

                if (m_TargetLOSCounter > 10 && m_ZoomState == eIdleCamZoomState::ZOOMED_IN) {
                    m_ZoomState = eIdleCamZoomState::ZOOMING_OUT;
                }

                if (m_ZoomState == eIdleCamZoomState::ZOOMED_OUT && !m_bHasZoomedIn && isLOSClear) {
                    m_ZoomState = eIdleCamZoomState::ZOOMING_IN;
                    newZoomTo   = zoomNearest;
                    applyZoomTo = true;
                    keepStart   = prevState == eIdleCamZoomState::ZOOMING_IN;
                }
            }
        } else if (prevState == eIdleCamZoomState::ZOOMED_IN) {
            newZoomTo   = m_ZoomFarthest;
            m_ZoomState = eIdleCamZoomState::ZOOMING_OUT;
            applyZoomTo = true;
        }

        if (applyZoomTo) {
            m_ZoomTo = newZoomTo;
            if (!keepStart) {
                m_TimeZoomStarted = curTimeMs;
                m_ZoomFrom        = m_CurFOV;
            }
        }
    }

    if (m_ZoomState == eIdleCamZoomState::ZOOMED_IN) {
        m_TimeLastZoomIn = curTimeMs;
    }

    if (m_nForceAZoomOut && m_ZoomState == eIdleCamZoomState::ZOOMED_IN) {
        m_ZoomFrom        = m_CurFOV;
        m_TimeZoomStarted = curTimeMs;
        m_ZoomState       = eIdleCamZoomState::ZOOMING_OUT;
        m_ZoomTo          = m_ZoomFarthest;
    }
    m_nForceAZoomOut = false;

    // Interpolates the FOV between `m_ZoomFrom` and `m_ZoomTo`
    const auto InterpolateFOV = [&] {
        const float t = (270.0f - ((curTimeMs - m_TimeZoomStarted) / m_DurationFOVZoom) * 180.0f) * 0.017453292f;
        m_CurFOV = (float)((double)(m_ZoomTo - m_ZoomFrom) * ((std::sin((double)t) + 1.0) * 0.5) + (double)m_ZoomFrom); // fsin result stays unrounded on the x87 stack
    };

    switch (m_ZoomState) {
    case eIdleCamZoomState::ZOOMING_IN:
        if (std::fabs(m_CurFOV - zoomNearest) >= 1.0f) {
            InterpolateFOV();
        } else {
            m_ZoomState      = eIdleCamZoomState::ZOOMED_IN;
            m_bHasZoomedIn   = true;
            m_CurFOV         = zoomNearest;
        }
        break;
    case eIdleCamZoomState::ZOOMING_OUT:
        if (std::fabs(m_CurFOV - m_ZoomFarthest) >= 1.0f) {
            InterpolateFOV();
        } else {
            m_ZoomState = eIdleCamZoomState::ZOOMED_OUT;
            m_CurFOV    = m_ZoomFarthest;
        }
        break;
    case eIdleCamZoomState::ZOOMED_IN:
        m_CurFOV = zoomNearest;
        break;
    case eIdleCamZoomState::ZOOMED_OUT:
        m_CurFOV = m_ZoomFarthest;
        break;
    default:
        break;
    }

    m_Cam->m_fFOV = m_CurFOV;
}

// 0x517770
bool CIdleCam::IsTargetValid(CEntity* target) {
    if (!target) {
        return false;
    }

    if (target == FindPlayerPed()) {
        return true;
    }
    CVector lookAtPos{};
    GetLookAtPositionOnTarget(target, lookAtPos);

    // exe: x87, term order z, y, x
    const double dx   = (double)m_Cam->m_vecSource.x - (double)lookAtPos.x;
    const double dy   = (double)m_Cam->m_vecSource.y - (double)lookAtPos.y;
    const double dz   = (double)m_Cam->m_vecSource.z - (double)lookAtPos.z;
    const double dist = std::sqrt(dz * dz + dy * dy + dx * dx);
    if (dist < (double)m_DistTooClose || dist > (double)m_DistTooFar) {
        return false;
    }

    if (m_SlerpTime < 1.0f) {
        return true;
    }

    const auto oldIgnore  = CWorld::pIgnoreEntity;
    CWorld::pIgnoreEntity = target;
    notsa::ScopeGuard _([&]{ CWorld::pIgnoreEntity = oldIgnore; });

    if (CWorld::GetIsLineOfSightClear(
        m_Cam->m_vecSource,
        lookAtPos,
        true,
        false,
        false,
        true,
        false,
        false,
        true
    )) {
        return true;
    }

    return m_TargetLOSCounter++ <= m_TargetLOSFramestoReject; // exe: `jg` => false only if the old counter was > frames
}

// 0x50A280
void CIdleCam::SetTarget(CEntity* target) {
    const auto time = static_cast<float>(CTimer::GetTimeInMS());
    if (m_Target) {
        m_PositionToSlerpFrom = m_LastIdlePos;
    } else {
        m_PositionToSlerpFrom = m_Cam->m_vecSource + m_Cam->m_vecFront;
    }

    CEntity::ChangeEntityReference(m_Target, target);

    m_TimeSlerpStarted       = time;
    m_TimeLastTargetSelected = time;
    m_TargetLOSCounter       = 0;
    m_bHasZoomedIn           = false;
}

// 0x50EB50
void CIdleCam::SetTargetPlayer() {
    SetTarget(FindPlayerPed());
    m_nForceAZoomOut = true;
}

// 0x517870
void CIdleCam::ProcessTargetSelection() {
    auto timeDelta = static_cast<float>(CTimer::GetTimeInMS()) - m_TimeLastTargetSelected;
    if (m_ZoomState != eIdleCamZoomState::ZOOMED_OUT && m_TargetLOSCounter <= 0) {
        timeDelta /= m_IncreaseMinimumTimeFactorForZoomedIn;
    }

    if (timeDelta > m_TimeMinimumToLookAtSomething) {
        g_InterestingEvents.InvalidateNonVisibleEvents();
        auto* event = g_InterestingEvents.GetInterestingEvent();

        if (event) {
            auto* eventEntity = event->entity;
            if (m_Target != eventEntity) {
                if (IsTargetValid(eventEntity)) {
                    if (m_ZoomState == eIdleCamZoomState::ZOOMED_OUT) {
                        SetTarget(eventEntity);
                    } else {
                        m_nForceAZoomOut = true;
                    }
                }
            } else if (!IsTargetValid(eventEntity)) {
                g_InterestingEvents.InvalidateEvent(event);
            }
        } else if (!m_Target || !IsTargetValid(m_Target) && m_Target != FindPlayerPed()) {
            if (m_ZoomState == eIdleCamZoomState::ZOOMED_OUT) {
                SetTargetPlayer();
            } else {
                m_nForceAZoomOut = true;
            }
        }
    }

    if (!m_Target) {
        // The exe stores the player into `m_Target` directly (no reference registration) BEFORE calling `SetTarget`,
        // so `SetTarget` takes its "has a target" branch (slerps from `m_LastIdlePos`) and cleans up a reference that was never registered.
        m_Target         = FindPlayerPed();
        m_nForceAZoomOut = true;
        SetTarget(FindPlayerPed());
        m_nForceAZoomOut = true;
    }

    if (!IsTargetValid(m_Target) && timeDelta > m_TimeMinimumToLookAtSomething) {
        m_nForceAZoomOut = true;
        if (m_ZoomState == eIdleCamZoomState::ZOOMED_OUT || m_TargetLOSCounter > 0) {
            SetTargetPlayer();
        }
    }

    if (m_TargetLOSCounter > m_TargetLOSFramestoReject) {
        SetTargetPlayer();
    }
}

// 0x5179E0
float CIdleCam::ProcessSlerp(float& outX, float& outZ) {
    const auto beginTime = CTimer::GetTimeInMS();

    CVector lookAtPos{};
    if (m_TargetLOSCounter >= m_TargetLOSFramestoReject) {
        lookAtPos = m_LastIdlePos;
    } else {
        GetLookAtPositionOnTarget(m_Target, lookAtPos);
    }

    // exe: `GetATanOfXY` leaves its result unrounded on the x87 stack; only the first atan of each vector (+ pi) and the 2nd atan of the
    // slerp-from vector are spilled to float, the 2nd atan of the look-at vector stays in extended precision until the final store
    const CVector fromVec = m_PositionToSlerpFrom - m_Cam->m_vecSource;
    const CVector toVec   = lookAtPos - m_Cam->m_vecSource;
    float  slerpAtan      = (float)(CGeneral::GetATanOfXYExt(fromVec.x, fromVec.y) + 3.1415927f);
    float  slerpDistAtan  = (float)CGeneral::GetATanOfXYExt(fromVec.Magnitude2D(), fromVec.z);
    float  lookAtAtan     = (float)(CGeneral::GetATanOfXYExt(toVec.x, toVec.y) + 3.1415927f);
    double lookAtDistAtan = CGeneral::GetATanOfXYExt(toVec.Magnitude2D(), toVec.z);

    constexpr float PI_F     = 3.1415927f; // 0x858CB8
    constexpr float TWO_PI_F = 6.2831855f; // 0x858CBC
    const auto ClampAngle = [&](auto& angle, auto diff) {
        if (diff > PI_F) {
            angle -= TWO_PI_F;
        } else if (diff < -PI_F) { // 0x858CC0
            angle += TWO_PI_F;
        }
    };

    ClampAngle(lookAtDistAtan, lookAtDistAtan - (double)slerpDistAtan);
    ClampAngle(lookAtAtan, lookAtAtan - slerpAtan);

    float slerpT = ((float)beginTime - m_TimeLastTargetSelected) / m_SlerpDuration;
    if (slerpT > 1.0f) { // `fcomp 1.0; jne` => NaN stays NaN
        slerpT = 1.0f;
    }
    // exe: (sin((270 - slerpT * 180) * 0.017453292) + 1) * 0.5  (0x859070, 0x85A994, 0x8595EC)
    const float lerpT = (float)((std::sin((double)((270.0f - slerpT * 180.0f) * 0.017453292f)) + 1.0) * 0.5);

    // NOTE: NOT the common.h `lerp` (that one is `to * t + from * (1 - t)`); the exe computes `(to - from) * t + from`
    outX = (float)((lookAtDistAtan - (double)slerpDistAtan) * (double)lerpT + (double)slerpDistAtan);
    outZ = (lookAtAtan - slerpAtan) * lerpT + slerpAtan;
    return slerpT;
}

// 0x50E760
void CIdleCam::FinaliseIdleCamera(float curAngleX, float curAngleY, float shakeDegree) {
    // NOTE: the original declaration here was `auto &vecFwd = ..., vecUp = ...;` => `vecUp` was a COPY
    auto& vecFwd = m_Cam->m_vecFront;
    auto& vecUp  = m_Cam->m_vecUp;

    // x87: the products stay unrounded until the float store
    vecFwd = CVector{
        (float)-(std::cos((double)curAngleY) * std::cos((double)curAngleX)),
        (float)-(std::sin((double)curAngleY) * std::cos((double)curAngleX)),
        (float)std::sin((double)curAngleX)
    };
    vecFwd.Normalise();
    m_LastIdlePos = vecFwd + m_Cam->m_vecSource;

    auto& hs = gHandShaker[0];
    hs.Process(shakeDegree * m_DegreeShakeIdleCam); // exe: `shake * m_DegreeShakeIdleCam`
    const float angle = (hs.m_ang.z * m_DegreeShakeIdleCam) * shakeDegree;
    vecFwd = hs.m_resultMat.InverseTransformVector(vecFwd); // 0x59C810 `Multiply3x3(out, v, m)` (NOT TransformPoint)

    vecUp.Set((float)std::sin((double)angle), 0.0f, (float)std::cos((double)angle));

    // The exe orthonormalises the 3 times: (a) with the shaken up vector, (b) after resetting `up` to (0, 0, 1)
    const auto Orthonormalise = [&] {
        auto rightDir = CrossProduct(vecFwd, vecUp);
        rightDir.Normalise();
        vecUp = CrossProduct(rightDir, vecFwd);
    };
    const auto FixDegenerateFront = [&] {
        if (vecFwd.x == 0.0f && vecFwd.y == 0.0f) {
            vecFwd.x = vecFwd.y = 0.0001f; // 0x38D1B717
        }
    };

    Orthonormalise();
    FixDegenerateFront();
    Orthonormalise();

    vecUp.Set(0.0f, 0.0f, 1.0f);
    vecFwd.Normalise();
    FixDegenerateFront();
    Orthonormalise();
    // (no `GetVectorsReadyForRW` here: the exe doesn't call it)
}

// 0x51D3E0
void CIdleCam::Run() {
    const auto beginTime = CTimer::GetTimeInMS();
    ProcessTargetSelection();

    float angleX{}, angleZ{};
    m_SlerpTime = ProcessSlerp(angleX, angleZ);
    ProcessFOVZoom(m_SlerpTime);

    const auto delta = beginTime - m_TimeIdleCamStarted;
    FinaliseIdleCamera(angleX, angleZ, delta < m_ShakeBuildUpTime ? delta / m_ShakeBuildUpTime : 1.f);
}

// 0x522C80
bool CIdleCam::Process() {
    ProcessIdleCamTicker();
    if (!IsItTimeForIdleCam()) {
        return false;
    }

    m_Cam = &TheCamera.GetActiveCam();
    if (m_LastFrameProcessed < CTimer::GetFrameCounter() - 1) {
        g_InterestingEvents.m_b1 = true;
        Reset(false);
        m_TimeIdleCamStarted = static_cast<float>(CTimer::GetTimeInMS());
        SetTarget(FindPlayerPed());
        m_nForceAZoomOut = true;
    }
    m_LastFrameProcessed = CTimer::GetFrameCounter();
    Run();
    gbCineyCamProcessedOnFrame = m_LastFrameProcessed;
    return true;
}
