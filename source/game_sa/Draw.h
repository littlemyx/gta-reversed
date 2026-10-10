/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

NOTSA_GLOBAL_HDR(JustLoadedDontFadeInYet, 0xC16EDC, (bool), {});
NOTSA_GLOBAL_HDR(StillToFadeOut, 0xC16EDD, (bool), {});

class CDraw {
public:
    static inline NOTSA_GLOBAL(ms_fFOV, 0x8D5038, (float), { 45.0f }); // 45.0
    static inline NOTSA_GLOBAL(ms_fLODDistance, 0xC3EF98, (float), {});
    static inline NOTSA_GLOBAL(ms_fNearClipZ, 0xC3EFA0, (float), {});
    static inline NOTSA_GLOBAL(ms_fFarClipZ, 0xC3EF9C, (float), {});
    static inline NOTSA_GLOBAL(ms_fAspectRatio, 0xC3EFA4, (float), {});

    static inline NOTSA_GLOBAL(FadeRed, 0xC3EFA8, (uint8), {});
    static inline NOTSA_GLOBAL(FadeGreen, 0xC3EFA9, (uint8), {});
    static inline NOTSA_GLOBAL(FadeBlue, 0xC3EFAA, (uint8), {});
    static inline NOTSA_GLOBAL(FadeValue, 0xC3EFAB, (uint8), {});

public:
    static void InjectHooks();

    static void SetFOV(float fov);
    static float GetFOV() { return ms_fFOV; }

    static void SetNearClipZ(float nearClip) { ms_fNearClipZ = nearClip; }
    static float GetNearClipZ() { return ms_fNearClipZ; }

    static void SetFarClipZ(float farClip) { ms_fFarClipZ = farClip; }
    static float GetFarClipZ() { return ms_fFarClipZ; }

    static float GetAspectRatio() { return ms_fAspectRatio; }
    static void SetAspectRatio(float ratio) { ms_fAspectRatio = ratio; }

    static void CalculateAspectRatio();

    // @notsa
    static bool IsFading() { return FadeValue != 0u; }
};

extern void DoFade();
