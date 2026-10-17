#pragma once

#include <span>

#include "Vector.h"
#include "ActiveOccluder.h"
#include "Occluder.h"

class COcclusion {
public:
    static constexpr int32 MAX_INTERIOR_OCCLUDERS            = 40;
    static constexpr int32 MAX_MAP_OCCLUDERS                 = 1'000;
    static constexpr int32 MAX_ACTIVE_OCCLUDERS              = 28;
    static constexpr int32 NUM_OCCLUDERS_PROCESSED_PER_FRAME = 16;

    static inline NOTSA_GLOBAL(InteriorOccluders, 0xC73CC8, (std::array<COccluder, MAX_INTERIOR_OCCLUDERS>), {});
    static inline NOTSA_GLOBAL(Occluders, 0xC73FA0, (std::array<COccluder, MAX_MAP_OCCLUDERS>), {});
    static inline NOTSA_GLOBAL(ActiveOccluders, 0xC78610, (std::array<CActiveOccluder, MAX_ACTIVE_OCCLUDERS>), {});

    static inline NOTSA_GLOBAL(NumInteriorOccludersOnMap, 0xC73CC4, (size_t), {});
    static inline NOTSA_GLOBAL(NumOccludersOnMap, 0xC73F98, (size_t), {});
    static inline NOTSA_GLOBAL(NumActiveOccluders, 0xC73CC0, (size_t), {});

    static inline NOTSA_GLOBAL(NearbyList, 0x8D5D6C, (int16), { -1 });

    static inline NOTSA_GLOBAL(FarAwayList, 0x8D5D68, (int16), { -1 });
    static inline NOTSA_GLOBAL(ListWalkThroughFA, 0x8D5D70, (int16), { -1 });
    static inline NOTSA_GLOBAL(PreviousListWalkThroughFA, 0x8D5D74, (int16), { -1 });

public:
    static void InjectHooks();

public:
    static void Init();
    static void AddOne(float centerX, float centerY, float centerZ, float width, float length, float height, float rotZ, float rotY, float rotX, uint32 flags, bool isInterior);
    static bool OccluderHidesBehind(CActiveOccluder* first, CActiveOccluder* second);
    static bool IsPositionOccluded(CVector vecPos, float fRadius);
    static void ProcessBeforeRendering();
    static auto GetActiveOccluders() { return ActiveOccluders | rng::views::take((size_t)NumActiveOccluders); }
};
