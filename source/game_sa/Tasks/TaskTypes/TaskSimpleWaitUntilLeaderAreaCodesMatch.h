#pragma once

#include "TaskSimpleWaitUntilAreaCodesMatch.h"
#include "Vector.h"

class CPed;
class CEvent;

class NOTSA_EXPORT_VTABLE CTaskSimpleWaitUntilLeaderAreaCodesMatch final : public CTaskSimpleWaitUntilAreaCodesMatch {
public:
    bool    m_bGrabbedStartPos{};
    CVector m_vStartPos{};
    bool    m_b{};

public:
    static constexpr auto Type = eTaskType::TASK_WAIT_FOR_MATCHING_LEADER_AREA_CODES;

    static void InjectHooks();

    using CTaskSimpleWaitUntilAreaCodesMatch::CTaskSimpleWaitUntilAreaCodesMatch;
    CTaskSimpleWaitUntilLeaderAreaCodesMatch(const CTaskSimpleWaitUntilLeaderAreaCodesMatch& o) = default;

    CTask*    Clone() const override { // 0x5F67F0: new task from the area code, then [esi+24h] = [esi+34h] = 0: the runtime state (grabbed start pos / m_b) is NOT cloned
        auto* const t = new CTaskSimpleWaitUntilLeaderAreaCodesMatch{*this};
        t->m_bGrabbedStartPos = false;
        t->m_b = false;
        return t;
    }
    eTaskType GetTaskType() const override { return Type; }
    bool      MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) override;
    bool      ProcessPed(CPed* ped) override;
};
