#include "StdInc.h"

#include "TaskComplexReactToAttack.h"
#include "TaskComplexFallAndGetUp.h"
#include "TaskSimpleBeHit.h"
#include "TaskSimpleNone.h"
#include "Pad.h"

namespace {
//! 0x44E480 - `CVector2D::Normalise` as the exe has it: the squared length and `1 / sqrt` stay in extended precision,
//! a NaN length is normalised (JP), a non-positive one yields (1, y)
void NormaliseOriginal(CVector2D& v) {
    const double sq = (double)v.x * (double)v.x + (double)v.y * (double)v.y;
    if (!(sq <= 0.0)) {
        const double recip = 1.0 / std::sqrt(sq);
        v.x = (float)((double)v.x * recip);
        v.y = (float)((double)v.y * recip);
    } else {
        v.x = 1.0f;
    }
}
}; // namespace

void CTaskComplexReactToAttack::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexReactToAttack, 0x86D868, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x620B00);
    RH_ScopedInstall(CreateSubTask, 0x620C30);
    RH_ScopedVMTDestructorInstall(0x625C00);
    RH_ScopedVMTInstall(Clone, 0x623300);
    RH_ScopedVMTInstall(GetTaskType, 0x620B80);
    RH_ScopedVMTInstall(MakeAbortable, 0x620BF0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x625C20);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x625C70);
    RH_ScopedVMTInstall(ControlSubTask, 0x620E00);
}

// 0x620B00
CTaskComplexReactToAttack::CTaskComplexReactToAttack(eWeaponType weaponType, CEntity* attacker, float damage, int32 hitDir, ePedPieceTypes pieceType) :
    m_WeaponType{ weaponType },
    m_Attacker{ attacker },
    m_Damage{ damage },
    m_HitDir{ hitDir },
    m_PieceType{ pieceType }
{
}

// 0x620BF0
bool CTaskComplexReactToAttack::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (m_bAborting) {
        return true;
    }
    switch (priority) {
    case ABORT_PRIORITY_URGENT:
    case ABORT_PRIORITY_IMMEDIATE:
        return m_bAborting = m_pSubTask->MakeAbortable(ped, priority, event);
    default:
        return false;
    }
}

// 0x625C20
CTask* CTaskComplexReactToAttack::CreateNextSubTask(CPed* ped) {
    if (m_bAborting) {
        return nullptr;
    }
    switch (m_pSubTask->GetTaskType()) {
    case TASK_NONE:
    case TASK_COMPLEX_FALL_AND_GET_UP:
    case TASK_SIMPLE_BE_HIT:
        return CreateSubTask(TASK_FINISHED); // => always null
    default:
        return nullptr;
    }
}

// 0x625C70
CTask* CTaskComplexReactToAttack::CreateFirstSubTask(CPed* ped) {
    m_bRestart = false;

    // Players that are doing something (with a ranged weapon) just don't react
    if (ped->IsPlayer() && (int32)m_WeaponType > 9) { // 0x5DF8F0 (Signed compare, `> WEAPON_CHAINSAW`)
        const auto pad = CPad::GetPad(0); // 0x53FB70
        if (!pad->DisablePlayerControls) {
            const auto& ctrl = pad->NewState;
            if (pad->GetTarget() // 0x540670
                || ctrl.LeftStickX || ctrl.LeftStickY
                || ctrl.DPadUp || ctrl.DPadDown || ctrl.DPadLeft || ctrl.DPadRight
            ) {
                return CreateSubTask(TASK_NONE);
            }
        }
    }

    // Shotguns (and the desert eagle, but not for players) knock the ped back
    if (m_PieceType == PED_PIECE_TORSO) {
        if (ped->bCanBeShotInVehicle) { // 0x470, 0x4000000 (Resolved against the bitfield order in `Ped.h`)
            if (m_WeaponType == WEAPON_SPAS12_SHOTGUN || m_WeaponType == WEAPON_SHOTGUN || (m_WeaponType == WEAPON_DESERT_EAGLE && !ped->IsPlayer())) {
                const auto& attackerPos = m_Attacker->GetPosition();
                const auto& pedPos      = ped->GetPosition();

                CVector2D dir{
                    (float)((double)attackerPos.x - (double)pedPos.x),
                    (float)((double)attackerPos.y - (double)pedPos.y)
                };
                NormaliseOriginal(dir); // 0x44E480

                ped->bIsStanding = false;
                ped->ApplyMoveForce(CVector{
                    (float)((double)dir.x * -5.0), // 0x858FC8
                    (float)((double)dir.y * -5.0),
                    5.f
                }); // 0x5429F0
                return CreateSubTask(TASK_COMPLEX_FALL_AND_GET_UP);
            }
        }
        if ((int32)m_WeaponType < 9) { // 0x625DEF - (`jge` => skip if `>= 9`)
            if (ped->m_fHealth < 30.f) { // 0x858CA4 (FCOMP + JNP => taken on `<` only)
                return CreateSubTask(TASK_COMPLEX_FALL_AND_GET_UP);
            }
        }
    }
    return CreateSubTask(TASK_SIMPLE_BE_HIT);
}

// 0x620E00
CTask* CTaskComplexReactToAttack::ControlSubTask(CPed* ped) {
    if (m_bRestart) {
        return CreateFirstSubTask(ped); // Virtual call (slot 9)
    }
    // NOTE: The original calls `m_pSubTask->GetTaskType()` here, and discards the result
    return m_pSubTask;
}

// 0x620C30
CTask* CTaskComplexReactToAttack::CreateSubTask(eTaskType taskType) {
    switch (taskType) {
    case TASK_NONE:
        return new CTaskSimpleNone{};
    case TASK_COMPLEX_FALL_AND_GET_UP:
        if (m_HitDir == 2) {
            return new CTaskComplexFallAndGetUp{ ANIM_ID_KO_SKID_BACK, ANIM_GROUP_DEFAULT, 1000 };
        } else {
            return new CTaskComplexFallAndGetUp{ ANIM_ID_KO_SHOT_STOM, ANIM_GROUP_DEFAULT, 1000 };
        }
    case TASK_SIMPLE_BE_HIT: {
        // 0x821B40 (_ftol => truncation)
        const auto attackerPed = m_Attacker && m_Attacker->GetIsTypePed() ? m_Attacker->AsPed() : nullptr;
        return new CTaskSimpleBeHit{ attackerPed, m_PieceType, m_HitDir, (int32)m_Damage };
    }
    default: // Including `TASK_FINISHED`
        return nullptr;
    }
}
