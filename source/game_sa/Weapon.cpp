/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Weapon.h"

#include "Glass.h"
#include "WeaponInfo.h"
#include "CreepingFire.h"
#include "BulletInfo.h"
#include "InterestingEvents.h"
#include "Shadows.h"
#include "Birds.h"
#include "Automobile.h"
#include "DamageManager.h"
#include "SurfaceInfos_c.h"
#include "VehicleModelInfo.h"
#include "RwHelper.h"
#include "TaskSimpleUseGun.h"

//float& PELLET_COL_SCALE_RATIO_MULT = *(float*)0x8D6128; // 1.3

void CWeapon::InjectHooks() {
    RH_ScopedClass(CWeapon);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x73B430);

    RH_ScopedInstall(Shutdown, 0x73A380);
    RH_ScopedInstall(Reload, 0x73AEB0);
    RH_ScopedInstall(IsTypeMelee, 0x73B1C0);
    RH_ScopedInstall(IsType2Handed, 0x73B1E0);
    RH_ScopedInstall(IsTypeProjectile, 0x73B210);
    RH_ScopedInstall(HasWeaponAmmoToBeUsed, 0x73B2A0);
    RH_ScopedInstall(InitialiseWeapons, 0x73A300);
    RH_ScopedInstall(ShutdownWeapons, 0x73A330);
    RH_ScopedInstall(UpdateWeapons, 0x73A360);
    RH_ScopedInstall(AddGunshell, 0x73A3E0);
    RH_ScopedInstall(ProcessLineOfSight, 0x73B300);
    RH_ScopedInstall(StopWeaponEffect, 0x73B360);
    RH_ScopedInstall(TargetWeaponRangeMultiplier, 0x73B380);
    RH_ScopedInstall(Initialise, 0x73B4A0);
    RH_ScopedInstall(DoWeaponEffect, 0x73E690);
    RH_ScopedInstall(FireSniper, 0x73AAC0);
    RH_ScopedInstall(TakePhotograph, 0x73C1F0);
    RH_ScopedInstall(DoDoomAiming, 0x73CDC0);
    RH_ScopedInstall(GenerateDamageEvent, 0x73A530);
    RH_ScopedInstall(FireInstantHitFromCar2, 0x73CBA0);
    RH_ScopedInstall(Update, 0x73DB40);
    RH_ScopedInstall(SetUpPelletCol, 0x73C710);
    RH_ScopedInstall(FireAreaEffect, 0x73E800);
    RH_ScopedInstall(FireInstantHitFromCar, 0x73EC40);
    RH_ScopedInstall(FireFromCar, 0x73FA20);
    RH_ScopedInstall(FireInstantHit, 0x73FB10);
    RH_ScopedInstall(FireProjectile, 0x741360);
    RH_ScopedInstall(DoBulletImpact, 0x73B550);
    RH_ScopedInstall(LaserScopeDot, 0x73A8D0);
    RH_ScopedInstall(FireM16_1stPerson, 0x741C00);
    RH_ScopedInstall(Fire, 0x742300);
    RH_ScopedGlobalInstall(DoTankDoomAiming, 0x73D1E0);
    RH_ScopedGlobalInstall(DoDriveByAutoAiming, 0x73D720);
    RH_ScopedGlobalInstall(FindNearestTargetEntityWithScreenCoors, 0x73E240);
    RH_ScopedGlobalInstall(EvaluateTargetForHeatSeekingMissile, 0x73E560);
    RH_ScopedGlobalInstall(CheckForShootingVehicleOccupant, 0x73F480);
    RH_ScopedGlobalInstall(PickTargetForHeatSeekingMissile, 0x73F910);
    RH_ScopedOverloadedInstall(CanBeUsedFor2Player, "Static", 0x73B240, bool(*)(eWeaponType));
    RH_ScopedOverloadedInstall(CanBeUsedFor2Player, "Method", 0x73DEF0, bool(CWeapon::*)());
    RH_ScopedGlobalInstall(FireOneInstantHitRound, 0x73AF00);
}

// 0x73B430
CWeapon::CWeapon(eWeaponType weaponType, uint32 ammo) :
    m_Type{ weaponType },
    m_TotalAmmo{ std::min<uint32>(ammo, 99'999) }
{
    Reload();
}

// 0x73B4A0
void CWeapon::Initialise(eWeaponType weaponType, int32 ammo, CPed* owner) {
    m_Type = weaponType;
    m_State = eWeaponState::WEAPONSTATE_READY;
    m_AmmoInClip = 0;
    m_TotalAmmo = std::min(ammo, 99999);

    Reload(owner);

    m_TimeForNextShotMs = 0;

    for (const auto m : CWeaponInfo::GetWeaponInfo(m_Type)->GetModels()) {
        if (m != MODEL_INVALID) {
            CModelInfo::GetModelInfo(m)->AddRef();
        }
    }

    m_FxSystem = nullptr;
    m_DontPlaceInHand = false;
}

// 0x73A300
void CWeapon::InitialiseWeapons() {
    CWeaponInfo::Initialise();
    CShotInfo::Initialise();
    CExplosion::Initialise();
    CProjectileInfo::Initialise();
    CBulletInfo::Initialise();

    bPhotographHasBeenTaken = false;
    ms_bTakePhoto = false;
}

// 0x73A330
void CWeapon::ShutdownWeapons() {
    CWeaponInfo::Shutdown();
    CShotInfo::Shutdown();
    CExplosion::Shutdown();
    CProjectileInfo::Shutdown();
    CBulletInfo::Shutdown();

    ms_PelletTestCol.RemoveCollisionVolumes();
}

// 0x73A380
void CWeapon::Shutdown() {
    for (const auto m : CWeaponInfo::GetWeaponInfo(m_Type)->GetModels()) {
        if (m != MODEL_INVALID) {
            CModelInfo::GetModelInfo(m)->RemoveRef();
        }
    }

    m_Type            = eWeaponType::WEAPON_UNARMED;
    m_State           = eWeaponState::WEAPONSTATE_READY;
    m_TotalAmmo       = 0;
    m_AmmoInClip      = 0;
    m_TimeForNextShotMs = 0;
}

// 0x73A3E0
void CWeapon::AddGunshell(CEntity* creator, CVector& position, const CVector2D& direction, float size) {
    if (!creator || !creator->GetIsOnScreen()) {
        return;
    }

    // originally squared
    if ((creator->GetPosition() - TheCamera.GetPosition()).SquaredMagnitude() > sq(10.0f)) {
        return;
    }

    // Add gunshell fx particle
    FxPrtMult_c fxprt(0.5f, 0.5f, 0.5f, 1.0f, size, 1.0f, 1.0f);
    switch (m_Type) {
    case eWeaponType::WEAPON_SPAS12_SHOTGUN:
    case eWeaponType::WEAPON_SHOTGUN:
        fxprt.SetColor(0.6f, 0.1f, 0.1f);
    }
    g_fx.m_GunShell->AddParticle(position, { direction.x, direction.y, CGeneral::GetRandomNumberInRange(0.4f, 1.6f) }, 0.0f, fxprt);
}

// 0x73A530
bool CWeapon::GenerateDamageEvent(CPed* victim, CEntity* creator, eWeaponType weaponType, int32 damageFactor, ePedPieceTypes pedPiece, uint8 direction) {
    CPedDamageResponseCalculator pedDmgRespCalc{
        creator,
        (float)damageFactor,
        weaponType,
        pedPiece,
        false
    };

    CEventDamage eventDmg{
        creator,
        CTimer::GetTimeInMS(),
        weaponType,
        pedPiece,
        direction,
        false,
        victim->bInVehicle
    };

    if (   victim->m_fHealth <= 0.f
        && CLocalisation::Blood()
        && CLocalisation::KickingWhenDown()
        && victim->GetTaskManager().GetSimplestActiveTask()->GetTaskType() == TASK_SIMPLE_DEAD
    ) {
        const auto floorHitAnim = CAnimManager::BlendAnimation(
            victim->GetRpClump(),
            ANIM_GROUP_DEFAULT,
            RpAnimBlendClumpGetFirstAssociation(victim->GetRpClump(), ANIMATION_IS_FRONT)
                ? ANIM_ID_FLOOR_HIT_F
                : ANIM_ID_FLOOR_HIT
        );
        if (floorHitAnim) {
            floorHitAnim->SetFlag(ANIMATION_IS_FINISH_AUTO_REMOVE, false);
            floorHitAnim->Start();
        }
        return true;
    }

    if (!victim->IsAlive()) {
        return true;
    }

    if (!eventDmg.AffectsPed(victim)) { // 0x73A687
        return false;
    }

    if (creator == FindPlayerPed()) {
        CCrime::ReportCrime(CRIME_DAMAGED_PED, victim, static_cast<CPed*>(creator));
    }

    pedDmgRespCalc.ComputeDamageResponse(
        victim,
        eventDmg.m_damageResponse,
        true
    );

    bool ret = true;
    if (!victim->bInVehicle && (
           (!notsa::IsFixBugs() || CWeaponInfo::TypeIsWeapon(weaponType)) && CWeaponInfo::GetWeaponInfo(weaponType)->m_nWeaponFire == eWeaponFire::WEAPON_FIRE_MELEE
        || weaponType == WEAPON_FALL && creator && creator->GetIsTypeObject()
    )) { // 0x73A6F1
        eventDmg.ComputeAnim(victim, true);
        switch (eventDmg.m_nAnimID) {
        case ANIM_ID_SHOT_PARTIAL:
        case ANIM_ID_SHOT_LEFTP:
        case ANIM_ID_SHOT_PARTIAL_B:
        case ANIM_ID_SHOT_RIGHTP: { //> 0x73A769 - Inverted
            auto anim = RpAnimBlendClumpGetAssociation(victim->GetRpClump(), eventDmg.m_nAnimID);
            if (!anim) {
                anim = CAnimManager::AddAnimation(
                    victim->GetRpClump(),
                    (AssocGroupId)eventDmg.m_nAnimGroup,
                    (AnimationId)eventDmg.m_nAnimID
                );
            }
            anim->m_BlendAmount = 0.f;
            anim->m_BlendDelta = eventDmg.m_fAnimBlend;
            anim->m_Speed = eventDmg.m_fAnimSpeed;
            anim->Start();
            break;
        }
        case ANIM_ID_NO_ANIMATION_SET:
            break;
        case ANIM_ID_DOOR_LHINGE_O:
            ret = false;
            break;
        default: { //< 0x73A7B5
            const auto a = CAnimManager::BlendAnimation(
                victim->GetRpClump(),
                (AssocGroupId)eventDmg.m_nAnimGroup,
                (AnimationId)eventDmg.m_nAnimID,
                eventDmg.m_fAnimBlend
            );
            a->m_Speed = eventDmg.m_fAnimSpeed;
            a->SetFlag(ANIMATION_IS_PLAYING);
            break;
        }
        }
    }

    // 0x73A828
    eventDmg.m_bStealthMode =
           creator
        && creator->GetIsTypePed()
        && (weaponType == WEAPON_PISTOL_SILENCED || creator->AsPed()->GetTaskManager().GetActiveTask()->GetTaskType() == TASK_SIMPLE_STEALTH_KILL);

    if (!victim->bInVehicle || victim->m_fHealth <= 0.f || !victim->GetTaskManager().GetActiveTask() || victim->GetTaskManager().GetActiveTask()->GetTaskType() != TASK_SIMPLE_GANG_DRIVEBY) {
        victim->GetEventGroup().Add(eventDmg);
    }

    return ret;

}

// 0x73A8D0
bool CWeapon::LaserScopeDot(CVector* outCoord, float* outSize) {
    /* UNUSED */
    NOTSA_UNREACHABLE();
}

// 0x73AAC0
bool CWeapon::FireSniper(CPed* shooter, CEntity* victim, CVector* target) {
    const CCam& activeCam = CCamera::GetActiveCamera();

    if (FindPlayerPed() == shooter) {
        switch (activeCam.m_nMode) {
        case MODE_M16_1STPERSON:
        case MODE_SNIPER:
        case MODE_CAMERA:
        case MODE_ROCKETLAUNCHER:
        case MODE_ROCKETLAUNCHER_HS:
        case MODE_M16_1STPERSON_RUNABOUT:
        case MODE_SNIPER_RUNABOUT:
        case MODE_ROCKETLAUNCHER_RUNABOUT:
        case MODE_ROCKETLAUNCHER_RUNABOUT_HS:
            break;
        default:
            return false;
        }
    }

    // todo: make sense of literals.
    float vecFrontZ_Y = activeCam.m_vecFront.z * 0.145f - activeCam.m_vecFront.y * 0.98940003f;

    if (vecFrontZ_Y > 0.99699998f)
        CCoronas::MoonSize = (CCoronas::MoonSize + 1) % 8;

    CVector velocity = activeCam.m_vecFront;
    velocity.Normalise();
    velocity *= 16.0f;

    CBulletInfo::AddBullet(shooter, m_Type, activeCam.m_vecSource, velocity);

    // recoil effect for players
    if (shooter->IsPlayer()) {
        CVector creatorPos = FindPlayerCoors();
        CPad* creatorPad = CPad::GetPad(shooter->m_nPedType);

        creatorPad->StartShake_Distance(240, 128, creatorPos);
        CamShakeNoPos(&TheCamera, 0.2f);
    }

    if (shooter->GetIsTypePed()) {
        CCrime::ReportCrime(CRIME_FIRE_WEAPON, shooter, shooter);
    } else if (shooter->GetIsTypeVehicle() && shooter->m_roadRageWith) {
        CCrime::ReportCrime(CRIME_FIRE_WEAPON, shooter, shooter->m_roadRageWith);
    }

    CVector targetPoint = velocity * 40.0f + activeCam.m_vecSource;
    bool hasNoSound = m_Type == eWeaponType::WEAPON_PISTOL_SILENCED || m_Type == eWeaponType::WEAPON_TEARGAS;
    CEventGroup* eventGroup = GetEventGlobalGroup();

    CEventGunShot gs(shooter, activeCam.m_vecSource, targetPoint, hasNoSound);
    eventGroup->Add(static_cast<CEvent*>(&gs), false);

    CEventGunShotWhizzedBy gsw(shooter, activeCam.m_vecSource, targetPoint, hasNoSound);
    eventGroup->Add(static_cast<CEvent*>(&gsw), false);

    g_InterestingEvents.Add(CInterestingEvents::EType::INTERESTING_EVENT_22, shooter);

    return true;
}

// 0x73AEB0
void CWeapon::Reload(CPed* owner) {
    if (!m_TotalAmmo) {
        return;
    }

    uint32 ammo = GetWeaponInfo(owner).m_nAmmoClip;
    m_AmmoInClip = std::min(ammo, m_TotalAmmo);
}

// 0x73B1C0
bool CWeapon::IsTypeMelee() {
    return GetWeaponInfo().m_nWeaponFire == eWeaponFire::WEAPON_FIRE_MELEE;
}

// 0x73B1E0
bool CWeapon::IsType2Handed() {
    switch (m_Type) {
    case eWeaponType::WEAPON_M4:
    case eWeaponType::WEAPON_AK47:
    case eWeaponType::WEAPON_SPAS12_SHOTGUN:
    case eWeaponType::WEAPON_SHOTGUN:
    case eWeaponType::WEAPON_SNIPERRIFLE:
    case eWeaponType::WEAPON_FLAMETHROWER:
    case eWeaponType::WEAPON_COUNTRYRIFLE:
        return true;
    }
    return false;
}

// 0x73B210
bool CWeapon::IsTypeProjectile() {
    switch (m_Type) {
    case eWeaponType::WEAPON_GRENADE:
    case eWeaponType::WEAPON_REMOTE_SATCHEL_CHARGE:
    case eWeaponType::WEAPON_TEARGAS:
    case eWeaponType::WEAPON_MOLOTOV:
    case eWeaponType::WEAPON_FREEFALL_BOMB:
        return true;
    }
    return false;
}

// 0x73B240
bool CWeapon::CanBeUsedFor2Player(eWeaponType weaponType) {
    switch (weaponType) {
    case eWeaponType::WEAPON_CHAINSAW:
    case eWeaponType::WEAPON_SNIPERRIFLE:
    case eWeaponType::WEAPON_RLAUNCHER:
    case eWeaponType::WEAPON_PARACHUTE:
        return false;
    }
    return true;
}

// 0x73B2A0
bool CWeapon::HasWeaponAmmoToBeUsed() {
    switch (m_Type) {
    case eWeaponType::WEAPON_UNARMED:
    case eWeaponType::WEAPON_BRASSKNUCKLE:
    case eWeaponType::WEAPON_GOLFCLUB:
    case eWeaponType::WEAPON_NIGHTSTICK:
    case eWeaponType::WEAPON_KNIFE:
    case eWeaponType::WEAPON_BASEBALLBAT:
    case eWeaponType::WEAPON_KATANA:
    case eWeaponType::WEAPON_CHAINSAW:
    case eWeaponType::WEAPON_DILDO1:
    case eWeaponType::WEAPON_DILDO2:
    case eWeaponType::WEAPON_VIBE1:
    case eWeaponType::WEAPON_VIBE2:
    case eWeaponType::WEAPON_FLOWERS:
    case eWeaponType::WEAPON_PARACHUTE:
        return true;
    }
    return m_TotalAmmo != 0;
}

// 0x73B300
bool CWeapon::ProcessLineOfSight(const CVector& startPoint, const CVector& endPoint, CColPoint& outColPoint, CEntity*& outEntity, eWeaponType weaponType, CEntity* arg5,
                                 bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool arg11, bool doIgnoreCameraCheck) {
    CBirds::HandleGunShot(&startPoint, &endPoint);
    CShadows::GunShotSetsOilOnFire(startPoint, endPoint);
    return CWorld::ProcessLineOfSight(startPoint, endPoint, outColPoint, outEntity, buildings, vehicles, peds, objects, dummies, false, doIgnoreCameraCheck, true);
}

// 0x73B360
void CWeapon::StopWeaponEffect() {
    if (m_FxSystem && m_Type != WEAPON_MOLOTOV) {
        m_FxSystem->Kill();
        m_FxSystem = nullptr;
    }
}

// 0x73B380
float CWeapon::TargetWeaponRangeMultiplier(CEntity* target, CEntity* weaponOwner) {
    if (!target || !weaponOwner) {
        return 1.0f;
    }

    switch (target->GetType()) {
    case ENTITY_TYPE_VEHICLE: {
        if (!target->AsVehicle()->IsBike()) {
            return 3.0f;
        }
        break;
    }
    case ENTITY_TYPE_PED: {
        CPed* pedVictim = target->AsPed();

        if (pedVictim->m_pVehicle && !pedVictim->m_pVehicle->IsBike()) {
            return 3.0f;
        }

        if (CEntity* attachedTo = pedVictim->m_pAttachedTo) {
            if (attachedTo->GetIsTypeVehicle() && !attachedTo->AsVehicle()->IsBike()) {
                return 3.0f;
            }
        }

        break;
    }
    }

    if (!weaponOwner->GetIsTypePed() || !weaponOwner->AsPed()->IsPlayer()) {
        return 1.0f;
    }

    switch (CCamera::GetActiveCamera().m_nMode) {
    case MODE_TWOPLAYER_IN_CAR_AND_SHOOTING:
        return 2.0f;
    case MODE_HELICANNON_1STPERSON:
        return 3.0f;
    }

    return 1.0f;
}

// 0x73B550
void CWeapon::DoBulletImpact(CEntity* firedBy, CEntity* victim, const CVector& startPoint, const CVector& endPoint, const CColPoint& hitCP, int32 incrementalHit) {
    const auto firedByPed = firedBy->GetIsTypePed()
        ? firedBy->AsPed()
        : nullptr;
    const auto firedByPlayer = firedByPed && firedByPed->IsPlayer()
        ? firedByPed->AsPlayer()
        : nullptr;

    const auto wi = &GetWeaponInfo(firedByPed);

    if (firedByPed && firedByPed->IsPlayer()) {
        CCrime::ReportCrime(CRIME_FIRE_WEAPON, victim, firedByPed);
    }

    if (victim) { // Inverted
        CBulletTraces::AddTrace( // 0x73B60C
            incrementalHit
                ? startPoint + (hitCP.m_vecPoint - startPoint) * 0.4f
                : startPoint,
            hitCP.m_vecPoint,
            GetType(),
            firedBy
        );

        const auto DoBulletHitFx = [&] {
            if (incrementalHit <= 0) {
                const auto angle = (endPoint - startPoint).Normalized().Dot(hitCP.m_vecNormal);
                if (angle < 0.f) { // Normal is opposite to that of the bullet's direction
                    AudioEngine.ReportBulletHit(
                        victim,
                        hitCP.m_nSurfaceTypeB,
                        hitCP.m_vecPoint,
                        RWRAD2DEG(std::asin(-angle))
                    );
                }
            }
        };

#ifdef FIX_BUGS
        if (firedByPlayer && (firedByPlayer != victim || firedBy->GetStatus() == STATUS_PLAYER)) { // 0x73B6D0
#else
        if (firedByPlayer && firedByPlayer != victim || firedBy->GetStatus() == STATUS_PLAYER) { // 0x73B6D0
#endif
            if (notsa::contains({ ENTITY_TYPE_PED, ENTITY_TYPE_VEHICLE, ENTITY_TYPE_OBJECT }, victim->GetType())) {
                if (CStats::GetStatValue(STAT_BULLETS_FIRED) >= CStats::GetStatValue(STAT_BULLETS_THAT_HIT)) {
                    CStats::IncrementStat(STAT_BULLETS_THAT_HIT);
                }
            }
            if (CWeaponInfo::TypeHasSkillStats(GetType())) { // 0x73B738
                // Redundant `if` check here was removed
                if ([&]{
                    const auto victimEntity = notsa::coalesce(firedByPlayer->m_pTargetedObject, victim);

                    // NOTE: The code is written upside down to make the controlflow easier

                    if (victimEntity->GetIsTypePed() && CPedGroups::AreInSameGroup(victimEntity->AsPed(), firedByPed)) {
                        return false;
                    }

                    switch (victimEntity->GetType()) {
                    case eEntityType::ENTITY_TYPE_PED: {
                        const auto victimPed = victimEntity->AsPed();
                        return !CPedGroups::AreInSameGroup(victimPed, firedByPed) && victimPed->m_fHealth > 0.f;
                    }
                    case eEntityType::ENTITY_TYPE_VEHICLE: {
                        const auto victimVeh = victimEntity->AsVehicle();
                        if (notsa::contains(eCarPiece_WheelPieces, (eCarPiece)hitCP.m_nPieceTypeB)) {
                            if (!victimVeh->BurstTyre(hitCP.m_nPieceTypeB, true)) {
                                return false;
                            }
                        }
                        if (victimVeh->physicalFlags.bBulletProof || !victimVeh->vehicleFlags.bCanBeDamaged) {
                            return false;
                        }
                        if (victimVeh->m_fHealth <= 0.f || victimVeh->GetStatus() == STATUS_WRECKED) {
                            return false;
                        }
                        return true;
                    }
                    case eEntityType::ENTITY_TYPE_OBJECT: {
                        const auto victimObj = victimEntity->AsObject();
                        return victimObj->m_fHealth > 0.f && victimObj->m_nColDamageEffect && victimObj->m_pObjectInfo->m_fColDamageMultiplier >= 0.5f;
                    }
                    }
                    return false;
                }()) {
                    CStats::UpdateStatsWhenWeaponHit(GetType());
                }
                
            }
        }

        if (!victim->GetIsTypePed()) { // 0x73B85B
            CGlass::WasGlassHitByBullet(victim, hitCP.m_vecPoint);

            const auto DoBulletImpactFx = [&] {
                if (TheCamera.IsSphereVisible(hitCP.m_vecPoint, 1.f)) {
                    g_fx.AddBulletImpact(
                        hitCP.m_vecPoint,
                        hitCP.m_vecNormal,
                        hitCP.m_nSurfaceTypeB,
                        incrementalHit ? 2 : 8,
                        hitCP.m_nLightingA.GetCurrentLighting()
                    );
                }
            };

            switch (victim->GetType()) {
            case eEntityType::ENTITY_TYPE_BUILDING: { // 0x73C014
                DoBulletImpactFx();
                if (firedByPlayer) {
                    firedByPlayer->GetPlayerData()->m_nModelIndexOfLastBuildingShot = victim->m_nModelIndex;
                }
                break;
            }
            case eEntityType::ENTITY_TYPE_VEHICLE: { // 0x73BD2A
                const auto victimVeh = victim->AsVehicle();
                if (!notsa::contains(eCarPiece_WheelPieces, (eCarPiece)hitCP.m_nPieceTypeB)) {
                    victimVeh->InflictDamage(
                        firedBy,
                        GetType(),
                        (float)(firedByPlayer && TheCamera.GetActiveCam().m_nMode == MODE_TWOPLAYER_IN_CAR_AND_SHOOTING ? 2 * wi->m_nDamage : wi->m_nDamage),
                        hitCP.m_vecPoint
                    );
                    DoBulletImpactFx();
                    if (g_LoadMonitor.GetProcLevel() != eProcessingLevel::HIGH) { // 0x73BF6C - NOTE/TODO: Useless, remove
                        const auto wepForceMult = [this]{
                            switch (GetType()) {
                            case WEAPON_DESERT_EAGLE:
                            case WEAPON_MINIGUN:
                                return -20.f;
                            case WEAPON_SHOTGUN:
                            case WEAPON_SPAS12_SHOTGUN:
                                return -4.0f;
                            default:
                                return -10.f;
                            }
                        }();
                        victimVeh->ApplyForce(
                            hitCP.m_vecNormal * (wepForceMult * std::min(1.f, victimVeh->m_fMass / 1000.f)),
                            hitCP.m_vecPoint - victimVeh->GetPosition(),
                            true
                        );
                    }
                } else { // 0x73BD3F
                    victimVeh->BurstTyre(hitCP.m_nPieceTypeB, true);
                    g_fx.AddTyreBurst(hitCP.m_vecPoint, hitCP.m_vecNormal);
                    if (firedByPed) { // Add event to occupants
                        const auto AddEventVehicleDamageWeapon = [&](CPed* ped) {
                            if (ped) {
                                ped->GetEventGroup().Add(CEventVehicleDamageWeapon{
                                    victimVeh,
                                    firedBy,
                                    GetType()
                                });
                            }
                        };
                        AddEventVehicleDamageWeapon(victimVeh->m_pDriver);
                        rng::for_each(victimVeh->GetPassengers(), AddEventVehicleDamageWeapon);
                    }
                }
                break;
            }
            case eEntityType::ENTITY_TYPE_OBJECT: { // 0x73BB4F
                const auto victimObj = victim->AsObject();
                const auto oinfo     = victimObj->m_pObjectInfo;

                DoBulletImpactFx();
                if (victimObj->m_nColDamageEffect < 200) {
                    if (!victimObj->physicalFlags.bDisableCollisionForce && oinfo->m_fColDamageMultiplier < 99.9f) {
                        if (victimObj->GetIsStatic() && oinfo->m_fUprootLimit <= 0.f) {
                            victimObj->SetIsStatic(false);
                            victimObj->AddToMovingList();
                        }
                        if (!victimObj->GetIsStatic()) { // 0x73BC6B - Move the object a little
                            float force = -2.f;
                            if (victimObj->physicalFlags.bDisableZ || victimObj->physicalFlags.bDisableMoveForce) {
                                force *= 0.1f;
                            }
                            if (incrementalHit) {
                                force *= 0.2f;
                            }
                            victimObj->ApplyForce(
                                hitCP.m_vecNormal * force,
                                hitCP.m_vecPoint - victimObj->GetPosition(),
                                true
                            );
                        }
                    }
                } else { // 0x73BB94
                    victimObj->ObjectDamage(
                        [&]{
                            switch (oinfo->m_nGunBreakMode) {
                            case 1:  return 151.f;
                            case 2:  return 151.f * oinfo->m_fSmashMultiplier;
                            default: return 50.f;
                            }
                        }(),
                        &hitCP.m_vecPoint,
                        &hitCP.m_vecNormal,
                        firedBy,
                        GetType()
                    );
                }

                break;
            }
            }
            DoBulletHitFx();
        } else if (victim != firedBy) {
            const auto victimPed = victim->AsPed();
            if (   !firedByPed
                || firedByPed->m_nPedType != victimPed->m_nPedType
                || notsa::contains({ PED_TYPE_CIVMALE, PED_TYPE_CIVFEMALE }, victimPed->m_nPedType)
                || firedByPlayer
            ) {
                const auto bAddBloodFx = [&]{
                    if (incrementalHit > 0) { // 0x73B8B8
                        return false;
                    }
                    DoBulletHitFx();
                    return GenerateDamageEvent(
                        victimPed,
                        firedBy,
                        GetType(),
                        [&] {
                            if (firedByPlayer
                                && (victim->GetPosition() - startPoint).SquaredMagnitude() <= 1.f
                                && !victimPed->bNoCriticalHits
                                && !notsa::contains({ WEAPON_SHOTGUN, WEAPON_SPAS12_SHOTGUN }, GetType())
                            ) {
                                return 150;
                            }
                            return incrementalHit < 0
                                ? -(incrementalHit * (int32)wi->m_nDamage)
                                : (int32)wi->m_nDamage;
                        }(),
                        (ePedPieceTypes)hitCP.m_nPieceTypeB,
                        victimPed->GetLocalDirection(startPoint - victimPed->GetPosition2D())
                    );
                }();
                if (firedByPlayer) { // 0x73BA79
                    CCrime::ReportCrime(CRIME_DAMAGE_CAR, victim, firedByPed);
                }
                if (CLocalisation::Blood() && bAddBloodFx) { // 0x73BA81
                    g_fx.AddBlood(
                        hitCP.m_vecPoint,
                        hitCP.m_vecNormal,
                        hitCP.m_nPieceTypeB == ePedPieceTypes::PED_PIECE_HEAD
                                ? victimPed->m_fHealth <= 0.f ? 32 : 16
                                : incrementalHit ? 4 : 8,
                        victimPed->m_fContactSurfaceBrightness
                    );
                }
            }
        }
    } else {
        CBulletTraces::AddTrace(startPoint, endPoint, GetType(), firedBy);
    }

    // 0x73C11B [Moved down here]
    if (firedByPed && firedByPed->IsPlayer()) { // 0x73C14B
        firedByPed->AsPlayer()->GetPadFromPlayer()->StartShake_Distance(
            240,
            128,
            FindPlayerPed()->GetPosition()
        );
    }
}

// 0x73C1F0
bool CWeapon::TakePhotograph(CEntity* owner, const CVector* point) {
    UNUSED(owner);

    if (point) {
        if (const auto fx = g_fxMan.CreateFxSystem("camflash", *point, nullptr, false)) {
            fx->PlayAndKill();
        }
    }

    if (CCamera::GetActiveCamera().m_nMode != MODE_CAMERA) {
        return false;
    }

    CPickups::PictureTaken();
    bPhotographHasBeenTaken = true;
    ms_bTakePhoto = true;
    CStats::IncrementStat(STAT_PHOTOGRAPHS_TAKEN, 1.0f);

    // NOTSA - Optimization
    const auto& camMat = TheCamera.GetMatrix();
    const auto& camPos = camMat.GetPosition();

    const auto IsPosInRange = [&](const CVector& worldPos) {
        return (camPos - worldPos).SquaredMagnitude() >= sq(125.f); // NOTSA: Using squared mag.
    };

    // Check is in position in camera's frame
    const auto IsPosInCamFrame = [](const CVector& worldPos) {
        CVector pedHeadPos_Screen;
        if (float _w, _h; !CSprite::CalcScreenCoors(worldPos, &pedHeadPos_Screen, &_w, &_h, false, true)) {
            return false;
        }

        // TODO/BUG: Possibly buggy on bigger screens, because the border becomes too big (because of the relative multiplier) maybe?
        if (   (SCREEN_WIDTH * 0.1f >= pedHeadPos_Screen.x || pedHeadPos_Screen.x >= SCREEN_WIDTH * 0.9f)
            || (SCREEN_HEIGHT * 0.1f >= pedHeadPos_Screen.y || pedHeadPos_Screen.y >= SCREEN_HEIGHT * 0.9f)
        ) {
            return false;
        }

        return true;
    };

    const auto CheckIsLOSBlocked = [&, camFwd = camMat.GetForward()](const CVector& target, CEntity* ignore) {
        CColPoint _cp; // Unused
        CEntity* hitEntity{};
        if (!CWorld::ProcessLineOfSight(
            camPos + camFwd * 2.f,
            target,
            _cp,
            hitEntity,
            true,
            true,
            true,
            true,
            true,
            true,
            false,
            false
        ) || hitEntity == ignore) { // TODO: Here we could set CWorld::pIgnoreEntity to `&ped`, instead of this check.
            return false;
        }
        return true;
    };

    for (auto& ped : GetPedPool()->GetAllValid()) {
        if (IsPosInRange(ped.GetPosition())) {
            continue;
        }

        const auto pedHeadPos = ped.GetBonePosition(BONE_HEAD);

        if (!IsPosInCamFrame(pedHeadPos)) {
            continue;
        }

        if (!CheckIsLOSBlocked(
            pedHeadPos + Normalized(camPos - pedHeadPos) * 1.5f, // Point from ped's head towards camera
            &ped
        )) {
            ped.bHasBeenPhotographed = true;
        }
    }

    for (auto& obj : GetObjectPool()->GetAllValid()) {
        const auto& objPos = obj.GetPosition();

        if (!IsPosInRange(objPos) || !IsPosInCamFrame(objPos)) {
            continue;
        }

        if (!CheckIsLOSBlocked(objPos, &obj)) {
            obj.objectFlags.bIsPhotographed = true;
        }
    }

    return true;
}

// 0x73C710
void CWeapon::SetUpPelletCol(int32 numPellets, CEntity* owner, CEntity* victim, CVector& point, CColPoint& colPoint, CMatrix& outMat) {
    constexpr int32 MAX_NUM_PELLETS = 15;

    assert(numPellets <= MAX_NUM_PELLETS);

    auto* const cm = &ms_PelletTestCol;
    if (!cm->GetData()) {
        cm->AllocateData(0, 0, MAX_NUM_PELLETS, 0, 0, false);
        cm->GetBoundingSphere().Set(1.f, {0.f, 0.f, 0.f});
        cm->m_nColSlot = 0;
    }
    auto* const cd = ms_PelletTestCol.GetData();

    auto hitDir = (colPoint.m_vecPoint - point);
    const float depth = hitDir.NormaliseAndMag() * CWorld::fWeaponSpreadRate * 1.3f;

    //> 0x73C806 - Create pellet lines
    cd->m_nNumLines = (uint8)numPellets;
    const auto lines = cd->GetLines();
    lines[0].Set(
        { 0.f, -depth, 0.f },
        { 0.f,  depth, 0.f }
    );
    for (int32 i = 1; i < numPellets; i++) {
        const auto angle  = CGeneral::GetRandomNumberInRange(-PI, PI);
        const auto spread = CGeneral::GetRandomNumberInRange(0.f, depth * 0.8f);

        const auto oX = std::cos(angle) * spread;
        const auto oZ = std::sin(angle) * spread;

        lines[i].Set(
            { oX, -depth * 2.f, oZ },
            { oX,  depth * 2.f, oZ }
        );
    }

    //> 0x73C923 -  Calculate bounding volumes
    cm->GetBoundingBox().Set(
        { -depth, -depth * 2.f, -depth },
        {  depth,  depth * 2.f,  depth }
    );
    cm->GetBoundingSphere().Set(
        depth * 2.5f,
        {0.f, 0.f, 0.f}
    );

    const auto CalculateMatrixRotation = [&](CVector fwd, CVector zaxis) {
        const auto r = zaxis.Cross(fwd).Normalized();
        outMat.GetForward() = fwd;
        outMat.GetRight()   = r;
        outMat.GetUp()      = r.Cross(fwd);
    };

    if (victim->GetIsTypeBuilding()) { // 0x73C98E
        const auto& n = colPoint.m_vecNormal;
        CalculateMatrixRotation(
            -n,
            std::abs(n.z) >= 0.9f
                ? CVector{0.f, 1.f, 0.f}
                : CVector{1.f, 0.f, 0.f}
        );

    } else  if (std::abs(hitDir.z) <= 0.9f) { // 0x73CA4C
        CalculateMatrixRotation(
            hitDir,
            {0.f, 0.f, 1.f}
        );
    } else if (!owner->GetIsTypePed()) { // 0x73CA59
        CalculateMatrixRotation(
            hitDir,
            {1.f, 0.f, 0.f}
        );
    } else { // 0x73CA5B
        CalculateMatrixRotation(
            hitDir,
            owner->GetForward()
        );
    }

    // 0x73CAFF
    outMat.GetPosition() = colPoint.m_vecPoint;

    // 0x73CB1A
    if (!victim->GetIsTypeBuilding()) {
        outMat.GetPosition() -= colPoint.m_vecNormal.ProjectOnToNormal(outMat.GetForward()) * depth;
    }
}

// 0x73CBA0
void CWeapon::FireInstantHitFromCar2(CVector startPoint, CVector endPoint, CVehicle* vehicle, CEntity* owner) {
    CCrime::ReportCrime(CRIME_FIRE_WEAPON, FindPlayerPed(), FindPlayerPed());

    GetEventGlobalGroup()->Add(CEventGunShot{
        notsa::coalesce<CEntity*>(owner, vehicle),
        startPoint,
        endPoint,
        notsa::contains({WEAPON_PISTOL_SILENCED, WEAPON_TEARGAS}, GetType())
    });
    g_InterestingEvents.Add(CInterestingEvents::EType::INTERESTING_EVENT_22, owner);

    CPointLights::AddLight(PLTYPE_POINTLIGHT, startPoint, {}, 3.0f, 0.25f, 0.22f, 0.0f, 0, false, nullptr);
    CWorld::bIncludeBikers = true;
    CWorld::pIgnoreEntity = vehicle;
    CBirds::HandleGunShot(&startPoint, &endPoint);
    CShadows::GunShotSetsOilOnFire(startPoint, endPoint);

    CEntity* victim{};
    CColPoint cpImpact{};
    CWorld::ProcessLineOfSight(startPoint, endPoint, cpImpact, victim, true, true, true, true, true, false, false, true);
    CWorld::ResetLineTestOptions();
    DoBulletImpact(owner, victim, startPoint, endPoint, cpImpact, 0);
}

// 0x73CDC0
void CWeapon::DoDoomAiming(CEntity* owner, CVector* start, CVector* end) {
    int16 inRangeCount{};
    std::array<CEntity*, 16> objInRange{};
    CWorld::FindObjectsInRange(*start, (*start - *end).Magnitude(), true, &inRangeCount, (int16)objInRange.size(), objInRange.data(), false, true, true, false, false);

    CEntity* closestEntity{};
    float    closestDist{ 10'000 };
    for (auto entity : std::span{ objInRange.begin(), (size_t)inRangeCount }) {
        if (entity == owner || owner->AsPed()->CanSeeEntity(entity, PI / 8.f)) { // todo: add check owner->GetIsTypePed() NOTSA
            continue;
        }

        switch (entity->GetStatus()) {
        case STATUS_TRAIN_MOVING:
        case STATUS_TRAIN_NOT_MOVING:
        case STATUS_WRECKED:
            continue;
        }

        const auto dir = entity->GetPosition() - owner->GetPosition();
        if (const auto dist2D = dir.Magnitude2D(); std::abs(dir.z) * 1.5f < dist2D) {
            const auto dist3D = std::hypot(dist2D, dir.z);
            if (dist3D < closestDist) {
                closestEntity = entity;
                closestDist = dist3D;
            }
        }
    }

    if (closestDist < 9000.f) {
        // assert(closestEntity); // We should have one, because by default `closestDist` is FLT_MAX (originally 10 000)

        {
            CEntity*  _hitEntity{}; // Unused
            CColPoint _cp;          // Unused
            if (CWorld::ProcessLineOfSight(*start, closestEntity->GetPosition(), _cp, _hitEntity, true, false, false, false, false, false, false, true)) {
                return;
            }
        }

        float targetZ = closestEntity->GetPosition().z + 0.3f;
        if (closestEntity->GetIsTypePed() && closestEntity->AsPed()->bIsDucking) {
            targetZ -= 0.8f; // Effectively only -0.5 relative to the original Z
        }
        const auto t = (*start - *end).Magnitude2D() / (*start - closestEntity->GetPosition()).Magnitude2D();
        end->z = start->z + (targetZ - start->z) * t; // Re-ordered a little
    }
}

// 0x73D1E0
void CWeapon::DoTankDoomAiming(CEntity* vehicle, CEntity* owner, CVector* startPoint, CVector* endPoint) {
    const CVector dir  = *endPoint - *startPoint;
    const CVector start2D{ startPoint->x, startPoint->y, 0.f };
    const CVector end2D{ endPoint->x, endPoint->y, 0.f };

    int16                    numInRange{};
    std::array<CEntity*, 16> inRange{};
    CWorld::FindObjectsInRange(*startPoint, dir.Magnitude(), true, &numInRange, 15, inRange.data(), false, true, false, false, false);

    float       closestDist = 10'000.f;
    int16       closestIdx{};
    const float slope = (endPoint->z - startPoint->z) / dir.Magnitude();
    for (int16 i = 0; i < numInRange; i++) {
        const auto entity = inRange[i];
        if (vehicle == entity || owner == entity) {
            continue;
        }
        if (entity->GetStatus() == STATUS_TRAIN_MOVING || entity->GetStatus() == STATUS_TRAIN_NOT_MOVING) {
            continue;
        }
        // NOTSA: Original checks bit 29 of `CPhysical::m_nPhysicalFlags` (`bRenderScorched`)
        if (entity->GetIsTypeVehicle() && entity->AsPhysical()->physicalFlags.bRenderScorched) {
            continue;
        }

        const auto vehPos    = vehicle->GetPosition();
        const auto entPos    = entity->GetPosition();
        const auto dist2D    = std::sqrt((vehPos.x - entPos.x) * (vehPos.x - entPos.x) + (vehPos.y - entPos.y) * (vehPos.y - entPos.y));
        const auto heightEst = dist2D * slope;
        const auto zDiff     = vehPos.z - (heightEst + entPos.z);
        const auto absZDiff  = zDiff >= 0.f ? zDiff : -zDiff;
        if (!(absZDiff * 3.f < dist2D)) {
            continue;
        }

        const CVector entPos2D{ entPos.x, entPos.y, 0.f };
        const auto    boundRadius = CModelInfo::GetModelInfo(entity->m_nModelIndex)->GetColModel()->GetBoundRadius();
        if (CCollision::DistToLine(start2D, end2D, entPos2D) < boundRadius * 3.f) {
            const auto dist3D = std::sqrt(dist2D * dist2D + absZDiff * absZDiff);
            if (dist3D < closestDist) {
                closestIdx  = i;
                closestDist = dist3D;
            }
        }
    }

    if (numInRange > 0 && closestDist < 9000.f) {
        const auto target = inRange[closestIdx]->GetPosition();
        const auto dirLen2D = std::sqrt((endPoint->y - startPoint->y) * (endPoint->y - startPoint->y) + (endPoint->x - startPoint->x) * (endPoint->x - startPoint->x));
        const auto tgtLen2D = std::sqrt((target.x - startPoint->x) * (target.x - startPoint->x) + (target.y - startPoint->y) * (target.y - startPoint->y));
        endPoint->z = (dirLen2D / tgtLen2D) * ((target.z + 0.3f) - startPoint->z) + startPoint->z;
    }
}

// 0x73D720
void CWeapon::DoDriveByAutoAiming(CEntity* owner, CVehicle* vehicle, CVector* startPoint, CVector* endPoint, bool canAimVehicles) {
    if (!owner) {
        return;
    }

    const auto radius = (*endPoint - *startPoint).Magnitude();

    int16                    numPeds{}, numVehicles{};
    std::array<CEntity*, 32> inRange{};
    CWorld::FindObjectsInRange(*startPoint, radius, true, &numPeds, 16, inRange.data(), false, false, true, false, false);
    if (canAimVehicles) {
        CWorld::FindObjectsInRange(*startPoint, radius, true, &numVehicles, 16, inRange.data() + numPeds, false, true, false, false, false);
    }
    const auto numInRange = (int16)(numPeds + numVehicles);

    float closestScore = 10'000.f;
    int16 closestIdx{};
    for (int16 i = 0; i < numInRange; i++) {
        const auto entity = inRange[i];
        if (entity == owner) {
            continue;
        }
        if (entity->GetIsTypePed()) {
            const auto ped = entity->AsPed();
            if (ped->m_nPedState == PEDSTATE_DIE || ped->m_nPedState == PEDSTATE_DEAD || ped->m_pAttachedTo == vehicle) {
                continue;
            }
        }

        const auto entPos = entity->GetPosition();
        float      score  = CCollision::DistToLine(*startPoint, *endPoint, entPos);
        if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_PLANE || vehicle->m_nVehicleSubType == VEHICLE_TYPE_HELI) {
            const auto distToVeh = (entPos - vehicle->GetPosition()).Magnitude();
            score /= (distToVeh >= 5.f ? distToVeh : 5.f);
        } else {
            score += (entPos - owner->GetPosition()).Magnitude() * 0.15f;
        }

        const auto dir = *endPoint - *startPoint;
        if ((dir.z * (entPos.z - startPoint->z) + dir.y * (entPos.y - startPoint->y)) + dir.x * (entPos.x - startPoint->x) > 0.f) { // NOTE: Sum order as in the original
            if (score < closestScore) {
                closestScore = score;
                closestIdx   = i;
            }
        }
    }

    const auto autoAimAngle = vehicle->GetPlaneGunsAutoAimAngle();
    const auto maxScore     = autoAimAngle > 0.5f
        ? std::tan(autoAimAngle * 0.017453292f)
        : 2.5f;
    if (closestScore < maxScore) {
        const auto target = inRange[closestIdx]->GetPosition();
        const auto scale  = (*startPoint - *endPoint).Magnitude() / (*startPoint - target).Magnitude();
        *endPoint = (target - *startPoint) * scale + *startPoint;
    }
}

// 0x73DB40
void CWeapon::Update(CPed* owner) {
    const auto wi = &GetWeaponInfo(owner);
    const auto ao = &wi->GetAimingOffset();

    const auto ProcessReloadAudioIf = [&](auto Pred) {
        const auto ProcessOne = [&](uint32 delay, eAudioEvents ae) {
            if (Pred(delay, ae)) {
                owner->GetWeaponAE().AddAudioEvent(ae);
            }
        };
        ProcessOne(owner->bIsDucking ? ao->CrouchRLoadA : ao->RLoadA, AE_WEAPON_RELOAD_A);
        ProcessOne(owner->bIsDucking ? ao->CrouchRLoadB : ao->RLoadB, AE_WEAPON_RELOAD_B);
    };

    switch (m_State) {
    case WEAPONSTATE_FIRING: {
        if (owner && notsa::contains({ WEAPON_SPAS12_SHOTGUN, WEAPON_SHOTGUN }, m_Type)) { // 0x73DBA5    
            ProcessReloadAudioIf([&](uint32 rload, eAudioEvents ae) {
                if (!rload) {
                    return false;
                }
                const auto nextShotEnd = m_TimeForNextShotMs + rload;
                return CTimer::GetPreviousTimeInMS() < nextShotEnd && CTimer::GetTimeInMS() >= nextShotEnd;
            });
        }
        if (CTimer::GetTimeInMS() > m_TimeForNextShotMs) {
            m_State = wi->m_nWeaponFire == eWeaponFire::WEAPON_FIRE_MELEE || m_TotalAmmo != 0
                ? eWeaponState::WEAPONSTATE_READY
                : eWeaponState::WEAPONSTATE_OUT_OF_AMMO;
        }
        break;
    }
    case WEAPONSTATE_RELOADING: {
        if (owner && m_Type < WEAPON_LAST_WEAPON) {
            const auto DoPlayAnimlessReloadAudio = [&] {
                ProcessReloadAudioIf([
                    &,
                    shootDelta = m_TimeForNextShotMs - wi->GetWeaponReloadTime()
                ](uint32 rload, eAudioEvents ae) {
                    const auto audioTimeMs = rload + shootDelta;
                    return CTimer::GetPreviousTimeInMS() < audioTimeMs && CTimer::GetTimeInMS() >= audioTimeMs;
                });
            };
            if (wi->flags.bReload && (!owner->IsPlayer() || !FindPlayerInfo().m_bFastReload)) { // 0x73DCCE
                auto animRLoad = RpAnimBlendClumpGetAssociation(
                    owner->GetRpClump(),
                    ANIM_ID_RELOAD //(wi->m_Flags & 0x1000) != 0 ? ANIM_ID_RELOAD : ANIM_ID_WALK // Always going to be `ANIM_ID_RELOAD`
                );
                if (!animRLoad) {
                    animRLoad = RpAnimBlendClumpGetAssociation(owner->GetRpClump(), wi->GetCrouchReloadAnimationID());
                }
                if (animRLoad) { // 0x73DD30
                    ProcessReloadAudioIf([&](uint32 rloadMs, eAudioEvents ae) {
                        const auto rloadS = (float)rloadMs / 1000.f;
                        return rloadS <= animRLoad->m_CurrentTime && animRLoad->m_CurrentTime - animRLoad->m_TimeStep < rloadS;
                    });
                    if (CTimer::GetTimeInMS() > m_TimeForNextShotMs) {
                        if (animRLoad->GetTimeProgress() < 0.9f) {
                            m_TimeForNextShotMs = CTimer::GetTimeInMS();
                        }
                    }
                } else if (owner->GetIntelligence()->GetTaskUseGun()) { // 0x73DDF9
                    if (CTimer::GetTimeInMS() > m_TimeForNextShotMs) {
                        m_TimeForNextShotMs = CTimer::GetTimeInMS();
                    }
                } else { // 0x73DE16
                    DoPlayAnimlessReloadAudio();
                }
            } else {
                DoPlayAnimlessReloadAudio();
            }
        }
        //> 0x73DEA4
        if (CTimer::GetTimeInMS() > m_TimeForNextShotMs) {
            Reload(owner);
            m_State = WEAPONSTATE_READY;
        }
        StopWeaponEffect();
        break;
    }
    case WEAPONSTATE_MELEE_MADECONTACT: {
        m_State = WEAPONSTATE_READY;
        StopWeaponEffect();
        break;
    }
    default: {
        StopWeaponEffect();
        break;
    }
    }
}

// 0x73A360
void CWeapon::UpdateWeapons() {
    ZoneScoped;

    CShotInfo::Update();
    CExplosion::Update();
    CProjectileInfo::Update();
    CBulletInfo::Update();
}

// 0x73DEF0
bool CWeapon::CanBeUsedFor2Player() {
    return CanBeUsedFor2Player(m_Type);
}

// 0x73E240
CEntity* CWeapon::FindNearestTargetEntityWithScreenCoors(float screenX, float screenY, float range, CVector point, float* outScrX, float* outScrY) {
    float closestScrDist = SCREEN_WIDTH * (1.f / 15.f); // 0x863E0C
    screenX              = (screenX + 1.f) * SCREEN_WIDTH * 0.5f;
    screenY              = (screenY + 1.f) * SCREEN_HEIGHT * 0.5f;

    CEntity* closest{};
    const auto ProcessEntity = [&](CEntity* e) {
        const auto epos = e->GetPosition();

        CVector scrPos{};
        CVector scrSz{};
        if (!CSprite::CalcScreenCoors(epos, &scrPos, &scrSz.x, &scrSz.y, true, true)) {
            return;
        }
        const auto dx = scrPos.x - screenX;
        const auto dy = scrPos.y - screenY;
        const auto scrDist = std::sqrt(dx * dx + dy * dy);
        if (!(scrDist < closestScrDist)) {
            return;
        }
        if (!((epos.x - point.x) * (epos.x - point.x) + (epos.y - point.y) * (epos.y - point.y) + (epos.z - point.z) * (epos.z - point.z) < range * range)) {
            return;
        }
        closestScrDist = scrDist;
        closest        = e;

        // BUG: Original only checks `outScrX` for null, but writes both
        if (outScrX && (outScrY || !notsa::IsFixBugs())) {
            *outScrX = scrPos.x / (SCREEN_WIDTH * 0.5f) - 1.f;
            *outScrY = scrPos.y / (SCREEN_HEIGHT * 0.5f) - 1.f;
        }
    };

    // NOTSA: Original iterates the pools from the last slot to the first (matters on ties)
    const auto pedPool = GetPedPool();
    for (auto i = (int32)pedPool->GetSize(); i-- > 0;) {
        const auto ped = pedPool->GetAt(i);
        if (!ped) {
            continue;
        }
        if (ped->IsStateDead() || ped->bInVehicle) {
            continue;
        }
        if (!CDarkel::ThisPedShouldBeKilledForFrenzy(*ped)) {
            continue;
        }
        ProcessEntity(ped);
    }

    const auto vehPool = GetVehiclePool();
    for (auto i = (int32)vehPool->GetSize(); i-- > 0;) {
        const auto veh = vehPool->GetAt(i);
        if (!veh) {
            continue;
        }
        if (veh == FindPlayerVehicle()) {
            continue;
        }
        if (!CDarkel::ThisVehicleShouldBeKilledForFrenzy(*veh)) {
            continue;
        }
        ProcessEntity(veh);
    }

    return closest;
}

// notsa
auto CWeapon::GetProjectileType() {
    switch (GetType()) {
    case eWeaponType::WEAPON_RLAUNCHER:
        return eWeaponType::WEAPON_ROCKET;
    case eWeaponType::WEAPON_RLAUNCHER_HS:
        return eWeaponType::WEAPON_ROCKET_HS;
    case WEAPON_GRENADE:
    case WEAPON_TEARGAS:
    case WEAPON_MOLOTOV:
    case WEAPON_REMOTE_SATCHEL_CHARGE:
        return GetType();
    default:
        NOTSA_UNREACHABLE();
    }
}

// 0x73E560
float CWeapon::EvaluateTargetForHeatSeekingMissile(CEntity* potentialTarget, const CVector& origin, const CVector& aimingDir, float tolerance, bool arePlanesPriority, CEntity* preferredExistingTarget) {
    const auto potentialTargetDist = (origin - potentialTarget->GetPosition()).Magnitude();

    const auto potentialTargetDistToLine = CCollision::DistToLine(origin, origin + aimingDir * 250.f, potentialTarget->GetPosition());
    auto ret = std::sqrt(potentialTargetDist) / 10.f + potentialTargetDistToLine / potentialTargetDist;

    if (potentialTargetDistToLine * tolerance >= potentialTargetDist) {
        return -1.f;
    }

    if (arePlanesPriority) {
        if (potentialTarget->GetIsTypeVehicle() && notsa::contains({ VEHICLE_TYPE_PLANE, VEHICLE_TYPE_HELI }, potentialTarget->AsVehicle()->m_nVehicleSubType)) {
            ret *= 0.25f;
        }
    }

    if (preferredExistingTarget && preferredExistingTarget == potentialTarget) {
        ret *= 0.25f;
    }

    return ret;
}

// 0x73E690
void CWeapon::DoWeaponEffect(CVector origin, CVector dir) {
    const char* fxName{};
    switch (m_Type) {
    case eWeaponType::WEAPON_FLAMETHROWER: fxName = "flamethrower"; break;
    case eWeaponType::WEAPON_EXTINGUISHER: fxName = "extinguisher"; break;
    case eWeaponType::WEAPON_SPRAYCAN:     fxName = "spraycan";     break;
    default:                               StopWeaponEffect();      return;
    }

    const auto mat = RwMatrixCreate();
    g_fx.CreateMatFromVec(mat, &origin, &dir);

    if (m_FxSystem) {
        m_FxSystem->SetMatrix(mat);
    } else {
        m_FxSystem = g_fxMan.CreateFxSystem(fxName, CVector{}, mat, false);

        if (!m_FxSystem) {
            RwMatrixDestroy(mat);
            return;
        }

        m_FxSystem->CopyParentMatrix();
        m_FxSystem->Play();
        m_FxSystem->SetMustCreatePrts(true);
    }
    m_FxSystem->SetConstTime(1, 1.0f);

    RwMatrixDestroy(mat);
}

// 0x73E800
bool CWeapon::FireAreaEffect(CEntity* firingEntity, const CVector& origin, CEntity* targetEntity, CVector* target) {
    const auto wi = &GetWeaponInfo(); // TODO/NOTE: Why not `GetWeaponInfo(firingEntity)`?
    const auto [shotDir, shotPt] = [&]() -> std::pair<CVector, CVector> {
        if (!targetEntity && !target) {
            if (firingEntity == FindPlayerPed() && TheCamera.m_aCams[0].Using3rdPersonMouseCam()) {
                CVector camPos, camTargetPos;
                TheCamera.Find3rdPersonCamTargetVector(wi->m_fWeaponRange, origin, camPos, camTargetPos);
                return {
                    (camTargetPos - camPos) / wi->m_fWeaponRange, // Scale to a unit vector
                    camTargetPos
                }; 
            } else {
                // NOTE: Moved here from `0x73E83F`
                // NOTE: Original code used degs instead of radians
                //       and then converted back to radians... So we're gonna stick to radians only ;)
                const auto heading = [&] { // 0x73E83F
                    if (targetEntity) {
                        return (targetEntity->GetPosition() - origin).Heading();
                    }
                    if (target) {
                        return (*target - origin).Heading();
                    }
                    return firingEntity->GetHeading();
                }();
                CVector dir{
                    -std::sin(heading),
                    std::cos(heading),
                    0.f
                };
                if (firingEntity->GetIsTypePed()) {
                    if (const auto pd = firingEntity->AsPed()->GetPlayerData()) {
                        dir.z = -std::tan(pd->m_fLookPitch);
                    }
                }
                return { dir, origin + dir };
            }
        } else {
            const auto ptTarget = target
                ? *target
                : targetEntity->GetIsTypePed()
                    ? targetEntity->AsPed()->GetBonePosition(BONE_SPINE1)
                    : targetEntity->GetPosition();
            return { (ptTarget - origin).Normalized(), ptTarget };
        }
    }();
    CShotInfo::AddShot(firingEntity, m_Type, origin, shotPt);
    DoWeaponEffect(origin, shotDir);
    if (m_Type == WEAPON_FLAMETHROWER && CGeneral::RandomBool(1.f / 3.f * 100.f)) {
        if (CCreepingFire::TryToStartFireAtCoors(
            shotDir * CVector::Random(3.5f, 6.f) + origin + CVector{0.f, 0.f, 0.5f},
            0,
            true,
            false,
            2.3f)
        ) {
            CStats::IncrementStat(STAT_FIRES_STARTED);
        }
    }
    CCrime::ReportCrime(CRIME_FIRE_WEAPON, nullptr, firingEntity->AsPed());
    return true;
}

// 0x73EC40
bool CWeapon::FireInstantHitFromCar(CVehicle* vehicle, bool leftSide, bool rightSide) {
    const auto wi = CWeaponInfo::GetWeaponInfo(m_Type, eWeaponSkill::STD);
    const auto mi = CModelInfo::GetModelInfo(vehicle->m_nModelIndex)->AsVehicleModelInfoPtr();
    const auto driver = vehicle->m_pDriver;

    // BUG: Original doesn't check if there's a driver (except for bikes)
    const auto TransformByDriverHand = [&](CVector& pos) {
        const auto hier = GetAnimHierarchyFromSkinClump(driver->GetRpClump());
        const auto idx  = RpHAnimIDGetIndex(hier, driver->m_apBones[PED_NODE_RIGHT_HAND]->BoneTag);
        RwV3dTransformPoints(&pos, &pos, 1, &RpHAnimHierarchyGetMatrixArray(hier)[idx]);
    };
    const auto ComputeEndFromStart = [&](const CVector& start) -> CVector {
        const auto  range = wi->m_fWeaponRange;
        const auto& mat   = vehicle->GetMatrix();
        if (leftSide) {
            const auto& right = mat.GetRight();
            return { start.x - range * right.x, start.y - range * right.y, start.z - range * right.z };
        }
        const auto& dir = rightSide ? mat.GetRight() : mat.GetForward();
        return { range * dir.x + start.x, range * dir.y + start.y, range * dir.z + start.z };
    };

    CVector start{}, end{};
    if (vehicle->m_nVehicleType != VEHICLE_TYPE_BIKE) {
        if (rightSide) {
            start.x = wi->m_vecFireOffset.x * 1.8f;
            start.y = wi->m_vecFireOffset.y * 1.8f;
            start.z = wi->m_vecFireOffset.z * 1.8f - 0.1f;
        } else {
            start = wi->m_vecFireOffset;
        }
        TransformByDriverHand(start);
        start += CTimer::ms_fTimeStep * vehicle->m_vecMoveSpeed;
        end = ComputeEndFromStart(start);
    } else if (driver) {
        start = wi->m_vecFireOffset;
        TransformByDriverHand(start);
        start += CTimer::ms_fTimeStep * vehicle->m_vecMoveSpeed;
        end = ComputeEndFromStart(start);
    } else {
        // Driver-less bike
        const auto& mat  = vehicle->GetMatrix();
        const auto& seat = mi->GetFrontSeatPosn();
        CVector     localStart{}, localEnd{};
        if (leftSide) {
            const auto r  = rand();
            const auto cm = vehicle->GetColModel();
            localStart = CVector{
                -cm->GetBoundingBox().m_vecMax.x - 0.25f,
                (seat.y - 0.05f) + (float)(r & 0xFF) * 0.001f,
                seat.z + 0.63f
            };
            localEnd = CVector{ -wi->m_fWeaponRange, seat.y, seat.z + 0.6f };
        } else if (rightSide) {
            const auto r  = rand();
            const auto cm = vehicle->GetColModel();
            localStart = CVector{
                cm->GetBoundingBox().m_vecMax.x + 0.25f,
                (seat.y - 0.18f) + (float)(r & 0xFF) * 0.001f,
                seat.z + 0.52f
            };
            localEnd = CVector{ wi->m_fWeaponRange, seat.y, seat.z + 0.5f };
        } else {
            const auto cm = vehicle->GetColModel();
            const auto r  = rand();
            localStart = CVector{
                (float)(r & 0xFF) * 0.001f - 0.4f,
                (cm->GetBoundingBox().m_vecMax.y + seat.y) + 0.2f,
                seat.z + 0.55f
            };
            localEnd = CVector{ 0.f, wi->m_fWeaponRange, seat.z + 0.5f };
        }
        start = mat.TransformPoint(localStart);
        start += CTimer::ms_fTimeStep * vehicle->m_vecMoveSpeed;
        end   = mat.TransformPoint(localEnd);
    }

    // Add some inaccuracy
    const auto r1 = rand();
    const auto r2 = rand();
    const auto noiseX = (float)(r2 & 0xFF) * 0.01f - 1.28f;
    const auto noiseY = (float)(r1 & 0xFF) * 0.01f - 1.28f;
    const auto r3 = rand();
    end.x += noiseX;
    end.y += noiseY;
    end.z += (float)(r3 & 0xFF) * 0.01f - 1.28f;

    DoDriveByAutoAiming(FindPlayerPed(), vehicle, &start, &end, false);
    FireInstantHitFromCar2(start, end, vehicle, vehicle->m_pDriver);
    return true;
}

// 0x73F480
bool CWeapon::CheckForShootingVehicleOccupant(CEntity** pCarEntity, CColPoint* colPoint, eWeaponType weaponType, const CVector& origin, const CVector& target) {
    const auto veh = (*pCarEntity)->AsVehicle();
    if (!veh->GetIsTypeVehicle()) {
        return false; // NOTSA: Original returns garbage here
    }

    const CColPoint savedColPoint = *colPoint;
    float           depth         = 1.f;
    bool            hitOccupant   = false;
    CColLine        line{ origin, target };

    // Test if line goes through the occupants' heads
    const auto CheckOccupant = [&](CPed* ped) {
        if (!ped || !ped->bCanBeShotInVehicle) {
            return;
        }
        CVector headPos{};
        const auto hier = GetAnimHierarchyFromSkinClump(ped->GetRpClump());
        const auto idx  = RpHAnimIDGetIndex(hier, ped->m_apBones[PED_NODE_HEAD]->BoneTag);
        RwV3dTransformPoints(&headPos, &headPos, 1, &RpHAnimHierarchyGetMatrixArray(hier)[idx]);
        headPos.z += 0.1f;

        CColSphere sphere{};
        sphere.Set(0.2f, headPos, SURFACE_DEFAULT, 9, tColLighting{ 0xFF });
        if (CCollision::ProcessLineSphere(line, sphere, *colPoint, depth)) {
            *pCarEntity = ped;
            hitOccupant = true;
        }
    };
    CheckOccupant(veh->m_pDriver);
    for (const auto passenger : veh->m_apPassengers) {
        CheckOccupant(passenger);
    }

    // Test if the shot went through the windscreen (shot from the front, of an automobile)
    if (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
        const auto& mat = veh->GetMatrix();
        const auto  dir = target - origin;
        const auto& fwd = mat.GetForward();
        if (dir.x * fwd.x + (dir.y * fwd.y + dir.z * fwd.z) < 0.f) { // NOTE: Sum order as in the original
            const auto& up = mat.GetUp();
            if (dir.x * up.x + (dir.y * up.y + dir.z * up.z) <= 0.f || veh->vehicleFlags.bIsBig) {
                const auto cm = veh->GetColModel();
                const auto cd = cm->m_pColData;
                if (cd && cd->m_nNumTriangles > 0) { // NOTSA: Original doesn't check `cd` for null
                    const CMatrix invMat = Invert(mat);
                    line.m_vecStart = invMat.TransformPoint(line.m_vecStart);
                    line.m_vecEnd   = invMat.TransformPoint(line.m_vecEnd);
                    CCollision::CalculateTrianglePlanes(cm);
                    for (int16 i = 0; i < (int16)cd->m_nNumTriangles; i++) {
                        const auto& tri = cd->m_pTriangles[i];
                        if (!g_surfaceInfos.IsGlass(tri.GetSurfaceType())) {
                            continue;
                        }
                        if (!CCollision::TestLineTriangle(line, cd->m_pVertices, tri, cd->m_pTrianglePlanes[i])) {
                            continue;
                        }
                        const auto automobile = veh->AsAutomobile();
                        auto&      dmgMgr     = automobile->m_damageManager;
                        if (dmgMgr.ProgressPanelDamage(WINDSCREEN_PANEL)) {
                            if (dmgMgr.GetPanelStatus(WINDSCREEN_PANEL) == DAMSTATE_DAMAGED) {
                                dmgMgr.ProgressPanelDamage(WINDSCREEN_PANEL);
                            }
                            automobile->SetPanelDamage(WINDSCREEN_PANEL, true);
                        }
                        break;
                    }
                }
            }
        }
    }

    if (!hitOccupant) {
        *pCarEntity = veh;
        *colPoint   = savedColPoint;
    }
    return hitOccupant; // NOTSA: Original returns garbage
}

// 0x73F910
CEntity* CWeapon::PickTargetForHeatSeekingMissile(CVector origin, CVector direction, float distanceMultiplier, CEntity* ignoreEntity, bool arePlanesPriority, CEntity* preferredExistingTarget) {
    float minRating  = FLT_MAX;
    CEntity* minRated{};
    const auto point = origin + direction * 5.f;
    for (auto& veh : GetVehiclePool()->GetAllValid()) {
        if (&veh == ignoreEntity) {
            continue;
        }
        if (!veh.vehicleFlags.bVehicleCanBeTargettedByHS) {
            continue;
        }
        if (veh.m_fHealth <= 0.f) {
            continue;
        }
        const auto rating = EvaluateTargetForHeatSeekingMissile(&veh, point, direction, distanceMultiplier, arePlanesPriority, preferredExistingTarget);
        if (rating >= 0.f && rating <= minRating) {
            minRating = rating;
            minRated  = &veh;
        }
    }
    return minRated;
}

// 0x73FA20
bool CWeapon::FireFromCar(CVehicle* vehicle, bool leftSide, bool rightSide) {
    if (m_State != WEAPONSTATE_READY || m_AmmoInClip <= 0) {
        return false;
    }
    if (!CWeapon::FireInstantHitFromCar(vehicle, leftSide, rightSide)) {
        return notsa::IsFixBugs() ? false : true;
    }
    if (const auto d = vehicle->m_pDriver) {
        d->GetWeaponAE().AddAudioEvent(AE_WEAPON_FIRE);
    }
    if (!CCheat::IsActive(CHEAT_INFINITE_AMMO)) {
        if (m_AmmoInClip) { // NOTE: I'm pretty sure this is redundant
            m_AmmoInClip--;
        }
        if (  m_TotalAmmo < 25'000 && m_TotalAmmo > 0
            && (vehicle->GetStatus() != STATUS_PLAYER || CStats::GetPercentageProgress() < 100.f)
        ) {
            m_TotalAmmo--;
        }
    }
    m_State = WEAPONSTATE_FIRING;
    if (m_AmmoInClip) {
        m_TimeForNextShotMs = CTimer::GetTimeInMS() + 1000; // NOTE: Shoot delay can be adjusted here
    } else if (m_TotalAmmo) {
        m_State = WEAPONSTATE_RELOADING;
        m_TimeForNextShotMs = CTimer::GetTimeInMS() + GetWeaponInfo().GetWeaponReloadTime();
    }
    return true;
}

//! NOTSA: Original used the CRT `rand()` scaled by `1 / 32767` (0x858C7C), the intermediate results stay in an x87 register
static double FireInstantHit_Rand01() {
    return (double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL;
}

//! NOTSA: `CGeneral::GetRandomNumberInRange<float>` (0x41BD90), but without the assertion
static float FireInstantHit_RandomInRange(float lo, float hi) {
    return (float)((double)lo + ((double)hi - (double)lo) * FireInstantHit_Rand01());
}

/*!
* @brief Randomly offsets the point of impact (Used for non-player shooters, and for vehicles)
* @note The order of the `rand()` calls is: Z, Y, X
*/
static void FireInstantHit_ApplyRandomSpread(CVector& pt, float spread) {
    const auto rz = (float)FireInstantHit_Rand01();
    const auto ry = (float)FireInstantHit_Rand01();
    const auto rx = FireInstantHit_Rand01();

    const auto offsX = (rx * 0.4f - 0.2f) * spread;
    const auto offsY = ((double)ry * 0.4f - 0.2f) * spread;
    const auto offsZ = (float)(((double)rz * 0.2f - 0.1f) * spread);

    pt.x = (float)(offsX + pt.x);
    pt.y = (float)(pt.y + offsY);
    pt.z = offsZ + pt.z;
}

/*!
* @brief Makes the point of impact wobble in a circle (that is on the plane of `sinDir` and `cosDir`) depending on the time (Used for players)
*/
static void FireInstantHit_ApplyAimSway(CVector& pt, const CVector& sinDir, const CVector& cosDir, float spread) {
    constexpr auto SWAY_ROT_RATE = 0.0062831854f; // 0x8D6118 (2 * PI / 1000)

    const auto angle = (double)CTimer::GetTimeInMS() * (double)SWAY_ROT_RATE;

    // Part 1
    const auto sinZ = (double)sinDir.z * spread; // Not rounded to float
    const auto sinY = (float)((double)sinDir.y * spread);
    const auto sinX = (float)((double)sinDir.x * spread);
    const auto s    = (float)std::sin(angle);
    pt.x = (float)((double)sinX * s + pt.x);
    pt.y = (float)(pt.y + (double)sinY * s);
    const auto z = sinZ * s + pt.z; // Not rounded to float

    // Part 2
    const auto cosZ = (double)cosDir.z * spread; // Not rounded to float
    const auto cosY = (float)((double)cosDir.y * spread);
    const auto cosX = (float)((double)cosDir.x * spread);
    const auto c    = (float)std::cos(angle);
    pt.x = (float)((double)cosX * c + pt.x);
    pt.y = (float)(pt.y + (double)cosY * c);
    pt.z = (float)(cosZ * c + z);
}

//! @brief `start + dir * range`, with the same rounding as the original
static CVector FireInstantHit_PointAlongDir(const CVector& start, const CVector& dir, double range) {
    const auto offsY = (float)((double)dir.y * range);
    const auto offsZ = (float)((double)dir.z * range);
    return {
        (float)(start.x + (double)dir.x * range),
        offsY + start.y,
        start.z + offsZ
    };
}

// 0x73FB10
bool CWeapon::FireInstantHit(CEntity* firingEntity, CVector* origin, CVector* muzzlePosn, CEntity* targetEntity, CVector* target, CVector* originForDriveBy, bool arg6, bool muzzle) {
    constexpr auto PLAYER_AIM_SCALE      = 0.75f;           // 0x8D6110
    constexpr auto PLAYER_AIM_SCALE_DIST = 5.00f;           // 0x8D6114
    constexpr auto SHOTGUN_SPREAD_RATE   = 0.05f;           // 0x8D611C
    constexpr auto SHOTGUN_NUM_PELLETS   = 15;              // 0x8D6120
    constexpr auto SPAS_NUM_PELLETS      = 8;               // 0x8D6124 (NOTE: The value in the exe is 8, not 4)
    constexpr auto MIN_AIM_DIR_MAG       = 0.01f;           // 0x858C58

    assert(firingEntity);

    const auto ped = firingEntity->GetIsTypePed()
        ? firingEntity->AsPed()
        : nullptr;
    const auto wi = CWeaponInfo::GetWeaponInfo(m_Type, ped ? ped->GetWeaponSkill(m_Type) : eWeaponSkill::STD);

    CVector   muzzlePos = *muzzlePosn;
    CVector   start     = *origin;
    CVector   endPt{};                       // Where the shot ends (In some cases this is a direction for a while)
    CVector   dir{};                         // Direction of the shot - BUG: Left uninitialized in the original, but used by the muzzle flash code (if `muzzle` is set)
    CVector   camSource{};                   // Start of the line of sight test when the player is aiming
    CEntity*  hitEntity = nullptr;
    CColPoint colPoint{};
    float     spread    = 0.f;               // Accuracy modifier

    if (originForDriveBy) {
        start = *originForDriveBy;
    }

    CTaskSimpleUseGun* taskUseGun{};
    if (ped) {
        spread = (float)((100.0 - (double)ped->m_nWeaponAccuracy) / (double)wi->m_fAccuracy);
        if (ped->GetPlayerData() && ped->bIsDucking) {
            spread *= 0.5f;
        }
        taskUseGun = ped->GetIntelligence()->GetTaskUseGun();
    }

    if (notsa::contains({ WEAPON_SHOTGUN, WEAPON_SAWNOFF_SHOTGUN, WEAPON_SPAS12_SHOTGUN }, m_Type)) {
        spread = 0.f;
        CWorld::fWeaponSpreadRate = SHOTGUN_SPREAD_RATE / wi->m_fAccuracy;
    }

    // Does the hit test (used by every case below)
    const auto DoLineOfSight = [&](const CVector& from) {
        CBirds::HandleGunShot(&from, &endPt);
        CShadows::GunShotSetsOilOnFire(from, endPt);
        CWorld::ProcessLineOfSight(from, endPt, colPoint, hitEntity, true, true, true, true, true, false, false, true);
    };

    // 0x7407C1 - Default aiming (Direction of the shooter, with some inaccuracy)
    const auto DoDefaultAiming = [&] {
        if (firingEntity->GetIsTypeVehicle()) {
            const auto veh = firingEntity->AsVehicle();

            spread = 0.6f;
            endPt  = firingEntity->GetMatrix().GetForward();
            dir    = endPt;

            const auto status = veh->GetStatus();
            if (status == STATUS_PLAYER || status == STATUS_REMOTE_CONTROLLED) {
                endPt = FireInstantHit_PointAlongDir(start, endPt, wi->m_fWeaponRange);

                DoDriveByAutoAiming(
                    status == STATUS_REMOTE_CONTROLLED
                        ? FindPlayerPed(-1)
                        : veh->m_pDriver,
                    veh,
                    &start,
                    &endPt,
                    notsa::contains({ VEHICLE_TYPE_PLANE, VEHICLE_TYPE_HELI }, veh->m_nVehicleType)
                );

                endPt = CVector{
                    endPt.x - start.x,
                    endPt.y - start.y,
                    endPt.z - start.z
                };
                endPt.Normalise();

                spread = notsa::contains({ MODEL_SEASPAR, MODEL_SPARROW, MODEL_RCTIGER }, (eModelID)veh->m_nModelIndex)
                    ? 0.1f
                    : 0.3f;
            }

            FireInstantHit_ApplyRandomSpread(endPt, spread);
            endPt.Normalise();
            CWorld::pIgnoreEntity = firingEntity;
            endPt = FireInstantHit_PointAlongDir(start, endPt, wi->m_fWeaponRange);

            // NOTE: `CWorld::bIncludeBikers` isn't set here
        } else {
            const auto  range = wi->m_fWeaponRange;
            const auto& fwd   = firingEntity->GetMatrix().GetForward();

            const auto offsX = range * fwd.x;
            const auto offsY = range * fwd.y;
            endPt.x = offsX + muzzlePos.x;
            endPt.y = muzzlePos.y + offsY;
            endPt.z = (float)((double)range * fwd.z + muzzlePos.z);
            dir     = firingEntity->GetMatrix().GetForward();

            if (ped) {
                if (ped->bDoomAim && (!ped->IsPlayer() || !wi->flags.bCanAim)) {
                    DoDoomAiming(firingEntity, &start, &endPt);
                }
            }

            if (ped && ped->bInVehicle && ped->m_pVehicle) {
                CWorld::pIgnoreEntity = ped->m_pVehicle;
            } else if (ped && ped->m_pAttachedTo && ped->m_pAttachedTo->GetIsTypeVehicle()) {
                CWorld::pIgnoreEntity = ped->m_pAttachedTo;
            } else {
                CWorld::pIgnoreEntity = firingEntity;
            }
            CWorld::bIncludeBikers = true;
        }
        DoLineOfSight(start);
    };

    if (taskUseGun && taskUseGun->m_SkipAim) { // 0x73FC94 (NOTE: Offset 0xE of the task)
        // Shoot straight ahead from the hand of the ped
        endPt = CVector{ wi->m_fWeaponRange, 0.f, 0.f };

        const auto hier = GetAnimHierarchyFromSkinClump(ped->GetRpClump());
        const auto idx  = RpHAnimIDGetIndex(hier, ped->m_apBones[PED_NODE_RIGHT_HAND]->BoneTag);
        RwV3dTransformPoints(&endPt, &endPt, 1, &RpHAnimHierarchyGetMatrixArray(hier)[idx]);

        dir = CVector{
            endPt.x - start.x,
            endPt.y - start.y,
            endPt.z - start.z
        };
        dir.Normalise();

        if (ped->bInVehicle && ped->m_pVehicle) {
            CWorld::pIgnoreEntity = ped->m_pVehicle;
        } else if (ped->m_pAttachedTo && ped->m_pAttachedTo->GetIsTypeVehicle()) {
            CWorld::pIgnoreEntity = ped->m_pAttachedTo;
        }
        CWorld::bIncludeBikers = true;

        DoLineOfSight(start);
    } else if (!ped) {
        DoDefaultAiming();
    } else if (!targetEntity && !target) { // 0x74034A - Player shooting (without a target)
        const auto camMode = TheCamera.GetActiveCam().m_nMode;
        if (!ped->IsPlayer() || !notsa::contains({ MODE_AIMWEAPON, MODE_AIMWEAPON_FROMCAR, MODE_AIMWEAPON_ATTACHED, MODE_TWOPLAYER_IN_CAR_AND_SHOOTING }, camMode)) {
            DoDefaultAiming();
        } else { // 0x740389
            TheCamera.Find3rdPersonCamTargetVector(wi->m_fWeaponRange * 3.0f, start, camSource, endPt);

            dir = CVector{
                endPt.x - start.x,
                endPt.y - start.y,
                endPt.z - start.z
            };
            dir.Normalise();

            const auto playerData = ped->GetPlayerData();
            if (spread != 0.f) {
                const auto ratio = ((double)PLAYER_AIM_SCALE_DIST / wi->m_fWeaponRange) * 3.0;
                const auto scale = 1.0 < ratio
                    ? 1.0
                    : ratio;
                spread = (float)(scale * spread * playerData->m_fAttackButtonCounter * PLAYER_AIM_SCALE);

                const auto& cam = TheCamera.m_aCams[0]; // NOTE: Not the active cam
                CVector     sinDir, cosDir;
                if (notsa::contains({ MODE_AIMWEAPON, MODE_AIMWEAPON_FROMCAR, MODE_TWOPLAYER_IN_CAR_AND_SHOOTING }, cam.m_nMode)) {
                    if (cam.m_nMode == MODE_TWOPLAYER_IN_CAR_AND_SHOOTING) {
                        CVector unused{};
                        TheCamera.m_aCams[0].Get_TwoPlayer_AimVector(unused); // Result is unused
                    }
                    cosDir = cam.m_vecUp;
                    sinDir = CrossProduct(cam.m_vecFront, cosDir);
                    sinDir.Normalise();
                } else {
                    // BUG: Original uses the point where the camera is looking at, not a direction (relative to the shooter)
                    sinDir = CrossProduct(endPt, CVector{ 0.f, 0.f, 1.f });
                    sinDir.Normalise();
                    cosDir = CrossProduct(sinDir, endPt);
                    cosDir.Normalise();
                }
                FireInstantHit_ApplyAimSway(endPt, sinDir, cosDir, spread);

                playerData->m_fAttackButtonCounter = (float)((double)(int16)wi->m_nDamage * 0.04f + playerData->m_fAttackButtonCounter);
            }

            if (ped->bInVehicle && ped->m_pVehicle && !ped->m_pVehicle->vehicleFlags.bVehicleCanBeTargetted) {
                CWorld::pIgnoreEntity = ped->m_pVehicle;
            } else if (ped->m_pAttachedTo && ped->m_pAttachedTo->GetIsTypeVehicle() && !ped->m_pAttachedTo->AsVehicle()->vehicleFlags.bVehicleCanBeTargetted) {
                CWorld::pIgnoreEntity = ped->m_pAttachedTo;
            } else {
                CWorld::pIgnoreEntity = firingEntity;
            }
            CWorld::bIncludeDeadPeds = true;
            CWorld::bIncludeCarTyres = true;
            CWorld::bIncludeBikers   = true;

            DoLineOfSight(camSource);

            if (hitEntity) {
                // Check if the hit entity is in range of the weapon
                const auto dx = (double)colPoint.m_vecPoint.x - camSource.x;
                const auto dy = (double)colPoint.m_vecPoint.y - camSource.y;
                const auto dist2D = (float)std::sqrt(dx * dx + dy * dy);
                if ((double)TargetWeaponRangeMultiplier(hitEntity, ped) * wi->m_fWeaponRange < dist2D) {
                    hitEntity = nullptr;
                } else {
                    CheckForShootingVehicleOccupant(&hitEntity, &colPoint, m_Type, camSource, endPt);
                }
            }
        }
    } else { // 0x73FDF2 - AI shooting at a target
        if (ped->m_pedIK.bGunReachedTarget || arg6) { // Otherwise there's no line of sight test, and `hitEntity` stays null
            //> 0x73FE0A - Find the point to shoot at
            if (!target) {
                if (targetEntity->GetIsTypePed()) {
                    if (const auto playerData = ped->GetPlayerData()) {
                        endPt = playerData->m_vecTargetBoneOffset;
                        targetEntity->AsPed()->GetTransformedBonePosition(endPt, (eBoneTag)playerData->m_nTargetBone, false);
                    } else {
                        targetEntity->AsPed()->GetBonePosition(&endPt, BONE_SPINE1, false);
                    }
                } else {
                    endPt = targetEntity->GetPosition();
                }
            } else {
                endPt = *target;
            }

            //> 0x73FE81 - Direction to the point (Note: The X and Z components are not rounded to float before being used)
            {
                const auto dx = (double)endPt.x - start.x;
                const auto dy = endPt.y - start.y;
                const auto dz = (double)endPt.z - start.z;
                auto       mag = std::sqrt(dx * dx + (double)dy * dy + dz * dz);
                if (mag < MIN_AIM_DIR_MAG) {
                    mag = MIN_AIM_DIR_MAG;
                }
                const auto invMag = 1.0 / mag;
                endPt.x = (float)(invMag * dx);
                endPt.y = (float)(invMag * dy);
                endPt.z = (float)(invMag * dz);
                dir     = endPt;
            }

            // 0x73FF11 - Shoot as far as the weapon can
            endPt = FireInstantHit_PointAlongDir(
                start,
                endPt,
                (double)TargetWeaponRangeMultiplier(targetEntity, ped) * wi->m_fWeaponRange
            );

            const auto playerData = ped->GetPlayerData();
            if (!playerData || spread == 0.f) { // 0x7401DA
                if (spread > 0.f) {
                    if (targetEntity && targetEntity->GetIsTypePed() && targetEntity->AsPed()->IsPlayer()) {
                        // Be less accurate if the target is moving
                        const auto speed = targetEntity->AsPhysical()->m_vecMoveSpeed.Magnitude();
                        const auto m     = speed > 0.33f
                            ? 0.33f
                            : speed;
                        spread = (float)(((double)m * 0.90909094f + 0.8f) * spread); // 0x872C70, 0x858C98
                    }
                    FireInstantHit_ApplyRandomSpread(endPt, spread);
                }
            } else { // 0x73FF6B
                const auto ratio = (double)PLAYER_AIM_SCALE_DIST / wi->m_fWeaponRange;
                const auto scale = 1.0 < ratio
                    ? 1.0
                    : ratio;
                const auto scaledSpread = scale * spread * playerData->m_fAttackButtonCounter * PLAYER_AIM_SCALE;
                spread = (float)scaledSpread;

                const auto halfSpread = scaledSpread * 0.5;
                const auto minSpread  = 0.2f < halfSpread  // 0x858CC4
                    ? 0.2f
                    : (float)halfSpread;
                spread = FireInstantHit_RandomInRange(minSpread, spread);

                const auto& cam = TheCamera.m_aCams[0]; // NOTE: Not the active cam
                CVector     sinDir, cosDir;
                if (notsa::contains({ MODE_AIMWEAPON, MODE_AIMWEAPON_FROMCAR }, cam.m_nMode)) {
                    cosDir = cam.m_vecUp;
                    sinDir = CrossProduct(cam.m_vecFront, cosDir);
                    sinDir.Normalise();
                } else {
                    // BUG: Original uses the point we're shooting at, not a direction (relative to the shooter)
                    sinDir = CrossProduct(endPt, CVector{ 0.f, 0.f, 1.f });
                    sinDir.Normalise();
                    cosDir = CrossProduct(sinDir, endPt);
                    cosDir.Normalise();
                }
                FireInstantHit_ApplyAimSway(endPt, sinDir, cosDir, spread);

                playerData->m_fAttackButtonCounter = (float)((double)(int16)wi->m_nDamage * 0.04f + playerData->m_fAttackButtonCounter);
            }

            //> 0x7402EE
            if (ped->bInVehicle && ped->m_pVehicle && !ped->m_pVehicle->vehicleFlags.bVehicleCanBeTargetted) {
                CWorld::pIgnoreEntity = ped->m_pVehicle;
            } else if (ped->m_pAttachedTo && ped->m_pAttachedTo->GetIsTypeVehicle() && !ped->m_pAttachedTo->AsVehicle()->vehicleFlags.bVehicleCanBeTargetted) {
                CWorld::pIgnoreEntity = ped->m_pAttachedTo;
            }
            if (ped->IsPlayer()) {
                CWorld::bIncludeDeadPeds = true;
            }
            CWorld::bIncludeBikers = true;

            DoLineOfSight(start);
        }
    }

    //> 0x740B71 - Notify everyone about the shot
    CEventGunShot gunShotEvent{
        firingEntity,
        start,
        endPt,
        notsa::contains({ WEAPON_PISTOL_SILENCED, WEAPON_TEARGAS }, m_Type)
    };
    GetEventGlobalGroup()->Add(static_cast<CEvent*>(&gunShotEvent), false);

    CEventGunShotWhizzedBy gunShotWhizzedByEvent{
        firingEntity,
        start,
        endPt,
        m_Type == WEAPON_PISTOL_SILENCED
    };
    GetEventGlobalGroup()->Add(static_cast<CEvent*>(&gunShotWhizzedByEvent), false);

    g_InterestingEvents.Add(CInterestingEvents::EType::INTERESTING_EVENT_22, firingEntity);

    // NOTE: Original calls `ped->IsPlayer()` here and discards the result
    start = *origin;

    //> 0x740C66 - Muzzle flash
    if (muzzle) {
        float lightOffset{}, shellSize{};
        bool  doEffects = true;
        switch (m_Type) {
        case WEAPON_PISTOL:
        case WEAPON_PISTOL_SILENCED:
        case WEAPON_DESERT_EAGLE:
        case WEAPON_SNIPERRIFLE:
            lightOffset = 0.2f;
            shellSize   = 0.25f;
            break;
        case WEAPON_SHOTGUN:
        case WEAPON_SAWNOFF_SHOTGUN:
        case WEAPON_SPAS12_SHOTGUN:
            lightOffset = 0.3f;
            shellSize   = 0.45f;
            break;
        case WEAPON_MICRO_UZI:
        case WEAPON_MP5:
        case WEAPON_TEC9:
            lightOffset = 0.2f;
            shellSize   = 0.3f;
            break;
        case WEAPON_AK47:
        case WEAPON_M4:
        case WEAPON_MINIGUN: { // 0x740C8C
            // Weapons that fire too fast only get the effects every second shot
            static auto& s_RapidFireCounter = StaticRef<uint8>(0xC8A80C);
            const auto   animLoopLen        = (int32)(((double)wi->m_fAnimLoopEnd - wi->m_fAnimLoopStart) * 900.0);
            if (animLoopLen < 50) {
                s_RapidFireCounter++;
                if (s_RapidFireCounter & 1) {
                    doEffects = false;
                    break;
                }
            }
            lightOffset = 0.65f;
            shellSize   = 0.25f;
            break;
        }
        default:
            doEffects = false;
            break;
        }

        if (doEffects) { // 0x740CFC
            CPointLights::AddLight(PLTYPE_POINTLIGHT, muzzlePos, {}, 3.0f, 0.25f, 0.22f, 0.0f, 0, false, nullptr);

            g_fx.TriggerGunshot(
                firingEntity,
                muzzlePos,
                dir,
                !(ped && ped->m_pGunflashObject)
            );

            CVector shellPos{
                (float)((double)muzzlePos.x - (double)dir.x * lightOffset),
                (float)((double)muzzlePos.y - (double)dir.y * lightOffset),
                (float)((double)muzzlePos.z - (double)dir.z * lightOffset)
            };
            const auto& right = firingEntity->GetMatrix().GetRight();
            AddGunshell(firingEntity, shellPos, CVector2D{ right.x, right.y }, shellSize);
        }
    }

    //> 0x740E3A - Water splash
    {
        bool testWater = false;
        if (targetEntity) {
            testWater = notsa::contains({ ENTITY_TYPE_VEHICLE, ENTITY_TYPE_PED, ENTITY_TYPE_OBJECT }, targetEntity->GetType())
                && targetEntity->AsPhysical()->physicalFlags.bSubmergedInWater;
        } else if (endPt.z < start.z) {
            testWater = (ped && ped->IsPlayer())
                || notsa::contains({ STATUS_PLAYER, STATUS_REMOTE_CONTROLLED }, firingEntity->GetStatus());
        }
        if (testWater) { // 0x740EA3
            CVector waterHitPos{};
            const auto TestWater = [&](CVector to) { // 0x6E61B0 - Not reversed yet
                return plugin::CallAndReturn<bool, 0x6E61B0, CVector, CVector, CVector*>(start, to, &waterHitPos);
            };
            if (TestWater(hitEntity ? colPoint.m_vecPoint : endPt)) { // 0x740F42
                g_fx.TriggerBulletSplash(waterHitPos);
                AudioEngine.ReportBulletHit(nullptr, SURFACE_WATER_SHALLOW, waterHitPos, 0.f);
            }
        }
    }

    //> 0x740F6A - Do the actual bullet impact(s)
    int32 numIterations = 0;
    if (CWorld::fWeaponSpreadRate <= 0.f
        || !hitEntity
        || (hitEntity->GetIsTypeVehicle() && colPoint.m_nPieceTypeB > 12 && colPoint.m_nPieceTypeB < 17) // Wheels
    ) {
        DoBulletImpact(firingEntity, hitEntity, muzzlePos, endPt, colPoint, 0);
    } else { // Shotgun-like weapon
        do {
            numIterations++;

            const auto numPellets = m_Type == WEAPON_SPAS12_SHOTGUN
                ? SPAS_NUM_PELLETS
                : SHOTGUN_NUM_PELLETS;

            CMatrix pelletMat{};
            SetUpPelletCol(numPellets, firingEntity, hitEntity, start, colPoint, pelletMat);

            std::array<float, SHOTGUN_NUM_PELLETS> pelletTouchDist; // 1.0 = No hit
            pelletTouchDist.fill(1.f);

            const auto cm = hitEntity->GetIsTypePed()
                ? CModelInfo::GetModelInfo(hitEntity->m_nModelIndex)->AsPedModelInfoPtr()->AnimatePedColModelSkinned(hitEntity->GetRpClump())
                : hitEntity->GetColModel();
            CCollision::ProcessColModels(
                pelletMat,
                ms_PelletTestCol,
                hitEntity->GetMatrix(),
                *cm,
                CWorld::m_aTempColPts,
                CWorld::m_aTempColPts.data(),
                pelletTouchDist.data(),
                false
            );

            int32 numHits = 0, lastHit = 0;
            for (int32 i = 0; i < numPellets; i++) {
                if (pelletTouchDist[i] < 1.f) {
                    numHits++;
                    lastHit = i;
                }
            }

            for (int32 i = 0; i < numPellets; i++) {
                if (pelletTouchDist[i] < 1.f) {
                    // NOTE: The 2nd point passed in the original is actually the colpoint itself (its first member is the position)
                    DoBulletImpact(
                        firingEntity,
                        hitEntity,
                        muzzlePos,
                        CWorld::m_aTempColPts[i].m_vecPoint,
                        CWorld::m_aTempColPts[i],
                        i == lastHit ? -numHits : 1
                    );
                }
            }

            if (hitEntity->GetIsTypePed() || hitEntity->GetIsTypeVehicle()) {
                if ((pelletTouchDist[0] != 1.f && (double)numHits / numPellets >= 0.5) || numIterations >= 2) {
                    hitEntity = nullptr;
                } else {
                    // The pellets that missed may continue through the entity
                    CWorld::pIgnoreEntity = hitEntity;
                    start                 = colPoint.m_vecPoint;
                    hitEntity             = nullptr;
                    DoLineOfSight(start);
                }
            } else {
                DoBulletImpact(firingEntity, hitEntity, muzzlePos, endPt, colPoint, 0);
                hitEntity = nullptr;
            }
        } while (hitEntity);
    }

    CWorld::ResetLineTestOptions();

    return true;
}

// 0x741360
bool CWeapon::FireProjectile(CEntity* firedBy, const CVector& origin, CEntity* targetEntity, const CVector* targetPos, float force) {
    assert(firedBy);

    const auto firedByPed = firedBy->GetIsTypePed()
        ? firedBy->AsPed()
        : nullptr;
    auto projOrigin     = origin;
    auto losCheckTarget = origin;
    auto losCheckOrigin = origin;
    auto projType = GetProjectileType();
    if (notsa::contains({ WEAPON_RLAUNCHER, WEAPON_RLAUNCHER_HS }, GetType())) {
        if (firedByPed && firedByPed->IsPlayer()) {
            switch (TheCamera.GetActiveCam().m_nMode) {
            case MODE_M16_1STPERSON:
            case MODE_SNIPER:
            case MODE_ROCKETLAUNCHER:
            case MODE_ROCKETLAUNCHER_HS:
            case MODE_M16_1STPERSON_RUNABOUT:
            case MODE_SNIPER_RUNABOUT:
            case MODE_ROCKETLAUNCHER_RUNABOUT:
            case MODE_ROCKETLAUNCHER_RUNABOUT_HS:
                break;
            default:
                return false;
            }
            projOrigin = origin + TheCamera.GetActiveCam().m_vecFront;
        } else {
            projOrigin = origin + firedBy->GetForward();
        }
        if (firedByPed) {
            if (firedByPed->IsPlayer()) { // 0x7416DC
                CEntity* hsMissleTarget{};
                if (GetType() == WEAPON_RLAUNCHER_HS && CWeaponEffects::IsLockedOn(WEAPONEFFECTS_LOCK_ON)) {
                    const auto pd = firedByPed->GetPlayerData();
                    if (pd->m_nFireHSMissilePressedTime) {
                        hsMissleTarget = PickTargetForHeatSeekingMissile(
                            firedBy->GetPosition(),
                            firedBy->GetForward(),
                            1.2f,
                            firedBy,
                            false,
                            pd->m_LastHSMissileTarget
                        );
                        if (hsMissleTarget == pd->m_LastHSMissileTarget && CTimer::GetTimeInMS() - pd->m_nFireHSMissilePressedTime > 1500) { // 0x74178B
                            const auto ch = &gCrossHair[0];
                            ch->m_color                 = { 255, 0, 0, 255 };
                            ch->m_fRotation             = 1.f;
                            ch->m_nTimeWhenToDeactivate = 0;
                        }
                    }
                }
                if (hsMissleTarget) { // 0x7417BB
                    targetEntity = hsMissleTarget;
                } else {
                    targetEntity = nullptr;
                    projType     = WEAPON_ROCKET;
                }
            } else { // 0x7418A3
                if (targetEntity || targetPos) {
                    CWorld::pIgnoreEntity = firedBy;
                    const auto losClear = CWorld::GetIsLineOfSightClear(
                        projOrigin,
                        projOrigin + ((targetEntity ? targetEntity->GetPosition() : *targetPos) - projOrigin).Normalized() * 8.f,
                        true,
                        false,
                        false,
                        false
                    );
                    CWorld::pIgnoreEntity = nullptr;
                    if (!losClear) {
                        return false;
                    }
                }
            }
        }
    } else { // 0x74139B
        if (const auto t = (origin - firedBy->GetPosition()).Dot(firedBy->GetForward()); t < 0.3f) { // 0x7413FC
            projOrigin += (0.3f - t) * firedBy->GetForward();
        }
        losCheckTarget = projOrigin;
        if (projOrigin.z - firedBy->GetPosition().z > 0.f) {
            losCheckTarget += firedBy->GetForward() * 0.6f;
        }
        losCheckOrigin = projOrigin - (projOrigin - firedBy->GetPosition()).ProjectOnToNormal(firedBy->GetForward()); // 0x7415A2
    }

    // 0x7418F5
    CWorld::pIgnoreEntity = firedBy;
    if (CWorld::GetIsLineOfSightClear(
        losCheckOrigin,
        losCheckTarget,
        true,
        true,
        false,
        true
    )) {
        if (projType == WEAPON_ROCKET && targetEntity && targetPos) {
            const auto projTargetPos = targetEntity
                ? targetEntity->GetPosition()
                : *targetPos;
            const auto projDir = (projTargetPos - losCheckOrigin).Normalized();
            CProjectileInfo::AddProjectile( // 0x741AF9
                firedBy,
                WEAPON_ROCKET,
                projOrigin,
                force,
                &projDir,
                targetEntity
            );
        } else {
            CProjectileInfo::AddProjectile(
                firedBy,
                projType,
                projOrigin,
                force,
                nullptr,
                targetEntity
            );
        }

    } else if (notsa::contains({ WEAPON_GRENADE, WEAPON_REMOTE_SATCHEL_CHARGE }, GetType()) && firedBy->GetIsTypePed()) { // 0x74193B
        const auto thorwableProjOrigin = firedBy->GetPosition() - firedBy->GetForward() - CVector{0.f, 0.f, 0.4f};
        if (CWorld::TestSphereAgainstWorld(thorwableProjOrigin, 0.3f, nullptr, false, false, true, false, false, false)) { // 0x7419CE
            CProjectileInfo::AddProjectile(
                firedBy,
                projType,
                thorwableProjOrigin,
                force,
                nullptr,
                targetEntity
            );
        } else {
            CProjectileInfo::RemoveNotAdd(firedBy, projType, projOrigin);
        }
    } else {
        CProjectileInfo::RemoveNotAdd(firedBy, projType, projOrigin);
    }
    CWorld::pIgnoreEntity = nullptr;

    if (firedByPed) { // 0x741A74
        CCrime::ReportCrime(CRIME_EXPLOSION, firedByPed, firedByPed);
        g_InterestingEvents.Add(CInterestingEvents::INTERESTING_EVENT_22, firedBy);
    } else if (firedBy->GetIsTypeVehicle()) { // 0x741B10
        if (const auto drvr = firedBy->AsVehicle()->m_pDriver) {
            CCrime::ReportCrime(CRIME_FIRE_WEAPON, firedBy, drvr);
            g_InterestingEvents.Add(CInterestingEvents::INTERESTING_EVENT_22, drvr);
        }
    }

    GetEventGlobalGroup()->Add(
        CEventGunShot{
            firedBy,
            projOrigin,
            targetEntity
                ? targetEntity->GetPosition()
                : targetPos
                    ? *targetPos
                    : projOrigin,
            notsa::contains({WEAPON_PISTOL_SILENCED, WEAPON_TEARGAS}, GetType())
        }
    );

    return true;
}

// 0x741C00
bool CWeapon::FireM16_1stPerson(CPed* owner) {
    const auto cam = &TheCamera.GetActiveCam();

    switch (cam->m_nMode) {
    case MODE_M16_1STPERSON:
    case MODE_SNIPER:
    case MODE_CAMERA:
    case MODE_ROCKETLAUNCHER:
    case MODE_ROCKETLAUNCHER_HS:
    case MODE_M16_1STPERSON_RUNABOUT:
    case MODE_SNIPER_RUNABOUT:
    case MODE_ROCKETLAUNCHER_RUNABOUT:
    case MODE_ROCKETLAUNCHER_RUNABOUT_HS:
    case MODE_HELICANNON_1STPERSON:
        break;
    default:
        return false;
    }

    const auto wi = &GetWeaponInfo(); // NOTE: Why not `GetWeaponInfo(owner)`

    CWorld::bIncludeDeadPeds = true;
    CWorld::bIncludeCarTyres = true;
    CWorld::bIncludeBikers   = true;

    const auto camOriginPos = cam->m_vecSource;
    const auto camTargetPos = camOriginPos + cam->m_vecFront * 3.f;

    CBirds::HandleGunShot(&camOriginPos, &camTargetPos);
    CShadows::GunShotSetsOilOnFire(camOriginPos, camTargetPos);

    CColPoint shotCP;
    CEntity*  shotHitEntity;
    if (CWorld::ProcessLineOfSight(camOriginPos, camTargetPos, shotCP, shotHitEntity, true, true, true, true, true, false, false, true)) {
        CheckForShootingVehicleOccupant(&shotHitEntity, &shotCP, m_Type, camOriginPos, camTargetPos);
    }

    CWorld::bIncludeDeadPeds = false;
    CWorld::bIncludeCarTyres = false;
    CWorld::bIncludeBikers   = false;
    CWorld::pIgnoreEntity    = nullptr;

    //> 0x741DC4 - Check if hit entity is within range
    if (shotHitEntity) {
        if (TargetWeaponRangeMultiplier(shotHitEntity, owner) * wi->m_fWeaponRange >= (camOriginPos - shotCP.m_vecPoint).SquaredMagnitude2D()) {
            shotHitEntity = nullptr;
        }
    }

    DoBulletImpact(owner, shotHitEntity, camOriginPos, camTargetPos, shotCP, false);

    //> 0x741E48 - Visual/physical feedback for the player(s)
    if (owner->IsPlayer()) {
        auto intensity = [&]{
            switch (m_Type) {
            case WEAPON_AK47:
                return 0.00015f;
            case WEAPON_M4:
                return 0.0003f;
            default:
                return 0.0002f;
            }
        }();
        if (FindPlayerPed()->bIsDucking || FindPlayerPed()->m_pAttachedTo) {
            intensity *= 0.3f;
        }

        // Move the camera around a little
        cam->m_fHorizontalAngle += (float)CGeneral::GetRandomNumberInRange(-64, 64) * intensity;
        cam->m_fVerticalAngle += (float)CGeneral::GetRandomNumberInRange(-64, 64) * intensity;

        // Do pad shaking
        const auto shakeFreq = (uint8)lerp(130.f, 210.f, std::clamp((20.f - (wi->m_fAnimLoopEnd - wi->m_fAnimLoopStart) * 900.f) / 80.f, 0.f, 1.f));
        CPad::GetPad(owner->GetPadNumber())->StartShake(
            (int16)(CTimer::GetTimeStep() * 20'000.f / (float)shakeFreq),
            shakeFreq,
            0
        );
    }

    return true;
}

// 0x742300
bool CWeapon::Fire(CEntity* firedBy, CVector* startPosn, CVector* barrelPosn, CEntity* targetEnt, CVector* targetPosn, CVector* altPosn) {
    const auto firedByPed = firedBy && firedBy->GetIsTypePed()
        ? firedBy->AsPed()
        : nullptr;
    const auto wi = &GetWeaponInfo(firedByPed);

    CVector point{ 0.f, 0.f, 0.6f };

    const auto fxPos = startPosn
        ? startPosn
        : &point;
    const auto shotOrigin = startPosn
        ? barrelPosn
        : &point;
    if (!startPosn) {
        point     = firedBy->GetMatrix().TransformPoint(point);
        startPosn = &point;
    }

    if (m_IsFirstPersonWeaponModeSelected) {
        const auto r = 0.15f;

        const auto h = firedBy->GetHeading();
        fxPos->x -= std::sin(h) * r;
        fxPos->y += std::cos(h) * r;
    }

    switch (m_State) {
    case WEAPONSTATE_READY:
    case WEAPONSTATE_FIRING:
        break;
    default:
        return false;
    }

    if (!m_AmmoInClip) {
        if (!m_TotalAmmo) {
            return false;
        }
        m_AmmoInClip = std::min<uint32>(m_TotalAmmo, wi->m_nAmmoClip);
    }

    const auto [hasFired, delayNextShot] = [&]() -> std::pair<bool, bool> {
        switch (m_Type) {
        case WEAPON_GRENADE:
        case WEAPON_TEARGAS:
        case WEAPON_MOLOTOV:
        case WEAPON_REMOTE_SATCHEL_CHARGE: { // 0x74268B
            if (targetPosn) {
                return {
                    FireProjectile( // 0x742705
                        firedBy,
                        *shotOrigin,
                        targetEnt,
                        targetPosn,
                        std::clamp(((firedBy->GetPosition() - *targetPosn).Magnitude() - 10.f) / 10.f, 0.2f, 1.f)
                    ),
                    true
                };
            } else if (firedBy == FindPlayerPed()) { // 0x74271F
                return {
                    FireProjectile(
                        firedBy,
                        *shotOrigin,
                        targetEnt,
                        nullptr,
                        firedBy->AsPed()->GetPlayerData()->m_fAttackButtonCounter * 0.0375f
                    ),
                    true
                };
            }
            return {
                FireProjectile( // 0x74274E
                    firedBy,
                    *shotOrigin,
                    targetEnt,
                    nullptr,
                    0.3f
                ),
                true
            };
        }
        case WEAPON_PISTOL:
        case WEAPON_PISTOL_SILENCED:
        case WEAPON_DESERT_EAGLE:
        case WEAPON_MICRO_UZI:
        case WEAPON_MP5:
        case WEAPON_AK47:
        case WEAPON_M4:
        case WEAPON_TEC9:
        case WEAPON_COUNTRYRIFLE:
        case WEAPON_MINIGUN: { // 0x7424FE
            if (   firedByPed
                && firedByPed->m_nPedType == PED_TYPE_PLAYER1
                && notsa::contains({ MODE_M16_1STPERSON, MODE_HELICANNON_1STPERSON }, (eCamMode)TheCamera.m_PlayerWeaponMode.m_nMode)
            ) {
                return { FireM16_1stPerson(firedByPed), true };
            }
            const auto fired = FireInstantHit(firedBy, startPosn, shotOrigin, targetEnt, targetPosn, altPosn, false, true);
            if (firedByPed) { // 0x74255B
                if (!firedByPed->bInVehicle) {
                    return { fired, false };
                }
                if (const auto t = firedByPed->GetTaskManager().GetActiveTask()) {
                    return { fired, t->GetTaskType() == TASK_SIMPLE_GANG_DRIVEBY };
                }
            }
            return { fired, true };
        }
        case WEAPON_SHOTGUN:
        case WEAPON_SAWNOFF_SHOTGUN:
        case WEAPON_SPAS12_SHOTGUN:
            return {
                FireInstantHit( // 0x742495
                    firedBy,
                    startPosn,
                    shotOrigin,
                    targetEnt,
                    targetPosn,
                    altPosn,
                    false,
                    true
                ),
                true
            };
        case WEAPON_SNIPERRIFLE: { // 0x7424AC
            if (firedByPed && firedByPed->m_nPedType == PED_TYPE_PLAYER1 && TheCamera.m_PlayerWeaponMode.m_nMode == MODE_SNIPER) {
                return {
                    FireSniper(firedByPed, targetEnt, targetPosn),
                    true
                }; 
            }
            return {
                FireInstantHit(
                    firedBy,
                    startPosn,
                    shotOrigin,
                    targetEnt,
                    targetPosn,
                    nullptr,
                    false,
                    true
                ),
                true
            };
        }
        case WEAPON_RLAUNCHER:
        case WEAPON_RLAUNCHER_HS: { // 0x7425B3
            if (firedByPed) {
                const auto CanFire = [&](CVector origin, CVector end) {
                    return (origin - end).SquaredMagnitude() <= sq(8.f) && !firedBy->GetIsTypePed();
                };
                if (   targetEnt  && !CanFire(firedBy->GetPosition(), targetEnt->GetPosition())
                    || targetPosn && !CanFire(firedBy->GetPosition(), *targetPosn)
                ) {
                    return { false, true };
                }
            }
            return {
                FireProjectile(
                    firedBy,
                    *shotOrigin,
                    targetEnt,
                    targetPosn
                ),
                true
            };
        }
        case WEAPON_FLAMETHROWER:
        case WEAPON_SPRAYCAN:
        case WEAPON_EXTINGUISHER:
            return {
                FireAreaEffect(
                    firedBy,
                    *shotOrigin,
                    targetEnt,
                    targetPosn
                ),
                true
            };
        case WEAPON_DETONATOR: {
            assert(firedByPed);
            CWorld::UseDetonator(firedByPed);
            m_AmmoInClip = m_TotalAmmo  = 1;
            return { true, true };
        }
        case WEAPON_CAMERA:
            return {
                TakePhotograph(firedBy, shotOrigin),
                true
            };
        default:
            NOTSA_UNREACHABLE();
        }
    }();

    // 0x74279A
    if (hasFired) {
        // 0x7427B3
        const bool isPlayerFiring = firedByPed && m_Type != WEAPON_CAMERA && firedByPed->IsPlayer();
        if (firedByPed) {
            if (m_Type != WEAPON_CAMERA) {
                firedByPed->bFiringWeapon = true;
            }
            firedByPed->GetWeaponAE().AddAudioEvent(AE_WEAPON_FIRE);
            if (isPlayerFiring && targetEnt && targetEnt->GetIsTypePed() && m_Type != WEAPON_PISTOL_SILENCED) {
                firedByPed->Say(CTX_GLOBAL_SHOOT, 200); // 0x74280E
            }
        }

        // 0x74282C
        if (m_Type == WEAPON_REMOTE_SATCHEL_CHARGE) {
            firedByPed->GiveWeapon(WEAPON_DETONATOR, true, true);
            if (firedByPed->GetWeapon(WEAPON_REMOTE_SATCHEL_CHARGE).m_TotalAmmo <= 1) {
                firedByPed->GetWeapon(WEAPON_DETONATOR).m_State = eWeaponState::WEAPONSTATE_READY;
                firedByPed->SetCurrentWeapon(WEAPON_DETONATOR);
            }
        }

        //> 0x74286D - Increase stats
        if (isPlayerFiring) {
            switch (m_Type)
            {
            case WEAPON_GRENADE:
            case WEAPON_MOLOTOV:
            case WEAPON_ROCKET:
            case WEAPON_RLAUNCHER:
            case WEAPON_RLAUNCHER_HS:
            case WEAPON_REMOTE_SATCHEL_CHARGE:
            case WEAPON_DETONATOR:
                CStats::IncrementStat(STAT_KGS_OF_EXPLOSIVES_USED);
                break;
            case WEAPON_PISTOL:
            case WEAPON_PISTOL_SILENCED:
            case WEAPON_DESERT_EAGLE:
            case WEAPON_SHOTGUN:
            case WEAPON_SAWNOFF_SHOTGUN:
            case WEAPON_SPAS12_SHOTGUN:
            case WEAPON_MICRO_UZI:
            case WEAPON_MP5:
            case WEAPON_AK47:
            case WEAPON_M4:
            case WEAPON_TEC9:
            case WEAPON_COUNTRYRIFLE:
            case WEAPON_SNIPERRIFLE:
            case WEAPON_MINIGUN:
                CStats::IncrementStat(STAT_BULLETS_FIRED);
                break;
            default:
                break;
            }
        }

        // 0x7428A6
        if (!CCheat::IsActive(CHEAT_INFINITE_AMMO)) {
            if (m_AmmoInClip) {
                m_AmmoInClip--;
            }
            if (m_TotalAmmo > 0) {
                if (isPlayerFiring
                        ? m_Type == WEAPON_DETONATOR || CStats::GetPercentageProgress() < 100.f
                        : m_TotalAmmo < 25'000
                ) {
                    m_TotalAmmo--;    
                }
            }
        }

        m_State = WEAPONSTATE_FIRING;

        if (!m_AmmoInClip) { // 0x7428FB
            if (m_TotalAmmo) {
                m_State = WEAPONSTATE_RELOADING;
                m_TimeForNextShotMs = s_DebugSettings.NoShotDelay
                    ? 0
                    : firedBy == FindPlayerPed() && FindPlayerInfo().m_bFastReload
                        ? wi->GetWeaponReloadTime() / 4
                        : wi->GetWeaponReloadTime();
                m_TimeForNextShotMs += CTimer::GetTimeInMS();
            } else if (TheCamera.GetActiveCam().m_nMode == MODE_CAMERA) {
                CPad::GetPad()->Clear(false, true);
            }
            return true;
        }

        m_TimeForNextShotMs = s_DebugSettings.NoShotDelay
            ? 0
            : delayNextShot
                ? m_Type == WEAPON_CAMERA
                    ? 1100
                    : (uint32)((wi->m_fAnimLoopEnd - wi->m_fAnimLoopStart) * 900.f)
                : 0;
        m_TimeForNextShotMs += CTimer::GetTimeInMS();
    }
    // 0x7429F2
    if (m_Type == WEAPON_UNARMED || m_Type == WEAPON_BASEBALLBAT) {
        return true;
    }
    return hasFired;
}

CWeaponInfo& CWeapon::GetWeaponInfo(CPed* owner) const {
    return GetWeaponInfo(owner ? owner->GetWeaponSkill(GetType()) : eWeaponSkill::STD);
}

CWeaponInfo& CWeapon::GetWeaponInfo(eWeaponSkill skill) const {
    return *CWeaponInfo::GetWeaponInfo(GetType(), skill);
}

// 0x73AF00
void FireOneInstantHitRound(const CVector& startPoint, const CVector& endPoint, int32 intensity) {
    CPointLights::AddLight(
        PLTYPE_POINTLIGHT,
        startPoint,
        CVector{0.f, 0.f, 0.f},
        3.f,
        0.25f,
        0.22f,
        0.0f
    );

    CColPoint hitCP;
    CEntity* hitEntity;
    CWorld::ProcessLineOfSight(
        startPoint,
        endPoint,
        hitCP,
        hitEntity,
        true,
        true,
        true,
        true,
        true,
        true,
        false,
        false
    );

    CBulletTraces::AddTrace(
        startPoint,
        hitEntity ? hitCP.m_vecPoint : endPoint,
        0.02f,
        750,
        150
    );

    if (hitEntity) {
        switch (hitEntity->GetType()) {
        case ENTITY_TYPE_PED: {
            const auto hitPed = hitEntity->AsPed();

            if (!notsa::contains({ PEDSTATE_DIE, PEDSTATE_DEAD }, hitPed->GetPedState())) {
                const auto pedHitDir = hitPed->GetLocalDirection(startPoint - hitPed->GetPosition2D());
                CAnimManager::AddAnimation(
                    hitPed->GetRpClump(),
                    ANIM_GROUP_DEFAULT,
                    std::to_array({ANIM_ID_SHOT_PARTIAL, ANIM_ID_SHOT_LEFTP, ANIM_ID_SHOT_PARTIAL_B, ANIM_ID_SHOT_RIGHTP})[pedHitDir]
                );
                CWeapon::GenerateDamageEvent(
                    hitPed,
                    nullptr,
                    WEAPON_UZI_DRIVEBY,
                    intensity,
                    (ePedPieceTypes)hitCP.m_nPieceTypeB,
                    pedHitDir
                );
            }
            break;
        }
        case ENTITY_TYPE_VEHICLE: {
            const auto hitVeh = hitEntity->AsVehicle();

            hitVeh->InflictDamage(
                nullptr,
                WEAPON_MICRO_UZI,
                (float)intensity,
                CVector{0.f, 0.f, 0.f}
            );
            break;
        }
        }

        const auto angleOfIncidenceCos = (endPoint - startPoint).Normalized().Dot(hitCP.m_vecNormal); // 0x73B0CC
        if (angleOfIncidenceCos < 0.f) {
            AudioEngine.ReportBulletHit(
                hitEntity,
                hitCP.m_nSurfaceTypeB,
                hitCP.m_vecPoint,
                RWRAD2DEG(std::asin(-angleOfIncidenceCos)) // Really should've used `acos + PI / 2` here to make this cleaner
            );
        }
    } else { // no hit entity
        float waterZ;
        if (CWaterLevel::GetWaterLevel(endPoint.x, endPoint.y, endPoint.z + 10.f, waterZ, true, nullptr)) {
            AudioEngine.ReportBulletHit(
                nullptr,
                SURFACE_WATER_SHALLOW,
                {endPoint.x, endPoint.y, waterZ},
                0.f
            );
        }
    }
}

float CWeapon::GetWeaponRange(CPed* owner, CEntity* target) const noexcept {
    const auto r = GetWeaponInfo(owner).m_fTargetRange;
    if (target) {
        return r * TargetWeaponRangeMultiplier(target, owner);
    }
    return r;
}
