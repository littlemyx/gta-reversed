#include "StdInc.h"

#include "Decision.h"
#include "DecisionSimple.h"

// _ftol (0x821B40) truncates to int64 (0x8000000000000000 on overflow/NaN); only the low byte is stored
static uint8 FtolToByte(float f) {
    const double v = f;
    if (!(v > -9223372036854775808.0 && v < 9223372036854775808.0)) {
        return 0;
    }
    return (uint8)(int64)v;
}

void CDecision::InjectHooks() {    
    RH_ScopedClass(CDecision);
    RH_ScopedCategory("DecisionMakers");

    RH_ScopedInstall(SetDefault, 0x600530);
    RH_ScopedInstall(Set, 0x600570);
    RH_ScopedInstall(Add, 0x600600);
    RH_ScopedInstall(From, 0x6006B0);
    //RH_ScopedInstall(HasResponse, 0x600710, { .Reversed = false });
    RH_ScopedInstall(MakeDecision, 0x6040D0);

    CDecisionSimple::InjectHooks(); // NOTSA: Not registered in InjectHooksMain.cpp
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
    for (auto i = 0; i < MAX_NUM_CHOICES; i++) {
        if (m_Tasks[i] != (eTaskType)-1) {
            continue;
        }
        if (m_Probs[i][0] || m_Probs[i][1] || m_Probs[i][2] || m_Probs[i][3]) {
            continue;
        }
        if (m_Bools[i][0] || m_Bools[i][1]) {
            continue;
        }

        // First free choice found
        m_Tasks[i] = taskId;
        for (auto j = 0; j < 4; j++) {
            m_Probs[i][j] = FtolToByte(responseChances[j]);
        }
        m_Bools[i][0] = flags[0] != 0;
        m_Bools[i][1] = flags[1] != 0;
        return;
    }
}

// 0x6040D0
void CDecision::MakeDecision(int32 eventSourceType, bool bIsPedInVehicle, eTaskType taskTypeToAvoid1, eTaskType taskTypeToAvoid2, eTaskType taskTypeToAvoid3, eTaskType taskTypeToSeek, int16& taskType, int16& facialTaskType) {
    // The valid choices (the original fills the arrays with 200 / 0)
    std::array<int32, MAX_NUM_CHOICES> tasks;
    std::array<uint8, MAX_NUM_CHOICES> probs{};
    tasks.fill(200);
    auto numChoices = 0;

    for (auto i = 0; i < MAX_NUM_CHOICES; i++) {
        const auto prob = m_Probs[i][eventSourceType];
        if (prob == 0) {
            continue;
        }
        const auto task = m_Tasks[i];
        if (task == (eTaskType)-1 || task == taskTypeToAvoid1 || task == taskTypeToAvoid2 || task == taskTypeToAvoid3) {
            continue;
        }
        if (!m_Bools[i][bIsPedInVehicle ? 1 : 0]) {
            continue;
        }
        tasks[numChoices] = task;
        probs[numChoices] = prob;
        numChoices++;
    }

    taskType = 200;
    if (numChoices <= 0) {
        facialTaskType = (int16)0xFFFF;
        return;
    }

    // Build the cumulative distribution (`CDecisionSimple::Set`, inlined in the original)
    CDecisionSimple simple;
    double sum = 0.0; // x87 register
    for (auto i = 0; i < MAX_NUM_CHOICES; i++) {
        simple.m_anTasks[i] = (eTaskType)tasks[i];
        sum += (double)probs[i];
        simple.m_afChances[i] = (float)sum;
    }
    simple.m_nCount = MAX_NUM_CHOICES;
    const double scale = (double)1.0f / sum; // 0x858624 = 1.0f
    for (auto i = 0; i < MAX_NUM_CHOICES; i++) {
        simple.m_afChances[i] = (float)(scale * (double)simple.m_afChances[i]);
    }

    int32 decisionIdx; // Unused
    simple.MakeDecision(taskTypeToSeek, taskType, decisionIdx); // 0x6007A0
    facialTaskType = (int16)0xFFFF;
}
