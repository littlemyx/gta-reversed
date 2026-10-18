#pragma once

#include "TaskAllocatorKillThreatsBasic.h"

// Originally didn't inherit from `CTaskAllocatorKillThreatsBasic`,
// but they share a few things, so why not
class NOTSA_EXPORT_VTABLE CTaskAllocatorKillThreatsDriveby final : public CTaskAllocatorKillThreatsBasic {
public:
    constexpr static inline auto Type = eTaskAllocatorType::KILL_THREATS_DRIVEBY;

    static void InjectHooks();

    using CTaskAllocatorKillThreatsBasic::CTaskAllocatorKillThreatsBasic;
    ~CTaskAllocatorKillThreatsDriveby() override = default;

    eTaskAllocatorType GetType() override { return eTaskAllocatorType::KILL_THREATS_DRIVEBY; }; // 0x69CB50
    void               AllocateTasks(CPedGroupIntelligence* intel) override;

// NOTE: no members of its own. A `m_Threat` / `m_Timer` declared here shadowed the base's (which the inherited constructor and the inherited
// ProcessGroup (0x69CBC0 is byte-for-byte the same logic as Basic's 0x69C7E0: 5 s timer) fill and use): AllocateTasks then always saw a null threat and
// the driveby response never allocated a single task (VT-OVERRIDES). Same layout as the exe: vptr, threat ref (+4), timer (+8).

private: // Wrappers for hooks
    // 0x69CAF0
    CTaskAllocatorKillThreatsDriveby* Constructor(CPed* a) {
        this->CTaskAllocatorKillThreatsDriveby::CTaskAllocatorKillThreatsDriveby(a);
        return this;
    }

    // 0x69CB60
    CTaskAllocatorKillThreatsDriveby* Destructor() {
        this->CTaskAllocatorKillThreatsDriveby::~CTaskAllocatorKillThreatsDriveby();
        return this;
    }
};
