#pragma once

#include "TaskComplex.h"
#include "TaskTimer.h"
#include "Vector.h"

class CPedGroup;

class NOTSA_EXPORT_VTABLE CTaskComplexGangFollower : public CTaskComplex {
public:
    CPedGroup* m_PedGroup;
    CPed*      m_Leader;
    CVector    m_PedPosn;       // Leader's position (set on construction)
    CVector    m_Offset;        // Current offset from the leader (updated by `CalculateOffsetPosition`)
    CVector    m_BaseOffset;    // Offset passed to the constructor
    float      m_fArg38;        // Unknown (ctor's last arg)
    uint8      m_Arg3C;         // Unknown (ctor's `a4`)
    bool       m_bAnimsReferenced : 1{};  // "gangs" anim block is referenced by this task
    bool       m_bSignalAtLeader : 1{};   // Signal at the leader, then leave the group
    bool       m_bUseSeekEntity : 1{};    // Use `CTaskComplexSeekEntity` to follow the leader
    bool       m_bLeaderIsPlayer : 1{};
    bool       m_bFlag4 : 1{};            // Unknown, set in the ctor only
    CTaskTimer m_ExhaleTimer;             // Timer for the exhale FX (when smoking)

public:
    static constexpr auto Type = eTaskType::TASK_COMPLEX_GANG_FOLLOWER;

    static constexpr bool ms_bUseClimbing = true; // 0x8D2EDC

    CTaskComplexGangFollower(CPedGroup* pedGroup, CPed* ped, uint8 a4, CVector pos, float a6);
    ~CTaskComplexGangFollower() override;

    eTaskType GetTaskType() const  override{ return Type; }
    CTask* Clone() const override;
    bool MakeAbortable(CPed* ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent* event = nullptr) override;
    CTask* CreateNextSubTask(CPed* ped) override;
    CTask* CreateFirstSubTask(CPed* ped) override;
    CTask* ControlSubTask(CPed* ped) override;

    CVector CalculateOffsetPosition();

private:
    friend void InjectHooksMain();
    static void InjectHooks();
    CTaskComplexGangFollower* Constructor(CPedGroup* pedGroup, CPed* ped, uint8 uint8, CVector pos, float a6) { this->CTaskComplexGangFollower::CTaskComplexGangFollower(pedGroup, ped, uint8, pos, a6); return this; }
    CTaskComplexGangFollower* Destructor() { this->CTaskComplexGangFollower::~CTaskComplexGangFollower(); return this; }
};
VALIDATE_SIZE(CTaskComplexGangFollower, 0x4C);
