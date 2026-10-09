#include "StdInc.h"

#include "DecisionMaker.h"

void CDecisionMaker::InjectHooks() {
    RH_ScopedClass(CDecisionMaker);
    RH_ScopedCategory("DecisionMakers");

    RH_ScopedInstall(Constructor, 0x4650A0);
}

// 0x4650A0
CDecisionMaker::CDecisionMaker() {
    // NOTE: The original runs the `CDecision` ctor (0x6040C0 => SetDefault) and then `SetDefault` (0x600530) once more per element; idempotent.
    for (auto& decision : m_aDecisions) {
        decision.SetDefault();
    }
}
