#pragma once

#include "TaskComplexSequence.h"

class CTaskSequences {
public:
    static constexpr auto NUM_SEQUENCES = 64;

    static inline NOTSA_GLOBAL(ms_iActiveSequence, 0x8D2E98, (int32), { -1 });
    static inline NOTSA_GLOBAL(ms_taskSequence, 0xC178F0, (CTaskComplexSequence[NUM_SEQUENCES]), {}); static_assert(sizeof(ms_taskSequence) == 4096);
    static inline NOTSA_GLOBAL(ms_bIsOpened, 0xC17898, (bool[NUM_SEQUENCES]), {});

public:
    static void InjectHooks();

    static void Init();
    static void CleanUpForShutdown();
    static int32 GetAvailableSlot(uint8 slot);

    static CTaskComplexSequence& GetActiveSequence() { return ms_taskSequence[ms_iActiveSequence]; }
    static inline void AddTaskToActiveSequence(CTask* task) {
        ms_taskSequence[ms_iActiveSequence].AddTask(task);
    }
    static auto Get(uint32 slot) { return &ms_taskSequence[slot]; }
};
