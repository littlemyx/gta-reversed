#include "StdInc.h"

#include <bit>
#include <numbers>

#include "CollisionEventScanner.h"

#include "EventAcquaintancePedHate.h"
#include "EventBuildingCollision.h"
#include "EventDamage.h"
#include "EventGlobalGroup.h"
#include "EventObjectCollision.h"
#include "EventPedCollisionWithPed.h"
#include "EventPedCollisionWithPlayer.h"
#include "EventPlayerCollisionWithPed.h"
#include "EventSoundQuiet.h"
#include "EventVehicleCollision.h"
#include "PedClothesDesc.h"
#include "PedDamageResponse.h"
#include "PedDamageResponseCalculator.h"
#include "PedGeometryAnalyser.h"
#include "PedType.h"
#include "PlayerPedData.h"
#include "TaskSimpleGoTo.h"
#include "Weapon.h"

// Sanity checks for the raw offsets used by the original
static_assert(offsetof(CPed, m_pContactEntity) == 0x584 && offsetof(CPed, m_standingOnEntity) == 0x568);
static_assert(offsetof(CPed, m_vecAnimMovingShift) == 0x550);
static_assert(offsetof(CPhysical, m_fMass) == 0x8C && offsetof(CPhysical, m_pAttachedTo) == 0xFC && offsetof(CPhysical, m_pEntityIgnoredCollision) == 0x128);
static_assert(offsetof(CPhysical, m_fDamageIntensity) == 0xD8 && offsetof(CPhysical, m_pDamageEntity) == 0xDC && offsetof(CPhysical, m_nPieceType) == 0xF8);
static_assert(offsetof(CPhysical, m_vecLastCollisionImpactVelocity) == 0xE0 && offsetof(CPhysical, m_vecLastCollisionPosn) == 0xEC && offsetof(CPhysical, m_vecTurnSpeed) == 0x50);

namespace {
//! 0x40FDB0 - `a.z * b.z + a.y * b.y + a.x * b.x` (the result is left in an x87 register => extended precision)
double DotProductX87(const CVector& a, const CVector& b) {
    return (double)a.z * b.z + (double)a.y * b.y + (double)a.x * b.x;
}

//! 0x59C890 - the original evaluation order; each component is summed in extended precision, stored as float
CVector MultiplyMatrixWithVectorX87(const CMatrix& m, const CVector& v) {
    const auto& r = m.GetRight();
    const auto& f = m.GetForward();
    const auto& u = m.GetUp();
    const auto& p = m.GetPosition();
    return {
        (float)((double)u.x * v.z + (double)f.x * v.y + (double)r.x * v.x + (double)p.x),
        (float)((double)u.y * v.z + (double)r.y * v.x + (double)f.y * v.y + (double)p.y),
        (float)((double)u.z * v.z + (double)r.z * v.x + (double)f.z * v.y + (double)p.z),
    };
}

//! 0x821B40 (_ftol2) truncates to int64 (0x8000000000000000 on overflow/NaN) and returns the low 32 bits
int32 Ftol(double v) {
    if (!(v > -9223372036854775808.0 && v < 9223372036854775808.0)) {
        return 0;
    }
    return (int32)(int64)v;
}
}; // namespace

void CCollisionEventScanner::InjectHooks() {
    RH_ScopedClass(CCollisionEventScanner);
    RH_ScopedCategory("Collision");

    RH_ScopedInstall(ScanForCollisionEvents, 0x604500);
}

// 0x604500
void CCollisionEventScanner::ScanForCollisionEvents(CPed* victim, CEventGroup* eventGroup) {
    CPed& ped = *victim;

    [&] { // NOTSA: Wrapper so that early exits (`goto 0x6052C4` in the original) still reach the epilogue below
        if (!ped.m_pDamageEntity || ped.m_fDamageIntensity == 0.f) { // 0x604523, 0x604535
            return;
        }

        auto* const intel = ped.GetIntelligence();

        // 0x604557 - Move state of the victim (EBP in the original)
        const auto goToTask = intel->m_TaskMgr.GetSimplestActiveTask();
        const eMoveState victimMoveState = goToTask && CTask::IsGoToTask(goToTask)
            ? static_cast<CTaskSimpleGoTo*>(goToTask)->m_moveState
            : PEDMOVE_STILL;

        CEntity* const hitEntity = ped.m_pDamageEntity;
        auto* const    hitPhys   = static_cast<CPhysical*>(hitEntity);

        switch (hitEntity->GetType()) { // 0x604596 (jump table @ 0x6052E4)
        case ENTITY_TYPE_BUILDING: { // 0x605102
            CEventBuildingCollision event{
                (int16)ped.m_nPieceType,
                ped.m_fDamageIntensity,
                static_cast<CBuilding*>(hitEntity),
                ped.m_vecLastCollisionImpactVelocity,
                ped.m_vecLastCollisionPosn,
                (int16)victimMoveState
            };
            eventGroup->Add(&event, false);
            break;
        }
        case ENTITY_TYPE_VEHICLE: { // 0x60459D
            if (m_bAlreadyHitByCar) {
                break;
            }
            auto* const veh = static_cast<CVehicle*>(hitEntity);

            const CVector& vehSpeed = veh->m_vecMoveSpeed;
            const float    speedSq  = (float)((double)vehSpeed.x * vehSpeed.x + (double)vehSpeed.y * vehSpeed.y + (double)vehSpeed.z * vehSpeed.z); // 0x406DA0, stored as float
            // 0x86C924 / 0x86C920 - Both are 0x3B23D70B (not 0.0025f, which is 0x3B23D70A)
            const float    threshold = veh->m_pTowingVehicle ? std::bit_cast<float>(0x3B23D70Bu) : std::bit_cast<float>(0x3B23D70Bu);

            // 0x6045D8
            if (veh->m_nVehicleSubType == VEHICLE_TYPE_TRAIN
                && veh->physicalFlags.bDisableCollisionForce
                && ped.m_bIsStuck
                && ped.bIsStanding
                && !ped.m_standingOnEntity
                && speedSq > 0.0001f // 0x858FC4 (0x38D1B717)
            ) {
                ped.KillPedWithCar(veh, 15.f, false); // 0x60461F
            }

            if (speedSq > threshold) { // 0x604624
                float damage = ped.m_fDamageIntensity; // 0x60463E
                if (ped.bIsStanding) { // 0x604637
                    const double dot = (double)ped.m_vecLastCollisionImpactVelocity.x * ped.m_vecAnimMovingShift.x + (double)ped.m_vecLastCollisionImpactVelocity.y * ped.m_vecAnimMovingShift.y;
                    if (dot < 0.0) {
                        const double v = dot * ped.m_fMass + damage;
                        damage = (0.0 > v) ? 0.f : (float)v;
                    }
                }

                if (ped.IsPlayer()) { // 0x60469C
                    if (damage > 20.f) { // 0x858BA4
                        damage = 20.f;
                    }

                    const float heading = veh->GetHeading(); // 0x6046C4
                    const auto  colModel = veh->GetColModel();
                    const CVector bbMin = colModel->GetBoundingBox().m_vecMin; // 0x6046CF
                    const CVector bbMax = colModel->GetBoundingBox().m_vecMax; // 0x6046EA
                    const CVector bbCenterOS = CVector{ bbMax.x + bbMin.x, bbMin.y + bbMax.y, bbMax.z + bbMin.z } / 2.f; // 0x4119D0
                    const CVector bbCenterWS = MultiplyMatrixWithVectorX87(veh->GetMatrix(), bbCenterOS); // 0x59C890

                    const CVector pedPos = ped.GetPosition();
                    const CVector pedToCenter = bbCenterWS - pedPos; // 0x40FE60 (0x604791)
                    const auto    centerAngle = x87::atan2((double)-pedToCenter.x, (double)pedToCenter.y); // fpatan, 0x6047AD
                    const float   angleDiff   = CGeneral::LimitRadianAngle((float)((double)heading - centerAngle)); // 0x6047BF

                    const float halfAngle = (float)x87::atan2((double)bbMax.x - (double)bbMin.x, (double)bbMax.y - (double)bbMin.y); // 0x6047EA

                    CVector dir = pedPos - veh->GetPosition(); // 0x60480E
                    dir.Normalise(); // 0x60481D

                    const float absAngle = std::abs(angleDiff);
                    double      result;
                    if (absAngle < halfAngle || ((double)std::numbers::pi_v<float> - (double)halfAngle < (double)absAngle)) { // 0x604828, pi = 0x858CB8
                        result = (double)dir.y * vehSpeed.y + (double)dir.z * vehSpeed.z + (double)dir.x * vehSpeed.x; // 0x6048DF
                    } else if (angleDiff > 0.f) { // 0x604850
                        const CVector negRight = -1.f * veh->GetMatrix().GetRight(); // 0x40FE90
                        result = DotProductX87(negRight, vehSpeed);
                    } else { // 0x604886
                        const double r1 = DotProductX87(veh->GetMatrix().GetRight(), vehSpeed);
                        result = r1;
                        if (speedSq > std::bit_cast<float>(0x3C23D70Bu) && r1 < (double)0.1f) { // 0x86CD7C (0x3C23D70B, not 0.01f), 0x858B1C
                            // The original calculates `Dot(vehRight, pedForward)` here, but its only effect is a dead store.
                            result = (double)(float)r1; // It reloads the (float) copy of `r1` on the x87 stack
                        }
                    }
                    if (result > (double)0.1f) { // 0x858B1C
                        ped.KillPedWithCar(veh, damage, false); // 0x60491B
                    }
                } else { // 0x604925
                    const auto attachedTo = ped.m_pAttachedTo;
                    if (attachedTo && attachedTo->GetType() == ENTITY_TYPE_VEHICLE) { // 0x604933
                        const auto hitSide = CPedGeometryAnalyser::ComputePedHitSide(ped, *veh); // 0x604944
                        CPedDamageResponseCalculator calc{ veh, ped.m_fDamageIntensity, WEAPON_RAMMEDBYCAR, PED_PIECE_TORSO, false }; // 0x604966
                        CEventDamage event{ veh, CTimer::GetTimeInMS(), WEAPON_RAMMEDBYCAR, PED_PIECE_TORSO, (uint8)hitSide, false, ped.bInVehicle }; // 0x60499C
                        if (event.AffectsPed(&ped)) { // 0x6049B1
                            CPedDamageResponse response;
                            calc.ComputeDamageResponse(&ped, response, true); // 0x6049C9
                        }
                    } else { // 0x6049FB
                        ped.KillPedWithCar(veh, ped.m_fDamageIntensity, false); // 0x604A0D
                    }
                }
            } else { // 0x604A17
                const auto attachedTo = ped.m_pAttachedTo;
                if (attachedTo && attachedTo->GetType() == ENTITY_TYPE_VEHICLE) { // 0x604A1F
                    const auto hitSide = CPedGeometryAnalyser::ComputePedHitSide(ped, *veh); // 0x604A34
                    CPedDamageResponseCalculator calc{ veh, ped.m_fDamageIntensity, WEAPON_RAMMEDBYCAR, PED_PIECE_TORSO, false }; // 0x604A56
                    CEventDamage event{ veh, CTimer::GetTimeInMS(), WEAPON_RAMMEDBYCAR, PED_PIECE_TORSO, (uint8)hitSide, false, ped.bInVehicle }; // 0x604A8B
                    if (event.AffectsPed(&ped)) { // 0x604AA0
                        CPedDamageResponse response;
                        calc.ComputeDamageResponse(&ped, response, true); // 0x604AB8
                    }
                } else if (!ped.IsPlayer()) { // 0x604ACA
                    CEventVehicleCollision event{
                        (int16)ped.m_nPieceType,
                        ped.m_fDamageIntensity,
                        veh,
                        ped.m_vecLastCollisionImpactVelocity,
                        ped.m_vecLastCollisionPosn,
                        (int8)intel->GetMoveStateFromGoToTask(), // 0x604AE1
                        0
                    };
                    eventGroup->Add(&event, false);
                }
            }
            break;
        }
        case ENTITY_TYPE_PED: { // 0x604B56
            auto* const hitter      = static_cast<CPed*>(hitEntity);
            auto* const hitterIntel = hitter->GetIntelligence();
            const auto  hitterMoveState = hitterIntel->GetMoveStateFromGoToTask(); // 0x604B5C

            if (ped.IsPlayer()) { // 0x604B67
                CEventPlayerCollisionWithPed event{
                    (int16)ped.m_nPieceType,
                    ped.m_fDamageIntensity,
                    hitter,
                    ped.m_vecLastCollisionImpactVelocity,
                    ped.m_vecLastCollisionPosn,
                    victimMoveState,
                    hitterMoveState
                };
                eventGroup->Add(&event, false);
                hitter->Say(CTX_GLOBAL_BUMP, 0, 1.f, false, false, false); // 0x604BD8
            } else if (hitter->IsPlayer()) { // 0x604BEE
                CEventPedCollisionWithPlayer event{
                    (int16)ped.m_nPieceType,
                    ped.m_fDamageIntensity,
                    hitter,
                    ped.m_vecLastCollisionImpactVelocity,
                    ped.m_vecLastCollisionPosn,
                    victimMoveState,
                    hitterMoveState
                };
                eventGroup->Add(&event, false);
                intel->IncrementAngerAtPlayer(1); // 0x604C62
                ped.Say(CTX_GLOBAL_BUMP, 0, 1.f, false, false, false); // 0x604C78
                if (ped.GetAcquaintance().GetAcquaintances(ACQUAINTANCE_HATE) & CPedType::GetPedFlag(hitter->m_nPedType)) { // 0x604C85
                    CEventAcquaintancePedHate hateEvent{ hitter }; // 0x604CAA
                    eventGroup->Add(&hateEvent, false);
                }
            } else { // 0x604CF0
                CEventPedCollisionWithPed event{
                    (int16)ped.m_nPieceType,
                    ped.m_fDamageIntensity,
                    hitter,
                    ped.m_vecLastCollisionImpactVelocity,
                    ped.m_vecLastCollisionPosn,
                    victimMoveState,
                    hitterMoveState
                };
                eventGroup->Add(&event, false);
            }

            // 0x604D3E - Let the other ped know about the collision as well (if it didn't collide with anything else itself)
            if (hitter->m_pDamageEntity) {
                break;
            }
            const CVector invVelocity{
                (float)((double)ped.m_vecLastCollisionImpactVelocity.x * -1.0), // 0x858C1C
                (float)((double)ped.m_vecLastCollisionImpactVelocity.y * -1.0),
                (float)((double)ped.m_vecLastCollisionImpactVelocity.z * -1.0),
            };
            if (hitter->IsPlayer()) { // 0x604D8C
                CEventPlayerCollisionWithPed event{
                    (int16)ped.m_nPieceType,
                    ped.m_fDamageIntensity,
                    &ped,
                    invVelocity,
                    ped.m_vecLastCollisionPosn,
                    victimMoveState,
                    hitterMoveState
                };
                hitterIntel->m_eventGroup.Add(&event, false);
            } else if (ped.IsPlayer()) { // 0x604E0D
                CEventPedCollisionWithPlayer event{
                    (int16)ped.m_nPieceType,
                    ped.m_fDamageIntensity,
                    &ped,
                    invVelocity,
                    ped.m_vecLastCollisionPosn,
                    victimMoveState,
                    hitterMoveState
                };
                hitterIntel->m_eventGroup.Add(&event, false);
                hitterIntel->IncrementAngerAtPlayer(1); // 0x604E76
                if (hitter->GetAcquaintance().GetAcquaintances(ACQUAINTANCE_HATE) & CPedType::GetPedFlag(ped.m_nPedType)) { // 0x604E83
                    CEventAcquaintancePedHate hateEvent{ &ped }; // 0x604EA2
                    hitterIntel->m_eventGroup.Add(&hateEvent, false);
                }
            } else { // 0x604F03
                CEventPedCollisionWithPed event{
                    (int16)ped.m_nPieceType,
                    ped.m_fDamageIntensity,
                    &ped,
                    invVelocity,
                    ped.m_vecLastCollisionPosn,
                    victimMoveState,
                    hitterMoveState
                };
                hitterIntel->m_eventGroup.Add(&event, false);
            }
            break;
        }
        case ENTITY_TYPE_OBJECT: { // 0x604F70
            const bool bStanding = ped.bIsStanding;
            float      damage    = ped.m_fDamageIntensity;
            if (bStanding && !hitEntity->GetIsStatic()) { // 0x604F83
                const double dot = (double)ped.m_vecLastCollisionImpactVelocity.x * ped.m_vecAnimMovingShift.x + (double)ped.m_vecLastCollisionImpactVelocity.y * ped.m_vecAnimMovingShift.y;
                if (dot < 0.0) {
                    const float v = (float)(dot * ped.m_fMass + damage); // 0x604FBF - passed as a float to 0x420800 (`max(0, v)`)
                    damage = (0.f > v) ? 0.f : v;
                }
            }
            const float threshold = ped.GetPlayerData() ? 2.f : 1.f; // 0x8D23AC : 0x8D23B0

            if (!(damage > threshold)
                || hitEntity->GetIsStatic()
                || !bStanding
                || hitEntity == ped.m_pContactEntity
                || (hitPhys->m_pAttachedTo && hitEntity == hitPhys->m_pAttachedTo)
            ) { // 0x605000
                CEventObjectCollision event{
                    (int16)ped.m_nPieceType,
                    ped.m_fDamageIntensity,
                    static_cast<CObject*>(hitEntity),
                    ped.m_vecLastCollisionImpactVelocity,
                    ped.m_vecLastCollisionPosn,
                    (int16)victimMoveState
                };
                eventGroup->Add(&event, false);
            } else { // 0x605036
                const float scaledDamage = (float)((double)damage / threshold * 10.0); // 0x8D23A8
                const CVector2D dir{ -ped.m_vecLastCollisionImpactVelocity.x, -ped.m_vecLastCollisionImpactVelocity.y };
                const auto localDir = ped.GetLocalDirection(dir); // 0x60506F
                CWeapon::GenerateDamageEvent(&ped, ped.m_pDamageEntity, WEAPON_FALL, Ftol(scaledDamage), PED_PIECE_TORSO, (uint8)localDir); // 0x60508B
                ped.m_pEntityIgnoredCollision = ped.m_pDamageEntity; // 0x605099
            }
            break;
        }
        default:
            break;
        }

        // 0x60515F - Wearing a balaclava makes noise when hitting buildings/objects
        if (!ped.GetPlayerData()) {
            return;
        }
        const auto hitEntityType = ped.m_pDamageEntity->GetType(); // Note: Re-read, as in the original
        if (hitEntityType != ENTITY_TYPE_BUILDING && hitEntityType != ENTITY_TYPE_OBJECT) {
            return;
        }
        if (!ped.GetPlayerData()->m_pPedClothesDesc->GetIsWearingBalaclava()) { // 0x60518E
            return;
        }

        auto* const hitPhysical = static_cast<CPhysical*>(ped.m_pDamageEntity);
        float       volume;
        if (hitPhysical->GetIsTypePhysical() && hitPhysical->m_nPhysicalFlags & 0x20) { // 0x6051A3
            if (!(std::abs(hitPhysical->m_vecTurnSpeed.z) > 0.04f)) { // 0x8D23A4
                return;
            }
            if (CTimer::GetTimeInMS() <= ms_LastSpinningNoiseTime + 2000) {
                return;
            }
            volume = 40.f; // 0x42200000
            ms_LastSpinningNoiseTime = CTimer::GetTimeInMS();
        } else { // 0x6051F0
            if (!(ped.m_fDamageIntensity > 1.f)) { // 0x8D23A0
                return;
            }
            const auto now = CTimer::GetTimeInMS();
            if (now <= ms_LastImpactNoiseTime + 1000) {
                return;
            }
            volume = ped.m_fDamageIntensity > 3.f ? 40.f : 30.f; // 0x8D239C, 0x858A10, 0x858CA4
            ms_LastImpactNoiseTime = now;
            if (!(volume > 0.f)) {
                return;
            }
        }

        // 0x605259
        const CVector pos{ 0.f, 0.f, 0.f };
        CEventSoundQuiet event{ &ped, volume, (uint32)-1, pos };
        GetEventGlobalGroup()->Add(&event, false);
    }();

    m_bAlreadyHitByCar = false; // 0x6052C4
}
