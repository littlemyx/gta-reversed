#include "StdInc.h"
#include "Ragdoll/IKChainManager.h"
#include "ModelIndices.h"
#include "EventPassObject.h"
#include "EventGroupEvent.h"
#include "EventLeanOnVehicle.h"
#include "TaskComplexBeInGroup.h"
#include "TaskComplexPassObject.h"
#include "TaskSimpleHoldEntity.h"
#include "TaskSimpleRunAnim.h"
#include "TaskComplexPlayHandSignalAnim.h"
#include "TaskComplexGangLeader.h"
#include "TaskComplexWanderGang.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "TaskSimpleStandStill.h"

/// NOTSA - All th gang talk anims
constexpr AnimationId s_gangTalkAnims[]{
    ANIM_ID_PRTIAL_GNGTLKA,
    ANIM_ID_PRTIAL_GNGTLKB,
    ANIM_ID_PRTIAL_GNGTLKC,
    ANIM_ID_PRTIAL_GNGTLKD,

    ANIM_ID_PRTIAL_GNGTLKE,
    ANIM_ID_PRTIAL_GNGTLKF,
    ANIM_ID_PRTIAL_GNGTLKG,
    ANIM_ID_PRTIAL_GNGTLKH,
};

void CTaskComplexGangLeader::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGangLeader, 0x86F8FC, 12);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x65DED0);
    RH_ScopedInstall(Destructor, 0x65DF30);

    RH_ScopedInstall(GetRandomGangAmbientAnim, 0x65E730);
    RH_ScopedInstall(ShouldLoadGangAnims, 0x65E7F0);
    RH_ScopedInstall(DoGangAbuseSpeech, 0x65E860);
    RH_ScopedInstall(DoGangAttackSpeech, 0x65E9A0);
    RH_ScopedInstall(TryToPassObject, 0x65EA50);

    RH_ScopedVMTInstall(Clone, 0x661FA0);
    RH_ScopedVMTInstall(GetTaskType, 0x65DF20);
    RH_ScopedVMTInstall(MakeAbortable, 0x65DFA0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x65DFF0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x65E1F0);
    RH_ScopedVMTInstall(ControlSubTask, 0x662370);
    RH_ScopedVMTInstall(ScanForStuff, 0x65E200);
}

// 0x65DED0
CTaskComplexGangLeader::CTaskComplexGangLeader(CPedGroup* gang) :
    m_gang{gang}
{
}

CTaskComplexGangLeader::CTaskComplexGangLeader(const CTaskComplexGangLeader& o) :
    CTaskComplexGangLeader{ o.m_gang }
{
}

// 0x65DF30
CTaskComplexGangLeader::~CTaskComplexGangLeader() {
    if (m_animsReferenced) {
        UnrefAnimBlock();
    }
}

// 0x65E730
AnimationId CTaskComplexGangLeader::GetRandomGangAmbientAnim(CPed* ped, CEntity* entity) {
    if (!entity) {
        return CGeneral::RandomChoice(s_gangTalkAnims);
    }

    const bool isFemaleBallasOrFam = [&] { // 0x65E767 / 0x65E710
        switch (ped->m_nModelIndex) {
        case MODEL_BALLAS2:
        case MODEL_FAM1: return true;
        default:         return false;
        }
    }();

    if (entity->m_nModelIndex == ModelIndices::MI_GANG_DRINK) {
        if (CGeneral::DoCoinFlip()) {
            return CGeneral::RandomChoice(s_gangTalkAnims | rng::views::take(4));
        }
        return isFemaleBallasOrFam ? ANIM_ID_DRNKBR_PRTL_F : ANIM_ID_DRNKBR_PRTL;
    }

    if (entity->m_nModelIndex == ModelIndices::MI_GANG_SMOKE && !CGeneral::DoCoinFlip()) {
        return isFemaleBallasOrFam ? ANIM_ID_SMKCIG_PRTL_F : ANIM_ID_SMKCIG_PRTL; // 0x65E710
    }

    return CGeneral::RandomChoice(s_gangTalkAnims);
}

// 0x65E7F0
bool CTaskComplexGangLeader::ShouldLoadGangAnims() {
    if (CStreaming::IsVeryBusy()) {
        return false;
    }

    // NOTE: The original checks the speed first (and the streaming state second); both are side-effect free.
    // The threshold is 0x863244 = 0x3D23D70B (~0.04, i.e. 0.2^2), and the compare is `!(x > c)` (NaN => "not too fast")
    const auto player = FindPlayerPed();
    constexpr auto MAX_SPEED_SQ = std::bit_cast<float>(0x3D23D70Bu);
    return !(player->IsInVehicle() && player->m_pVehicle->m_vecMoveSpeed.SquaredMagnitude() > MAX_SPEED_SQ);
}

// 0x65E860
void CTaskComplexGangLeader::DoGangAbuseSpeech(CPed* talker, CPed* sayTo) {
    if (!talker->IsGangster()) {
        return;
    }

    if (!sayTo->IsGangster() && !sayTo->IsPlayer()) {
        return;
    }

    if (const auto phrase = [&] {
        switch (sayTo->m_nPedType) {
        case PED_TYPE_GANG1:   return CTX_GLOBAL_ABUSE_GANG_BALLAS;
        case PED_TYPE_GANG2:   
        case PED_TYPE_PLAYER1: 
        case PED_TYPE_PLAYER2: return CTX_GLOBAL_ABUSE_GANG_FAMILIES;
        case PED_TYPE_GANG4:   return CTX_GLOBAL_ABUSE_RIFA;
        case PED_TYPE_GANG5:   return CTX_GLOBAL_ABUSE_DA_NANG;
        case PED_TYPE_GANG6:   return CTX_GLOBAL_ABUSE_MAFIA;
        case PED_TYPE_GANG7:   return CTX_GLOBAL_ABUSE_TRIAD;
        case PED_TYPE_GANG8:   return CTX_GLOBAL_ABUSE_GANG_VLA;
        default:               return CTX_GLOBAL_NO_SPEECH;
        }
    }()) {
        talker->Say(phrase);
    }
}

// 0x65E9A0 (cdecl, free function in the original)
void CTaskComplexGangLeader::DoGangAttackSpeech(CPed* talker, CPed* target) {
    if (!talker || !target) {
        return;
    }

    if (!talker->IsGangster()) {
        return;
    }

    // Only gangsters, or the player (checked by pointer, not by ped type), are valid targets
    if (!target->IsGangster() && target != FindPlayerPed(0)) {
        return;
    }

    switch (target->m_nPedType) {
    case PED_TYPE_GANG1: talker->Say(CTX_GLOBAL_ATTACK_GANG_BALLAS, 0, 1.f, false, false, false); break; // 0x5EFFE0
    case PED_TYPE_GANG3: talker->Say(CTX_GLOBAL_ATTACK_GANG_LSV,    0, 1.f, false, false, false); break; // 0x5EFFE0
    case PED_TYPE_GANG8: talker->Say(CTX_GLOBAL_ATTACK_GANG_VLA,    0, 1.f, false, false, false); break; // 0x5EFFE0
    default: break;
    }
}

// 0x65EA50 (cdecl, free function in the original)
CPed* CTaskComplexGangLeader::TryToPassObject(CPed* ped, CPedGroup* group) {
    float distSq;
    if (const auto closestPed = group->GetClosestGroupPed(ped, &distSq)) {
        // NOTE: `distSq` is a squared distance, but it's compared against 4.0 (0x858B90) as is.
        // NOTE: The player check is done on `ped` (the one passing the object), not on `closestPed`.
        if (distSq < 4.f && !ped->IsPlayer()) {
            return closestPed;
        }
    }
    return nullptr;
}

// 0x65DFA0
bool CTaskComplexGangLeader::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if (m_pSubTask && !m_pSubTask->MakeAbortable(ped, priority, event)) {
        return false;
    }
    ped->bDontAcceptIKLookAts = false;
    return true;
}

// 0x65DFF0
CTask* CTaskComplexGangLeader::CreateNextSubTask(CPed* ped) {
    // The original scales `rand() & 0xFFFF` by 1/32768 (0x858B14) and by the range, all on the x87 stack, then truncates
    const auto RandScaled = [](double range) {
        return (int32)((double)CGeneral::GetRandomNumber() * (double)(1.f / 32768.f) * range);
    };

    auto& membership = m_gang->GetMembership();
    const auto numMembers = (int32)membership.CountMembers();

    if (m_pSubTask) {
        switch (m_pSubTask->GetTaskType()) {
        case TASK_SIMPLE_STAND_STILL:
        case TASK_COMPLEX_HANDSIGNAL_ANIM: {
            // 0x65E044 - Random slot in [0, numMembers), NOT the n-th existing member (the slot can be empty => fallthrough)
            if (const auto mem = membership.GetMember(RandScaled((double)numMembers))) {
                return new CTaskComplexTurnToFaceEntityOrCoord{ mem };
            }
        }
        }
    }

    // 0x65E0C3
    if (numMembers >= 3 && RandScaled(100.0) <= 95) {
        return new CTaskSimpleStandStill{ 5000 };
    }

    // 0x65E137
    m_wanderTimer.Start(15'000 + RandScaled(15'000.0)); // 15000 - (int)(r * -15000.f)
    return new CTaskComplexWanderGang{ PEDMOVE_WALK, (uint8)RandScaled(8.0), 5000, true, 0.05f };
}

// 0x65E1F0
CTask* CTaskComplexGangLeader::CreateFirstSubTask(CPed* ped) {
    return CreateNextSubTask(ped);
}

// 0x662370
CTask* CTaskComplexGangLeader::ControlSubTask(CPed* ped) {
    ped->bDontAcceptIKLookAts = false; // 0x662391 (set again below if a drink/smoke anim is playing)

    // Make sure anmims are loaded (if they can/need to be)
    if (m_animsReferenced) { // 0x66239B
        if (!ShouldLoadGangAnims()) {
            UnrefAnimBlock();
        }
    } else if (ShouldLoadGangAnims()) {
        const auto blk = CAnimManager::GetAnimationBlockIndex("gangs");
        if (CAnimManager::GetAnimBlocks()[blk].IsLoaded) {
            CAnimManager::AddAnimBlockRef(blk);
            m_animsReferenced = true;
        } else {
            CStreaming::RequestModel(IFPToModelId(blk), STREAMING_KEEP_IN_MEMORY);
        }
    }

    // If we're wandering and the wander time is out of time...
    // NOTE: 0x390 == TASK_COMPLEX_WANDER (all wander variants return it)
    if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_WANDER) { // 0x66241F
        const auto tWander = static_cast<CTaskComplexWander*>(m_pSubTask);
        if (m_wanderTimer.IsOutOfTime()) {
            if (tWander->GetDistSqOfClosestPathNodeToPed(ped) < 2.f) { // 0x66246C (FCOMP, JP: strictly less)
                m_gang->GetIntelligence().SetDefaultTaskAllocatorType(ePedGroupDefaultTaskAllocatorType::RANDOM);
                // Above call causes this task to be flushed (deleted), and changes our vfptr to `CTaskComplex`'s.
                // If we return non-null here, `CTaskManager::ParentsControlChildren` will be called, and calls our
                // `ControlSubTask` causing an assert (as CTaskComplex defines it as `pure` (`= 0`))
                // This is an OG bug, and we can't even wrap it into `FIX_BUGS` because it literally aborts the process, 
                // makes no sense keeping it.
                return nullptr; //return new CTaskSimpleStandStill{ 500 };
            }
        }
    }

    if (m_exhaleTimer.IsOutOfTime()) { // 0x6624C1
        if (ped->GetRpClump()) {
            if (auto matrix = RwFrameGetMatrix(RpClumpGetFrame(ped->GetRpClump()))) {
                if (const auto fx = g_fxMan.CreateFxSystem("exhale", CVector{ 0.f, 0.1f, 0.f }, matrix)) {
                    fx->AttachToBone(ped, eBoneTag::BONE_HEAD);
                    fx->PlayAndKill();
                }
                m_exhaleTimer.Stop(); // 0x662557 (only reached if there was a matrix)
            }
        }
    }

    ScanForStuff(ped);

    if (!ped->IsVisible()) {
        return m_pSubTask;
    }

    // If ped isn't already looking at someone, find a random meber to look at them
    // 0x662574 - `(int)(r * 100) > 95`, i.e. 4% (NOT 5%)
    if (!g_ikChainMan.IsLooking(ped) && (int32)((double)CGeneral::GetRandomNumber() * (double)(1.f / 32768.f) * 100.0) > 95) {
        // NOTE: Order of the random calls is the same as in the original: look time first, then the member slot
        const auto lookTime = CGeneral::GetRandomNumberInRange(3000, 5000);
        if (const auto mem = m_gang->GetMembership().GetMember(CGeneral::GetRandomNumberInRange(0, 8))) { // Random slot, may be empty
            if (mem != ped) {
                g_ikChainMan.LookAt(
                    "TaskGangLeader",
                    ped,
                    mem,
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
    }

    if (!m_animsReferenced || ped->IsRunningOrSprinting()) { // 0x66261A
        return m_pSubTask;
    }

    const auto pedHeldEntity = ped->GetEntityThatThisPedIsHolding();

    if (!pedHeldEntity) {
        // If they're already playing an anim, early out
        if (ped->IsPlayingHandSignal() || ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM)) {
            return m_pSubTask;
        }

        // Otherwise create a new partial anim task
        ped->GetTaskManager().SetTaskSecondary([this, ped]() -> CTask* {
            const auto rnd = CGeneral::GetRandomNumberInRange(0, 500);
            if (50 >= rnd || rnd >= 56) { // 5%
                if (rnd != 100) { // 99.8%
                    return m_pSubTask;
                }

                // Overall chance to reach this point... 0.02% * 5% = 0.1%
                return new CTaskComplexPlayHandSignalAnim{};
            } else { // 95%
                return new CTaskSimpleRunAnim{ ANIM_GROUP_GANGS, CGeneral::RandomChoice(s_gangTalkAnims) };
            }
        }(), TASK_SECONDARY_PARTIAL_ANIM);

        return m_pSubTask;
    }

    // Enum to be in the same order as anims below
    enum {
        DRNKBR_PRTL,
        SMKCIG_PRTL,
        DRNKBR_PRTL_F,
        SMKCIG_PRTL_F,
    };
    const auto GetAnim = [ped](AnimationId id) { return RpAnimBlendClumpGetAssociation(ped->GetRpClump(), id); };
    const CAnimBlendAssociation* anims[]{
        GetAnim(ANIM_ID_DRNKBR_PRTL),
        GetAnim(ANIM_ID_SMKCIG_PRTL),
        GetAnim(ANIM_ID_DRNKBR_PRTL_F),
        GetAnim(ANIM_ID_SMKCIG_PRTL_F),
    };
    const bool anyOfTheAnimsPlaying = rng::any_of(anims, notsa::NotIsNull{});

    // If any of the anims are playing, stop looking, start exhale timer of smkcig anims
    if (anyOfTheAnimsPlaying) { // 0x662696
        if (g_ikChainMan.IsLooking(ped)) {
            g_ikChainMan.AbortLookAt(ped);
        }

        ped->bDontAcceptIKLookAts = true;

        // Start exhale timer (for smkcig anims)
        if (!m_exhaleTimer.IsStarted()) {
            if (rng::any_of(
                std::array{ SMKCIG_PRTL, SMKCIG_PRTL_F },
                [&](auto idx) {
                    const auto anim = anims[idx];
                    return anim && anim->m_CurrentTime < 0.5f;
                })
            ) {
                m_exhaleTimer.Start(2700);
            }
        }
    }

    if (pedHeldEntity->m_nModelIndex == ModelIndices::MI_GANG_DRINK) { // 0x662729
        ped->Say(CTX_GLOBAL_BOOZE_RECEIVE, 0, 0.2f);
    } else if (pedHeldEntity->m_nModelIndex == ModelIndices::MI_GANG_SMOKE) {
        ped->Say(CTX_GLOBAL_SPLIFF_RECEIVE, 0, 0.2f);
    }

    // Now, pass on the entity held in hand (if not already)

    if (ped->GetTaskManager().Find<TASK_COMPLEX_PASS_OBJECT>()) { // 0x662766
        return m_pSubTask;
    }

    if (CGeneral::GetRandomNumberInRange(0, 500) != 200) { // 99.8%
        if (CGeneral::GetRandomNumberInRange(0, 100) == 50) { // 0x6628B3 (1%)
            if (const auto task = ped->GetTaskManager().Find<CTaskSimpleHoldEntity>()) {
                task->PlayAnim(GetRandomGangAmbientAnim(ped, pedHeldEntity), ANIM_GROUP_GANGS);
            }
        }
        return m_pSubTask;
    }

    if (anyOfTheAnimsPlaying) { // 0x66279A
        return m_pSubTask;
    }

    if (const auto passObjTo = TryToPassObject(ped, m_gang)) { // 0x6627CD
        if (!passObjTo->GetEntityThatThisPedIsHolding() && passObjTo->IsCurrentlyUnarmed()) {    
            // Very similar to code above, but not quite the same!
            if (pedHeldEntity->m_nModelIndex == ModelIndices::MI_GANG_DRINK) {
                passObjTo->Say(CTX_GLOBAL_BOOZE_REQUEST);
            } else if (pedHeldEntity->m_nModelIndex == ModelIndices::MI_GANG_SMOKE) {
                passObjTo->Say(CTX_GLOBAL_SPLIFF_REQUEST);
            }
            passObjTo->GetEventGroup().Add(CEventPassObject{ ped });
            return new CTaskComplexPassObject{ passObjTo, true };           
        }
    }

    return m_pSubTask;
}

// 0x65E200
void CTaskComplexGangLeader::ScanForStuff(CPed* ped) {
    // NOTE: An unstarted timer does NOT block the scan (0x65E21B), but `IsOutOfTime()` returns false for those
    if (m_scanTimer.IsStarted() && !m_scanTimer.IsOutOfTime()) {
        return;
    }

    const auto& pedPos = ped->GetPosition();

    const auto rndChance = CGeneral::GetRandomNumberInRange(0, 100);
    if (rndChance < 5) { // 5% chance
        // Find a nearby vehicle to lean onto
        for (auto& veh : ped->GetIntelligence()->GetVehicleScanner().GetEntities<CVehicle>()) {
            // 0x65E2C1
            if (veh.m_nVehicleSubType != VEHICLE_TYPE_AUTOMOBILE) { // 0x65E2B3 (+0x594, the sub type; `IsAutomobile` tests +0x590)
                continue;
            }

            if (veh.GetStatus() != STATUS_ABANDONED) {
                continue;
            }

            if (veh.vehicleFlags.bHasGangLeaningOn) {
                continue;
            }

            if (veh.IsMissionVehicle()) {
                continue;
            }

            // 0x65E351 || 0x65E330 (in that order)
            // (FCOMP + JP: the conditions are "strictly less", NaN => skip)
            if (const auto vehToPed = veh.GetPosition() - pedPos; !(vehToPed.SquaredMagnitude() < 300.f) || !(std::abs(vehToPed.z) < 5.f)) {
                continue;
            }

            m_gang->GetIntelligence().AddEvent(
                CEventGroupEvent{
                    ped,
                    new CEventLeanOnVehicle{&veh, CGeneral::GetRandomNumberInRange(10'000, 25'000)}
                }
            );

            m_scanTimer.Start(60'000);

            // 0x65E3E8
#ifdef FIX_BUGS
            break;
#endif
        }
    } else if (rndChance == 20) { // 1% chance
        // Try recruiting one nearby ped to the gang
        for (auto& scannedPed : ped->GetIntelligence()->GetPedScanner().GetEntities<CPed>()) {
            if (!scannedPed.IsCreatedBy(PED_GAME) || scannedPed.m_nPedType != ped->m_nPedType || scannedPed.bInVehicle) {
                continue;
            }

            const auto scannedPedGrp = scannedPed.GetGroup();

            // Already in the gang
            if (scannedPedGrp == m_gang) {
                continue;
            }

            // In the player's gang
            if (FindPlayerPed()->GetPlayerGroup().GetMembership().IsMember(&scannedPed)) { // 0x65E494
                continue;
            }

            // Can it join a gang at all?
#ifdef FIX_BUGS
            if (const auto wander = scannedPed.GetTaskManager().Find<CTaskComplexWander>()) { // 0x65E4BE
#else
            if (const auto wander = ped->GetTaskManager().Find<CTaskComplexWander>()) {
#endif
                if (wander->GetWanderType() == WANDER_TYPE_GANG) {
                    if (!static_cast<CTaskComplexWanderGang*>(wander)->CanJoinGang()) {
                        continue;
                    }
                }
            }

            // If scanned ped has no group try to add them to this gang
            if (!scannedPedGrp && m_gang->GetMembership().CanAddFollower()) { // 0x65E4EA
                // 0x65E551 - NOTE: The event is added to the SCANNED ped's event group (not the gang's)
                scannedPed.GetEventGroup().Add(
                    CEventScriptCommand{
                        TASK_PRIMARY_PRIMARY,
                        new CTaskComplexBeInGroup{m_gang->GetId()}
                    }
                );
                m_gang->GetMembership().AddFollower(&scannedPed);
                m_gang->Process(); // 0x65E57D
            }

            // Find a member close enough to the scanned ped, and make them partners
            if (const auto [closestMem, distSq] = m_gang->GetMembership().GetMemberClosestTo(&scannedPed); // 0x65E61F
                closestMem && sq(10.f) >= distSq && distSq >= sq(4.f)
            ) {           
                const auto partnerType = CGeneral::GetRandomNumberInRange(0, 7);
                scannedPed.GetEventGroup().Add(CEventCreatePartnerTask{ partnerType, closestMem, true, 0.5f });
                closestMem->GetEventGroup().Add(CEventCreatePartnerTask{ partnerType, &scannedPed, false, 0.5f });
            }

            m_scanTimer.Start(10'000);

            break;
        }
    }
}

void CTaskComplexGangLeader::UnrefAnimBlock() {
    CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
    m_animsReferenced = false;
}
