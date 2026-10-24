#pragma once

class CGamma {
public:
    inline static NOTSA_GLOBAL(ms_SavedGamma, 0xC8D4C8, (D3DGAMMARAMP), {});
    inline static NOTSA_GLOBAL(ms_GammaTable, 0xC8DAC8, (D3DGAMMARAMP), {});

public:
    bool  m_IsEnabled;
    float m_CurrentLevel;
    float m_TargetLevel;
    float m_PrevLevel;

public:
    static void InjectHooks();

    void Init();
    void SetGamma(float gammaLevel, bool fade);
};

VALIDATE_SIZE(CGamma, 0x10);
#ifdef NOTSA_PORTABLE
#include <cmath>
#define gamma notsa_gamma // glibc <math.h> declares a function gamma(double): the game's global CGamma `gamma` is renamed (the libm declaration is already in)
#endif

NOTSA_GLOBAL_HDR(gbGammaChanged, 0xC920C8, (bool), {});
NOTSA_GLOBAL_HDR(gamma, 0xC92134, (CGamma), {});
