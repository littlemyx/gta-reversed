/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"

#include "Rope.h"
#include "Ropes.h"
#include <bit>

void CRopes::InjectHooks() {
    RH_ScopedClass(CRopes);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x555DC0);
    RH_ScopedInstall(Shutdown, 0x556B10);
    RH_ScopedInstall(Update, 0x558D70);
    RH_ScopedInstall(Render, 0x556AE0);
    RH_ScopedInstall(RegisterRope, 0x556B40);
    RH_ScopedInstall(FindPickupHeight, 0x556760);
    RH_ScopedInstall(FindRope, 0x556000);
    RH_ScopedInstall(FindCoorsAlongRope, 0x555E40);
    RH_ScopedInstall(CreateRopeForSwatPed, 0x558D10);
    RH_ScopedInstall(IsCarriedByRope, 0x555F80);
    RH_ScopedInstall(SetSpeedOfTopNode, 0x555DF0);
}

// 0x555DC0
void CRopes::Init() {
    for (auto& rope : aRopes) {
        rope.m_nType = eRopeType::NONE;
    }
    PlayerControlsCrane = eControlledCrane::NONE;
}

// 0x556B10
void CRopes::Shutdown() {
    for (auto& rope : aRopes) {
        if (rope.m_nType == eRopeType::NONE)
            continue;

        rope.Remove();
    }
}

// 0x558D70
void CRopes::Update() {
    ZoneScoped;

    if (CReplay::Mode == MODE_PLAYBACK)
        return;

    for (auto& rope : aRopes) {
        if (rope.m_nType != eRopeType::NONE)
            rope.Update();
    }
}

// 0x556AE0
void CRopes::Render() {
    ZoneScoped;

    for (auto& rope : aRopes) {
        if (rope.m_nType != eRopeType::NONE)
            rope.Render();
    }
}

// Must be used in loop to make attached to holder
// 0x556B40
bool CRopes::RegisterRope(uint32 ropeID, uint32 ropeType, CVector startPos, bool bExpires, uint8 segmentCount, uint8 flags, CPhysical* holder, uint32 timeExpire) {
    // NOTE: `CRope::m_fTotalLength` is the length of a single segment, and `CRope::m_nSegments` is the index of the last fixed segment

    // Rope already exists => just reset it
    if (const auto ropeIdx = FindRope(ropeID); ropeIdx != -1) {
        auto& rope = aRopes[ropeIdx];
        rope.m_aSegments[0] = startPos;
        rope.m_aSpeed[0]    = CVector{};
        rope.m_nFlags2 |= 1;
        rope.m_nSegments = segmentCount;
        for (size_t i = 0; i <= (size_t)rope.m_nSegments; i++) {
            rope.m_aSegments[i] = startPos;
            rope.m_aSpeed[i]    = CVector{};
        }
        rope.CreateHookObjectForRope();
        return true;
    }

    // Find a free slot
    const auto freeIt = rng::find(aRopes, eRopeType::NONE, &CRope::m_nType);
    if (freeIt == aRopes.end()) {
        return false;
    }
    auto& rope = *freeIt;

    rope.m_nId              = ropeID;
    rope.m_aSegments[0]     = startPos;
    rope.m_aSpeed[0]        = CVector{};
    rope.m_nSegments        = segmentCount;
    rope.m_nFlags2          = static_cast<uint8>(((flags & 1) << 2) | (rope.m_nFlags2 & 0xF9) | 1);
    rope.m_fGroundZ         = 0.f;
    rope.m_pAttachedEntity  = nullptr;
    rope.m_pRopeAttachObject = nullptr;
    rope.m_fSegmentLength   = (holder && holder->GetIsTypeVehicle()) ? std::bit_cast<float>(0x3f666666u) : 0.5f;
    rope.m_nFlags1          = 0;
    rope.m_pRopeHolder      = holder;
    rope.m_nType            = (eRopeType)ropeType;
    if (holder) {
        holder->RegisterReference(reinterpret_cast<CEntity**>(&rope.m_pRopeHolder));
    }
    rope.m_nTime = bExpires
        ? CTimer::GetTimeInMS() + timeExpire
        : 0;

    switch (rope.m_nType) {
    case eRopeType::MAGNET:
        rope.m_fMass        = std::bit_cast<float>(0x41200000u); // 10
        rope.m_fTotalLength = std::bit_cast<float>(0x3ea5294au);
        break;
    case eRopeType::CRANE_MAGNO:
        rope.m_fMass        = std::bit_cast<float>(0x42480000u); // 50
        rope.m_fTotalLength = std::bit_cast<float>(0x3fce739du);
        break;
    case eRopeType::WRECKING_BALL:
    case eRopeType::QUARRY_CRANE_ARM:
    case eRopeType::CRANE_TROLLEY:
        rope.m_fMass        = std::bit_cast<float>(0x42880000u); // 68
        rope.m_fTotalLength = std::bit_cast<float>(0x400c6319u);
        break;
    default:
        rope.m_fMass        = std::bit_cast<float>(0x41a00000u); // 20
        rope.m_fTotalLength = std::bit_cast<float>(0x3f25294au);
        break;
    }

    const auto segLen = rope.m_fTotalLength;
    switch (rope.m_nType) {
    case eRopeType::CRANE_MAGNO:
    case eRopeType::WRECKING_BALL:
    case eRopeType::QUARRY_CRANE_ARM:
    case eRopeType::CRANE_TROLLEY: {
        // Hang straight down
        // NOTE: Unlike in the other case, the speeds of the segments aren't reset here
        for (size_t i = 1; i < NUM_ROPE_SEGMENTS; i++) {
            const auto& prev = rope.m_aSegments[i - 1];
            rope.m_aSegments[i] = CVector{ prev.x, prev.y, prev.z - segLen };
        }
        break;
    }
    default: {
        // Zig-zag
        for (size_t i = 1; i < NUM_ROPE_SEGMENTS; i++) {
            const auto& prev = rope.m_aSegments[i - 1];
            rope.m_aSegments[i] = CVector{
                (i & 1) ? segLen + prev.x : prev.x - segLen,
                prev.y,
                prev.z
            };
            rope.m_aSpeed[i] = CVector{};
        }
        break;
    }
    }

    rope.CreateHookObjectForRope();
    return true;
}

// 0x556760
float CRopes::FindPickupHeight(CEntity* entity) {
    return CModelInfo::GetModelInfo(entity->m_nModelIndex)->GetColModel()->GetBoundingBox().m_vecMax.z;
}

// Returns id to array
// 0x556000
int32 CRopes::FindRope(uint32 id) {
    for (auto ropeId = 0; ropeId < MAX_NUM_ROPES; ropeId++) {
        if (aRopes[ropeId].m_nType != eRopeType::NONE && aRopes[ropeId].m_nId == id)
            return ropeId;
    }
    return -1;
}

// a4 always nullptr
// 0x555E40
bool CRopes::FindCoorsAlongRope(uint32 ropeId, float fDistAlongRope, CVector* outPosn, CVector* outSpeed) {
    const auto ropeIdx = FindRope(ropeId);
    if (ropeIdx == -1) {
        return false;
    }
    const auto& rope = aRopes[ropeIdx];

    // Clamp to [0, 0.999] (NaN is left as-is)
    if (fDistAlongRope < 0.f) {
        fDistAlongRope = 0.f;
    } else if (0.999f < fDistAlongRope) {
        fDistAlongRope = 0.999f;
    }

    const double scaledDist = (double)fDistAlongRope * 31.0;
    const auto   segIdx     = (size_t)(int32)scaledDist;
    const double frac       = scaledDist - (double)(int32)scaledDist;

    const auto& s0 = rope.m_aSegments[segIdx];
    const auto& s1 = rope.m_aSegments[segIdx + 1];
    const double inv = 1.0 - frac;
    outPosn->x = (float)((double)(float)(inv * s0.x) + frac * (double)s1.x);
    outPosn->y = (float)(inv * s0.y) + (float)(frac * (double)s1.y);
    outPosn->z = (float)(inv * s0.z) + (float)(frac * (double)s1.z);

    if (outSpeed) {
        *outSpeed = rope.m_aSpeed[segIdx + 1];
    }
    return true;
}

// 0x558D10
int32 CRopes::CreateRopeForSwatPed(const CVector& startPos) {
    int32 newRopeId = m_nRopeIdCreationCounter + 100;
    if (RegisterRope(newRopeId, static_cast<uint32>(eRopeType::SWAT), startPos, true, 0, 0, nullptr, 4000)) {
        return -1;
    }

    m_nRopeIdCreationCounter += 1;
    return newRopeId;
}

// 0x555F80
bool CRopes::IsCarriedByRope(CPhysical* entity) {
    if (!entity)
        return false;

    for (auto& rope : aRopes) {
        if (rope.m_nType != eRopeType::NONE && rope.m_pRopeAttachObject == entity)
            return true;
    }
    return false;
}

// 0x555DF0
void CRopes::SetSpeedOfTopNode(uint32 ropeId, CVector dirSpeed) {
    for (auto& rope : aRopes) {
        if (rope.m_nType != eRopeType::NONE && rope.m_nId == ropeId) {
            rope.m_aSpeed[0] = dirSpeed;
            return;
        }
    }
}
