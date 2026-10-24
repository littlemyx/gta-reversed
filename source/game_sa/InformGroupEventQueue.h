#pragma once
#include "InformGroupEvent.h"

class CEventEditableResponse;
class CPed;
class CPedGroup;

class CInformGroupEventQueue {
public:
    static inline NOTSA_GLOBAL(ms_informGroupEvents, 0xA9B018, (std::array<CInformGroupEvent, 8>), {}); VALIDATE_SIZE(ms_informGroupEvents, 128);

public:
    static void InjectHooks();

    static void Init();
    static bool Add(CPed* ped, CPedGroup* pedGroup, CEventEditableResponse* event);
    static void Flush();
    static void Process();
};
