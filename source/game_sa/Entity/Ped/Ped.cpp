/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Ped.h"

#include "PedType.h"
#include "Fx/FxFtol.h"
#include "Buoyancy.h"
#include "TaskSimpleSwim.h"
#include "PedStats.h"
#include "Conversations.h"
#include "TaskSimpleLand.h"
#include "AEAudioUtility.h"
#include "PedClothesDesc.h"
#include "EventAcquaintancePedHate.h"
#include "TaskSimpleHoldEntity.h"
#include "TaskComplexGoPickUpEntity.h"
#include "Radar.h"
#include "PostEffects.h"
#include "PedStdBonePositions.h"
#include "TaskSimpleJetPack.h"
#include "PedSaveStructure.h"
#include "TaskSimpleStandStill.h"
#include "TaskComplexFacial.h"
#include "WeaponInfo.h"
#include "Shadows.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "RealTimeShadowManager.h"
#include "WindModifiers.h"
#include "Automobile.h"
#include "Bike.h"
#include "Object.h"
#include "Crime.h"
#include "EventDamage.h"
#include "PedDamageResponseCalculator.h"
#include "TaskComplexDestroyCarMelee.h"
#include "AnimManager.h"
#include "ModelInfo.h"
#include "Pad.h"
#include "CarEnterExit.h"
#include "PedPlacement.h"
#include "Pathfind.h"
#include "World.h"
#include "Fx.h"
#include "FxSystem.h"
#include "FxPrtMult.h"
#include "EventSoundQuiet.h"
#include "Weather.h"
#include "CullZones.h"
#include "Localisation.h"
#include "General.h"
#include "WaterLevel.h"
#include "TaskSimpleUseGun.h"
#include "TempColModels.h"
#include "CustomBuildingDNPipeline.h"

void CPed::InjectHooks() {
    RH_ScopedVirtualClass(CPed, 0x86C358, 26);
    RH_ScopedCategory("Entity/Ped");

    RH_ScopedInstall(Constructor, 0x5E8030);
    RH_ScopedInstall(Destructor, 0x5E8620);

    RH_ScopedInstall(RequestDelayedWeapon, 0x5E8910);
    RH_ScopedInstall(DettachPedFromEntity, 0x5E7EC0);
    RH_ScopedInstall(AttachPedToBike, 0x5E7E60);
    RH_ScopedInstall(AttachPedToEntity, 0x5E7CB0);
    RH_ScopedInstall(OurPedCanSeeThisEntity, 0x5E1660);
    RH_ScopedOverloadedInstall(operator delete, "anon", 0x5E4760, void (*)(void*));
    RH_ScopedOverloadedInstall(operator new, "anon", 0x5E4720, void* (*)(unsigned));
    RH_ScopedOverloadedInstall(operator new, "poolIndexed", 0x5E4730, void* (*)(unsigned, int32));
    RH_ScopedInstall(SpawnFlyingComponent, 0x5F0190);
    RH_ScopedInstall(PedCanPickUpPickUp, 0x455560);
    RH_ScopedInstall(Update, 0x5DEBE0);
    RH_ScopedInstall(Initialise, 0x5DEBB0);
    RH_ScopedInstall(UpdateStatLeavingVehicle, 0x5E01B0);
    RH_ScopedInstall(UpdateStatEnteringVehicle, 0x5E01A0);
    RH_ScopedInstall(ShoulderBoneRotation, 0x5DF560);
    RH_ScopedInstall(RestoreHeadingRateCB, 0x5DFD70);
    RH_ScopedInstall(PedIsInvolvedInConversation, 0x43AB90);
    RH_ScopedInstall(ClearWeapons, 0x5E6320);
    RH_ScopedInstall(ClearWeapon, 0x5E62B0);
    RH_ScopedOverloadedInstall(SetCurrentWeapon, "WepType", 0x5E6280, void(CPed::*)(eWeaponType));
    RH_ScopedOverloadedInstall(SetCurrentWeapon, "Slot", 0x5E61F0, void(CPed::*)(int32));
    RH_ScopedOverloadedInstall(GiveWeapon, "", 0x5E6080, eWeaponSlot(CPed::*)(eWeaponType, uint32, bool));
    RH_ScopedInstall(TakeOffGoggles, 0x5E6010);
    RH_ScopedInstall(AddWeaponModel, 0x5E5ED0);
    RH_ScopedInstall(PlayFootSteps, 0x5E57F0);
    RH_ScopedInstall(DoFootLanded, 0x5E5380);
    RH_ScopedInstall(ClearAll, 0x5E5320);
    RH_ScopedInstall(CalculateNewOrientation, 0x5E52E0);
    RH_ScopedInstall(CalculateNewVelocity, 0x5E4C50);
    RH_ScopedInstall(SetCharCreatedBy, 0x5E47E0);
    RH_ScopedInstall(SetPedState, 0x5E4500);
    RH_ScopedInstall(GiveObjectToPedToHold, 0x5E4390);
    RH_ScopedInstall(ClearLookFlag, 0x5E1950);
    RH_ScopedInstall(WorkOutHeadingForMovingFirstPerson, 0x5E1A00);
    RH_ScopedInstall(UpdatePosition, 0x5E1B10);
    RH_ScopedInstall(MakeTyresMuddySectorList<CPtrListSingleLink<CPhysical*>>, 0x6AE0D0);
    RH_ScopedInstall(IsPedInControl, 0x5E3960);
    RH_ScopedInstall(RemoveWeaponModel, 0x5E3990);
    RH_ScopedInstall(RemoveWeaponWhenEnteringVehicle, 0x5E6370);
    RH_ScopedInstall(AddGogglesModel, 0x5E3A90);
    RH_ScopedInstall(SetWeaponSkill, 0x5E3C10);
    RH_ScopedInstall(ClearLook, 0x5E3FF0);
    RH_ScopedInstall(TurnBody, 0x5E4000);
    RH_ScopedInstall(IsPointerValid, 0x5E4220);
    RH_ScopedOverloadedInstall(GetBonePosition, "Original", 0x5E4280, void(CPed::*)(CVector*, eBoneTag, bool));
    RH_ScopedInstall(PutOnGoggles, 0x5E3AE0);
    RH_ScopedInstall(ReplaceWeaponWhenExitingVehicle, 0x5E6490);
    RH_ScopedInstall(KillPedWithCar, 0x5F0360);
    RH_ScopedInstall(IsPedHeadAbovePos, 0x5F02C0);
    RH_ScopedInstall(RemoveWeaponAnims, 0x5F0250);
    RH_ScopedInstall(DoesLOSBulletHitPed, 0x5F01A0);
    RH_ScopedInstall(RemoveBodyPart, 0x5F0140);
    RH_ScopedInstall(Say, 0x5EFFE0);
    RH_ScopedInstall(SayScript, 0x5EFFB0);
    RH_ScopedInstall(CanPedHoldConversation, 0x5EFFA0);
    RH_ScopedInstall(EnablePedSpeechForScriptSpeech, 0x5EFF90);
    RH_ScopedInstall(DisablePedSpeechForScriptSpeech, 0x5EFF80);
    RH_ScopedInstall(EnablePedSpeech, 0x5EFF70);
    RH_ScopedInstall(DisablePedSpeech, 0x5EFF60);
    RH_ScopedInstall(GetPedTalking, 0x5EFF50);
    RH_ScopedInstall(GiveWeaponWhenJoiningGang, 0x5E8BE0);
    RH_ScopedInstall(GiveDelayedWeapon, 0x5E89B0);
    RH_ScopedOverloadedInstall(GetWeaponSkill, "Current", 0x5E6580, eWeaponSkill(CPed::*)());
    RH_ScopedOverloadedInstall(GetWeaponSkill, "WeaponType", 0x5E3B60, eWeaponSkill(CPed::*)(eWeaponType));
    RH_ScopedInstall(PreRenderAfterTest, 0x5E65A0);
    RH_ScopedInstall(SetIdle, 0x5E7980);
    RH_ScopedOverloadedInstall(SetLook, "Heading", 0x5E79B0, void(CPed::*)(float));
    RH_ScopedOverloadedInstall(SetLook, "Entity", 0x5E7A60, void(CPed::*)(CEntity *));
    RH_ScopedInstall(Look, 0x5E7B20);
    RH_ScopedInstall(ReplaceWeaponForScriptedCutscene, 0x5E6530);
    RH_ScopedInstall(RemoveWeaponForScriptedCutscene, 0x5E6550);
    RH_ScopedInstall(GiveWeaponAtStartOfFight, 0x5E8AB0);
    RH_ScopedInstall(ProcessBuoyancy, 0x5E1FA0);
    RH_ScopedInstall(SortPeds, 0x5E17E0);
    RH_ScopedInstall(PositionPedOutOfCollision, 0x5E0820);
    RH_ScopedInstall(GrantAmmo, 0x5DF220);
    RH_ScopedInstall(GetWeaponSlot, 0x5DF200);
    RH_ScopedInstall(PositionAnyPedOutOfCollision, 0x5E13C0);
    RH_ScopedInstall(CanBeDeletedEvenInVehicle, 0x5DF150);
    RH_ScopedInstall(CanBeDeleted, 0x5DF100);
    RH_ScopedInstall(CanStrafeOrMouseControl, 0x5DF090);
    RH_ScopedInstall(CanBeArrested, 0x5DF060);
    RH_ScopedInstall(CanSetPedState, 0x5DF030);
    RH_ScopedInstall(CanPedReturnToState, 0x5DF000);
    RH_ScopedInstall(UseGroundColModel, 0x5DEFE0);
    RH_ScopedInstall(IsPedShootable, 0x5DEFD0);
    RH_ScopedInstall(GetLocalDirection, 0x5DEF60);
    RH_ScopedInstall(ClearAimFlag, 0x5DEF20);
    RH_ScopedOverloadedInstall(SetAimFlag, "Entity", 0x5DEED0, void(CPed::*)(CEntity *));
    RH_ScopedOverloadedInstall(SetAimFlag, "Heading", 0x5E8830, void(CPed::*)(float));
    RH_ScopedOverloadedInstall(SetLookFlag, "Entity", 0x5DEE40, void(CPed::*)(CEntity *, bool, bool));
    RH_ScopedOverloadedInstall(SetLookFlag, "Heading", 0x5DEDC0, void(CPed::*)(float, bool, bool));
    RH_ScopedInstall(CanUseTorsoWhenLooking, 0x5DED90);
    RH_ScopedInstall(PedIsReadyForConversation, 0x43ABA0);
    RH_ScopedInstall(CreateDeadPedMoney, 0x4590F0);
    RH_ScopedOverloadedInstall(CreateDeadPedPickupCoors, "", 0x459180, void(CPed::*)(float&, float&, float&));
    RH_ScopedInstall(CreateDeadPedWeaponPickups, 0x4591D0);
    RH_ScopedInstall(IsWearingGoggles, 0x479D10);
    RH_ScopedInstall(SetAmmo, 0x5DF290);
    RH_ScopedInstall(SetStayInSamePlace, 0x481090);
    RH_ScopedInstall(SetPedStats, 0x5DEBC0);
    RH_ScopedInstall(SetMoveState, 0x5DEC00);
    RH_ScopedInstall(SetMoveAnimSpeed, 0x5DEC10);
    RH_ScopedInstall(StopNonPartialAnims, 0x5DED10);
    RH_ScopedInstall(RestartNonPartialAnims, 0x5DED50);
    RH_ScopedInstall(DoWeHaveWeaponAvailable, 0x5DF300);
    RH_ScopedInstall(RemoveGogglesModel, 0x5DF170);
    RH_ScopedInstall(SetGunFlashAlpha, 0x5DF400);
    RH_ScopedInstall(CanSeeEntity, 0x5E0730);
    RH_ScopedInstall(SetPedDefaultDecisionMaker, 0x5E06E0);
    RH_ScopedInstall(GetWalkAnimSpeed, 0x5E04B0);
    RH_ScopedInstall(StopPlayingHandSignal, 0x5E0480);
    RH_ScopedInstall(IsPlayingHandSignal, 0x5E0460);
    RH_ScopedInstall(CanThrowEntityThatThisPedIsHolding, 0x5E0400);
    RH_ScopedInstall(DropEntityThatThisPedIsHolding, 0x5E0360);
    RH_ScopedInstall(GetEntityThatThisPedIsHolding, 0x5E02E0);
    RH_ScopedInstall(GetHoldingTask, 0x5E0290);
    RH_ScopedInstall(ReleaseCoverPoint, 0x5E0270);
    RH_ScopedInstall(DoGunFlash, 0x5DF340);
    RH_ScopedInstall(GetTransformedBonePosition, 0x5E01C0);
    RH_ScopedInstall(IsAlive, 0x5E0170);
    RH_ScopedInstall(DeadPedMakesTyresBloody, 0x6B4200);
    RH_ScopedInstall(Undress, 0x5E00F0);
    RH_ScopedInstall(SetLookTimer, 0x5DF8D0);
    RH_ScopedInstall(RestoreHeadingRate, 0x5DFD60);
    RH_ScopedInstall(Dress, 0x5E0130);
    RH_ScopedInstall(IsPlayer, 0x5DF8F0);
    RH_ScopedInstall(GetBikeRidingSkill, 0x5DF510);
    RH_ScopedInstall(SetPedPositionInCar, 0x5DF910);
    RH_ScopedInstall(SetRadioStation, 0x5DFD90);
    RH_ScopedInstall(PositionAttachedPed, 0x5DFDF0);
    RH_ScopedInstall(ResetGunFlashAlpha, 0x5DF4E0);

    RH_ScopedVMTInstall(SetModelIndex, 0x5E4880);
    RH_ScopedVMTInstall(DeleteRwObject, 0x5DEBF0);
    RH_ScopedVMTInstall(ProcessControl, 0x5E8CD0);
    RH_ScopedVMTInstall(Teleport, 0x5E4110);
    RH_ScopedVMTInstall(SpecialEntityPreCollisionStuff, 0x5E3C30);
    RH_ScopedVMTInstall(SpecialEntityCalcCollisionSteps, 0x5E3E90);
    RH_ScopedVMTInstall(PreRender, 0x5E8A20);
    RH_ScopedVMTInstall(Render, 0x5E7680);
    RH_ScopedVMTInstall(SetupLighting, 0x553F00);
    RH_ScopedVMTInstall(RemoveLighting, 0x5533B0);
    RH_ScopedVMTInstall(FlagToDestroyWhenNextProcessed, 0x5E7B70);
    RH_ScopedVMTInstall(ProcessEntityCollision, 0x5E2530);
    RH_ScopedVMTInstall(SetMoveAnim, 0x5E4A00);
    RH_ScopedVMTInstall(Save, 0x5D5730);
    RH_ScopedVMTInstall(Load, 0x5D4640);

    RH_ScopedGlobalInstall(SetPedAtomicVisibilityCB, 0x5F0060);
}

// 0x5E8030
CPed::CPed(ePedType pedType) : CPhysical(), m_pedIK{CPedIK(this)} {
    m_vecAnimMovingShiftLocal = CVector2D();

    m_fHealth = 100.0f;
    m_fMaxHealth = 100.0f;
    m_fArmour = 0.0f;

    m_nPedType = pedType;
    SetTypePed();

    // 0x5E8196
    physicalFlags.bCanBeCollidedWith = true;
    physicalFlags.bDisableTurnForce = true;

    SetCreatedBy(PED_GAME);

    m_pVehicle = nullptr;
    m_nAntiSpazTimer = 0;
    m_nUnconsciousTimer = 0;
    m_nAttackTimer = 0;
    m_nLookTime = 0;
    m_nDeathTimeMS = 0;

    m_vecAnimMovingShift = CVector2D();
    field_56C = CVector();
    field_578 = CVector(0.0f, 0.0f, 1.0f);

    m_nPedState = PEDSTATE_IDLE;
    m_nMoveState = PEDMOVE_STILL;
    m_fCurrentRotation = 0.0f;
    m_fHeadingChangeRate = 15.0f;
    m_fMoveAnim = 0.1f;
    m_fAimingRotation = 0.0f;
    m_standingOnEntity = nullptr;
    m_nWeaponShootingRate = 40;
    field_594 = 0;
    m_pEntityIgnoredCollision = nullptr;
    m_nSwimmingMoveState = 0;
    m_pFire = nullptr;
    m_fireDmgMult = 1.0f;
    m_pTargetedObject = nullptr;
    m_pLookTarget = nullptr;
    m_fLookDirection = 0.0f;
    m_pContactEntity = nullptr;
    field_588 = 99999.992f;
    m_fMass = 70.0f;
    m_fTurnMass = 100.0f;
    m_fAirResistance = 1.f / 175.f;
    m_fElasticity = 0.05f;
    m_nBodypartToRemove = -1;
    bHasACamera = CGeneral::GetRandomNumber() % 4 != 0;

    m_weaponAudio.Initialise(this);
    m_pedAudio.Initialise(this);

    m_acquaintance = CPedType::GetPedTypeAcquaintances(m_nPedType);
    m_nSavedWeapon = WEAPON_UNIDENTIFIED;
    m_nDelayedWeapon = WEAPON_UNIDENTIFIED;
    m_nActiveWeaponSlot = 0;

    for (auto& weapon : m_aWeapons ) {
        weapon.m_Type = WEAPON_UNARMED;
        weapon.m_State = WEAPONSTATE_READY;
        weapon.m_AmmoInClip = 0;
        weapon.m_TotalAmmo = 0;
        weapon.m_TimeForNextShotMs = 0;
    }

    m_nWeaponSkill = eWeaponSkill::STD;
    m_nFightingStyle = STYLE_STANDARD;
    m_nAllowedAttackMoves = 0;

    GiveWeapon(WEAPON_UNARMED, 0, true);

    m_nWeaponAccuracy = 60;
    m_nLastWeaponDamage = -1;
    m_pLastEntityDamage = nullptr;
    field_768 = 0;
    m_pAttachedTo = nullptr;
    m_nTurretAmmo = 0;
    m_roadRageWith = nullptr;
    field_468 = 0;
    m_nWeaponModelId = -1;
    m_nMoneyCount = 0;
    field_72F = 0;
    m_nTimeTillWeNeedThisPed = 0;
    m_VehDeadInFrontOf = nullptr;

    m_pWeaponObject = nullptr;
    m_pGunflashObject = nullptr;
    m_pGogglesObject = nullptr;
    m_pGogglesState = nullptr;

    m_nWeaponGunflashAlphaMP1 = 0;
    m_nWeaponGunFlashAlphaProgMP1 = 0;
    m_nWeaponGunflashAlphaMP2 = 0;
    m_nWeaponGunFlashAlphaProgMP2 = 0;

    m_pCoverPoint = nullptr;
    m_pEnex = nullptr;
    field_798 = -1;

    m_pIntelligence = new CPedIntelligence(this);
    m_pPlayerData = nullptr;

    if (!IsPlayer()) {
        GetTaskManager().SetTaskSecondary(new CTaskComplexFacial{}, TASK_SECONDARY_FACIAL_COMPLEX);
    }
    GetTaskManager().SetTask(new CTaskSimpleStandStill{ 0, true, false, 8.0 }, TASK_PRIMARY_DEFAULT, false);

    m_Wobble = 0.0f;
    m_fRemovalDistMultiplier = 1.0f;
    m_StreamedScriptBrainToLoad = -1;

    CPopulation::UpdatePedCount(this, 0);

    if (CCheat::IsActive(CHEAT_HAVE_ABOUNTY_ON_YOUR_HEAD)) {
        if (!IsPlayer()) {
            GetAcquaintance().SetAsAcquaintance(ACQUAINTANCE_HATE, CPedType::GetPedFlag(ePedType::PED_TYPE_PLAYER1));
            GetEventGroup().Add(CEventAcquaintancePedHate{FindPlayerPed()});
        }
    }
}

/*!
* @addr 0x5E8620
 */
CPed::~CPed() {
    CReplay::RecordPedDeleted(this);

    // Remove script brain
    if (bWaitingForScriptBrainToLoad) {
        CStreaming::SetMissionDoesntRequireModel(SCMToModelId(CTheScripts::ScriptsForBrains.m_aScriptForBrains[m_StreamedScriptBrainToLoad].m_StreamedScriptIndex));
        bWaitingForScriptBrainToLoad = false;
        CTheScripts::RemoveFromWaitingForScriptBrainArray(this, m_StreamedScriptBrainToLoad);
        m_StreamedScriptBrainToLoad = -1;
    }

    CWorld::Remove(this);
    CRadar::ClearBlipForEntity(BLIP_CHAR, GetPedPool()->GetRef(this));
    CConversations::RemoveConversationForPed(this);

    ClearReference(m_pVehicle);

    if (m_pFire) {
        m_pFire->Extinguish();
    }

    ReleaseCoverPoint();
    ClearWeapons();

    if (bMiamiViceCop) {
        CPopulation::NumMiamiViceCops--;
    }

    CPopulation::UpdatePedCount(this, 1);

    m_pedSpeech.Terminate();
    m_weaponAudio.Terminate();
    m_pedAudio.Terminate();

    delete m_pIntelligence;

    ClearReference(m_pLookTarget);
}

/*!
* @addr 0x5E4720
*/
void* CPed::operator new(unsigned size) {
    return GetPedPool()->New();
}

/*!
* @addr 0x5E4730
*/
void* CPed::operator new(unsigned size, int32 poolRef) {
    return GetPedPool()->NewAt(poolRef);
}

/*!
* @addr 0x5E4760
*/
void CPed::operator delete(void* data) {
    GetPedPool()->Delete((CPed*)data);
}

// NOTSA
void CPed::operator delete(void* data, int poolRef) {
    GetPedPool()->Delete((CPed*)data);
}

/*!
* @addr 0x5E4A00
*/
void CPed::SetMoveAnim() {
    if (!IsAlive() || bIsDucking || m_pAttachedTo) {
        return;
    }

    const auto DoUpdateMoveAnim = [this](auto* assoc) {
        if (!bMoveAnimSpeedHasBeenSetByTask) {
            SetMoveAnimSpeed(assoc);
        }
    };

    if (m_nSwimmingMoveState == m_nMoveState) {
        switch (m_nMoveState) {
        case PEDMOVE_WALK:
        case PEDMOVE_JOG:
        case PEDMOVE_RUN:
        case PEDMOVE_SPRINT: {
            const auto GetAnimId = [this] {
                switch (m_nMoveState) {
                case PEDMOVE_RUN:
                    return ANIM_ID_RUN;
                case PEDMOVE_SPRINT:
                    return ANIM_ID_SPRINT;
                }
                return ANIM_ID_WALK;
            };

            if (const auto assoc = RpAnimBlendClumpGetAssociation(GetRpClump(), GetAnimId())) {
                DoUpdateMoveAnim(assoc);
            }

            break;
        }
        }
    } else if (m_nMoveState != PEDMOVE_NONE) {
        m_nSwimmingMoveState = m_nMoveState;

        switch (m_nMoveState) { // TODO: What's happening here?
        case PEDMOVE_WALK:
        case PEDMOVE_RUN:
        case PEDMOVE_SPRINT: {
            for (auto assoc = RpAnimBlendClumpGetFirstAssociation(GetRpClump(), ANIMATION_IS_PARTIAL); assoc; assoc = RpAnimBlendGetNextAssociation(assoc, ANIMATION_IS_PARTIAL)) {
                if ((assoc->m_Flags & ANIMATION_IS_FINISH_AUTO_REMOVE) == 0 && (assoc->m_Flags & ANIMATION_DONT_ADD_TO_PARTIAL_BLEND) == 0) {
                    assoc->m_BlendDelta = -2.f;
                    assoc->SetFlag(ANIMATION_IS_BLEND_AUTO_REMOVE, true);
                }
            }

            ClearAimFlag();
            ClearLookFlag();

            break;
        }
        }

        // Do BlendAnimation and call `DoUpdateMoveAnim` afterwards
        const auto DoBlendAnim = [&, this](AssocGroupId grp, AnimationId animId, float blendDelta) {
            if (const auto assoc = CAnimManager::BlendAnimation(GetRpClump(), grp, animId, blendDelta)) {
                DoUpdateMoveAnim(assoc);
            }
        };

        switch (m_nMoveState) {
        case PEDMOVE_STILL:
            DoBlendAnim(m_nAnimGroup, ANIM_ID_IDLE, 4.f);
            return;

        case PEDMOVE_TURN_L:
            DoBlendAnim(ANIM_GROUP_DEFAULT, ANIM_ID_TURN_L, 16.f);
            return;

        case PEDMOVE_TURN_R:
            DoBlendAnim(ANIM_GROUP_DEFAULT, ANIM_ID_TURN_R, 16.f);
            return;

        case PEDMOVE_WALK:
            DoBlendAnim(m_nAnimGroup, ANIM_ID_WALK, 1.f);
            return;

        case PEDMOVE_RUN:
            DoBlendAnim(m_nAnimGroup, ANIM_ID_RUN, m_nPedState == PEDSTATE_FLEE_ENTITY ? 3.f : 1.f);
            return;

        case PEDMOVE_SPRINT: {
            // If we're in a group, and our leader is sprinting as well sprinting should be played with a different anim group
            if (CPedGroups::IsInPlayersGroup(this)) {
                if (const auto leader = CPedGroups::GetPedsGroup(this)->GetMembership().GetLeader()) {
                    switch (leader->m_nMoveState) {
                    case PEDMOVE_RUN:
                    case PEDMOVE_SPRINT: {
                        DoBlendAnim(ANIM_GROUP_PLAYER, ANIM_ID_SPRINT, 1.f);
                        return;
                    }
                    }
                }
            }
            DoBlendAnim(m_nAnimGroup, ANIM_ID_SPRINT, 1.f);
            return;
        }
        }
    } else {
        m_nSwimmingMoveState = PEDMOVE_NONE;
    }
}

/*!
* @addr 0x5D4640
 */
bool CPed::Load() {
    auto size = CGenericGameStorage::LoadDataFromWorkBuffer<uint32>();

    // TODO: Can't do `auto save = CGenericGameStorage::LoadDataFromWorkBuffer<CPedSaveStructure>()` dure to deleted copy constructor in CWanted which is used somehow inside.
    // Would be nice if someone with more knowledge of templates and shit can fix that
    CPedSaveStructure save;
    CGenericGameStorage::LoadDataFromWorkBuffer(save);
    assert(size == sizeof(save));

    save.Extract(this);
    return true;
}

/*!
* @addr 0x5D5730
*/
bool CPed::Save() {
    CPedSaveStructure save;
    save.Construct(this);

    CGenericGameStorage::SaveDataToWorkBuffer(sizeof(save));
    CGenericGameStorage::SaveDataToWorkBuffer(save);

    return true;
}

/*!
* @addr 0x43AB90
*/
bool CPed::PedIsInvolvedInConversation() {
    return this == CPedToPlayerConversations::m_pPed;
}

/*!
* @addr 0x43ABA0
*/
bool CPed::PedIsReadyForConversation(bool checkLocalPlayerWantedLevel) {
    // We don't talk when we're behind the wheel! (Nor when we're fighting...)
    if (bInVehicle || GetIntelligence()->GetTaskFighting()) {
        return false;
    }

    if (checkLocalPlayerWantedLevel && FindPlayerPed()->GetWanted()->GetWantedLevel() != eWantedLevel::WANTED_CLEAN) {
        return false;
    }

    // If we're doing any of these we don't have the mental power to chat...
    switch (m_nMoveState) {
    case PEDMOVE_JOG:
    case PEDMOVE_RUN:
    case PEDMOVE_SPRINT:
        return false;
    }

    if (!IsCreatedByMission()) { // Don't check if we've a chatting task/event if we're a mission ped
        if (GetIntelligence()->FindTaskByType(TASK_COMPLEX_PARTNER_CHAT)) {
            return false;
        }

        if (GetEventGroup().GetEventOfType(eEventType::EVENT_CHAT_PARTNER)) {
            return false;
        }
    }

    return true;
}

/*!
* @addr 0x455560
*/
bool CPed::PedCanPickUpPickUp() {
    return !FindPlayerPed(0)->GetTaskManager().FindActiveTaskFromList({ TASK_COMPLEX_ENTER_CAR_AS_DRIVER, TASK_COMPLEX_USE_MOBILE_PHONE });
}

/*!
* @addr 0x4590F0
*/
void CPed::CreateDeadPedMoney() {
    if (!CLocalisation::StealFromDeadPed()) {
        return;
    }

    switch (m_nPedType) {
    case PED_TYPE_COP:
    case PED_TYPE_MEDIC:
    case PED_TYPE_FIREMAN:
        return;
    }

    if (IsCreatedByMission() && !bMoneyHasBeenGivenByScript) {
        return;
    }

    if (bInVehicle) {
        return;
    }

    if (m_nMoneyCount > 10) {
        CPickups::CreateSomeMoney(GetPosition(), m_nMoneyCount);
        m_nMoneyCount = 0;
    }
}

/*!
* @addr 0x459180
* @brief Created a pickup close to the ped's position (Using CPickups::CreatePickupCoorsCloseToCoors)
* @param [out] outPickupX, outPickupY, outPickupZ Position of the created pickup.
*/
void CPed::CreateDeadPedPickupCoors(float& outPickupX, float& outPickupY, float& outPickupZ) {
    CPickups::CreatePickupCoorsCloseToCoors(GetPosition(), outPickupX, outPickupY, outPickupZ);
}

/*!
* @notsa
* @copybrief CPed::CreateDeadPedPickupCoors
* @param [out] pickupPos Position of the created pickup.
*/
void CPed::CreateDeadPedPickupCoors(CVector& pickupPos) {
    return CreateDeadPedPickupCoors(pickupPos.x, pickupPos.y, pickupPos.z);
}

/*!
* @notsa
*/
RpHAnimHierarchy& CPed::GetAnimHierarchy() const {
    return *GetAnimHierarchyFromSkinClump(GetRpClump());
}

CAnimBlendClumpData& CPed::GetAnimBlendData() const {
    return *RpAnimBlendClumpGetData(GetRpClump());
}

/*!
* @addr 0x4591D0
* @brief Create weapon/ammo pickups for dead ped
*/
void CPed::CreateDeadPedWeaponPickups() {
    if (bInVehicle || bDoesntDropWeaponsWhenDead) {
        return;
    }

    for (auto& wep : m_aWeapons) {
        switch (wep.m_Type) {
        case WEAPON_UNARMED:
        case WEAPON_DETONATOR:
            continue;
        }

        if (!wep.m_TotalAmmo && !wep.IsTypeMelee()) {
            continue; // Has no ammo, but isn't a melee weapon.. so it's a weapon with no ammo :D
        }

        // Now, create a pickup at close to our position
        CVector pickupPos{};
        CreateDeadPedPickupCoors(pickupPos);
        pickupPos.z += 0.3f;

        // No. of ammo the pickups will contain
        const auto pickupAmmo{ std::min(wep.m_TotalAmmo, (uint32)AmmoForWeapon_OnStreet[(size_t)wep.m_Type] * 2) };

        if (!CPickups::TryToMerge_WeaponType(
            pickupPos,
            wep.m_Type,
            ePickupType::PICKUP_ONCE_TIMEOUT,
            pickupAmmo,
            false
        )) {
            CPickups::GenerateNewOne_WeaponType(
                pickupPos,
                wep.m_Type,
                bDeathPickupsPersist ? ePickupType::PICKUP_ONCE_FOR_MISSION : ePickupType::PICKUP_ONCE_TIMEOUT,
                pickupAmmo,
                false,
                nullptr
            );
        }
    }
}

/*!
* @addr 0x5DEBB0
*/
void CPed::Initialise() {
    CPedType::Initialise();
    CCarEnterExit::SetAnimOffsetForEnterOrExitVehicle();
}

/*!
* @addr 0x5DEBC0
* @unused
*/
void CPed::SetPedStats(ePedStats statsType) {
    m_pStats = &CPedStats::ms_apPedStats[(size_t)statsType];
}

/*!
* @addr 0x5DEBE0
*/
void CPed::Update()
{
    // NOP
}

/*!
* @addr 0x5DEC00
*/
void CPed::SetMoveState(eMoveState moveState) {
    m_nMoveState = moveState;
}

/*!
* @addr 0x5DEC10
*/
void CPed::SetMoveAnimSpeed(CAnimBlendAssociation* association) {
    const auto pitchFactor = std::clamp(m_pedIK.m_fSlopePitch, -0.3f, 0.3f);
    if (IsCreatedByMission()) {
        association->m_Speed = pitchFactor + 1.f;
    } else {
        association->m_Speed = pitchFactor + 1.2f - (float)m_nRandomSeed * RAND_MAX_FLOAT_RECIPROCAL * 0.4f; // todo: use GetRandom from CGeneral::
    }
}

/*!
* @addr 0x5DED10
*/
void CPed::StopNonPartialAnims() {
    RpAnimBlendClumpForEachAssociation(GetRpClump(), [](CAnimBlendAssociation* a) {
        if (!(a->m_Flags & ANIMATION_IS_PARTIAL)) {
            a->SetFlag(ANIMATION_IS_PLAYING, false);
        }
    });
}

/*!
* @addr 0x5DED50
*/
void CPed::RestartNonPartialAnims() {
    RpAnimBlendClumpForEachAssociation(GetRpClump(), [](CAnimBlendAssociation* a) {
        if (!(a->m_Flags & ANIMATION_IS_PARTIAL)) {
            a->SetFlag(ANIMATION_IS_PLAYING, true);
        }
    });
}

/*!
* @addr 0x5DED90
*/
bool CPed::CanUseTorsoWhenLooking() const {
    switch (m_nPedState) {
    case PEDSTATE_DRIVING:
    case PEDSTATE_DRAGGED_FROM_CAR:
        return false;
    }

    if (bIsDucking) {
        return false;
    }

    return true;
}

/*!
* @addr 0x5DEDC0
*/
void CPed::SetLookFlag(float lookHeading, bool unused, bool ignoreLookTime) {
    UNUSED(unused);

    if (m_nLookTime >= CTimer::GetTimeInMS() && !ignoreLookTime) {
        return;
    }

    bIsLooking = true;
    m_fLookDirection = lookHeading;
    m_nLookTime = 0;

    ClearReference(m_pLookTarget);

    if (CanUseTorsoWhenLooking()) {
        m_pedIK.bTorsoUsed = false;
    }
}

/*!
* @addr 0x5DEE40
* @brief Start looking at entity \a lookingTo
*/
void CPed::SetLookFlag(CEntity* lookingTo, bool unused, bool ignoreLookTime) {
    UNUSED(unused);

    if (m_nLookTime >= CTimer::GetTimeInMS() && !ignoreLookTime) {
        return;
    }

    bIsRestoringLook = false;
    bIsLooking = true;

    ChangeEntityReference(m_pLookTarget, lookingTo);

    m_fLookDirection = 999'999.f;
    m_nLookTime = 0;

    if (CanUseTorsoWhenLooking()) {
        m_pedIK.bTorsoUsed = false;
    }
}

/*!
* @addr 0x5DEED0
*/
void CPed::SetAimFlag(CEntity* aimingTo) {
    bIsAimingGun = true;
    bIsRestoringGun = false;
    ChangeEntityReference(m_pLookTarget, aimingTo);
    m_nLookTime = 0;
}

/*!
* @addr 0x5DEF20
* @brief Clear gun aiming flag
*/
void CPed::ClearAimFlag() {
    if (bIsAimingGun) {
        bIsAimingGun = false;
        bIsRestoringGun = true;
        m_pedIK.bUseArm = false;
        m_nLookTime = 0;
    }

    if (GetPlayerData()) {
        GetPlayerData()->m_fLookPitch = 0.f;
    }
}

/*!
* @addr 0x5DEF60
* @returns Which quadrant a given point is in relative to the ped's rotation. (Google: "Angle quadrants" - https://www.mathstips.com/wp-content/uploads/2014/03/unit-circle.png)
* @param point Point should be relative to the ped's position. Eg.: point = actualPoint - ped.GetPostion2D()
*/
int32 CPed::GetLocalDirection(const CVector2D& point) const {
    // Exact x87 form of the exe (0x5DEF60): everything stays in extended precision (doubles), the angle is truncated by _ftol2 after `* (2/PI)` (0x858FB8)
    // and then reduced with `(dir - 4) & 3` for dir > 3 (== dir & 3).
    double angle = std::atan2(-(double)point.x, (double)point.y) - (double)m_fCurrentRotation + (double)std::bit_cast<float>(0x3F490FDBu); // 0x859AB0 (PI / 4)
    while (angle < 0.0) {
        angle += (double)TWO_PI; // 0x858CBC
    }
    int32 dir = notsa::detail::Ftol(angle * (double)std::bit_cast<float>(0x3F22F983u)); // 0x858FB8 (2 / PI)
    if (dir > 3) {
        dir = dir - 4 - (int32)(((uint32)(dir - 4) >> 2) << 2);
    }
    return dir;
}

/*!
* @addr 0x5DEFD0
*/
bool CPed::IsPedShootable() const {
    // Not sure if they used a switch case or `<= PEDSTATE_STATES_CAN_SHOOT` originally, but I'll use a switch case.
    switch (m_nPedState) {
    case PEDSTATE_NONE:
    case PEDSTATE_IDLE:
    case PEDSTATE_LOOK_ENTITY:
    case PEDSTATE_LOOK_HEADING:
    case PEDSTATE_WANDER_RANGE:
    case PEDSTATE_WANDER_PATH:
    case PEDSTATE_SEEK_POSITION:
    case PEDSTATE_SEEK_ENTITY:
    case PEDSTATE_FLEE_POSITION:
    case PEDSTATE_FLEE_ENTITY:
    case PEDSTATE_PURSUE:
    case PEDSTATE_FOLLOW_PATH:
    case PEDSTATE_SNIPER_MODE:
    case PEDSTATE_ROCKETLAUNCHER_MODE:
    case PEDSTATE_DUMMY:
    case PEDSTATE_PAUSE:
    case PEDSTATE_ATTACK:
    case PEDSTATE_FIGHT:
    case PEDSTATE_FACE_PHONE:
    case PEDSTATE_MAKE_PHONECALL:
    case PEDSTATE_CHAT:
    case PEDSTATE_MUG:
    case PEDSTATE_AIMGUN:
    case PEDSTATE_AI_CONTROL:
    case PEDSTATE_SEEK_CAR:
    case PEDSTATE_SEEK_BOAT_POSITION:
    case PEDSTATE_FOLLOW_ROUTE:
    case PEDSTATE_CPR:
    case PEDSTATE_SOLICIT:
    case PEDSTATE_BUY_ICE_CREAM:
    case PEDSTATE_INVESTIGATE_EVENT:
    case PEDSTATE_EVADE_STEP:
    case PEDSTATE_ON_FIRE:
    case PEDSTATE_SUNBATHE:
    case PEDSTATE_FLASH:
    case PEDSTATE_JOG:
    case PEDSTATE_ANSWER_MOBILE:
    case PEDSTATE_HANG_OUT:
    case PEDSTATE_STATES_NO_AI:
    case PEDSTATE_ABSEIL_FROM_HELI:
    case PEDSTATE_SIT:
    case PEDSTATE_JUMP:
    case PEDSTATE_FALL:
    case PEDSTATE_GETUP:
    case PEDSTATE_STAGGER:
    case PEDSTATE_EVADE_DIVE:
    case PEDSTATE_STATES_CAN_SHOOT:
        return true;
    }
    return false;
}

/*!
* @addr 0x5DEFE0
*/
bool CPed::UseGroundColModel() const {
    switch (m_nPedState) {
    case PEDSTATE_FALL:
    case PEDSTATE_EVADE_DIVE:
    case PEDSTATE_DIE:
    case PEDSTATE_DEAD:
        return true;
    }
    return false;
}

/*!
* @addr 0x5DF000
*/
bool CPed::CanPedReturnToState() const {
    switch (m_nPedState) {
    case PEDSTATE_NONE:
    case PEDSTATE_IDLE:
    case PEDSTATE_LOOK_HEADING:
    case PEDSTATE_WANDER_RANGE:
    case PEDSTATE_WANDER_PATH:
    case PEDSTATE_SEEK_POSITION:
    case PEDSTATE_SEEK_ENTITY:
    case PEDSTATE_FLEE_POSITION:
    case PEDSTATE_FLEE_ENTITY:
    case PEDSTATE_PURSUE:
    case PEDSTATE_FOLLOW_PATH:
    case PEDSTATE_ROCKETLAUNCHER_MODE:
    case PEDSTATE_DUMMY:
    case PEDSTATE_PAUSE:
    case PEDSTATE_FACE_PHONE:
    case PEDSTATE_MAKE_PHONECALL:
    case PEDSTATE_CHAT:
    case PEDSTATE_MUG:
    case PEDSTATE_AI_CONTROL:
    case PEDSTATE_SEEK_CAR:
    case PEDSTATE_SEEK_BOAT_POSITION:
    case PEDSTATE_FOLLOW_ROUTE:
    case PEDSTATE_CPR:
    case PEDSTATE_SOLICIT:
    case PEDSTATE_BUY_ICE_CREAM:
    case PEDSTATE_INVESTIGATE_EVENT:
    case PEDSTATE_ON_FIRE:
    case PEDSTATE_SUNBATHE:
    case PEDSTATE_FLASH:
    case PEDSTATE_JOG:
    case PEDSTATE_ANSWER_MOBILE:
    case PEDSTATE_HANG_OUT:
    case PEDSTATE_STATES_NO_AI:
        return true;
    }
    return false;
}

/*!
* @addr 0x5DF030
*/
bool CPed::CanSetPedState() const {
    switch (m_nPedState) {
    case PEDSTATE_DIE:
    case PEDSTATE_DEAD:
    case PEDSTATE_ARRESTED:
    case PEDSTATE_ENTER_CAR:
    case PEDSTATE_CARJACK:
    case PEDSTATE_STEAL_CAR:
        return false;
    }
    return true;
}

/*!
* @addr 0x5DF060
*/
bool CPed::CanBeArrested() const {
    switch (m_nPedState) {
    case PEDSTATE_DIE:
    case PEDSTATE_DEAD:
    case PEDSTATE_ARRESTED:
    case PEDSTATE_ENTER_CAR:
    case PEDSTATE_EXIT_CAR:
        return false;
    }
    return true;
}

/*!
* @addr 5DF090
*/
bool CPed::CanStrafeOrMouseControl() const {
    switch (m_nPedState) {
    case PEDSTATE_IDLE:
    case PEDSTATE_FLEE_ENTITY:
    case PEDSTATE_FLEE_POSITION:
    case PEDSTATE_NONE:
    case PEDSTATE_AIMGUN:
    case PEDSTATE_ATTACK:
    case PEDSTATE_FIGHT:
    case PEDSTATE_JUMP:
    case PEDSTATE_ANSWER_MOBILE:
        return true;
    }
    return false;
}

/*!
* @addr 0x5DF100
* @brief Check if ped can be deleted
* @returns Always false if ped is in vehicle or is follower of player's group.
*/
bool CPed::CanBeDeleted() {
    return !bInVehicle && !IsFollowerOfGroup(FindPlayerGroup()) && CanBeDeletedEvenInVehicle();
}

/*!
* @addr 0x5DF150
* @brief Check if ped can be deleted even if it's in a vehicle.
* @returns False only if created by PED_UNKNOWN or PED_MISSION, true otherwise.
*/
bool CPed::CanBeDeletedEvenInVehicle() const {
    switch (GetCreatedBy()) {
    case ePedCreatedBy::PED_MISSION:
    case ePedCreatedBy::PED_GAME_MISSION:
        return false;
    }
    return true;
}

/*!
* @addr 0x5DF170
* @brief Remove goggles model, also disabled related PostFX.
*/
void CPed::RemoveGogglesModel() {
    if (!m_pGogglesObject) {
        return;
    }

    // Release model info
    CVisibilityPlugins::GetClumpModelInfo(m_pGogglesObject)->RemoveRef();

#ifdef SA_SKINNED_PEDS
    // Remove skin anim
    if (IsClumpSkinned(m_pGogglesObject)) {
        RpClumpForAllAtomics(m_pGogglesObject, AtomicRemoveAnimFromSkinCB, nullptr);
    }
#endif

    // Destroy clump
    RpClumpDestroy(m_pGogglesObject);
    m_pGogglesObject = nullptr;

    // Disable FX's of the goggles. (See mem. var. `m_pGogglesState` in the header)
    if (m_pGogglesState) {
        *m_pGogglesState = false;
        m_pGogglesState = nullptr;
    }
}

/*!
* @addr   0x5DF200
* @return \a weaponType weapon's slot (CWeaponInfo::GetWeaponInfo()->slot)
*/
int32 CPed::GetWeaponSlot(eWeaponType weaponType)
{
    return CWeaponInfo::GetWeaponInfo(weaponType)->m_nSlot;
}

/*!
* @addr 0x5DF220
* @brief Set \a weaponType's slot totalAmmo to \a ammo. Also changes the gun's state to `READY`
*/
void CPed::GrantAmmo(eWeaponType weaponType, uint32 ammo) {
    const auto wepSlot = GetWeaponSlot(weaponType);
    if (wepSlot != -1) {
        auto& wepInSlot = GetWeaponInSlot(wepSlot);

        wepInSlot.m_TotalAmmo = std::min(wepInSlot.m_TotalAmmo + ammo, 99'999u); // Clamp upper

        // TODO: Inlined
        if (wepInSlot.m_State == WEAPONSTATE_OUT_OF_AMMO) {
            if (wepInSlot.m_TotalAmmo > 0) {
                wepInSlot.m_State = WEAPONSTATE_READY;
            }
        }
    }
}

/*!
* @addr 0x5DF290
* @brief Im lazy to write it :D Similar to CPed::GrantAmmo
*/
void CPed::SetAmmo(eWeaponType weaponType, uint32 ammo) {
    const auto wepSlot = GetWeaponSlot(weaponType);
    if (wepSlot != -1) {
        auto& wepInSlot = GetWeaponInSlot(wepSlot);

        wepInSlot.m_TotalAmmo = std::min(ammo, 99'999u);
        wepInSlot.m_AmmoInClip = std::max(wepInSlot.m_TotalAmmo, wepInSlot.m_AmmoInClip);

        // TODO: Inlined
        if (wepInSlot.m_State == WEAPONSTATE_OUT_OF_AMMO) {
            if (wepInSlot.m_TotalAmmo > 0) {
                wepInSlot.m_State = WEAPONSTATE_READY;
            }
        }
    }
}

/*!
* @addr 0x5DF300
* @brief Check if ped has a weapon of type \a weaponType
*/
bool CPed::DoWeHaveWeaponAvailable(eWeaponType weaponType) {
    const auto slot = GetWeaponSlot(weaponType);
    return slot != -1 && GetWeaponInSlot(slot).m_Type == weaponType;
}

/*!
* @addr 0x5DF340
* @brief Do gun flash by resetting it's alpha to max
*/
bool CPed::DoGunFlash(int32 duration, bool isLeftHand) {
    if (!m_pGunflashObject || !m_pWeaponObject) {
        return false;
    }

    // Really elegant.. ;D
    if (isLeftHand) {
        m_nWeaponGunflashAlphaMP2     = m_sGunFlashBlendStart;
        m_nWeaponGunFlashAlphaProgMP2 = (uint16)m_sGunFlashBlendStart / duration;
    } else {
        m_nWeaponGunflashAlphaMP1     = m_sGunFlashBlendStart;
        m_nWeaponGunFlashAlphaProgMP1 = (uint16)m_sGunFlashBlendStart / duration;
    }

    const auto angle = CGeneral::GetRandomNumberInRange(-360.0f, 360.0f);
    RwMatrixRotate(RwFrameGetMatrix(m_pGunflashObject), &CPedIK::XaxisIK, angle, rwCOMBINEPRECONCAT);

    return true;
}

/*!
* @addr 0x5DF400
* @brief Set alpha of gun flash object
*/
void CPed::SetGunFlashAlpha(bool rightHand) {
    if (!m_pGunflashObject) {
        return;
    }

    if (m_nWeaponGunflashAlphaMP1 < 0 && m_nWeaponGunflashAlphaMP2 < 0) { // Reordered a little.
        return;
    }

    auto& gunFlashAlphaInHand = rightHand ? m_nWeaponGunflashAlphaMP2 : m_nWeaponGunflashAlphaMP1;

    if (auto atomic = (RpAtomic*)GetFirstObject(m_pGunflashObject)) {
        // They used a clever trick to not have to convert to float..
        // Then they converted to a float to check if the number is higher than 255.. XDDD
        if (gunFlashAlphaInHand <= 0) {
            CVehicle::SetComponentAtomicAlpha(atomic, 0);
        } else {
            CVehicle::SetComponentAtomicAlpha(atomic, std::min(255, 350 * gunFlashAlphaInHand / m_sGunFlashBlendStart));
        }
        RpAtomicSetFlags(atomic, rpATOMICRENDER);
    }

    if (!gunFlashAlphaInHand) {
        gunFlashAlphaInHand = (uint16)-1;
    }
}

/*!
* @addr 0x5DF4E0
* @brief Reset alpha of gun flash object
*/
void CPed::ResetGunFlashAlpha() {
    if (m_pGunflashObject) {
        if (auto atomic = (RpAtomic*)GetFirstObject(m_pGunflashObject)) {
            RpAtomicSetFlags(atomic, 0);
            CVehicle::SetComponentAtomicAlpha(atomic, 0);
        }
    }
}

/*!
* @addr 0x5DF510
* @returns If ped is a player returns stat value BIKE_SKILL, otherwise 1 for mission peds and 0 for all others.
*/
float CPed::GetBikeRidingSkill() const {
    if (GetPlayerData()) {
        return std::min(1000.f, CStats::GetStatValue(eStats::STAT_BIKE_SKILL) / 1000.f);
    }
    return IsCreatedByMission() ? 1.f : 0.f;
}

/*!
* @addr 0x5DF560
* @brief Deal with shoulder bone (clavicle) rotation based on arm and breast rotation
*/
void CPed::ShoulderBoneRotation(RpClump* clump) {
    const auto GetMatrixOf = [hier = GetAnimHierarchyFromClump(clump)](eBoneTag bone) {
        return RpHAnimHierarchyGetNodeMatrix(hier, bone);
    };
    const auto DoUpdate = [&](eBoneTag breast, eBoneTag upperArm, eBoneTag clavicle) {
        auto* const breastRwMat = GetMatrixOf(breast);

        // Make the breast's matrix same as the upper arm's
        RwMatrixCopy(breastRwMat, GetMatrixOf(upperArm));

        CMatrix breastMat{ breastRwMat };
        CMatrix clavicleMat{ GetMatrixOf(clavicle) };

        // Calculate breast to clavicle transformation matrix (and store it in breastMat)
        breastMat = Invert(clavicleMat) * breastMat;

        // Half it's X rotation
        float rx, ry, rz;
        breastMat.ConvertToEulerAngles(&rx, &ry, &rz, ORDER_ZYX | SWAP_XZ);
        // Originally there is an `if` check of a static bool value, which is always true.
        rx /= 2.f;
        breastMat.ConvertFromEulerAngles(rx, ry, rz, ORDER_ZYX | SWAP_XZ);

        // Transform it back into it's own space
        breastMat = clavicleMat * breastMat;

        // Finally, update it's RW associated matrix
        breastMat.UpdateRW();
    };
    DoUpdate(eBoneTag::BONE_L_BREAST, eBoneTag::BONE_L_UPPER_ARM, eBoneTag::BONE_L_CLAVICLE);
    DoUpdate(eBoneTag::BONE_R_BREAST, eBoneTag::BONE_R_UPPER_ARM, eBoneTag::BONE_R_CLAVICLE);
}

/*!
* @addr 0x5DF8D0
* @brief Set look timer relative to now, but only if it has expired.
* @param time Time the timer ends relative to now
*/
void CPed::SetLookTimer(uint32 time) {
    if (CTimer::GetTimeInMS() > m_nLookTime) {
        m_nLookTime = CTimer::GetTimeInMS() + time;
    }
}

/*!
* @addr 0x5DF8F0
*/
bool CPed::IsPlayer() const
{
    switch (m_nPedType) {
    case PED_TYPE_PLAYER1:
    case PED_TYPE_PLAYER2:
        return true;
    }
    return false;
}

/*!
* @addr 0x5DF910
*/
void CPed::SetPedPositionInCar() {
    if (CReplay::Mode == MODE_PLAYBACK) {
        return;
    }

    assert(IsInVehicle());

    CVector seatLocalPos = GetSeatPositionInVehicle();
    CMatrix vehMat = m_pVehicle->GetMatrix();
    CMatrix tempMat;
    const float heading = m_pVehicle->GetHeading();

    if (m_pVehicle->IsBike()) {
        auto* bike = (CBike*)m_pVehicle;
        bike->CalculateLeanMatrix();
        vehMat = bike->m_mLeanMatrix;
    } else if (m_pVehicle->GetModelIndex() == MODEL_COMBINE) { // 532 ?
        float chassisPosZ = 0.0f;
        auto  autoMobile  = (CAutomobile*)m_pVehicle;

        if (autoMobile->m_aCarNodes[CAR_CHASSIS]) {
            tempMat.Attach(RwFrameGetMatrix(autoMobile->m_aCarNodes[CAR_CHASSIS]), false);
            chassisPosZ = tempMat.GetPosition().z;
            tempMat.Detach();
        }

        tempMat.SetTranslate({ 0.0f, 0.0f, -chassisPosZ });
        tempMat.RotateY(autoMobile->m_fDoomVerticalRotation); // AKA GunOrientation ?
        tempMat.SetTranslateOnly({ 0.0f, 0.0f, chassisPosZ });
        vehMat *= tempMat;
    }

    vehMat.GetPosition() += vehMat.TransformVector(seatLocalPos);
    tempMat.SetUnity();

    if (m_pVehicle->m_pHandlingData->GetAnimGroupId() == 13) { // ?
        if (this == m_pVehicle->m_apPassengers[1]) {
            m_fCurrentRotation = heading - HALF_PI;
            tempMat.SetTranslate({ 0.0f, 0.0f, 0.0f });
            tempMat.RotateZ(-HALF_PI);
            tempMat.SetTranslateOnly({ 0.0f, 0.6f, 0.0f });
            vehMat *= tempMat;
        } else if (this == m_pVehicle->m_apPassengers[2]) {
            m_fCurrentRotation = heading + HALF_PI;
            tempMat.SetTranslate({ 0.0f, 0.0f, 0.0f });
            tempMat.RotateZ(HALF_PI);
            vehMat *= tempMat;
        } else {
            m_fCurrentRotation = heading;
        }
    } else {
        m_fCurrentRotation = heading;
    }

    m_fAimingRotation = m_fCurrentRotation;
    SetMatrix(vehMat);
}

/*!
* @addr 0x5DFD60
* @brief Set head changing rate to value stored in m_pStats
*/
void CPed::RestoreHeadingRate() {
    m_fHeadingChangeRate = m_pStats->m_fHeadingChangeRate;
}

/*!
* @addr 0x5DFD70
*/
void CPed::RestoreHeadingRateCB(CAnimBlendAssociation* assoc, void* data) {
    UNUSED(assoc);

    auto& ped = *((CPed*)data);
    ped.m_fHeadingChangeRate = ped.m_pStats->m_fHeadingChangeRate;
}

/*!
* @addr 0x5DFD90
* @brief Set random radio station if ped is in car. The station is chosen randomly, and is either `m_nRadio1` or `m_nRadio2` from the ped's CPedModelInfo
*/
void CPed::SetRadioStation()
{
    if (IsPlayer() || !m_pVehicle)
        return;

    if (m_pVehicle->m_pDriver == this) {
        const auto& mi = *(CPedModelInfo*)GetModelInfo();
        m_pVehicle->m_vehicleAudio.m_AuSettings.RadioStation = (CGeneral::GetRandomNumber() <= RAND_MAX / 2) ? mi.m_nRadio1 : mi.m_nRadio2;
    }
}

/*!
* @addr 0x5DFDF0
*/
void CPed::PositionAttachedPed()
{
    if (!m_pAttachedTo) {
        return;
    }

    CMatrix attachedMat{};
    CVector offset;
    if (m_pAttachedTo->GetModelIndex() == MODEL_FIRELA
        && m_pAttachedTo->AsAutomobile()->m_aCarNodes[CAR_MISC_B]
        && m_vecTurretOffset.z < -900.0f
    ) {
        // Attached to the fire truck ladder
        attachedMat.Attach(RwFrameGetLTM(m_pAttachedTo->AsAutomobile()->m_aCarNodes[CAR_MISC_B]), false);
        attachedMat.Detach();
        offset = m_vecTurretOffset;
        offset.z += 1000.0f;
    } else {
        attachedMat = m_pAttachedTo->GetMatrix(); // Allocates the matrix if needed
        offset      = m_vecTurretOffset;
    }

    attachedMat.GetPosition() += attachedMat.TransformVector(offset);

    const float heading = std::atan2(-attachedMat.GetForward().x, attachedMat.GetForward().y);

    if (!IsPlayer()) {
        float desiredHeading = heading;
        switch (m_fTurretAngleA) {
        case 1: desiredHeading = heading + HALF_PI; break;
        case 2: desiredHeading = heading + PI;      break;
        case 3: desiredHeading = heading - HALF_PI; break;
        }
        desiredHeading = CGeneral::LimitRadianAngle(desiredHeading);

        m_fCurrentRotation = CGeneral::LimitRadianAngle(m_fCurrentRotation);

        float diff = m_fCurrentRotation - desiredHeading;
        if (diff > PI) {
            diff -= TWO_PI;
        } else if (diff < -PI) {
            diff += TWO_PI;
        }

        if (diff > m_fTurretAngleB) {
            m_fCurrentRotation = desiredHeading + m_fTurretAngleB;
        } else if (diff < -m_fTurretAngleB) {
            m_fCurrentRotation = desiredHeading - m_fTurretAngleB;
        }

        m_fCurrentRotation = CGeneral::LimitRadianAngle(m_fCurrentRotation);
    }

    CMatrix rotMat{};
    rotMat.SetRotateZ(m_fCurrentRotation - heading);
    attachedMat *= rotMat;
    SetMatrix(attachedMat);

    if (m_pAttachedTo->GetIsTypeVehicle() || m_pAttachedTo->GetIsTypeObject()) {
        m_vecMoveSpeed = m_pAttachedTo->m_vecMoveSpeed;
        m_vecTurnSpeed = m_pAttachedTo->m_vecTurnSpeed;
    }

    m_standingOnEntity = nullptr;
    bIsStanding        = true;
}

/*!
* @addr 0x5E00F0
* @brief Remove ped from the world, and request special model for it.
*/
void CPed::Undress(char* modelName) {
    DeleteRwObject();
    CStreaming::RequestSpecialModel(IsPlayer() ? 0 : m_nModelIndex, modelName, STREAMING_KEEP_IN_MEMORY | STREAMING_MISSION_REQUIRED);
    CWorld::Remove(this);
}

/*!
* @addr 0x5E0130
* @brief Re-add ped to the world
*/
void CPed::Dress() {
    SetModelIndex(m_nModelIndex);
    if (m_nPedState != PEDSTATE_DRIVING) {
        SetPedState(PEDSTATE_IDLE);
    }
    CWorld::Add(this);
    RestoreHeadingRate();
}

/*!
* @addr 0x5E0170
* @brief Checks if the Pedestrian is still alive.
*/
bool CPed::IsAlive() const {
    switch (m_nPedState) {
    case PEDSTATE_DIE:
    case PEDSTATE_DEAD:
        return false;
    }
    return true;
}

/*!
* @addr 0x5E01A0
*/
void CPed::UpdateStatEnteringVehicle()
{
    // NOP
}

/*!
* @addr 0x5E01B0
*/
void CPed::UpdateStatLeavingVehicle()
{
    // NOP
}

/*!
* @addr 0x5E0270
* @brief Release current cover point
*/
void CPed::ReleaseCoverPoint() {
    if (m_pCoverPoint) {
        m_pCoverPoint->ReleaseCoverPointForPed(this);
        m_pCoverPoint = nullptr;
    }
}

/*!
* @addr 0x5E0290
* @returns Any active task of type HOLD_ENTITY, PICKUP_ENTITY, PUTDOWN_ENTITY.
*/
CTaskSimpleHoldEntity* CPed::GetHoldingTask() {
    // Man programming in C++03 must've been a pain.. if, if, if, if, if, if... IF.
    if (const auto task = GetTaskManager().FindActiveTaskFromList({ TASK_SIMPLE_HOLD_ENTITY, TASK_SIMPLE_PICKUP_ENTITY, TASK_SIMPLE_PUTDOWN_ENTITY })) {
        return static_cast<CTaskSimpleHoldEntity*>(task);
    }
    return nullptr;
}

/*!
* @addr 0x5E02E0
*/
CEntity* CPed::GetEntityThatThisPedIsHolding()
{
    if (const auto task = GetHoldingTask()) {
        return task->m_pEntityToHold;
    }

    if (const auto task = GetTaskManager().Find<CTaskComplexGoPickUpEntity>()) {
        return task->m_pEntity;
    }

    return nullptr;
}

/*!
* @addr  0x5E0360
* @brief Drop held entity, possibly deleting it.
*/
void CPed::DropEntityThatThisPedIsHolding(bool bDeleteHeldEntity) {
    if (const auto task = GetHoldingTask()) {
        // Drop the entity
        task->DropEntity(this, true);

        // Delete held entity (If any)
        if (bDeleteHeldEntity) {
            if (const auto heldEntity = task->m_pEntityToHold) {
                if (!heldEntity->GetIsTypeObject() || !heldEntity->AsObject()->IsMissionObject()) {
                    heldEntity->DeleteRwObject(); // TODO; Are these 3 lines inlined?
                    CWorld::Remove(heldEntity);
                    delete heldEntity;
                }
            }
        }
    }
}

/*!
* @addr 0x5E0400
* @returns If there's a holding task and the held entity can be thrown.
*/
bool CPed::CanThrowEntityThatThisPedIsHolding() {
    if (const auto task = GetHoldingTask()) {
        return task->CanThrowEntity();
    }
    return false;
}

/*!
* @addr 0x5E0460
* @returns If there's a HANDSIGNAL task
*/
bool CPed::IsPlayingHandSignal() {
    return GetTaskManager().HasAnyOf<TASK_COMPLEX_HANDSIGNAL_ANIM>();
}

/*!
* @addr 0x5E0480
* @brief Stop the HANDSINGAL task
*/
void CPed::StopPlayingHandSignal() {
    if (const auto task = GetTaskManager().Find<TASK_COMPLEX_HANDSIGNAL_ANIM>()) {
        task->MakeAbortable(this);
    }
}

/*!
* @addr 0x5E04B0
* @returns Get walk speed in units/s based on the ped's anim group's WALK anim.
*/
float CPed::GetWalkAnimSpeed() {
    auto hier = CAnimManager::GetAnimAssociation(m_nAnimGroup, ANIM_ID_WALK)->m_BlendHier;

    CAnimManager::UncompressAnimation(hier);
    auto* const seq = &hier->m_pSequences[ANIM_ID_WALK];

    if (!seq->m_FramesNum) {
        return 0.f; // No frames
    }

    // NOTE: This is quite garbage, based on at least 5 assumptions, more of a hack than a solution from R*'s side.
    //       It won't work correctly if first frame has no translation, nor if the animation happens on any other axis than Y, etc..

    const auto lastFrame = seq->GetUKeyFrame(seq->m_FramesNum - 1);
    const auto lastFrameY = seq->HasTranslation()
        ? lastFrame->Trans.y
        : ((KeyFrame*)lastFrame)->Rot.imag.y;
    const auto firstFrameY = seq->GetUKeyFrame(0)->Trans.y;
    return (lastFrameY - firstFrameY) / hier->m_fTotalTime;
}

/*!
* @addr 0x5E06E0
*/
void CPed::SetPedDefaultDecisionMaker() {
    if (!IsPlayer()) {
        GetIntelligence()->SetPedDecisionMakerType(IsCreatedByMission() ? -1 : m_pStats->m_nDefaultDecisionMaker);
    } else {
        GetIntelligence()->SetPedDecisionMakerType(-2);
    }
}

/*!
* @addr 0x5E0730
* @returns If entity is a given range angle relative to our current rotation given by limitAngle [-limitAngle, limitAngle]
*/
bool CPed::CanSeeEntity(CEntity* entity, float limitAngle) {

    // TODO: Inlined? 0x5E0780, 0x5E07BB
    const auto FixRadianAngle = [](float angle) {
        if (angle < TWO_PI) {
            if (angle < 0.f) {
                return angle + TWO_PI;
            }
        } else {
            return angle - TWO_PI;
        }
        return angle;
    };

    // R* used the degree returning function, and converted to radians, we just use the radian version directly
    const auto pointAngle = FixRadianAngle(CGeneral::GetRadianAngleBetweenPoints(entity->GetPosition2D(), GetPosition2D()));

    const auto delta = std::abs(m_fCurrentRotation - pointAngle);
    return delta < limitAngle || delta > TWO_PI - limitAngle;
}

/*!
* @addr 0x5E0820
*/
bool CPed::PositionPedOutOfCollision(int32 exitDoor, CVehicle* vehicle, bool findClosestNode)
{
    if (!vehicle) {
        vehicle = m_pVehicle;
        if (!vehicle) {
            return false;
        }
    }

    if (bDonePositionOutOfCollision) {
        return true;
    }

    const auto& box = vehicle->GetColModel()->m_boundBox;
    // Using the same indices as the original (`float*` into the bounding box)
    const float boxMinX = box.m_vecMin.x, boxMinY = box.m_vecMin.y, boxMinZ = box.m_vecMin.z;
    const float boxMaxX = box.m_vecMax.x, boxMaxY = box.m_vecMax.y, boxMaxZ = box.m_vecMax.z;

    const CVector vehPos         = vehicle->GetPosition(); // Copy
    const CVector origPedPos     = GetPosition();          // Copy
    CVector       tempPos        = origPedPos;             // Last position that was tried (NOTE: Used even if it hasn't been set by us!)
    const bool    usedCollision  = m_bUsesCollision;
    bool          foundPos       = false;

    CWorld::pIgnoreEntity = vehicle;
    m_vecMoveSpeed        = CVector{};
    physicalFlags.bForceHitReturnFalse = true;
    m_bUsesCollision      = false;

    const auto& vehMat = vehicle->GetMatrix();

    // Is the current position (`tempPos`) valid?
    const auto IsTempPosClear = [&] {
        return !CheckCollision()
            && CWorld::GetIsLineOfSightClear(vehPos, tempPos, true, false, false, true, false, false, false);
    };

    // Sets the position (the vehicle's matrix is not touched)
    const auto TryPos = [&](const CVector& pos) {
        tempPos = pos;
        SetPosn(tempPos);
        return IsTempPosClear();
    };

    if (vehicle->IsOnItsSide() && vehicle->m_nVehicleType != VEHICLE_TYPE_BIKE) {
        // Try standing on top of the vehicle
        if (TryPos(CVector{ vehPos.x, vehPos.y, (vehPos.z + boxMaxX) + 1.0f })) { // BUG: Uses `boxMaxX` as the height
            foundPos = true;
        }
    } else if (exitDoor) {
        // Try the position next to the door
        if (TryPos(CCarEnterExit::GetPositionToOpenCarDoor(vehicle, exitDoor))) {
            foundPos = true;
        } else if (vehicle->m_nVehicleType == VEHICLE_TYPE_BIKE && (exitDoor == 10 || exitDoor == 11)) {
            // Try the other side
            if (TryPos(CCarEnterExit::GetPositionToOpenCarDoor(vehicle, exitDoor == 11 ? 9 : 8))) {
                foundPos = true;
            }
        }
    }

    const float maxZ     = -boxMinZ < boxMaxZ ? boxMaxZ : -boxMinZ;
    float       sideDist = std::abs(vehMat.GetRight().z) * maxZ + (boxMinX - 0.355f);
    if (exitDoor == 8 || exitDoor == 9) {
        sideDist = boxMaxX + 0.355f;
    }

    if (!foundPos) {
        // Try moving the ped to the side of the vehicle (Perpendicular to its right vector)
        const float dot  = (origPedPos.z - vehPos.z) * vehMat.GetRight().z
                         + (origPedPos.y - vehPos.y) * vehMat.GetRight().y
                         + (origPedPos.x - vehPos.x) * vehMat.GetRight().x;
        const float dist = sideDist - dot;
        foundPos = TryPos(CVector{
            dist * vehMat.GetRight().x + origPedPos.x,
            dist * vehMat.GetRight().y + origPedPos.y,
            dist * vehMat.GetRight().z + origPedPos.z
        });
    }

    if (!foundPos) {
        // Search along the side of the vehicle
        const float minY = boxMinY;
        const float step = (boxMaxY - minY) * 0.33333334f; // 0x859040
        for (int32 i = 0; i < 4 && !foundPos; i++) {
            const float y = (float)i * step + minY;
            const CVector pos{
                (sideDist * vehMat.GetRight().x + vehPos.x) + y * vehMat.GetForward().x,
                (sideDist * vehMat.GetRight().y + vehPos.y) + y * vehMat.GetForward().y,
                (sideDist * vehMat.GetRight().z + vehPos.z) + y * vehMat.GetForward().z
            };
            foundPos = TryPos(pos);
        }

        if (!foundPos) { // In front of the vehicle
            const float d = boxMinY - 0.355f;
            foundPos = TryPos(CVector{
                d * vehMat.GetForward().x + vehPos.x,
                d * vehMat.GetForward().y + vehPos.y,
                d * vehMat.GetForward().z + vehPos.z
            });
        }

        if (!foundPos) { // Behind the vehicle
            const float d = boxMaxY + 0.355f;
            foundPos = TryPos(CVector{
                d * vehMat.GetForward().x + vehPos.x,
                d * vehMat.GetForward().y + vehPos.y,
                d * vehMat.GetForward().z + vehPos.z
            });
        }

        if (!foundPos) { // The other side of the vehicle (front)
            const float d = boxMinY;
            foundPos = TryPos(CVector{
                (vehPos.x - sideDist * vehMat.GetRight().x) + d * vehMat.GetForward().x,
                (vehPos.y - sideDist * vehMat.GetRight().y) + d * vehMat.GetForward().y,
                (vehPos.z - sideDist * vehMat.GetRight().z) + d * vehMat.GetForward().z
            });
        }

        if (!foundPos) { // The other side of the vehicle (back)
            const float d = boxMaxY;
            foundPos = TryPos(CVector{
                (vehPos.x - sideDist * vehMat.GetRight().x) + d * vehMat.GetForward().x,
                (vehPos.y - sideDist * vehMat.GetRight().y) + d * vehMat.GetForward().y,
                (vehPos.z - sideDist * vehMat.GetRight().z) + d * vehMat.GetForward().z
            });
        }

        if (!foundPos && vehicle->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) { // On top of the vehicle
            foundPos = TryPos(CVector{
                boxMaxZ * vehMat.GetUp().x + vehPos.x,
                boxMaxZ * vehMat.GetUp().y + vehPos.y,
                (boxMaxZ * vehMat.GetUp().z + vehPos.z) + 1.0f
            });
        }
    }

    CWorld::pIgnoreEntity              = nullptr;
    physicalFlags.bForceHitReturnFalse = false;
    m_bUsesCollision                   = usedCollision;

    if (foundPos) {
        m_vecMoveSpeed = CVector{};
        bDonePositionOutOfCollision = true;
        m_vecTurnSpeed = CVector{};
        if (vehicle->m_nVehicleType != VEHICLE_TYPE_BIKE || findClosestNode) {
            vehicle->m_vecMoveSpeed = CVector{};
            vehicle->m_vecMoveSpeed.z -= 0.05f;
            vehicle->m_vecTurnSpeed = CVector{};
        }
        return true;
    }

    if (!findClosestNode) {
        return false;
    }

    // Couldn't find a position around the vehicle, use the closest path node instead
    const auto pedNode = ThePaths.FindNodeClosestToCoors(vehPos, PATH_TYPE_PED, 999999.88f, 0, 0, 0, 0, 0);
    const auto vehNode = ThePaths.FindNodeClosestToCoors(vehPos, PATH_TYPE_VEH, 999999.88f, 0, 0, 0, 0, 0);

    const bool hasPedNode = pedNode.IsAreaValid();
    if (hasPedNode) {
        tempPos = ThePaths.GetPathNode(pedNode)->GetPosition();
    }

    if (!vehNode.IsAreaValid()) {
        if (!hasPedNode) {
            return false;
        }
    } else {
        const CVector vehNodePos = ThePaths.GetPathNode(vehNode)->GetPosition();
        const CVector diff       = vehNodePos - vehPos;
        const float   nodeDist   = std::sqrt(diff.y * diff.y + diff.x * diff.x);
        const float   tempDist   = std::sqrt((tempPos.x - vehPos.x) * (tempPos.x - vehPos.x) + (tempPos.y - vehPos.y) * (tempPos.y - vehPos.y));
        if (nodeDist < tempDist) {
            tempPos = vehNodePos;
        }
    }

    tempPos = CPedPlacement::FindZCoorForPed(tempPos).first;
    SetPosn(tempPos);

    // Face the same direction as the vehicle
    const float heading = vehicle->m_matrix
        ? std::atan2(-vehicle->m_matrix->GetForward().x, vehicle->m_matrix->GetForward().y)
        : vehicle->m_placement.m_fHeading;
    if (m_matrix) {
        m_matrix->SetRotateZOnly(heading);
    } else {
        m_placement.m_fHeading = heading;
    }

    bDonePositionOutOfCollision = true;
    m_vecMoveSpeed              = CVector{};
    m_vecTurnSpeed              = CVector{};
    vehicle->m_vecTurnSpeed     = CVector{};
    vehicle->m_vecMoveSpeed     = CVector{};
    vehicle->m_vecMoveSpeed.z  += 0.02f;
    return true;
}

/*!
* @addr     0x5E13C0
* @brief    Teleport ped to the furthest point from here that isn't colliding with a building or ped.
* @returns  If there was at lest one such point.
*/
bool CPed::PositionAnyPedOutOfCollision() {
    struct Point {
        CVector pos{};
        bool    found{};
        float   distSq{};
    };
    Point vehiclePoint{}, noCollPoint{};

    // Find 2 points furthest away from us:
    // - One that is colliding with a vehicle, (vehiclePoint)
    // - One that isn't colliding with one. (noCollPoint)
    // Neither point should be colliding with a building or ped.
    auto testPoint{ GetPosition() };
    for (auto y = 0; y < 15; y++) {
        testPoint.y -= 3.5f;
        for (auto x = 0; x < 15; x++) {
            testPoint.x -= 3.5f;

            // If we collide with a building or ped skip
            if (!CWorld::TestSphereAgainstWorld(testPoint, 0.6f, this, /*buildings: */true, false, false, true, false, false)) {
                continue;
            }

            const auto PossiblyUpdatePoint = [&, this](Point& p) {
                const auto distSq{ (testPoint - GetPosition()).SquaredMagnitude() };
                if (distSq < p.distSq) {
                    p.pos = testPoint;
                    p.distSq = distSq;
                    p.found = true;
                }
            };

            // Check for collision with vehicles
            if (CWorld::TestSphereAgainstWorld(testPoint, 0.6f, this, false, /*vehicles: */true, false, false, false, false)) {
                PossiblyUpdatePoint(vehiclePoint);
            } else { // Not collided with anything
                PossiblyUpdatePoint(noCollPoint);
            }
        }
    }

    // Set our position to one of the point's
    if (noCollPoint.found) { // If there was a point not colliding with a vehicle use it
        SetPosn(noCollPoint.pos);
    } else if (vehiclePoint.found) { // Otherwise use the other one collided with one (if any)
        SetPosn(vehiclePoint.pos + CVector{ 0.f, 0.f, GetBoundingBox().m_vecMax.z });
    }

    return noCollPoint.found || vehiclePoint.found;
}

/*!
* @addr 0x5E1660
*/
bool CPed::OurPedCanSeeThisEntity(CEntity* entity, bool isSpotted) {
    if (!isSpotted) {
        const auto dir2D{ entity->GetPosition2D() - GetPosition2D() };
        if (   DotProduct2D(dir2D, m_matrix->GetForward()) < 0.f // Is behind us
            || dir2D.SquaredMagnitude() >= 40.f * 40.f           // NOTSA: Using SqMag instead of Mag
        ) {
            return false;
        }
    }

    auto target{entity->GetPosition()};
    if (entity->GetIsTypePed()) {
        target.z += 1.f; // Adjust for head pos?
    }

    // Seems like they explicitly use this one instead of `IsLineOfSightClear` because of the `shootThru` check.
    CColPoint cp{};
    CEntity* hitEntity{};
    return !CWorld::ProcessLineOfSight(GetPosition(), target, cp, hitEntity, true, false, false, isSpotted, false, false, false, isSpotted);
}

/*!
* @addr 0x5E17E0
* @unused
*/
void CPed::SortPeds(CPed** pedList, int32 lo, int32 hi) {
    if (lo >= hi) {
        return;
    }

    // Distance to this ped, accumulated in extended precision (z, y, x order) like the x87 code
    const auto DistTo = [this](const CPed* other) -> double {
        const auto& a = GetPosition();
        const auto& b = other->GetPosition();
        const double dx = (double)a.x - b.x;
        const double dy = (double)a.y - b.y;
        const double dz = (double)a.z - b.z;
        return std::sqrt((dz * dz + dy * dy) + dx * dx);
    };

    // Quicksort (ascending distance), recursion on the left part, loop on the right part
    do {
        int32 i = lo;
        int32 j = hi;

        const float pivotDist = (float)DistTo(pedList[(i + j) / 2]); // Spilled to a float in the original
        do {
            while (DistTo(pedList[i]) < (double)pivotDist) { // `!(x < pivot)` ends the loop (NaN too)
                i++;
            }
            while (DistTo(pedList[j]) > (double)pivotDist) {
                j--;
            }
            if (i > j) {
                break;
            }
            std::swap(pedList[i], pedList[j]);
            i++;
            j--;
        } while (i <= j);

        SortPeds(pedList, lo, j);
        lo = i;
    } while (lo < hi);
}

/*!
* @addr 0x5E1A00
* @brief Update `m_fFPSMoveHeading` depending on the ped's Up/Down, Left/Right control states.
*/
float CPed::WorkOutHeadingForMovingFirstPerson(float heading) {
    if (!IsPlayer() || !GetPlayerData()) {
        return 0.f; // Probably shouldn't ever happen, but okay
    }

    const auto walkUpDown = (float)CPad::GetPad()->GetPedWalkUpDown();
    const auto walkLeftRight = (float)CPad::GetPad()->GetPedWalkLeftRight();
    if (walkUpDown == 0.f) {
        if (walkLeftRight != 0.f) {
            GetPlayerData()->m_fFPSMoveHeading = walkLeftRight < 0.f ? HALF_PI : -HALF_PI;
        }
    } else {
        GetPlayerData()->m_fFPSMoveHeading = CGeneral::GetRadianAngleBetweenPoints(0.f, 0.f, -walkLeftRight, walkUpDown);
    }

    return CGeneral::LimitRadianAngle(heading + GetPlayerData()->m_fFPSMoveHeading);
}

/*!
* @addr 0x5E1B10
*/
void CPed::UpdatePosition()
{
    if (CReplay::Mode == MODE_PLAYBACK) {
        return;
    }

    const auto ApplyRotation = [this] {
        if (m_matrix) {
            m_matrix->SetRotateZOnly(m_fCurrentRotation);
        } else {
            m_placement.m_fHeading = m_fCurrentRotation;
        }
    };

    if (!bIsStanding) {
        if (!GetIntelligence()->GetTaskSwim() && !GetIntelligence()->GetTaskJetPack()) {
            const auto task = GetTaskManager().GetTaskPrimary(TASK_PRIMARY_PRIMARY);
            if (!task || task->GetTaskType() != TASK_COMPLEX_USE_SWAT_ROPE) {
                return;
            }
        }
        ApplyRotation();
        return;
    }

    if (m_pAttachedTo) {
        return;
    }

    ApplyRotation();

    const auto standingOn = m_standingOnEntity;

    float dx, dy; // Wanted change in velocity
    if (!standingOn) {
        if (g_surfaceInfos.IsSteepSlope(m_nContactSurface) && (field_578.x != 0.0f || field_578.y != 0.0f)) {
            CVector2D slopeDir{ field_578.x, field_578.y };
            slopeDir.Normalise();

            dx = slopeDir.x * 0.02f;
            m_vecMoveSpeed = CVector{ 0.0f, 0.0f, -0.001f };
            dx += m_vecAnimMovingShift.x;
            dy = slopeDir.y * 0.02f + m_vecAnimMovingShift.y;

            const auto dot = slopeDir.y * dy + slopeDir.x * dx;
            if (dot < 0.0f) {
                dx -= slopeDir.x * dot;
                dy -= dot * slopeDir.y;
            }
        } else {
            dx = m_vecAnimMovingShift.x - m_vecMoveSpeed.x;
            dy = m_vecAnimMovingShift.y - m_vecMoveSpeed.y;
        }
    } else {
        float speedX, speedY;
        if (!IsPlayer()
            && standingOn->GetIsTypeVehicle()
            && standingOn->AsVehicle()->m_nVehicleType == VEHICLE_TYPE_BOAT
        ) {
            // Calculate the speed at our position on the boat, plus a centripetal term
            CVector relPos = GetPosition();
            relPos.z -= 1.0f;
            relPos -= standingOn->GetPosition();

            const auto* const phys = standingOn->AsPhysical();
            CVector speed = phys->m_vecMoveSpeed + CrossProduct(phys->m_vecTurnSpeed, relPos);
            speed += (relPos * (phys->m_vecTurnSpeed.SquaredMagnitude() * -1.0f)) * CTimer::GetTimeStep();

            speedX = speed.x;
            speedY = speed.y;
            m_vecMoveSpeed.z = speed.z;
        } else {
            const auto speed = standingOn->AsPhysical()->GetSpeed(field_56C);
            speedX = speed.x;
            speedY = speed.y;
        }

        dx = (speedX + m_vecAnimMovingShift.x) - m_vecMoveSpeed.x;
        dy = (speedY + m_vecAnimMovingShift.y) - m_vecMoveSpeed.y;

        m_fCurrentRotation += CTimer::GetTimeStep() * standingOn->AsPhysical()->m_vecTurnSpeed.z;
        m_fAimingRotation  += CTimer::GetTimeStep() * standingOn->AsPhysical()->m_vecTurnSpeed.z;
    }

    // Limit the change in velocity
    const auto standingOn2 = m_standingOnEntity;
    if (!standingOn2
        || standingOn2->AsPhysical()->m_pAttachedTo
        || (standingOn2->AsPhysical()->physicalFlags.bDisableCollisionForce && !standingOn2->AsPhysical()->physicalFlags.bCollidable)
    ) {
        if (bFallenDown && !standingOn2) {
            const auto mag = std::sqrt(dy * dy + dx * dx);
            const auto maxMag = CTimer::GetTimeStep() * 0.01f;
            if (mag > maxMag) {
                const auto scale = maxMag / mag;
                dx *= scale;
                dy *= scale;
            }
        }
    } else {
        const auto mag = std::sqrt(dy * dy + dx * dx);

        float maxMag;
        bool  useMaxMag = true;
        if (standingOn2->GetIsTypeVehicle()) {
            if (m_nPedState == PEDSTATE_DIE) {
                maxMag = CTimer::GetTimeStep() * 0.002f;
            } else {
                const auto vehType = standingOn2->AsVehicle()->m_nVehicleType;
                if (vehType == VEHICLE_TYPE_BIKE && standingOn2->AsPhysical()->m_vecMoveSpeed.SquaredMagnitude() > 0.04f) {
                    maxMag = CTimer::GetTimeStep() * std::bit_cast<float>(0x3951B718u); // 0x86C304 (one ulp above the literal 0.0002f)
                } else {
                    maxMag = mag;
                    if (vehType == VEHICLE_TYPE_AUTOMOBILE) {
                        maxMag = CTimer::GetTimeStep() * 0.01f;
                    }
                }
            }
        } else {
            maxMag = CTimer::GetTimeStep() * 0.01f;
        }

        if (mag > maxMag) {
            const auto scale = maxMag / mag;
            dx *= scale;
            dy *= scale;
        }
    }

    m_vecMoveSpeed.x += dx;
    m_vecMoveSpeed.y += dy;
}

/*!
* @addr 0x5E1FA0
*/
void CPed::ProcessBuoyancy()
{
    if (bInVehicle)
        return;

    float fBuoyancyMult = 1.1F;
    if (!IsAlive())
        fBuoyancyMult = 1.8F;

    float fBuoyancy = fBuoyancyMult * m_fMass / 125.0F;
    CVector vecBuoyancyTurnPoint;
    CVector vecBuoyancyForce;
    if (!mod_Buoyancy.ProcessBuoyancy(this, fBuoyancy, &vecBuoyancyTurnPoint, &vecBuoyancyForce)) {
        physicalFlags.bTouchingWater = false;
        auto swimTask = GetIntelligence()->GetTaskSwim();
        if (swimTask)
            swimTask->m_fSwimStopTime = 1000.0F;

        return;
    }

    if (bIsStanding) {
        auto& standingOnEntity = m_pContactEntity;
        if (standingOnEntity && standingOnEntity->GetIsTypeVehicle()) {
            auto pStandingOnVehicle = standingOnEntity->AsVehicle();
            if (pStandingOnVehicle->IsBoat() && !pStandingOnVehicle->physicalFlags.bRenderScorched) {
                physicalFlags.bSubmergedInWater = false;
                auto swimTask = GetIntelligence()->GetTaskSwim();
                if (!swimTask)
                    return;

                swimTask->m_fSwimStopTime += CTimer::GetTimeStep();
                return;
            }
        }
    }

    if (GetPlayerData()) {
        const auto& vecPedPos = GetPosition();
        float fCheckZ = vecPedPos.z - 3.0F;
        CColPoint lineColPoint;
        CEntity* colEntity;
        if (CWorld::ProcessVerticalLine(vecPedPos, fCheckZ, lineColPoint, colEntity, false, true, false, false, false, false, nullptr)) {
            if (colEntity->GetIsTypeVehicle()) {
                auto colVehicle = colEntity->AsVehicle();
                if (colVehicle->IsBoat()
                    && !colVehicle->physicalFlags.bRenderScorched
                    && colVehicle->GetMatrix().GetUp().z > 0.0F) {

                    physicalFlags.bSubmergedInWater = false;
                    return;
                }
            }
        }
    }

    // Those 4 are called for some reason
    /*
    CTimeCycle::GetAmbientRed();
    CTimeCycle::GetAmbientGreen();
    CTimeCycle::GetAmbientBlue();
    CGeneral::GetRandomNumber();
    */

    // Add splash particle if it's the first frame we're touching water, and
    // the movement of ped is downward, preventing particles from being created
    // if ped is standing still and water wave touches him
    if (!physicalFlags.bTouchingWater && m_vecMoveSpeed.z < -0.01F) {
        auto vecMoveDir = m_vecMoveSpeed * CTimer::GetTimeStep() * 4.0F;
        auto vecSplashPos = GetPosition() + vecMoveDir;
        float fWaterZ;
        if (CWaterLevel::GetWaterLevel(vecSplashPos, fWaterZ, true, nullptr)) {
            vecSplashPos.z = fWaterZ;
            g_fx.TriggerWaterSplash(vecSplashPos);
            AudioEngine.ReportWaterSplash(this, -100.0F, true);
        }
    }

    physicalFlags.bTouchingWater = true;
    physicalFlags.bSubmergedInWater = true;
    ApplyMoveForce(vecBuoyancyForce);

    if (CTimer::GetTimeStep() / 125.0F < vecBuoyancyForce.z / m_fMass
        || GetPosition().z + 0.6F < mod_Buoyancy.m_fWaterLevel) {

        bIsStanding = false;
        bIsDrowning = true;

        bool bPlayerSwimmingOrClimbing = false;
        if (!IsPlayer()) {
            CEventInWater cEvent(0.75F);
            GetEventGroup().Add(&cEvent, false);
        }
        else {
            auto swimTask = GetIntelligence()->GetTaskSwim();
            if (swimTask) {
                swimTask->m_fSwimStopTime = 0.0F;
                bPlayerSwimmingOrClimbing = true;
            }
            else if (GetIntelligence()->GetTaskClimb()) {
                bPlayerSwimmingOrClimbing = true;
            }
            else {
                auto fAcceleration = vecBuoyancyForce.z / (CTimer::GetTimeStep() * m_fMass / 125.0F);
                CEventInWater cEvent(fAcceleration);
                GetEventGroup().Add(&cEvent, false);
            }

            if (bPlayerSwimmingOrClimbing)
                return;
        }

        float fTimeStep = pow(0.9F, CTimer::GetTimeStep());
        m_vecMoveSpeed.x *= fTimeStep;
        m_vecMoveSpeed.y *= fTimeStep;
        if (m_vecMoveSpeed.z < 0.0F)
            m_vecMoveSpeed.z *= fTimeStep;

        return;
    }

    auto swimTask = GetIntelligence()->GetTaskSwim();
    if (bIsStanding && swimTask)
    {
        swimTask->m_fSwimStopTime += CTimer::GetTimeStep();
        return;
    }

    if (GetPlayerData()) {
        CVector vecHeadPos(0.0F, 0.0F, 0.1F);
        GetTransformedBonePosition(vecHeadPos, eBoneTag::BONE_HEAD, false);
        if (vecHeadPos.z < mod_Buoyancy.m_fWaterLevel) {
            AsPlayer()->HandlePlayerBreath(true, 1.0F);
        }
    }
}

/*!
* @addr 0x5E3960
* @returns If ped can be moved, that is: not in the air or landing, is not arrested/dead/dying.
*/
bool CPed::IsPedInControl() const
{
    return !bIsLanding && !bIsInTheAir && IsAlive() && m_nPedState != PEDSTATE_ARRESTED;
}

/*!
* @addr 0x5E3990
* @brief Remove current weapon's object model. modelIndex should be the same as with which the object was created with.
*/
void CPed::RemoveWeaponModel(int32 modelIndex) {

    // For players remove any attached FX (Created in `AddWeaponModel` for molotov)
    if (IsPlayer()) {
        auto& activeWep = GetActiveWeapon();
        if (activeWep.m_FxSystem) {
            g_fxMan.DestroyFxSystem(activeWep.m_FxSystem);
            activeWep.m_FxSystem = nullptr;
        }
    }

    // Deal with weapon's loaded clump (if any)
    if (m_pWeaponObject) {
        if (   modelIndex == MODEL_INVALID
            || CModelInfo::GetModelInfo(modelIndex) == CVisibilityPlugins::GetClumpModelInfo(m_pWeaponObject)
        ) {
            // Release model info
            CVisibilityPlugins::GetClumpModelInfo(m_pWeaponObject)->RemoveRef();

#ifdef SA_SKINNED_PEDS
            // Remove skin anim
            if (IsClumpSkinned(m_pWeaponObject)) {
                RpClumpForAllAtomics(m_pWeaponObject, AtomicRemoveAnimFromSkinCB, nullptr);
            }
#endif

            // Destroy clump
            RpClumpDestroy(m_pWeaponObject);
            m_pWeaponObject = nullptr;
            m_pGunflashObject = nullptr;
        }
    }

    m_nWeaponGunflashAlphaMP1 = 0;
    m_nWeaponGunflashAlphaMP2 = 0;
    m_nWeaponModelId = -1;
}

/*!
* @addr 0x5E3A90
* @brief Creates goggles model for current infrared/night vision. See PutOnGoggles.
*/
void CPed::AddGogglesModel(int32 modelIndex, bool& inOutGogglesState) {
    assert(!m_pGogglesObject); // Make sure it's not created already

    if (modelIndex != MODEL_INVALID) {
        m_pGogglesObject = reinterpret_cast<RpClump*>(CModelInfo::GetModelInfo(modelIndex)->CreateInstanceAddRef());

        m_pGogglesState = &inOutGogglesState;
        inOutGogglesState = true;
    }
}

/*!
* @addr 0x5E3AE0
* @brief Puts on goggles if current weapon is infrared/night vision. (Also removes weapon model from hand and enabled corresponding PostFX)
*/
void CPed::PutOnGoggles() {
    auto& wepInSlot = GetWeaponInSlot(GetWeaponSlot(WEAPON_INFRARED));

    // Game checks if wepInSlot.m_nType != UNARMED here, not sure why? Probably compiler mistake on switch case codegen..

    switch (wepInSlot.m_Type) {
    case WEAPON_INFRARED:
    case WEAPON_NIGHTVISION: {

        // Add(load) googles model and enable PostFX
        const auto DoAddGogglesModel = [&, this](bool& state) {
            AddGogglesModel(wepInSlot.GetWeaponInfo().m_nModelId1, state);
        };

        switch (wepInSlot.m_Type) {
        case WEAPON_INFRARED:
            DoAddGogglesModel(CPostEffects::m_bInfraredVision);
            break;
        case WEAPON_NIGHTVISION:
            DoAddGogglesModel(CPostEffects::m_bNightVision);
            break;
        }

        // Make sure weapon model doesn't get loaded (Because we've put the them on)
        wepInSlot.m_DontPlaceInHand = true;

        // If it was the active weapon: unload it's weapon model
        if (&wepInSlot == &GetActiveWeapon()) {
            RemoveWeaponModel(wepInSlot.GetWeaponInfo().m_nModelId1);
        }

        break;
    }
    }
}

/*!
* @addr    0x5E6580
* @returns Weapon skill with current weapon
*/
eWeaponSkill CPed::GetWeaponSkill() {
    return GetWeaponSkill(GetActiveWeapon().m_Type);
}

/*!
* @addr     0x5E3B60
* @returns Skill with \a weaponType. In case we're a player it's based on our current stat level with this weapon type, otherwise the mem. var. `m_nWeaponSkill`.
*/
eWeaponSkill CPed::GetWeaponSkill(eWeaponType weaponType)
{
    if (!CWeaponInfo::TypeHasSkillStats(weaponType)) {
        return eWeaponSkill::STD;
    }

    if (IsPlayer())
    {
        const auto GetReqStatLevelWith = [this, weaponType](eWeaponSkill skill) {
            return (float)CWeaponInfo::GetWeaponInfo(weaponType, skill)->m_nReqStatLevel;
        };

        const auto statValue = CStats::GetStatValue((eStats)CWeaponInfo::GetSkillStatIndex(weaponType));
        if (statValue >= GetReqStatLevelWith(eWeaponSkill::PRO)) {
            return eWeaponSkill::PRO;
        } else if (statValue <= GetReqStatLevelWith(eWeaponSkill::POOR)) {
            return eWeaponSkill::POOR;
        } else {
            return eWeaponSkill::STD; // Somewhere in-between poor and pro stat levels
        }
    } else {
        if (weaponType == WEAPON_PISTOL && m_nPedType == PED_TYPE_COP)
            return eWeaponSkill::COP;
        return m_nWeaponSkill;
    }
}

/*!
* @addr 0x5E3C10
* @brief Set weapon skill, unless we're a player ped (IsPlayer)
*/
void CPed::SetWeaponSkill(eWeaponType weaponType, eWeaponSkill skill)
{
    if (!IsPlayer()) {
        m_nWeaponSkill = skill;
    }
}

/*!
* @addr 0x5E1950
* @brief Clear ped look, and start restoring it
*/
void CPed::ClearLookFlag() {
    if (!bIsLooking) {
        return;
    }

    // Originally there's a do-while loop, but it will never iterate more than once, so I won't add it.
    // do { ...

    bIsLooking = false;
    bIsDrowning = false;
    bIsRestoringLook = true;

    if (CanUseTorsoWhenLooking()) {
        m_pedIK.bTorsoUsed = false;
    }

    m_nLookTime = CTimer::GetTimeInMS() + (IsPlayer() ? 2000 : 4000);

    // .. } while ((PEDSTATE_LOOK_HEADING || PEDSTATE_LOOK_ENTITY) && bIsLooking), but `bIsLooking` will never be true at this point.
}

/*!
* @addr 0x5E3FF0
* @brief Just calls ClearLookFlag
*/
void CPed::ClearLook() {
    ClearLookFlag();
}

/*!
* @addr 0x5E4000
*/
/*!
* @addr    0x5E4000
* @brief   Turns ped to look at `m_pLookTarget`.
* @returns If `m_fCurrentRotation` changed.
*/
bool CPed::TurnBody() {
    if (m_pLookTarget) {
        m_fLookDirection = CGeneral::GetRadianAngleBetweenPoints(m_pLookTarget->GetPosition2D(), GetPosition2D());
    }

    m_fLookDirection = CGeneral::LimitRadianAngle(m_fLookDirection);

    // Some logic to make sure `m_fCurrentRotation` is always in the range [-PI, PI] or [0, 2PI] ? Not sure.. TODO.
    if (m_fCurrentRotation + PI >= m_fLookDirection) {
        if (m_fCurrentRotation - PI > m_fLookDirection) {
            m_fCurrentRotation += PI;
        }
    } else {
        m_fCurrentRotation -= PI;
    }

    m_fAimingRotation = m_fLookDirection;

    if (std::abs(m_fCurrentRotation - m_fLookDirection) <= 0.05f) {
        return true;
    } else {
        m_fCurrentRotation -= (m_fCurrentRotation - m_fLookDirection) * ExeRecip(5.f);
        return false;
    }
}

/*!
* @addr 0x5E4220
* @brief Check if `this` is valid. Probably used by scripts?
*/
bool CPed::IsPointerValid() {
    return GetPedPool()->IsObjectValid(this) && (!m_pCollisionList.IsEmpty() || this == FindPlayerPed());
}

/*!
* @addr 0x5E4280
* @brief Retrieve object-space position of the given \a bone.
* @param updateSkinBones if not already called `UpdateRpHAnim` will be called. If this param is not set, and the latter function wasn't yet called a default position will be returned.
*/
CVector CPed::GetBonePosition(eBoneTag bone, bool updateSkinBones) {
    if (!bCalledPreRender) {
        if (!updateSkinBones) {
            return m_matrix->TransformPoint(GetPedBoneStdPosition(bone));
        }
        UpdateRpHAnim();
        bCalledPreRender = true;
    }
    if (const auto* const m = GetBoneMatrix(bone)) {
        return *RwMatrixGetPos(m);
    }
    return GetPosition();
}

/*
* @addr 0x5E4280
* @brief Added for compatiblity reasons for hooking - use the version returning CVector where possible in code.
*/
void CPed::GetBonePosition(CVector* outVec, eBoneTag bone, bool updateSkinBones) {
    *outVec = CPed::GetBonePosition(bone, updateSkinBones);
}

/*!
* @addr 0x5E01C0
* @brief Transform inOutPos into the given \a bone's space
*
* @param [in,out] inOutPos The position to be transformed in-place.
* @param          updateSkinBones If `UpdateRpHAnim` should be called
*/
void CPed::GetTransformedBonePosition(RwV3d& inOutPos, eBoneTagU32 bone, bool updateSkinBones) { // todo: fix this too!!!
    // Pretty much the same as GetBonePosition..
    if (updateSkinBones) {
        if (!bCalledPreRender) {
            UpdateRpHAnim();
            bCalledPreRender = true;
        }
    } else if (!bCalledPreRender) { // Return static local bone position instead
        inOutPos = m_matrix->TransformPoint(GetPedBoneStdPosition(bone));
        return;
    }

    // Return actual position
    RwV3dTransformPoints(&inOutPos, &inOutPos, 1, GetBoneMatrix(bone));
}

/*!
* @addr 0x5E4390
* @brief Give ped an object to hold. If he already has one and \a replace is `true` they will drop it, otherwise do nothing.
*/
void CPed::GiveObjectToPedToHold(int32 modelIndex, uint8 replace) {

    // Deal with ped already holding an entity.
    // If `replace` is `true`, just drop the entity, otherwise do nothing.
    if (GetTaskManager().HasAnyOf<TASK_SIMPLE_HOLD_ENTITY>()) {
        if (!GetEntityThatThisPedIsHolding() || !replace) {
            return;
        }
        DropEntityThatThisPedIsHolding(true);
    }

    // Create object
    auto object = new CObject(modelIndex, false);
    object->SetPosn(GetPosition());
    CWorld::Add(object);

    // Create task
    CVector pos{ 0.f, 0.f, 0.f };
    GetTaskManager().SetTaskSecondary(new CTaskSimpleHoldEntity(object, &pos, PED_NODE_RIGHT_HAND), TASK_SECONDARY_PARTIAL_ANIM);
}

/*!
* @addr  0x5E4500
* @brief Set ped's state. If he's now !IsAlive() blip is deleted (if `bClearRadarBlipOnDeath` is set) and `ReleaseCoverPoint()` is called.
*/
void CPed::SetPedState(ePedState pedState) {
    m_nPedState = pedState;
    if (!IsAlive()) {
        ReleaseCoverPoint();
        if (bClearRadarBlipOnDeath) {
            CRadar::ClearBlipForEntity(BLIP_CHAR, GetPedPool()->GetRef(this));
            // TODO: Shouldn't we `bClearRadarBlipOnDeath = false` here?
        }
    }
}

/*!
* @addr 0x5E47E0
* @brief Set ped's created by. If created by mission, set it's hearing and seeing range to 30.
*/
void CPed::SetCharCreatedBy(ePedCreatedBy createdBy) {
    SetCreatedBy(createdBy);

    SetPedDefaultDecisionMaker();

    if (IsCreatedByMission()) {
        auto intel = GetIntelligence();

        intel->SetSeeingRange(30.f);
        intel->SetHearingRange(30.f);
        if (!IsPlayer()) {
            intel->m_fDmRadius = 0.f;
            intel->m_nDmNumPedsToScan = 0;
        }
    }
}

/*!
 * @addr 0x5E4C50
 */
void CPed::CalculateNewVelocity() {
    const auto timeStep = CTimer::GetTimeStep();

    // Turn towards the aiming rotation
    float diff{};
    if (!bIsInTheAir && !bIsLanding
        && m_nPedState != PEDSTATE_DIE && m_nPedState != PEDSTATE_DEAD && m_nPedState != PEDSTATE_ARRESTED
    ) {
        const float turnRate = m_fHeadingChangeRate * 0.017453292f /* 0x8595EC */ * timeStep;

        m_fCurrentRotation = CGeneral::LimitRadianAngle(m_fCurrentRotation);

        float targetRot = CGeneral::LimitRadianAngle(m_fAimingRotation);
        if (targetRot > m_fCurrentRotation + PI) {
            targetRot -= TWO_PI;
        } else if (targetRot < m_fCurrentRotation - PI) {
            targetRot += TWO_PI;
        }
        diff = targetRot - m_fCurrentRotation;

        const auto isPlayer = IsPlayer();
        if (isPlayer) {
            m_fMoveAnim = 1.0f;
        } else if (diff >= 0.0f && m_fMoveAnim < 0.0f) {
            m_fMoveAnim = 0.1f;
        } else if (diff < 0.0f && m_fMoveAnim > 0.0f) {
            m_fMoveAnim = -0.1f;
        }

        bool turned{};
        const float maxTurn = std::abs(m_fMoveAnim) * turnRate;
        if (diff > maxTurn) {
            m_fCurrentRotation += maxTurn;
            m_fMoveAnim        += timeStep * 0.1f;
            if (m_fMoveAnim > 1.0f) {
                m_fMoveAnim = 1.0f;
            }
            turned = true;
        } else if (-1.0f * maxTurn > diff) {
            m_fCurrentRotation -= maxTurn;
            m_fMoveAnim        -= timeStep * 0.1f;
            if (m_fMoveAnim < -1.0f) {
                m_fMoveAnim = -1.0f;
            }
            turned = true;
        } else {
            if (isPlayer || std::abs(diff) <= 0.1f * turnRate) {
                m_fCurrentRotation += diff;
                float anim = std::abs(diff) / turnRate;
                if (anim < 0.1f) {
                    anim = 0.1f;
                }
                m_fMoveAnim = anim;
            } else {
                m_fCurrentRotation += diff * 0.5f;
                m_fMoveAnim        *= 0.5f;
            }
        }

        if (isPlayer
            || (m_nMoveState != PEDMOVE_STILL && m_nMoveState != PEDMOVE_NONE)
            || !turned
            || GetIntelligence()->GetTaskUseGun()
            || GetIntelligence()->GetTaskFighting()
        ) {
            if (m_nMoveState == PEDMOVE_TURN_L || m_nMoveState == PEDMOVE_TURN_R) {
                m_nMoveState = PEDMOVE_STILL;
            }
        } else {
            m_nMoveState = diff <= 0.0f ? PEDMOVE_TURN_R : PEDMOVE_TURN_L;
        }

        m_pedIK.m_fBodyRoll = 0.0f;
    }

    // Slope handling
    const auto& mat = GetMatrix();
    const float fwdDot   = field_578.z * mat.GetForward().z + field_578.y * mat.GetForward().y + field_578.x * mat.GetForward().x;
    const float rightDot = field_578.z * mat.GetRight().z   + field_578.y * mat.GetRight().y   + field_578.x * mat.GetRight().x;

    const auto AsinClamped = [](float v) {
        return std::asin(std::clamp(v, -1.0f, 1.0f));
    };

    const auto UpdateSlopeRollFromRight = [&] { m_pedIK.m_fSlopeRoll = AsinClamped(rightDot); };
    const auto UpdateSlopePitchFromFwd  = [&] { m_pedIK.m_fSlopePitch = 0.75f * m_pedIK.m_fSlopePitch + (1.0f - 0.75f) * AsinClamped(fwdDot); };

    if (!m_pedIK.bSlopePitch) {
        if (m_nMoveState >= PEDMOVE_WALK && !IsPlayer()) {
            if (std::abs(m_pedIK.m_fSlopeRoll) > 0.02f) {
                m_pedIK.m_fSlopeRoll = std::pow(0.9f, timeStep) * m_pedIK.m_fSlopeRoll;
            }
            // BUG: The roll is zeroed regardless of the above
            m_pedIK.m_fSlopeRoll = 0.0f;
        }
    } else {
        float decay = 0.0f;
        if (m_nPedState == PEDSTATE_DIE) {
            UpdateSlopeRollFromRight();
            UpdateSlopePitchFromFwd();
        } else if (!bFallenDown
            && (m_nMoveState < PEDMOVE_WALK || g_surfaceInfos.IsStairs(m_nContactSurface) || bPedHitWallLastFrame)
        ) {
            if (m_pedIK.m_fSlopePitch != 0.0f || m_pedIK.m_fSlopeRoll != 0.0f) {
                decay = std::pow(0.9f, timeStep);
            }

            if (std::abs(m_pedIK.m_fSlopePitch) <= 0.01f) {
                m_pedIK.m_fSlopePitch = 0.0f;
            } else {
                m_pedIK.m_fSlopePitch = decay * m_pedIK.m_fSlopePitch;
            }

            if (std::abs(m_pedIK.m_fSlopeRoll) > 0.02f) {
                m_pedIK.m_fSlopeRoll = decay * m_pedIK.m_fSlopeRoll;
            } else {
                m_pedIK.m_fSlopeRoll = 0.0f;
            }
        } else if (m_nPedState == PEDSTATE_DIE || bFallenDown) {
            UpdateSlopeRollFromRight();
            UpdateSlopePitchFromFwd();
        } else {
            m_pedIK.m_fSlopeRoll = 0.0f;
            UpdateSlopePitchFromFwd();
        }
    }

    // Anim moving shift
    const float fwdScale   = std::sqrt(std::max(0.0f, 1.0f - fwdDot * fwdDot));
    const float rightScale = std::sqrt(std::max(0.0f, 1.0f - rightDot * rightDot));

    m_vecAnimMovingShift = CVector2D{};

    const float fwdShift = fwdScale * m_vecAnimMovingShiftLocal.y;
    m_vecAnimMovingShift.x += fwdShift * mat.GetForward().x;
    m_vecAnimMovingShift.y += fwdShift * mat.GetForward().y;

    const float rightShift = rightScale * m_vecAnimMovingShiftLocal.x;
    m_vecAnimMovingShift.x += rightShift * mat.GetRight().x;
    m_vecAnimMovingShift.y += rightShift * mat.GetRight().y;

    if (timeStep < 0.01f && !CTimer::bSlowMotionActive) {
        m_vecAnimMovingShift.x *= 0.01f;
        m_vecAnimMovingShift.y *= 0.01f;
    } else {
        const float invTimeStep = 1.0f / timeStep;
        m_vecAnimMovingShift.x *= invTimeStep;
        m_vecAnimMovingShift.y *= invTimeStep;
    }
}

/*!
* @addr 0x5E52E0
*/
void CPed::CalculateNewOrientation() {
    if (CReplay::Mode != MODE_PLAYBACK && IsPedInControl()) {
        SetOrientation(0.f, 0.f, m_fCurrentRotation);
    }
}

/*!
* @addr 0x5E5320
*/
void CPed::ClearAll() {
    if (IsPedInControl() || IsStateDead()) {
        bRenderPedInCar = true;
        bHitSteepSlope = false;
        bCrouchWhenScared = false;
        m_nPedState = PEDSTATE_NONE;
        m_nMoveState = PEDMOVE_NONE;
        m_pEntityIgnoredCollision = nullptr;
    }
}

namespace {
// NOTSA: Name by hand, original name unknown.
// Byte at 0xB72C70: Read only by `SpawnFootDust` (0x5E37C0), never written by any code. If set, no dust particles are created.
auto& s_bSuppressFootDust = StaticRef<bool>(0xB72C70);

// Returns a random number in [0, 1]
float GetRandomUnitFloat() {
    return (float)CGeneral::GetRandomNumber() * RAND_MAX_FLOAT_RECIPROCAL;
}

// 0x5E3630 - Original is a custom calling convention (count: eax, pos: edi, ped: ebx)
// Spawns small splash particles around `pos` (used while it's raining)
void SpawnFootRainSplash(int32 count, const CVector& pos, CPed* ped) {
    if (!ped->GetIsOnScreen()) {
        return;
    }

    const auto& camPos = TheCamera.GetPosition();
    const float dx     = pos.x - camPos.x;
    const float dy     = pos.y - camPos.y;
    if (dy * dy + dx * dx > 100.0f) {
        return;
    }

    const FxPrtMult_c prt{ 1.0f, 1.0f, 1.0f, 0.1f, 0.15f, 0.0f, 0.15f };
    for (int32 i = 0; i < count; i++) {
        CVector particlePos = pos;
        particlePos.x = (GetRandomUnitFloat() * 0.2f + particlePos.x) - 0.1f;
        particlePos.y = (GetRandomUnitFloat() * 0.2f + particlePos.y) - 0.1f;

        g_fx.m_Splash->AddParticle(particlePos, CVector{}, 0.0f, prt);
        g_fx.m_Splash->AddParticle(particlePos, ped->GetMatrix().GetForward() * 1.5f, 0.0f, prt);
    }
}

// 0x5E37C0 - Original is a custom calling convention (count: eax, pos: ebx, ped: esi)
// Spawns dust particles around `pos` (if the surface the ped is standing on produces them)
void SpawnFootDust(int32 count, const CVector& pos, CPed* ped) {
    if (!ped->GetIsOnScreen()) {
        return;
    }

    const auto& camPos = TheCamera.GetPosition();
    const float dx     = pos.x - camPos.x;
    const float dy     = pos.y - camPos.y;
    if (dy * dy + dx * dx > 100.0f) {
        return;
    }

    const FxPrtMult_c prt{ 1.0f, 1.0f, 1.0f, 0.1f, 0.15f, 0.0f, 0.15f };
    if (!g_surfaceInfos.ProducesFootDust(ped->m_nContactSurface)) {
        return;
    }

    for (int32 i = 0; i < count; i++) {
        CVector particlePos = pos;
        particlePos.x = (GetRandomUnitFloat() * 0.2f + particlePos.x) - 0.1f;
        particlePos.y = (GetRandomUnitFloat() * 0.2f + particlePos.y) - 0.1f;

        CVector vel;
        vel.x = GetRandomUnitFloat() * 0.3f - 0.15f;
        vel.y = GetRandomUnitFloat() * 0.3f - 0.15f;
        vel.z = 0.0f;

        if (!s_bSuppressFootDust) {
            g_fx.m_SmokeII3expand->AddParticle(particlePos, vel, 0.0f, prt);
        }
    }
}
} // namespace

/*!
 * @addr 0x5E5380
 */
void CPed::DoFootLanded(bool leftFoot, uint8 arg1) {
    if (!bCalledPreRender || m_bDontUpdateHierarchy) {
        return;
    }

    CVector footPos;
    GetBonePosition(&footPos, leftFoot ? BONE_L_FOOT : BONE_R_FOOT, false);

    const auto& mat   = GetMatrix();
    const auto  fwd   = mat.GetForward(); // Copies
    const auto  right = mat.GetRight();

    // Move the position a bit forward, and down
    footPos.z -= 0.1f;
    footPos.x = fwd.x * 0.2f + footPos.x;
    footPos.y = footPos.y + fwd.y * 0.2f;
    footPos.z = fwd.z * 0.2f + footPos.z;

    if (bDoBloodyFootprints && CLocalisation::Blood()) {
        CShadows::AddPermanentShadow(
            SHADOW_DEFAULT,
            gpBloodPoolTex,
            &footPos,
            fwd.x * 0.26f, fwd.y * 0.26f,
            right.x * 0.14f, right.y * 0.14f,
            255, 200, 0, 0,
            4.0f,
            3000,
            1.0f
        );

        // NOTE: `m_nDeathTimeMS` is reused as the footprint counter
        if ((uint32)m_nDeathTimeMS > 20u) {
            m_nDeathTimeMS -= 20;
        } else {
            m_nDeathTimeMS = 0;
            bDoBloodyFootprints = false;
        }
    }

    if (g_surfaceInfos.LeavesFootsteps(m_nContactSurface)) {
        const auto& pedPos = GetPosition();
        const auto& camPos = TheCamera.GetPosition();
        if (std::sqrt((camPos.x - pedPos.x) * (camPos.x - pedPos.x) + (camPos.y - pedPos.y) * (camPos.y - pedPos.y)) < 10.0f) {
            CShadows::AddPermanentShadow(
                SHADOW_DEFAULT,
                gpShadowPedTex,
                &footPos,
                fwd.x * -0.26f, fwd.y * -0.26f,
                right.x * -0.1f, right.y * -0.1f,
                120, 250, 250, 50,
                4.0f,
                2000u + (IsPlayer() ? 3000u : 0u),
                1.0f
            );
        }
    }

    // Rain splashes and dust
    if (CWeather::Rain > 0.1f && !CCullZones::CamNoRain() && !CCullZones::PlayerNoRain() && CGame::currArea == AREA_CODE_NORMAL_WORLD) {
        SpawnFootRainSplash(4, footPos, this);
    }
    SpawnFootDust(4, footPos, this);

    if (arg1) {
        m_Wobble      = TWO_PI;
        m_WobbleSpeed = (m_vecAnimMovingShift.y * m_vecAnimMovingShift.y + m_vecAnimMovingShift.x * m_vecAnimMovingShift.x) * 20.0f + 0.4f;
    }

    CVector splashPos;
    if (physicalFlags.bSubmergedInWater) {
        const auto pos = GetPosition(); // Copy
        float waterZ{};
        CWaterLevel::GetWaterLevel(pos.x, pos.y, pos.z + 1.5f, waterZ, true, nullptr);

        const auto dx = CTimer::GetTimeStep() * m_vecMoveSpeed.x;
        const auto dy = CTimer::GetTimeStep() * m_vecMoveSpeed.y;
        splashPos.x = (dx + dx) + pos.x;
        splashPos.y = (dy + dy) + pos.y;
        splashPos.z = waterZ;

        if (pos.z - 0.4f <= waterZ) {
            return;
        }
    } else {
        if (!g_surfaceInfos.IsShallowWater(m_nContactSurface)) {
            return;
        }

        splashPos = CVector{ footPos.x, footPos.y, 0.3f + footPos.z };
        splashPos += GetForwardVector() * -0.2f;
    }

    g_fx.TriggerFootSplash(splashPos);
    m_pedAudio.AddAudioEvent(AE_PED_FOOTSTEP_LEFT, 0.0f, 1.0f, nullptr, SURFACE_WATER_SHALLOW, 0, 0);
}

/*!
* @addr 0x5E57F0
*/
void CPed::PlayFootSteps() {
    auto* const firstAssoc = RpAnimBlendClumpGetFirstAssociation(GetRpClump());

    float                  idleBlendTotal{}; // Blend of all non-walk animations
    float                  walkBlendTotal{}; // Blend of all walk animations
    CAnimBlendAssociation* walkAssoc{};
    const uint8            isWalkRunSprintAnim = firstAssoc->m_AnimId == ANIM_ID_WALK
                                              || firstAssoc->m_AnimId == ANIM_ID_RUN
                                              || firstAssoc->m_AnimId == ANIM_ID_SPRINT;

    const bool isSkater = m_pStats == &CPedStats::ms_apPedStats[(size_t)ePedStats::SKATER];

    if (bDoBloodyFootprints) {
        // NOTE: `m_nDeathTimeMS` is reused as the footprint counter
        if (m_nDeathTimeMS != 0 && (uint32)m_nDeathTimeMS < 300u) {
            m_nDeathTimeMS--;
            if (m_nDeathTimeMS == 0) {
                bDoBloodyFootprints = false;
            }
        }
    }

    if (!bIsStanding) {
        return;
    }

    // 0x5E5E69
    const auto ProcessLanding = [this]() {
        if (!bIsLanding) {
            return;
        }
        const auto task = GetTaskManager().GetSimplestActiveTask();
        if (task->GetTaskType() != TASK_SIMPLE_LAND) {
            return;
        }
        const auto landTask = static_cast<CTaskSimpleLand*>(task);
        if (landTask->RightFootLanded()) {
            DoFootLanded(false, true);
        } else if (landTask->LeftFootLanded()) {
            DoFootLanded(true, true);
        }
    };

    for (auto* a = firstAssoc; a; a = RpAnimBlendGetNextAssociation(a)) {
        if (a->m_Flags & ANIMATION_WALK) {
            walkBlendTotal += a->m_BlendAmount;
            walkAssoc       = a;
        } else if (!(a->m_Flags & ANIMATION_DONT_ADD_TO_PARTIAL_BLEND)
            && a->m_AnimId != ANIM_ID_FIGHT_IDLE
            && ((a->m_Flags & ANIMATION_IS_PARTIAL) || !bIsDucking)
        ) {
            idleBlendTotal += a->m_BlendAmount;
        }
    }

    if (!walkAssoc || walkBlendTotal <= 0.5f || idleBlendTotal >= 1.0f) {
        ProcessLanding();
        return;
    }

    const float curTime  = walkAssoc->m_CurrentTime;
    const float timeStep = walkAssoc->m_TimeStep;

    const float totalTime = walkAssoc->m_BlendHier->m_fTotalTime;
    float       minAnimTime = totalTime * ExeRecip(15.0f); // 0x863E0C
    float       maxAnimTime = totalTime * 0.5f + minAnimTime;
    if (bIsDucking) {
        minAnimTime += 0.2f;
        maxAnimTime += 0.2f;
    }

    // Plays the footstep audio event + (ducking/sneaking adjusted)
    const auto DoFootStepAE = [&](eAudioEvents event) {
        float volume{}, speed{};
        if (!bIsDucking) {
            if (m_nMoveState == PEDMOVE_RUN) {
                volume = -6.0f;
                speed  = 1.1f;
            } else if (m_nMoveState == PEDMOVE_SPRINT) {
                speed = 1.2f;
            } else {
                volume = -12.0f;
                speed  = 0.9f;
            }
            if (m_nAnimGroup == ANIM_GROUP_PLAYERSNEAK) {
                volume -= 6.0f;
                speed  -= 0.1f;
            }
        } else {
            volume = -18.0f;
            speed  = 0.8f;
        }
        if (m_pedAudio.m_bCanAddEvent) {
            m_pedAudio.AddAudioEvent(event, volume, speed);
        }
    };

    if (isSkater) {
        const float threshold = walkAssoc->m_AnimId == ANIM_ID_WALK ? 0.53333336f : 0.33333334f;
        float adhesion  = 1.0f;

        switch (g_surfaceInfos.GetAdhesionGroup(m_nContactSurface)) {
        case ADHESION_GROUP_LOOSE:
            if (CGeneral::GetRandomNumber() & 0x7F) {
                m_vecAnimMovingShiftLocal.x *= 0.5f;
                m_vecAnimMovingShiftLocal.y *= 0.5f;
            }
            adhesion = 0.5f;
            break;
        case ADHESION_GROUP_SAND:
            if (CGeneral::GetRandomNumber() & 0x3F) {
                m_vecAnimMovingShiftLocal.x *= 0.2f;
                m_vecAnimMovingShiftLocal.y *= 0.2f;
            }
            ProcessLanding();
            return;
        case ADHESION_GROUP_WET:
            m_vecAnimMovingShiftLocal.x *= 0.3f;
            m_vecAnimMovingShiftLocal.y *= 0.3f;
            ProcessLanding();
            return;
        default:
            break;
        }

        const float skateSpeed = walkAssoc->m_AnimId == ANIM_ID_WALK ? 0.75f : 1.0f;
        if (curTime > 0.0f && curTime - timeStep <= 0.0f) {
            if (m_pedAudio.m_bCanAddEvent) {
                m_pedAudio.AddAudioEvent(AE_PED_SKATE_LEFT, std::log10(adhesion) * 20.0f, skateSpeed);
            }
        } else if (adhesion > 0.2f && curTime > threshold && curTime - timeStep < threshold) {
            if (m_pedAudio.m_bCanAddEvent) {
                m_pedAudio.AddAudioEvent(AE_PED_SKATE_RIGHT, std::log10(adhesion) * 20.0f, skateSpeed);
            }
        }

        ProcessLanding();
        return;
    }

    if (minAnimTime > curTime || curTime - timeStep >= minAnimTime) {
        // Right foot
        if (curTime >= maxAnimTime && curTime - timeStep < maxAnimTime) {
            DoFootStepAE(AE_PED_FOOTSTEP_RIGHT);
            DoFootLanded(false, isWalkRunSprintAnim);
        }
    } else {
        // Left foot
        if (IsPlayer() && m_pPlayerData) {
            const bool wearingBalaclava = m_pPlayerData->m_pPedClothesDesc->GetIsWearingBalaclava();
            const float ratio           = m_pPlayerData->m_fMoveBlendRatio;

            float volume{};
            bool  makeSound{};
            if (m_nMoveState == PEDMOVE_JOG || m_nMoveState == PEDMOVE_RUN) {
                if (ratio >= 2.0f) {
                    volume    = wearingBalaclava ? 55.0f : 45.0f;
                    makeSound = true;
                } else {
                    bool calcVolume{};
                    float factor{};
                    if (wearingBalaclava && ratio > 1.1f) {
                        calcVolume = true;
                        factor     = 20.0f;
                    } else if (ratio > 1.5f) {
                        calcVolume = true;
                        factor     = 15.0f;
                    }
                    if (calcVolume) {
                        volume = (ratio - 1.0f) * factor + 30.0f;
                        if (volume > 0.0f) {
                            makeSound = true;
                        }
                    }
                }
            } else if (m_nMoveState == PEDMOVE_SPRINT) {
                volume    = wearingBalaclava ? 65.0f : 55.0f;
                makeSound = true;
            }
            if (makeSound) {
                GetEventGlobalGroup()->Add(CEventSoundQuiet{ this, volume, (uint32)(-1), CVector{} });
            }
        }

        DoFootStepAE(AE_PED_FOOTSTEP_LEFT);
        DoFootLanded(true, isWalkRunSprintAnim);
    }

    ProcessLanding();
}

/*!
* @addr 0x5E5ED0
* @brief Create model for current active weapon. Also creates FX for molotov if `this->IsPlayer()`.
* @param modelIndex Model that should be created for the current weapon.
*/
void CPed::AddWeaponModel(int32 modelIndex) {
   if (modelIndex == MODEL_INVALID) {
       return;
   }

   // Make sure this weapon is supposed to have a model
   // (May be set to false even if it does, eg.: in case of infrared or night googles, see `TakeOffGoggles`)
   auto& activeWep = GetActiveWeapon();
   if (activeWep.m_DontPlaceInHand) {
       return;
   }

   // Remove old model (if any)
   if (m_pWeaponObject) {
       RemoveWeaponModel(MODEL_INVALID);
   }

   // Create clump for model
   auto& wepMI = *CModelInfo::GetModelInfo(modelIndex);
   m_pWeaponObject = (RpClump*)wepMI.CreateInstance();
   m_pGunflashObject = m_pWeaponObject ? CClumpModelInfo::GetFrameFromName(m_pWeaponObject, "gunflash") : nullptr;
   wepMI.AddRef();

   m_nWeaponModelId = modelIndex;

   // Create FX for molotovs
   if (IsPlayer()) {
       if (   activeWep.m_Type == WEAPON_MOLOTOV
           && modelIndex == eModelID::MODEL_MOLOTOV
           && !activeWep.m_FxSystem
        ) {
           activeWep.m_FxSystem = g_fxMan.CreateFxSystem("molotov_flame", CVector{ 0.f, 0.f, 0.f }, GetBoneMatrix(eBoneTag::BONE_R_HAND), false);
           if (const auto fx = activeWep.m_FxSystem) {
               fx->SetLocalParticles(true);
               fx->CopyParentMatrix();
               fx->Play();
           }
       }
   }
}

/*!
* @addr 0x5E6010
* @brief Take off goggles (Infrared/Night Vision weapon)
*/
void CPed::TakeOffGoggles()
{
    auto& wepInSlot = GetWeaponInSlot(GetWeaponSlot(WEAPON_INFRARED));

    // Game checks if wepInSlot.m_nType != UNARMED here, not sure why? Probably compiler mistake on switch case codegen..

    switch (wepInSlot.m_Type) {
    case WEAPON_INFRARED:
    case WEAPON_NIGHTVISION: {
        // Remove googles model
        RemoveGogglesModel();

        wepInSlot.m_DontPlaceInHand = false;

        // Since we've took off the goggles we might have to load it's weapon model
        if (&wepInSlot == &GetActiveWeapon()) {
            AddWeaponModel(wepInSlot.GetWeaponInfo().m_nModelId1);
        }

        break;
    }
    }
}

/*!
* @addr 0x5E6080
* @brief Give ped weapon \a weaponType with ammo \a ammo. If ped has already the same weapon, just add the ammo to the weapon's current total ammo, and reload it.
* @returns Slot of \a weaponType
*/
eWeaponSlot CPed::GiveWeapon(eWeaponType weaponType, uint32 ammo, bool likeUnused) {
    const auto givenWepInfo = CWeaponInfo::GetWeaponInfo(weaponType);
    auto& wepInSlot = GetWeaponInSlot(givenWepInfo->m_nSlot);
    const auto wepSlot = (eWeaponSlot)givenWepInfo->m_nSlot;

    if (wepInSlot.m_Type != weaponType) { // Another weapon in the slot, remove it, and set this weapon

        // Remove previous weapon (and possibly add any ammo it had to `ammo`)
        if (wepInSlot.m_Type != WEAPON_UNARMED) {
            switch (wepSlot) {
            case eWeaponSlot::SHOTGUN:
            case eWeaponSlot::SMG:
            case eWeaponSlot::RIFLE: {
                ammo += wepInSlot.m_TotalAmmo;
                break;
            }
            }

            RemoveWeaponModel(wepInSlot.GetWeaponInfo().m_nModelId1);

            if (givenWepInfo->m_nSlot == CWeaponInfo::GetWeaponInfo(WEAPON_INFRARED)->m_nSlot) {
                RemoveGogglesModel();
            }

            wepInSlot.Shutdown();
        }

        // Give new
        wepInSlot.Initialise(weaponType, ammo, this);

        // Now `wepInSlot` is the weapon we've given to the player

        if (givenWepInfo->m_nSlot == m_nActiveWeaponSlot && !bInVehicle) {
            AddWeaponModel(givenWepInfo->m_nModelId1);
        }
    } else { // Same weapon already in the slot, update its ammo count and `Reload()` it
        if (wepSlot == eWeaponSlot::GIFT) { // Gifts have no ammo :D
            return eWeaponSlot::GIFT;
        }

        wepInSlot.m_TotalAmmo = std::min(99'999u, wepInSlot.m_TotalAmmo + ammo);
        wepInSlot.Reload(this);

        // TODO: Inlined
        if (wepInSlot.m_State == WEAPONSTATE_OUT_OF_AMMO) {
            if (wepInSlot.m_TotalAmmo > 0) {
                wepInSlot.m_State = WEAPONSTATE_READY;
            }
        }
    }

    if (wepInSlot.m_State != WEAPONSTATE_OUT_OF_AMMO) {
        wepInSlot.m_State = WEAPONSTATE_READY;
    }

    return wepSlot;
}

/*!
* @notsa
*/
void CPed::GiveWeaponSet1() {
    GiveWeapon(WEAPON_BRASSKNUCKLE, 1, true);
    GiveWeapon(WEAPON_BASEBALLBAT, 1, true);
    GiveWeapon(WEAPON_MOLOTOV, 10, true);
    GiveWeapon(WEAPON_PISTOL, 100, true);
    GiveWeapon(WEAPON_SHOTGUN, 50, true);
    GiveWeapon(WEAPON_MICRO_UZI, 150, true);
    GiveWeapon(WEAPON_AK47, 120, true);
    GiveWeapon(WEAPON_COUNTRYRIFLE, 25, true);
    GiveWeapon(WEAPON_RLAUNCHER, 200, true);
    GiveWeapon(WEAPON_SPRAYCAN, 200, true);
}

/*!
* @notsa
*/
void CPed::GiveWeaponSet2() {
    GiveWeapon(WEAPON_KNIFE, 0, true);
    GiveWeapon(WEAPON_GRENADE, 10, true);
    GiveWeapon(WEAPON_DESERT_EAGLE, 40, true);
    GiveWeapon(WEAPON_SAWNOFF_SHOTGUN, 40, true);
    GiveWeapon(WEAPON_TEC9, 150, true);
    GiveWeapon(WEAPON_M4, 150, true);
    GiveWeapon(WEAPON_SNIPERRIFLE, 21, true);
    GiveWeapon(WEAPON_FLAMETHROWER, 500, true);
    GiveWeapon(WEAPON_EXTINGUISHER, 200, true);
}

/*!
* @notsa
*/
void CPed::GiveWeaponSet3() {
    GiveWeapon(WEAPON_CHAINSAW, 0, true);
    GiveWeapon(WEAPON_REMOTE_SATCHEL_CHARGE, 5, true);
    GiveWeapon(WEAPON_PISTOL_SILENCED, 40, true);
    GiveWeapon(WEAPON_SPAS12_SHOTGUN, 30, true);
    GiveWeapon(WEAPON_MP5, 100, true);
    GiveWeapon(WEAPON_M4, 150, true);
    GiveWeapon(WEAPON_RLAUNCHER_HS, 200, true);
}

/*!
 * @notsa
 */
void CPed::GiveWeaponSet4() {
    // todo: GiveWeapon(WEAPON_INFRARED, 200, true);
    // todo: GiveWeapon(WEAPON_NIGHTVISION, 200, true);
    GiveWeapon(WEAPON_MINIGUN, 500, true);
    GiveWeapon(WEAPON_DILDO2, 0, true);
}

/*!
* @addr 0x5E61F0
* @brief Set current weapon to be the one in \a slot
*/
void CPed::SetCurrentWeapon(int32 slot) {
    if (slot == -1) {
        return;
    }

    // Remove current weapon's model (if any)
    if (const auto currWepType = GetActiveWeapon().m_Type; currWepType != WEAPON_UNARMED) {
        RemoveWeaponModel(CWeaponInfo::GetWeaponInfo(currWepType)->m_nModelId1);
    }

    // Set as active slot
    m_nActiveWeaponSlot = slot;

    // Set chosen weapon in player data
    if (const auto playerData = AsPlayer()->GetPlayerData()) {
        playerData->m_nChosenWeapon = slot;
    }

    // Load weapon in this slot (if any)
    if (const auto wepInSlotType = m_aWeapons[slot].m_Type; wepInSlotType != WEAPON_UNARMED) {
        AddWeaponModel(CWeaponInfo::GetWeaponInfo(wepInSlotType)->m_nModelId1);
    }
}

/*!
* @addr 0x5E6280
* @brief Set current weapon as \a weaponType
*/
void CPed::SetCurrentWeapon(eWeaponType weaponType) {
    SetCurrentWeapon(GetWeaponSlot(weaponType));
}

/*!
* @addr  0x5E62B0
* @brief If ped's current weapon is \a weaponType clear it, and set current weapon as UNARMED
*/
void CPed::ClearWeapon(eWeaponType weaponType)
{
    auto wepSlot = CWeaponInfo::GetWeaponInfo(weaponType)->m_nSlot;
    if (wepSlot == -1) {
        return; // Weapon has no slot. (How could this happen?)
    }

    auto& wep = m_aWeapons[wepSlot];
    if (wep.m_Type != weaponType) {
        return; // Slot doesn't contain the given weapon - Might happen as some weapons share slots.
    }

    if (m_nActiveWeaponSlot == wepSlot) {
        SetCurrentWeapon(WEAPON_UNARMED);
    }

    wep.Shutdown();

    switch (weaponType) {
    case WEAPON_NIGHTVISION:
    case WEAPON_INFRARED:
        RemoveGogglesModel();
        break;
    }
}

/*!
* @addr 0x5E6320
* @brief Clears every weapon from the ped.
*/
void CPed::ClearWeapons()
{
    RemoveWeaponModel(MODEL_INVALID);
    RemoveGogglesModel();
    for (auto& m_aWeapon : m_aWeapons) {
        m_aWeapon.Shutdown();
    }
    CWeaponInfo* getWeaponInfo = CWeaponInfo::GetWeaponInfo(WEAPON_UNARMED, eWeaponSkill::STD);
    SetCurrentWeapon(getWeaponInfo->m_nSlot);
}

/*!
* @addr 0x5E6370
* @brief Saves ped's in foot weapon and equips a weapon that can be used in a vehicle if available.
* @param isJetpack  1: when entering jetpack, 0: any other vehicle
*/
void CPed::RemoveWeaponWhenEnteringVehicle(int32 isJetpack) {
    assert(isJetpack == 0 || isJetpack == 1);
    if (GetPlayerData()) {
        GetPlayerData()->m_bInVehicleDontAllowWeaponChange = true;
    }

    if (m_nSavedWeapon != WEAPON_UNIDENTIFIED) {
        return;
    }

    const auto SaveCurrentWeaponAndEquipInSlot = [&](eWeaponSlot slot) {
        // if (m_nSavedWeapon == WEAPON_UNIDENTIFIED) // always true
        m_nSavedWeapon = GetActiveWeapon().GetType();
        SetCurrentWeapon(GetWeaponInSlot(slot).GetWeaponInfo().m_nSlot);
    };

    const auto EquipHandgunIfPossible = [&] {
        const auto &shotgun = GetWeaponInSlot(eWeaponSlot::SHOTGUN), &handgun = GetWeaponInSlot(eWeaponSlot::HANDGUN);
        if (shotgun.GetType() == WEAPON_SAWNOFF_SHOTGUN && shotgun.GetTotalAmmo() > 0 || handgun.GetType() == WEAPON_PISTOL && handgun.GetTotalAmmo() > 0) {
            SaveCurrentWeaponAndEquipInSlot(eWeaponSlot::HANDGUN);
        } else {
            RemoveWeaponModel(CWeaponInfo::GetWeaponInfo(this, eWeaponSkill::STD)->m_nModelId1);
        }
    };

    const auto HandleDriveByWeapons = [&](const CWeapon& w) {
        if (w.GetTotalAmmo() > 0) {
            SaveCurrentWeaponAndEquipInSlot(eWeaponSlot::SMG);
        } else if (isJetpack != 1) {
            RemoveWeaponModel(CWeaponInfo::GetWeaponInfo(this, eWeaponSkill::STD)->m_nModelId1);
        } else {
            EquipHandgunIfPossible();
        }
    };

    if (!IsPlayer() || !AsPlayer()->GetPlayerInfoForThisPlayerPed()->m_bCanDoDriveBy) {
        return RemoveWeaponModel(CWeaponInfo::GetWeaponInfo(this, eWeaponSkill::STD)->m_nModelId1);
    }

    if (const auto& w = GetWeaponInSlot(eWeaponSlot::SMG); notsa::contains({ WEAPON_MICRO_UZI, WEAPON_TEC9 }, w.GetType())) {
        return HandleDriveByWeapons(w);
    }

    if (isJetpack == 1) {
        // Whole logic of this function is complicated asf because the player is not
        // supposed to be able to use MP5 while using jetpack. Only Tec-9 or Uzi.
        return EquipHandgunIfPossible();
    }

    if (const auto& w = GetWeaponInSlot(eWeaponSlot::SMG); w.GetType() == eWeaponType::WEAPON_MP5) {
        return HandleDriveByWeapons(w);
    }
}

/*!
* @addr  0x5E6490
* @brief If ped has no saved weapon and is not a player load current weapon's model, otherwise set saved weapon as current.
*/
void CPed::ReplaceWeaponWhenExitingVehicle() {
    if (GetPlayerData()) {
        GetPlayerData()->m_bInVehicleDontAllowWeaponChange = false;
    }

    if (IsPlayer() && m_nSavedWeapon != WEAPON_UNIDENTIFIED) {
        SetCurrentWeapon(m_nSavedWeapon);
        m_nSavedWeapon = WEAPON_UNIDENTIFIED;
    } else {                                                           // Not player, or has no saved weapon
        AddWeaponModel(GetActiveWeapon().GetWeaponInfo().m_nModelId1); // Load current active weapon
    }
}

/*!
* @addr 0x5E6530
* @brief Save ped's current weapon and set current to UNARMED.
*/
void CPed::ReplaceWeaponForScriptedCutscene()
{
    m_nSavedWeapon = GetActiveWeapon().m_Type;
    SetCurrentWeapon(0);
}

/*!
* @addr 0x5E6550
* @brief Set ped's current weapon to saved weapon
*/
void CPed::RemoveWeaponForScriptedCutscene()
{
    if (m_nSavedWeapon != WEAPON_UNIDENTIFIED) {
        CWeaponInfo* weaponInfo = CWeaponInfo::GetWeaponInfo(m_nSavedWeapon, eWeaponSkill::STD);
        SetCurrentWeapon(weaponInfo->m_nSlot);
        m_nSavedWeapon = WEAPON_UNIDENTIFIED;
    }
}

/*!
* @addr 0x5E65A0
*/
void CPed::PreRenderAfterTest()
{
    auto* const intel = GetIntelligence();
    if (const auto* swim = intel->GetTaskSwim()) {
        swim->ApplyRollAndPitch(this);
        m_pedIK.bSlopePitch = false;
    } else if (auto* jetpack = intel->GetTaskJetPack()) {
        jetpack->ApplyRollAndPitch(this);
        m_pedIK.bSlopePitch = false;
    }

    if (intel->GetTaskInAir()) {
        m_pedIK.bSlopePitch = false;
    } else if (m_pedIK.bSlopePitch || !IsPlayer() && m_pedIK.m_fSlopePitch != 0.0f) {
        m_pedIK.PitchForSlope();
    }
    bCalledPreRender = true;
    UpdateRpHAnim();

    if (!CTimer::bSkipProcessThisFrame && m_pWeaponObject && GetPlayerData()) {
        if (GetActiveWeapon().GetType() == eWeaponType::WEAPON_MINIGUN) {
            if (const auto f = CClumpModelInfo::GetFrameFromName(m_pWeaponObject, "minigun2")) {
                RwMatrixRotate(
                    RwFrameGetMatrix(f),
                    &CPedIK::XaxisIK,
                    RadiansToDegrees(CTimer::GetTimeStep() * GetPlayerData()->m_fGunSpinSpeed),
                    rwCOMBINEPRECONCAT
                );
            }
        }
    }

    if (GetIsVisible() && CTimeCycle::GetShadowStrength()) {
        const auto [shadowNeeded, activeTask] = [&]() -> std::pair<bool, CTask*> {
            if (!bInVehicle) {
                return std::make_pair(false, intel->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_ENTER_ANY_CAR_AS_DRIVER));
            }

            return std::make_pair(intel->GetTaskManager().FindActiveTaskFromList({ TASK_COMPLEX_LEAVE_CAR, TASK_COMPLEX_DRAG_PED_FROM_CAR }) != nullptr, nullptr);
        }();

        // Low quality circle below feet shadow
        const auto DrawDummyShadow = [&] {
            if (!m_pShadowData && (!bInVehicle || shadowNeeded)) {
                CShadows::StoreShadowForPedObject(
                    this,
                    CTimeCycle::m_fShadowDisplacementX[CTimeCycle::m_CurrentStoredValue],
                    CTimeCycle::m_fShadowDisplacementY[CTimeCycle::m_CurrentStoredValue],
                    CTimeCycle::m_fShadowFrontX[CTimeCycle::m_CurrentStoredValue],
                    CTimeCycle::m_fShadowFrontY[CTimeCycle::m_CurrentStoredValue],
                    CTimeCycle::m_fShadowSideX[CTimeCycle::m_CurrentStoredValue],
                    CTimeCycle::m_fShadowSideY[CTimeCycle::m_CurrentStoredValue]
                );
            }
        };

        // FIX_BUGS: Original check was only for 1st player in high FX quality.
        if (g_fx.GetFxQuality() != FX_QUALITY_VERY_HIGH && (g_fx.GetFxQuality() != FX_QUALITY_HIGH || !IsPlayer())) {
            DrawDummyShadow();
        } else if (const auto b = GetBonePosition(eBoneTag::BONE_ROOT); DistanceBetweenPoints2D(b, TheCamera.GetPosition2D()) <= MAX_DISTANCE_PED_SHADOWS_SQR) {
            const auto IsVehicleRTShadable = [](eVehicleType t) {
                switch (t) {
                case VEHICLE_TYPE_BMX:
                case VEHICLE_TYPE_BIKE:
                case VEHICLE_TYPE_QUAD:
                    return true;
                default:
                    return false;
                }
            };

            auto drawRealTimeShadow = true;
            if (!physicalFlags.bSubmergedInWater) {
                if (const auto* veh = GetVehicleIfInOne()) {
                    drawRealTimeShadow = IsVehicleRTShadable(veh->m_nVehicleSubType);
                }

                if (activeTask) {
                    drawRealTimeShadow = false;
                    if (const auto* targetVeh = notsa::cast<CTaskComplexEnterCarAsDriver>(activeTask)->GetTargetCar()) {
                        drawRealTimeShadow = IsVehicleRTShadable(targetVeh->m_nVehicleSubType);
                    }
                }

                if (const auto bsp = GetBonePosition(eBoneTag::BONE_SPINE1); IsAlive() && GetPosition().z - 0.2f > bsp.z) {
                    drawRealTimeShadow = bIsDucking;
                }

                if (drawRealTimeShadow) {
                    g_realTimeShadowMan.DoShadowThisFrame(this);
                    DrawDummyShadow();
                }
            }
        }
    }

    if (GetModelId() == MODEL_PLAYER) {
        ShoulderBoneRotation(GetRpClump());
        m_bDontUpdateHierarchy = true;
    }
    float windMod{};
    const auto rainAffectsPlayer = IsPlayer() && CWindModifiers::FindWindModifier(GetPosition(), &windMod, &windMod) && !CCullZones::PlayerNoRain();
    const auto drivingOpenTopVeh = IsStateDriving() && IsInVehicle() && (m_pVehicle->IsBike() || m_pVehicle->IsAutomobile() && m_pVehicle->IsOpenTopCar());

    const auto GetHierMatrix = [h = GetAnimHierarchyFromSkinClump(GetRpClump())](AnimationId id) {
        return &RpHAnimHierarchyGetMatrixArray(h)[RpHAnimIDGetIndex(h, id)];
    };

    if (!GetPlayerData() || !GetPlayerData()->m_pPedClothesDesc->IsWearingModel("vest") && !GetPlayerData()->m_pPedClothesDesc->IsWearingModel("torso")) {
        if (rainAffectsPlayer || drivingOpenTopVeh) {
            float vehSpeed = drivingOpenTopVeh ? m_pVehicle->GetMoveSpeed().Magnitude() : 0.0f;

            if (rainAffectsPlayer) {
                vehSpeed = std::max(vehSpeed, std::abs(windMod - 1.0f));
            }

            static constexpr float flt_8D1378 = 0.2f, flt_8D1380 = 0.2f;
            static constexpr float flt_8D137C = 0.1f;

            const auto ScaleAnimHierMat = [GetHierMatrix](float range, AnimationId id) {
                const CVector scale{
                    CGeneral::GetRandomNumberInRange(1.0f - range, 1.0f + range),
                    CGeneral::GetRandomNumberInRange(1.0f - range, 1.0f + range),
                    CGeneral::GetRandomNumberInRange(1.0f - range, 1.0f + range),
                };
                RwMatrixScale(GetHierMatrix(id), &scale, rwCOMBINEPRECONCAT);
                return scale;
            };

            ScaleAnimHierMat(flt_8D1378 * vehSpeed, ANIM_ID_ROADCROSS);
            auto scale = ScaleAnimHierMat(flt_8D137C * vehSpeed, ANIM_ID_SHOT_RIGHTP);
            RwMatrixScale(GetHierMatrix(ANIM_ID_GAS_CWR), &scale, rwCOMBINEPRECONCAT);
            if (drivingOpenTopVeh || !intel->GetTaskJetPack()) {
                RwMatrixScale(GetHierMatrix(ANIM_ID_IDLE), &scale, rwCOMBINEPRECONCAT);
            }
            scale = ScaleAnimHierMat(flt_8D1380 * vehSpeed, ANIM_ID_HIT_FRONT);
            RwMatrixScale(GetHierMatrix(ANIM_ID_KD_LEFT), &scale, rwCOMBINEPRECONCAT);
        }
    }

    if (bIsTalking && m_nBodypartToRemove == 2) {
        const CVector scale{};
        RwMatrixScale(GetHierMatrix(ANIM_ID_WALK_START), &scale, rwCOMBINEPRECONCAT);
        RwMatrixScale(GetHierMatrix(ANIM_ID_IDLE_HBHB_0), &scale, rwCOMBINEPRECONCAT);
        RwMatrixScale(GetHierMatrix(ANIM_ID_RUN_STOP), &scale, rwCOMBINEPRECONCAT);
        RwMatrixScale(GetHierMatrix(ANIM_ID_RUN_STOPR), &scale, rwCOMBINEPRECONCAT);
    }

    if (m_Wobble > 0.0f) {
        static constexpr float WOBBLE_FACTOR = 5.0f; // 0x8D21F0

        const auto angle = std::sin(m_Wobble) * -WOBBLE_FACTOR;
        m_Wobble -= CTimer::GetTimeStep() * m_WobbleSpeed;

        if (IsPlayer()) {
            RwMatrixRotate(GetHierMatrix(ANIM_ID_SMKCIG_PRTL_F), &CPedIK::ZaxisIK, angle, rwCOMBINEPRECONCAT);
            RwMatrixRotate(GetHierMatrix(ANIM_ID_DRNKBR_PRTL_F), &CPedIK::ZaxisIK, angle, rwCOMBINEPRECONCAT);
        }
        RwMatrixRotate(GetHierMatrix(ANIM_ID_BIKE_HIT), &CPedIK::ZaxisIK, angle, rwCOMBINEPRECONCAT);
    }

    if (CWeather::Earthquake > 0.0f) {
        const auto swing = CGeneral::GetRandomNumberInRange(-CWeather::Earthquake, CWeather::Earthquake);

        RwMatrixRotate(GetHierMatrix(ANIM_ID_FIGHTSH_LEFT), &CPedIK::ZaxisIK, swing, rwCOMBINEPOSTCONCAT);
        RwMatrixRotate(GetHierMatrix(ANIM_ID_GUNMOVE_BWD), &CPedIK::ZaxisIK, swing, rwCOMBINEPOSTCONCAT);
        RwMatrixRotate(GetHierMatrix(ANIM_ID_HIT_L), &CPedIK::ZaxisIK, swing, rwCOMBINEPOSTCONCAT);
        RwMatrixRotate(GetHierMatrix(ANIM_ID_KD_RIGHT), &CPedIK::ZaxisIK, swing, rwCOMBINEPOSTCONCAT);
        RwMatrixRotate(GetHierMatrix(ANIM_ID_HIT_FRONT), &CPedIK::ZaxisIK, swing, rwCOMBINEPOSTCONCAT);
        RwMatrixRotate(GetHierMatrix(ANIM_ID_KD_LEFT), &CPedIK::ZaxisIK, swing, rwCOMBINEPOSTCONCAT);
        RwMatrixRotate(GetHierMatrix(ANIM_ID_FIGHTSH_BWD), &CPedIK::ZaxisIK, swing, rwCOMBINEPOSTCONCAT);
        RwMatrixRotate(GetHierMatrix(ANIM_ID_GUNMOVE_R), &CPedIK::ZaxisIK, swing, rwCOMBINEPOSTCONCAT);
        RwMatrixRotate(GetHierMatrix(ANIM_ID_HIT_BACK), &CPedIK::ZaxisIK, swing, rwCOMBINEPOSTCONCAT);
        RwMatrixRotate(GetHierMatrix(ANIM_ID_KO_SKID_FRONT), &CPedIK::ZaxisIK, swing, rwCOMBINEPOSTCONCAT);
        RwMatrixRotate(GetHierMatrix(ANIM_ID_WALK_START), &CPedIK::ZaxisIK, swing, rwCOMBINEPOSTCONCAT);
    }

    if (bIsTalking && m_nBodypartToRemove == 2 && !IsStateDead() && !bIsDyingStuck && CTimer::GetFrameCounter() % 8 > 3) {
        g_fx.AddBlood(GetHierMatrix(ANIM_ID_WALK_START)->pos, 0.6f * GetUp(), 16, m_fContactSurfaceBrightness);
    }

    if (CWeather::Rain > 0.3f
        && TheCamera.m_fSoundDistUp > 15.0f
        && !bInVehicle
        && CGame::CanSeeOutSideFromCurrArea()
        && GetPosition().z < 900.0f
        && !CCullZones::CamNoRain()
    ) {
        if (DistanceBetweenPoints(TheCamera.GetPosition(), GetPosition()) < 25.0f) {
            auto* const pedModelInfo = GetModelInfo()->AsPedModelInfoPtr();
            pedModelInfo->AnimatePedColModelSkinnedWorld(GetRpClump());

            if (const auto& s = FindPlayerSpeed(); std::abs(s.x) <= 0.05f
                && std::abs(s.y) <= 0.05f
                && !IsStateDying()
                && !notsa::contains({ PEDSTATE_FALL, PEDSTATE_ATTACK, PEDSTATE_FIGHT }, m_nPedState)
                && IsPedHeadAbovePos(0.3f)
                && !RpAnimBlendClumpGetAssociation(GetRpClump(), ANIM_ID_IDLE_TIRED)
            ) {
                const auto* colData = pedModelInfo->GetColModel()->GetData();
                for (const auto& sphere : colData->GetSpheres()) {
                    if (notsa::contains(std::initializer_list<uint8>{ 5, 6, 9 }, sphere.m_Surface.m_nPiece)) {
                        g_fx.m_Splash->AddParticle(sphere.m_vecCenter + CVector::Random({ -0.08f, -0.08f, -0.08f }, { 0.08f, 0.08f, 0.02f }), s * 50.0f, 0.0f, FxPrtMult_c{ 1.0f, 1.0f, 1.0f, 0.35f, 0.01f, 0.0f, 0.03f });
                    }
                }
            }
        }
    }

    if (GetPlayerData() && GetPlayerData()->m_nWetness && GetPlayerData()->m_nWaterCoverPerc < 30u) {
        FxPrtMult_c p{1.0f, 1.0f, 1.0f, 0.2f, 0.15f, 0.0f, 0.1f};
        CVector     pos = GetPosition();
        pos.x += CGeneral::GetRandomNumberInRange(-0.03f, 0.03f);
        pos.y += CGeneral::GetRandomNumberInRange(-0.03f, 0.03f);
        pos.z += CGeneral::GetRandomNumberInRange(-0.8f, 0.2f);
        p.m_Color.alpha *= (float)GetPlayerData()->m_nWetness * ExeRecip(100.0f);
        g_fx.m_WaterSplash->AddParticle(pos, {}, 0.0f, p);
    }

    if (const auto* veh = GetVehicleIfInOne()) {
        m_fContactSurfaceBrightness = veh->m_fContactSurfaceBrightness;
    }
}

/*!
* @addr 0x5E7980
* @brief Set ped's state to IDLE
*/
void CPed::SetIdle() {
    switch (m_nPedState) {
    case PEDSTATE_IDLE:
    case PEDSTATE_MUG:
    case PEDSTATE_FLEE_ENTITY:
        break;

    case PEDSTATE_AIMGUN:
        m_nPedState = PEDSTATE_IDLE;
        m_nMoveState = PEDMOVE_STILL;
        break;

    default:
        m_nMoveState = PEDMOVE_STILL;
        break;
    }
}

/*!
* @addr 0x5E79B0
* @brief Set ped to look in direction \a heading
*/
void CPed::SetLook(float heading) {
    if (!IsPedInControl()) {
        return;
    }

    m_nPedState = PEDSTATE_LOOK_HEADING;

    if (m_nLookTime >= CTimer::GetTimeInMS()) {
        return;
    }

    bIsLooking = true;
    m_fLookDirection = heading;
    m_nLookTime = 0;

    ClearReference(m_pLookTarget);

    if (CanUseTorsoWhenLooking()) {
        m_pedIK.bTorsoUsed = false;
    }
}

/*!
* @addr 0x5E7A60
* @brief Set ped to look at \a entity
*/
void CPed::SetLook(CEntity* entity) {
    if (IsPedInControl()) {
        m_nPedState = PEDSTATE_LOOK_ENTITY;
        SetLookFlag(entity, false, false);
    }
}

/*!
* @addr 0x5E7B20
* @copydoc TurnBody
*/
void CPed::Look() {
    TurnBody();
}

/*!
* @addr 0x5E7CB0
* @brief Attach a weapon to us or something, not sure.
* @returns \a entity
*/
CEntity* CPed::AttachPedToEntity(CEntity* entity, CVector offset, uint16 turretAngleA, float turretAngleB, eWeaponType weaponType) {
    if (!entity || bInVehicle) {
        return nullptr;
    }

    // BUG/NOTE: ClearReference not called here?
    m_pAttachedTo = entity->AsPhysical();
    assert(m_pAttachedTo);
    m_pAttachedTo->RegisterReference(reinterpret_cast<CEntity**>(&m_pAttachedTo));

    m_vecTurretOffset = offset;
    m_fTurretAngleB = turretAngleB;
    m_fTurretAngleA = turretAngleA;

    // Deal collision with `entity`
    if (!IsPlayer()) {
        if (entity->GetIsTypeVehicle()) {
            m_pEntityIgnoredCollision = entity->AsPhysical();
        }
    } else { // For player just disable collision
        SetUsesCollision(false);
    }

    if (m_nSavedWeapon == WEAPON_UNIDENTIFIED) {
        m_nSavedWeapon = GetActiveWeapon().m_Type;
        m_nTurretAmmo = GetActiveWeapon().m_TotalAmmo; // todo: unify types
    }

    if (!IsPlayer()) {
        GiveWeapon(weaponType, 30'000, true);
        SetCurrentWeapon(weaponType);
        PositionAttachedPed();
    } else {
        if (weaponType != WEAPON_UNARMED) {
            GiveWeapon(weaponType, 30'000, true);
        }

        GetPlayerData()->m_nChosenWeapon = weaponType;

        if (weaponType == WEAPON_CAMERA) {
            TheCamera.SetNewPlayerWeaponMode(eCamMode::MODE_CAMERA);
        } else {
            // With the pool cue we can aim as well, so it needs a different cam mode.
            if (   entity->m_nModelIndex == eModelID::MODEL_POOLCUE
                && !CWeaponInfo::GetWeaponInfo(weaponType)->flags.b1stPerson
            ) {
                TheCamera.SetNewPlayerWeaponMode(eCamMode::MODE_AIMWEAPON_ATTACHED);
                GetPlayerData()->m_bFreeAiming = true;
            } else {
                TheCamera.SetNewPlayerWeaponMode(eCamMode::MODE_HELICANNON_1STPERSON);
            }
        }
        m_nPedState = PEDSTATE_SNIPER_MODE;
        PositionAttachedPed();
    }

    return entity;
}

/*!
* @addr 0x5E7E60
*/
void CPed::AttachPedToBike(CEntity* entity, CVector offset, uint16 turretAngleA, float turretAngleB, float turretPosnMode, eWeaponType weaponType) {
    if (AttachPedToEntity(entity, offset, turretAngleA, turretAngleB, weaponType)) {
        m_nTurretPosnMode = turretPosnMode;
    }
}

/*!
* @addr 0x5E7EC0
*/
void CPed::DettachPedFromEntity(){
    const auto wasAttachedTo = m_pAttachedTo;

    // BUG/NOTE: Again, ClearOldReference not called.
    m_pAttachedTo = nullptr;

    switch (m_nPedState) {
    case PEDSTATE_DIE: {
        m_pEntityIgnoredCollision = wasAttachedTo;
        ApplyMoveForce(wasAttachedTo->GetMatrix().GetForward() * -4.f);
        bIsStanding = false;
        break;
    }
    case PEDSTATE_DEAD: // Skip this
        break;

    default: {
        CAnimManager::BlendAnimation(GetRpClump(), m_nAnimGroup, ANIM_ID_IDLE, 1000.f);
        bIsStanding = true;

        // Restore old weapon if any
        if (m_nSavedWeapon != WEAPON_UNIDENTIFIED) {
            GetActiveWeapon().m_AmmoInClip = 0;
            GetActiveWeapon().m_TotalAmmo = 0;

            SetCurrentWeapon(m_nSavedWeapon);
            GetActiveWeapon().m_TotalAmmo = (uint32)m_nTurretAmmo;

            m_nSavedWeapon = WEAPON_UNIDENTIFIED;
        }

        if (IsPlayer()) {
            AsPlayer()->ClearWeaponTarget();
        }

        break;
    }
    }
}

/*!
* @addr 0x5E8830
*/
void CPed::SetAimFlag(float heading) {
    bIsAimingGun = true;
    bIsRestoringGun = false;
    ClearReference(m_pLookTarget);
    m_nLookTime = 0;

    if (bIsDucking) {
        m_pedIK.bUseArm = false;
    } // It's intentionally overwritten

    m_pedIK.bUseArm = CanWeRunAndFireWithWeapon();
}

/*!
* @addr 0x5E88E0
*/
bool CPed::CanWeRunAndFireWithWeapon() {
    return GetActiveWeapon().GetWeaponInfo(this).flags.bAimWithArm;
}

/*!
* @addr 0x5E8910
*/
void CPed::RequestDelayedWeapon() {
    if (m_nDelayedWeapon == WEAPON_UNIDENTIFIED) {
        return;
    }

    // Simplified a little using an array. Originally it had too much copy paste.

    const auto models = CWeaponInfo::GetWeaponInfo(m_nDelayedWeapon)->GetModels();

    // Request models
    for (auto model : models) {
        if (model != -1) {
            CStreaming::RequestModel(model, STREAMING_KEEP_IN_MEMORY);
        }
    }

    // If it has no model, or at least one model is loaded..
    if (rng::all_of(models, [](auto m) { return m == -1 || CStreaming::IsModelLoaded(m); })) {
        GiveWeapon(m_nDelayedWeapon, m_nDelayedWeaponAmmo, true);
        m_nDelayedWeapon = WEAPON_UNIDENTIFIED;
    }
}

/*!
* @addr 0x5E89B0
* @brief Set delayed weapon (If ped doesn't already have one). Also drops entity in hand unless ped is a player.
*/
void CPed::GiveDelayedWeapon(eWeaponType weaponType, uint32 ammo) {
    // If not a player drop entity in ped's hand (if any)
    if (!IsPlayer()) {
        if (const auto task = (CTaskSimpleHoldEntity*)GetIntelligence()->GetTaskHold(false)) {
            if (task->m_pEntityToHold) {
                if (task->m_nBoneFrameId == ePedNode::PED_NODE_RIGHT_HAND) {
                    DropEntityThatThisPedIsHolding(true);
                }
            }
        }
    }

    // Set delayed weapon (If ped doesn't already have one)
    if (m_nDelayedWeapon == WEAPON_UNIDENTIFIED) {
        m_nDelayedWeaponAmmo = ammo;
        m_nDelayedWeapon = weaponType;
        RequestDelayedWeapon();
    }
}

/*!
* @addr 0x5E8A30
*/
bool IsPedPointerValid(CPed* ped) {
    if(!IsPedPointerValid_NotInWorld(ped)) {
        return false;
    }

    if (ped->IsInVehicle()) {
        return IsEntityPointerValid(ped->m_pVehicle);
    }

    return (ped->m_pCollisionList.GetNodePtr() || ped == FindPlayerPed());
}

inline bool IsPedPointerValid_NotInWorld(CPed* ped) {
    return GetPedPool()->IsObjectValid(ped);
}

/*!
* @addr 0x5E8AB0
*/
void CPed::GiveWeaponAtStartOfFight()
{
    if (GetCreatedBy() != PED_MISSION && GetActiveWeapon().m_Type == WEAPON_UNARMED)
    {
        const auto GiveRandomWeaponByType = [this](eWeaponType type, uint16 maxRandom)
        {
            if ((m_nRandomSeed % 1024) >= maxRandom)
                return;

            if (m_nDelayedWeapon != WEAPON_UNIDENTIFIED)
                return;

            GiveDelayedWeapon(type, 50);
            SetCurrentWeapon(GetWeaponSlot(type));
        };

        switch (m_nPedType)
        {
        case PED_TYPE_GANG1:
        case PED_TYPE_GANG2:
        case PED_TYPE_GANG3:
        case PED_TYPE_GANG4:
        case PED_TYPE_GANG5:
        case PED_TYPE_GANG6:
        case PED_TYPE_GANG7:
        case PED_TYPE_GANG8:
        case PED_TYPE_GANG9:
        case PED_TYPE_GANG10:
            GiveRandomWeaponByType(WEAPON_PISTOL, 400);
            break;
        case PED_TYPE_DEALER:
        case PED_TYPE_CRIMINAL:
        case PED_TYPE_PROSTITUTE:
            GiveRandomWeaponByType(WEAPON_KNIFE, 200);
            GiveRandomWeaponByType(WEAPON_PISTOL, 400);
            break;
        default:
            break;
        }
    }
}

/*!
* @addr 5E8BE0
* @brief If ped has no weapons give them one. (AK-47 if `CHEAT_NO_ONE_CAN_STOP_US` is active, RLauncher if `CHEAT_ROCKET_MAYHEM` is active, or a pistol otherwise)
*/
void CPed::GiveWeaponWhenJoiningGang()
{
    if (GetActiveWeapon().m_Type == WEAPON_UNARMED && m_nDelayedWeapon == WEAPON_UNIDENTIFIED) {
        if (CCheat::IsActive(CHEAT_NO_ONE_CAN_STOP_US)) {
            GiveDelayedWeapon(WEAPON_AK47, 200);
            SetCurrentWeapon(CWeaponInfo::GetWeaponInfo(WEAPON_AK47, eWeaponSkill::STD)->m_nSlot);
        }
        else {
            CWeaponInfo* weaponInfo = nullptr;
            if (CCheat::IsActive(CHEAT_ROCKET_MAYHEM)) {
                GiveDelayedWeapon(WEAPON_RLAUNCHER, 200);
                weaponInfo = CWeaponInfo::GetWeaponInfo(WEAPON_RLAUNCHER, eWeaponSkill::STD);
            }
            else {
                GiveDelayedWeapon(WEAPON_PISTOL, 200);
                weaponInfo = CWeaponInfo::GetWeaponInfo(WEAPON_PISTOL, eWeaponSkill::STD);
            }
            SetCurrentWeapon(weaponInfo->m_nSlot);
        }
    }
}

/*!
* @addr 0x5EFF50
*/
bool CPed::GetPedTalking() {
    return m_pedSpeech.GetPedTalking();
}

/*!
* @addr 0x5EFF60
*/
void CPed::DisablePedSpeech(bool stopCurrentSpeech) {
    m_pedSpeech.DisablePedSpeech(stopCurrentSpeech);
}

/*!
* @addr 0x5EFF70
*/
void CPed::EnablePedSpeech() {
    m_pedSpeech.EnablePedSpeech();
}

/*!
* @addr 0x5EFF80
*/
void CPed::DisablePedSpeechForScriptSpeech(bool stopCurrentSpeech) {
    m_pedSpeech.DisablePedSpeechForScriptSpeech(stopCurrentSpeech);
}

/*!
* @addr 0x5EFF90
*/
void CPed::EnablePedSpeechForScriptSpeech() {
    m_pedSpeech.EnablePedSpeechForScriptSpeech();
}

/*!
* @addr 0x5EFFA0
*/
bool CPed::CanPedHoldConversation() const {
    return m_pedSpeech.CanPedHoldConversation();
}

/*!
* @addr 0x5EFFB0
*/
void CPed::SayScript(eAudioEvents scriptID, bool overrideSilence, bool isForceAudible, bool isFrontEnd) {
    m_pedSpeech.AddScriptSayEvent(AE_SCRIPT_SPEECH_PED, scriptID, overrideSilence, isForceAudible, isFrontEnd);
}

/*!
* @addr 0x5EFFE0
* @returns Played soundID - TODO: I'm not sure about this..
*/
int16 CPed::Say(eGlobalSpeechContext gCtx, uint32 startTimeDelay, float probability, bool overrideSilence, bool isForceAudible, bool isFrontEnd) {
    return gCtx != CTX_GLOBAL_NO_SPEECH
        ? m_pedSpeech.AddSayEvent(AE_SPEECH_PED, gCtx, startTimeDelay, probability, overrideSilence, isForceAudible, isFrontEnd)
        : -1;
}

/*!
* @addr 0x5F0060
*/
RwObject* SetPedAtomicVisibilityCB(RwObject* rwObject, void* data) {
    if (!data) {
        rwObjectSetFlags(rwObject, 0); // TODO: Figure out what the flag is
    }
    return rwObject;
}

/*!
* @addr 0x5F0140
* @brief Remove body part
* @todo See if it works with body parts other than the head
*/
void CPed::RemoveBodyPart(ePedNode pedNode, char localDir) {
    UNUSED(localDir);

    if (m_apBones[pedNode]->KeyFrame) {
        if (CLocalisation::ShootLimbs()) {
            bRemoveHead = true;
            m_nBodypartToRemove = pedNode;
        }
    } else {
        NOTSA_LOG_DEBUG("Trying to remove ped component");
    }
}

/*!
* @addr 0x5F0190
*/
void CPed::SpawnFlyingComponent(int32 arg0, char arg1)
{
    // NOP
}

/*!
* @addr 0x5F01A0
*/
/*!
* @addr 0x5F01A0
* @brief Check if line of sight bullet would hit the ped (Does a basic check of colpoint.point.z against head position)
* @returns 0, 1 - Yes , 2 - No. Always `1` if ped is falling.
*/
uint8 CPed::DoesLOSBulletHitPed(CColPoint& colPoint) {
    // TODO: Below is just a copy of the code in `IsPedHeadAbovePos` - A separate function should be made.
    RwV3d zero{}; // Placeholder - 0, 0, 0
    RwV3d headPos{};

    // TODO: Doesn't this just return the position of the matrix? Eg.: `BoneMatrix.pos` ?
    RwV3dTransformPoint(&headPos, &zero, GetBoneMatrix((eBoneTag)m_apBones[ePedNode::PED_NODE_HEAD]->BoneTag));

    if (m_nPedState == PEDSTATE_FALL || colPoint.m_vecPoint.z < headPos.z) { // Ped falling, adjust
        return 1;
    } else if (headPos.z + 0.2f <= colPoint.m_vecPoint.z) {
        return 0;
    } else {
        return 2;
    }
}

/*!
* @addr  0x5F0250
* @brief Remove all weapon animations by blending them into the IDLE animation.
* @param likeUnused Unused
*/
void CPed::RemoveWeaponAnims(int32 likeUnused, float blendDelta) {
    UNUSED(likeUnused);

    bool bFoundNotPartialAnim{};
    for (auto i = 0; i < 34; i++) { // TODO: Magic number `34`
        if (const auto assoc = RpAnimBlendClumpGetAssociation(GetRpClump(), ANIM_ID_FIRE)) {
            assoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
            if ((assoc->m_Flags & ANIMATION_IS_PARTIAL)) {
                assoc->m_BlendDelta = blendDelta;
            } else {
                bFoundNotPartialAnim = true;
            }
        }
    }

    if (bFoundNotPartialAnim) {
        CAnimManager::BlendAnimation(GetRpClump(), m_nAnimGroup, ANIM_ID_IDLE, -blendDelta);
    }
}

/*!
* @addr 0x5F02C0
* @returns If world space \a zPos is above ped's head
*/
bool CPed::IsPedHeadAbovePos(float zPos) {
    RwV3d zero{}; // Placeholder - 0, 0, 0
    RwV3d headPos{};

    // TODO: Doesn't this just return the position of the matrix? Eg.: `BoneMatrix.pos` ?
    RwV3dTransformPoint(&headPos, &zero, GetBoneMatrix((eBoneTag)m_apBones[ePedNode::PED_NODE_HEAD]->BoneTag));

    return zPos + GetPosition().z < headPos.z;
}

/*!
* @addr 0x5F0360
*/
void CPed::KillPedWithCar(CVehicle* car, float fDamageIntensity, bool bPlayDeadAnimation)
{
    if (const auto task = GetTaskManager().GetSimplestActiveTask()) {
        switch (task->GetTaskType()) {
        case TASK_SIMPLE_FALL:
        case TASK_SIMPLE_DIE:
            // Just remember the car (Unless we already have one, that's not a player's car)
            if (!m_pEntityIgnoredCollision || car->GetStatus() == STATUS_PLAYER) {
                m_pEntityIgnoredCollision = car;
            }
            return;
        case TASK_SIMPLE_DEAD:
            return;
        default:
            break;
        }
    }

    if (m_pContactEntity && m_pContactEntity->GetIsTypeVehicle()) {
        if (m_pContactEntity->AsVehicle()->m_nVehicleType == VEHICLE_TYPE_BOAT) {
            return;
        }
        if (IsPlayer()) {
            return;
        }
    } else if (m_pVehicle && m_pVehicle == car) {
        if (car->m_nVehicleType == VEHICLE_TYPE_BOAT) {
            return;
        }
        if (car->m_nVehicleSubType == VEHICLE_TYPE_PLANE) {
            return;
        }
    }

    // Ignore slow cars if we're attacking them
    if (const auto task = GetTaskManager().Find<CTaskComplexDestroyCarMelee>(); task && task->m_VehToDestroy == car) {
        if (car->m_vecMoveSpeed.SquaredMagnitude() < 0.0225f) {
            return;
        }
    }

    // Vector from the car to us
    CVector diff = GetPosition() - car->GetPosition();

    AnimationId animId    = ANIM_ID_NO_ANIMATION_SET;
    float       animSpeed = 1.0f;

    if (fDamageIntensity <= 12.0f || IsPlayer()) {
        // Light hit - Knock the ped over
        if (m_vecLastCollisionImpactVelocity.z >= -0.8f || fDamageIntensity <= 3.0f) {
            if (fDamageIntensity <= 6.0f) {
                return;
            }
            if (IsPlayer() && fDamageIntensity <= 10.0f) {
                return;
            }
        }

        bIsStanding = false;

        const int32 dir = GetLocalDirection(CVector2D{ -car->m_vecMoveSpeed.x, -car->m_vecMoveSpeed.y });

        float damage = 30.0f;
        if (IsPlayer() && car->m_nVehicleSubType == VEHICLE_TYPE_TRAIN) {
            damage = 150.0f;
        }

        CPedDamageResponseCalculator calculator{ car, damage, WEAPON_RAMMEDBYCAR, PED_PIECE_TORSO, false };
        CEventDamage                 event{ car, CTimer::GetTimeInMS(), WEAPON_RAMMEDBYCAR, PED_PIECE_TORSO, (uint8)dir, false, bInVehicle };

        animId = (AnimationId)(dir + ANIM_ID_KO_SKID_FRONT); // ANIM_ID_KO_SKID_FRONT, ANIM_ID_KD_LEFT.. etc (Based on the direction)

        if (event.AffectsPed(this)) {
            calculator.ComputeDamageResponse(this, event.m_damageResponse, true);

            event.m_nAnimGroup = ANIM_GROUP_DEFAULT;
            event.m_nAnimID    = animId;
            event.m_fAnimBlend = 8.0f;
            event.m_fAnimSpeed = 1.0f;
            if (bPlayDeadAnimation) {
                CAnimManager::BlendAnimation(GetRpClump(), ANIM_GROUP_DEFAULT, animId, 8.0f)->m_Speed = 1.0f;
                event.m_bAnimAdded = true;
            }

            GetEventGroup().Add(&event, false);

            if (!IsPlayer() || m_bHasHitWall || car->m_nVehicleSubType == VEHICLE_TYPE_TRAIN || m_vecLastCollisionImpactVelocity.z < -0.8f) {
                m_pEntityIgnoredCollision = car;
            }
            GetIntelligence()->m_collisionScanner.m_bAlreadyHitByCar = true;
        }
        bKnockedUpIntoAir = false;

        if (car->m_nVehicleSubType == VEHICLE_TYPE_TRAIN) {
            if (m_bHasHitWall) {
                m_vecMoveSpeed = CVector{};
            } else {
                // Remove the part of our speed that is in the direction of the train, and use some of the train's speed
                CVector dirOfTrain = car->m_vecMoveSpeed;
                dirOfTrain.Normalise();
                const float speedInDir = dirOfTrain.y * m_vecMoveSpeed.y + dirOfTrain.z * m_vecMoveSpeed.z + dirOfTrain.x * m_vecMoveSpeed.x;
                m_vecMoveSpeed -= dirOfTrain * speedInDir;
                m_vecMoveSpeed += car->m_vecMoveSpeed * 0.3f;
            }
        } else if (!m_bHasHitWall) {
            m_vecMoveSpeed = car->m_vecMoveSpeed * 0.75f;
        }
        m_vecMoveSpeed.z = 0.0f;

        if (CLocalisation::KnockDownPeds()) {
            car->m_vehicleAudio.AddAudioEvent(AE_PED_KNOCK_DOWN, 0.0f);
        }
        Say(CTX_GLOBAL_PAIN_LOW, 0, 1.0f, false, false, false);
    } else {
        // Heavy hit
        enum eHitType : uint8 {
            HIT_NONE = 0,
            HIT_HOOD = 1,
            HIT_PUSHED_AWAY = 2,
            HIT_SIDE_LEFT = 3,
            HIT_SIDE_RIGHT = 4,
        };

        const uint8 rnd = CGeneral::GetRandomNumber() & 3;
        auto        hitType = HIT_NONE;
        auto        weapon  = WEAPON_RAMMEDBYCAR;

        if (car == FindPlayerVehicle()) {
            // Rumble the pad
            const float invMass = 1.0f / car->m_fMass;
            const float shake   = std::min(car->m_vecMoveSpeed.Magnitude() * invMass * 200000.0f + 80.0f, 250.0f);
            const uint8 freq    = (uint8)(int32)shake;
            CPad::GetPad(0)->StartShake((int16)(40000 / freq), freq, 0);
        }

        bIsStanding = false;

        int32 dir = GetLocalDirection(CVector2D{ -car->m_vecMoveSpeed.x, -car->m_vecMoveSpeed.y });

        const auto* const carMat = &car->GetMatrix();
        const float sideDot = diff.z * carMat->GetRight().z + diff.y * carMat->GetRight().y + diff.x * carMat->GetRight().x;
        const float upDot   = diff.z * carMat->GetUp().z    + diff.y * carMat->GetUp().y    + diff.x * carMat->GetUp().x;

        bool isOverTheHood = false;
        bool pushAway      = false;

        if (car->m_nVehicleSubType == VEHICLE_TYPE_TRAIN) {
            hitType  = HIT_PUSHED_AWAY;
            pushAway = true;
        } else if (DotProduct(car->m_vecMoveSpeed, carMat->GetForward()) >= 0.0f) {
            if (car->GetColModel()->m_boundBox.m_vecMax.x * 0.99f < std::abs(sideDot)) {
                // Hit by the side of the car
                hitType = sideDot > 0.0f ? HIT_SIDE_RIGHT : HIT_SIDE_LEFT;

                const float fwdDot = diff.z * carMat->GetForward().z + diff.y * carMat->GetForward().y + diff.x * carMat->GetForward().x;
                if (std::abs(fwdDot) < car->GetColModel()->m_boundBox.m_vecMax.y * 0.85f) {
                    pushAway = true;
                }
            } else if ((rnd != 0 && (upDot <= 0.1f || rnd < 2)) || car->m_pHandlingData->m_bIsBig) {
                hitType  = HIT_PUSHED_AWAY;
                pushAway = true;
            } else {
                hitType       = HIT_HOOD;
                isOverTheHood = true;
            }
        }

        if (pushAway) {
            weapon         = WEAPON_RUNOVERBYCAR;
            m_vecMoveSpeed = car->m_vecMoveSpeed * 0.9f;
            m_vecMoveSpeed.z = 0.0f;
            if (dir == 1 || dir == 3) {
                dir = 2;
            }
        }

        if (isOverTheHood) {
            // Get thrown over the hood
            const auto& box  = car->GetColModel()->m_boundBox;
            const float maxY = box.m_vecMax.y;
            const float minY = box.m_vecMin.y;
            const float maxZ = box.m_vecMax.z;

            float length;  // [fStack_cc] "Length" of the car part we're going over
            float targetZ; // [fStack_b8]
            if (carMat->GetForward().z < -0.2f) { // Car is looking down
                const CVector top = car->GetPosition() + carMat->GetUp() * maxZ + carMat->GetForward() * minY;
                targetZ = top.z;
                length  = maxY - minY;
            } else if (carMat->GetForward().z <= 0.1f) { // Car is level
                const CVector top = car->GetPosition() + carMat->GetUp() * maxZ;
                targetZ = top.z;
                length  = car->GetColModel()->m_boundBox.m_vecMax.y;
            } else { // Car is looking up
                const CVector top = car->GetPosition() + carMat->GetUp() * maxZ + carMat->GetForward() * maxY;
                targetZ = top.z;
                length  = maxY;
                if (targetZ - GetPosition().z > 0.0f) {
                    // Lift ourselves up a bit
                    GetMatrix().GetPosition().z += (targetZ - GetPosition().z) * 0.5f;
                    targetZ = (targetZ - GetPosition().z) * 0.25f + targetZ;
                }
            }

            const float carSpeed = car->m_vecMoveSpeed.Magnitude();
            const float time     = length / carSpeed; // Time it takes the car to go over
            const float zSpeed   = (targetZ - GetPosition().z) / time;
            const float randMult = (float)(((double)(CGeneral::GetRandomNumber() & 0xFF) * 0.002 + 1.5) * (double)zSpeed);

            CVector newVel = car->m_vecMoveSpeed;
            newVel.Normalise();
            newVel *= randMult * 0.2f;
            newVel.z += randMult;
            m_vecMoveSpeed = newVel;

            // Turn the direction around (We're being thrown)
            dir += 2;
            if (dir > 3) {
                dir -= 4;
            }

            // Knock the bonnet off
            if (car->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
                if (const auto bonnet = car->AsAutomobile()->RemoveBonnetInPedCollision()) {
                    const auto& m = car->GetMatrix();
                    if (CGeneral::GetRandomNumber() & 1) {
                        bonnet->m_vecMoveSpeed = (m_vecMoveSpeed + m.GetRight() * 0.1f) + m.GetUp() * 0.5f;
                    } else {
                        bonnet->m_vecMoveSpeed = (m_vecMoveSpeed - m.GetRight() * 0.1f) + m.GetUp() * 0.5f;
                    }
                    bonnet->ApplyTurnForce(m.GetUp() * 10.0f, m.GetForward());
                }
            }

            diff = GetPosition() - car->GetPosition();

            if (CLocalisation::KnockDownPeds()) {
                car->m_vehicleAudio.AddAudioEvent(AE_PED_BOUNCE, 0.0f);
            }
        } else {
            if (CLocalisation::KnockDownPeds()) {
                car->m_vehicleAudio.AddAudioEvent(AE_PED_CRUNCH, 0.0f);
            }
        }

        if (car->m_pDriver) {
            CCrime::ReportCrime(m_nPedType == PED_TYPE_COP ? CRIME_KILL_COP_PED_WITH_CAR : CRIME_KILL_PED_WITH_CAR, this, car->m_pDriver);
        }

        CPedDamageResponseCalculator calculator{ car, 1000.0f, weapon, PED_PIECE_TORSO, false };
        CEventDamage                 event{ car, CTimer::GetTimeInMS(), weapon, PED_PIECE_TORSO, (uint8)dir, false, bInVehicle };

        const uint8 rnd2 = CGeneral::GetRandomNumber() & 3;
        switch (dir) {
        case 0:
            if (hitType == HIT_SIDE_LEFT) {
                animId = rnd2 < 2 ? ANIM_ID_KO_SKID_FRONT : ANIM_ID_KO_SPIN_R;
            } else if (hitType == HIT_SIDE_RIGHT && rnd2 >= 2) {
                animId = ANIM_ID_KO_SPIN_L;
            } else {
                animId = ANIM_ID_KO_SKID_FRONT;
            }
            break;
        case 1:
            animId = ANIM_ID_KD_LEFT;
            break;
        case 2:
            if (hitType == HIT_SIDE_LEFT && rnd2 >= 2) {
                animId = ANIM_ID_KD_LEFT;
            } else if (hitType == HIT_SIDE_RIGHT && rnd2 >= 2) {
                animId = ANIM_ID_KD_RIGHT;
            } else {
                animId = ANIM_ID_KO_SKID_BACK;
            }
            break;
        case 3:
            animId = ANIM_ID_KD_RIGHT;
            break;
        }

        if (!event.AffectsPed(this)) {
            bKnockedUpIntoAir = false;
        } else {
            float animBlend;
            if (weapon == WEAPON_RAMMEDBYCAR) {
                animBlend = car->m_vecMoveSpeed.Magnitude() * 8.0f + 4.0f;
            } else {
                animBlend = car->m_vecMoveSpeed.Magnitude() * 12.0f + 4.0f;
                animSpeed = (float)((double)car->m_vecMoveSpeed.Magnitude() * 16.0 + 1.0);
            }

            calculator.ComputeDamageResponse(this, event.m_damageResponse, true);

            event.m_nAnimGroup = ANIM_GROUP_DEFAULT;
            event.m_nAnimID    = animId;
            event.m_fAnimBlend = animBlend;
            event.m_fAnimSpeed = animSpeed;
            if (bPlayDeadAnimation) {
                CAnimManager::BlendAnimation(GetRpClump(), ANIM_GROUP_DEFAULT, animId, animBlend)->m_Speed = animSpeed;
                event.m_bAnimAdded = true;
            }

            GetEventGroup().Add(&event, false);

            if (!m_pEntityIgnoredCollision) {
                m_pEntityIgnoredCollision = car;
            }
            bKnockedUpIntoAir = hitType == HIT_HOOD;
            GetIntelligence()->m_collisionScanner.m_bAlreadyHitByCar = true;
        }

        Say(CTX_GLOBAL_PAIN_DEATH_HIGH, 0, 1.0f, false, false, false);
    }

    // Apply a force to the car (We're hitting it)
    const auto& carUp = car->GetMatrix().GetUp();
    const float upComp = diff.y * carUp.y + diff.z * carUp.z + diff.x * carUp.x;
    diff -= carUp * upComp;

    CVector dirToPed = diff;
    dirToPed.Normalise();

    const float speedInDir = dirToPed.y * car->m_vecMoveSpeed.y + dirToPed.z * car->m_vecMoveSpeed.z + dirToPed.x * car->m_vecMoveSpeed.x;
    if (!bKnockedUpIntoAir) {
        dirToPed.z -= 0.2f;
    }

    const float mult = car->m_nVehicleType == VEHICLE_TYPE_BIKE ? -0.75f : -0.5f;
    const CVector point = diff * 0.25f;

    const float massMult = std::min(1.0f, car->m_fMass / 1600.0f);
    const float totalForce = std::min(car->m_fMass, 1600.0f) * massMult * (speedInDir * mult);
    car->ApplyForce(
        CVector{ dirToPed.x * totalForce, dirToPed.y * totalForce, dirToPed.z * totalForce },
        point,
        true
    );
}

/*!
* @addr 0x6AE0D0
*/
template<typename PtrListType>
void CPed::MakeTyresMuddySectorList(PtrListType& ptrList)
{
    // NOTE: The original code doesn't reset these between iterations (they are only updated for cars and bikes),
    // meaning that a vehicle of another type (heli, boat, etc) will re-process the last car/bike.
    CAutomobile* car{};
    CBike*       bike{};

    for (auto* const entity : ptrList) {
        auto* const veh = entity->AsVehicle();
        if (veh->IsScanCodeCurrent()) {
            continue;
        }
        veh->SetCurrentScanCode();

        // Only process vehicles close enough to us
        const auto& pedPos = GetPosition();
        const auto& vehPos = veh->GetPosition();
        if (!(std::abs(pedPos.x - vehPos.x) < 10.0f)) {
            continue;
        }
        if (!(std::abs(pedPos.y - vehPos.y) < 10.0f)) {
            continue;
        }

        if (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
            car  = veh->AsAutomobile();
            bike = nullptr;
        } else if (veh->m_nVehicleType == VEHICLE_TYPE_BIKE) {
            car  = nullptr;
            bike = veh->AsBike();
        }

        // Skip slow vehicles
        if (veh->m_vecMoveSpeed.x * veh->m_vecMoveSpeed.x + veh->m_vecMoveSpeed.y * veh->m_vecMoveSpeed.y <= 0.05f) {
            continue;
        }

        if (car) {
            const auto& box = CModelInfo::GetModelInfo(car->m_nModelIndex)->GetColModel()->m_boundBox;
            for (size_t i = 0; i < 4; i++) {
                if (car->m_wheelSkidmarkBloodState[i] || !(car->m_fWheelsSuspensionCompression[i] < 1.0f)) {
                    continue;
                }

                // Offset of the wheel (in the car's space)
                CVector wheelOffset{};
                switch (i) {
                case 0: wheelOffset = CVector{ -box.m_vecMax.x, box.m_vecMax.y, 0.0f }; break;
                case 1: wheelOffset = CVector{ -box.m_vecMax.x, box.m_vecMin.y, 0.0f }; break;
                case 2: wheelOffset = CVector{  box.m_vecMax.x, box.m_vecMax.y, 0.0f }; break;
                case 3: wheelOffset = CVector{  box.m_vecMax.x, box.m_vecMin.y, 0.0f }; break;
                }
                const auto wheelPos = car->GetMatrix().TransformPoint(wheelOffset);

                if (!(std::abs(wheelPos.z - pedPos.z) < 2.0f)) {
                    continue;
                }
                if (!((wheelPos.y - pedPos.y) * (wheelPos.y - pedPos.y) + (wheelPos.x - pedPos.x) * (wheelPos.x - pedPos.x) < 1.0f)) {
                    continue;
                }

                if (CLocalisation::Blood()) {
                    car->m_wheelSkidmarkBloodState[i] = true;
                    car->m_vehicleAudio.AddAudioEvent(AE_PED_DRIVE_OVER, 0.0f);
                }

                if (car->m_fMass > 500.0f) {
                    car->ApplyMoveForce(0.0f, 0.0f, std::min(m_fMass * 0.001f, 1.0f) * 50.0f);
                    car->ApplyTurnForce(
                        CVector{ 0.0f, 0.0f, std::min(m_fTurnMass * 0.0005f, 1.0f) * 50.0f },
                        wheelPos - car->GetPosition()
                    );
                    if (car == FindPlayerVehicle()) {
                        CPad::GetPad(0)->StartShake(300, 70, 0);
                    }
                }
            }
        } else if (bike) {
            const auto& box = CModelInfo::GetModelInfo(bike->m_nModelIndex)->GetColModel()->m_boundBox;
            for (size_t i = 0; i < 2; i++) {
                // NOTE: Each wheel has 2 suspension lines
                if (bike->m_bWheelBloody[i] || !(bike->m_aWheelRatios[i * 2] < 1.0f)) {
                    continue;
                }

                const auto wheelOffset = CVector{ 0.0f, (i == 0 ? box.m_vecMax.y : box.m_vecMin.y) * 0.8f, 0.0f };
                const auto wheelPos    = bike->GetMatrix().TransformPoint(wheelOffset);

                if (!(std::abs(wheelPos.z - pedPos.z) < 2.0f)) {
                    continue;
                }
                if (!((wheelPos.y - pedPos.y) * (wheelPos.y - pedPos.y) + (wheelPos.x - pedPos.x) * (wheelPos.x - pedPos.x) < 1.0f)) {
                    continue;
                }

                if (CLocalisation::Blood()) {
                    bike->m_bWheelBloody[i] = true;
                    bike->m_vehicleAudio.AddAudioEvent(AE_PED_DRIVE_OVER, 0.0f);
                }

                if (bike->m_fMass > 100.0f) {
                    bike->ApplyMoveForce(0.0f, 0.0f, 10.0f);
                    bike->ApplyTurnForce(CVector{ 0.0f, 0.0f, 10.0f }, wheelPos - bike->GetPosition());
                    if (bike == FindPlayerVehicle()) {
                        CPad::GetPad(0)->StartShake(300, 70, 0);
                    }
                }
            }
        }
    }
}

/*!
* @addr  0x6B4200
* @brief Do sector list processing in a range of -/+2 (Calls MakeTyresMuddySectorList)
*/
void CPed::DeadPedMakesTyresBloody() {
    const auto& pos = GetPosition();
    const auto startSectorX = std::max(CWorld::GetLodSectorX(pos.x - 2.0f), 0);
    const auto startSectorY = std::max(CWorld::GetLodSectorY(pos.y - 2.0f), 0);
    const auto endSectorX   = std::min(CWorld::GetLodSectorX(pos.x + 2.0f), MAX_LOD_PTR_LISTS_X - 1);
    const auto endSectorY   = std::min(CWorld::GetLodSectorY(pos.y + 2.0f), MAX_LOD_PTR_LISTS_Y - 1);

    CWorld::AdvanceCurrentScanCode();

    for (int32 sy = startSectorY; sy <= endSectorY; ++sy) {
        for (int32 sx = startSectorX; sx <= endSectorX; ++sx) {
            MakeTyresMuddySectorList(CWorld::GetRepeatSector(sx, sy).Vehicles);
        }
    }
}

/*!
* @notsa
* @returns If player is in a vehicle that has a driver as a passenger
*/
bool CPed::IsInVehicleThatHasADriver() {
    if (bInVehicle) { // todo: IsInVehicleAsPassenger - Before refactoring check if `IsPassanger` returns true if `this` is the driver.
        if (m_pVehicle && m_pVehicle->IsPassenger(this) && m_pVehicle->m_pDriver)
            return true;
    }
    return false;
}

/*!
* @notsa
*/
CPedGroup* CPed::GetGroup() const {
    return CPedGroups::GetPedsGroup(this);
}

/*!
* @notsa
*/
int32 CPed::GetGroupId() {
    return GetGroup()
        ? GetGroup()->GetId()
        : -1;
}

/*!
* @notsa
* @returns If ped is follower of \a group
*/
bool CPed::IsFollowerOfGroup(const CPedGroup& group) const {
    return group.GetMembership().IsFollower(this);
}

/*!
* @addr 0x5E4880
* @brief Set model index (Also re-inits animblend, MoneyCount, and default decision-marker)
*/
void CPed::SetModelIndex(uint32 modelIndex) {
    assert(modelIndex != MODEL_PLAYER || IsPlayer());

    SetIsVisible(true);

    CEntity::SetModelIndex(modelIndex);

    RpAnimBlendClumpInit(GetRpClump());
    RpAnimBlendClumpFillFrameArray(GetRpClump(), m_apBones.data());

    auto* mi = GetModelInfo()->AsPedModelInfoPtr();

    SetPedStats(mi->m_nStatType);
    RestoreHeadingRate();

    SetPedDefaultDecisionMaker();

    // Set random money count
    const auto GetRandomMoneyCount = [this]() -> int16 {
        if (CGeneral::GetRandomNumberInRange(0.f, 100.f) < 3.f) { // Moved up here
            return 400;
        } else if (CPopCycle::IsPedInGroupTheseGroups(m_nModelIndex, { POPCYCLE_GROUP_BUSINESS, POPCYCLE_GROUP_CASUAL_RICH })) {
            return CGeneral::GetRandomNumber() % 50 + 20;
        } else {
            return CGeneral::GetRandomNumber() % 25;
        }
    };
    m_nMoneyCount = GetRandomMoneyCount();

    m_nAnimGroup = mi->m_nAnimType;
    CAnimManager::AddAnimation(GetRpClump(), m_nAnimGroup, ANIM_ID_IDLE);

    if (!CanUseTorsoWhenLooking()) {
        m_pedIK.bTorsoUsed = true;
    }

    // Deal with animation stuff once again
    RpAnimBlendClumpGetData(GetRpClump())->m_PedPosition = (CVector*)&m_vecAnimMovingShiftLocal;

    // Create hit col model
    if (!mi->m_pHitColModel) {
        mi->CreateHitColModelSkinned(GetRpClump());
    }

    // And finally update our rph anim
    UpdateRpHAnim();
}

/*!
* @addr 0x5DEBF0
*/
void CPed::DeleteRwObject()
{
    CEntity::DeleteRwObject();
}

/*!
* @addr 0x5E8CD0
*/
void CPed::ProcessControl()
{
    // x87 keeps the (x*x + y*y) + z*z sum in extended precision (0x406DA0 returns it in ST0 without a sqrt)
    const auto SquaredMagnitudeExt = [](const CVector& v) -> double {
        return ((double)v.x * (double)v.x + (double)v.y * (double)v.y) + (double)v.z * (double)v.z;
    };

    m_pedAudio.Service(); // 0x4E2EE0

    // Molotov fire effect in hand
    if (m_nPedType == PED_TYPE_PLAYER1 || m_nPedType == PED_TYPE_PLAYER2) {
        if (auto& wep = GetActiveWeapon(); wep.m_Type == WEAPON_MOLOTOV) {
            if (!bDontRender
                && m_bIsVisible
                && (!physicalFlags.bSubmergedInWater || !GetIntelligence()->GetTaskSwim())
                && m_pWeaponObject
            ) {
                if (!wep.m_FxSystem) {
                    wep.m_FxSystem = g_fxMan.CreateFxSystem("molotov_flame", CVector{ 0.f, 0.f, 0.f }, GetBoneMatrix(eBoneTag::BONE_R_HAND), false);
                    if (const auto fx = wep.m_FxSystem) {
                        fx->SetLocalParticles(true);
                        fx->CopyParentMatrix();
                        fx->Play();
                    }
                }
            } else if (wep.m_FxSystem) {
                g_fxMan.DestroyFxSystem(wep.m_FxSystem);
                wep.m_FxSystem = nullptr;
            }
        }
    }

    if (bUsedForReplay) {
        return;
    }

    if (bPartOfAttackWave) {
        if (CGame::currArea != AREA_CODE_NORMAL_WORLD) {
            return;
        }
        if (FindPlayerCoors().z > 950.f) { // 0x858F4C
            return;
        }
    }

    if (((uint8)m_nRandomSeed + CTimer::m_FrameCounter & 0x1F) == 0) {
        PruneReferences();
    }

    {
        auto alpha = CVisibilityPlugins::GetClumpAlpha(GetRpClump());
        if (bFadeOut) {
            alpha = std::max(alpha - 8, 0);
        } else if (alpha < 255) {
            alpha = std::min(alpha + 16, 255);
        }
        CVisibilityPlugins::SetClumpAlpha(GetRpClump(), alpha);
    }

    // The value of the `field_588` that means "no ground Z" (99999.99f)
    constexpr auto NO_GROUND_Z = std::bit_cast<float>(0x47C34FFFu);

    if (bKnockedOffBike
        && (bIsStanding || bIsDrowning)
        && !m_standingOnEntity
        && !bHeadStuckInCollision
        && std::bit_cast<uint32>(field_588) == std::bit_cast<uint32>(NO_GROUND_Z)
        && SquaredMagnitudeExt(m_vecMoveSpeed) < (double)0.01f // 0x858C58
    ) {
        if (m_pVehicle) {
            static auto& colPoints = StaticRef<std::array<CColPoint, 32>>(0xC092A8);
            if (!CCollision::ProcessColModels(
                GetMatrix(), *GetModelInfo()->GetColModel(),
                m_pVehicle->GetMatrix(), *m_pVehicle->GetModelInfo()->GetColModel(),
                colPoints, nullptr, nullptr, false
            )) {
                bKnockedOffBike = false;
                m_pEntityIgnoredCollision = nullptr;
            }
        } else {
            bKnockedOffBike = false;
        }
    }

    if (bHeadStuckInCollision && bCheckColAboveHead) {
        if ((double)GetPosition().z + (double)1.5f < (double)field_588) { // 0x858CE8
            bHeadStuckInCollision = false;
        }
    }

    bWasStanding = false;
    bFiringWeapon = false;
    bIsDrowning = false;
    physicalFlags.bSubmergedInWater = false;
    bDonePositionOutOfCollision = false;
    bPushOtherPeds = false;
    bCheckColAboveHead = false;
    bPedHitWallLastFrame = false;
    bTestForShotInVehicle = false;
    field_588 = NO_GROUND_Z;

    // Decay of the gunflash alphas
    const auto DecayGunflashAlpha = [](int16& alpha, int16 prog) {
        if (alpha <= 0) {
            return;
        }
        const double x = (double)CTimer::ms_fTimeStep * (double)0.02f * (double)1000.f; // 0x858B38, 0x858C4C
        const auto   d = (int32)x; // _ftol
        const auto   decrement = (uint32)(int32)prog * (uint32)d;
        if ((uint32)(int32)alpha <= decrement) {
            alpha = 0;
        } else {
            alpha = (int16)((uint32)(int32)alpha - decrement);
        }
    };
    DecayGunflashAlpha(m_nWeaponGunflashAlphaMP1, m_nWeaponGunFlashAlphaProgMP1);
    DecayGunflashAlpha(m_nWeaponGunflashAlphaMP2, m_nWeaponGunFlashAlphaProgMP2);

    if (!bIsStanding) {
        bHeadStuckInCollision = false;
    }

    if (m_nCreatedBy == PED_MISSION && FindPlayerPed() != this) {
        if (FindPlayerPed()->GetPlayerGroup().m_groupMembership.IsMember(this)) { // 0x5F6A10
            bCheckColAboveHead = false;
        }
    }

    ProcessBuoyancy(); // 0x5E1FA0

    if (const auto pd = m_pPlayerData) {
        if (physicalFlags.bSubmergedInWater && pd->m_nWaterCoverPerc > 50) {
            if ((int8)pd->m_nWetness < 100) {
                pd->m_nWetness++;
            }
        } else {
            if ((int8)pd->m_nWetness > 0) {
                pd->m_nWetness--;
            }
        }
    }

    if (bIsStanding && !CWorld::bForceProcessControl && m_standingOnEntity) {
        if (m_standingOnEntity->m_bIsInSafePosition
            || (m_standingOnEntity->GetIsTypeVehicle() && m_standingOnEntity->AsVehicle()->m_nVehicleSubType == VEHICLE_TYPE_TRAIN)
        ) {
            m_bWasPostponed = true;
            return;
        }
    }

    GetIntelligence()->ProcessFirst(); // 0x6073A0

    const bool bWasStandingAtStart = bIsStanding;
    if (!bWasStandingAtStart && m_vecMoveSpeed.z > 0.25) { // 0x862E38 (double)
        if (m_pPlayerData) {
            m_vecMoveSpeed.z = 0.25f;
        } else {
            const auto scale = (float)std::pow(0.949999988079071, (double)CTimer::ms_fTimeStep); // 0x86C3C8 (double), _CIpow
            m_vecMoveSpeed = m_vecMoveSpeed * scale;
        }
    }

    if (   m_nPedType == PED_TYPE_PLAYER1
        || m_nPedType == PED_TYPE_PLAYER2
        || !bWasStandingAtStart
        || m_vecMoveSpeed.x != 0.f
        || m_vecMoveSpeed.y != 0.f
        || m_vecMoveSpeed.z != 0.f
        || (m_nMoveState != PEDMOVE_STILL && m_nMoveState != PEDMOVE_NONE)
        || m_vecAnimMovingShiftLocal.x != 0.f
        || m_vecAnimMovingShiftLocal.y != 0.f
        || m_nPedState == PEDSTATE_JUMP
        || bIsInTheAir
        || m_standingOnEntity
    ) {
        CPhysical::ProcessControl(); // 0x5485E0
    } else {
        CPhysical::SkipPhysics(); // 0x5433B0
    }

    RequestDelayedWeapon(); // 0x5E8910
    PlayFootSteps();        // 0x5E57F0
    bTestForBlockedPositions = false;
    bFallenDown = false;
    GetIntelligence()->Process(); // 0x608260
    if (m_nPedState != PEDSTATE_DEAD) {
        CalculateNewVelocity(); // 0x5E4C50
    }
    UpdatePosition(); // 0x5E1B10
    SetMoveAnim();    // vtable +0x5C
    bRightArmBlocked = false;
    bLeftArmBlocked = false;
    bDuckRightArmBlocked = false;
    bMidriffBlockedForJump = false;

    // Bleeding: leave blood puddles
    if ((bPedIsBleeding || field_72F) && CLocalisation::Blood() && !bInVehicle) {
        if (field_72F) {
            field_72F--;
        }
        if ((CTimer::m_FrameCounter & 3) == 0) {
            const auto& camPos = TheCamera.GetPosition();
            const auto  diff   = GetPosition() - camPos;
            if (SquaredMagnitudeExt(diff) < (double)2500.f) { // 0x8598B0
                const auto size = (float)((double)(rand() & 0x7F) * (double)0.0015f + (double)0.15f); // 0x863E38, 0x858FCC

                CVector pos;
                pos.x = (float)((double)((rand() & 0x7F) - 0x40) * (double)0.007f + (double)GetPosition().x); // 0x86C3C0
                pos.y = (float)((double)((rand() & 0x7F) - 0x40) * (double)0.007f + (double)GetPosition().y);
                pos.z = (float)((double)GetPosition().z + (double)1.f); // 0x858624

                CShadows::AddPermanentShadow(
                    SHADOW_DEFAULT, // 1
                    gpBloodPoolTex,
                    &pos,
                    size, 0.f,
                    0.f, -size,
                    255, 200, 0, 0,
                    4.f,
                    (rand() & 0xFFF) + 2000,
                    1.f
                );
            }
        }
    }

    if (bInVehicle) {
        if (m_pVehicle) {
            CPopulation::UpdatePedCount(this, true);
        } else {
            bInVehicle = false;
        }
    } else {
        CPopulation::UpdatePedCount(this, false);
    }

    if (((uint16)m_nRandomSeed + CTimer::m_FrameCounter & 0x3FFF) == 0 && bDruggedUp) {
        Say(CTX_GLOBAL_DRUGGED_CHAT, 0, 1.f, false, false, false); // 0x51
    }

    if (GetActiveWeapon().m_Type == WEAPON_CHAINSAW && m_nPedState != PEDSTATE_ATTACK && !bInVehicle && !GetIntelligence()->GetTaskSwim()) {
        m_weaponAudio.AddAudioEvent(AE_WEAPON_CHAINSAW_IDLE); // 0x4E69F0
    }
    m_weaponAudio.Service(); // 0x4E6AE0
}

/*!
* @addr 0x5E4110
* @brief Set player's position (Also resets move/turn speed, and cancels all tasks)
*/
void CPed::Teleport(CVector destination, bool resetRotation) {
    UNUSED(resetRotation);

    if (IsPlayer() || GetTaskManager().HasAnyOf<TASK_COMPLEX_LEAVE_CAR>()) {
        GetIntelligence()->FlushImmediately(true);
    }

    CWorld::Remove(this);
    SetPosn(destination);
    bIsStanding = false;
    ClearReference(m_pDamageEntity);
    // todo: m_pDamageEntity = nullptr;
    CWorld::Add(this);

    m_vecMoveSpeed.Reset();
    m_vecTurnSpeed.Reset();
}

/*!
* @addr 0x5E3C30
*/
void CPed::SpecialEntityPreCollisionStuff(CPhysical* colPhysical,
                                          bool  bIgnoreStuckCheck,
                                          bool& bCollisionDisabled,
                                          bool& bCollidedEntityCollisionIgnored,
                                          bool& bCollidedEntityUnableToMove,
                                          bool& bThisOrCollidedEntityStuck)
{
    // Returns whether to set `bSkipLineCol` (0x1000 in physical flags) afterwards
    const auto Process = [&]() -> bool {
        // Ped (knocked off bike) vs. the vehicle it was knocked off
        if (colPhysical->GetIsTypeVehicle() && bKnockedOffBike && m_pVehicle == colPhysical) {
            bCollisionDisabled = true;
            return false;
        }

        if (m_pEntityIgnoredCollision == colPhysical || colPhysical->m_pEntityIgnoredCollision == this) {
            bCollidedEntityCollisionIgnored = true;
            return !(bKnockedUpIntoAir && !bKnockedOffBike);
        }

        if (m_pAttachedTo == colPhysical || colPhysical->m_pAttachedTo == this) {
            bCollisionDisabled = true;
            return false;
        }
        if (m_pAttachedTo && colPhysical->m_pAttachedTo) {
            bCollisionDisabled = true;
            return false;
        }

        if (colPhysical->physicalFlags.bDisableMoveForce) {
            if (!colPhysical->physicalFlags.bDisableCollisionForce && !colPhysical->physicalFlags.bDoorHitEndStop) {
                if (bIgnoreStuckCheck) {
                    bCollisionDisabled = true;
                } else if (GetIsStuck() || colPhysical->GetIsStuck()) {
                    bThisOrCollidedEntityStuck = true;
                }
            } else {
                bCollidedEntityUnableToMove = true;
            }
            return true;
        }

        if (colPhysical->physicalFlags.bInfiniteMass || colPhysical->physicalFlags.bDisableZ) {
            if (bIgnoreStuckCheck) {
                bCollidedEntityCollisionIgnored = true;
            } else if (GetIsStuck() || colPhysical->GetIsStuck()) {
                bThisOrCollidedEntityStuck = true;
            }
            return true;
        }

        if (colPhysical->GetIsTypeObject()) {
            const auto obj = colPhysical->AsObject();
            if (obj->objectFlags.bIsLampPost && obj->GetUp().z < 0.66f) {
                bCollidedEntityCollisionIgnored = true;
                return true;
            }
            if (obj->m_nModelIndex == MODEL_GRENADE && obj->GetPosition().z < GetPosition().z) {
                bCollidedEntityCollisionIgnored = true;
                return true;
            }
            if (obj->m_pObjectInfo->m_fUprootLimit > 0.0f || obj->physicalFlags.bDisableCollisionForce) {
                if (std::abs(obj->m_vecMoveSpeed.x) < 0.001f
                    && std::abs(obj->m_vecMoveSpeed.y) < 0.001f
                    && std::abs(obj->m_vecMoveSpeed.z) < 0.001f
                ) {
                    bCollidedEntityUnableToMove = true;
                    return false;
                }
            }
            if (obj->GetIsStuck()) {
                bCollidedEntityUnableToMove = true;
            }
            return false;
        }

        const auto mi = colPhysical->m_nModelIndex;
        if (mi == MODEL_RCBANDIT || mi == MODEL_RCTIGER || mi == MODEL_RCCAM) {
            bCollidedEntityCollisionIgnored = true;
            return true;
        }
        if (colPhysical->GetIsStuck()) {
            bCollidedEntityUnableToMove = true;
        }
        return false;
    };
    if (Process()) {
        physicalFlags.bSkipLineCol = true; // 0x1000
    }

    if (m_nPedType == PED_TYPE_PLAYER1 || m_nPedType == PED_TYPE_PLAYER2) {
        if (GetIntelligence()->GetTaskClimb()) {
            physicalFlags.bSkipLineCol = true;
        }
    }
}

/*!
* @addr 0x5E3E90
*/
uint8 CPed::SpecialEntityCalcCollisionSteps(bool& bProcessCollisionBeforeSettingTimeStep, bool& unk2)
{
    if (m_pAttachedTo) { // 0xFC
        return 1;
    }

    const double ts = CTimer::GetTimeStep();

    // The original keeps everything on the x87 stack (extended precision) up to the final stores
    const double sqMoveSpeed = (double)m_vecMoveSpeed.x * m_vecMoveSpeed.x + (double)m_vecMoveSpeed.y * m_vecMoveSpeed.y + (double)m_vecMoveSpeed.z * m_vecMoveSpeed.z;

    if (!m_pPlayerData) {
        // 0x5E3EE5: `TEST DL, AH` after FCOMP => taken if less than OR unordered
        if (!(sqMoveSpeed * ts * ts >= 0.09f)) {
            return 1;
        }
    }

    const double moveSpeed = std::sqrt(sqMoveSpeed) * ts;

    int32 steps;
    if (!m_pPlayerData) {
        steps = (int32)std::ceil(moveSpeed * 5.0f); // 0x5E3FA8 (the product is stored as a double)
    } else {
        // 0x5E3F2B..: `FST float [tmp]` keeps a float-rounded copy, but the argument of the first `ceil`
        // is `FSTP double` of the unrounded product. The clamp decision is made on that first ceil;
        // when it is not taken, `ceil` is called AGAIN with the float-rounded copy and that result is used.
        const auto Calc = [&](float mult, double minSteps) {
            const double product = moveSpeed * mult;
            const double ceiled0 = std::ceil(product);
            // FCOMP + `TEST AH, 5` + JP => take the clamp only if ceiled0 < minSteps (ordered)
            if (ceiled0 < minSteps) {
                return (int32)minSteps;
            }
            return (int32)std::ceil((double)(float)product);
        };
        steps = m_standingOnEntity
            ? Calc(6.6666665f, 4.0)
            : Calc(3.3333333f, 2.0);
    }

    if (!m_pPlayerData) {
        m_fElasticity += m_fElasticity;
    }

    return (uint8)steps;
}

/*!
* @addr 0x5E8A20
*/
void CPed::PreRender()
{
    if (m_nPedState != PEDSTATE_DRIVING)
        PreRenderAfterTest();
}

/*!
* @addr 0x5E7680
*/
void CPed::Render() {
    // Save alpha fn. ref for player peds
    uint32 storedAlphaRef{1};
    if (IsPlayer()) {
        RwRenderStateGet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(&storedAlphaRef));
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(1u));
    }

    // 0x5E76BE
    if (bDontRender || !(GetIsVisible() || CMirrors::ShouldRenderPeds())) {
        return;
    }

    // 0x5E76F9 - 0x5E7735
    // Now do some extra checks if in vehicle (possibly early out)
    if (   bInVehicle
        && m_pVehicle
        && !GetTaskManager().FindActiveTaskFromList({ TASK_COMPLEX_LEAVE_CAR, TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT_AND_STAND_UP })
    ) {
        // 0x5E774A
        if (!bRenderPedInCar) {
            return;
        }

        // 0x5E7765 - 0x5E7774
        if (   !m_pVehicle->IsBike()
            && !m_pVehicle->IsSubQuad()
            && !IsPlayer()
        ) {
            // 0x5E77DD
            // Okay, let's check if the ped is close enough to the camera

            const auto IsPedInRangeOfCamera = [this](auto range) {
                const auto distSq = (TheCamera.GetPosition() - GetPosition()).SquaredMagnitude();
                const auto finalRange = range * TheCamera.m_fLODDistMultiplier;
                return distSq < finalRange * finalRange;
            };
            if (!IsPedInRangeOfCamera(m_pVehicle->IsBoat() ? 40.f : 25.f)) { // Boats have bigger range
                return;
            }
        }
    }

    RenderBigHead();
    RenderThinBody();

    // 0x5E77E3
    // Render us (And any extra FX)
    if (CPostEffects::IsVisionFXActive()) {
        CPostEffects::InfraredVisionStoreAndSetLightsForHeatObjects(this);
        CPostEffects::NightVisionSetLights();
        CEntity::Render();
        CPostEffects::InfraredVisionRestoreLightsForHeatObjects();
    } else {
        CEntity::Render();
    }

    // 0x5E7817
    // Render weapon (and gun flash) as well. (Done for local player only if flag is set.)
    if (m_pWeaponObject) {
        if (!GetPlayerData() || GetPlayerData()->m_bRenderWeapon) {
            if ((!bInVehicle || !GetIntelligence()->GetTaskSwim()) && !GetIntelligence()->GetTaskHold(false)) {
                CVisibilityPlugins::AddWeaponPedForPC(this);
                if (m_nWeaponGunflashAlphaMP1 > 0 || m_nWeaponGunflashAlphaMP2 > 0) {
                    ResetGunFlashAlpha();
                }
            }
        }
    }

    // 0x5E787C
    // Render goggles object
    if (m_pGogglesObject) {
        const auto* const headMat = GetBoneMatrix(eBoneTag::BONE_HEAD);

        // Update goggle's matrix with head's
        *RwFrameGetMatrix(RpClumpGetFrame(m_pGogglesObject)) = *headMat; // TODO: Is there a better way to do this?

        // Calculate it's new position
        RwV3d pos{0.f, 0.084f, 0.f};                  // Offset
        RwV3dTransformPoints(&pos, &pos, 1, headMat); // Transform offset into the head's space

        RwV3dAssign(RwMatrixGetPos(RwFrameGetMatrix(RpClumpGetFrame(m_pGogglesObject))), &pos);
        RwFrameUpdateObjects(RpClumpGetFrame(m_pGogglesObject)); // After changing the position it has to be updated

        RpClumpRender(m_pGogglesObject);
    }

    // 0x5E7927
    // Render JetPack (if any)
    if (const auto task = GetIntelligence()->GetTaskJetPack()) {
        task->RenderJetPack(this);
    }

    bHasBeenRendered = true;

    // 0x5E794D
    // Restore alpha test fn for player peds
    if (IsPlayer()) {
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(storedAlphaRef));
    }
}

// https://github.com/gennariarmando/bobble-heads
// NOTSA
void CPed::RenderBigHead() const {
    if (!CCheat::IsActive(CHEAT_BIG_HEAD)) {
        return;
    }

    auto hier = GetAnimHierarchyFromSkinClump(GetRpClump());
    auto* matrices = RpHAnimHierarchyGetMatrixArray(hier);

    const float scale = 3.0f;
    const CVector s = { scale, scale, scale };
    CVector t = { 0.0f, -(scale / 6.0f) / 10.0f, 0.0f };

    for (auto& bone : { BONE_L_BROW, BONE_R_BROW, BONE_JAW }) {
        auto index = RpHAnimIDGetIndex(hier, bone);
        if (RwMatrix* mat = &matrices[index]) {
            RwMatrixScale(mat, &s, rwCOMBINEPRECONCAT);
            if (bone == BONE_JAW) {
                t.x = ((scale / 8.0f) / 10.0f) / 8.0f;
                t.y /= 8.0f;
            }
            RwMatrixTranslate(mat, &t, rwCOMBINEPRECONCAT);
        }
    }

    auto index = RpHAnimIDGetIndex(hier, BONE_HEAD);
    if (RwMatrix* mat = &matrices[index]) {
        RwMatrixScale(mat, &s, rwCOMBINEPRECONCAT);
    }
}

bool CPed::CanBeCriminal() const {
    if (IsPlayer() || IsCreatedBy(PED_MISSION)) {
        return false;
    }

    switch (m_nPedType) {
    case PED_TYPE_COP:
    case PED_TYPE_MEDIC:
    case PED_TYPE_FIREMAN:
    case PED_TYPE_MISSION1:
    case PED_TYPE_MISSION2:
    case PED_TYPE_MISSION3:
    case PED_TYPE_MISSION4:
    case PED_TYPE_MISSION5:
    case PED_TYPE_MISSION6:
    case PED_TYPE_MISSION7:
    case PED_TYPE_MISSION8:
        return false;
    }

    return true;
}

// NOTSA
void CPed::RenderThinBody() const {
    if (!CCheat::IsActive(CHEAT_THIN_BODY)) {
        return;
    }
}

/*!
 * @addr 0x553F00
 */
bool CPed::SetupLighting() {
  ActivateDirectional();
  return CRenderer::SetupLightingForEntity(this);
}

/*!
* @addr 0x5533B0
*/
void CPed::RemoveLighting(bool bRemove) {
    UNUSED(bRemove);

    if (!physicalFlags.bRenderScorched) {
        CPointLights::RemoveLightsAffectingObject();
    }

    SetAmbientColours();
    DeActivateDirectional();
}

/*!
* @addr 0x5E7B70
*/
void CPed::FlagToDestroyWhenNextProcessed() {
    m_bRemoveFromWorld = true;

    if (!IsInVehicle()) {
        return;
    }

    if (m_pVehicle->IsDriver(this)) {
        ClearReference(m_pVehicle->m_pDriver);
        if (IsPlayer() && m_pVehicle->GetStatus() != STATUS_WRECKED) {
            m_pVehicle->SetStatus(STATUS_ABANDONED);
        }
    } else {
        m_pVehicle->RemovePassenger(this);
    }

    bInVehicle = false;

    if (IsVehiclePointerValid(m_pVehicle)) {
        SafeCleanUpRef(m_pVehicle);
    }

    m_pVehicle = nullptr;

    SetPedState(IsCreatedByMission() ? PEDSTATE_DEAD : PEDSTATE_NONE);
}

/*!
* @addr 0x5E2530
*/
int32 CPed::ProcessEntityCollision(CEntity* entity, CColPoint* colPoint) {
    // Constants from .rdata
    constexpr float STEP_DOWN_MULT          = -0.15f;      // 0x86C330
    constexpr float STEEP_NORMAL_Z          = -0.867f;     // 0x86C32C
    constexpr float NO_CEILING_Z            = 99999.99f;   // 0x86C328
    constexpr float FALL_ANIM_Z_MULT        = -0.016f;     // 0x86C320
    constexpr float SOFT_LANDING_SPEED      = 0.375f;      // 0x86C324
    constexpr float FALL_SPEED_XY           = 0.33f;       // 0x86C290
    constexpr float FALL_SPEED_Z            = -0.25f;      // 0x86C294
    constexpr float FALL_DMG_MULT_XY        = 100.0f;      // 0x86C298
    constexpr float FALL_DMG_MULT_Z         = 400.0f;      // 0x86C29C
    constexpr float FALL_DEADLY_SPEED_Z     = -0.6f;       // 0x86C2A0
    constexpr float MOVE_THRESHOLD          = 0.01f;       // 0x858C58 / -0x86C310
    constexpr float WALL_NORMAL_Z           = -0.99f;      // 0x86C314
    constexpr double SAVED_HEADING_THRESHOLD = -1000.0;    // 0x86C318 (double)
    constexpr float CONTACT_LIGHT_SCALE     = 0.033333335f; // 0x858F10
    constexpr float EXTRA_POINT_DIST        = 0.35f;       // 0x858F9C

    const float stepDown = CTimer::GetTimeStep() * STEP_DOWN_MULT; // 0x86C330

    float           lineTouchDists[2]{ 1.0f, 1.0f };
    CColPoint       lineCPs[2];
    CColModel*      colModel = CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel();
    CCollisionData* colData  = colModel->m_pColData;
    CVector         extraNormal{ 0.0f, 0.0f, 1.0f };
    bool            bEntityIsBoat = false;
    bool            bDoLineTest   = false;
    float           sphereTop     = 0.94f;
    float           savedHeading  = -1001.0f;

    // Peds that test for blocked positions use a special col model (with extra spheres)
    if (bTestForBlockedPositions) {
        if (m_bUsesCollision) {
            if ((m_nPedType == PED_TYPE_PLAYER1 || m_nPedType == PED_TYPE_PLAYER2)
                && TheCamera.m_aCams[TheCamera.m_nActiveCam].m_nMode == MODE_AIMWEAPON // 0x35
            ) {
                const auto* const gun = GetIntelligence()->GetTaskUseGun(); // 0x600F70
                if (gun && gun->m_WeaponInfo && gun->m_WeaponInfo->flags.bAimWithArm) {
                    savedHeading = GetHeading(); // 0x441DB0
                    const auto& camFront = TheCamera.m_aCams[TheCamera.m_nActiveCam].m_vecFront;
                    SetHeading((float)std::atan2((double)-camFront.x, (double)camFront.y)); // 0x43E0C0
                }
            }
            colModel = &CTempColModels::ms_colModelPed2; // 0x968E20
            colData  = colModel->m_pColData;
        }
    }

    if (!m_bUsesCollision && !physicalFlags.bForceHitReturnFalse) { // 0x42, 0x1
        return 0;
    }

    const auto otherType = entity->GetType();
    if (otherType == ENTITY_TYPE_VEHICLE && entity->AsVehicle()->m_nVehicleType == VEHICLE_TYPE_BOAT) {
        bEntityIsBoat = true;
    }

    // NOTE: For non-physical entities the original reads the flags at +0x40 anyways
    const auto GetEntityPhysFlags = [](CEntity* e) { return e->AsPhysical()->m_nPhysicalFlags; };

    // Set up the col model for the ground checks
    if (!(m_nPhysicalFlags & 0x19000) && !m_pAttachedTo && otherType != ENTITY_TYPE_PED) { // bSkipLineCol | bProcessingShift | bForceHitReturnFalse
        m_bCollisionProcessed = true;

        auto* const lines = colData->m_pLines;
        lines[0].m_vecStart.z = 0.0f;
        lines[0].m_vecEnd.z   = -1.0f;
        bDoLineTest = true;
        if (bWasStanding) {
            lines[0].m_vecEnd.z = stepDown + lines[0].m_vecEnd.z;
        }

        auto* const spheres = colData->m_pSpheres;
        sphereTop = spheres[2].m_fRadius + spheres[2].m_vecCenter.z;

        if (bCheckColAboveHead) {
            // The original keeps this in extended precision
            const double t = ((double)spheres[0].m_vecCenter.z - (double)spheres[0].m_fRadius) + 1.0;
            lines[1].m_vecStart.z = sphereTop;
            lines[1].m_vecStart.z = (float)((double)lines[1].m_vecStart.z - t);
            lines[1].m_vecEnd.z   = sphereTop;
            lines[1].m_vecEnd.z   = (float)(t + (double)lines[1].m_vecEnd.z);

            colData->m_nNumLines = 2;
            colModel->m_boundSphere.m_fRadius = lines[1].m_vecEnd.z;
            colModel->m_boundBox.m_vecMax.z   = lines[1].m_vecEnd.z;
            colModel->m_boundBox.m_vecMin.z   = lines[0].m_vecEnd.z;
        } else {
            colData->m_nNumLines = 1;
            colModel->m_boundSphere.m_fRadius = std::abs(lines[0].m_vecEnd.z);
            colModel->m_boundBox.m_vecMax.z   = 0.95f;
            colModel->m_boundBox.m_vecMin.z   = lines[0].m_vecEnd.z;
        }
    } else {
        colData->m_nNumLines = 0;
        colModel->m_boundSphere.m_fRadius = 1.0f;
        colModel->m_boundBox.m_vecMax.z   = 0.95f;
        colModel->m_boundBox.m_vecMin.z   = -1.0f;
    }

    const bool bUnkColArg = m_bIsStuck && (otherType == ENTITY_TYPE_BUILDING || (GetEntityPhysFlags(entity) & 4));

    // 0x535300 (GetColModel) is called with a few unused stack arguments
    int32 numCols = CCollision::ProcessColModels(
        *m_matrix,
        *colModel,
        entity->GetMatrix(),
        *entity->GetColModel(),
        *(std::array<CColPoint, 32>*)colPoint, // should be okay for now
        lineCPs,
        lineTouchDists,
        bUnkColArg
    ); // 0x4185C0
    const float colPedZ = m_matrix->GetPosition().z;

    if (bDoLineTest) {
        colData->m_nNumLines = 0;
        colModel->m_boundSphere.m_fRadius = 1.0f;
        colModel->m_boundBox.m_vecMin.z   = -1.0f;
        colModel->m_boundBox.m_vecMax.z   = 0.95f;

        const CColPoint& lineCP0 = lineCPs[0];
        const CColPoint& lineCP1 = lineCPs[1];

        const bool bWasOnGround = bIsStanding;
        if (lineTouchDists[0] < 1.0f) { // 0x4185C0 found ground below the ped
            bool bProcessGround;
            if (!bWasOnGround) {
                bProcessGround = true;
            } else if ((double)lineCP0.m_vecPoint.z + 1.0 > (double)m_matrix->GetPosition().z) {
                bProcessGround = true;
            } else {
                bProcessGround = bEntityIsBoat && ((double)lineCP0.m_vecPoint.z + 3.0 > (double)m_matrix->GetPosition().z); // 0x858B3C
            }

            if (bProcessGround) {
                bool bSteepNormal = false;
                for (int32 i = 0; i < numCols; i++) {
                    if (colPoint[i].m_vecNormal.z < STEEP_NORMAL_Z) {
                        bSteepNormal = true;
                    }
                }

                // Update the brightness of the surface the ped is on
                // The original keeps the intermediate results in extended precision, hence `double`
                {
                    const double a = (double)CTimer::GetTimeStep() * (double)StaticRef<float>(0x8D21E4);
                    const double d = (double)lineCP0.m_nLightingB.day   * (double)CONTACT_LIGHT_SCALE;
                    const double n = (double)lineCP0.m_nLightingB.night * (double)CONTACT_LIGHT_SCALE;
                    const double w = (double)CCustomBuildingDNPipeline::m_fDNBalanceParam;
                    const double blended = (1.0 - w) * d + w * n;
                    if (m_nPedType == PED_TYPE_PLAYER1 || m_nPedType == PED_TYPE_PLAYER2) {
                        m_fContactSurfaceBrightness = (float)(blended * a + (1.0 - a) * (double)m_fContactSurfaceBrightness);
                    } else {
                        m_fContactSurfaceBrightness = (float)blended;
                    }
                }

                if (!bWasOnGround && (otherType == ENTITY_TYPE_VEHICLE || otherType == ENTITY_TYPE_OBJECT)) {
                    m_standingOnEntity = entity;
                    entity->RegisterReference(&m_standingOnEntity); // 0x571B70

                    const auto& entityPos = entity->GetPosition();
                    m_pContactEntity = entity;
                    field_56C = CVector{
                        lineCP0.m_vecPoint.x - entityPos.x,
                        lineCP0.m_vecPoint.y - entityPos.y,
                        lineCP0.m_vecPoint.z - entityPos.z,
                    };
                    entity->RegisterReference(&m_pContactEntity);

                    bOnBoat = otherType == ENTITY_TYPE_VEHICLE && entity->AsVehicle()->m_nVehicleType == VEHICLE_TYPE_BOAT;
                    if (otherType == ENTITY_TYPE_VEHICLE) {
                        m_fContactSurfaceBrightness = m_standingOnEntity->AsPhysical()->m_fContactSurfaceBrightness;
                    }
                } else {
                    m_pContactEntity = entity;
                    entity->RegisterReference(&m_pContactEntity);
                    bOnBoat                 = false;
                    bTryingToReachDryLand   = false; // 0xFFFFFF7D
                }

                // Ceiling
                if (bCheckColAboveHead
                    && lineTouchDists[1] < 1.0f
                    && lineCP1.m_vecPoint.z < field_588
                    && (!entity->GetIsTypePhysical() || (GetEntityPhysFlags(entity) & 8))
                ) {
                    field_588 = lineCP1.m_vecPoint.z;
                }

                bool bUseExtraNormal;
                if (bHeadStuckInCollision && !((double)lineCP0.m_vecPoint.z + 1.0 < (double)m_matrix->GetPosition().z)) {
                    if (otherType != ENTITY_TYPE_BUILDING) {
                        bUseExtraNormal = true;
                    } else {
                        const double dot = ((double)lineCP0.m_vecNormal.y * (double)m_vecMoveSpeed.y + (double)lineCP0.m_vecNormal.z * (double)m_vecMoveSpeed.z)
                                         + (double)lineCP0.m_vecNormal.x * (double)m_vecMoveSpeed.x;
                        bUseExtraNormal = !(dot > 0.0);
                    }
                } else {
                    bUseExtraNormal = false;
                }
                if (!bUseExtraNormal) {
                    bUseExtraNormal = bSteepNormal;
                }

                if (bUseExtraNormal) {
                    if (bHeadStuckInCollision && otherType != ENTITY_TYPE_VEHICLE) {
                        extraNormal = lineCP0.m_vecNormal;
                    }
                } else if (!(field_588 < NO_CEILING_Z)) {
                    m_matrix->GetPosition().z = (float)((double)lineCP0.m_vecPoint.z + 1.0);
                    bHeadStuckInCollision = false;
                } else {
                    double newZ = (double)lineCP0.m_vecPoint.z + 1.0;
                    if (((double)lineCP0.m_vecPoint.z + (double)sphereTop) + 1.0 > (double)field_588) {
                        extraNormal = lineCP0.m_vecNormal;

                        const double ceilingZ = (double)field_588 - (double)sphereTop;
                        const float  ceilingZF = (float)ceilingZ;
                        if (ceilingZ < newZ) {
                            newZ = (double)ceilingZF;
                        }
                    }
                    m_matrix->GetPosition().z = (float)newZ;
                }

                field_578         = lineCP0.m_vecNormal;
                m_nContactSurface = lineCP0.m_nSurfaceTypeB;
                if (g_surfaceInfos.IsSteepSlope(lineCP0.m_nSurfaceTypeB)) { // 0x55E770
                    bHitSteepSlope = true;
                }
            }

            // Fall damage
            float fallSpeedXY = FALL_SPEED_XY;
            float fallSpeedZ  = FALL_SPEED_Z;
            if (m_nPedState == PEDSTATE_IDLE) {
                fallSpeedXY = fallSpeedXY + fallSpeedXY;
                fallSpeedZ  = fallSpeedZ * 1.5f; // 0x858CE8
            }

            auto* const fallAnim = RpAnimBlendClumpGetAssociation(GetRpClump(), (uint32)ANIM_ID_FALL_FALL); // 0x4D68B0

            CVector vel = m_vecMoveSpeed;
            if (otherType > ENTITY_TYPE_BUILDING && otherType < ENTITY_TYPE_DUMMY) { // 2..4
                const auto& otherVel = entity->AsPhysical()->m_vecMoveSpeed;
                vel.x = vel.x - otherVel.x;
                vel.y = vel.y - otherVel.y;
                vel.z = vel.z - otherVel.z;
            }

            bool bFallDamage = false;
            if (!bWasStanding) {
                const double speedXY = std::sqrt((double)vel.y * (double)vel.y + (double)vel.x * (double)vel.x);
                const float  speedXYF = (float)speedXY;

                bool bCheckFall;
                if (speedXY > (double)fallSpeedXY && !bPushedAlongByCar) {
                    bCheckFall = true;
                } else {
                    bCheckFall = vel.z < fallSpeedZ;
                }

                if (bCheckFall && m_pEntityIgnoredCollision != entity && m_pVehicle != entity) {
                    bFallDamage = true;

                    float  landingFallSpeedZ = FALL_SPEED_Z; // 0x86C294
                    double softLanding;
                    if (g_surfaceInfos.IsSoftLanding(lineCP0.m_nSurfaceTypeB)) { // 0x55E690
                        softLanding = (double)SOFT_LANDING_SPEED;
                        landingFallSpeedZ = landingFallSpeedZ * 1.5f; // 0x858CE8
                    } else {
                        softLanding = 0.25;
                    }

                    double dXY = (double)speedXYF - softLanding;
                    if (0.0 > dXY) {
                        dXY = 0.0;
                    }
                    const double dmgXY = dXY * (double)FALL_DMG_MULT_XY;

                    double dZ = (double)landingFallSpeedZ - (double)vel.z;
                    if (0.0 > dZ) {
                        dZ = 0.0;
                    }
                    const double dmgZ = dZ * (double)FALL_DMG_MULT_Z;

                    float damage = (float)(dmgZ + dmgXY);
                    if (vel.z < FALL_DEADLY_SPEED_Z) {
                        damage = 500.0f;
                    }

                    uint8 direction = 2;
                    if (vel.x > MOVE_THRESHOLD || vel.x < -MOVE_THRESHOLD || vel.y > MOVE_THRESHOLD || vel.y < -MOVE_THRESHOLD) {
                        const CVector2D point{ vel.x * -1.0f, vel.y * -1.0f }; // 0x858C1C
                        direction = (uint8)GetLocalDirection(point); // 0x5DEF60
                    }

                    CEventDamage event{ entity, CTimer::GetTimeInMS(), WEAPON_FALL, PED_PIECE_TORSO, direction, false, false }; // 0x4AD830
                    if (event.AffectsPed(this)) { // 0x4B35A0
                        CPedDamageResponseCalculator calc{ entity, damage, WEAPON_FALL, PED_PIECE_TORSO, false }; // 0x4AD3F0
                        calc.ComputeDamageResponse(this, event.m_damageResponse, true); // 0x4B5AC0
                        GetIntelligence()->m_eventGroup.Add(&event, false); // 0x4AB420
                    }
                }
            }

            if (!bFallDamage) {
                if (!bWasStanding && fallAnim && (double)CTimer::GetTimeStep() * (double)FALL_ANIM_Z_MULT > (double)vel.z && m_pVehicle != entity) {
                    CEventDamage event{ entity, CTimer::GetTimeInMS(), WEAPON_FALL, PED_PIECE_TORSO, 2, false, false };
                    if (event.AffectsPed(this)) {
                        CPedDamageResponseCalculator calc{ entity, 15.0f, WEAPON_FALL, PED_PIECE_TORSO, false };
                        calc.ComputeDamageResponse(this, event.m_damageResponse, true);
                        GetIntelligence()->m_eventGroup.Add(&event, false);
                    }
                }
            }

            bIsStanding = true;
            m_vecMoveSpeed.z = 0.0f;

            // Moved up by more than a bit => check again without the line tests
            if ((double)colPedZ + (double)0.1f < (double)m_matrix->GetPosition().z // 0x858B1C
                && colData->m_nNumLines == 0
                && (m_nPedType == PED_TYPE_PLAYER1 || m_nPedType == PED_TYPE_PLAYER2)
            ) {
                numCols = CCollision::ProcessColModels(
                    *m_matrix,
                    *colModel,
                    entity->GetMatrix(),
                    *entity->GetColModel(),
                    *(std::array<CColPoint, 32>*)colPoint,
                    nullptr,
                    nullptr,
                    false
                );
            }
        } else { // No ground found
            if (bCheckColAboveHead
                && lineTouchDists[1] < 1.0f
                && lineCP1.m_vecPoint.z < field_588
                && (!entity->GetIsTypePhysical() || (GetEntityPhysFlags(entity) & 8))
            ) {
                field_588 = lineCP1.m_vecPoint.z;
                if (bIsStanding) {
                    if ((double)sphereTop + (double)GetPosition().z > (double)lineCP1.m_vecPoint.z) {
                        m_matrix->GetPosition().z = (float)((double)lineCP1.m_vecPoint.z - (double)sphereTop);
                    }
                }
            }
            bOnBoat = false;
        }
    }

    // Process the collision points
    for (int32 i = 0; i < numCols;) {
        CColPoint& cp = colPoint[i];

        if (bTestForBlockedPositions && m_bUsesCollision && cp.m_nPieceTypeA > 2) {
            bool bArmFlag1 = true;
            bool bArmFlag2 = true;
            bool bMidriff  = true;
            if (otherType == ENTITY_TYPE_PED) {
                bMidriff = false;
                if (entity->AsPed()->bFallenDown) {
                    bArmFlag1 = false;
                } else if (entity->AsPed()->bIsDucking) {
                    bArmFlag2 = false;
                }
            }

            if (cp.m_nPieceTypeA == 6 && bArmFlag1 && bArmFlag2) {
                bRightArmBlocked = true;
            } else if (cp.m_nPieceTypeA == 5 && bArmFlag1 && bArmFlag2) {
                bLeftArmBlocked = true;
            } else if (cp.m_nPieceTypeA == 8 && bArmFlag1) {
                bDuckRightArmBlocked = true;
            } else if (cp.m_nPieceTypeA == 4 && bMidriff) {
                bMidriffBlockedForJump = true;
            }

            // Remove this col point
            for (int32 j = i; j < numCols - 1; j++) {
                colPoint[j] = colPoint[j + 1]; // 0x40FC80
            }
            numCols--;
            continue;
        }

        if (otherType == ENTITY_TYPE_VEHICLE
            && entity->AsVehicle()->m_nVehicleSubType == VEHICLE_TYPE_TRAIN
            && (GetEntityPhysFlags(entity) & 4)
            && cp.m_vecNormal.z < 0.0f
        ) {
            cp.m_vecNormal.z = 0.0f;
            cp.m_vecNormal.Normalise(); // 0x59C910
        }
        i++;
    }

    // Restore the heading
    if ((double)savedHeading > SAVED_HEADING_THRESHOLD) {
        if (m_matrix) {
            m_matrix->SetRotateZOnly(savedHeading); // 0x59B020
        } else {
            m_placement.m_fHeading = savedHeading;
        }
    }

    const bool bOtherIsStaticLike = otherType == ENTITY_TYPE_BUILDING || entity->m_bIsStatic || entity->m_bIsStaticWaitingForCollision; // 0x1C: 4, 0x40000

    // Fix up the normals of collision points of static things
    if (bOtherIsStaticLike && bWasStanding && numCols > 0) {
        for (int32 i = 0; i < numCols; i++) {
            CColPoint& cp = colPoint[i];
            CVector    n  = cp.m_vecNormal;

            if (n.z < WALL_NORMAL_Z && cp.m_nPieceTypeA == 2 && (m_nPedType == PED_TYPE_PLAYER1 || m_nPedType == PED_TYPE_PLAYER2)) {
                // Wedged under something => push the ped down and away from its movement direction
                double speed = std::sqrt((double)m_vecMoveSpeed.x * (double)m_vecMoveSpeed.x + (double)m_vecMoveSpeed.y * (double)m_vecMoveSpeed.y);
                if (speed < 0.001f) { // 0x858CDC
                    speed = (double)0.001f;
                }
                const double mult = (double)-1.0f / speed; // 0x858C1C
                n.x = (float)(mult * (double)m_vecMoveSpeed.x);
                n.y = (float)(mult * (double)m_vecMoveSpeed.y);

                m_matrix->GetPosition().z = (float)((double)m_matrix->GetPosition().z - (double)0.05f); // 0x858C28
                bHeadStuckInCollision = true;
                bHitSteepSlope        = true; // 0x200020
            } else {
                const double len = std::sqrt((double)n.y * (double)n.y + (double)n.x * (double)n.x);
                if (!(len == 0.0)) {
                    const double mult = 1.0 / len;
                    n.x = (float)((double)n.x * mult);
                    n.y = (float)((double)n.y * mult);
                }
            }
            n.Normalise(); // 0x59C910
            cp.m_vecNormal = n;

            if (g_surfaceInfos.IsSteepSlope(cp.m_nSurfaceTypeB)) { // 0x55E770
                bHitSteepSlope = true;
            }
        }
    }

    // Add a col point pushing the ped away from the stuck-in wall
    if (extraNormal.z < 1.0f) {
        CColPoint& cp = colPoint[numCols];
        cp.m_vecNormal.x = extraNormal.x;
        cp.m_vecNormal.y = extraNormal.y;
        cp.m_vecNormal.z = 0.0f;
        cp.m_vecNormal.Normalise(); // 0x59C910

        const auto& pos = GetPosition();
        const double nx = (double)cp.m_vecNormal.x * (double)EXTRA_POINT_DIST;
        const double ny = (double)cp.m_vecNormal.y * (double)EXTRA_POINT_DIST;
        const float  nz = (float)((double)cp.m_vecNormal.z * (double)EXTRA_POINT_DIST);
        cp.m_vecPoint.x = (float)((double)pos.x - nx);
        cp.m_vecPoint.y = (float)((double)pos.y - ny);
        cp.m_vecPoint.z = (float)((double)pos.z - (double)nz);

        numCols++;
        bHitSteepSlope = true;
    }

    if (numCols > 0 || lineTouchDists[0] < 1.0f) {
        AddCollisionRecord(entity); // 0x543490
        if (otherType != ENTITY_TYPE_BUILDING) {
            entity->AsPhysical()->AddCollisionRecord(this);
        }
        if (numCols > 0 && bOtherIsStaticLike) {
            m_bHasHitWall = true; // 0x1000
        }
    }

    return numCols;
}

// NOTSA
bool CPed::IsInVehicleAsPassenger() const noexcept {
    return bInVehicle && m_pVehicle && m_pVehicle->m_pDriver != this;
}

// NOTSA
CVector CPed::GetSeatPositionInVehicle() const {
    auto* mi = m_pVehicle->GetVehicleModelInfo();
    if (this == m_pVehicle->GetDriver()) {
        CVector pos = mi->GetFrontSeatPosn();

        if (!m_pVehicle->IsBoat() && !m_pVehicle->IsBike()) {
            pos.x = -pos.x;
        }

        if (m_pVehicle->IsSubBMX()) {
            pos.z -= (0.001f * std::abs(m_pVehicle->AsBmx()->m_fControlJump));
        }

        return pos;
    } else if (this == m_pVehicle->m_apPassengers[0]) {
        return m_pVehicle->IsBike() || m_pVehicle->IsSubQuad() ? mi->GetBackSeatPosn() : mi->GetFrontSeatPosn();
    } else if (this == m_pVehicle->m_apPassengers[1]) {
        CVector pos   = mi->GetBackSeatPosn();
        pos.x = -pos.x;
        return pos;
    } else if (this == m_pVehicle->m_apPassengers[2]) {
        return mi->GetBackSeatPosn();
    }
    return mi->GetFrontSeatPosn(); /* Default to front seat position */
}

bool CPed::IsJoggingOrFaster() const {
    switch (m_nMoveState) {
    case PEDMOVE_JOG:
    case PEDMOVE_RUN:
    case PEDMOVE_SPRINT:
        return true;
    }
    return false;
}

bool CPed::IsRunningOrSprinting() const {
    switch (m_nMoveState) {
    case PEDMOVE_RUN:
    case PEDMOVE_SPRINT:
        return true;
    }
    return false;
}

bool CPed::IsPedStandingInPlace() const {
    switch (m_nMoveState) {
    case PEDMOVE_NONE:
    case PEDMOVE_STILL:
    case PEDMOVE_TURN_L:
    case PEDMOVE_TURN_R:
        return true;
    }
    return false;
}

// 0x6497A0
bool SayJacked(CPed* jacked, CVehicle* vehicle, uint32 timeDelay) {
    switch (vehicle->m_vehicleAudio.GetVehicleTypeForAudio()) {
    case eAEVehicleAudioType::CAR:
        return jacked->Say(CTX_GLOBAL_JACKED_CAR, timeDelay) != -1;
    case eAEVehicleAudioType::BIKE:
    case eAEVehicleAudioType::GENERIC:
        return jacked->Say(CTX_GLOBAL_JACKED_GENERIC, timeDelay) != -1;
    default:
        NOTSA_UNREACHABLE();
    }
}

// 0x6497F0
bool SayJacking(CPed* jacker, CPed* jacked, CVehicle* vehicle, uint32 timeDelay) {
    switch (vehicle->m_vehicleAudio.GetVehicleTypeForAudio()) {
    case eAEVehicleAudioType::BIKE:
        return jacker->Say(CTX_GLOBAL_JACKING_BIKE, timeDelay) != -1;
    case eAEVehicleAudioType::CAR:
        return jacked->GetSpeechAE().IsPedFemaleForAudio()
            ? jacker->Say(CTX_GLOBAL_JACKING_CAR_FEM, timeDelay) != -1
            : jacker->Say(CTX_GLOBAL_JACKING_CAR_MALE, timeDelay) != -1;
    case eAEVehicleAudioType::GENERIC:
        return jacker->Say(CTX_GLOBAL_JACKING_GENERIC, timeDelay) != -1;
    default:
        NOTSA_UNREACHABLE();
    }
}

// NOTSA
int32 CPed::GetPadNumber() const {
    switch (m_nPedType) {
    case PED_TYPE_PLAYER1: return 0;
    case PED_TYPE_PLAYER2: return 1;
    default:               NOTSA_UNREACHABLE();
    }
}

bool CPed::IsRightArmBlockedNow() const {
    if (bIsDucking) {
        return bDuckRightArmBlocked;
    }
    return bRightArmBlocked;
}
