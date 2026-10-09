#include "StdInc.h"

#include "TaskSimpleCarSetPedOut.h"

#include "World.h"

void CTaskSimpleCarSetPedOut::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleCarSetPedOut, 0x86EEB8, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(PositionPedOutOfCollision, 0x6479B0);
}

CTaskSimpleCarSetPedOut::CTaskSimpleCarSetPedOut(CVehicle* targetVehicle, eTargetDoor nTargetDoor, bool bSwitchOffEngine, bool warpingOutOfCar) :
    m_nTargetDoor{ nTargetDoor },
    m_pTargetVehicle{ targetVehicle },
    m_bSwitchOffEngine{ bSwitchOffEngine },
    m_bWarpingOutOfCar{ warpingOutOfCar }
{
    CEntity::SafeRegisterRef(m_pTargetVehicle);
}

CTaskSimpleCarSetPedOut::~CTaskSimpleCarSetPedOut() {
    CEntity::SafeCleanUpRef(m_pTargetVehicle);
}

// 0x6479B0
void CTaskSimpleCarSetPedOut::PositionPedOutOfCollision(CPed* ped, CVehicle* veh, int32 door) {
    if (!veh) {
        veh = ped->m_pVehicle;
        if (!veh) {
            return;
        }
    }

    // Face the same way as the vehicle
    const float heading = veh->m_matrix
        ? (float)std::atan2((double)-veh->m_matrix->GetForward().x, (double)veh->m_matrix->GetForward().y) // FPATAN
        : veh->m_placement.m_fHeading;
    ped->m_fAimingRotation  = heading;
    ped->m_fCurrentRotation = heading;
    if (ped->m_matrix) {
        ped->m_matrix->SetRotateZOnly(heading);
    } else {
        ped->m_placement.m_fHeading = heading;
    }

    if (veh->m_nVehicleType == VEHICLE_TYPE_BOAT) {
        auto& pedPos = ped->m_matrix->GetPosition(); // NOTE: The original doesn't check `m_matrix` for null from here on

        if (veh->m_pHandlingData->m_bSitInBoat) {
            // x, y: extended precision products; z: spilled to a float
            const auto& up = veh->m_matrix->GetUp();
            const double px = (double)up.x * 0.5;
            const double py = (double)up.y * 0.5;
            const float  pz = (float)((double)up.z * 0.5);
            pedPos.x = (float)(px + (double)pedPos.x);
            pedPos.y = (float)(py + (double)pedPos.y);
            pedPos.z = (float)((double)pz + (double)pedPos.z);
        }

        if (ped->TestCollision(false)) {
            auto& mat = *ped->m_matrix;
            if (veh->vehicleFlags.bIsDrowning) {
                mat.GetPosition().z = (float)((double)mat.GetPosition().z - (double)0.3f);
                if (!ped->TestCollision(false)) {
                    return;
                }
                mat.GetPosition().z = (float)((double)mat.GetPosition().z + (double)0.3f);
                ped->PositionPedOutOfCollision(door, veh, true);
                return;
            }

            const auto& fwd = mat.GetForward();
            const double bx = (double)fwd.x * (double)0.3f;
            const double by = (double)fwd.y * (double)0.3f;
            const float  bz = (float)((double)fwd.z * (double)0.3f);
            mat.GetPosition().x = (float)((double)mat.GetPosition().x - bx);
            mat.GetPosition().y = (float)((double)mat.GetPosition().y - by);
            mat.GetPosition().z = (float)((double)mat.GetPosition().z - (double)bz);

            if (ped->TestCollision(false)) {
                auto& m = *ped->m_matrix;
                m.GetPosition() += m.GetForward() * 0.3f; // 0x40FE90 + 0x411A00
                ped->PositionPedOutOfCollision(door, veh, true);
                return;
            }
        }

        // Inherit (some of) the boat's momentum
        ped->m_vecMoveSpeed.x = veh->m_vecMoveSpeed.x * 0.9f;
        ped->m_vecMoveSpeed.y = veh->m_vecMoveSpeed.y * 0.9f;
        ped->m_vecMoveSpeed.z = veh->m_vecMoveSpeed.z * 0.9f;
        ped->m_vecMoveSpeed.z -= 0.1f;
        ped->bIsStanding = true;
        if (!ped->m_standingOnEntity) {
            ped->m_standingOnEntity = veh;
            veh->RegisterReference(&ped->m_standingOnEntity);
        }
        return;
    }

    CWorld::pIgnoreEntity = veh;

    bool bBlocked = false;
    const auto TestSphere = [&](float zOffset) {
        const auto& p = ped->GetPosition();
        const auto  e = CWorld::TestSphereAgainstWorld(CVector{p.x, p.y, p.z + zOffset}, 0.4f, veh, true, true, false, false, false, false);
        if (e && e != veh->m_pAttachedTo) {
            bBlocked = true;
        }
    };
    TestSphere(-0.2f);
    TestSphere(+0.2f);

    if (!CWorld::GetIsLineOfSightClear(veh->GetPosition(), ped->GetPosition(), true, false, false, true, false, false, false) || bBlocked) {
        ped->PositionPedOutOfCollision(door, veh, true);
    }
    CWorld::pIgnoreEntity = nullptr;
}

CTask* CTaskSimpleCarSetPedOut::Clone() const {
    return plugin::CallMethodAndReturn<CTask*, 0x649F50, const CTask*>(this);
}

bool CTaskSimpleCarSetPedOut::ProcessPed(CPed* ped) {
    return plugin::CallMethodAndReturn<bool, 0x647D10, CTask*, CPed*>(this, ped);
}
