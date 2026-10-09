#include "StdInc.h"

#include "TaskComplexGangFollower.h"

#include "Ragdoll/IKChainManager.h"
#include "ModelIndices.h"
#include "EventPassObject.h"
#include "TaskComplexGangLeader.h"
#include "TaskComplexPassObject.h"
#include "TaskSimpleHoldEntity.h"
#include "TaskSimpleRunAnim.h"
#include "TaskComplexPlayHandSignalAnim.h"
#include "TaskComplexWanderGang.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "TaskComplexSignalAtPed.h"
#include "TaskComplexLeaveCar.h"
#include "TaskSimpleCarDrive.h"
#include "TaskSimplePause.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleGoToPoint.h"
#include "SeekEntity/TaskComplexSeekEntity.h"
#include "TaskComplexFollowLeaderInFormation.h"

void CTaskComplexGangFollower::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGangFollower, 0x86F938, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x65EAA0);
    RH_ScopedInstall(Destructor, 0x65EBB0);
    RH_ScopedInstall(CalculateOffsetPosition, 0x65ED40); // returns `CVector` by value == hidden sret pointer on stack, same as the original `CVector&` param
    RH_ScopedVMTInstall(Clone, 0x65ECB0);
    RH_ScopedVMTInstall(MakeAbortable, 0x65EC30);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x665E00);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x666160);
    RH_ScopedVMTInstall(ControlSubTask, 0x662A10);
}

// 0x65EAA0
CTaskComplexGangFollower::CTaskComplexGangFollower(CPedGroup* pedGroup, CPed* ped, uint8 a4, CVector pos, float a6) :
    CTaskComplex{},
    m_PedGroup{pedGroup},
    m_Leader{ped},
    m_PedPosn{},
    m_Offset{pos},
    m_BaseOffset{pos},
    m_fArg38{a6},
    m_Arg3C{a4}
{
    m_bUseSeekEntity = true;
    m_bFlag4         = true;

    CEntity::SafeRegisterRef(m_Leader);
    if (m_Leader) {
        m_PedPosn = m_Leader->GetPosition();
    }
    m_bAnimsReferenced = false;
    m_bSignalAtLeader  = false;
    m_bLeaderIsPlayer  = m_Leader == FindPlayerPed(0);
}

// 0x65EBB0
CTaskComplexGangFollower::~CTaskComplexGangFollower() {
    CEntity::SafeCleanUpRef(m_Leader);

    if (m_bAnimsReferenced) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_bAnimsReferenced = false;
    }
}

// 0x65ED40
CVector CTaskComplexGangFollower::CalculateOffsetPosition() {
    CPed* const leader = m_Leader;

    const auto moveState = leader->m_nMoveState;
    const bool bLeaderMoving = moveState == PEDMOVE_WALK || moveState == PEDMOVE_RUN || moveState == PEDMOVE_SPRINT;

    // Squared distance the leader moved since the last update (the original evaluates it in extended precision: (dz^2 + dy^2) + dx^2)
    const auto& leaderPos = leader->GetPosition();
    const double dx = (double)leaderPos.x - (double)m_PedPosn.x;
    const double dy = (double)leaderPos.y - (double)m_PedPosn.y;
    const double dz = (double)leaderPos.z - (double)m_PedPosn.z;
    const double distSq = (dz * dz + dy * dy) + dx * dx;

    if (bLeaderMoving) {
        const auto& moving = CTaskComplexFollowLeaderInFormation::ms_offsets.MovingOffsets[m_Arg3C];
        m_Offset.FromMultiply3x3(*leader->m_matrix, CVector{moving.x, moving.y, 0.f});
        m_bFlag4 = false;
    } else if (distSq > 9.0 || !m_bFlag4) { // skipped only if `!(distSq > 9.0f)` (NaN-safe) and the flag is set
        m_PedPosn = leaderPos;
        const auto& standing = CTaskComplexFollowLeaderInFormation::ms_offsets.Offsets[m_Arg3C];
        m_Offset  = CVector{standing.x, standing.y, 0.f};
        m_bFlag4  = true;
    }
    return m_Offset;
}

// 0x65ECB0
CTask* CTaskComplexGangFollower::Clone() const {
    const auto clone = new CTaskComplexGangFollower{m_PedGroup, m_Leader, m_Arg3C, m_Offset, m_fArg38};
    clone->m_bUseSeekEntity = m_bUseSeekEntity;
    return clone;
}

// 0x65EC30
bool CTaskComplexGangFollower::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (!m_pSubTask || m_pSubTask->MakeAbortable(ped, priority, event)) {
        ped->bDontAcceptIKLookAts            = false;
        ped->bMoveAnimSpeedHasBeenSetByTask = false;
        return true;
    }
    return false;
}

// 0x665E00
CTask* CTaskComplexGangFollower::CreateNextSubTask(CPed* ped) {
    if (!m_Leader) {
        ped->bMoveAnimSpeedHasBeenSetByTask = false;
        return nullptr;
    }

    const bool bTooManyCollisions = ped->GetIntelligence()->m_AnotherStaticCounter > 30;

    if (m_bSignalAtLeader && m_pSubTask->GetTaskType() == TASK_COMPLEX_SIGNAL_AT_PED) {
        // Signalled at the leader => leave the group and wander around as a gang member
        m_PedGroup->GetMembership().RemoveMember(ped);
        ped->GetTaskManager().SetTask(
            new CTaskComplexWanderGang{PEDMOVE_WALK, (uint8)CGeneral::GetRandomNumberInRange(0, 8), 30'000, true, 0.5f},
            TASK_PRIMARY_DEFAULT
        );
        ped->bMoveAnimSpeedHasBeenSetByTask = false;
        return nullptr;
    }

    if (bTooManyCollisions || (m_pSubTask->GetTaskType() == TASK_COMPLEX_SEEK_ENTITY && m_Leader->m_nMoveState < PEDMOVE_WALK)) {
        if (m_Leader && m_Leader->IsPlayer()) {
            ped->Say(CTX_GLOBAL_FOLLOW_ARRIVE, 0, 0.3f);
        }
        return new CTaskSimpleStandStill{500, false, false, 8.f};
    }

    if (m_pSubTask->GetTaskType() == TASK_SIMPLE_STAND_STILL || m_pSubTask->GetTaskType() == TASK_COMPLEX_HANDSIGNAL_ANIM) {
        if (m_bSignalAtLeader) {
            return new CTaskComplexSignalAtPed{m_Leader, -1, false};
        }
        if (CGeneral::GetRandomNumberInRange(0, 30) == 20) {
            auto& mem = m_PedGroup->GetMembership();
            auto  other = mem.GetMember(CGeneral::GetRandomNumberInRange(0, (int32)mem.CountMembers()));
            if (other == ped) {
                other = mem.GetLeader();
            }
            if (other) {
                return new CTaskComplexTurnToFaceEntityOrCoord{other, 0.5f, 0.2f};
            }
        }
        return new CTaskSimplePause{50};
    }

    if (m_pSubTask->GetTaskType() == TASK_SIMPLE_CAR_DRIVE) {
        return CreateFirstSubTask(ped);
    }

    if (m_bUseSeekEntity) {
        const auto seek = new CTaskComplexSeekEntity<CEntitySeekPosCalculatorXYOffset>{
            m_Leader,
            50'000,
            1'000,
            0.5f,
            5.f,
            2.f,
            false,
            false,
            CEntitySeekPosCalculatorXYOffset{m_Offset}
        };
        seek->SetIsTrackingEntity(true);
        seek->SetMoveState(PEDMOVE_SPRINT);
        return seek;
    }

    return new CTaskSimpleStandStill{500, false, false, 8.f};
}

// 0x666160
CTask* CTaskComplexGangFollower::CreateFirstSubTask(CPed* ped) {
    if (!m_Leader) {
        ped->bMoveAnimSpeedHasBeenSetByTask = false;
        return nullptr;
    }

    const bool bInVehicle = ped->bInVehicle;
    if (bInVehicle && ped->m_pVehicle && m_Leader->m_pVehicle == ped->m_pVehicle) {
        return new CTaskSimpleCarDrive{ped->m_pVehicle, nullptr, false};
    }

    if (bInVehicle && ped->m_pVehicle) {
        return new CTaskComplexLeaveCar{ped->m_pVehicle, 0, 0, true, false};
    }

    if (m_bUseSeekEntity && ped->GetIntelligence()->m_AnotherStaticCounter <= 30) {
        const auto seek = new CTaskComplexSeekEntity<CEntitySeekPosCalculatorXYOffset>{
            m_Leader,
            50'000,
            1'000,
            0.5f,
            5.f,
            2.f,
            false,
            true,
            CEntitySeekPosCalculatorXYOffset{m_Offset}
        };
        seek->SetIsTrackingEntity(true);
        seek->SetMoveState(PEDMOVE_SPRINT);
        return seek;
    }

    return new CTaskSimpleStandStill{500, false, false, 8.f};
}

// 0x662A10
CTask* CTaskComplexGangFollower::ControlSubTask(CPed* ped) {
    ped->bDontAcceptIKLookAts = false;

    if (!m_Leader) {
        if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            ped->bMoveAnimSpeedHasBeenSetByTask = false;
            return nullptr;
        }
        return m_pSubTask;
    }

    if (m_pSubTask->GetTaskType() == TASK_SIMPLE_CAR_DRIVE) {
        return m_pSubTask;
    }

    const auto walkAssoc = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_WALK);

    const auto moveState = ped->GetIntelligence()->GetMoveStateFromGoToTask();
    const bool bIsMoving = moveState == PEDMOVE_WALK || moveState == PEDMOVE_RUN || moveState == PEDMOVE_SPRINT;

    m_Offset = CalculateOffsetPosition();

    const CVector pedToLeader = m_Leader->GetPosition() - ped->GetPosition();
    const float   pedToLeaderDist2DSq = pedToLeader.y * pedToLeader.y + pedToLeader.x * pedToLeader.x;

    bool bSpeedAdjusted = false;
    if (m_pSubTask->GetTaskType() == TASK_COMPLEX_SEEK_ENTITY) {
        const bool bTooManyCollisions = ped->GetIntelligence()->m_AnotherStaticCounter > 8;
        if (m_bUseSeekEntity) {
            const auto seek = static_cast<CTaskComplexSeekEntity<CEntitySeekPosCalculatorXYOffset>*>(m_pSubTask);
            seek->GetSeekPosCalculator().SetOffset(m_Offset);
            seek->SetEntityMinDist2D(2.f);

            const auto goTo   = static_cast<CTaskSimpleGoToPoint*>(ped->GetTaskManager().FindActiveTaskByType(TASK_SIMPLE_GO_TO_POINT));
            const auto follow = ped->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_FOLLOW_NODE_ROUTE);
            if (goTo && !follow) {
                CVector target   = m_Leader->GetPosition() + m_Offset;
                CVector toTarget = target - ped->GetPosition();
                toTarget.z = 0.f;

                if (bIsMoving) {
                    const auto& leaderFwd = m_Leader->GetForward();
                    const float planeD    = -(target.y * leaderFwd.y + target.z * leaderFwd.z + target.x * leaderFwd.x);
                    if (DotProduct(ped->GetPosition(), leaderFwd) + planeD < 0.f) { // Ped is behind the leader
                        const float radius = goTo->m_fRadius;
                        if (toTarget.SquaredMagnitude() > (radius + 1.f) * (1.f + radius)) {
                            // Too far => speed up
                            if (walkAssoc) {
                                const float oldSpeed = walkAssoc->m_Speed;
                                ped->SetMoveAnimSpeed(walkAssoc);
                                const float newSpeed = walkAssoc->m_Speed;
                                if (approxEqual(oldSpeed, newSpeed, 0.013f)) {
                                    walkAssoc->m_Speed = oldSpeed;
                                } else if (newSpeed < oldSpeed) {
                                    walkAssoc->m_Speed = oldSpeed - 0.0125f;
                                } else {
                                    walkAssoc->m_Speed = oldSpeed + 0.0125f;
                                }
                                bSpeedAdjusted = true;
                            }
                        } else {
                            // Close enough => aim for a point in front of the leader, and slow down
                            target += leaderFwd * 2.f;
                            if (walkAssoc) {
                                walkAssoc->m_Speed = std::max(0.85f, walkAssoc->m_Speed - 0.0125f);
                                bSpeedAdjusted = true;
                            }
                        }
                    }
                }

                goTo->UpdatePoint(target, 0.5f, false);
            }

            if (bTooManyCollisions && pedToLeaderDist2DSq < 64.f && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
                m_Offset = CVector{pedToLeader.x, pedToLeader.y, 0.f};
                return new CTaskSimpleStandStill{500, false, false, 8.f};
            }
        }
    }
    ped->bMoveAnimSpeedHasBeenSetByTask = bSpeedAdjusted;

    // Make sure anims are loaded (if they can/need to be)
    if (m_bAnimsReferenced) {
        if (!CTaskComplexGangLeader::ShouldLoadGangAnims()) {
            CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
            m_bAnimsReferenced = false;
        }
    } else if (CTaskComplexGangLeader::ShouldLoadGangAnims()) {
        const auto blk = CAnimManager::GetAnimationBlockIndex("gangs");
        if (CAnimManager::GetAnimBlocks()[blk].IsLoaded) {
            CAnimManager::AddAnimBlockRef(blk);
            m_bAnimsReferenced = true;
        } else {
            CStreaming::RequestModel(IFPToModelId(blk), STREAMING_KEEP_IN_MEMORY);
        }
    }

    // Randomly decide to signal at the leader (and then leave the group)
    if (!m_PedGroup->m_bIsMissionGroup && !m_bLeaderIsPlayer && m_PedGroup->GetMembership().CountMembers() > 3) {
        if (CGeneral::GetRandomNumberInRange(0, 2000) == 500) {
            m_bSignalAtLeader = true;
        }
    }

    if (m_ExhaleTimer.IsStarted() && m_ExhaleTimer.IsOutOfTime()) {
        if (ped->GetRpClump()) {
            if (auto matrix = RwFrameGetMatrix(RpClumpGetFrame(ped->GetRpClump()))) {
                if (const auto fx = g_fxMan.CreateFxSystem("exhale", CVector{0.f, 0.1f, 0.f}, matrix)) {
                    fx->AttachToBone(ped, eBoneTag::BONE_HEAD);
                    fx->PlayAndKill();
                }
                m_ExhaleTimer.Stop();
            }
        }
    }

    if (!ped->IsVisible()) {
        return m_pSubTask;
    }

    // If ped isn't already looking at someone, find a random member (or the leader) to look at
    if (!g_ikChainMan.IsLooking(ped) && CGeneral::GetRandomNumberInRange(0, 100) > 95) {
        const auto lookTime = CGeneral::GetRandomNumberInRange(3000, 5000);
        CPed* lookAt = m_PedGroup->GetMembership().GetMember(CGeneral::GetRandomNumberInRange(0, 8));
        if (lookAt == ped) {
            lookAt = m_Leader;
        }
        if (lookAt) {
            g_ikChainMan.LookAt(
                "TaskGangFollower",
                ped,
                lookAt,
                lookTime,
                BONE_HEAD,
                nullptr,
                true,
                0.15f,
                500,
                3,
                false
            );
        }
    }

    if (!m_bAnimsReferenced || ped->m_nMoveState >= PEDMOVE_RUN) {
        return m_pSubTask;
    }

    // Randomly say something (for when the partial anims are played)
    const auto SaySomethingRandom = [ped] {
        switch (CGeneral::GetRandomNumberInRange(0, 10)) {
        case 0:
        case 1:
        case 2: ped->Say(CTX_GLOBAL_CHAT, 0, 1.f); break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7: ped->Say(CTX_GLOBAL_PCONV_GREET_MALE, 0, 1.f); break;
        case 8: ped->Say(CTX_GLOBAL_BOOZE_REQUEST, 0, 1.f); break;
        case 9: ped->Say(CTX_GLOBAL_SPLIFF_REQUEST, 0, 1.f); break;
        default: break;
        }
    };

    const auto pedHeldEntity = ped->GetEntityThatThisPedIsHolding();

    if (!pedHeldEntity) {
        // If they're already playing an anim, early out
        if (ped->IsPlayingHandSignal() || ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM)) {
            return m_pSubTask;
        }

        const auto rnd = CGeneral::GetRandomNumberInRange(0, 500);
        if (rnd > 50 && rnd < 56) { // 1%
            // BUG: `rand() & 0xFFFF` * 2^-15 * 8 [truncated] => [0, 7]
            const auto animIdx = (int32)((float)(CGeneral::GetRandomNumber() & 0xFFFF) * (1.f / 32768.f) * 8.f);
            ped->GetTaskManager().SetTaskSecondary(
                new CTaskSimpleRunAnim{ANIM_GROUP_GANGS, (AnimationId)(ANIM_ID_PRTIAL_GNGTLKA + animIdx), 4.f, false},
                TASK_SECONDARY_PARTIAL_ANIM
            );
            if (ped->m_nMoveState == PEDMOVE_STILL) {
                SaySomethingRandom();
            }
        } else if (rnd == 100 && ped->m_nMoveState == PEDMOVE_STILL) { // 0.2%
            ped->GetTaskManager().SetTaskSecondary(
                new CTaskComplexPlayHandSignalAnim{ANIM_ID_UNDEFINED, 4.f},
                TASK_SECONDARY_PARTIAL_ANIM
            );
            SaySomethingRandom();
        }
        return m_pSubTask;
    }

    const auto GetAnim = [ped](AnimationId id) { return RpAnimBlendClumpGetAssociation(ped->GetRpClump(), id); };
    const auto animDrink     = GetAnim(ANIM_ID_DRNKBR_PRTL);
    const auto animSmoke     = GetAnim(ANIM_ID_SMKCIG_PRTL);
    const auto animDrinkF    = GetAnim(ANIM_ID_DRNKBR_PRTL_F);
    const auto animSmokeF    = GetAnim(ANIM_ID_SMKCIG_PRTL_F);
    const bool anyOfTheAnimsPlaying = animDrink || animSmoke || animDrinkF || animSmokeF;

    if (anyOfTheAnimsPlaying) {
        if (g_ikChainMan.IsLooking(ped)) {
            g_ikChainMan.AbortLookAt(ped, 250);
        }

        ped->bDontAcceptIKLookAts = true;

        // Start exhale timer (for smkcig anims)
        if ((animSmoke && animSmoke->m_CurrentTime < 0.5f) || (animSmokeF && animSmokeF->m_CurrentTime < 0.5f)) {
            if (!m_ExhaleTimer.IsStarted()) {
                m_ExhaleTimer.Start(2700);
            }
        }
    }

    if (ped->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_PASS_OBJECT)) {
        return m_pSubTask;
    }

    if (CGeneral::GetRandomNumberInRange(0, 500) != 200) {
        if (CGeneral::GetRandomNumberInRange(0, 100) == 50) {
            if (const auto task = ped->GetTaskManager().Find<CTaskSimpleHoldEntity>()) {
                task->PlayAnim(CTaskComplexGangLeader::GetRandomGangAmbientAnim(ped, pedHeldEntity), ANIM_GROUP_GANGS);
            }
        }
        return m_pSubTask;
    }

    if (anyOfTheAnimsPlaying) {
        return m_pSubTask;
    }

    // NOTSA: `CTaskComplexGangLeader::TryToPassObject` is a non-static member in our header, but the original is a plain cdecl function
    const auto passObjTo = plugin::CallAndReturn<CPed*, 0x65EA50, CPed*, CPedGroup*>(ped, m_PedGroup);
    if (!passObjTo || passObjTo->GetEntityThatThisPedIsHolding() || !passObjTo->IsCurrentlyUnarmed()) {
        return m_pSubTask;
    }

    if (pedHeldEntity->m_nModelIndex == ModelIndices::MI_GANG_DRINK) {
        if (CGeneral::GetRandomNumberInRange(0, 500) < 250) {
            passObjTo->Say(CTX_GLOBAL_BOOZE_REQUEST, 0, 1.f);
        } else {
            ped->Say(CTX_GLOBAL_BOOZE_RECEIVE, 1500, 1.f);
        }
    } else if (pedHeldEntity->m_nModelIndex == ModelIndices::MI_GANG_SMOKE) {
        if (CGeneral::GetRandomNumberInRange(0, 500) >= 250) {
            ped->Say(CTX_GLOBAL_SPLIFF_RECEIVE, 1500, 1.f);
        } else {
            passObjTo->Say(CTX_GLOBAL_SPLIFF_REQUEST, 0, 1.f);
        }
    }

    passObjTo->GetEventGroup().Add(CEventPassObject{ped, false});
    return new CTaskComplexPassObject{passObjTo, true};
}
