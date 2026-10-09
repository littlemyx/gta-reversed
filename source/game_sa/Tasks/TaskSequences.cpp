#include "StdInc.h"

void CTaskSequences::InjectHooks() {
    RH_ScopedClass(CTaskSequences);
    RH_ScopedCategory("Tasks");

    RH_ScopedInstall(Init, 0x632D90);
    RH_ScopedInstall(CleanUpForShutdown, 0x632DD0);
    RH_ScopedInstall(GetAvailableSlot, 0x632E00);
}

// 0x632D90
void CTaskSequences::Init() {
    ms_iActiveSequence = -1;
    CleanUpForShutdown();
}

// 0x632DD0
void CTaskSequences::CleanUpForShutdown() {
    for (auto i = 0; i < NUM_SEQUENCES; i++) {
        ms_bIsOpened[i] = false;
        auto taskSequence = &ms_taskSequence[i];
        taskSequence->Flush();
    }
}

// 0x632E00
// `slot` is really a bool: the script uses mission cleanup => slots [32, 64), else [0, 32).
// A slot is free if it isn't opened and has no task in its first position.
int32 CTaskSequences::GetAvailableSlot(uint8 slot) {
    const int32 first = slot ? NUM_SEQUENCES / 2 : 0;
    const int32 end   = slot ? NUM_SEQUENCES : NUM_SEQUENCES / 2;
    for (int32 i = first; i < end; i++) {
        if (!ms_bIsOpened[i] && !ms_taskSequence[i].m_Tasks[0]) {
            return i;
        }
    }
    return -1;
}
