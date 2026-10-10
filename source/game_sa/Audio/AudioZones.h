#pragma once

#include "CompressedBox.h"
#include <span>

struct tAudioZoneData {
    char  m_szName[8];
    int16 m_nAudioZone;
    bool  m_IsActive : 1;
};
VALIDATE_SIZE(tAudioZoneData, 0xC);

struct tAudioZoneSphere : tAudioZoneData {
    CSphere m_Sphere;
};
VALIDATE_SIZE(tAudioZoneSphere, 0x1C);

struct tAudioZoneBox : tAudioZoneData {
    CompressedBox m_Box;

    void DrawWireFrame(CRGBA color, const CMatrix& transform) const {
        m_Box.DrawWireFrame(color, transform);
    }
};
VALIDATE_SIZE(tAudioZoneBox, 0x18);

class CAudioZones {
public:
    static inline NOTSA_GLOBAL(m_aActiveBoxes, 0xB6DC6C, (std::array<int32, 10>), {});
    static inline NOTSA_GLOBAL(m_aActiveSpheres, 0xB6DC94, (std::array<int32, 10>), {});

    static inline NOTSA_GLOBAL(m_NumActiveBoxes, 0xB6DCBC, (uint32), {});
    static inline NOTSA_GLOBAL(m_NumActiveSpheres, 0xB6DCC0, (uint32), {});
    static inline NOTSA_GLOBAL(m_NumBoxes, 0xB6DCC4, (uint32), {});
    static inline NOTSA_GLOBAL(m_NumSpheres, 0xB6DCC8, (uint32), {});

    static constexpr int32 NUM_AUDIO_BOXES = 158;
    static inline NOTSA_GLOBAL(m_aBoxes, 0xB6DCD0, (std::array<tAudioZoneBox, NUM_AUDIO_BOXES>), {});

    static constexpr int32 NUM_AUDIO_SPHERES = 3;
    static inline NOTSA_GLOBAL(m_aSpheres, 0xB6EBA8, (std::array<tAudioZoneSphere, NUM_AUDIO_SPHERES>), {});

public:
    static void InjectHooks();

    static void Init();

    static void RegisterAudioBox(char name[8], int32 id, bool isActive, CVector min, CVector max);
    static void RegisterAudioSphere(char name[8], int32 id, bool isActive, CVector position, float radius);

    static void SwitchAudioZone(const char* zoneName, bool enable);
    static void Update(bool forceUpdate, CVector posn);

    static auto GetActiveAuZoBoxes() { // TODO/NOTE: This isn't how it works! See `m_aActiveBoxes`
        return m_aBoxes | rng::views::take((size_t)m_NumBoxes);
    }

    // TODO: idc about func at top rn.
    static auto GetAvailableBoxes() {
        return m_aBoxes | rng::views::take((size_t)m_NumBoxes);
    }

    static auto GetAvailableSpheres() {
        return m_aSpheres | rng::views::take((size_t)m_NumSpheres);
    }
};
