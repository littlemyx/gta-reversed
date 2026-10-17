#pragma once

#include "Skidmark.h"

constexpr auto SKIDMARKS_COUNT{ 32u };

class CSkidmarks {
public:
    static inline NOTSA_GLOBAL(m_aIndices, 0xC799C8, (std::array<RxVertexIndex, 96>), {});
    static inline NOTSA_GLOBAL(m_pTexture, 0xC79A88, (RwTexture*), {});
    static inline NOTSA_GLOBAL(m_aSkidmarks, 0xC79AA8, (CSkidmark[SKIDMARKS_COUNT]), {});

public:
    static void InjectHooks();

    static void Init();
    static void Shutdown();
    static void Clear();
    static void Update();
    static void Render();
    static void RegisterOne(uint32 index, const CVector& posn, float dirX, float dirY, eSkidmarkType type, bool* bloodState, float length);
    static void RegisterOne(uint32 index, const CVector& posn, float dirX, float dirY, bool* isSandy, bool* bloodState, float length);

    static CSkidmark* FindById(uint32 id);
    static CSkidmark* GetNextFree(bool forceFree);
};
