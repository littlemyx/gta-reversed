#include "StdInc.h"

#include "TaskComplexDriveToPoint.h"
#include "TaskComplexGoToPointAnyMeans.h"

void CTaskComplexDriveToPoint::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexDriveToPoint, 0x86E9DC, 14);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTInstall(Drive, 0x645420);
    RH_ScopedOverloadedInstall(IsTargetBlocked, "Ped", 0x6452C0, bool (CTaskComplexDriveToPoint::*)(CPed*) const);
    RH_ScopedOverloadedInstall(IsTargetBlocked, "Entities", 0x6432A0, bool (CTaskComplexDriveToPoint::*)(CPed*, CEntity**, int32) const);
}

// 0x63CE00
CTaskComplexDriveToPoint::CTaskComplexDriveToPoint(CVehicle* vehicle, const CVector& point, float speed, int32 arg4, eModelID carModelIndexToCreate, float radius, eCarDrivingStyle drivingStyle) :
      CTaskComplexCarDrive(vehicle, speed, carModelIndexToCreate, drivingStyle),
      m_Point{ point },
      field_30{ arg4 },
      m_Radius{ radius },
      field_38{ false }
{

}

// 0x63CE80
CTask* CTaskComplexDriveToPoint::CreateSubTaskCannotGetInCar(CPed* ped) {
    return new CTaskComplexGoToPointAnyMeans(PEDMOVE_RUN, m_Point, 0.5f, m_DesiredCarModel);
}

// 0x63CF00
void CTaskComplexDriveToPoint::SetUpCar() {
    m_OriginalDrivingStyle = m_Veh->m_autoPilot.m_nCarDrivingStyle;
    m_OriginalMission         = m_Veh->m_autoPilot.m_nCarMission;
    m_OriginalSpeed              = m_Veh->m_autoPilot.m_nCruiseSpeed;

    m_bIsCarSetUp = true;

    if (m_CruiseSpeed > 0.0f) {
        assert(m_CruiseSpeed < 255.0f);
        m_Veh->m_autoPilot.SetCruiseSpeed((uint8)m_CruiseSpeed);
    }
    m_Veh->m_autoPilot.m_nCarDrivingStyle    = static_cast<eCarDrivingStyle>(m_CarDrivingStyle);
    m_Veh->m_autoPilot.m_nTimeToStartMission = CTimer::GetTimeInMS();
}

// 0x645420
CTask* CTaskComplexDriveToPoint::Drive(CPed* ped) {
    const auto subTask = m_pSubTask;
    const auto veh     = m_Veh;

    const auto& vehPos = veh->GetPosition();
    const double dx = (double)m_Point.x - vehPos.x;
    const double dy = (double)m_Point.y - vehPos.y;
    const double dz = (double)m_Point.z - vehPos.z;
    const double dist = std::sqrt((dy * dy + dz * dz) + dx * dx); // Kept in extended precision by the original

    if (dist < (double)m_Radius) {
        veh->m_autoPilot.m_nCarMission = MISSION_NONE;
        field_38 = true;
        return CreateSubTask(TASK_FINISHED, ped);
    }

    if (dist < 3.0 && !veh->m_autoPilot.m_nCarMission) {
        field_38 = true;
        return CreateSubTask(TASK_FINISHED, ped);
    }

    if (!veh->m_autoPilot.m_nCruiseSpeed) {
        veh->m_autoPilot.m_nCruiseSpeed = (uint8)(int32)m_CruiseSpeed; // 0x821B40 (_ftol), truncates
    }

    if (IsTargetBlocked(ped)) {
        field_38 = true;
        return CreateSubTask(TASK_FINISHED, ped);
    }

    switch (field_30) {
    case field_30_enum::DEFAULT:       CCarAI::GetCarToGoToCoors(m_Veh, m_Point, m_CarDrivingStyle, false); break;
    case field_30_enum::ACCURATE:      CCarAI::GetCarToGoToCoorsAccurate(m_Veh, m_Point, m_CarDrivingStyle, false); break;
    case field_30_enum::STRAIGHT_LINE: CCarAI::GetCarToGoToCoorsStraightLine(m_Veh, m_Point, m_CarDrivingStyle, false); break;
    case field_30_enum::RACING:        CCarAI::GetCarToGoToCoorsRacing(m_Veh, m_Point, m_CarDrivingStyle, false); break;
    default:                           break; // Original does nothing for out-of-range values
    }
    return subTask;
}

// 0x6452C0
bool CTaskComplexDriveToPoint::IsTargetBlocked(CPed* ped) const {
    const auto& pedPos = ped->GetPosition();
    const double dx = (double)pedPos.x - m_Point.x;
    const double dy = (double)pedPos.y - m_Point.y;
    const double dz = (double)pedPos.z - m_Point.z;
    if (36.0 < (dz * dz + dx * dx) + dy * dy) { // NaN => doesn't return
        return false;
    }

    auto* const intel = ped->GetIntelligence();
    return IsTargetBlocked(ped, intel->GetPedEntities(), 16) || IsTargetBlocked(ped, intel->GetVehicleEntities(), 16);
}

// 0x6432A0
bool CTaskComplexDriveToPoint::IsTargetBlocked(CPed* ped, CEntity** entities, int32 numEntities) const {
    const auto veh = ped->m_pVehicle;
    if (!veh) {
        return false;
    }

    const float vehRadius = veh->GetModelInfo()->GetColModel()->GetBoundRadius();

    const auto& vehPos = veh->GetPosition();
    const double vx = (double)vehPos.x - m_Point.x;
    const double vy = (double)vehPos.y - m_Point.y;
    const double vz = (double)vehPos.z - m_Point.z;
    const float  vehDistSq = (float)((vz * vz + vy * vy) + vx * vx);

    for (auto i = 0; i < numEntities; ++i) {
        CEntity* const entity = entities[i];
        if (!entity || entity == veh) {
            continue;
        }

        const auto& entPos = entity->GetPosition();
        const double ex = (double)entPos.x - m_Point.x;
        const double ey = (double)entPos.y - m_Point.y;
        const double ez = (double)entPos.z - m_Point.z;
        const double entDistSq = (ez * ez + ey * ey) + ex * ex;

        const float entRadius = entity->GetModelInfo()->GetColModel()->GetBoundRadius();
        if (!((double)entRadius * entRadius > entDistSq)) { // Target is not inside the entity's bounding sphere
            continue;
        }

        const double radiusSum = (double)entRadius + vehRadius;
        if ((radiusSum * radiusSum) * 1.5 > (double)vehDistSq) {
            return true;
        }
    }
    return false;
}

void CTaskComplexDriveToPoint::GoToPoint(const CVector& point) {
    m_Point = point;
}
