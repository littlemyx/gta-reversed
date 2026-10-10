/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "PlayerPed.h"
#include "TagManager.h"
#include "PedClothesDesc.h"
#include "PedStats.h"
#include "TaskSimpleUseGun.h"
#include "EntryExitManager.h"
#include "MBlur.h"
#include "Clothes.h"
#include "AnimManager.h"
#include "WeaponInfo.h"
#include "TaskSimpleFight.h"
#include "WeaponModelInfo.h"
#include "PedModelInfo.h"
#include "VisibilityPlugins.h"
#include "Streaming.h"
#include "EventGunAimedAt.h"
#include "EventGroupEvent.h"
#include "PedGroups.h"
#include "TaskManager.h"
#include "TaskComplexSmartFleeEntity.h"
#include "EventScriptCommand.h"
#include "CarCtrl.h"
#include "TaskComplexBeInGroup.h"
#include "TaskSimplePlayerOnFoot.h"
#include "TaskComplexFacial.h"
#include "EventDontJoinPlayerGroup.h"
#include "EventNewGangMember.h"
#include "EventPlayerCommandToGroupAttack.h"
#include "EventPlayerCommandToGroupGather.h"
#include "Cheat.h"
#include "Radar.h"
#include "Stats.h"
#include "Darkel.h"
#include "GameLogic.h"
#include "PedType.h"
#include <numbers>

bool CPlayerPed::bDebugPlayerInvincible;
bool CPlayerPed::bDebugTargeting;
bool CPlayerPed::bDebugTapToTarget;

void CPlayerPed::InjectHooks() {
    RH_ScopedVirtualClass(CPlayerPed, 0x86D168, 26);
    RH_ScopedCategory("Entity/Ped");

    RH_ScopedInstall(ResetSprintEnergy, 0x60A530);
    RH_ScopedInstall(ResetPlayerBreath, 0x60A8A0);
    RH_ScopedInstall(RemovePlayerPed, 0x6094A0);
    RH_ScopedInstall(Busted, 0x609EF0);
    RH_ScopedInstall(GetWantedLevel, 0x41BE60);
    RH_ScopedInstall(SetWantedLevel, 0x609F10);
    RH_ScopedInstall(SetWantedLevelNoDrop, 0x609F30);
    RH_ScopedInstall(CheatWantedLevel, 0x609F50);
    RH_ScopedInstall(DoStuffToGoOnFire, 0x60A020);
    RH_ScopedVMTInstall(Load, 0x5D46E0);
    RH_ScopedVMTInstall(Save, 0x5D57E0);
    RH_ScopedInstall(DeactivatePlayerPed, 0x609520);
    RH_ScopedInstall(ReactivatePlayerPed, 0x609540);
    RH_ScopedInstall(GetPadFromPlayer, 0x609560);
    RH_ScopedInstall(CanPlayerStartMission, 0x609590);
    RH_ScopedInstall(IsHidden, 0x609620);
    RH_ScopedInstall(ReApplyMoveAnims, 0x609650);
    RH_ScopedInstall(DoesPlayerWantNewWeapon, 0x609710);
    RH_ScopedInstall(ProcessPlayerWeapon, 0x6097F0);
    RH_ScopedInstall(ProcessAnimGroups, 0x6098F0);
    RH_ScopedInstall(PickWeaponAllowedFor2Player, 0x609800);
    RH_ScopedInstall(UpdateCameraWeaponModes, 0x609830);
    RH_ScopedInstall(ClearWeaponTarget, 0x609c80);
    RH_ScopedInstall(GetWeaponRadiusOnScreen, 0x609CD0);
    RH_ScopedInstall(PedCanBeTargettedVehicleWise, 0x609D90);
    RH_ScopedInstall(FindTargetPriority, 0x609DE0);
    RH_ScopedInstall(Clear3rdPersonMouseTarget, 0x609ED0);
    RH_ScopedInstall(CanIKReachThisTarget, 0x609F80);
    RH_ScopedInstall(GetPlayerInfoForThisPlayerPed, 0x609FF0);
    RH_ScopedInstall(AnnoyPlayerPed, 0x60A040);
    RH_ScopedInstall(ClearAdrenaline, 0x60A070);
    RH_ScopedInstall(DisbandPlayerGroup, 0x60A0A0);
    RH_ScopedInstall(MakeGroupRespondToPlayerTakingDamage, 0x60A110);
    RH_ScopedInstall(TellGroupToStartFollowingPlayer, 0x60A1D0);
    RH_ScopedInstall(MakePlayerGroupDisappear, 0x60A440);
    RH_ScopedInstall(MakePlayerGroupReappear, 0x60A4B0);
    RH_ScopedInstall(HandleSprintEnergy, 0x60A550);
    RH_ScopedInstall(ControlButtonSprint, 0x60A610);
    RH_ScopedInstall(GetButtonSprintResults, 0x60A820);
    RH_ScopedInstall(SetRealMoveAnim, 0x60A9C0);
    RH_ScopedInstall(HandlePlayerBreath, 0x60A8D0);
    RH_ScopedOverloadedInstall(MakeChangesForNewWeapon, "", 0x60B460, void(CPlayerPed::*)(eWeaponType));
    RH_ScopedGlobalInstall(LOSBlockedBetweenPeds, 0x60B550);
    RH_ScopedInstall(Compute3rdPersonMouseTarget, 0x60B650);
    RH_ScopedInstall(DrawTriangleForMouseRecruitPed, 0x60BA80);
    RH_ScopedInstall(DoesTargetHaveToBeBroken, 0x60C0C0);
    RH_ScopedInstall(KeepAreaAroundPlayerClear, 0x60C1E0);
    RH_ScopedInstall(SetPlayerMoveBlendRatio, 0x60C520);
    RH_ScopedInstall(FindPedToAttack, 0x60C5F0);
    RH_ScopedInstall(PlayerWantsToAttack, 0x60CC50);
    RH_ScopedInstall(FindWeaponLockOnTarget, 0x60DC50);
    RH_ScopedInstall(FindNextWeaponLockOnTarget, 0x60E530);
    RH_ScopedInstall(ForceGroupToAlwaysFollow, 0x60C7C0);
    RH_ScopedInstall(ForceGroupToNeverFollow, 0x60C800);
    RH_ScopedInstall(MakeThisPedJoinOurGroup, 0x60C840);
    RH_ScopedInstall(SetInitialState, 0x60CD20);
    RH_ScopedOverloadedInstall(MakeChangesForNewWeapon, "BySlot", 0x60D000, void(CPlayerPed::*)(uint32));
    RH_ScopedInstall(EvaluateTarget, 0x60D020);
    RH_ScopedInstall(EvaluateNeighbouringTarget, 0x60D1C0);
    RH_ScopedInstall(ProcessGroupBehaviour, 0x60D350);
    RH_ScopedInstall(PlayerHasJustAttackedSomeone, 0x60D5A0);
    RH_ScopedInstall(SetupPlayerPed, 0x60D790);
    RH_ScopedInstall(ProcessWeaponSwitch, 0x60D850);

    RH_ScopedVMTInstall(ProcessControl, 0x60EA90);
    RH_ScopedVMTInstall(SetMoveAnim, 0x609490);
}

// TODO: To class and create 2 function
struct CPlayerPedDataSaveStructure {
    uint32          ChaosLevel{};
    eWantedLevel    WantedLevel{};
    CPedClothesDesc ClothesDesc{};
    uint32          ChosenWeapon{};
    // float          Multiplier{}; // Mobile
};

VALIDATE_SIZE(CPlayerPedDataSaveStructure, 0x84 /* + 0x4 */);

// 0x5D46E0
bool CPlayerPed::Load() {
    CPed::Load();

    CGenericGameStorage::LoadDataFromWorkBuffer<uint32>(); // Discard structure size
    auto sd = CGenericGameStorage::LoadDataFromWorkBuffer<CPlayerPedDataSaveStructure>();

    CWanted* wanted = GetPlayerWanted();
    wanted->m_ChaosLevel = sd.ChaosLevel;
    wanted->m_WantedLevel= sd.WantedLevel;

    *GetPlayerData()->m_pPedClothesDesc = sd.ClothesDesc;
    GetPlayerData()->m_nChosenWeapon   = sd.ChosenWeapon;

    return true;
}

// 0x5D57E0
bool CPlayerPed::Save() {
    CPlayerPedDataSaveStructure saveData{};

    CWanted* wanted = GetPlayerWanted();
    saveData.ChaosLevel = wanted->m_ChaosLevel;
    saveData.WantedLevel = wanted->m_WantedLevel;
    saveData.ChosenWeapon = GetPlayerData()->m_nChosenWeapon;
    saveData.ClothesDesc  = *GetPlayerData()->m_pPedClothesDesc;

    CPed::Save();
    CGenericGameStorage::SaveDataToWorkBuffer(sizeof(CPlayerPedDataSaveStructure));
    CGenericGameStorage::SaveDataToWorkBuffer(saveData);

    return true;
}

// 0x60D5B0
CPlayerPed::CPlayerPed(int32 playerId, bool bGroupCreated) : CPed(PED_TYPE_PLAYER1) {
    m_pPlayerData = &CWorld::Players[playerId].m_PlayerData;
    GetPlayerData()->AllocateData();

    CPed::SetModelIndex(MODEL_PLAYER);

    CPlayerPed::SetInitialState(bGroupCreated);

    CEntity::ClearReference(m_pTargetedObject);

    SetPedState(PEDSTATE_IDLE);

    gPlayIdlesAnimBlockIndex = CAnimManager::GetAnimationBlockIndex("playidles");

    if (!bGroupCreated) {
        GetPlayerData()->m_nPlayerGroup = CPedGroups::AddGroup();

        auto& group = CPedGroups::GetGroup(GetPlayerData()->m_nPlayerGroup);
        group.GetIntelligence().SetDefaultTaskAllocatorType(ePedGroupDefaultTaskAllocatorType::RANDOM);
        group.m_bIsMissionGroup = true;
        group.m_groupMembership.SetLeader(this);
        group.Process();

        GetPlayerData()->m_bGroupStuffDisabled = false;
        GetPlayerData()->m_bGroupAlwaysFollow  = false;
        GetPlayerData()->m_bGroupNeverFollow   = false;
    }

    m_fMaxHealth = CStats::GetFatAndMuscleModifier(STAT_MOD_MAX_HEALTH);
    m_fHealth    = m_fMaxHealth;

    m_nFightingStyle      = STYLE_GRAB_KICK;
    m_nAllowedAttackMoves = 15;

    m_p3rdPersonMouseTarget = nullptr;
    field_7A0 = 0;
    m_pedSpeech.Initialise(this);
    GetIntelligence()->m_fDmRadius = 30.0f;
    GetIntelligence()->m_nDmNumPedsToScan = 2;

    bUsedForReplay = bGroupCreated;
}

// 0x6094A0
void CPlayerPed::RemovePlayerPed(int32 playerId) {
    CPed* player = FindPlayerPed(playerId);
    CPlayerInfo* playerInfo = &FindPlayerInfo(playerId);
    if (player)
    {
        CVehicle* playerVehicle = player->m_pVehicle;
        if (playerVehicle && playerVehicle->m_pDriver == player)
        {
            playerVehicle->SetStatus(STATUS_PHYSICS);
            playerVehicle->m_GasPedal = 0.0f;
            playerVehicle->m_BrakePedal = 0.1f;
        }
        CWorld::Remove(static_cast<CEntity*>(player));
        delete player;
        playerInfo->m_pPed = nullptr;
    }
}

// 0x609520
void CPlayerPed::DeactivatePlayerPed(int32 playerId) {
    assert(playerId >= 0);
    CWorld::Remove(FindPlayerPed(playerId));
}

// 0x609540
void CPlayerPed::ReactivatePlayerPed(int32 playerId) {
    assert(playerId >= 0);
    CWorld::Add(FindPlayerPed(playerId));
}

// 0x609560
CPad* CPlayerPed::GetPadFromPlayer() const {
    switch (m_nPedType) {
    case PED_TYPE_PLAYER1:
        return CPad::GetPad(0);

    case PED_TYPE_PLAYER2:
        return CPad::GetPad(1);
    }
    assert(0); // shouldn't happen
    return nullptr;
}

// 0x609590
bool CPlayerPed::CanPlayerStartMission() {
    if (CGameLogic::GameState != GAMELOGIC_STATE_PLAYING || CGameLogic::IsCoopGameGoingOn())
        return false;

    if (!IsPedInControl() && !IsStateDriving())
        return false;

    auto& taskMgr = GetTaskManager();
    if (taskMgr.GetTaskPrimary(TASK_PRIMARY_PHYSICAL_RESPONSE))
        return false;

    if (taskMgr.GetTaskPrimary(TASK_PRIMARY_EVENT_RESPONSE_NONTEMP))
        return false;

    auto primaryTask = taskMgr.GetTaskPrimary(TASK_PRIMARY_PRIMARY);
    if (primaryTask != nullptr && primaryTask->GetTaskType() != TASK_SIMPLE_CAR_DRIVE)
        return false;

    if (taskMgr.GetTaskSecondary(TASK_SECONDARY_ATTACK))
        return false;

    if (!IsAlive())
        return false;

    return !GetEventGroup().GetEventOfType(EVENT_SCRIPT_COMMAND);
}

// 0x609620
bool CPlayerPed::IsHidden() {
    return bInVehicle && GetLightingTotal() <= 0.05f;
}

// 0x609650
void CPlayerPed::ReApplyMoveAnims() {
    constexpr AnimationId anims[]{
        ANIM_ID_WALK,
        ANIM_ID_RUN,
        ANIM_ID_SPRINT,
        ANIM_ID_IDLE,
        ANIM_ID_WALK_START
    };
    for (const AnimationId& id : anims) {
        if (CAnimBlendAssociation* anim = RpAnimBlendClumpGetAssociation(GetRpClump(), id)) {
            if (anim->GetHashKey() != CAnimManager::GetAnimAssociation(m_nAnimGroup, id)->GetHashKey()) {
                CAnimBlendAssociation* addedAnim = CAnimManager::AddAnimation(GetRpClump(), m_nAnimGroup, id);
                addedAnim->m_BlendDelta = anim->m_BlendDelta;
                addedAnim->m_BlendAmount = anim->m_BlendAmount;

                anim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
                anim->m_BlendDelta = -1000.0f;
            }
        }
    }
}

// 0x609710
bool CPlayerPed::DoesPlayerWantNewWeapon(eWeaponType weaponType, bool arg1) {
    // GetPadFromPlayer(); // Called, but not used

    auto weaponSlot = GetWeaponSlot(weaponType);
    auto weaponInSlotType = GetWeaponInSlot(weaponSlot).m_Type;
    if (weaponInSlotType == weaponType)
        return true;

    if (weaponInSlotType == eWeaponType::WEAPON_UNARMED)
        return true;

    if (arg1)
        return false;

    if (GetIntelligence()->GetTaskJetPack())
        return false;

    /* !See comment!
    if (m_nActiveWeaponSlot == weaponSlot
        && CWeaponInfo::GetWeaponInfo(weaponInSlotType, GetWeaponSkill(weaponInSlotType))->flags.bAimWithArm   \ One of these two is always false, so
        && !CWeaponInfo::GetWeaponInfo(weaponInSlotType, GetWeaponSkill(weaponInSlotType))->flags.bAimWithArm) / the whole expression is always false
        return false;
    */

    if (m_nActiveWeaponSlot == weaponSlot) {
        switch (m_nPedState) {
        case PEDSTATE_ATTACK:
        case PEDSTATE_AIMGUN:
            return false;
        }
    }

    return true;
}

// 0x6097F0
void CPlayerPed::ProcessPlayerWeapon(CPad* pad) {
    /* empty */
}

// 0x609800
void CPlayerPed::PickWeaponAllowedFor2Player() {
    if (!GetWeaponInSlot(GetPlayerData()->m_nChosenWeapon).CanBeUsedFor2Player()) {
        GetPlayerData()->m_nChosenWeapon = eWeaponType::WEAPON_UNARMED;
    }
}

// 0x609830
void CPlayerPed::UpdateCameraWeaponModes(CPad* pad) {
    switch (GetActiveWeapon().m_Type) {
    case eWeaponType::WEAPON_M4:
        TheCamera.SetNewPlayerWeaponMode(eCamMode::MODE_M16_1STPERSON, 0, 0);
        break;

    case eWeaponType::WEAPON_SNIPERRIFLE:
        TheCamera.SetNewPlayerWeaponMode(eCamMode::MODE_SNIPER, 0, 0);
        break;

    case eWeaponType::WEAPON_RLAUNCHER:
        TheCamera.SetNewPlayerWeaponMode(eCamMode::MODE_ROCKETLAUNCHER, 0, 0);
        break;

    case eWeaponType::WEAPON_RLAUNCHER_HS:
        TheCamera.SetNewPlayerWeaponMode(eCamMode::MODE_ROCKETLAUNCHER_HS, 0, 0);
        break;

    case eWeaponType::WEAPON_CAMERA:
        TheCamera.SetNewPlayerWeaponMode(eCamMode::MODE_CAMERA, 0, 0);
        break;

    default:
        TheCamera.ClearPlayerWeaponMode();
        break;
    }
}

// 0x6098F0
void CPlayerPed::ProcessAnimGroups() {
    // 0x609A3B: Default motion group + the offset of `group` relative to ANIM_GROUP_PLAYER
    const auto DefaultMotionGroup = [](int32 group) -> int32 {
        return group + CClothes::GetDefaultPlayerMotionGroup() - ANIM_GROUP_PLAYER;
    };
    // 0x609A1F: Player peds use their model's anim group (if it isn't the default player one)
    const auto PlayerModelGroupOrDefault = [&](int32 group) -> int32 {
        // NOTE: The original compares with 1 (= PED_TYPE_PLAYER2), not with PED_TYPE_PLAYER1 (0x609A1F)
        if (m_nPedType == PED_TYPE_PLAYER2) {
            const auto modelGroup = CModelInfo::GetModelInfo(m_nModelIndex)->AsPedModelInfoPtr()->m_nAnimType;
            if (modelGroup != ANIM_GROUP_PLAYER) {
                return modelGroup;
            }
        }
        return DefaultMotionGroup(group);
    };

    int32 newGroup;
    int32 group = m_nAnimGroup;

    // NOTE: -50deg < heading < 50deg, NaN falls through to the camera checks
    const auto heading = m_pPlayerData->m_fFPSMoveHeading;
    if (   !(-0.87266463f < heading && heading < 0.87266463f)
        && TheCamera.GetActiveCam().Using3rdPersonMouseCam()
        && CanStrafeOrMouseControl()
    ) {
        newGroup = group == ANIM_GROUP_PLAYER
            ? PlayerModelGroupOrDefault(group)
            : DefaultMotionGroup(group);
    } else {
        group = 0;

        bool hasRocketLauncher = false;
        if (m_pWeaponObject) {
            if (auto* const mi = CVisibilityPlugins::GetClumpModelInfo(m_pWeaponObject)) {
                if (mi->GetModelType() == MODEL_INFO_WEAPON) {
                    group = (int32)static_cast<CWeaponModelInfo*>(mi)->GetWeaponInfo();
                    hasRocketLauncher = group == WEAPON_RLAUNCHER || group == WEAPON_RLAUNCHER_HS;
                }
            }
        }

        if (hasRocketLauncher) {
            newGroup = DefaultMotionGroup(ANIM_GROUP_PLAYERROCKET);
        } else if (GetIntelligence()->GetTaskJetPack()) {
            newGroup = ANIM_GROUP_PLAYERJETPACK;
        } else switch (group) {
        case WEAPON_BASEBALLBAT:
        case WEAPON_SHOVEL:
        case WEAPON_POOL_CUE:
            newGroup = DefaultMotionGroup(ANIM_GROUP_PLAYERBBBAT);
            break;
        case WEAPON_CHAINSAW:
        case WEAPON_FLAMETHROWER:
        case WEAPON_MINIGUN:
            newGroup = DefaultMotionGroup(ANIM_GROUP_PLAYERCSAW);
            break;
        case WEAPON_M4:
        case WEAPON_AK47:
        case WEAPON_SPAS12_SHOTGUN:
        case WEAPON_SHOTGUN:
        case WEAPON_SNIPERRIFLE:
        case WEAPON_COUNTRYRIFLE:
            newGroup = DefaultMotionGroup(ANIM_GROUP_PLAYER2ARMED);
            break;
        default:
            if (m_pPlayerData->m_pPedClothesDesc->GetIsWearingBalaclava()) {
                newGroup = ANIM_GROUP_PLAYERSNEAK;
            } else {
                newGroup = PlayerModelGroupOrDefault(ANIM_GROUP_PLAYER);
            }
            break;
        }
    }

    if (m_nAnimGroup != newGroup) {
        m_nAnimGroup = (AssocGroupId)newGroup;
        ReApplyMoveAnims();
    }

    // Anim blocks of the melee weapon / fighting style
    const auto* const wi = CWeaponInfo::GetWeaponInfo(GetActiveWeapon().m_Type, GetWeaponSkill());
    bool releaseWeaponAnims = false, releaseStyleAnims = false;

    // 0x609AA8 etc: Reference the anim block of `animGroup`, store it into `ref`
    const auto ReferenceAnimBlock = [](uint32& ref, AssocGroupId animGroup) {
        auto* block = CAnimManager::GetAnimationBlock(animGroup);
        if (!block) {
            block = CAnimManager::GetAnimationBlock(CAnimManager::GetAnimBlockName(animGroup));
        }
        const auto blockIdx = CAnimManager::GetAnimationBlockIndex(block);
        if (block->IsLoaded) {
            if (ref == 0) {
                CAnimManager::AddAnimBlockRef(blockIdx);
                ref = animGroup;
            }
        } else {
            CStreaming::RequestModel(IFPToModelId(blockIdx), STREAMING_KEEP_IN_MEMORY);
        }
    };

    if (wi->m_nWeaponFire == WEAPON_FIRE_MELEE && !bInVehicle) {
        const auto weaponStyle = (int8)wi->m_nBaseCombo;
        if (weaponStyle == STYLE_STANDARD) {
            if (m_pPlayerData->m_nMeleeWeaponAnimReferenced) {
                releaseWeaponAnims = true;
            }
        } else {
            const auto grp = CTaskSimpleFight::m_aComboData[weaponStyle - STYLE_STANDARD].m_nAnimGroup;
            if (grp != m_pPlayerData->m_nMeleeWeaponAnimReferenced) {
                releaseWeaponAnims = m_pPlayerData->m_nMeleeWeaponAnimReferenced != 0;
                ReferenceAnimBlock(m_pPlayerData->m_nMeleeWeaponAnimReferenced, grp);
            }
        }

        if (m_nFightingStyle == STYLE_STANDARD) {
            if (m_pPlayerData->m_nMeleeWeaponAnimReferencedExtra) {
                releaseStyleAnims = true;
            }
        } else {
            const auto grp = CTaskSimpleFight::m_aComboData[(uint8)m_nFightingStyle - STYLE_STANDARD].m_nAnimGroup;
            if (grp != m_pPlayerData->m_nMeleeWeaponAnimReferencedExtra) {
                releaseStyleAnims = m_pPlayerData->m_nMeleeWeaponAnimReferencedExtra != 0;
                ReferenceAnimBlock(m_pPlayerData->m_nMeleeWeaponAnimReferencedExtra, grp);
            }
        }

    } else {
        // Not holding a melee weapon (or in a vehicle): release both
        releaseWeaponAnims = true;
        releaseStyleAnims  = true;
    }

    if (releaseWeaponAnims) {
        if (const auto ref = m_pPlayerData->m_nMeleeWeaponAnimReferenced) {
            CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex((AssocGroupId)ref));
            m_pPlayerData->m_nMeleeWeaponAnimReferenced = 0;
        }
    }

    if (releaseStyleAnims) {
        if (const auto ref = m_pPlayerData->m_nMeleeWeaponAnimReferencedExtra) {
            CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex((AssocGroupId)ref));
            m_pPlayerData->m_nMeleeWeaponAnimReferencedExtra = 0;
        }
    }
}

// 0x609C80
void CPlayerPed::ClearWeaponTarget() {
    if (IsPlayer()) {
        CEntity::ClearReference(m_pTargetedObject);
        TheCamera.ClearPlayerWeaponMode();
        CWeaponEffects::ClearCrossHair(m_nPedType);
    }
}

// 0x609CD0
float CPlayerPed::GetWeaponRadiusOnScreen() {
    CWeapon& wep = GetActiveWeapon();
    CWeaponInfo& wepInfo = wep.GetWeaponInfo(this);

    if (wep.IsTypeMelee())
        return 0.0f;

    const float accuracyProg = 0.5f / wepInfo.m_fAccuracy;
    switch (wep.m_Type) {
    case eWeaponType::WEAPON_SHOTGUN:
    case eWeaponType::WEAPON_SPAS12_SHOTGUN:
    case eWeaponType::WEAPON_SAWNOFF_SHOTGUN:
        return std::max(0.2f, accuracyProg); // here they multiply *accuracyProg * 1.0f* :thinking

    default: {
        const float rangeProg = std::min(1.0f, 15.0f / wepInfo.m_fWeaponRange);
        const float radius = (GetPlayerData()->m_fAttackButtonCounter * 0.5f + 1.0f) * rangeProg * accuracyProg;
        if (bIsDucking)
            return std::max(0.2f, radius / 2.0f);
        return std::max(0.2f, radius);
    }
    }
}

// 0x609D90
bool CPlayerPed::PedCanBeTargettedVehicleWise(CPed* ped) {
    if (ped->bInVehicle) {
        CVehicle* veh = ped->m_pVehicle;
        return veh && (veh->IsBike() || veh->vehicleFlags.bVehicleCanBeTargetted);
    }
    return true;
}

// 0x609DE0
float CPlayerPed::FindTargetPriority(CEntity* entity) {
    switch (entity->GetType()) {
    case ENTITY_TYPE_VEHICLE:
        return 0.1f;

    case ENTITY_TYPE_PED: {
        auto ped = entity->AsPed();

        if (ped->bThisPedIsATargetPriority)
            return 1.0f;

        if (ped->GetTaskManager().HasAnyOf<TASK_COMPLEX_KILL_PED_ON_FOOT, TASK_COMPLEX_ARREST_PED>()) {
            return 0.8f;
        }

        if (CPedGroups::AreInSameGroup(this, ped))
            return 0.05f;

        if (ped->m_nPedType == PED_TYPE_GANG2)
            return 0.06f;

        if (ped->IsCreatedByMission())
            return 0.25f;

        return 0.1f;
    }

    case ENTITY_TYPE_OBJECT: {
        switch (entity->AsObject()->m_nObjectType) {
        case eObjectType::OBJECT_MISSION:
        case eObjectType::OBJECT_MISSION2:
            return 0.1f;

        default:
            return 0.0f;
        }
    }
    default: {
        return 0.1f;
    }
    }
}

// 0x609ED0
void CPlayerPed::Clear3rdPersonMouseTarget() {
    CEntity::ClearReference(m_p3rdPersonMouseTarget);
}

// 0x609EF0
void CPlayerPed::Busted() {
    CWanted* wanted = GetWanted();
    if (wanted) {
        wanted->m_ChaosLevel = 0;
    }
}

// 0x41BE60
eWantedLevel CPlayerPed::GetWantedLevel() const {
    if (const auto* wanted = GetWanted()) {
        return wanted->GetWantedLevel();
    }

    return eWantedLevel::WANTED_CLEAN;
}

// 0x609F10
void CPlayerPed::SetWantedLevel(eWantedLevel level) {
    CWanted* wanted = GetWanted();
    wanted->SetWantedLevel(level);
}

// 0x609F30
void CPlayerPed::SetWantedLevelNoDrop(eWantedLevel level) {
    CWanted* wanted = GetWanted();
    wanted->SetWantedLevelNoDrop(level);
}

// 0x609F50
void CPlayerPed::CheatWantedLevel(eWantedLevel level) {
    CWanted* wanted = GetWanted();
    wanted->CheatWantedLevel(level);
}

// 0x609F80
bool CPlayerPed::CanIKReachThisTarget(CVector posn, CWeapon* weapon, bool arg2) {
    if (!weapon->GetWeaponInfo(this).flags.bAimWithArm) {
        const CVector thisPos = GetPosition();
        return (posn - thisPos).Magnitude2D() >= thisPos.z - posn.z;
    }
    return true;
}

// 0x609FF0
CPlayerInfo* CPlayerPed::GetPlayerInfoForThisPlayerPed() {
    // TODO: Use range for here
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (FindPlayerPed(i) == this)
            return &FindPlayerInfo(i);
    }
    return nullptr;
}

// 0x60A020
void CPlayerPed::DoStuffToGoOnFire() {
    if (m_nPedState == PEDSTATE_SNIPER_MODE)
        TheCamera.ClearPlayerWeaponMode();
}

// 0x60A040
void CPlayerPed::AnnoyPlayerPed(bool arg0) {
    auto& temper = m_pStats->m_nTemper;

    if (temper < 52) {
        temper++;
    } else if (arg0) {
        if (temper < 55)
            temper++;
        else
            temper = 46;
    }
}

// 0x60A070
void CPlayerPed::ClearAdrenaline() {
    if (GetPlayerData()->m_bAdrenaline && GetPlayerData()->m_nAdrenalineEndTime != 0) {
        GetPlayerData()->m_nAdrenalineEndTime = 0;
        CTimer::ResetTimeScale();
    }
}

// 0x60A0A0
void CPlayerPed::DisbandPlayerGroup() {
    auto& ms = GetPlayerGroup().GetMembership();
    if (const auto numMembers = ms.CountMembersExcludingLeader()) {
        Say(numMembers > 1 ? CTX_GLOBAL_ORDER_DISBAND_MANY : CTX_GLOBAL_ORDER_ATTACK_SINGLE);
    } else {
        ms.RemoveAllFollowers(true);
    }
}

// 0x60A110
void CPlayerPed::MakeGroupRespondToPlayerTakingDamage(CEventDamage& damageEvent) {
    auto& group = GetPlayerGroup();
    if (!damageEvent.m_pSourceEntity)
        return;
    if (group.GetMembership().CountMembersExcludingLeader() < 1)
        return;
    if (!group.m_bMembersEnterLeadersVehicle)
        return;

    CEventGroupEvent groupEvent(this, damageEvent.Clone());
    group.GetIntelligence().AddEvent(&groupEvent);
}

// 0x60A1D0
void CPlayerPed::TellGroupToStartFollowingPlayer(bool arg0, bool arg1, bool arg2) {
    if (GetPlayerData()->m_bGroupAlwaysFollow && !arg0)
        return;
    if (GetPlayerData()->m_bGroupNeverFollow && arg0)
        return;

    CPedGroup& group = GetPlayerGroup();
    CPedGroupIntelligence& groupIntel = group.GetIntelligence();
    CPedGroupMembership& membership = group.GetMembership();
    if (!arg2 && !membership.CountMembersExcludingLeader())
        return;

    group.m_bMembersEnterLeadersVehicle = arg0;
    groupIntel.SetDefaultTaskAllocatorType(ePedGroupDefaultTaskAllocatorType::RANDOM);
    if (arg0) {
        CEventPlayerCommandToGroup playerCmdEvent;
        playerCmdEvent.ComputeResponseTaskType(&group);
        if (playerCmdEvent.WillRespond()) {
            auto gatherCmdEvent = new CEventPlayerCommandToGroup(ePlayerGroupCommand::PLAYER_GROUP_COMMAND_GATHER);
            gatherCmdEvent->m_TaskId = playerCmdEvent.m_TaskId;

            CEventGroupEvent groupEvent(this, gatherCmdEvent);
            groupIntel.AddEvent(&groupEvent);
        }
    }

    if (arg1) {
        const uint32 nMembers = membership.CountMembersExcludingLeader();
        if (!nMembers)
            return;

        if (arg0) {
            const float distToFurthest = group.FindDistanceToFurthestMember();
            if (nMembers > 1) {
                if (distToFurthest >= 3.0f)
                    Say(distToFurthest >= 10.0f ? CTX_GLOBAL_ORDER_FOLLOW_FAR_MANY : CTX_GLOBAL_ORDER_FOLLOW_NEAR_MANY);
                else
                    Say(CTX_GLOBAL_ORDER_FOLLOW_VNEAR_MANY);
            } else {
                if (distToFurthest >= 3.0f)
                    Say(distToFurthest >= 10.0f ? CTX_GLOBAL_ORDER_FOLLOW_FAR_ONE : CTX_GLOBAL_ORDER_FOLLOW_NEAR_ONE);
                else
                    Say(CTX_GLOBAL_ORDER_FOLLOW_VNEAR_ONE);
            }
        } else if (nMembers > 1) {
            Say(CTX_GLOBAL_ORDER_WAIT_MANY);
        } else {
            Say(CTX_GLOBAL_ORDER_WAIT_ONE);
        }
    }
}

// 0x60A440
void CPlayerPed::MakePlayerGroupDisappear() {
    CPedGroupMembership& membership = GetPlayerGroup().GetMembership();
    for (int i = 0; i < TOTAL_PED_GROUP_FOLLOWERS; i++) {
        if (CPed* member = membership.GetMember(i)) {
            if (!member->IsCreatedByMission()) {
                member->SetCollisionProcessed(false);
                member->SetIsVisible(false);
                abTempNeverLeavesGroup[i] = member->bNeverLeavesGroup;
                member->bNeverLeavesGroup = true;
            }
        }
    }
}

// 0x60A4B0
void CPlayerPed::MakePlayerGroupReappear() {
    CPedGroupMembership& membership = GetPlayerGroup().GetMembership();
    for (int i = 0; i < TOTAL_PED_GROUP_FOLLOWERS; i++) {
        if (CPed* member = membership.GetMember(i)) {
            if (!member->IsCreatedByMission()) {
                member->SetIsVisible(true);
                if (!member->bInVehicle)
                    member->SetUsesCollision(true);
                member->bNeverLeavesGroup = abTempNeverLeavesGroup[i];
            }
        }
    }
}

// 0x60A530
void CPlayerPed::ResetSprintEnergy()
{
    GetPlayerData()->m_fTimeCanRun = CStats::GetFatAndMuscleModifier(STAT_MOD_TIME_CAN_RUN);
}

// 0x60A550
bool CPlayerPed::HandleSprintEnergy(bool sprint, float adrenalineConsumedPerTimeStep) {
    float& timeCanRun = GetPlayerData()->m_fTimeCanRun;
    if (sprint) {
        if (FindPlayerInfo().m_bDoesNotGetTired)
            return true;
        if (GetPlayerData()->m_bAdrenaline || adrenalineConsumedPerTimeStep == 0.0f)
            return true;

        if (timeCanRun > -150.0f) { // TODO: Find out what this magic number is
            timeCanRun = std::max(-150.0f, timeCanRun - CTimer::GetTimeStep() * adrenalineConsumedPerTimeStep);
            return true;
        }
    } else {
        if (CStats::GetFatAndMuscleModifier(STAT_MOD_TIME_CAN_RUN) > timeCanRun) {
            timeCanRun += CTimer::GetTimeStep() * adrenalineConsumedPerTimeStep / 2.0f;
        }
    }
    return false;
}

constexpr auto PLAYER_SPRINT_THRESHOLD{ 5.0f }; // 0x8D2458
constexpr struct tPlayerSprintSet { // From 0x8D2460
    float field_0;
    float field_4;
    float field_8;
    float field_C;
    float field_10;
    float field_14;
    float field_18;
    float field_1C;
} PLAYER_SPRINT_SET[] = {
    // 0x0, 0x4,  0x8,  0xC,  0x10,  0x14, 0x18, 0x1C
    { 4.0f, 0.7f, 0.2f, 5.0f, 10.0f, 1.0f, 0.5f, 0.3f }, // GROUND
    { 4.0f, 0.7f, 0.2f, 5.0f, 10.0f, 0.0f, 0.4f, 1.0f }, // BMX
    { 4.0f, 0.7f, 0.2f, 5.0f, 10.0f, 1.0f, 0.3f, 0.3f }, // WATER
    { 4.0f, 0.7f, 0.2f, 5.0f, 10.0f, 0.0f, 0.0f, 1.0f }  // UNDERWATER
};

// 0x60A610
float CPlayerPed::ControlButtonSprint(eSprintType sprintType) {
    const auto pd = GetPlayerData();
    if (!pd) {
        return 0.0f;
    }

    // 0x53FB70. NOTE: The original passes a null pad on for other ped types (and would crash on it)
    CPad* const pad = m_nPedType == PED_TYPE_PLAYER1 ? CPad::GetPad(0)
                    : m_nPedType == PED_TYPE_PLAYER2 ? CPad::GetPad(1)
                    : nullptr;

    const auto& set = PLAYER_SPRINT_SET[sprintType];
    const double ts = CTimer::GetTimeStep();

    const bool canSprint = !pd->m_bPlayerSprintDisabled && (pd->m_fMoveSpeed > 0.0f || pd->m_fTimeCanRun > 0.0f);

    if (pad->SprintJustDown() && canSprint) { // 0x5407F0
        // The original keeps the sum in extended precision (and compares it as such)
        const double sum = (double)set.field_0 + (double)pd->m_fMoveSpeed;
        pd->m_fMoveSpeed = set.field_10 < sum ? set.field_10 : (float)sum;
    } else if (pad->GetSprint() && canSprint) { // 0x5407A0
        const double val = (double)pd->m_fMoveSpeed - ts * (double)set.field_4;
        pd->m_fMoveSpeed = 1.0f > val ? 1.0f : (float)val;
    } else if (pd->m_fMoveSpeed > 0.0f) {
        const double val = (double)pd->m_fMoveSpeed - ts * (double)set.field_8;
        pd->m_fMoveSpeed = 0.0f > val ? 0.0f : (float)val;
    }

    float  progress;
    float  energyRate;
    double progressD;
    if (pd->m_fMoveSpeed > set.field_C) {
        progressD  = (double)pd->m_fMoveSpeed / (double)set.field_C;
        progress   = (float)progressD;
        energyRate = set.field_18;
        if (!(progressD > 0.0)) {
            return 0.0f;
        }
    } else {
        if (!(pd->m_fMoveSpeed > 0.0f) || !canSprint) {
            return 0.0f;
        }
        progress   = 1.0f;
        energyRate = set.field_14;
    }

    if (!HandleSprintEnergy(true, energyRate)) { // 0x60A550
        pd->m_fMoveSpeed = 0.0f;
        return 0.0f;
    }

    const double excess = (double)progress - 1.0;
    return (float)((0.0 > excess ? 0.0 : excess) * (double)set.field_1C + 1.0);
}

// 0x60A820
float CPlayerPed::GetButtonSprintResults(eSprintType sprintType) {
    const auto moveSpeed = GetPlayerData()->m_fMoveSpeed;
    if (!(moveSpeed > PLAYER_SPRINT_THRESHOLD)) { // NOTE: NaN takes this branch (FCOMP + JNZ on C0|C3)
        return moveSpeed > 0.0f ? 1.0f : 0.0f;
    }
    // The original keeps the intermediate results in extended precision, hence `double`
    const auto progress = std::max(0.0, (double)moveSpeed / (double)PLAYER_SPRINT_THRESHOLD - 1.0);
    return (float)(progress * (double)PLAYER_SPRINT_SET[sprintType].field_1C + 1.0);
}

// 0x60A8A0
void CPlayerPed::ResetPlayerBreath() {
    GetPlayerData()->m_fBreath = CStats::GetFatAndMuscleModifier(STAT_MOD_AIR_IN_LUNG);
    GetPlayerData()->m_bRequireHandleBreath = false;
}

// 0x60A8D0
void CPlayerPed::HandlePlayerBreath(bool bDecreaseAir, float fMultiplier) {
    float& breath = GetPlayerData()->m_fBreath;
    float  decreaseAmount = CTimer::GetTimeStep() * fMultiplier;
    if (!bDecreaseAir || CCheat::IsActive(CHEAT_INFINITE_OXYGEN)) {
        if (CStats::GetFatAndMuscleModifier(STAT_MOD_AIR_IN_LUNG) > breath)
            breath += decreaseAmount * 2.0f;
    } else {
        if (breath > 0.0f && bDrownsInWater)
            breath = std::max(0.0f, breath - decreaseAmount);
        else
            CWeapon::GenerateDamageEvent(this, this, eWeaponType::WEAPON_DROWNING, (int32)(decreaseAmount * 3.0f), PED_PIECE_TORSO, 0);
    }
    GetPlayerData()->m_bRequireHandleBreath = false;
}

// 0x60A9C0
void CPlayerPed::SetRealMoveAnim() {
    auto* const clump = GetRpClump();

    // NOTE: The order of these lookups is the same as in the original
    auto* walk      = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_WALK);
    auto* run       = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_RUN);
    auto* sprint    = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_SPRINT);
    auto* walkStart = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_WALK_START);
    auto* idle      = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_IDLE);
    auto* runStop   = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_RUN_STOP);
    auto* runStopR  = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_RUN_STOPR);
    auto* turnL     = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_TURN_L);
    auto* turnR     = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_TURN_R);
    auto* idleTired = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_IDLE_TIRED);

    auto* const pd = m_pPlayerData;

    const auto IsPlaying = [](const CAnimBlendAssociation* a) { return (a->m_Flags & ANIMATION_IS_PLAYING) != 0; };

    if (bResetWalkAnims) {
        if (walk) {
            walk->SetCurrentTime(0.f);
        }
        if (run) {
            run->SetCurrentTime(0.f);
        }
        if (sprint) {
            sprint->SetCurrentTime(0.f);
        }
        bResetWalkAnims = false;
    }

    if ((runStop && IsPlaying(runStop)) || (runStopR && IsPlaying(runStopR))) { // 0x60AAAF
        m_nMoveState = PEDMOVE_RUN;
        if (runStop && !IsPlaying(runStop)) { // Only possible when it's `runStopR` that is playing
            if (runStop->m_CurrentTime < runStop->m_BlendHier->m_fTotalTime) {
                runStop->m_Flags |= ANIMATION_IS_PLAYING;
            }
        }
        goto tail;
    }

    if ((runStop && runStop->m_BlendDelta >= 0.f) || (runStopR && runStopR->m_BlendDelta >= 0.f)) { // 0x60AB03
        auto* const stop = runStop ? runStop : runStopR;
        stop->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
        stop->m_BlendAmount = 1.f;
        stop->m_BlendDelta  = -8.f;

        RestoreHeadingRate();

        if (!idle) {
            idle = CAnimManager::BlendAnimation(clump, m_nAnimGroup, ANIM_ID_IDLE, 8.f);
        }
        idle->m_BlendAmount = 0.f;
        idle->m_BlendDelta  = 8.f;
        goto tail;
    }

    if (pd->m_fMoveBlendRatio == 0.f && !sprint) { // 0x60AB8D (Standing still)
        if (!GetPadFromPlayer()->GetForceCameraBehindPlayer()
            || GetPadFromPlayer()->AimWeaponLeftRight(nullptr) == 0
            || TheCamera.GetActiveCam().m_nMode != MODE_FOLLOWPED
        ) {
            if (!idle) { // 0x60AD2E
                CAnimManager::BlendAnimation(clump, m_nAnimGroup, ANIM_ID_IDLE, 4.f);
            }
        } else {
            // Turn on the spot
            CAnimBlendAssociation* turn;
            if ((double)GetPadFromPlayer()->AimWeaponLeftRight(nullptr) < 0.0) {
                turn = turnL;
                if (!turn
                    || turn->m_BlendDelta < 0.f
                    || (turn->m_BlendAmount < 1.f && turn->m_BlendDelta <= 0.f)
                ) {
                    turn = CAnimManager::BlendAnimation(clump, ANIM_GROUP_DEFAULT, ANIM_ID_TURN_L, 16.f);
                }
            } else {
                turn = turnR;
                if (!turn
                    || turn->m_BlendDelta < 0.f
                    || (turn->m_BlendAmount < 1.f && turn->m_BlendDelta <= 0.f)
                ) {
                    turn = CAnimManager::BlendAnimation(clump, ANIM_GROUP_DEFAULT, ANIM_ID_TURN_R, 16.f);
                }
            }
            turn->m_Speed = (float)(std::abs((double)GetPadFromPlayer()->AimWeaponLeftRight(nullptr)) * 1.0 * 0.0078125);

            if (idle && idle->m_BlendAmount <= 0.01f) {
                delete idle;
            }
        }

        // 0x60AD50
        if (!(pd->m_fTimeCanRun < 0.f)
            || GetIntelligence()->GetTaskFighting()
            || GetIntelligence()->GetTaskUseGun()
            || GetIntelligence()->GetTaskDuck(true)
            || GetIntelligence()->GetTaskThrow()
            || GetIntelligence()->GetTaskJetPack()
            || CWorld::TestSphereAgainstWorld(GetPosition(), 0.5f, nullptr, true, false, false, false, false, false)
        ) { // 0x60AE6D
            if (idleTired && idleTired->m_BlendAmount > 0.f && idleTired->m_BlendDelta >= 0.f) {
                idleTired->m_Flags &= ~ANIMATION_IS_PLAYING;
                idleTired->m_BlendDelta = -2.f;
            }
        } else if (!idleTired) { // 0x60AE0E
            const auto tiredGroup = CClothes::GetDefaultPlayerMotionGroup() == ANIM_GROUP_FAT
                ? ANIM_GROUP_FAT_TIRED
                : ANIM_GROUP_DEFAULT;
            idleTired = CAnimManager::BlendAnimation(clump, tiredGroup, ANIM_ID_IDLE_TIRED, 4.f);
            idleTired->m_Flags |= ANIMATION_IS_PLAYING;
        }
        m_nMoveState = PEDMOVE_STILL;
        goto tail;
    }

    // 0x60AEAF (Moving)
    if (idle) {
        if (!walkStart) {
            walkStart = CAnimManager::AddAnimation(clump, m_nAnimGroup, ANIM_ID_WALK_START);
        } else {
            walkStart->m_BlendAmount = 1.f;
            walkStart->m_BlendDelta  = 0.f;
        }
        if (walk) {
            walk->SetCurrentTime(0.f);
        }
        if (run) {
            run->SetCurrentTime(0.f);
        }
        delete idle;
        if (idleTired) {
            idleTired->m_BlendDelta = -4.f;
        }
        if (sprint) {
            delete sprint;
        }
        sprint = nullptr;
        m_nMoveState = PEDMOVE_WALK;
    }

    if (runStop) { // 0x60AF45
        delete runStop;
        RestoreHeadingRate();
    }
    if (runStopR) {
        delete runStopR;
        RestoreHeadingRate();
    }
    if (turnL) {
        delete turnL;
    }
    if (turnR) {
        delete turnR;
    }

    if (!walk) { // 0x60AF9B
        walk = CAnimManager::AddAnimation(clump, m_nAnimGroup, ANIM_ID_WALK);
        walk->m_BlendAmount = 0.f;
    }
    if (!run) {
        run = CAnimManager::AddAnimation(clump, m_nAnimGroup, ANIM_ID_RUN);
        run->m_BlendAmount = 0.f;
    }

    if (walkStart) { // 0x60AFED
        // NOTE: x87 keeps the sum in extended precision
        if (!IsPlaying(walkStart)
            || (double)walkStart->m_BlendHier->m_fTotalTime <= (double)walkStart->m_TimeStep + (double)walkStart->m_CurrentTime
        ) {
            delete walkStart;
            walk->m_Flags |= ANIMATION_IS_PLAYING;
            run->m_Flags  |= ANIMATION_IS_PLAYING;
            walkStart = nullptr;
        }
    }

    if (m_nMoveState == PEDMOVE_SPRINT && walkStart) { // 0x60B036
        m_nMoveState = PEDMOVE_STILL;
    }

    if (sprint && !(m_nMoveState == PEDMOVE_SPRINT && !(pd->m_fMoveBlendRatio < 0.4f))) { // 0x60B057
        // 0x60B081
        if (sprint->m_BlendAmount == 0.f) {
            sprint->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
            sprint->m_BlendDelta = -1000.f;
        } else if (sprint->m_BlendDelta < 0.f && sprint->m_BlendAmount < 0.8f) { // 0x60B0A1
            if (pd->m_fMoveBlendRatio < 1.f) {
                sprint->m_BlendDelta = -8.f;
                run->m_BlendDelta    = 8.f;
            }
        } else if (pd->m_fMoveBlendRatio < 0.4f) { // 0x60B0EE
            // NOTE: x87 division in extended precision
            const auto stopAnimId = ((double)sprint->m_CurrentTime / (double)sprint->m_BlendHier->m_fTotalTime < 0.5)
                ? ANIM_ID_RUN_STOP
                : ANIM_ID_RUN_STOPR;
            auto* const stop = CAnimManager::AddAnimation(clump, ANIM_GROUP_DEFAULT, stopAnimId);
            stop->m_BlendAmount = 1.f;
            stop->SetDeleteCallback(RestoreHeadingRateCB, this);

            m_fHeadingChangeRate = 0.f;
            sprint->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
            sprint->m_BlendDelta = -1000.f;
            walk->m_Flags &= ~ANIMATION_IS_PLAYING;
            run->m_Flags  &= ~ANIMATION_IS_PLAYING;
            walk->m_BlendAmount = 0.f;
            run->m_BlendAmount  = 0.f;
            walk->m_BlendDelta  = 0.f;
            run->m_BlendDelta   = 0.f;
        } else if (sprint->m_BlendDelta >= 0.f) { // 0x60B1A3
            sprint->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
            sprint->m_BlendDelta = -1.f;
            run->m_BlendDelta    = 1.f;
        }

        // 0x60B1C5
        m_nMoveState = pd->m_fMoveBlendRatio > 1.f ? PEDMOVE_RUN : PEDMOVE_WALK;
        goto tail;
    }

    // 0x60B1EE
    if (walkStart) {
        walk->m_Flags &= ~ANIMATION_IS_PLAYING;
        run->m_Flags  &= ~ANIMATION_IS_PLAYING;
        walk->m_BlendAmount = 0.f;
        run->m_BlendAmount  = 0.f;
        goto tail;
    }

    if (m_nMoveState == PEDMOVE_SPRINT) { // 0x60B210
        if (!sprint) {
            if (run->m_BlendAmount < 1.f) { // 0x60B224
                if (walk->m_BlendAmount == 0.f && run->m_BlendAmount == 0.f) {
                    walk->m_BlendAmount = 1.f;
                }
                if (run->m_BlendDelta <= 0.f) {
                    run = CAnimManager::BlendAnimation(clump, m_nAnimGroup, ANIM_ID_RUN, 4.f);
                }
                pd->m_fMoveBlendRatio = (float)((double)run->m_BlendDelta + 1.0);
                goto tail;
            }
            sprint = CAnimManager::BlendAnimation(clump, m_nAnimGroup, ANIM_ID_SPRINT, 2.f); // 0x60B2A6
        } else if (sprint->m_BlendDelta < 0.f) { // 0x60B2CA
            sprint->m_BlendDelta = 2.f;
            run->m_BlendDelta    = -2.f;
        }
        if (sprint) { // 0x60B2E8
            CStats::UpdateStatsWhenSprinting();
        }
        goto tail;
    }

    // 0x60B2FA
    if (pd->m_fMoveBlendRatio < 1.f) {
        walk->m_BlendAmount = 1.f;
        run->m_BlendAmount  = 0.f;
        walk->m_BlendDelta  = 0.f;
        run->m_BlendDelta   = 0.f;
        m_nMoveState = PEDMOVE_WALK;
    } else if (pd->m_fMoveBlendRatio < 2.f) { // 0x60B330
        // NOTE: x87 - single rounding on the store
        walk->m_BlendAmount = (float)(2.0 - (double)pd->m_fMoveBlendRatio);
        run->m_BlendAmount  = (float)((double)pd->m_fMoveBlendRatio - 1.0);
        walk->m_BlendDelta  = 0.f;
        run->m_BlendDelta   = 0.f;
        m_nMoveState = PEDMOVE_RUN;
    } else { // 0x60B374
        walk->m_BlendAmount = 0.f;
        run->m_BlendAmount  = 1.f;
        walk->m_BlendDelta  = 0.f;
        run->m_BlendDelta   = 0.f;
        m_nMoveState = PEDMOVE_RUN;
        CStats::UpdateStatsWhenRunning();
    }

tail: // 0x60B399
    if (pd->m_bAdrenaline) {
        float speed;
        static_assert(CHEAT_ADRENALINE_MODE == 0x47); // 0x969177
        if (CTimer::m_snTimeInMilliseconds > pd->m_nAdrenalineEndTime && !CCheat::IsActive(CHEAT_ADRENALINE_MODE)) {
            pd->m_bAdrenaline = false;
            CTimer::ms_fTimeScale = 1.f;
            speed = 1.f;
        } else {
            CTimer::ms_fTimeScale = 1.f / 3.f;
            speed = 2.f;
        }
        if (walkStart) {
            walkStart->m_Speed = speed;
        }
        if (walk) {
            walk->m_Speed = speed;
        }
        if (run) {
            run->m_Speed = speed;
        }
        if (!sprint) {
            return;
        }
        sprint->m_Speed = speed;
    }

    if (!sprint) { // 0x60B402
        return;
    }

    if (TheCamera.GetActiveCam().m_nMode == MODE_FIXED) { // 0xF
        sprint->m_Speed = 0.7f;
        return;
    }

    const auto sprintResult = GetButtonSprintResults(SPRINT_GROUND);
    sprint->m_Speed = 1.f > sprintResult ? 1.f : sprintResult;
}

// 0x60B460
void CPlayerPed::MakeChangesForNewWeapon(eWeaponType weaponType) {
    GetActiveWeapon().StopWeaponEffect();
    if (m_nPedState == PEDSTATE_SNIPER_MODE)
        TheCamera.ClearPlayerWeaponMode();

    SetCurrentWeapon(weaponType);

    GetPlayerData()->m_nChosenWeapon = m_nActiveWeaponSlot;
    GetPlayerData()->m_fAttackButtonCounter= 0.0f;

    CWeapon& wep = GetActiveWeapon();
    CWeaponInfo& wepInfo = wep.GetWeaponInfo(this);

    wep.m_AmmoInClip = std::min<uint32>(wep.m_TotalAmmo, (uint32)wepInfo.m_nAmmoClip);

    if (!wepInfo.flags.bCanAim)
        ClearWeaponTarget();

    if (!wepInfo.flags.bOnlyFreeAim)
        GetPlayerData()->m_bFreeAiming = false;


    if (auto anim = RpAnimBlendClumpGetAssociation(GetRpClump(), ANIM_ID_FIRE))
        anim->m_Flags |= ANIMATION_IS_PLAYING & ANIMATION_IS_FINISH_AUTO_REMOVE;

    TheCamera.ClearPlayerWeaponMode();
}

// 0x60B550
bool LOSBlockedBetweenPeds(CEntity* entity1, CEntity* entity2) {
    CVector origin{};
    if (entity1->GetIsTypePed()) {
        origin = entity1->AsPed()->GetBonePosition(eBoneTag::BONE_NECK, false);
        if (entity1->AsPed()->bIsDucking)
            origin.z += 0.35f;
    } else {
        origin = entity1->GetPosition();
    }

    const auto target = entity2->GetIsTypePed()
        ? entity1->AsPed()->GetBonePosition(eBoneTag::BONE_NECK, false)
        : entity1->GetPosition();

    CColPoint colPoint;
    CEntity* hitEntity;
    if (CWorld::ProcessLineOfSight(origin, target, colPoint, hitEntity, true, false, false, true, false, false, false, true))
        return hitEntity != entity2;
    return false;
}

// 0x60B650
void CPlayerPed::Compute3rdPersonMouseTarget(bool meleeWeapon) {
    CPed* target = nullptr;

    if (CCamera::m_bUseMouse3rdPerson) {
        const auto* const wi = CWeaponInfo::GetWeaponInfo(GetActiveWeapon().m_Type, GetWeaponSkill());
        const float       range = wi->m_fTargetRange;
        const CVector     pos = GetPosition();

        CVector source; // Start of the line
        CVector dest;   // End of the line
        if (meleeWeapon) {
            TheCamera.Find3rdPersonCamTargetVector(range, pos, source, dest);
        } else {
            const auto& cam = TheCamera.GetActiveCam();
            source = cam.m_vecSource;
            dest   = cam.m_vecFront;

            // NOTE: The original keeps all of the intermediate values in x87 registers (extended precision)
            const double dx = (double)source.x - (double)pos.x;
            const double dy = (double)source.y - (double)pos.y;
            const double dz = (double)source.z - (double)pos.z;

            // Distance of the camera in front of the ped (along the camera's direction)
            const double d = ((double)dest.y * dy + (double)dest.z * dz) + dx * (double)dest.x;
            if (d < 0.0) {
                // Move the source to the plane of the ped
                const double ax = d * (double)dest.x;
                const double ay = (double)dest.y * d;
                const double az = (double)dest.z * d;
                source.x = (float)((double)source.x - ax);
                source.y = (float)((double)source.y - ay);
                source.z = (float)((double)source.z - az);
            }

            const double fx = (double)dest.x * (double)range;
            const double fy = (double)dest.y * (double)range;
            const float  fz = (float)((double)dest.z * (double)range); // This one is stored to memory before use
            dest.x = (float)((double)source.x + fx);
            dest.y = (float)(fy + (double)source.y);
            dest.z = (float)((double)fz + (double)source.z);
        }

        CColPoint colPoint;
        CEntity*  hitEntity = nullptr;
        CWorld::pIgnoreEntity  = this;
        CWorld::bIncludeBikers = true;
        if (CWorld::ProcessLineOfSight(source, dest, colPoint, hitEntity, false, false, true, false, false, false, false, false)) {
            if (hitEntity != this && hitEntity->AsPed()->IsAlive()) {
                target = hitEntity->AsPed();
            }
        }
        CWorld::ResetLineTestOptions();

        if (target) {
            if (target != m_p3rdPersonMouseTarget) {
                CEntity::ChangeEntityReference(m_p3rdPersonMouseTarget, target);
            }
            field_7A0 = CTimer::GetTimeInMS() + 1000;

            if (CWeaponInfo::GetWeaponInfo(GetActiveWeapon().m_Type, eWeaponSkill::STD)->m_nWeaponFire == WEAPON_FIRE_MELEE) {
                return;
            }
            if (!(CWeaponInfo::GetWeaponInfo(GetActiveWeapon().m_Type, GetWeaponSkill())->m_nFlags & 1)) { // bCanAim
                return;
            }
            if (!target->GetIntelligence()->IsInSeeingRange(GetPosition())) {
                return;
            }

            // Don't annoy peds that are already reacting to someone aiming at them
            {
                auto& tasks = target->GetIntelligence()->GetTaskManager().m_aPrimaryTasks;
                CTask* const task = tasks[0] ? tasks[0] : (tasks[1] ? tasks[1] : tasks[2]);
                if (task && task->GetTaskType() == TASK_COMPLEX_REACT_TO_GUN_AIMED_AT) {
                    return;
                }
            }

            if (GetActiveWeapon().m_Type != WEAPON_PISTOL_SILENCED) {
                static_assert(CTX_GLOBAL_PULL_GUN == 0xB0);
                Say(CTX_GLOBAL_PULL_GUN, 0, 1.f, false, false, false);
            }

            if (auto* const group = CPedGroups::GetPedsGroup(target)) { // 0x60B97D
                if (CPedGroups::AreInSameGroup(target, this)) {
                    return;
                }
                CEventGroupEvent groupEvent{ target, new CEventGunAimedAt(this) };
                group->GetIntelligence().AddEvent(&groupEvent);
            } else {
                CEventGunAimedAt event{ this };
                target->GetIntelligence()->GetEventGroup().Add(&event, false);
            }
            return;
        }
    }

    // BUG?: Clears the pointer without unregistering the reference
    if (m_p3rdPersonMouseTarget && (uint32)field_7A0 < CTimer::GetTimeInMS()) {
        m_p3rdPersonMouseTarget = nullptr;
    }
}

// 0x60BA80
void CPlayerPed::DrawTriangleForMouseRecruitPed() {
    CPed* const target = m_p3rdPersonMouseTarget;
    if (!target) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, RWRSTATE(rwALPHATESTFUNCTIONALWAYS));

    // The x87 code keeps all of the following intermediates in extended precision, hence `double`

    // Health bar color: red (low health) => green (full health)
    double healthRatio = (double)target->m_fHealth / (double)target->m_fMaxHealth;
    if (healthRatio > 1.0) { // NOTE: NaN doesn't take this branch
        healthRatio = 1.0;
    }
    uint8 colR{}, colG{}, colB{};
    if (healthRatio > 0.0) { // NOTE: NaN takes the "all zero" branch
        const double inv = 1.0 - healthRatio;
        colR = (uint8)(int32)(inv * 255.0 + healthRatio * 0.0);
        colG = (uint8)(int32)(healthRatio * 255.0 + inv * 0.0);
        colB = (uint8)(int32)(healthRatio * 0.0 + inv * 0.0);
    }

    // Size of the triangle depends on the distance to the ped
    const auto& selfPos   = GetPosition();
    const auto& targetPos = target->GetPosition();
    const double dx = (double)targetPos.x - (double)selfPos.x;
    const double dy = (double)targetPos.y - (double)selfPos.y;
    const double dz = (double)targetPos.z - (double)selfPos.z;
    double scale = std::sqrt(dz * dz + dy * dy + dx * dx) - 10.0; // 0x85862C
    if (!(scale > 0.0)) {
        scale = 0.0;
    }
    scale *= (double)0.02f; // 0x858B38
    if (1.0 < scale) {
        scale = 1.0;
    }
    scale = scale * (double)0.825f + (double)0.175f; // 0x86D1DC, 0x85A5C8

    // Camera's right vector (as there might be no matrix, it's computed from the heading then)
    double right_x, right_y;
    float  right_z;
    if (TheCamera.m_matrix) {
        const auto& right = TheCamera.m_matrix->GetRight();
        right_x = right.x;
        right_y = right.y;
        right_z = right.z;
    } else {
        const auto heading = TheCamera.m_placement.m_fHeading;
        right_x = std::cos((double)heading);
        right_y = std::sin((double)heading);
        right_z = 0.f;
    }
    const float rx = (float)(right_x * scale);
    const float ry = (float)(right_y * scale);
    const float rz = (float)((double)right_z * scale);
    const float k  = (float)(0.0 * scale); // Always 0

    const float posZ = (float)((double)targetPos.z + 1.0);

    CVector verts[3];
    if (target->m_nPedType == PED_TYPE_GANG1) { // Triangle pointing downwards
        verts[0] = CVector{ targetPos.x, targetPos.y, posZ };
        verts[1] = CVector{
            (targetPos.x - rx) + k,
            k + (targetPos.y - ry),
            (float)((double)(posZ - rz) + scale)
        };
        verts[2] = CVector{
            (targetPos.x + rx) + k,
            k + (targetPos.y + ry),
            (float)(scale + (double)(posZ + rz))
        };
    } else { // Triangle pointing upwards
        verts[0] = CVector{
            targetPos.x + k,
            k + targetPos.y,
            (float)((double)posZ + scale)
        };
        verts[1] = CVector{ targetPos.x + rx, ry + targetPos.y, posZ + rz };
        verts[2] = CVector{ targetPos.x - rx, targetPos.y - ry, posZ - rz };
    }

    // Move all of the vertices 1 unit towards the camera
    const CVector camPos = TheCamera.GetPosition();
    for (auto& vert : verts) {
        CVector toCam = camPos - vert;

        // NOTE: Inlined 0x59C910 (`CVector::Normalise`): The sum of squares, `1 / sqrt` and the multiplication are all done
        //       in extended precision (`double` here) and only the results are rounded to float. The shared `CVector::Normalise` is float-only.
        const double sumSq = ((double)toCam.x * toCam.x + (double)toCam.y * toCam.y) + (double)toCam.z * toCam.z;
        if (sumSq > 0.0) { // NOTE: NaN takes the other branch
            const double recip = 1.0 / std::sqrt(sumSq);
            toCam.x = (float)(toCam.x * recip);
            toCam.y = (float)(toCam.y * recip);
            toCam.z = (float)(toCam.z * recip);
        } else {
            toCam.x = 1.f; // Only `x` is touched, as in the original
        }

        vert += toCam;
    }

    // Opaque in the tip, fully transparent at the base
    const uint32 colRGB = ((uint32)colR << 16) | ((uint32)colG << 8) | (uint32)colB;
    for (auto i = 0u; i < std::size(verts); i++) {
        auto& vtx = TempBufferVertices.m_3d[i];
        RwCompatVertexPos(vtx) = { verts[i].x, verts[i].y, verts[i].z };
        vtx.color     = (i == 0) ? (0xFF000000 | colRGB) : colRGB;
        aTempBufferIndices[i] = (RxVertexIndex)i;
    }

    if (RwIm3DTransform(TempBufferVertices.m_3d, std::size(verts), nullptr, rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA)) {
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, aTempBufferIndices, std::size(verts));
        RwIm3DEnd();
    }

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, RWRSTATE(rwALPHATESTFUNCTIONGREATER));
}

// 0x60C0C0
bool CPlayerPed::DoesTargetHaveToBeBroken(CEntity* target, CWeapon* weapon) {
    if (!target->GetIsVisible()) {
        return true;
    }

    if (weapon->GetWeaponRange(this, target) < (target->GetPosition() - GetPosition()).Magnitude()) {
        return true;
    }

    if (weapon->m_Type == eWeaponType::WEAPON_SPRAYCAN) {
        if (target->GetIsTypeBuilding()) {
            if (CTagManager::IsTag(*target)) {
                if (CTagManager::GetAlpha(*target) == 255) { // they probably used -1
                    return true;
                }
            }
        }
    }

    return !CanIKReachThisTarget(target->GetPosition(), weapon, false);
}

// 0x60C1E0
void CPlayerPed::KeepAreaAroundPlayerClear() {
    // Get rid of the (game created) peds nearby
    for (CEntity* const entity : GetIntelligence()->GetPedScanner().m_apEntities) {
        auto* const ped = static_cast<CPed*>(entity);
        if (!ped || !ped->IsCreatedBy(PED_GAME) || ped->bInVehicle || !ped->IsAlive()) {
            continue;
        }
        if (CPedGroups::ms_groups[0].GetMembership().IsMember(ped)) { // NOTE: Not `GetPlayerGroup()`, the original hardcodes the group 0 (0xC09928)
            continue;
        }
        if (!ped->GetIsOnScreen() || ped->bKeepTasksAfterCleanUp) {
            ped->FlagToDestroyWhenNextProcessed();
            continue;
        }

        // Already fleeing from us?
        if (const auto* const task = ped->GetIntelligence()->FindTaskByType(TASK_COMPLEX_SMART_FLEE_ENTITY)) {
            if (static_cast<const CTaskComplexSmartFleeEntity*>(task)->m_fleeFrom == this) {
                continue;
            }
        }
        // Is about to flee from something (Only the first script command event is checked)
        if (const auto* const event = ped->GetEventGroup().GetEventOfType(EVENT_SCRIPT_COMMAND)) {
            if (const auto* const task = static_cast<const CEventScriptCommand*>(event)->m_task) {
                if (task->GetTaskType() == TASK_COMPLEX_SMART_FLEE_ENTITY) {
                    continue;
                }
            }
        }

        auto* const task = new CTaskComplexSmartFleeEntity{ this, false, 1000.f, 100'000, 1'000, 1.f }; // 0x86F678, 0xC18CF0
        task->m_moveState = PEDMOVE_WALK;
        CEventScriptCommand event{ 3, task, false };
        ped->GetEventGroup().Add(&event, false);
    }

    // Get rid of the vehicles nearby
    const auto& selfPos = GetPosition();
    const CVector center = (bInVehicle && m_pVehicle) // `selfPos` is used for the search (below) in any case
        ? m_pVehicle->GetPosition()
        : selfPos;

    CEntity* nearby[6];
    int16    numNearby;
    CWorld::FindObjectsInRange(selfPos, 15.f, true, &numNearby, (int16)std::size(nearby), nearby, false, true, false, false, false);
    for (int16 i = 0; i < numNearby; i++) {
        auto* const veh = nearby[i]->AsVehicle();
        if (veh->IsMissionVehicle()) {
            continue;
        }
        if (veh->GetStatus() == STATUS_PLAYER || veh->GetStatus() == STATUS_FORCED_STOP) {
            continue;
        }

        const auto& vehPos = veh->GetPosition();
        const double dx = (double)vehPos.x - (double)center.x;
        const double dy = (double)vehPos.y - (double)center.y;
        const double dz = (double)vehPos.z - (double)center.z;
        if (dz * dz + dy * dy + dx * dx > (double)25.f) { // 0x858FE8 - NOTE: NaN takes the other branch
            veh->m_autoPilot.SetTempAction(TEMPACT_WAIT, 5000);
        } else {
            // BUG: Dereferences the matrix without checking if there's one
            const auto& fwd = veh->m_matrix->GetForward();
            const double dot = ((double)center.y - (double)vehPos.y) * (double)fwd.y
                             + ((double)center.x - (double)vehPos.x) * (double)fwd.x;
            veh->m_autoPilot.SetTempAction(dot > 0.0 ? TEMPACT_REVERSE : TEMPACT_GOFORWARD, 2000);
        }
        CCarCtrl::PossiblyRemoveVehicle(veh);
    }
}

// 0x60C520
void CPlayerPed::SetPlayerMoveBlendRatio(CVector* point) {
    float& moveBlendRatio = GetPlayerData()->m_fMoveBlendRatio;
    if (point) {
        moveBlendRatio = std::min(2.0f, (*point - GetPosition()).Magnitude2D() * 2.0f);
    } else {
        switch (m_nMoveState)
        {
        case PEDMOVE_WALK:
            moveBlendRatio = 1.0f;
            break;

        case PEDMOVE_RUN:
            moveBlendRatio = 1.8f;
            break;

        case PEDMOVE_SPRINT:
            moveBlendRatio = 2.5f;
            break;

        default:
            moveBlendRatio = 0.0f;
            break;
        }
    }
    SetRealMoveAnim();
}

// 0x60C5F0
CPed* CPlayerPed::FindPedToAttack() {
    CVector origin = FindPlayerCoors();
    origin.z = 0.0f;

    CVector end = origin + TheCamera.GetForward() * 100.0f;
    end.z = 0.0f;

    CPed* closestPed{};
    float closestDistance = std::numeric_limits<float>::max();

    CPedGroupMembership& membership = GetPlayerGroup().GetMembership();
    for (int i = 0; GetPedPool()->GetSize(); i++) {
        CPed* ped = GetPedPool()->GetAt(i);
        if (!ped)
            continue;
        if (ped->IsPlayer())
            continue;
        if (!ped->IsAlive())
            continue;
        if (membership.IsMember(ped))
            continue;
        if (ped->m_nPedType == PED_TYPE_GANG2)
            continue;

        CVector point = ped->GetPosition();
        point.z = 0.0f;

        float dist = CCollision::DistToLine(origin, end, point);
        float pointDist = (point - origin).Magnitude2D();
        if (pointDist > 20.0f)
            dist += (pointDist - 20.0f) * ExeRecip(5.0f);

        if (IsPedTypeGang(ped->m_nPedType))
            dist = std::max(0.0f, dist / 2.0f - 2.0f);

        if (dist < closestDistance) {
            closestDistance = dist;
            closestPed = ped;
        }
    }
    return closestPed;
}

// 0x60C7C0
void CPlayerPed::ForceGroupToAlwaysFollow(bool enable) {
    GetPlayerData()->m_bGroupAlwaysFollow = enable;
    if (enable)
        TellGroupToStartFollowingPlayer(true, false, true);
}

// 0x60C800
void CPlayerPed::ForceGroupToNeverFollow(bool enable) {
    GetPlayerData()->m_bGroupNeverFollow = enable;
    if (enable)
        TellGroupToStartFollowingPlayer(false, false, true);
}

// 0x60C840
void CPlayerPed::MakeThisPedJoinOurGroup(CPed* ped) {
    if (ped->bDruggedUp) {
        Say(CTX_GLOBAL_DRUGGED_IGNORE);
        return;
    }

    if (ped->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_KILL_PED_ON_FOOT)) {
        return;
    }

    // 0x609380 (inlined here)
    static_assert(CHEAT_WANNA_BE_IN_MY_GANG == 0x4C && CHEAT_NO_ONE_CAN_STOP_US == 0x4D && CHEAT_ROCKET_MAYHEM == 0x4E); // 0x96917C, 0x96917D, 0x96917E
    const auto IsRecruitCheatActive = [] {
        return CCheat::IsAnyActive({ CHEAT_WANNA_BE_IN_MY_GANG, CHEAT_NO_ONE_CAN_STOP_US, CHEAT_ROCKET_MAYHEM });
    };
    if (ped->m_nPedType != PED_TYPE_GANG2 && !IsRecruitCheatActive()) {
        return;
    }

    auto& group      = GetPlayerGroup();
    auto& membership = group.GetMembership();
    if (membership.IsMember(ped)) {
        return;
    }

    CAEPedSpeechAudioEntity::SetCJMood(MOOD_UNK, 10'000, 1, -1, -1);
    Say(CTX_GLOBAL_JOIN_ME_ASK, 0, 1.f, true);

    // Max. number of group members
    int32 maxMembers = std::min<int32>(CStats::FindMaxNumberOfGroupMembers(), GetPlayerData()->m_nScriptLimitToGangSize);
    if (CStats::GetStatValue(STAT_CITY_UNLOCKED) == 1.f || CStats::GetStatValue(STAT_CITY_UNLOCKED) == 2.f) { // 0x858624, 0x858CA0
        maxMembers = 0;
    }

    // Can the ped be recruited?
    const auto numFollowers = membership.CountMembersExcludingLeader();
    if ((IsRecruitCheatActive() && numFollowers < 7) || numFollowers < maxMembers) {
        if (auto* const pedGroup = CPedGroups::GetPedsGroup(ped)) { // Remove from its current group
            pedGroup->GetMembership().RemoveMember(ped);
        }

        // Tell the ped to be in our group
        auto* const task = new CTaskComplexBeInGroup{ (int32)FindPlayerPed(-1)->GetPlayerData()->m_nPlayerGroup, false };
        CEventScriptCommand scriptCmdEvent{ 3, task, false };
        ped->GetEventGroup().Add(&scriptCmdEvent, false);

        membership.AddFollower(ped);
        group.Process();
        ped->GiveWeaponWhenJoiningGang();

        CEventGroupEvent groupEvent{ this, new CEventNewGangMember{ ped } };
        group.GetIntelligence().AddEvent(&groupEvent);

        ped->bDrownsInWater = false;

        CStats::IncrementStat(STAT_GANG_MEMBERS_RECRUITED, 1.f);
        CStats::DisplayScriptStatUpdateMessage(STAT_UPDATE_INCREASE, STAT_GANG_STRENGTH, 1.f);

        // NOTE: Original passes the name "CODEPLR" (0x86D1E0) as the 5th argument, but `SetEntityBlip` (cdecl, 4 args) never uses it
        // RGBA read from the gang color tables (0x8D1344, 0x8D1350, 0x8D135C) for gang 1 (Grove) - same as `CGangWars::GetGangColor(GANG_GROVE)` (which is private)
        const auto blipColor = (eBlipColour)0x46C800FF; // (70, 200, 0, 255)
        const auto blip      = CRadar::SetEntityBlip(BLIP_CHAR, GetPedPool()->GetRef(ped), (uint32)blipColor, BLIP_DISPLAY_BLIPONLY);
        CRadar::ChangeBlipScale(blip, 2);
        CRadar::ChangeBlipColour(blip, blipColor);
        CRadar::SetBlipFriendly(blip, true);

        ped->bClearRadarBlipOnDeath = true;

        membership.SetSeparationRange(120.f); // 0x86C6C0 (`ms_fPlayerGroupMaxSeparation`)

        ped->Say(CTX_GLOBAL_JOIN_GANG_YES, 2500, 1.f, true);
    } else {
        ped->Say(CTX_GLOBAL_JOIN_GANG_NO, 2500, 1.f, true);

        CEventDontJoinPlayerGroup event{ this };
        ped->GetEventGroup().Add(&event, false);
    }
}

// 0x60CC50
bool CPlayerPed::PlayerWantsToAttack() {
    auto& group = CPedGroups::ms_groups[GetPlayerData()->m_nPlayerGroup]; // 0xC09920 + group * 0x2D4
    if (group.GetMembership().CountMembersExcludingLeader() < 1) {
        return false; // NOTSA: The original returns void (AL is garbage), nobody uses the result
    }

    if (!group.m_bMembersEnterLeadersVehicle) { // 0xC09924 + group * 0x2D4
        return false;
    }

    group.GetIntelligence().ReportAllBarScriptTasksFinished(); // 0xC09950 + group * 0x2D4

    CEntity* target = m_pTargetedObject;
    if (CCamera::m_bUseMouse3rdPerson && !target) {
        target = m_p3rdPersonMouseTarget;
    }

    CPed* pedToAttack;
    if (target && target->GetType() == ENTITY_TYPE_PED) {
        pedToAttack = target->AsPed();
    } else {
        if (target) {
            if (CTagManager::IsTag(*target)) { // 0x49CCE0
                return false;
            }
            if (target->GetType() == ENTITY_TYPE_OBJECT && target->AsObject()->CanBeTargetted()) { // 0x59F320
                return false;
            }
        }
        pedToAttack = FindPedToAttack(); // 0x60C5F0
    }

    if (pedToAttack) {
        CPedGroups::ms_groups[GetPlayerData()->m_nPlayerGroup].PlayerGaveCommand_Attack(this, pedToAttack); // 0x5F7CC0
    }

    return false; // NOTSA: see above
}

// 0x60CD20
void CPlayerPed::SetInitialState(bool bGroupCreated) {
    CMBlur::ClearDrunkBlur();
    CTimer::ms_fTimeScale = 1.f;

    m_bUsesCollision = true;
    physicalFlags.bApplyGravity = true;

    ClearAimFlag();
    ClearLookFlag();

    bRenderPedInCar = true;

    if (m_pFire) {
        m_pFire->Extinguish();
    }

    SetPedState(PEDSTATE_IDLE);
    SetMoveState(PEDMOVE_STILL);

    bIsDucking     = false;
    bDontRender    = false;
    bIsBeingArrested = false;
    bCanExitCar    = true;

    GetIntelligence()->FlushIntelligence();
    RpAnimBlendClumpRemoveAllAssociations(GetRpClump());

    GetTaskManager().SetTask(new CTaskSimplePlayerOnFoot{}, TASK_PRIMARY_DEFAULT, false);

    m_nAnimGroup         = ANIM_GROUP_PLAYER;
    bIsPedDieAnimPlaying = false;

    if (GetPlayerData()) {
        GetPlayerData()->m_bAdrenaline = false;
    }

    SetRealMoveAnim();

    m_pStats->m_nTemper = 50;

    if (m_pAttachedTo && !m_bUsesCollision) {
        m_bUsesCollision = true;
    }
    m_pAttachedTo = nullptr;

    m_nTurretAmmo = 0;

    GetTaskManager().SetTaskSecondary(new CTaskComplexFacial{}, TASK_SECONDARY_FACIAL_COMPLEX);

    if (!bGroupCreated && !GetPlayerData()->m_bGroupNeverFollow) {
        // NOTE: At this point (when called from the constructor) the player's group isn't created yet, so this uses
        //       whatever is in `m_nPlayerGroup` - that's why `GetPlayerGroup()` (which asserts the group is active) isn't used.
        auto& group = CPedGroups::ms_groups[GetPlayerData()->m_nPlayerGroup];
        group.m_bMembersEnterLeadersVehicle = true;
        group.GetIntelligence().SetDefaultTaskAllocatorType(ePedGroupDefaultTaskAllocatorType::RANDOM);

        CEventPlayerCommandToGroupAttack attackEvent{ nullptr }; // The original constructs a `CEventPlayerCommandToGroup(0, 0)` and swaps the vtable
        attackEvent.ComputeResponseTaskType(&group);
        if (attackEvent.WillRespond()) {
            auto* const gatherEvent = new CEventPlayerCommandToGroupGather{ nullptr };
            gatherEvent->m_TaskId = attackEvent.m_TaskId;

            CEventGroupEvent groupEvent{ this, gatherEvent };
            group.GetIntelligence().AddEvent(&groupEvent);
        }
    }

    if (GetPlayerData()) {
        GetPlayerData()->SetInitialState();
    }
}

// 0x60D000
void CPlayerPed::MakeChangesForNewWeapon(uint32 weaponSlot) {
    if (weaponSlot != -1)
        MakeChangesForNewWeapon(GetWeaponInSlot(weaponSlot).m_Type);
}

static auto& PLAYER_MAX_TARGET_VIEW_ANGLE = StaticRef<float>(0x8D243C); // 140.0f

// 0x60D020
void CPlayerPed::EvaluateTarget(CEntity* target, CEntity *& outTarget, float & outTargetPriority, float maxDistance, float compensationRotRad, bool arg5) {
    const CVector dir = target->GetPosition() - GetPosition();
    const float dist = dir.Magnitude();

    if (dist > maxDistance)
        return;

    if (DoesTargetHaveToBeBroken(target, &GetActiveWeapon()))
        return;

    const float targetAngleDeg = std::fabs(RadiansToDegrees(CGeneral::LimitRadianAngle(CGeneral::GetATanOf(dir) - compensationRotRad)));

    float viewAngleMultiplier = 1.0f - targetAngleDeg / PLAYER_MAX_TARGET_VIEW_ANGLE;
    if (dist > 1.0f)
        viewAngleMultiplier /= sqrt(sqrt(dist)); // Take quad root of dist

    const float targetPriority = FindTargetPriority(target) * viewAngleMultiplier;
    if (targetPriority > outTargetPriority && !LOSBlockedBetweenPeds(this, target)) {
        outTarget = target;
        outTargetPriority = targetPriority;
    }
}

// 0x60D1C0
void CPlayerPed::EvaluateNeighbouringTarget(CEntity* target, CEntity** outTarget, float* outTargetPriority, float maxDistance, float arg4, bool arg5) {
    // The original keeps the intermediate results in extended precision, hence `double`
    const auto& selfPos   = GetPosition();
    const auto& targetPos = target->GetPosition();
    const double dx = (double)targetPos.x - (double)selfPos.x;
    const double dy = (double)targetPos.y - (double)selfPos.y;
    const double dz = (double)targetPos.z - (double)selfPos.z;
    if (!(std::sqrt(dz * dz + dy * dy + dx * dx) <= (double)maxDistance)) { // NOTE: NaN returns too
        return;
    }

    if (DoesTargetHaveToBeBroken(target, &GetActiveWeapon())) {
        return;
    }

    // Angle between the camera and the target, relative to `arg4`, normalized to [-pi, pi]
    const auto& camPos = TheCamera.GetPosition();
    const float diffY = targetPos.y - camPos.y;
    const float diffX = targetPos.x - camPos.x;
    double angle = CGeneral::GetATanOfXYExt(diffX, diffY) - (double)arg4;
    constexpr auto PI = std::numbers::pi_v<float>; // 0x858CB8
    while (angle > (double)PI) {
        angle -= (double)(2.f * PI); // 0x858CBC
    }
    while (angle < (double)-PI) { // 0x858CC0
        angle += (double)(2.f * PI);
    }

    constexpr auto MAX_ANGLE = 0.8726646304130554f; // 0x86D1D0 (50 deg)
    if (!(std::abs(angle) < (double)MAX_ANGLE)) {
        return;
    }

    // `arg5` selects which side of the camera's view direction the target has to be on
    // Priority is the (negated) angle distance, or a big negative number if the target is on the wrong side.
    constexpr auto WRONG_SIDE_PRIORITY = -100000.f; // 0x86D1E8
    double priority;
    if (!arg5) {
        priority = !(angle >= 0.0) ? angle : (double)WRONG_SIDE_PRIORITY;
    } else {
        priority = !(angle <= 0.0) ? -angle : (double)WRONG_SIDE_PRIORITY;
    }

    if (priority > (double)*outTargetPriority) {
        *outTarget         = target;
        *outTargetPriority = (float)priority;
    }
}

// 0x60D350
void CPlayerPed::ProcessGroupBehaviour(CPad* pad) {
    // The ped the player is looking at (or the one the mouse is pointing at)
    CEntity* target = m_pTargetedObject;
    if (CCamera::m_bUseMouse3rdPerson && !target) {
        target = m_p3rdPersonMouseTarget;
    }

    // `CTimer::ms_fTimeStepNonClipped * 0.02f * 1000.0f` kept in extended precision, then truncated by _ftol (0x821B40)
    const auto TimeStepInMS = [] {
        return (uint16)(int32)((double)CTimer::GetTimeStepNonClipped() * (double)0.02f * 1000.0);
    };

    // 0x609380 (inlined here)
    const auto IsRecruitCheatActive = [] {
        return CCheat::IsAnyActive({ CHEAT_WANNA_BE_IN_MY_GANG, CHEAT_NO_ONE_CAN_STOP_US, CHEAT_ROCKET_MAYHEM });
    };

    constexpr uint16 HOLD_TIME_MS = 1200; // 0x4B0

    // Up: Tell the group to follow the player/recruit a ped (on a short press), disband the group (on a long press)
    if (!FindPlayerVehicle(-1, false)) {
        auto* const pd = GetPlayerData();
        if (pad->GetGroupControlForward()) {
            pd->m_nPadUpPressedInMilliseconds += TimeStepInMS();
            if (GetPlayerData()->m_nPadUpPressedInMilliseconds == HOLD_TIME_MS && !GetPlayerData()->m_bGroupStuffDisabled) {
                DisbandPlayerGroup();
            }
        } else {
            if (pd->m_nPadUpPressedInMilliseconds != 0 && pd->m_nPadUpPressedInMilliseconds < HOLD_TIME_MS) {
                if (   !target
                    || !target->GetIsTypePed()
                    || (target->AsPed()->m_nPedType != PED_TYPE_GANG2 && !IsRecruitCheatActive())
                ) {
                    TellGroupToStartFollowingPlayer(true, true, false);
                } else if (!pd->m_bGroupStuffDisabled) {
                    MakeThisPedJoinOurGroup(target->AsPed());
                }
            }
            GetPlayerData()->m_nPadUpPressedInMilliseconds = 0;
        }
    }

    if (GetPlayerData()->m_bGroupStuffDisabled) {
        return;
    }

    // Down: Tell the group to stop following the player/recruit a ped (on a short press), disband the group (on a long press)
    if (!FindPlayerVehicle(-1, false)) {
        auto* const pd = GetPlayerData();
        if (pad->GetGroupControlBack()) {
            pd->m_nPadDownPressedInMilliseconds += TimeStepInMS();
            if (GetPlayerData()->m_nPadDownPressedInMilliseconds >= HOLD_TIME_MS) {
                DisbandPlayerGroup();
            }
        } else {
            if (pd->m_nPadDownPressedInMilliseconds != 0 && pd->m_nPadDownPressedInMilliseconds < HOLD_TIME_MS) {
                if (   target
                    && target->GetIsTypePed()
                    && target->AsPed()->m_nPedType == PED_TYPE_GANG2
                ) {
                    MakeThisPedJoinOurGroup(target->AsPed());
                } else {
                    TellGroupToStartFollowingPlayer(false, true, false);
                }
            }
            GetPlayerData()->m_nPadDownPressedInMilliseconds = 0;
        }
    }

    // Every 32 frames: Grove Street peds hate cops if the player is wanted (and don't otherwise)
    if ((CTimer::GetFrameCounter() & 0x1F) == 6) {
        auto* const pool = GetPedPool();
        for (auto i = pool->GetSize(); i-- > 0;) {
            auto* const ped = pool->GetAt(i);
            if (!ped || ped->m_nPedType != PED_TYPE_GANG2) {
                continue;
            }
            if ((int32)FindPlayerPed(-1)->GetPlayerData()->m_pWanted->m_WantedLevel > 0) {
                ped->m_acquaintance.SetAsAcquaintance(ACQUAINTANCE_HATE, CPedType::GetPedFlag(PED_TYPE_COP));
            } else {
                ped->m_acquaintance.ClearAsAcquaintance(ACQUAINTANCE_HATE, CPedType::GetPedFlag(PED_TYPE_COP));
            }
        }
    }
}

// 0x60D5A0
bool CPlayerPed::PlayerHasJustAttackedSomeone() {
    return PlayerWantsToAttack();
}

// 0x60D790
void CPlayerPed::SetupPlayerPed(int32 playerId) {
    auto ped = new CPlayerPed(playerId, false);
    auto& playerInfo = FindPlayerInfo(playerId);
    playerInfo.m_pPed = ped;

    if (playerId == 1)
        ped->m_nPedType = PED_TYPE_PLAYER2;

    ped->SetOrientation(0.0f, 0.0f, 0.0f);
    CWorld::Add(ped);
    ped->m_nWeaponAccuracy = 100;
    playerInfo.m_nPlayerState = ePlayerState::PLAYERSTATE_PLAYING;
}

// 0x60D850
void CPlayerPed::ProcessWeaponSwitch(CPad* pad) {
    // NOTE: The slot indices are signed bytes in the original (`m_nChosenWeapon` is -1 when nothing's chosen)
    const auto GetChosenSlot = [this] { return (int8)GetPlayerData()->m_nChosenWeapon; };
    const auto SetChosenSlot = [this](int32 slot) { GetPlayerData()->m_nChosenWeapon = (uint8)(int8)slot; };
    const auto GetActiveSlot = [this] { return (int8)m_nActiveWeaponSlot; };

    const auto IsCamMode = [](std::initializer_list<eCamMode> modes) {
        return rng::contains(modes, (eCamMode)TheCamera.GetActiveCam().m_nMode);
    };

    // Can the weapon in the slot be switched to?
    const auto CanSwitchToSlot = [this](int32 slot) {
        auto& weapon = m_aWeapons[slot];
        return weapon.m_Type != WEAPON_UNARMED
            && weapon.HasWeaponAmmoToBeUsed()
            && (!CGameLogic::IsCoopGameGoingOn() || weapon.CanBeUsedFor2Player());
    };

    // Returns whether the second part (applying the chosen weapon) should be executed
    const auto ChooseWeapon = [&]() -> bool {
        if (CDarkel::FrenzyOnGoing() || m_pAttachedTo || GetIntelligence()->GetTaskJetPack()) {
            return true;
        }

        // Cycle through the weapons using the pad
        if (!m_pTargetedObject
            && !GetPlayerData()->m_bFreeAiming
            && !GetPlayerData()->m_bDontAllowWeaponChange
            && !GetPlayerData()->m_bInVehicleDontAllowWeaponChange
        ) {
            if (pad->CycleWeaponRightJustDown()) {
                if (!IsCamMode({ MODE_M16_1STPERSON, MODE_M16_1STPERSON_RUNABOUT, MODE_SNIPER, MODE_SNIPER_RUNABOUT, MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_RUNABOUT, MODE_ROCKETLAUNCHER_HS, MODE_ROCKETLAUNCHER_RUNABOUT_HS, MODE_CAMERA })) {
                    SetChosenSlot(GetActiveSlot() + 1);
                    for (; GetChosenSlot() < 13; SetChosenSlot(GetChosenSlot() + 1)) {
                        if (CanSwitchToSlot(GetChosenSlot())) {
                            goto chosen;
                        }
                    }
                    SetChosenSlot(0);
                }
            } else if (pad->CycleWeaponLeftJustDown()) {
                if (!IsCamMode({ MODE_M16_1STPERSON, MODE_SNIPER, MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_HS, MODE_CAMERA })) {
                    SetChosenSlot(GetActiveSlot() - 1);
                    if (GetChosenSlot() < 0) {
                        SetChosenSlot(12);
                    }
                    while (GetChosenSlot() != 0) {
                        if (CanSwitchToSlot(GetChosenSlot())) {
                            goto chosen;
                        }
                        SetChosenSlot(GetChosenSlot() - 1);
                        if (GetChosenSlot() < 0) {
                            SetChosenSlot(12);
                        }
                    }
                }
            }
        }
    chosen:

        // Out of ammo => switch to the next best weapon
        auto& activeWeapon = m_aWeapons[GetActiveSlot()];
        if (CWeaponInfo::GetWeaponInfo(activeWeapon.m_Type, eWeaponSkill::STD)->GetFireType() == WEAPON_FIRE_MELEE) {
            return true;
        }
        if (pad->GetWeapon(this) && activeWeapon.m_Type == WEAPON_MINIGUN) {
            return true;
        }
        if ((int32)activeWeapon.m_TotalAmmo > 0) {
            return true;
        }
        if (IsCamMode({ MODE_M16_1STPERSON, MODE_SNIPER, MODE_ROCKETLAUNCHER, MODE_ROCKETLAUNCHER_HS })) {
            return false;
        }

        if (activeWeapon.m_Type == WEAPON_DETONATOR && m_aWeapons[8].m_Type == WEAPON_REMOTE_SATCHEL_CHARGE) {
            SetChosenSlot(8);
        } else {
            SetChosenSlot(GetActiveSlot() - 1);
        }
        for (; GetChosenSlot() >= 0; SetChosenSlot(GetChosenSlot() - 1)) {
            const auto slot = GetChosenSlot();
            if (slot == 5 && m_aWeapons[5].m_Type == WEAPON_BASEBALLBAT) {
                return true;
            }
            // NOTE: The slots 16..18 don't exist
            if ((int32)m_aWeapons[slot].m_TotalAmmo > 0 && slot != 0x12 && slot != 0x11 && slot != 0x10) {
                return true;
            }
        }
        SetChosenSlot(0);
        return true;
    };

    if (!ChooseWeapon()) {
        return;
    }

    // Apply the chosen weapon
    if (GetChosenSlot() == GetActiveSlot()) {
        return;
    }

    if (const auto* const gun = GetIntelligence()->GetTaskUseGun()) { // Original calls `GetTaskUseGun` for every check
        switch (gun->GetLastGunCommand()) {
        case eGunCommand::FIRE:
        case eGunCommand::FIREBURST:
            return;
        case eGunCommand::RELOAD:
            if (gun->m_Anim) {
                return;
            }
            break;
        default:
            break;
        }
    }

    RemoveWeaponAnims(GetActiveSlot(), -1000.f);
    if (GetChosenSlot() != -1) {
        MakeChangesForNewWeapon(m_aWeapons[GetChosenSlot()].m_Type);
    }
}

// 0x60DC50
bool CPlayerPed::FindWeaponLockOnTarget() {
    constexpr auto PI     = std::numbers::pi_v<float>; // 0x858CB8
    constexpr auto TWO_PI = 2.0f * PI;                 // 0x858CBC
    constexpr auto HALF_PI = std::numbers::pi_v<float> / 2.0f; // 0x858FE4

    // The original does the angle arithmetic in extended precision, hence `double`
    // Wraps the angle into [-pi, pi] and returns its absolute value
    const auto WrapAngleAbs = [&](double a) {
        while (a > (double)PI) {
            a -= (double)TWO_PI;
        }
        while (a < (double)-PI) { // 0x858CC0
            a += (double)TWO_PI;
        }
        return a < 0.0 ? -a : a;
    };

    const auto  wepType = GetActiveWeapon().m_Type;
    const auto* wepInfo = CWeaponInfo::GetWeaponInfo(wepType, GetWeaponSkill()); // 0x743C60
    const float wepRange = wepInfo->m_fTargetRange;

    // Already have a target => check that it is still in range
    if (m_pTargetedObject) {
        const auto& targetPos = m_pTargetedObject->GetPosition();
        const auto& selfPos   = GetPosition();
        const double dx = (double)targetPos.x - (double)selfPos.x;
        const double dy = (double)targetPos.y - (double)selfPos.y;
        const float  dist = (float)std::sqrt(dy * dy + dx * dx);
        const float  mult = CWeapon::TargetWeaponRangeMultiplier(m_pTargetedObject, this); // 0x73B380
        if (!((double)mult * (double)wepRange >= (double)dist)) { // NOTE: `TEST AH, 1` after FCOMP => also taken if unordered
            CEntity::ClearReference(m_pTargetedObject);
            return false;
        }
        return true;
    }

    CEntity* bestTarget   = nullptr;
    float    bestPriority = -10000.0f; // 0xC61C4000

    const auto& fwd = GetForward();
    float aimAngle = (float)CGeneral::GetATanOfXYExt(fwd.x, fwd.y); // 0x53CC70

    // 0x53FB70. NOTE: The original uses a null pad for other ped types (and would crash)
    CPad* const pad = m_nPedType == PED_TYPE_PLAYER1 ? CPad::GetPad(0)
                    : m_nPedType == PED_TYPE_PLAYER2 ? CPad::GetPad(1)
                    : nullptr;

    // Use the direction of the left stick (relative to the camera), if it is pushed far enough
    constexpr auto STICK_THRESHOLD = 60.0f; // 0x858B34
    if (std::abs((float)pad->GetPedWalkLeftRight()) > STICK_THRESHOLD || std::abs((float)pad->GetPedWalkUpDown()) > STICK_THRESHOLD) {
        const auto upDown    = pad->GetPedWalkUpDown();
        const auto leftRight = pad->GetPedWalkLeftRight();
        const double stickAngle = CGeneral::GetRadianAngleBetweenPointsExt(0.0f, 0.0f, (float)-(int32)leftRight, (float)upDown); // 0x53CBE0
        aimAngle = CGeneral::LimitRadianAngle((float)(stickAngle - (double)TheCamera.m_fOrientation + (double)HALF_PI));
    }

    // Spray can => look for a tag to spray first
    if (GetActiveWeapon().m_Type == WEAPON_SPRAYCAN) {
        // NOTE: The original has a `m_matrix == nullptr` path for the heading (m_placement.m_fHeading),
        // but dereferences the matrix unconditionally right afterwards.
        const float heading = (float)std::atan2((double)-fwd.x, (double)fwd.y);
        float       bestDelta = PI;

        const auto& selfPos = GetPosition();
        const CVector center{
            (float)((double)fwd.x * 8.0 + (double)selfPos.x), // 0x859000
            (float)((double)fwd.y * 8.0 + (double)selfPos.y),
            (float)((double)fwd.z * 8.0 + (double)selfPos.z),
        };

        int16    numFound{};
        CEntity* found[16]{};
        CWorld::FindObjectsInRange(center, 8.0f, false, &numFound, 15, found, true, false, false, false, false); // 0x564A20

        CEntity* bestTag{};
        for (int32 i = 0; i < numFound; i++) {
            CEntity* const e = found[i];
            if (!CTagManager::IsTag(*e) || CTagManager::GetAlpha(*e) >= 0xFF) {
                continue;
            }

            const auto& ePos = e->GetPosition();
            const double dx = (double)ePos.x - (double)GetPosition().x;
            const double dy = (double)ePos.y - (double)GetPosition().y;
            const double dz = (double)ePos.z - (double)GetPosition().z;
            if (!((double)wepRange * (double)wepRange > (dx * dx + dz * dz) + dy * dy)) {
                continue;
            }

            double delta = std::atan2(-dx, dy) - (double)heading;
            if (delta < (double)-PI) {
                delta += (double)TWO_PI;
            } else if (delta > (double)PI) {
                delta -= (double)TWO_PI;
            }

            if (delta < (double)bestDelta || !bestTag) {
                bestDelta = (float)delta;
                bestTag   = e;
            }
        }

        if (bestTag && !LOSBlockedBetweenPeds(this, bestTag)) { // 0x60B550
            CEntity::ChangeEntityReference(m_pTargetedObject, bestTag);
            GetPlayerData()->m_bDontAllowWeaponChange = true; // 0x85
            return true;
        }
    }

    // Peds
    constexpr auto FOV_SCALE = 0.008726646f; // 0x8631D4 (pi / 360)
    for (auto i = GetPedPool()->GetSize(); i-- > 0;) {
        CPed* const ped = GetPedPool()->GetAt(i);
        if (!ped || ped == this) {
            continue;
        }
        if (ped->m_nPedState == PEDSTATE_DIE || ped->m_nPedState == PEDSTATE_DEAD) { // 0x36, 0x37
            continue;
        }
        if (ped->bInVehicle) {
            const auto* const veh = ped->m_pVehicle;
            if (!veh || (veh->m_nVehicleType != VEHICLE_TYPE_BIKE && !veh->vehicleFlags.bVehicleCanBeTargetted)) {
                continue;
            }
        }
        if (ped->bNeverEverTargetThisPed) { // 0x470, 0x10000000
            continue;
        }
        if ((ped->m_nPedType == PED_TYPE_PLAYER1 || ped->m_nPedType == PED_TYPE_PLAYER2) && CGameLogic::bPlayersCannotTargetEachOther) {
            continue;
        }
        if (CPedGroups::AreInSameGroup(ped, this)) { // 0x5F7F40
            continue;
        }

        // Is the ped within the view cone?
        const auto& pedPos  = ped->GetPosition();
        const auto& selfPos = GetPosition();
        const double angleToPed = CGeneral::GetATanOfXYExt(pedPos.x - selfPos.x, pedPos.y - selfPos.y); // 0x53CC70
        if (!(WrapAngleAbs(angleToPed - (double)aimAngle) < (double)PLAYER_MAX_TARGET_VIEW_ANGLE * (double)FOV_SCALE)) {
            continue;
        }

        // ...and also from a point behind the player
        CVector fwdNorm = GetForward();
        fwdNorm.Normalise(); // 0x59C910
        const CVector behindPos = GetPosition() - fwdNorm * StaticRef<float>(0x8D2440); // 3.0f
        const double angleToPedFromBehind = CGeneral::GetATanOfXYExt(ped->GetPosition().x - behindPos.x, ped->GetPosition().y - behindPos.y); // 0x53CC70
        if (!(WrapAngleAbs(angleToPedFromBehind - (double)aimAngle) < (double)StaticRef<float>(0x8D2438) * (double)FOV_SCALE)) { // 90.0f
            continue;
        }

        const float dist    = (ped->GetPosition() - GetPosition()).Magnitude();
        const float maxDist = (float)((double)CWeapon::TargetWeaponRangeMultiplier(ped, this) * (double)wepRange);
        if (!(dist < maxDist)) { // NOTE: `JP` after FCOMP => also skipped if unordered
            continue;
        }

        EvaluateTarget(ped, bestTarget, bestPriority, maxDist, aimAngle, false); // 0x60D020
    }

    // Objects
    for (auto i = GetObjectPool()->GetSize(); i-- > 0;) {
        CObject* const obj = GetObjectPool()->GetAt(i);
        if (!obj || !obj->CanBeTargetted() || obj->objectFlags.bIsExploded /* 0x140, 0x40 */ || !obj->GetRwObject()) {
            continue;
        }
        if (!CanIKReachThisTarget(obj->GetPosition(), &GetActiveWeapon(), true)) { // 0x609F80
            continue;
        }
        EvaluateTarget(obj, bestTarget, bestPriority, wepRange, aimAngle, true);
    }

    // Vehicles (co-op only)
    if (CGameLogic::IsCoopGameGoingOn()) { // 0x441390
        for (auto i = GetVehiclePool()->GetSize(); i-- > 0;) {
            CVehicle* const veh = GetVehiclePool()->GetAt(i);
            if (!veh || veh->physicalFlags.bRenderScorched /* 0x40, 0x20000000 */ || veh->m_nVehicleSubType == VEHICLE_TYPE_BMX) {
                continue;
            }
            if (!CanIKReachThisTarget(veh->GetPosition(), &GetActiveWeapon(), true)) {
                continue;
            }
            EvaluateTarget(veh, bestTarget, bestPriority, wepRange, aimAngle, true);
        }
    }

    if (!bestTarget) {
        return false;
    }

    // NOTE: The original has two identical copies of this (for ped and non-ped targets)
    CEntity::ChangeEntityReference(m_pTargetedObject, bestTarget);
    GetPlayerData()->m_bDontAllowWeaponChange = true; // 0x85
    return true;
}

// 0x60E530
bool CPlayerPed::FindNextWeaponLockOnTarget(CEntity* currentTarget, bool arg1) {
    const auto  wepType  = GetActiveWeapon().m_Type;
    const auto* wepInfo  = CWeaponInfo::GetWeaponInfo(wepType, GetWeaponSkill()); // 0x743C60
    const float wepRange = wepInfo->m_fTargetRange;

    CEntity* bestTarget   = nullptr;
    float    bestPriority = -10000.0f; // 0xC61C4000

    // NOTE: The original calls `GetATanOfXY(GetForward())` here and discards the result

    // Angle from the camera to the current target, or the camera's forward direction if there is none
    float refX, refY;
    if (currentTarget) {
        const auto& targetPos = currentTarget->GetPosition();
        const auto& camPos    = TheCamera.GetPosition();
        refY = targetPos.y - camPos.y;
        refX = targetPos.x - camPos.x;
    } else {
        refX = TheCamera.m_mCameraMatrix.GetForward().x; // 0xB6F9AC
        refY = TheCamera.m_mCameraMatrix.GetForward().y; // 0xB6F9B0
    }
    const float angle = (float)CGeneral::GetATanOfXYExt(refX, refY); // 0x53CC70

    // BUG: In the original the range multiplier of the object and vehicle loops below is evaluated for
    // whatever the register held after the ped loop (the ped in slot 0 or null; `currentTarget` if the
    // ped pool has no slots) instead of the object / vehicle being evaluated.
    CEntity* rangeTarget = currentTarget;

    // Peds
    for (auto i = GetPedPool()->GetSize(); i-- > 0;) {
        CPed* const ped = GetPedPool()->GetAt(i);
        rangeTarget = ped;
        if (!ped || ped == this || ped == currentTarget) {
            continue;
        }
        if (ped->m_nPedState == PEDSTATE_DIE || ped->m_nPedState == PEDSTATE_DEAD) { // 0x36, 0x37
            continue;
        }
        if (ped->bInVehicle) {
            const auto* const veh = ped->m_pVehicle;
            if (!veh || (veh->m_nVehicleType != VEHICLE_TYPE_BIKE && !veh->vehicleFlags.bVehicleCanBeTargetted)) {
                continue;
            }
        }
        if (ped->bNeverEverTargetThisPed) { // 0x470, 0x10000000
            continue;
        }
        if (CPedGroups::AreInSameGroup(ped, this)) { // 0x5F7F40
            continue;
        }
        if ((ped->m_nPedType == PED_TYPE_PLAYER1 || ped->m_nPedType == PED_TYPE_PLAYER2) && CGameLogic::bPlayersCannotTargetEachOther) {
            continue;
        }
        if (LOSBlockedBetweenPeds(this, ped)) { // 0x60B550
            continue;
        }
        if (!CanIKReachThisTarget(ped->GetPosition(), &GetActiveWeapon(), true)) { // 0x609F80
            continue;
        }
        const float mult = CWeapon::TargetWeaponRangeMultiplier(ped, this); // 0x73B380
        EvaluateNeighbouringTarget(ped, &bestTarget, &bestPriority, (float)((double)mult * (double)wepRange), angle, arg1); // 0x60D1C0
    }

    // Objects
    for (auto i = GetObjectPool()->GetSize(); i-- > 0;) {
        CObject* const obj = GetObjectPool()->GetAt(i);
        if (!obj || !obj->CanBeTargetted() || obj->objectFlags.bIsExploded /* 0x140, 0x40 */ || !obj->GetRwObject()) {
            continue;
        }
        if (!CanIKReachThisTarget(obj->GetPosition(), &GetActiveWeapon(), true)) {
            continue;
        }
        const float mult = CWeapon::TargetWeaponRangeMultiplier(notsa::IsFixBugs() ? (CEntity*)obj : rangeTarget, this);
        EvaluateNeighbouringTarget(obj, &bestTarget, &bestPriority, (float)((double)mult * (double)wepRange), angle, arg1);
    }

    // Vehicles (co-op only)
    if (CGameLogic::IsCoopGameGoingOn()) { // 0x441390
        for (auto i = GetVehiclePool()->GetSize(); i-- > 0;) {
            CVehicle* const veh = GetVehiclePool()->GetAt(i);
            if (!veh || veh->physicalFlags.bRenderScorched /* 0x40, 0x20000000 */ || veh->m_nVehicleSubType == VEHICLE_TYPE_BMX) {
                continue;
            }
            if (!CanIKReachThisTarget(veh->GetPosition(), &GetActiveWeapon(), true)) {
                continue;
            }
            const float mult = CWeapon::TargetWeaponRangeMultiplier(notsa::IsFixBugs() ? (CEntity*)veh : rangeTarget, this);
            EvaluateNeighbouringTarget(veh, &bestTarget, &bestPriority, (float)((double)mult * (double)wepRange), angle, arg1);
        }
    }

    if (!bestTarget) {
        return false;
    }

    // Tell the new target (or its group) that a gun is aimed at it
    if (bestTarget->GetIsTypePed() && CWeaponInfo::GetWeaponInfo(GetActiveWeapon().m_Type, eWeaponSkill::STD)->m_nWeaponFire != WEAPON_FIRE_MELEE) {
        CPed* const targetPed = bestTarget->AsPed();
        if (auto* const group = CPedGroups::GetPedsGroup(targetPed)) { // 0x5F7E80
            if (!CPedGroups::AreInSameGroup(targetPed, this)) {
                CEventGroupEvent event{ targetPed, new CEventGunAimedAt{ this } }; // 0x4B0700, 0x4ADFD0
                group->GetIntelligence().AddEvent(&event); // 0x5F7470
            }
        } else {
            CEventGunAimedAt event{ this }; // 0x4B0700
            targetPed->GetIntelligence()->m_eventGroup.Add(&event, false); // 0x4AB420
        }
    }

    CEntity::ChangeEntityReference(m_pTargetedObject, bestTarget);
    GetPlayerData()->m_bDontAllowWeaponChange = true; // 0x85
    return true;
}

// 0x60EA90
void CPlayerPed::ProcessControl() {
    if (GetPlayerData()->m_nCarDangerCounter)
        GetPlayerData()->m_nCarDangerCounter--;
    if (!GetPlayerData()->m_nCarDangerCounter)
        GetPlayerData()->m_pDangerCar = 0;
    if (GetPlayerData()->m_nFadeDrunkenness) {
        if (GetPlayerData()->m_nDrunkenness - 1 > 0) {
            --GetPlayerData()->m_nDrunkenness;
        } else {
            GetPlayerData()->m_nDrunkenness = 0;
            CMBlur::ClearDrunkBlur();
            GetPlayerData()->m_nFadeDrunkenness = 0;
        }
    }
    if (GetPlayerData()->m_nDrunkenness) 
        CMBlur::SetDrunkBlur(GetPlayerData()->m_nDrunkenness * ExeRecip(255.0f));
    if (GetPlayerData()->m_bRequireHandleBreath) {
        if (CStats::GetFatAndMuscleModifier(STAT_MOD_AIR_IN_LUNG) > GetPlayerData()->m_fBreath)
            GetPlayerData()->m_fBreath += CTimer::GetTimeStep() + CTimer::GetTimeStep();
        GetPlayerData()->m_bRequireHandleBreath = false;
    }
    GetPlayerData()->m_bRequireHandleBreath = true;
    CPed::ProcessControl();
    bCheckColAboveHead = true;
    float markColor = 1.0f;
    bool limitMarkColor = true;
    CVector effectPos;
    CPad* pad = CPad::GetPad(m_nPedType);
    if (!bCanPointGunAtTarget) {
        GetPlayerWanted()->Update();
        PruneReferences();
        if (GetActiveWeapon().m_Type == WEAPON_MINIGUN) {
            auto weaponInfo = CWeaponInfo::GetWeaponInfo(WEAPON_MINIGUN, eWeaponSkill::STD);
            if (GetIntelligence()->GetTaskUseGun()) {
                auto animAssoc = GetIntelligence()->GetTaskUseGun()->m_Anim;
                if (animAssoc && animAssoc->m_CurrentTime - animAssoc->m_TimeStep < weaponInfo->m_fAnimLoopEnd) {
                    if (GetPlayerData()->m_fGunSpinSpeed < 0.45f) {
                        GetPlayerData()->m_fGunSpinSpeed += CTimer::GetTimeStep() * 0.025f;
                        GetPlayerData()->m_fGunSpinSpeed = std::min(GetPlayerData()->m_fGunSpinSpeed, 0.45f);
                    }
                    if (pad->GetWeapon(this) && GetActiveWeapon().m_TotalAmmo > 0 && animAssoc->m_CurrentTime >= weaponInfo->m_fAnimLoopStart) 
                        m_weaponAudio.AddAudioEvent(AE_WEAPON_FIRE_MINIGUN_AMMO);
                    else 
                        m_weaponAudio.AddAudioEvent(AE_WEAPON_FIRE_MINIGUN_NO_AMMO);
                }
            } else {
                if (GetPlayerData()->m_fGunSpinSpeed > 0.0f) {
                    GetPlayerData()->m_fGunSpinSpeed -= CTimer::GetTimeStep() * 0.003f;
                    GetPlayerData()->m_fGunSpinSpeed = std::max(GetPlayerData()->m_fGunSpinSpeed, 0.0f);
                }
            }
        }
        if (GetActiveWeapon().m_Type == WEAPON_CHAINSAW && m_nPedState != PEDSTATE_ATTACK && !bInVehicle) {
            GetIntelligence()->GetTaskSwim(); // hmmm?
        }
        if (m_pTargetedObject) {
            ClearReference(m_p3rdPersonMouseTarget);
            if (m_pTargetedObject->GetIsTypePed()) {
                CPed* targetPed = m_pTargetedObject->AsPed();
                auto weaponInfo = CWeaponInfo::GetWeaponInfo(GetActiveWeapon().m_Type, GetWeaponSkill());
                float targetHeadRange = weaponInfo->GetTargetHeadRange();
                markColor = targetPed->m_fHealth / targetPed->m_fMaxHealth;
                bool instantFireHit = false;
                if (targetPed->IsAlive()) {
                    auto stdWeaponInfo = CWeaponInfo::GetWeaponInfo(GetActiveWeapon().m_Type, eWeaponSkill::STD);
                    if (stdWeaponInfo->m_nWeaponFire == WEAPON_FIRE_INSTANT_HIT) {
                        instantFireHit = true;
                        CVector distance = targetPed->GetPosition() - GetPosition();
                        if (targetHeadRange * targetHeadRange > distance.SquaredMagnitude()) {
                            GetPlayerData()->m_nTargetBone = BONE_HEAD;
                            GetPlayerData()->m_vecTargetBoneOffset.x = 0.05f;
                        }
                    }
                }
                if (!instantFireHit) {
                    GetPlayerData()->m_nTargetBone = BONE_SPINE1;
                    GetPlayerData()->m_vecTargetBoneOffset.x = 0.2f;
                }
                effectPos = GetPlayerData()->m_vecTargetBoneOffset;
                targetPed->GetTransformedBonePosition(effectPos, static_cast<eBoneTag>(GetPlayerData()->m_nTargetBone), false);
                bool targetIsInVehicle = false;
                if (markColor > 0.0f) {
                    if (!targetPed->bInVehicle && targetPed->m_nMoveState != PEDMOVE_STILL) {
                        effectPos += targetPed->m_vecMoveSpeed * CTimer::GetTimeStep();
                    }
                }
                if (targetPed->bInVehicle) {
                    auto targetVeh = targetPed->m_pVehicle;
                    if (targetVeh)
                        effectPos += (targetVeh->m_vecMoveSpeed + targetVeh->m_vecTurnSpeed) * CTimer::GetTimeStep();
                }
            } else if (m_pTargetedObject->GetIsTypeVehicle()) {
                CVehicle* targetVeh = m_pTargetedObject->AsVehicle();
                effectPos = (targetVeh->m_vecMoveSpeed + targetVeh->m_vecTurnSpeed) * CTimer::GetTimeStep();
                effectPos += targetVeh->GetPosition();
            } else if (m_pTargetedObject->GetIsTypeObject()) {
                CObject* targetObj = m_pTargetedObject->AsObject();
                effectPos = targetObj->m_vecMoveSpeed * CTimer::GetTimeStep();
                effectPos += targetObj->GetPosition();
                markColor = targetObj->m_fHealth * 0.001f;
            } else {
                effectPos = m_pTargetedObject->GetPosition();
                limitMarkColor = false; 
            }
        }
    }
    if (m_pTargetedObject) {
        uint8 r = 0, g = 0, b = 0;
        bool setRGB = true;
        if (limitMarkColor) {
            if (markColor > 0.0f)
                markColor = std::min(markColor, 1.0f);
            else
                setRGB = false;
        }
        if (setRGB) {
            r = static_cast<uint8>((1.0f - markColor) * 255.0f);
            g = static_cast<uint8>(markColor * 255.0f);
            b = static_cast<uint8>(0.0f);
        }
        CVector distance = effectPos - GetPosition();
        float size = 1.0f - distance.Magnitude() * 0.02f;
        CWeaponEffects::MarkTarget(m_nPedType, effectPos, r, g, b, 255u, size, false);
    }
    if (m_nMoveState != PEDMOVE_NONE) {
        if (m_nMoveState != PEDMOVE_RUN) {
            if (m_nMoveState != PEDMOVE_SPRINT)
                HandleSprintEnergy(false, 1.0f);
        } else if (CStats::GetFatAndMuscleModifier(STAT_MOD_TIME_CAN_RUN) > GetPlayerData()->m_fTimeCanRun)
            GetPlayerData()->m_fTimeCanRun += CTimer::GetTimeStep() * 0.15f;
    } else if (bInVehicle) {
        if (m_pVehicle && !m_pVehicle->IsSubBMX())
            HandleSprintEnergy(false, 1.0f);
    }
    GetActiveWeapon().Update(this);
    if (m_nPedState == PEDSTATE_DEAD || m_nPedState == PEDSTATE_DIE) {
        ClearWeaponTarget();
        return;
    }
    if (pad) {
        if (pad->WeaponJustDown(this)) {
            auto& activeWeapon = GetActiveWeapon();
            auto weaponType = activeWeapon.m_Type;
            if (!TheCamera.Using1stPersonWeaponMode() || activeWeapon.m_State == WEAPONSTATE_OUT_OF_AMMO) {
                if (!GetIntelligence()->GetTaskSwim()) {
                    if (weaponType == WEAPON_SNIPERRIFLE) {
                        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_FIRE_FAIL_SNIPERRIFFLE, 0.0f, 1.0f);
                    } else if (weaponType == WEAPON_RLAUNCHER || weaponType == WEAPON_RLAUNCHER_HS) {
                        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_FIRE_FAIL_ROCKET, 0.0f, 1.0f);
                    }
                }
            }
        }
        if (IsPedShootable() && this->m_nPedState != PEDSTATE_ANSWER_MOBILE) {
            int32 slot = CWorld::FindPlayerSlotWithPedPointer(this);
            if (!CWorld::Players[slot].m_pRemoteVehicle)
                ProcessWeaponSwitch(pad);
        }
    }
    ProcessAnimGroups();
    if (pad && TheCamera.GetActiveCamera().m_nMode == MODE_FOLLOWPED && !TheCamera.GetActiveCamera().m_nDirectionWasLooking) {
        auto& activeCam = TheCamera.GetActiveCamera();
        m_nLookTime = 0;
        float lookDir = CGeneral::LimitRadianAngle(atan2(-activeCam.m_vecFront.x, activeCam.m_vecFront.y));
        float angle = fabs(lookDir - m_fCurrentRotation);
        if (m_nPedState != PEDSTATE_ATTACK && angle > DegreesToRadians(30.0f) && angle < DegreesToRadians(330.0f)) {
            if (angle > DegreesToRadians(150.0f) && angle < DegreesToRadians(210.0f)) {
                float dir1 = CGeneral::LimitRadianAngle(m_fCurrentRotation - DegreesToRadians(150.0f));
                float dir2 = CGeneral::LimitRadianAngle(m_fCurrentRotation + DegreesToRadians(150.0f));
                lookDir = dir1;
                if (m_fLookDirection != 999'999.f && !bIsDucking) {
                    if (fabs(dir2 - m_fLookDirection) <= fabs(dir1 - m_fLookDirection))
                        lookDir = dir2;
                }
            }
            SetLookFlag(lookDir, true, false);
            SetLookTimer(static_cast<uint32>((CTimer::GetTimeStep() * 0.02f * 1000.0f) * 5.0f));
        } else {
            ClearLookFlag();
        }
    }
    if (m_nMoveState == PEDMOVE_SPRINT && bIsLooking) {
        ClearLookFlag();
        SetLookTimer(250);
    }
    if (m_vecMoveSpeed.Magnitude() >= 0.1f) {
        GetPlayerData()->m_nStandStillTimer = 0;
        GetPlayerData()->m_bStoppedMoving = false;
    } else if (!GetPlayerData()->m_nStandStillTimer) {
        GetPlayerData()->m_nStandStillTimer = CTimer::GetTimeInMS() + 500;
    } else if (CTimer::GetTimeInMS() > GetPlayerData()->m_nStandStillTimer) {
        GetPlayerData()->m_bStoppedMoving = true;
    }
    if (GetPlayerData()->m_bDontAllowWeaponChange) {
        if (IsPlayer()) {
            if (!CPad::GetPad(0)->GetTarget())
                GetPlayerData()->m_bDontAllowWeaponChange = false;
        }
    }
    if (m_nPedState != PEDSTATE_SNIPER_MODE && GetActiveWeapon().m_State == WEAPONSTATE_FIRING)
        GetPlayerData()->m_nLastTimeFiring = CTimer::GetTimeInMS();
    ProcessGroupBehaviour(pad);
    if (bInVehicle)
        CCarCtrl::RegisterVehicleOfInterest(m_pVehicle);
    if (!GetIsVisible())
        UpdateRpHAnim();
    if (bInVehicle) {
        CPad* pad = CPad::GetPad(0);
        if (!pad->IsDPadDownPressed()) {
            if (pad->IsDPadUpPressed())
                GetPlayerData()->m_bPlayersGangActive = true;
        } else {
            GetPlayerData()->m_bPlayersGangActive = false;
        }
    }
    if (physicalFlags.bSubmergedInWater) {
        CVector pos = GetPosition();
        pos.z += 1.5f;
        if (CWaterLevel::GetWaterLevel(pos.x, pos.y, pos.z, GetPlayerData()->m_fWaterHeight, true, nullptr)) {
            auto& box = CEntity::GetColModel()->GetBoundingBox();
            float playerMinZ = pos.z + box.m_vecMin.z;
            float playerMaxZ = pos.z + box.m_vecMax.z;
            if (GetPlayerData()->m_fWaterHeight < playerMaxZ) {
                if (GetPlayerData()->m_fWaterHeight > playerMinZ)
                    GetPlayerData()->m_nWaterCoverPerc = static_cast<uint8>((GetPlayerData()->m_fWaterHeight - playerMinZ) / (playerMaxZ - playerMinZ) * 100.0f);
                else
                    GetPlayerData()->m_nWaterCoverPerc = 0;
            } else {
                GetPlayerData()->m_nWaterCoverPerc = 100;
            }
        } else {
            physicalFlags.bSubmergedInWater = false;
        }
    } else {
        GetPlayerData()->m_nWaterCoverPerc = 0;
    }
    if ((CTimer::GetFrameCounter() & 0x7F) == 0 && !FindPlayerVehicle()) {
        auto& group = CPedGroups::GetGroup(GetPlayerData()->m_nPlayerGroup);
        if (group.m_bMembersEnterLeadersVehicle) {
            int32 memberCount = group.m_groupMembership.CountMembersExcludingLeader();
            if (memberCount > 0) {
                float distance = group.FindDistanceToNearestMember(nullptr);
                if (distance > 20.0f && distance < 100.0f && CGame::currArea == AREA_CODE_NORMAL_WORLD) {
                    if (memberCount == 1)
                        Say(CTX_GLOBAL_ORDER_KEEP_UP_ONE);
                    else
                        Say(CTX_GLOBAL_ORDER_KEEP_UP_MANY);
                    for (int32 i = 0; i < TOTAL_PED_GROUP_FOLLOWERS; ++i) {
                        CPed* member = group.m_groupMembership.GetMember(i);
                        if (member && CGeneral::GetRandomNumberInRange(0.0f, 1.0f) < 0.5f) {
                            int32 offset = CGeneral::GetRandomNumberInRange(3000, 4500);
                            member->Say(CTX_GLOBAL_FOLLOW_REPLY, offset);
                        }
                    }
                }
            }
        }
    }
    if (!bInVehicle && GetLightingTotal() <= 0.05f && !CEntryExitManager::WeAreInInteriorTransition())
        Say(CTX_GLOBAL_BREATHING);
}

// 0x609490
void CPlayerPed::SetMoveAnim() {
    //nop
}
