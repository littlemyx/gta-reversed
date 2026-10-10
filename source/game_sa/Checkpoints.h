#pragma once

#include "Checkpoint.h"

class CCheckpoint;
namespace checkpoints_detail { // all-zero CCheckpoint (CVector has a user-provided operator=: std::bit_cast of zero bytes is not available)
inline constexpr CCheckpoint kZeroCheckpoint{ eCheckpointType::TUBE, false, false, 0, CRGBA{ 0, 0, 0, 0 }, 0, 0, CVector{}, CVector{}, 0.f, 0.f, 0.f, 0.f };
template<size_t... I> constexpr std::array<CCheckpoint, sizeof...(I)> MakeZeroCheckpoints(std::index_sequence<I...>) { return { { (static_cast<void>(I), kZeroCheckpoint)... } }; }
}
#line 6

constexpr auto MAX_NUM_CHECKPOINTS{ 32u };

class CCheckpoints {
public:
    static inline NOTSA_GLOBAL(NumActiveCPts, 0xC7C6D4, (uint32), {}); // not used, only initialised (0)
    static inline NOTSA_GLOBAL(m_aCheckPtArray, 0xC7F158, (std::array<CCheckpoint, 32>), = checkpoints_detail::MakeZeroCheckpoints(std::make_index_sequence<32>{})); // all-zero like the exe's .bss (NOT the NSDMI defaults of CCheckpoint)

public:
    static void InjectHooks();

    static void Init();
    static void Shutdown();
    static void SetHeading(uint32 id, float angle);
    static void Update();
    static CCheckpoint* PlaceMarker(uint32 id, eCheckpointTypeU16 type, const CVector& posn, const CVector& direction, float size, CRGBA color, uint16 pulsePeriod, float pulseFraction, int16 rotateRate);
    /*!
     * @brief Set the position of a checkpoint with the given `id`
     * @param id ID of the checkpoint
     * @param posn The new position
    */
    static void UpdatePos(uint32 id, const CVector& posn);
    static void DeleteCP(uint32 id, uint16 type);
    static void Render();

    static inline CCheckpoint* FindById(uint32 id);
};
