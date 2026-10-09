#pragma once

#include "TaskComplex.h"
#include "Ped.h"
#include "eWeaponType.h"
#include <extensions/EntityRef.hpp>

//! Ped reacts to being hit by someone (Falls over, or is just "hit")
class NOTSA_EXPORT_VTABLE CTaskComplexReactToAttack : public CTaskComplex {
public:
    static constexpr auto Type = TASK_COMPLEX_REACT_TO_ATTACK;

    static void InjectHooks();

    /*!
    * @param weaponType The weapon the ped was hit with
    * @param attacker   The attacker (if any)
    * @param damage     The damage dealt (truncated to an int when passed on to `CTaskSimpleBeHit`)
    * @param hitDir     The direction the ped was hit from (see `CTaskSimpleBeHit`)
    * @param pieceType  The piece of the ped that was hit
    */
    CTaskComplexReactToAttack(eWeaponType weaponType, CEntity* attacker, float damage, int32 hitDir, ePedPieceTypes pieceType); // 0x620B00
    ~CTaskComplexReactToAttack() override = default; // 0x620B90 (scalar deleting dtor: 0x625C00)

    eTaskType GetTaskType() const override { return Type; } // 0x620B80
    CTask*    Clone() const override { return new CTaskComplexReactToAttack{ m_WeaponType, m_Attacker, m_Damage, m_HitDir, m_PieceType }; } // 0x623300
    bool      MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override; // 0x620BF0
    CTask*    CreateNextSubTask(CPed* ped) override; // 0x625C20
    CTask*    CreateFirstSubTask(CPed* ped) override; // 0x625C70
    CTask*    ControlSubTask(CPed* ped) override; // 0x620E00

    CTask* CreateSubTask(eTaskType taskType); // 0x620C30

private:
    CTaskComplexReactToAttack* Constructor(eWeaponType weaponType, CEntity* attacker, float damage, int32 hitDir, ePedPieceTypes pieceType) {
        this->CTaskComplexReactToAttack::CTaskComplexReactToAttack(weaponType, attacker, damage, hitDir, pieceType);
        return this;
    }

private:
    bool                      m_bRestart{};      // 0x0C - If set `ControlSubTask` creates the first sub-task again
    bool                      m_bAborting{};     // 0x0D - Set once the sub-task agreed to be aborted
    eWeaponType               m_WeaponType{};    // 0x10
    notsa::EntityRef<CEntity> m_Attacker;        // 0x14
    float                     m_Damage{};        // 0x18
    int32                     m_HitDir{};        // 0x1C
    ePedPieceTypes            m_PieceType{};     // 0x20
};
VALIDATE_SIZE(CTaskComplexReactToAttack, 0x24);
