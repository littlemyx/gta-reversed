#pragma once

struct CWindModifier {
    CVector m_vecPos;
    int32   m_iActive;
    float   m_fPower;
};

class CWindModifiers {
public:
    static constexpr int32 MAX_NUM_MODIFIERS = 16;

    static inline NOTSA_GLOBAL(Number, 0xC81450, (int32), {});
    static inline NOTSA_GLOBAL(Array, 0xC81458, (CWindModifier[MAX_NUM_MODIFIERS]), {});

public:
    static void InjectHooks();

    static void RegisterOne(CVector vecPos, int32 iActive, float fPower);
    static bool FindWindModifier(CVector vecPos, float* outX, float* outY);
};
