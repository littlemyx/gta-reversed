#include "StdInc.h"

#include "Decision.h"

void CDecision::InjectHooks() {    
    RH_ScopedClass(CDecision);
    RH_ScopedCategory("DecisionMakers");

    RH_ScopedInstall(SetDefault, 0x600530);
    RH_ScopedInstall(Set, 0x600570);
    //RH_ScopedInstall(Add, 0x600600, { .Reversed = false });
    RH_ScopedInstall(From, 0x6006B0);
    //RH_ScopedInstall(HasResponse, 0x600710, { .Reversed = false });
    //RH_ScopedInstall(MakeDecision, 0x6040D0, { .Reversed = false });
}

// 0x6040C0
CDecision::CDecision() {
    SetDefault();
}

// 0x600530
void CDecision::SetDefault() {
    for (auto i = 0; i < MAX_NUM_CHOICES; i++) {
        m_Tasks[i] = static_cast<eTaskType>(-1);
        m_Probs[i].fill(0);
    }
    // Original zeroes a uint16 per choice at +0x30 (12 bytes in total)
    for (auto& row : m_Bools) {
        row.fill(false);
    }
}

// 0x6006B0
void CDecision::From(const CDecision& rhs) {
    *this = rhs;
}

// 0x600570
void CDecision::Set(
    notsa::mdarray<int32, MAX_NUM_CHOICES>&    tasks,
    notsa::mdarray<float, MAX_NUM_CHOICES, 4>& probs,
    notsa::mdarray<int32, MAX_NUM_CHOICES, 2>& bools,
    notsa::mdarray<float, MAX_NUM_CHOICES, 6>& facialProbs
) {
    // NOTE: `facialProbs` is unused by the original

    // _ftol (0x821B40) truncates to int64 (0x8000000000000000 on overflow/NaN); only the low byte is stored
    const auto FtolToByte = [](float f) -> uint8 {
        const double v = f;
        if (!(v > -9223372036854775808.0 && v < 9223372036854775808.0)) {
            return 0;
        }
        return (uint8)(int64)v;
    };

    for (auto i = 0; i < MAX_NUM_CHOICES; i++) {
        m_Tasks[i] = (eTaskType)tasks[i];
        for (auto j = 0; j < 4; j++) {
            m_Probs[i][j] = FtolToByte(probs[i][j]);
        }
        m_Bools[i][0] = bools[i][0] != 0;
        m_Bools[i][1] = bools[i][1] != 0;
    }
}

// 0x600600
void CDecision::Add(eTaskType taskId, float* responseChances, int32* flags) {
    plugin::CallMethod<0x600600, CDecision*, eTaskType, float*, int32*>(this, taskId, responseChances, flags);
}

// 0x6040D0
void CDecision::MakeDecision(int32 eventSourceType, bool bIsPedInVehicle, eTaskType taskTypeToAvoid1, eTaskType taskTypeToAvoid2, eTaskType taskTypeToAvoid3, eTaskType taskTypeToSeek, int16& taskType, int16& facialTaskType) {
    plugin::CallMethod<0x6040D0, CDecision*, int32, bool, eTaskType, eTaskType, eTaskType, eTaskType, int16*, int16*>(this, eventSourceType, bIsPedInVehicle, taskTypeToAvoid1, taskTypeToAvoid2, taskTypeToAvoid3, taskTypeToSeek, &taskType, &facialTaskType);
}
