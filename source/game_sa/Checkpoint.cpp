#include "StdInc.h"

#include "Checkpoint.h"
#include "3dMarkers.h"

void CCheckpoint::InjectHooks() {
    RH_ScopedClass(CCheckpoint);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Render, 0x725C00);
}

// 0x725C00
void CCheckpoint::Render() {
    switch (m_Type) {
    case eCheckpointType::TUBE:
    case eCheckpointType::ENDTUBE:
    case eCheckpointType::EMPTYTUBE: {
        //> 0x725C1C - Make sure it's above the water
        float waterZ;
        const auto wasUnderwater = CWaterLevel::GetWaterLevelNoWaves(m_Pos, &waterZ) && waterZ >= m_Pos.z;
        if (wasUnderwater) {
            m_Pos.z = waterZ;
        }

        //> 0x725CB4 - Render flag for finish checkpoint
        if (m_Type == eCheckpointType::ENDTUBE) {
            RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(RwTextureGetRaster(gpFinishFlagTex)));
            RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE)); // 0x725CBF: (8, 0)
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
            RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
            RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,    RWRSTATE(rwTEXTUREADDRESSWRAP));

            CVector flagPos;
            CVector2D flagSz;
            if (CSprite::CalcScreenCoors(m_Pos + CVector{ 0.f, 0.f, 3.f }, &flagPos, &flagSz.x, &flagSz.y, false, true)) {
                CSprite::RenderOneXLUSprite(
                    flagPos,
                    flagSz * (m_Size / 2.f),
                    m_Colour.r,
                    m_Colour.g,
                    m_Colour.b,
                    255,
                    1.f / flagPos.z,
                    m_Colour.a,
                    0,
                    0
                );
            }

            RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(0));
            RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
            RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE)); // 0x725D9E: (0xC, 0)
        }

        //> 0x725DEE - Render the marker (Checkpoints are basically just markers with some extra stuff)
        C3dMarkers::PlaceMarker(
            m_ID,
            e3dMarkerType::MARKER3D_TUBE,
            m_Pos,
            m_Size * 1.5f,
            m_Colour.r,
            m_Colour.g,
            m_Colour.b,
            128,
            m_PulsePeriod,
            m_PulseFraction,
            1,
            { 0.f, 0.f, 0.f },
            !wasUnderwater // `!` needed because the variable is inverted
        );
        
        //> 0x725DF6 - Render direction arrow too
        if (m_Type == eCheckpointType::TUBE) { // There was a variable that was only set to true if m_Type != eCheckpointType::ENDTUBE, so this condition boils down to this....
            C3dMarkers::DirectionArrowSet(
                m_Pos,
                m_Size * 0.625f,
                255,
                64,
                64,
                255,
                -m_Fwd.x,
                -m_Fwd.y,
                -m_Fwd.z
            );
        }

        break;
    }
    case eCheckpointType::TORUS: { // 0x725E6D
        // 0x725E66: dist3D = sqrt((dz^2 + dy^2) + dx^2) (unrounded x87), `break` unless 2*size <= dist (ordered; NaN breaks)
        const auto pc = FindPlayerCoors();
        const x87d dx = (x87d)pc.x - m_Pos.x;
        const x87d dy = (x87d)pc.y - m_Pos.y;
        const x87d dz = (x87d)pc.z - m_Pos.z;
        const x87d dist = x87::sqrt((dz * dz + dy * dy) + dx * dx);
        if (!((x87d)(m_Size + m_Size) <= dist)) {
            break;
        }
        [[fallthrough]];
    }
    case eCheckpointType::TORUS_NOFADE:
    case eCheckpointType::TORUSROT:
    case eCheckpointType::TORUS_UPDOWN:
    case eCheckpointType::TORUS_DOWN: { // 0x725FE1
        C3dMarkers::PlaceMarker(
            m_ID,
            MARKER3D_TORUS,
            m_Pos,
            m_Size,
            m_Colour.r,
            m_Colour.g,
            m_Colour.b,
            m_Colour.a,
            m_PulsePeriod,
            m_PulseFraction,
            m_RotateRate,
            m_Fwd,
            false
        );
        break;
    }
    case eCheckpointType::TORUSTHROUGH: { // 0x725F05
        const auto PlaceTorusMarker = [this](size_t i) {
            // NOTE: Originally this would underflow... Not a desired behaviour.
            const auto a = (int32)m_Colour.a - (int32)(16 * i);
            if constexpr (notsa::IsFixBugs()) {
                if (a < 0) {
                    return;
                }
            }
            // 0x725F10: x = (i*fx)*multi + px unrounded; y/z: i*f is rounded to float first, z's product is rounded again
            const x87d fi = (x87d)(int32)i;
            const float fyi = (float)(fi * m_Fwd.y);
            const float fzi = (float)(fi * m_Fwd.z);
            const float zoff = (float)((x87d)fzi * m_MultiSize);
            CVector p{
                (float)((fi * m_Fwd.x) * m_MultiSize + m_Pos.x),
                (float)((x87d)fyi * m_MultiSize + m_Pos.y),
                (float)((x87d)zoff + m_Pos.z)
            };
            C3dMarkers::PlaceMarker(
                m_ID,
                MARKER3D_TORUS,
                p,
                m_Size,
                m_Colour.r,
                m_Colour.g,
                m_Colour.b,
                (uint8)a,
                m_PulsePeriod,
                m_PulseFraction,
                m_RotateRate,
                m_Fwd,
                false
            );
        };
        for (size_t i = 1; i < 4; i++) {
            PlaceTorusMarker(i);
        }
        PlaceTorusMarker(0);
        break;
    }
    case eCheckpointType::NA:
        break; // See `CCheckpoint::Update` - This type is used for something (Perhaps to make a checkpoint invisible? Not sure.)
    default:
        NOTSA_UNREACHABLE("Unknown checkpoint type ({})", m_Type.get_underlying());
    }
}

// Based on 0x7229C0
void CCheckpoint::Update() {
    // exe tests both bounds independently (no else)
    const auto UpdateRotFlag = [this](float maxSz, float minSz) {
        if (m_MultiSize > maxSz) {
            m_RotFlag = false;
        }
        if (m_MultiSize < minSz) {
            m_RotFlag = true;
        }
    };

    // exe 0x722AC7: (dy*dy + dx*dx) summed unrounded (x87)
    const auto SumSq2D = [this](const CVector& pc) -> x87d {
        const x87d dx = (x87d)pc.x - m_Pos.x;
        const x87d dy = (x87d)pc.y - m_Pos.y;
        return dy * dy + dx * dx;
    };

    if (m_Type == eCheckpointType::TORUSROT) { // 0x7229E7
        UpdateRotFlag(0.5f, -0.5f);
        m_MultiSize = m_RotFlag
            ? 0.1f
            : -0.1f;
        m_Fwd = (m_Fwd + CVector{m_MultiSize}).Normalized(); // 0x722A37
    }

    if (m_Type == eCheckpointType::TORUSTHROUGH) { // 0x722A53
        UpdateRotFlag(0.75f, 0.f);
        m_MultiSize += m_RotFlag // TODO/BUG: Use timestep here!
            ? 0.01f // exe 0x858C58 = 0.01f
            : -0.01f;
        if (m_MultiSize < 0.f) { // 0x722AA7
            m_Type = eCheckpointType::NA; // Weird?
        }
    }

    if (m_Type == eCheckpointType::TORUS_UPDOWN) { // 0x722ABD
        // 0x722B12: `fcompp` size^2 vs sum, the else arm is taken on C0 (size^2 < sum) or unordered => inside is `sum <= size^2`
        if (SumSq2D(FindPlayerCoors()) <= (x87d)m_Size * m_Size) { // 0x722B21
            m_Type      = eCheckpointType::TORUS_DOWN;
            m_MultiSize = -0.1f;
        } else { // 0x722B2E
            UpdateRotFlag(0.2f, -0.2f);
            m_MultiSize += m_RotFlag // TODO/BUG: Use timestep here!
                ? 0.02f
                : -0.02f;
            m_Pos.z += m_MultiSize;
        }
    }

    if (m_Type == eCheckpointType::TORUS_DOWN) { // 0x722B84
        const auto pc = FindPlayerCoors();
        if (SumSq2D(pc) > (x87d)m_Size * m_Size) { // 0x722BE1
            m_Type      = eCheckpointType::TORUS_UPDOWN;
            m_RotFlag   = true;
            m_MultiSize = 0.02f;
        }

        // 0x722BF7: pz - 0.75 < pos.z (ordered) => advance; otherwise (also NaN) the exe prints a debug string
        if ((x87d)pc.z - 0.75f < m_Pos.z) {
            m_Pos.z += m_MultiSize;
        } else {
            NOTSA_LOG_DEBUG("Done"); // OG
        }
    }
}

// Based on 0x722900
void CCheckpoint::SetPosition(const CVector& pos) {
    m_Pos.x = pos.x;
    m_Pos.y = pos.y;
    switch (m_Type) {
    case eCheckpointType::TORUS_UPDOWN: m_Pos.z = m_MultiSize + pos.z; break;
    case eCheckpointType::TORUS_DOWN:   break; // Ignore Z axis
    default:                            m_Pos.z = pos.z; break;
    }
}

// Based on 0x722FC0
void CCheckpoint::MarkAsDeleted() {
    m_IsUsed = false;
    m_Type   = eCheckpointType::NA;
    m_ID     = 0;
}

// Based on 0x722970
void CCheckpoint::SetHeading(float heading) {
    m_Fwd = CVector{
        (float)(x87::cos(DegreesToRadians(heading))),
        (float)(x87::sin(DegreesToRadians(heading))),
        m_Fwd.z
    }.Normalized();
}
