#include "StdInc.h"

#include "DecisionSimple.h"
#include "General.h"

void CDecisionSimple::InjectHooks() {
    RH_ScopedClass(CDecisionSimple);
    RH_ScopedCategory("DecisionMakers");

    RH_ScopedInstall(MakeDecision, 0x6007A0);
}

// 0x6007A0
void CDecisionSimple::MakeDecision(int32 taskType, int16& outTaskType, int32& outDecisionIndex) {
    outTaskType      = 200;
    outDecisionIndex = -1;

    // Look for the requested task
    if (taskType != -1) {
        for (auto i = 0; i < (int32)m_nCount; i++) {
            if ((int32)m_anTasks[i] == taskType) {
                outTaskType      = (int16)m_anTasks[i];
                outDecisionIndex = i;
                break;
            }
        }
    }
    if (outTaskType != 200) {
        return;
    }

    // Otherwise pick one randomly
    const float roll = (float)((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL); // 0x858C7C
    for (auto i = 0; i < (int32)m_nCount; i++) {
        if (roll <= m_afChances[i]) { // FCOMP + JNP (exactly one of C0/C3 set): `roll <= chance`, NaN => no match
            outTaskType      = (int16)m_anTasks[i];
            outDecisionIndex = i;
            return;
        }
    }
}
