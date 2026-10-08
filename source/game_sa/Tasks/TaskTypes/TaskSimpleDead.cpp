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
        const float dotFwd   = (float)((double)n.z * (double)fwd.z   + (double)n.y * (double)fwd.y   + (double)n.x * (double)fwd.x);
        const float dotRight = (float)((double)n.z * (double)right.z + (double)n.y * (double)right.y + (double)n.x * (double)right.x);

        const auto ClampAcos = [](float v) {
            if (-1.f > v) {
                v = -1.f;
            } else if (!(1.f > v)) { // NOTE: NaN is passed through the 1.0 check the same way as the original
                v = 1.f;
            }
            return (float)std::acos((double)v);
        };
        ped->m_pedIK.m_fSlopeRoll  = ClampAcos(dotRight);
        ped->m_pedIK.m_fSlopePitch = ClampAcos(dotFwd);
    }

    ped->DeadPedMakesTyresBloody();

    if (CLocalisation::Blood() && !m_bHasDrowned) {
        const uint32 timeSinceDeath = CTimer::GetTimeInMS() - m_nDeathTimeMS;

        // Radius in which the peds will step into the blood
        double radius; // x87: stays in extended precision
        if (timeSinceDeath < 2000) {
            radius = 0.0;
        } else if (timeSinceDeath > 7000) {
            radius = (double)0.75f;
        } else {
            radius = (double)(int32)(timeSinceDeath - 2000) * (double)0.00015f;
        }

        const auto& pedPos = ped->GetPosition();
        for (auto* const entity : ped->GetIntelligence()->GetPedScanner().m_apEntities) {
            if (!entity) {
                continue;
            }
            const auto& pos = entity->GetPosition();
            const double dx = (double)pos.x - (double)pedPos.x;
            const double dy = (double)pos.y - (double)pedPos.y;
            const double dz = (double)pos.z - (double)pedPos.z;
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
                    if (!(ped->GetPosition().z > waterLevel)) { // Also true if equal
                        m_bBloodPuddleCreated = true;
                    }
                }
            }

            if (!m_bBloodPuddleCreated && CLocalisation::Blood()) {
                if (puddleTime < 5000) {
                    const double f = (double)(int32)puddleTime;
                    CShadows::StoreStaticShadow(
                        (uint32)(uintptr_t)this + 0x11,
                        SHADOW_DEFAULT,
                        gpBloodPoolTex,
                        pos,
                        (float)(f * (double)0.00015f),
                        0.f,
                        0.f,
                        (float)((double)-0.00015f * f),
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
