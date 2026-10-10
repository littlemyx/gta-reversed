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
            return MODEL_INVALID; // 0x556183: other rope types leave without creating a hook object
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
    //      The last writers before that are (in order): the old position of the last simulated segment (only if any were simulated),
    //      the output of `UpdateWeightInRope` (only when it returns true) and the adjusted speed of the hanging entity (same condition).
    //      Nothing in the ped search touches it, so here it holds exactly the same value (zero if none of the above ran, garbage in the original).
    CVector scratch{};

    // Fraction of the speed that is kept each frame
    const float damping = static_cast<float>(std::pow(static_cast<double>(0.8f), static_cast<double>(CTimer::GetTimeStep())));

    // Everything below is done only for ropes near the camera (NOTE: `m_nFlags2` isn't cleared if we bail out here)
    {
        // NOTE: x87 keeps the differences, their squares and the root in extended precision
        const auto&  camPos = TheCamera.GetPosition();
        const double dy     = (double)camPos.y - (double)m_aSegments[0].y;
        const double dx     = (double)camPos.x - (double)m_aSegments[0].x;
        if (!(std::sqrt(dx * dx + dy * dy) < 200.0)) {
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
        // NOTE: The products stay in extended precision until the single float store
        const double ts = CTimer::GetTimeStep();
        m_aSpeed[0].z    = (float)((double)m_aSpeed[0].z - ts * 0.0015f);
        m_aSegments[0].x = (float)(ts * m_aSpeed[0].x + m_aSegments[0].x);
        m_aSegments[0].y = (float)(ts * m_aSpeed[0].y + m_aSegments[0].y);
        m_aSegments[0].z = (float)(ts * m_aSpeed[0].z + m_aSegments[0].z);
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
            // NOTE: x87 keeps the product in extended precision before it's added and stored
            speed.x = (float)((double)((rand() & 0xF) - 8) * 0.001f + speed.x);
            speed.y = (float)((double)((rand() & 0xF) - 8) * 0.001f + speed.y);

            // Speed is a blend of the previous segment's one and own
            // NOTE: Only for Z the products aren't stored (rounded) before they are added
            speed = CVector{
                oneMinusDamping * prevSpeed.x + damping * speed.x,
                oneMinusDamping * prevSpeed.y + damping * speed.y,
                (float)((double)damping * speed.z + (double)oneMinusDamping * prevSpeed.z)
            };

            // Gravity
            pos.z = (float)((double)pos.z - (double)CTimer::GetTimeStep() * 0.15f);

            // Don't go below the ground
            if (m_nFlags2 & 4) {
                const double minZ = (double)m_fGroundZ + 0.3f; // NOTE: Not rounded to float for the comparison
                if (!((double)pos.z > minZ)) {
                    pos.z = (float)minZ;
                }
            }

            // Constrain the distance to the previous segment
            // NOTE: A mix of what's stored as a float (`dz`, `dx` for the multiplication, the squared length) and what isn't (`dy`, the others)
            const double dxE    = (double)pos.x - prevPos.x;
            const double dyE    = (double)pos.y - prevPos.y;
            const float  dz     = pos.z - prevPos.z;
            const float  dx     = (float)dxE;
            const float  lenSq  = (float)(dxE * dxE + dyE * dyE + (double)dz * dz);
            const double k      = (double)m_fTotalLength / std::sqrt((double)lenSq); // NOTE: `m_fTotalLength` is really the length of a single segment
            const float  newDx  = (float)((double)dx * k);
            const float  newDy  = (float)(dyE * k);
            const float  newDz  = (float)((double)dz * k);
            pos = CVector{
                newDx + prevPos.x,
                newDy + prevPos.y,
                newDz + prevPos.z
            };

            // New speed from the position change
            // NOTE: Only for Z the difference and the inverse (of the time step) aren't rounded
            const float invTimeStep = 1.f / CTimer::GetTimeStep();
            speed = CVector{
                (pos.x - scratch.x) * invTimeStep,
                (pos.y - scratch.y) * invTimeStep,
                (float)(((double)pos.z - scratch.z) * (1.0 / (double)CTimer::GetTimeStep()))
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
                // NOTE: The whole expression is evaluated in extended precision before the store
                m_fSegmentLength = (float)((double)m_fSegmentLength - (double)stick * CTimer::GetTimeStep() * 0.00001f);
            }
            if (0.84f < m_fSegmentLength) {
                m_fSegmentLength = 0.84f;
            }
        } else { // Digital (the other types are always one of CRANE_MAGNO, WRECKING_BALL, QUARRY_CRANE_ARM and CRANE_TROLLEY here)
            if (CTheScripts::bEnableCraneRaise) {
                // NOTE: Evaluated in extended precision, rounded to float only when stored. Same for the sums below
                const float delta = (float)((double)CPad::GetPad(0)->NewState.ButtonSquare * CTimer::GetTimeStep() * 0.00001f);
                if (delta > 0.f && (double)delta + m_fSegmentLength < 0.9f) {
                    AudioEngine.ReportMissionAudioEvent(0x68, m_pRopeHolder->AsPhysical(), 0.f, 1.f);
                }
                m_fSegmentLength = delta + m_fSegmentLength;
            }
            if (CTheScripts::bEnableCraneLower) {
                const float delta = (float)((double)CPad::GetPad(0)->NewState.ButtonCross * CTimer::GetTimeStep() * 0.00001f);
                if (delta > 0.f && (double)m_fSegmentLength - delta > 0.01f) {
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
        // NOTE: x87 keeps the whole expression and the comparison in extended precision
        double blend = (double)kMassBlendFactor * std::bit_cast<float>(0x3a5a740eu) * mass + kBaseBlend; // 0x863E30: 1/1200
        if (0.5 < blend) {
            blend = 0.5;
        }
        hangingBlend = (float)blend;
        hanging->AsPhysical()->m_nFakePhysics = 0;
    } else {
        hanging      = m_pAttachedEntity;
        hangingBlend = kBaseBlend;
        // BUG: Not null-checked in the original (the check below makes it clear it's expected to be possible)
        if (hanging || !notsa::IsFixBugs()) {
            hanging->SetUsesCollision(true);
        }
    }

    if (hanging) {
        const auto& hangingPos = hanging->GetPosition();
        if (UpdateWeightInRope(hangingPos.x, hangingPos.y, hangingPos.z, 0x3DCCCCCD, &scratch.x)) {
            // The rope is taut => pull the hanging entity back
            hanging->SetPosn(scratch);

            auto* const hangingPhy = hanging->AsPhysical();
            // BUG: Not null-checked in the original
            auto* const holder     = m_pRopeHolder->AsPhysical();

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
            // NOTE: x87 keeps `dot` (the comparison), the X component and the sums below in extended precision.
            //       Only `dot` (for the products), `scratch.y`, `scratch.z` and the differences of Y and Z are stored as floats.
            const double dotE = (double)dir.z * scratch.z + (double)dir.x * scratch.x + (double)dir.y * scratch.y;
            const float  dot  = (float)dotE;
            double       speedX = scratch.x;
            if (dotE > 0.0) {
                speedX    = (double)scratch.x - (double)dir.x * dot;
                scratch.y = (float)((double)scratch.y - (double)dot * dir.y);
                scratch.z = (float)((double)scratch.z - (double)dot * dir.z);
            }
            const double newSpeedX = speedX + holderSpeed.x;
            const double newSpeedY = (double)holderSpeed.y + scratch.y;
            const double newSpeedZ = (double)holderSpeed.z + scratch.z;

            // The speed change is shared between the hanging entity and the holder
            const float  dvz = (float)(newSpeedZ - hangingPhy->m_vecMoveSpeed.z);
            const float  dvy = (float)(newSpeedY - hangingPhy->m_vecMoveSpeed.y);
            const double dvx = newSpeedX - hangingPhy->m_vecMoveSpeed.x;

            const double hangingShare = 1.0 - hangingBlend;
            const float  hangingDvz   = (float)(dvz * hangingShare);
            const float  hangingDvy   = (float)(dvy * hangingShare);
            const float  hangingDvx   = (float)(dvx * hangingShare);
            hangingPhy->m_vecMoveSpeed.z = hangingDvz + hangingPhy->m_vecMoveSpeed.z;
            hangingPhy->m_vecMoveSpeed.y = hangingDvy + hangingPhy->m_vecMoveSpeed.y;
            hangingPhy->m_vecMoveSpeed.x = hangingDvx + hangingPhy->m_vecMoveSpeed.x;

            const float holderDvz = (float)(dvz * (double)hangingBlend);
            const float holderDvy = (float)(dvy * (double)hangingBlend);
            const float holderDvx = (float)(dvx * (double)hangingBlend);
            holder->m_vecMoveSpeed.z = holder->m_vecMoveSpeed.z - holderDvz;
            holder->m_vecMoveSpeed.y = holder->m_vecMoveSpeed.y - holderDvy;
            holder->m_vecMoveSpeed.x = holder->m_vecMoveSpeed.x - holderDvx;

            // Orientation: the entity hangs from the rope
            if (!m_pRopeAttachObject) {
                hanging->GetMatrix().ForceUpVector(-dir);
            } else {
                // Smooth: 90% of what it was, 10% of the new one
                auto&       mat = hanging->GetMatrix();
                const CMatrix prev{ mat };
                mat.ForceUpVector(-dir);
                const CMatrix forced{ mat };
                // NOTE: The products aren't rounded before they are added
                const auto Blend = [](float prevVal, float forcedVal) {
                    return (float)((double)prevVal * 0.9f + (double)forcedVal * 0.1f);
                };
                mat.GetRight().x   = Blend(prev.GetRight().x,   forced.GetRight().x);
                mat.GetRight().y   = Blend(prev.GetRight().y,   forced.GetRight().y);
                mat.GetRight().z   = Blend(prev.GetRight().z,   forced.GetRight().z);
                mat.GetForward().x = Blend(prev.GetForward().x, forced.GetForward().x);
                mat.GetForward().y = Blend(prev.GetForward().y, forced.GetForward().y);
                mat.GetForward().z = Blend(prev.GetForward().z, forced.GetForward().z);
                mat.GetUp().x      = Blend(prev.GetUp().x,      forced.GetUp().x);
                mat.GetUp().y      = Blend(prev.GetUp().y,      forced.GetUp().y);
                mat.GetUp().z      = Blend(prev.GetUp().z,      forced.GetUp().z);
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
    // NOTE: The whole expression is evaluated in extended precision (including `1 / mass`).
    //       The original calls `RopeMax` only for peds, for the rest it's inlined and compares the unrounded value,
    //       but the result is the same (the value is rounded to float on the way out in both cases).
    const auto OnPickedUp = [&](double dzRope) {
        if (dzRope > 0.0) {
            const double invMass = 1.0 / (double)m_fMass;
            m_fSegmentLength     = RopeMax((float)((m_fSegmentLength - dzRope * invMass) - invMass * m_fTotalLength), 0.01f);
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
            // NOTE: x87 keeps the differences, the squares and the root in extended precision (same everywhere below)
            const double dx    = (double)pedPos.x - lastPos.x;
            const double dy    = (double)pedPos.y - lastPos.y;
            const double dz    = (double)pedPos.z - lastPos.z;
            if (!(std::sqrt(dz * dz + dy * dy + dx * dx) < 2.5)) {
                continue;
            }

            m_pRopeAttachObject = ped;
            ped->RegisterReference(&m_pRopeAttachObject);
            ped->physicalFlags.bCarriedByRope = true;
            m_pAttachedEntity->SetPosn(ped->GetMatrix().TransformPoint(CVector{ 0.f, 0.f, CRopes::FindPickupHeight(ped) }));
            m_pAttachedEntity->SetUsesCollision(false);

            // BUG: `scratch.z` (a stale value: the last thing written to that stack slot before, see its declaration) is used
            //      instead of the position of what's being picked up (copy-paste from the other cases)
            OnPickedUp((double)lastPos.z - scratch.z);
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

            const double dx = (double)scratch.x - lastPos.x;
            const double dy = (double)scratch.y - lastPos.y;
            const double dz = (double)scratch.z - lastPos.z;
            if (!(std::sqrt(dx * dx + dz * dz + dy * dy) < 2.5)) {
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

            OnPickedUp((double)lastPos.z - scratch.z);
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
                                       || modelIdx == MI_DEAD_TIED_COP) // 0x8CD714, `dead_tied_cop` in the model names table
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

        const double dx = (double)scratch.x - lastPos.x;
        const double dy = (double)scratch.y - lastPos.y;
        const double dz = (double)scratch.z - lastPos.z;
        if (!(std::sqrt(dz * dz + dy * dy + dx * dx) < 2.5)) {
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

        OnPickedUp((double)lastPos.z - scratch.z);

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
