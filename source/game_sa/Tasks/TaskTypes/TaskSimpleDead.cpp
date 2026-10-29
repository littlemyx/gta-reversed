#include "StdInc.h"

#include "TaskSimpleDead.h"
#include "CarEnterExit.h"
#include "AnimManager.h"
#include "AccidentManager.h"
#include "EventDeadPed.h"
#include "EventGroup.h"
#include "RwHelper.h"
#include "WeaponInfo.h"
#include "Shadows.h"
#include "Localisation.h"
#include "WaterLevel.h"

void CTaskSimpleDead::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleDead, 0x86DEA4, 9);
    RH_ScopedCategory("Tasks/TaskTypes");
    RH_ScopedVMTInstall(ProcessPed, 0x630600);
}

// NOTSA: *deathTime* originally int32
// 0x630590
CTaskSimpleDead::CTaskSimpleDead(uint32 deathTime, bool hasDrowned) :
    m_nDeathTimeMS{deathTime},
    m_bHasDrowned{hasDrowned}
{
}

// 0x636100
CTaskSimpleDead::CTaskSimpleDead(const CTaskSimpleDead& o) :
    CTaskSimpleDead{o.m_nDeathTimeMS, o.m_bHasDrowned}
{
}

// 0x630600
bool CTaskSimpleDead::ProcessPed(CPed* ped) {
    bool wasStanding = false;
    bool isTargeted  = false;

    if (m_bFirstTime) {
        if (ped->bInVehicle) {
            const auto door = CCarEnterExit::ComputeTargetDoorToExit(ped->m_pVehicle, ped);
            CAnimManager::BlendAnimation(
                ped->GetRpClump(),
                ANIM_GROUP_DEFAULT,
                (door == 10 || door == 11) ? (AnimationId)0xBB : (AnimationId)0xBC, // 0xBB/0xBC = Dead-in-car anims (left/right)
                4.f
            );
        }
        ped->SetPedState(PEDSTATE_DEAD);
        m_bFirstTime = false;

        if (ped->bIsStanding || ped->bWasStanding) {
            wasStanding = true;
        }

        if (FindPlayerPed(0)->m_pTargetedObject == ped || (FindPlayerPed(1) && FindPlayerPed(1)->m_pTargetedObject == ped)) {
            isTargeted = true;
        }

        if (!m_bHasDrowned && !ped->m_standingOnEntity && !isTargeted) {
            ped->SetUsesCollision(false);
        }

        ped->m_fHealth = 0.f;
        ped->RemoveWeaponModel(CWeaponInfo::GetWeaponInfo(ped->GetActiveWeapon().m_Type, eWeaponSkill::STD)->m_nModelId1);
        ped->m_nActiveWeaponSlot = 0;

        if (!ped->IsPlayer()) {
            ped->RemoveWeaponAnims(0, -1000.f);
            ped->CreateDeadPedWeaponPickups();
            ped->CreateDeadPedMoney();
        }

        {
            CEventDeadPed event{ ped, m_bHasDrowned, m_nDeathTimeMS };
            GetEventGlobalGroup()->Add(&event, false);
            CAccidentManager::GetInstance()->ReportAccident(ped);
        }
    }

    bool doTilt;
    if (!m_bFirstTime && m_bHasDrowned && ped->bIsStanding) {
        ped->SetUsesCollision(false);
        m_bHasDrowned = false;
        const auto anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_DEFAULT, (AnimationId)0x27, 8.f);
        anim->m_Flags &= ~ANIMATION_IS_FINISH_AUTO_REMOVE;
        doTilt = true;
    } else if (
           ped->GetUsesCollision()
        && !m_bHasDrowned
        && ped->bIsStanding
        && !ped->m_standingOnEntity
        && FindPlayerPed(0)->m_pTargetedObject != ped
        && !(FindPlayerPed(1) && FindPlayerPed(1)->m_pTargetedObject == ped)
        && !isTargeted
    ) {
        ped->SetUsesCollision(false);
        doTilt = true;
    } else {
        doTilt = wasStanding;
    }

    if (doTilt) {
        // Tilt the ped according to the surface it died on
        const auto& mat   = ped->GetMatrix();
        const auto& fwd   = mat.GetForward();
        const auto& right = mat.GetRight();
        const auto& n     = ped->field_578;

        // x87: chains are kept in extended precision, and rounded to float on store
        const float dotFwd   = (float)((x87d)n.z * (x87d)fwd.z   + (x87d)n.y * (x87d)fwd.y   + (x87d)n.x * (x87d)fwd.x);
        const float dotRight = (float)((x87d)n.z * (x87d)right.z + (x87d)n.y * (x87d)right.y + (x87d)n.x * (x87d)right.x);

        // 0x6308DA / 0x630927 call 0x821E70 = the CRT's _CIasin (NOT _CIacos 0x822380: a flat ground gives dot = 0 -> 0 rad, the body lies flat;
        // acos would give pi/2 and PitchForSlope would stand every corpse on its head once the die anim ends)
        const auto ClampAsin = [](float v) {
            if (-1.f > v) {
                v = -1.f;
            } else if (v > 1.f) { // NOTE: NaN is passed through both checks unchanged, same as the original
                v = 1.f;
            }
            return (float)x87::asin((double)v);
        };
        ped->m_pedIK.m_fSlopeRoll  = ClampAsin(dotRight);
        ped->m_pedIK.m_fSlopePitch = ClampAsin(dotFwd);
    }

    ped->DeadPedMakesTyresBloody();

    if (CLocalisation::Blood() && !m_bHasDrowned) {
        const uint32 timeSinceDeath = CTimer::GetTimeInMS() - m_nDeathTimeMS;

        // Radius in which the peds will step into the blood
        x87d radius; // x87: stays in extended precision
        if (timeSinceDeath < 2000) {
            radius = (x87d)0.0;
        } else if (timeSinceDeath > 7000) {
            radius = (x87d)0.75000006f; // exe data 0x85B900 = 0x3F400001, not 0.75f
        } else {
            radius = (x87d)(int32)(timeSinceDeath - 2000) * (x87d)0.00015f;
        }

        const auto& pedPos = ped->GetPosition();
        for (auto* const entity : ped->GetIntelligence()->GetPedScanner().m_apEntities) {
            if (!entity) {
                continue;
            }
            const auto& pos = entity->GetPosition();
            const x87d dx = (x87d)pos.x - (x87d)pedPos.x;
            const x87d dy = (x87d)pos.y - (x87d)pedPos.y;
            const x87d dz = (x87d)pos.z - (x87d)pedPos.z;
            if (dz * dz + dy * dy + dx * dx < radius * radius) {
                const auto other = static_cast<CPed*>(entity);
                other->m_nDeathTimeMS = 200; // Used as the number of remaining bloody footprints
                other->bDoBloodyFootprints = true;
            }
        }

        if (timeSinceDeath > 2000 && !m_bBloodPuddleCreated) {
            CVector pos = ped->GetPosition();
            const uint32 puddleTime = timeSinceDeath - 2000;

            // Don't spawn the puddle in water
            if (puddleTime <= CTimer::GetTimeInMS() - CTimer::GetPreviousTimeInMS()) {
                float waterLevel;
                if (CWaterLevel::GetWaterLevelNoWaves(ped->GetPosition(), &waterLevel, nullptr, nullptr)) {
                    if (ped->GetPosition().z <= waterLevel) { // exe: fcomp + `test ah,0x41; jp`: skipped for z > level and for NaN

                        m_bBloodPuddleCreated = true;
                    }
                }
            }

            if (!m_bBloodPuddleCreated && CLocalisation::Blood()) {
                if (puddleTime < 5000) {
                    const x87d f = (x87d)(int32)puddleTime;
                    CShadows::StoreStaticShadow(
                        (uint32)(uintptr_t)this + 0x11,
                        SHADOW_DEFAULT,
                        gpBloodPoolTex,
                        pos,
                        (float)(f * (x87d)0.00015f),
                        0.f,
                        0.f,
                        (float)((x87d)-0.00015f * f),
                        255,
                        200,
                        0,
                        0,
                        4.f,
                        1.f,
                        40.f,
                        false,
                        0.f
                    );
                } else {
                    CShadows::AddPermanentShadow(
                        SHADOW_DEFAULT,
                        gpBloodPoolTex,
                        &pos,
                        0.75000006f,
                        0.f,
                        0.f,
                        -0.75000006f,
                        255,
                        200,
                        0,
                        0,
                        4.f,
                        40000,
                        1.f
                    );
                    m_bBloodPuddleCreated = true;
                }
            }
        }
    }

    if (m_bHasDrowned) {
        ped->bIsStanding  = false;
        ped->bWasStanding = false;
    } else {
        ped->m_pedIK.bSlopePitch = true;
        ped->m_vecMoveSpeed = CVector{};
    }
    return false;
}
