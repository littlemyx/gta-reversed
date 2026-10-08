#include "StdInc.h"

#include "Rope.h"
#include "Ropes.h"
#include "Scripts/TheScripts.h"

#include <bit>
#include <cmath>

void CRope::InjectHooks() {
    RH_ScopedClass(CRope);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(ReleasePickedUpObject, 0x556030);
    RH_ScopedInstall(CreateHookObjectForRope, 0x556070);
    RH_ScopedInstall(UpdateWeightInRope, 0x5561B0);
    RH_ScopedInstall(Remove, 0x556780);
    RH_ScopedInstall(Render, 0x556800);
    RH_ScopedInstall(PickUpObject, 0x5569C0);
    RH_ScopedInstall(Update, 0x557530);
}

// inlined see 0x557959
// use switch like in Android?
// 0x555FB0
bool CRope::DoControlsApply() const {
    return    m_nType == eRopeType::CRANE_MAGNO && CRopes::PlayerControlsCrane == eControlledCrane::MAGNO_CRANE
           || m_nType == eRopeType::WRECKING_BALL && CRopes::PlayerControlsCrane == eControlledCrane::WRECKING_BALL
           || m_nType == eRopeType::CRANE_TROLLEY && CRopes::PlayerControlsCrane == eControlledCrane::LAS_VEGAS_CRANE
           || m_nType == eRopeType::QUARRY_CRANE_ARM && CRopes::PlayerControlsCrane == eControlledCrane::QUARRY_CRANE
           || m_nType == eRopeType::CRANE_MAGNET1
           || m_nType == eRopeType::MAGNET
           || m_nType == eRopeType::CRANE_HARNESS;
}

// 0x556030
void CRope::ReleasePickedUpObject() {
    if (m_pRopeAttachObject) {
        m_pRopeAttachObject->AsPhysical()->physicalFlags.bAttachedToEntity = false;
        m_pRopeAttachObject->AsPhysical()->physicalFlags.bCarriedByRope = false;
        m_pRopeAttachObject = nullptr;
    }
    m_pAttachedEntity->SetUsesCollision(true);
    m_nFlags1 = 60; // 6th, 7th bits set
}

// 0x556070
void CRope::CreateHookObjectForRope() {
    if (m_pAttachedEntity)
        return;

    using namespace ModelIndices;

    const auto modelIndex = [&]() -> ModelIndex {
        switch (m_nType) {
        case eRopeType::CRANE_MAGNET1:
        case eRopeType::CRANE_MAGNO:
        case eRopeType::QUARRY_CRANE_ARM:
        case eRopeType::CRANE_TROLLEY:
            return MI_CRANE_MAGNET;
        case eRopeType::CRANE_HARNESS:
            return MI_CRANE_HARNESS;
        case eRopeType::MAGNET:
            return MI_MINI_MAGNET;
        case eRopeType::WRECKING_BALL:
            return MI_WRECKING_BALL;
        case eRopeType::SWAT:
            return MODEL_INVALID; // Just so the assert below wont be hit.
        default:
            NOTSA_UNREACHABLE(); //assert(0);
        }
    }();
    if (modelIndex == ModelIndex{ MODEL_INVALID }) { // Must do it like this because `ModelIndex` is u16, `MODEL_ID` is i32, and u16 -1 casted to int32 is 0xffff...
        return;
    }

    auto* obj = new CObject(modelIndex, true);
    m_pAttachedEntity = obj;

    obj->RegisterReference(reinterpret_cast<CEntity**>(&m_pAttachedEntity));
    obj->SetPosn(m_aSegments[NUM_ROPE_SEGMENTS - 1]);
    obj->m_nObjectType = OBJECT_TYPE_DECORATION;
    obj->SetIsStatic(false);
    obj->physicalFlags.bAttachedToEntity = true;

    CWorld::Add(m_pAttachedEntity);

    m_pRopeAttachObject = nullptr;
    m_nFlags1 = 0;
}

// 0x5561B0
// NOTE: `m_fTotalLength` is actually the length of a single segment, and `m_nSegments` is the index of the last "fixed" segment (the one the rest of the rope hangs from)
bool CRope::UpdateWeightInRope(float x, float y, float z, int32 a5, float* outPos) {
    const auto anchorIdx = (size_t)m_nSegments;
    const auto anchor    = m_aSegments[anchorIdx];
    const auto segLen    = m_fTotalLength; // 0x30C
    constexpr auto LAST  = NUM_ROPE_SEGMENTS - 1; // 31

    m_aSegments[LAST] = CVector{ x, y, z };

    const float maxReach = (float)(LAST - anchorIdx) * segLen;
    const CVector delta  = CVector{ x, y, z } - anchor;
    const float dist     = std::sqrt(delta.x * delta.x + delta.z * delta.z + delta.y * delta.y);

    // Length of the vector from `b` to `a`
    const auto DistOf = [](const CVector& a, const CVector& b) {
        const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return std::sqrt(dx * dx + dz * dz + dy * dy);
    };

    if (!(dist < maxReach)) { // Target is out of reach => stretch the rope straight towards it
        const float scale = maxReach / dist;
        outPos[0] = delta.x * scale + anchor.x;
        outPos[1] = delta.y * scale + anchor.y;
        outPos[2] = delta.z * scale + anchor.z;

        CVector dir = delta;
        for (size_t i = anchorIdx + 1; i < NUM_ROPE_SEGMENTS; i++) {
            dir.Normalise();
            dir.x *= segLen;
            dir.y *= segLen;
            dir.z *= segLen;
            const float k = (float)(int32)(i - anchorIdx);
            m_aSegments[i] = CVector{
                dir.x * k + anchor.x,
                dir.y * k + anchor.y,
                dir.z * k + anchor.z
            };
        }
        return true;
    }

    // Rope has slack => relax the segment lengths a few times
    for (int32 iter = 0; iter < 6; iter++) {
        // Backwards: Pull previous segments towards the current one
        if (anchorIdx + 1 < LAST) {
            for (auto i = LAST; i > anchorIdx + 1; i--) {
                auto&       prev = m_aSegments[i - 1];
                const auto& cur  = m_aSegments[i];
                if (DistOf(cur, prev) <= segLen) {
                    break;
                }
                const CVector oldPrev = prev;
                const float   scale   = segLen / DistOf(cur, prev);
                prev = CVector{
                    (prev.x - cur.x) * scale + cur.x,
                    (prev.y - cur.y) * scale + cur.y,
                    (prev.z - cur.z) * scale + cur.z
                };
                const float invTimeStep = 1.f / CTimer::GetTimeStep();
                m_aSpeed[i - 1] = CVector{
                    (prev.x - oldPrev.x) * invTimeStep,
                    (prev.y - oldPrev.y) * invTimeStep,
                    (prev.z - oldPrev.z) * invTimeStep
                };
            }
        }

        // Forwards: Pull next segments towards the previous one
        if (anchorIdx + 1 < LAST) {
            for (auto i = anchorIdx + 1; i < LAST; i++) {
                const auto& prev = m_aSegments[i - 1];
                auto&       cur  = m_aSegments[i];
                const auto  d    = cur - prev;
                const float len  = std::sqrt(d.x * d.x + d.z * d.z + d.y * d.y);
                if (len > segLen) {
                    const float scale = segLen / len;
                    cur = CVector{
                        d.x * scale + prev.x,
                        d.y * scale + prev.y,
                        d.z * scale + prev.z
                    };
                }
            }
        }
    }

    // Final pass: Pull previous segments towards the current one, starting from the end
    if (anchorIdx + 1 < LAST) {
        for (auto i = LAST; i > anchorIdx + 1; i--) {
            auto&       prev = m_aSegments[i - 1];
            const auto& cur  = m_aSegments[i];
            const float len  = DistOf(cur, prev);
            if (len <= segLen) {
                break;
            }
            const float scale = segLen / len;
            prev = CVector{
                (prev.x - cur.x) * scale + cur.x,
                (prev.y - cur.y) * scale + cur.y,
                (prev.z - cur.z) * scale + cur.z
            };
        }
    }

    return false;
}

// 0x556780
void CRope::Remove() {
    m_nType = eRopeType::NONE;
    if (m_pRopeAttachObject)
        ReleasePickedUpObject();

    if (m_pAttachedEntity) {
        CWorld::Remove(m_pAttachedEntity);
        delete m_pAttachedEntity;
        m_pAttachedEntity = nullptr;
    }
}

// 0x556800
void CRope::Render() {
    // Note: Probably needs adjustments if `NUM_ROPE_SEGMENTS` is changed
    if (!TheCamera.IsSphereVisible(m_aSegments[NUM_ROPE_SEGMENTS / 2], 20.0f))
        return;

    if ((TheCamera.GetPosition() - m_aSegments[0]).Magnitude2D() >= 120.0f)
        return;

    DefinedState();

    const auto GetVertex = [](unsigned i) {
        return &TempBufferVertices.m_3d[i];
    };

    const RwRGBA color = { 0, 0, 0, 128 };
    for (auto i = 0u; i < NUM_ROPE_SEGMENTS; i++) {
        RxObjSpace3DVertexSetPreLitColor(GetVertex(i), &color);
        RxObjSpace3DVertexSetPos(GetVertex(i), &m_aSegments[i]);
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(FALSE));

    if (RwIm3DTransform(TempBufferVertices.m_3d, NUM_ROPE_SEGMENTS, nullptr, 0)) {
        RxVertexIndex indices[] = { // *(RxVertexIndex(*)[64])0x8CD818
            0,  1,  1,  2,  2,  3,  3,  4,
            4,  5,  5,  6,  6,  7,  7,  8,
            8,  9,  9,  10, 10, 11, 11, 12,
            12, 13, 13, 14, 14, 15, 15, 16,
            16, 17, 17, 18, 18, 19, 19, 20,
            20, 21, 21, 22, 22, 23, 23, 24,
            24, 25, 25, 26, 26, 27, 27, 28,
            28, 29, 29, 30, 30, 31, 31, 32
        };
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPELINELIST, indices, std::size(indices) - 2); // the last two indexes are not used
        RwIm3DEnd();
    }

    if (m_nType == eRopeType::QUARRY_CRANE_ARM) {
        const CVector pos[] = { m_aSegments[0], { 709.32f, 916.20f, 53.0f } }; // Hunter Quarry
        for (auto i = 0u; i < std::size(pos); i++) {
            RxObjSpace3DVertexSetPreLitColor(GetVertex(i), &color);
            RxObjSpace3DVertexSetPos(GetVertex(i), &pos[i]);
        }
        if (RwIm3DTransform(TempBufferVertices.m_3d, std::size(pos), nullptr, 0)) {
            RxVertexIndex indices[] = { 0, 1 };
            RwIm3DRenderIndexedPrimitive(rwPRIMTYPELINELIST, indices, std::size(indices));
            RwIm3DEnd();
        }
    }
}

// 0x5569C0
void CRope::PickUpObject(CEntity* obj) {
    if (m_pRopeAttachObject == obj)
        return;

    if (m_pRopeAttachObject)
        ReleasePickedUpObject();

    obj->RegisterReference(&m_pAttachedEntity);
    m_pRopeAttachObject = obj;

    // TODO: Move model => world space translation into CEntity
    // MultiplyMatrixWithVector should be used here
    CVector height = { {}, {}, CRopes::FindPickupHeight(obj) };
    m_pAttachedEntity->SetPosn(obj->GetPosition() + obj->GetMatrix().TransformVector(height));
    m_pAttachedEntity->SetUsesCollision(false);

    obj->AsPhysical()->physicalFlags.bAttachedToEntity = true;
    if (obj->GetIsTypeVehicle()) {
        if (obj->GetStatus() == STATUS_SIMPLE)
        {
            obj->SetStatus(STATUS_PHYSICS);
        }
    } else if (obj->GetIsTypeObject()) {
        if (obj->GetIsStatic()) {
            obj->AsObject()->SetIsStatic(false);
            obj->AsObject()->AddToMovingList();
            obj->AsObject()->m_nFakePhysics = 0;
        }
    }
}

namespace {
// Original globals used by `CRope::Update`
auto& kBaseBlend       = StaticRef<float>(0x8CD898); // 0.06: Base for the share of the speed change applied to the hanging entity
auto& kMassBlendFactor = StaticRef<float>(0x8CD89C); // 0.13: Multiplied by the mass of the entity to get the part that depends on it

// 0x420800
float RopeMax(float a, float b) {
    return a > b ? a : b; // NOTE: NaN => `b`
}
} // namespace

// 0x557530
void CRope::Update() {
    using namespace ModelIndices;

    // Scratch vector shared by (almost) the whole function in the original (a single stack slot, `local_128`).
    // It's used for: the old position of a segment, the output of `UpdateWeightInRope`, the (adjusted) speed of the hanging
    // entity, and the candidate position when looking for something to pick up.
    // BUG: In the original it is not initialized, and when picking up a ped it's read as if it were holding the candidate's position (see below).
    CVector scratch{};

    // Fraction of the speed that is kept each frame
    const float damping = static_cast<float>(std::pow(static_cast<double>(0.8f), static_cast<double>(CTimer::GetTimeStep())));

    // Everything below is done only for ropes near the camera (NOTE: `m_nFlags2` isn't cleared if we bail out here)
    {
        const auto& camPos = TheCamera.GetPosition();
        const float dy     = camPos.y - m_aSegments[0].y;
        const float dx     = camPos.x - m_aSegments[0].x;
        if (!(std::sqrt(dx * dx + dy * dy) < 200.f)) {
            return;
        }
    }

    // Executed once we're done (all code paths, except for the early return above)
    const auto Finish = [this] {
        if (!(m_nFlags2 & 1) && m_aSegments[0].z < -50.f) {
            Remove();
        }
        m_nFlags2 &= 0xFE;
    };

    // Expired rope: let the top segment fall
    if (!(m_nFlags2 & 1) && m_nTime < CTimer::GetTimeInMS()) {
        const float ts = CTimer::GetTimeStep();
        m_aSpeed[0].z = m_aSpeed[0].z - ts * 0.0015f;
        m_aSegments[0].x = ts * m_aSpeed[0].x + m_aSegments[0].x;
        m_aSegments[0].y = ts * m_aSpeed[0].y + m_aSegments[0].y;
        m_aSegments[0].z = ts * m_aSpeed[0].z + m_aSegments[0].z;
    }

    // Find the ground below the rope (not every frame)
    if ((m_nFlags2 & 4) && (CTimer::GetFrameCounter() & 7) == 2) {
        m_fGroundZ = CWorld::FindGroundZFor3DCoord(m_aSegments[0]);
    }

    // The ground of something that's being carried is 0.5 below the hook
    if ((m_nFlags2 & 4) && m_pRopeAttachObject) {
        const auto modelIdx = m_pRopeAttachObject->m_nModelIndex;
        if (m_pRopeAttachObject->GetIsTypeVehicle()
            || modelIdx == MI_OBJECTFORMAGNOCRANE1
            || modelIdx == MI_OBJECTFORMAGNOCRANE2
            || modelIdx == MI_OBJECTFORMAGNOCRANE3
            || modelIdx == MI_OBJECTFORMAGNOCRANE5
        ) {
            m_fGroundZ = m_pAttachedEntity->GetPosition().z - 0.5f;
        }
    }

    // Simulate the non-fixed segments
    if (const size_t firstFree = (size_t)m_nSegments + 1; firstFree < NUM_ROPE_SEGMENTS) {
        const float oneMinusDamping = 1.f - damping;
        for (size_t i = firstFree; i < NUM_ROPE_SEGMENTS; i++) {
            auto&       pos       = m_aSegments[i];
            auto&       speed     = m_aSpeed[i];
            const auto& prevPos   = m_aSegments[i - 1];
            const auto& prevSpeed = m_aSpeed[i - 1];

            scratch = pos; // Old position

            // Wind (NOTE: Not applied to Z)
            speed.x = (float)((rand() & 0xF) - 8) * 0.001f + speed.x;
            speed.y = (float)((rand() & 0xF) - 8) * 0.001f + speed.y;

            // Speed is a blend of the previous segment's one and own
            speed = CVector{
                oneMinusDamping * prevSpeed.x + damping * speed.x,
                oneMinusDamping * prevSpeed.y + damping * speed.y,
                oneMinusDamping * prevSpeed.z + damping * speed.z
            };

            // Gravity
            pos.z = pos.z - CTimer::GetTimeStep() * 0.15f;

            // Don't go below the ground
            if (m_nFlags2 & 4) {
                const float minZ = m_fGroundZ + 0.3f;
                if (!(pos.z > minZ)) {
                    pos.z = minZ;
                }
            }

            // Constrain the distance to the previous segment
            const float dz  = pos.z - prevPos.z;
            const float dy  = pos.y - prevPos.y;
            const float dx  = pos.x - prevPos.x;
            const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
            const float k   = m_fTotalLength / len; // NOTE: `m_fTotalLength` is really the length of a single segment
            pos = CVector{
                dx * k + prevPos.x,
                dy * k + prevPos.y,
                dz * k + prevPos.z
            };

            // New speed from the position change
            const float invTimeStep = 1.f / CTimer::GetTimeStep();
            speed = CVector{
                (pos.x - scratch.x) * invTimeStep,
                (pos.y - scratch.y) * invTimeStep,
                (pos.z - scratch.z) * invTimeStep
            };
        }
    }

    // Only for these types of ropes the code below is of any interest
    switch (m_nType) {
    case eRopeType::CRANE_MAGNET1:
    case eRopeType::CRANE_HARNESS:
    case eRopeType::MAGNET:
    case eRopeType::CRANE_MAGNO:
    case eRopeType::WRECKING_BALL:
    case eRopeType::QUARRY_CRANE_ARM:
    case eRopeType::CRANE_TROLLEY:
        break;
    default:
        return Finish();
    }

    // Player controls (the length of the rope)
    if (DoControlsApply()) {
        const auto IsAnalogControlled = m_nType == eRopeType::CRANE_MAGNET1
                                     || m_nType == eRopeType::CRANE_HARNESS
                                     || m_nType == eRopeType::MAGNET;
        if (IsAnalogControlled) {
            const int16 stick = CPad::GetPad(0)->GetCarGunUpDown();
            if ((stick < 0 && CTheScripts::bEnableCraneRaise) || (stick > 0 && CTheScripts::bEnableCraneLower)) {
                m_fSegmentLength = m_fSegmentLength - (float)stick * CTimer::GetTimeStep() * 0.00001f;
            }
            if (0.84f < m_fSegmentLength) {
                m_fSegmentLength = 0.84f;
            }
        } else { // Digital (the other types are always one of CRANE_MAGNO, WRECKING_BALL, QUARRY_CRANE_ARM and CRANE_TROLLEY here)
            if (CTheScripts::bEnableCraneRaise) {
                const float delta = (float)CPad::GetPad(0)->NewState.ButtonSquare * CTimer::GetTimeStep() * 0.00001f;
                if (delta > 0.f && delta + m_fSegmentLength < 0.9f) {
                    AudioEngine.ReportMissionAudioEvent(0x68, m_pRopeHolder->AsPhysical(), 0.f, 1.f);
                }
                m_fSegmentLength = delta + m_fSegmentLength;
            }
            if (CTheScripts::bEnableCraneLower) {
                const float delta = (float)CPad::GetPad(0)->NewState.ButtonCross * CTimer::GetTimeStep() * 0.00001f;
                if (delta > 0.f && m_fSegmentLength - delta > 0.01f) {
                    AudioEngine.ReportMissionAudioEvent(0x68, m_pRopeHolder->AsPhysical(), 0.f, 1.f);
                }
                m_fSegmentLength = m_fSegmentLength - delta;
            }
        }
    }
    if (m_fSegmentLength < 0.01f) {
        m_fSegmentLength = 0.01f;
    }
    if (0.9f < m_fSegmentLength) {
        m_fSegmentLength = 0.9f;
    }

    // Move what's at the end of the rope (the picked up entity if any, otherwise the hook)
    CEntity* hanging; // `ESI`
    float    hangingBlend; // How much of the speed difference is applied to the hanging entity (the rest goes to the holder)
    if (m_pRopeAttachObject) {
        hanging = m_pRopeAttachObject;

        float mass = hanging->AsPhysical()->m_fMass;
        if (hanging->m_nModelIndex == MODEL_SECURICA) {
            mass = 750.f;
        }
        hangingBlend = kMassBlendFactor * std::bit_cast<float>(0x3a5a740eu) * mass + kBaseBlend;
        if (hangingBlend > 0.5f) {
            hangingBlend = 0.5f;
        }
        hanging->AsPhysical()->m_nFakePhysics = 0;
    } else {
        // NOTE: Not null-checked in the original
        hanging      = m_pAttachedEntity;
        hangingBlend = kBaseBlend;
        hanging->SetUsesCollision(true);
    }

    if (hanging) {
        const auto& hangingPos = hanging->GetPosition();
        if (UpdateWeightInRope(hangingPos.x, hangingPos.y, hangingPos.z, 0x3DCCCCCD, &scratch.x)) {
            // The rope is taut => pull the hanging entity back
            hanging->SetPosn(scratch);

            auto* const hangingPhy = hanging->AsPhysical();
            auto* const holder     = m_pRopeHolder->AsPhysical(); // NOTE: Not null-checked in the original

            scratch = hangingPhy->m_vecMoveSpeed;

            CVector holderSpeed = holder->GetSpeed(m_aSegments[0] - holder->GetPosition());
            if (m_nType == eRopeType::CRANE_MAGNO || m_nType == eRopeType::WRECKING_BALL || m_nType == eRopeType::QUARRY_CRANE_ARM || m_nType == eRopeType::CRANE_TROLLEY) {
                holderSpeed = CVector{};
            }

            scratch.x = scratch.x - holderSpeed.x;
            scratch.y = scratch.y - holderSpeed.y;
            scratch.z = scratch.z - holderSpeed.z;

            // Direction from the rope's start to the hanging entity
            CVector dir = hanging->GetPosition() - m_aSegments[0];
            dir.Normalise();

            // Remove the speed component that's along the rope (away from the start)
            const float dot = dir.z * scratch.z + dir.x * scratch.x + dir.y * scratch.y;
            float       newSpeedX = scratch.x;
            if (dot > 0.f) {
                newSpeedX = scratch.x - dir.x * dot;
                scratch.y = scratch.y - dot * dir.y;
                scratch.z = scratch.z - dot * dir.z;
            }
            const CVector newSpeed{
                newSpeedX + holderSpeed.x,
                holderSpeed.y + scratch.y,
                holderSpeed.z + scratch.z
            };

            // The speed change is shared between the hanging entity and the holder
            const float dvz = newSpeed.z - hangingPhy->m_vecMoveSpeed.z;
            const float dvy = newSpeed.y - hangingPhy->m_vecMoveSpeed.y;
            const float dvx = newSpeed.x - hangingPhy->m_vecMoveSpeed.x;

            const float hangingShare = 1.f - hangingBlend;
            hangingPhy->m_vecMoveSpeed.x = dvx * hangingShare + hangingPhy->m_vecMoveSpeed.x;
            hangingPhy->m_vecMoveSpeed.y = dvy * hangingShare + hangingPhy->m_vecMoveSpeed.y;
            hangingPhy->m_vecMoveSpeed.z = dvz * hangingShare + hangingPhy->m_vecMoveSpeed.z;

            holder->m_vecMoveSpeed.x = holder->m_vecMoveSpeed.x - dvx * hangingBlend;
            holder->m_vecMoveSpeed.y = holder->m_vecMoveSpeed.y - dvy * hangingBlend;
            holder->m_vecMoveSpeed.z = holder->m_vecMoveSpeed.z - dvz * hangingBlend;

            // Orientation: the entity hangs from the rope
            if (!m_pRopeAttachObject) {
                hanging->GetMatrix().ForceUpVector(-dir);
            } else {
                // Smooth: 90% of what it was, 10% of the new one
                auto&       mat = hanging->GetMatrix();
                const CMatrix prev{ mat };
                mat.ForceUpVector(-dir);
                const CMatrix forced{ mat };
                mat.GetRight().x   = prev.GetRight().x   * 0.9f + forced.GetRight().x   * 0.1f;
                mat.GetRight().y   = prev.GetRight().y   * 0.9f + forced.GetRight().y   * 0.1f;
                mat.GetRight().z   = prev.GetRight().z   * 0.9f + forced.GetRight().z   * 0.1f;
                mat.GetForward().x = prev.GetForward().x * 0.9f + forced.GetForward().x * 0.1f;
                mat.GetForward().y = prev.GetForward().y * 0.9f + forced.GetForward().y * 0.1f;
                mat.GetForward().z = prev.GetForward().z * 0.9f + forced.GetForward().z * 0.1f;
                mat.GetUp().x      = prev.GetUp().x      * 0.9f + forced.GetUp().x      * 0.1f;
                mat.GetUp().y      = prev.GetUp().y      * 0.9f + forced.GetUp().y      * 0.1f;
                mat.GetUp().z      = prev.GetUp().z      * 0.9f + forced.GetUp().z      * 0.1f;
            }
        }

        // The hook follows the picked up entity
        if (auto* const picked = m_pRopeAttachObject) {
            m_pAttachedEntity->GetMatrix() = picked->GetMatrix();
            m_pAttachedEntity->AsPhysical()->m_vecMoveSpeed = picked->AsPhysical()->m_vecMoveSpeed;
            m_pAttachedEntity->SetPosn(picked->GetMatrix().TransformPoint(CVector{ 0.f, 0.f, CRopes::FindPickupHeight(picked) }));
        }
    }

    // End of the rope
    const CVector lastPos = m_aSegments[NUM_ROPE_SEGMENTS - 1];

    // Currently carrying something
    if (m_pRopeAttachObject) {
        // Release?
        if (DoControlsApply() && CTheScripts::bEnableCraneRelease) { // The condition is inlined in the original, see `DoControlsApply`
            if (CPad::GetPad(0)->CarGunJustDown() != 0) {
                ReleasePickedUpObject();
            }
        }

        // Something else took it (flag is named `bRenderScorched`, but isn't really that, ...)
        if (m_pRopeAttachObject && m_pRopeAttachObject->AsPhysical()->physicalFlags.bRenderScorched) {
            // Same as `ReleasePickedUpObject` (inlined in the original)
            m_pRopeAttachObject->AsPhysical()->physicalFlags.bAttachedToEntity = false;
            m_pRopeAttachObject->AsPhysical()->physicalFlags.bCarriedByRope    = false;
            m_pRopeAttachObject = nullptr;
            m_pAttachedEntity->SetUsesCollision(true);
            m_nFlags1 = 60;
        }

        // Drop bikes with a rider
        if (m_pRopeAttachObject && m_pRopeAttachObject->GetIsTypeVehicle()) {
            const auto* const veh = m_pRopeAttachObject->AsVehicle();
            if (veh->m_nVehicleType == VEHICLE_TYPE_BIKE && veh->m_pDriver) {
                ReleasePickedUpObject();
            }
        }
        return Finish();
    }

    // Can't pick up anything for a while after a drop
    if (m_nFlags1 != 0) {
        m_nFlags1--;
        return Finish();
    }

    // Reduce the length of the rope depending on how much the picked up thing pulls it down
    const auto OnPickedUp = [&](float dzRope) {
        if (dzRope > 0.f) {
            const float invMass = 1.f / m_fMass;
            m_fSegmentLength    = RopeMax((m_fSegmentLength - dzRope * invMass) - invMass * m_fTotalLength, 0.01f);
        }
    };

    // What to look for, depends on the type of the rope
    bool lookForMagnoCraneObjs = false; // CRANE_MAGNO
    bool lookForSiteCraneObjs  = false; // CRANE_TROLLEY
    bool lookForQuarryRocks    = false; // QUARRY_CRANE_ARM
    bool lookForWongDish       = false; // MAGNET
    bool lookForKmbRock        = false; // MAGNET
    bool lookForKmbPlank       = false; // MAGNET
    bool lookForKmbBomb        = false; // MAGNET
    bool onlyRcTiger           = false; // MAGNET: Of the vehicles only the RC Tiger is allowed
    bool searchVehicles        = true;

    switch (m_nType) {
    case eRopeType::CRANE_MAGNET1:
        break;
    case eRopeType::CRANE_HARNESS: { // Picks up peds
        auto* const pool = GetPedPool();
        for (auto i = pool->GetSize(); i-- > 0;) {
            auto* const ped = pool->GetAt(i);
            if (!ped || ped->m_nPedState == PEDSTATE_DEAD || ped->IsPlayer() || ped->bInVehicle) {
                continue;
            }
            const auto& pedPos = ped->GetPosition();
            const float dx     = pedPos.x - lastPos.x;
            const float dy     = pedPos.y - lastPos.y;
            const float dz     = pedPos.z - lastPos.z;
            if (!(std::sqrt(dx * dx + dy * dy + dz * dz) < 2.5f)) {
                continue;
            }

            m_pRopeAttachObject = ped;
            ped->RegisterReference(&m_pRopeAttachObject);
            ped->physicalFlags.bCarriedByRope = true;
            m_pAttachedEntity->SetPosn(ped->GetMatrix().TransformPoint(CVector{ 0.f, 0.f, CRopes::FindPickupHeight(ped) }));
            m_pAttachedEntity->SetUsesCollision(false);

            // BUG: `scratch.z` (a stale value) is used instead of the position of what's being picked up (copy-paste from the other cases)
            OnPickedUp(lastPos.z - scratch.z);
            break;
        }
        return Finish();
    }
    case eRopeType::MAGNET:
        lookForWongDish = lookForKmbRock = lookForKmbPlank = lookForKmbBomb = true;
        onlyRcTiger     = true;
        break;
    case eRopeType::CRANE_MAGNO:
        lookForMagnoCraneObjs = true;
        break;
    case eRopeType::QUARRY_CRANE_ARM:
        lookForQuarryRocks = true;
        break;
    case eRopeType::CRANE_TROLLEY:
        lookForSiteCraneObjs = true;
        searchVehicles       = false;
        break;
    default:
        return Finish();
    }

    if (searchVehicles) {
        auto* const pool = GetVehiclePool();
        for (auto i = pool->GetSize(); i-- > 0;) {
            auto* const veh = pool->GetAt(i);
            if (!veh) {
                continue;
            }

            const auto vehType = veh->m_nVehicleType;
            if (!(   vehType == VEHICLE_TYPE_AUTOMOBILE
                  || (vehType == VEHICLE_TYPE_BIKE && !veh->m_pDriver)
                  || vehType == VEHICLE_TYPE_MTRUCK
                  || veh->m_nModelIndex == MODEL_DINGHY
                  || veh->m_nModelIndex == MODEL_VORTEX
            )) {
                continue;
            }
            if (veh->physicalFlags.bRenderScorched) { // Same flag as above
                continue;
            }
            if (!veh->vehicleFlags.bWinchCanPickMeUp) { // NOTE: The comment on the flag says the opposite
                continue;
            }
            if (onlyRcTiger && veh->m_nModelIndex != MODEL_RCTIGER) {
                continue;
            }
            if (veh->m_ropeType != 0) { // Not picked up by a rope already
                continue;
            }

            const float height = CRopes::FindPickupHeight(veh);
            scratch            = veh->GetMatrix().TransformPoint(CVector{ 0.f, 0.f, height });

            const float dx = scratch.x - lastPos.x;
            const float dy = scratch.y - lastPos.y;
            const float dz = scratch.z - lastPos.z;
            if (!(std::sqrt(dx * dx + dy * dy + dz * dz) < 2.5f)) {
                continue;
            }

            m_pRopeAttachObject = veh;
            veh->RegisterReference(&m_pRopeAttachObject);
            veh->physicalFlags.bCarriedByRope = true;
            if (veh->GetStatus() == STATUS_SIMPLE) {
                veh->SetStatus(STATUS_PHYSICS);
            }

            m_pAttachedEntity->SetPosn(veh->GetMatrix().TransformPoint(CVector{ 0.f, 0.f, height }));
            m_pAttachedEntity->SetUsesCollision(false);

            OnPickedUp(lastPos.z - scratch.z);
            break;
        }

        // Not for the types that can only pick up vehicles
        if (!(lookForMagnoCraneObjs || lookForQuarryRocks || lookForWongDish || lookForKmbRock || lookForKmbPlank || lookForKmbBomb)) {
            return Finish();
        }
    }

    if (m_pRopeAttachObject) { // Picked up something above
        return Finish();
    }

    // Look for an object
    auto* const pool = GetObjectPool();
    for (auto i = pool->GetSize(); i-- > 0;) {
        auto* const obj = pool->GetAt(i);
        if (!obj || !obj->objectFlags.bCanBeAttachedToMagnet) {
            continue;
        }

        const auto modelIdx = obj->m_nModelIndex;
        const bool isPickable =
               (lookForMagnoCraneObjs && (   modelIdx == MI_OBJECTFORMAGNOCRANE1
                                          || modelIdx == MI_OBJECTFORMAGNOCRANE2
                                          || modelIdx == MI_OBJECTFORMAGNOCRANE3
                                          || modelIdx == MI_OBJECTFORMAGNOCRANE4
                                          || modelIdx == MI_OBJECTFORMAGNOCRANE5))
            || (lookForSiteCraneObjs && modelIdx == MI_OBJECTFORBUILDINGSITECRANE1)
            || (lookForQuarryRocks && (   modelIdx == MI_QUARY_ROCK1
                                       || modelIdx == MI_QUARY_ROCK2
                                       || modelIdx == MI_QUARY_ROCK3
                                       || modelIdx == MI_DEAD_TIED_COP) // The address (0x8CD714) is right, the name is probably not (a 4th rock?)
                                   && !obj->m_pAttachedTo)
            || (lookForWongDish && modelIdx == MI_WONG_DISH)
            || (lookForKmbRock  && modelIdx == MI_KMB_ROCK)
            || (lookForKmbPlank && modelIdx == MI_KMB_PLANK)
            || (lookForKmbBomb  && modelIdx == MI_KMB_BOMB);
        if (!isPickable) {
            continue;
        }

        // The point to grab it by: By default, the top of the object
        const float height = CRopes::FindPickupHeight(obj);
        auto&       mat    = obj->GetMatrix();
        scratch            = mat.TransformPoint(CVector{ 0.f, 0.f, height });
        int32 rotations    = 0; // How many times to rotate the object so the grabbed end faces up

        if (   modelIdx == MI_OBJECTFORMAGNOCRANE1
            || modelIdx == MI_OBJECTFORMAGNOCRANE2
            || modelIdx == MI_OBJECTFORMAGNOCRANE3
            || modelIdx == MI_OBJECTFORMAGNOCRANE5
        ) {
            // These can be grabbed by any end: use the highest one
            CVector candidate = mat.TransformPoint(CVector{ 0.f, 0.f, -CRopes::FindPickupHeight(obj) });
            if (scratch.z < candidate.z) {
                scratch   = candidate;
                rotations = 2;
            }

            candidate = mat.TransformPoint(CVector{ CRopes::FindPickupHeight(obj), 0.f, 0.f });
            if (scratch.z < candidate.z) {
                scratch   = candidate;
                rotations = 3;
            }

            candidate = mat.TransformPoint(CVector{ -CRopes::FindPickupHeight(obj), 0.f, 0.f });
            if (scratch.z < candidate.z) {
                scratch   = candidate;
                rotations = 1;
            }
        }

        const float dx = scratch.x - lastPos.x;
        const float dy = scratch.y - lastPos.y;
        const float dz = scratch.z - lastPos.z;
        if (!(std::sqrt(dx * dx + dy * dy + dz * dz) < 2.5f)) {
            continue;
        }

        m_pRopeAttachObject = obj;
        obj->RegisterReference(&m_pRopeAttachObject);
        obj->physicalFlags.bCarriedByRope = true;

        m_pAttachedEntity->SetPosn(mat.TransformPoint(CVector{ 0.f, 0.f, CRopes::FindPickupHeight(obj) }));
        m_pAttachedEntity->SetUsesCollision(false);

        if (obj->GetIsStatic()) { // `bIsStatic` || `bIsStaticWaitingForCollision`
            obj->SetIsStatic(false);
            obj->AddToMovingList();
        }
        obj->physicalFlags.bAttachedToEntity = true;

        OnPickedUp(lastPos.z - scratch.z);

        // Rotate around the forward axis (right => up => left => down)
        for (; rotations != 0; rotations--) {
            const CVector oldRight = mat.GetRight();
            mat.GetRight()         = mat.GetUp();
            mat.GetUp()            = -oldRight;
        }
        break;
    }

    return Finish();
}
