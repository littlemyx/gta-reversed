#include "StdInc.h"

#include "PedStuckChecker.h"
#include "Events/EventGroup.h"
#include "Events/EventInWater.h"
#include "Events/EventStuckInAir.h"

namespace {
// 0x59C910 - `CVector::Normalise` as the original evaluates it: the sum of squares and the reciprocal root stay in the FPU
// (extended precision), every component is stored as float. A length of 0 (or less) only writes `x = 1` (NaN takes the sqrt path).
void NormaliseExt(CVector& v) {
    const double sumSq = ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
    if (sumSq <= 0.0) {
        v.x = 1.0f;
    } else {
        const double recip = 1.0 / std::sqrt(sumSq);
        v.x = (float)(v.x * recip);
        v.y = (float)(v.y * recip);
        v.z = (float)(v.z * recip);
    }
}
}; // namespace

void CPedStuckChecker::InjectHooks() {
    RH_ScopedClass(CPedStuckChecker);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(TestPedStuck, 0x602C00);
}

// 0x602C00
bool CPedStuckChecker::TestPedStuck(CPed* ped, CEventGroup* eventGroup) {
    if (!ped->m_bUsesCollision
        || ped->m_pAttachedTo
        || ped->m_nPedState == PEDSTATE_DIE  // 0x36
        || ped->m_nPedState == PEDSTATE_DEAD // 0x37
    ) {
        m_radius = 0;
        m_state  = PED_STUCK_STATE_NONE;
        return false;
    }

    const auto ResetState = [this] { // 0x602F6E
        m_radius = 0;
        m_state  = PED_STUCK_STATE_NONE;
    };

    // The `return` statements in this lambda are jumps to 0x602F76 (the state is evaluated afterwards)
    [&] {
        if (ped->bIsStanding || ped->bWasStanding) {
            return ResetState();
        }
        if (!(ped->m_pDamageEntity && ped->m_fDamageIntensity > 0.0f)) {
            if (!ped->IsPlayer() || !ped->GetIsStuck()) { // 0x5DF8F0
                return ResetState();
            }
        }
        {
            auto& intel = *ped->GetIntelligence();
            if (intel.GetTaskSwim() || intel.GetTaskJetPack() || intel.GetTaskClimb()) { // 0x601070, 0x601110, 0x601180
                return ResetState();
            }
        }

        // Distance moved since the last check
        CVector delta{};
        if (m_radius <= 10'000 && m_radius != 0) {
            delta = ped->GetPosition() - m_lastNonStuckPoint; // 0x40FE60
            m_radius++;
        } else {
            m_radius          = 1;
            m_state           = PED_STUCK_STATE_NONE;
            m_lastNonStuckPoint = ped->GetPosition();
        }

        // x87: the limits are kept in extended precision, the time step is clamped (NaN is passed through)
        const float timeStep = (0.01f > CTimer::ms_fTimeStep) ? 0.01f : CTimer::ms_fTimeStep; // 0x858C58
        const float radiusF  = (float)m_radius;
        const auto  CanBeStuckForThisLong = [&](double tsFactor) { // `radius > 50 / (tsFactor * ts)`
            return (double)radiusF > 50.0 / (tsFactor * (double)timeStep);                      // 0x858B40
        };
        const auto HasBarelyMovedInZ = [&] {
            return (double)radiusF * (double)0.004f > (double)std::abs(delta.z); // 0x859CE0
        };

        bool attemptUnstuck{};
        {
            const double sqDist = ((double)delta.x * delta.x + (double)delta.y * delta.y) + (double)delta.z * delta.z; // 0x406DA0
            if (CanBeStuckForThisLong(4.0) && (double)radiusF * (double)0.01f > sqDist) {
                attemptUnstuck = true;
            }
        }

        bool wallResponse{};
        if (!attemptUnstuck) {
            const auto numCollided = ped->m_nNumEntitiesCollided;
            if (numCollided > 1) {
                if (!CanBeStuckForThisLong(2.0) || !HasBarelyMovedInZ()) {
                    return;
                }
                attemptUnstuck = true;
            } else {
                if (numCollided != 1) {
                    return;
                }
                const auto* const touched = ped->m_apCollidedEntities[0];
                if (!touched || !touched->GetIsTypeBuilding()) {
                    return;
                }
                if (!CanBeStuckForThisLong(2.0) || !HasBarelyMovedInZ()) {
                    return;
                }
                wallResponse = true;
            }
        }

        if (attemptUnstuck) { // 0x602E40
            if (((uint8)ped->m_nRandomSeed + 3 + CTimer::m_FrameCounter) & 7) { // Only try to unstuck every 8th frame
                m_state = PED_STUCK_STATE_STUCK;
                return;
            }

            CVector origin = ped->GetPosition();
            origin.z += 1.0f;

            CColPoint colPoint;
            CEntity*  hitEntity{};
            if (!CWorld::ProcessVerticalLine(origin, ped->GetPosition().z - 1.0f, colPoint, hitEntity, true, true, false, true, false, false, nullptr)) {
                return;
            }

            if (ped->bHeadStuckInCollision) {
                if (!((double)colPoint.m_vecPoint.z + 1.0 < (double)ped->GetPosition().z)) { // 0x602EF0
                    ped->bIsStanding = true;
                    return ResetState();
                }
            }

            const bool pushedByPed = ped->m_fDamageIntensity > 0.0f && ped->m_pDamageEntity && ped->m_pDamageEntity->GetIsTypePed()
                && ped->m_vecLastCollisionImpactVelocity.z > 0.3f; // 0x858C24
            if (!pushedByPed) {
                origin.z = (float)((double)colPoint.m_vecPoint.z + 1.0);
                ped->SetPosn(origin); // 0x4241C0
                ped->bHeadStuckInCollision = false;
            }
            ped->bIsStanding = true;
            return ResetState();
        }

        if (wallResponse) { // 0x603048
            CVector dir{ -delta.y, delta.x, 1.0f };
            NormaliseExt(dir); // 0x59C910

            // Probe the ground on either side of the ped (perpendicular to the movement)
            const auto ProbeSide = [&](bool left, float noHitValue, CColPoint& colPoint) -> float {
                const auto  GetProbePos = [&] { return left ? ped->GetPosition() + dir : ped->GetPosition() - dir; }; // 0x40FE30 / 0x40FE60
                CEntity*    hitEntity{};
                if (!CWorld::ProcessVerticalLine(GetProbePos(), -20.0f, colPoint, hitEntity, true, false, false, false, false, false, nullptr)) {
                    return noHitValue;
                }
                if (!CWorld::GetIsLineOfSightClear(ped->GetPosition(), GetProbePos(), true, true, false, true, false, false, false)) {
                    return noHitValue;
                }
                return colPoint.m_vecPoint.z;
            };
            CColPoint colPoint1, colPoint2;
            const float a0 = ProbeSide(true, 5001.0f, colPoint1);  // 0x459C4800
            const float a4 = ProbeSide(false, 5002.0f, colPoint2); // 0x459C5000

            int16 side = 0;
            if (a0 > 5000.0f && a4 > 5000.0f) { // 0x86CD78
                side = -1; // Nothing found
            } else {
                const auto  pedZ     = [&] { return (double)ped->GetPosition().z - 1.0; };
                if (pedZ() < a0 && a0 < 5000.0f && (a4 < 5001.0f || a0 < a4)) { // 0x86CD74
                    side = 1;
                } else if (pedZ() < a4 && a4 < 5001.0f) {
                    side = 2;
                }
                if (side > 0 && !ped->bHeadStuckInCollision) {
                    const bool pushedByPed = ped->m_fDamageIntensity > 0.0f && ped->m_pDamageEntity && ped->m_pDamageEntity->GetIsTypePed()
                        && ped->m_vecLastCollisionImpactVelocity.z > 0.3f; // 0x858C24
                    if (!pushedByPed) {
                        ped->SetPosn(side == 2 ? colPoint2.m_vecPoint : colPoint1.m_vecPoint); // 0x4241C0
                        ped->GetPosition().z += 1.0f;
                    }
                }
            }

            if (a0 < a4) {
                dir.z *= -1.0f; // 0x858C1C
                dir.y *= -1.0f;
                dir.x *= -1.0f;
            }
            dir.z = 1.0f;
            ped->ApplyMoveForce(CVector{ dir.x * 4.0f, dir.y * 4.0f, 4.0f }); // 0x5429F0

            if (side >= 0) {
                const CVector step{ dir.x * 0.25f, dir.y * 0.25f, dir.z * 0.25f }; // 0x858C84
                ped->SetPosn(ped->GetPosition() + step);

                const auto angle = CGeneral::GetRadianAngleBetweenPoints(dir.x, dir.y, 0.0f, 0.0f); // 0x53CBE0
                ped->m_fCurrentRotation = angle;
                const auto limited = CGeneral::LimitRadianAngle(angle); // 0x53CB50
                ped->m_fCurrentRotation = limited;
                ped->m_fAimingRotation  = limited;
                ped->SetOrientation(0.0f, 0.0f, limited); // 0x439A80
            }
            m_state = PED_STUCK_STATE_WAS_STUCK;
        }
    }();

    if (m_state == PED_STUCK_STATE_NONE) {
        return false;
    }

    // 0x602F80
    const auto* const inWater = static_cast<CEventInWater*>(eventGroup->GetEventOfType(EVENT_IN_WATER));
    if (!inWater || !(inWater->m_acceleration > 1.0f)) {
        eventGroup->Add(CEventStuckInAir{ ped }, false);
        return true;
    }
    ped->bIsStanding = false;
    return false;
}
