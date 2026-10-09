/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

class CDecisionSimple {
public:
    std::array<eTaskType, 6> m_anTasks;
    std::array<float, 6>     m_afChances; // Cumulative, normalized
    uint32     m_nCount; // tasks count (max 6)

public:
    static void InjectHooks();

    void Set(int32* taskTypes, uint8* chances, int32 count);
    void MakeDecision(int32 taskType, int16& outTaskType, int32& outDecisionIndex); // 0x6007A0
    void SetDefault();
};

VALIDATE_SIZE(CDecisionSimple, 0x34);
