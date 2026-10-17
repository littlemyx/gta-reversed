#pragma once

#include "common.h"
#include "Vector2D.h"
#include "Vector.h"

class CActiveOccluder;

class COccluder {
public:
    static inline NOTSA_GLOBAL(MaxYInOccluder, 0xC73CA0, (float), {});
    static inline NOTSA_GLOBAL(MinYInOccluder, 0xC73CA4, (float), {});
    static inline NOTSA_GLOBAL(MaxXInOccluder, 0xC73CA8, (float), {});
    static inline NOTSA_GLOBAL(MinXInOccluder, 0xC73CAC, (float), {});

    static inline NOTSA_GLOBAL(OccluderCoorsValid, 0xC73CB0, (std::array<bool, 8>), {});
    static inline NOTSA_GLOBAL(OccluderCoors, 0xC798E0, (std::array<CVector, 8>), {});
    static inline NOTSA_GLOBAL(OccluderCoorsOnScreen, 0xC79950, (std::array<CVector, 8>), {});
    static inline NOTSA_GLOBAL(CenterOnScreen, 0xC79940, (CVector), {});

public:
    static void InjectHooks();

    bool ProcessOneOccluder(CActiveOccluder* activeOccluder);
    bool ProcessLineSegment(int32 iInd1, int32 iInd2, CActiveOccluder* activeOccluder);
    bool NearCamera() const;
    int16 SetNext(int16 next);
    int16 GetNext() const { return m_NextIndex; }
    CVector GetRotation() const noexcept { return { m_RotX, m_RotY, m_RotZ }; }
    CVector GetSize() const noexcept { return { m_Width, m_Length, m_Height }; }

public:
    FixedVector<int16, 4.f> m_Center{};

    // As usual, some R* dev from 2003 decided to swap length and width
    // that is, in the structure, it's `m_Length, m_Width, m_Height`
    // so converting it all into one FixedVector would be fucking confusing
    // (as `size.x` would not be `m_Width` but rather `m_Length`
    // TODO(In standalone): Replace this with a vector and fix the abovementioend issue.
    FixedFloat<int16, 4.f> m_Length;
    FixedFloat<int16, 4.f> m_Width;
    FixedFloat<int16, 4.f> m_Height;

    // As usual, some R* dev from 2003 decided to swap the order of rotations as well :D
    // UNSIGNED bytes: the exe decodes them with `movzx; fild; fmul 2pi/256 (0x859BBC)` (0x71E743), so [0, 2pi); a signed type gave negative angles for raw >= 128
    FixedFloat<uint8, 256.f / TWO_PI> m_RotZ;
    FixedFloat<uint8, 256.f / TWO_PI> m_RotY;
    FixedFloat<uint8, 256.f / TWO_PI> m_RotX;

    struct {
        int16 m_NextIndex : 15;
        int16 m_DontStream : 1;
    };
};
VALIDATE_SIZE(COccluder, 0x12);
