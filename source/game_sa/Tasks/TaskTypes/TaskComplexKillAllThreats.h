#pragma once

#include "TaskComplex.h"

class CPed;

//! Kills all the peds the ped hates (Fights one at a time, stands still if none are around)
class NOTSA_EXPORT_VTABLE CTaskComplexKillAllThreats : public CTaskComplex {
public:
    static constexpr auto Type = TASK_KILL_ALL_THREATS;

    static void InjectHooks();

    CTaskComplexKillAllThreats(int32 pedFlags, int32 actionDelay, int32 actionChance); // 0x622180
    ~CTaskComplexKillAllThreats() override = default; // 0x6221C0 (scalar deleting dtor: 0x6293A0)

    eTaskType GetTaskType() const override { return Type; } // 0x6221B0
    CTask*    Clone() const override { return new CTaskComplexKillAllThreats{ m_PedFlags, m_ActionDelay, m_ActionChance }; } // 0x623750
    CTask*    CreateNextSubTask(CPed* ped) override; // 0x6293C0
    CTask*    CreateFirstSubTask(CPed* ped) override; // 0x629480
    CTask*    ControlSubTask(CPed* ped) override; // 0x622230

    //! @return The first (script) ped found among the ped's nearby peds, that it hates
    CPed* FindThreat(CPed* ped); // 0x6221D0

private:
    CTaskComplexKillAllThreats* Constructor(int32 pedFlags, int32 actionDelay, int32 actionChance) { this->CTaskComplexKillAllThreats::CTaskComplexKillAllThreats(pedFlags, actionDelay, actionChance); return this; }

private:
    int32 m_PedFlags{};     // 0x0C - Passed on to `CTaskComplexKillPedOnFoot`
    int32 m_ActionDelay{};  // 0x10 - ^
    int32 m_ActionChance{}; // 0x14 - ^
};
VALIDATE_SIZE(CTaskComplexKillAllThreats, 0x18);
