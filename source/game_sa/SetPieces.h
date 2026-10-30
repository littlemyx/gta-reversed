#pragma once

class CSetPiece;

static constexpr auto MAX_SET_PIECES = 210;

class CSetPieces {
public:
    static inline NOTSA_GLOBAL(aSetPieces, 0xA95818, (std::array<CSetPiece, MAX_SET_PIECES>), {});
    static inline NOTSA_GLOBAL(NumSetPieces, 0xA957FC, (uint32), {});
    static inline NOTSA_GLOBAL(bDebug, 0xA957F8, (bool), {});

public:
    static void InjectHooks();

    static void AddOne(eSetPieceType type, CVector2D cornerA, CVector2D cornerB, CVector2D spawnCoord1, CVector2D spawnCoord2, CVector2D targetCoord1, CVector2D targetCoord2);
    static void Init();
    static bool Load();
    static bool Save();
    static void Update();
};
