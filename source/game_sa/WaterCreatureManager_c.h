#pragma once

#include "WaterCreature_c.h"

// WaterCreatureInfo_t
struct tWaterCreatureInfo {
    uint16* m_pModelId;

    uint8 m_nMinCreated;
    uint8 m_nMaxCreated;

    float m_fMinSpawnDepth;
    float m_fMinDistFromSameCreature;

    float m_fMinDistFromFollowed;
    float m_fMaxDistFromFollowed;

    float m_fMinSpeed;
    float m_fMaxSpeed;

    float m_fMaxHeadingChange;
    float m_fChanceToRandomizeRotation;

    float m_Speed;
};
VALIDATE_SIZE(tWaterCreatureInfo, 0x2C);

class WaterCreatureManager_c {
public:
    static constexpr int32 NUM_WATER_CREATURES = 128;
    static constexpr int32 NUM_WATER_CREATURE_INFOS = 7;

    static constexpr float ms_fMaxWaterCreaturesDrawDistance        = 60.0F;
    static constexpr float ms_fMaxWaterCreaturesDrawDistanceSquared = sq(ms_fMaxWaterCreaturesDrawDistance);

private:
    static inline NOTSA_GLOBAL(ms_waterCreatureInfos, 0x8D3698, (tWaterCreatureInfo[NUM_WATER_CREATURE_INFOS]), { { reinterpret_cast<uint16*>(&ModelIndices::MI_FISH1SINGLE), 1, 20, 4.0f, 10.0f, 0.5f, 3.0f, 1.0f, 3.5f, 0.1f, 0.005f, 8.0f }, { reinterpret_cast<uint16*>(&ModelIndices::MI_FISH2SINGLE), 1, 20, 4.0f, 10.0f, 0.5f, 3.0f, 1.0f, 3.5f, 0.1f, 0.005f, 8.0f }, { reinterpret_cast<uint16*>(&ModelIndices::MI_FISH3SINGLE), 1, 20, 4.0f, 10.0f, 0.5f, 3.0f, 1.0f, 3.5f, 0.1f, 0.005f, 8.0f }, { reinterpret_cast<uint16*>(&ModelIndices::MI_JELLYFISH), 10, 25, 0.0f, 20.0f, 0.5f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }, { reinterpret_cast<uint16*>(&ModelIndices::MI_JELLYFISH01), 10, 25, 0.0f, 20.0f, 0.5f, 5.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }, { reinterpret_cast<uint16*>(&ModelIndices::MI_TURTLE), 1, 2, 6.0f, 20.0f, 3.0f, 6.0f, 2.0f, 5.0f, 0.07f, 0.005f, 12.0f }, { reinterpret_cast<uint16*>(&ModelIndices::MI_DOLPHIN), 1, 1, 8.0f, 30.0f, 8.0f, 10.0f, 4.0f, 7.0f, 0.05f, 0.005f, 20.0f } }); // Access using GetCreatureInfo()

    WaterCreature_c m_waterCreatureItems[NUM_WATER_CREATURES];
    TList_c<WaterCreature_c> m_waterCreaturePool;
    TList_c<WaterCreature_c> m_waterCreatureList;

    int32 m_lastUpdateTime;

public:
    WaterCreatureManager_c();
    ~WaterCreatureManager_c();

    bool Init();
    void Exit();

    void Update(float deltaTime);

    int32 TryToFreeUpWaterCreatures(int32 numToFree);

private:
    int32 GetRandomWaterCreatureId(); // eWaterCreatureType
    bool CanAddWaterCreatureAtPos(int32 id, CVector pos);
    void TryToExitGroup(WaterCreature_c* waterCreature);

public: // NOTSA
    static const tWaterCreatureInfo& GetCreatureInfo(uint32/*eWaterCreatureType*/ type) { return ms_waterCreatureInfos[type]; }

private:
    friend WaterCreature_c;
    friend void InjectHooksMain();
    static void InjectHooks();
};

NOTSA_GLOBAL_EXTERN(g_waterCreatureMan, (WaterCreatureManager_c));
