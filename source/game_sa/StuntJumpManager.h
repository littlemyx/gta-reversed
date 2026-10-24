#pragma once

#include "Vector.h"
#include "Pool.h"
#include "BoundingBox.h"

enum class eJumpState : uint8 {
    START_POINT_INTERSECTED,
    IN_FLIGHT,
    END_POINT_INTERSECTED,
};

struct CStuntJump {
    CBoundingBox start;
    CBoundingBox end;
    CVector      camera;
    int32        reward;
    bool         done;
    bool         found;
};

VALIDATE_SIZE_FILE(CStuntJump, 0x44);

typedef CPool<CStuntJump> CStuntJumpsPool;

class CStuntJumpManager {
public:
    static inline NOTSA_GLOBAL(mp_poolStuntJumps, 0xA9A888, (CStuntJumpsPool*), {});
    static inline NOTSA_GLOBAL(mp_Active, 0xA9A88C, (CStuntJump*), {}); // nullptr
    static inline NOTSA_GLOBAL(m_bActive, 0xA9A890, (bool), {});
    static inline NOTSA_GLOBAL(m_bHitReward, 0xA9A891, (bool), {});
    static inline NOTSA_GLOBAL(m_iTimer, 0xA9A894, (uint32), {});
    static inline NOTSA_GLOBAL(m_jumpState, 0xA9A898, (eJumpState), {});
    static inline NOTSA_GLOBAL(m_iNumJumps, 0xA9A89C, (int32), {});
    static inline NOTSA_GLOBAL(m_iNumCompleted, 0xA9A8A0, (uint32), {});

public:
    static void InjectHooks();

    static void Init();
    static void Shutdown();
    static void ShutdownForRestart();
    static bool Save();
    static bool Load();
    static void AddOne(const CBoundingBox& start, const CBoundingBox& end, const CVector& cameraPosn, int32 reward);
    static void Update();

    static void SetActive(bool active);
    static void Render();
};

extern void StuntJumpTestCode();
