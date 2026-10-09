#include "StdInc.h"

#include "AutoPilot.h"
#include "Curves.h"

// 0x6D5E20
CAutoPilot::CAutoPilot() : m_aPathFindNodesInfo() {
    _smthNext = 1;
    _smthCurr = 1;

    m_nCarCtrlFlags = 0;

    field_C = 0;
    m_nSpeedScaleFactor = 1000;
    m_nNextLane = 0;
    m_nCurrentLane = 0;
    m_nCarDrivingStyle = DRIVING_STYLE_STOP_FOR_CARS;
    m_nCarMission = eCarMission::MISSION_NONE;
    m_nTempAction = TEMPACT_NONE;
    SetCruiseSpeed(10);
    m_speed = 10.0F;
    m_nPathFindNodesCount = 0;
    m_TargetEntity = nullptr;

    m_nTimeToStartMission = CTimer::GetTimeInMS();
    m_nTimeSwitchedToRealPhysics = CTimer::GetTimeInMS();
    m_LastUpdateTimeMs = 0;

    m_nStraightLineDistance = 20;
    m_ucTempActionMode = 0;
    m_ucCarMissionModeCounter = 0;
    field_41 = 0;
    m_SpeedMult = 1.0f;
    m_ucHeliSpeedMult = 0;
    movementFlags.bIsStopped = false;
    movementFlags.bIsParked = false;
    field_4A = 0;
    m_ucCarFollowDist = 10;
    m_ucHeliTargetDist2 = 10;
    field_50 = CGeneral::GetRandomNumber() % 8 + 2;
    m_vehicleRecordingId = -1;
    m_bPlaneDogfightSomething = false;
    m_ObstructingEntity = nullptr;
    m_fMaxTrafficSpeed = 0.0F;
}

void CAutoPilot::InjectHooks() {
    RH_ScopedClass(CAutoPilot);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(ModifySpeed, 0x41B980);
    RH_ScopedInstall(RemoveOnePathNode, 0x41B950);
}

// 0x41B980
void CAutoPilot::ModifySpeed(float target) {
    constexpr float c01   = 0.01f;  // 0x858C58
    constexpr float c0125 = 0.125f; // 0x858C48
    constexpr float c54   = 5.4f;   // 0x858C50

    if (!(target > c01)) {
        target = c01;
    }

    // `(now - field_C)` is treated as unsigned (the original adds 2^32 if negative), divided as a signed int
    const auto progress = (float)((double)(uint32)(CTimer::GetTimeInMS() - (uint32)field_C) / (double)(int32)m_nSpeedScaleFactor);
    m_speed = target;

    if (!ThePaths.IsAreaLoaded(m_nCurrentPathNodeInfo.m_wAreaId) || !ThePaths.IsAreaLoaded(m_nNextPathNodeInfo.m_wAreaId)) {
        return;
    }

    // Both links are looked up through `m_pNaviNodes`
    const auto& linkA = ThePaths.GetCarPathLink(m_nCurrentPathNodeInfo);
    const auto& linkB = ThePaths.GetCarPathLink(m_nNextPathNodeInfo);

    // Raw compressed values (posn: int16 * 1/8, dir: int8 * 0.01)
    const auto rawA16 = reinterpret_cast<const int16*>(&linkA);
    const auto rawB16 = reinterpret_cast<const int16*>(&linkB);
    const auto rawA8  = reinterpret_cast<const int8*>(&linkA);
    const auto rawB8  = reinterpret_cast<const int8*>(&linkB);

    const double curr = _smthCurr;
    const double next = _smthNext;

    const auto Dir = [](int8 d, double s) { return (float)((double)d * (double)c01 * s); };
    const float adx = Dir(rawA8[8], curr);
    const float ady = Dir(rawA8[9], curr);
    const float bdx = Dir(rawB8[8], next);
    const float bdy = Dir(rawB8[9], next);

    const auto k1 = (float)((linkA.OneWayLaneOffsetExtended() + (double)m_nCurrentLane) * (double)c54);
    const auto k2 = (linkB.OneWayLaneOffsetExtended() + (double)m_nNextLane) * (double)c54; // not rounded to float

    const CVector end{
        (float)((double)rawB16[0] * (double)c0125 + k2 * (double)bdy),
        (float)((double)rawB16[1] * (double)c0125 - k2 * (double)bdx),
        0.0f
    };
    const CVector start{
        (float)((double)rawA16[0] * (double)c0125 + (double)k1 * (double)ady),
        (float)((double)rawA16[1] * (double)c0125 - (double)k1 * (double)adx),
        0.0f
    };

    const auto scale = CCurves::CalcSpeedScaleFactor(start, end, adx, ady, bdx, bdy);

    // `_ftol` truncates, the products are kept at extended precision
    const auto newScale = (int32)((double)scale * (1000.0 / (double)m_speed));
    m_nSpeedScaleFactor = (uint32)newScale;
    field_C             = (int32)((double)CTimer::GetTimeInMS() - (double)newScale * (double)progress);
}

// 0x41B950
void CAutoPilot::RemoveOnePathNode() {
    m_nPathFindNodesCount--;
    for (int16 i = 0; i < (int16)m_nPathFindNodesCount; i++) {
        m_aPathFindNodesInfo[i] = m_aPathFindNodesInfo[i + 1];
    }
}

void CAutoPilot::SetCarMission(eCarMission carMission, uint32 timeOffsetMs) {
    m_nCarMission = carMission;
    m_nTimeToStartMission = CTimer::GetTimeInMS() + timeOffsetMs;
}

// notsa
void CAutoPilot::SetTempAction(eAutoPilotTempAction action, uint32 durMs) {
    m_nTempAction = action;
    m_nTempActionTime = CTimer::GetTimeInMS() + durMs;
}
