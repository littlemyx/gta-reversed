#include "StdInc.h"

#include "FallingGlassPane.h"
#include "Glass.h"

void CFallingGlassPane::InjectHooks() {
    RH_ScopedClass(CFallingGlassPane);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Update, 0x71AA10);
    RH_ScopedInstall(Render, 0x71B100);
}

// 0x71AA10
void CFallingGlassPane::Update() {
    if (CTimer::GetTimeInMS() < m_nCreatedTime)
        return;

    auto& pos = m_Matrix.GetPosition();

    // exe 0x71AA20-0x71AAA8: pos += (timeStep * velocity) [* 0.35f]; velocity.z -= timeStep * 0.01f (0.02f); the products stay unrounded, one float rounding per store
    const x87d ts = CTimer::GetTimeStep();
    if (m_f6F) {
        pos.x = (float)(((ts * m_Velocity.x) * 0.35f) + pos.x);
        pos.y = (float)(((ts * m_Velocity.y) * 0.35f) + pos.y);
        pos.z = (float)(((ts * m_Velocity.z) * 0.35f) + pos.z);
        m_Velocity.z = (float)((x87d)m_Velocity.z - ts * 0.01f);
    } else {
        pos.x = (float)(ts * m_Velocity.x + pos.x);
        pos.y = (float)(ts * m_Velocity.y + pos.y);
        pos.z = (float)(ts * m_Velocity.z + pos.z);
        m_Velocity.z = (float)((x87d)m_Velocity.z - ts * 0.02f);
    }

    m_Matrix.GetRight()   += CrossProduct(m_RandomNumbers, m_Matrix.GetRight());
    m_Matrix.GetForward() += CrossProduct(m_RandomNumbers, m_Matrix.GetForward());
    m_Matrix.GetUp()      += CrossProduct(m_RandomNumbers, m_Matrix.GetUp());

    if (pos.z < m_fGroundZ) {
        m_bExist = false;
        // exe 0x71AB4B: both the sound and the glass particles use (x, y, groundZ), not the position of the pane
        const CVector groundPos{ pos.x, pos.y, m_fGroundZ };
        AudioEngine.ReportGlassCollisionEvent(AE_GLASS_HIT_GROUND, groundPos);
        if (!m_f6F) {
            RwRGBA color{ 255, 255, 255, 32 };
            for (auto i = 0; i < 4; i++) {
                g_fx.AddGlass(groundPos, color, CGeneral::GetRandomNumberInRange(0.02f, 0.06f), 1);
            }
        }
    }
}

RwRGBA CFallingGlassPane::CalculateHiLightPolyColor() {
    // exe 0x71B16E-0x71B1F0: colour = _ftol2((delta * 0.002f) * alpha), the delta is a SIGNED clamp(time - created, 0, 500) (a pane that has not started yet is dark),
    // the low byte is the colour and a slowly falling (m_f6F) pane has at least 0x40
    const auto alpha = (uint32)CGlass::CalcAlphaWithNormal(Normalized(m_Matrix.GetForward()));
    const auto delta = std::clamp((int32)(CTimer::GetTimeInMS() - m_nCreatedTime), 0, 500);
    const auto colorI = NOTSA_FTOL(int32, ((x87d)delta * 0.002f) * (x87d)alpha);
    auto       color  = (uint8)colorI;
    if (m_f6F && color < 0x40) {
        color = 0x40;
    }
    return { color, color, color, color };
}

RwRGBA CFallingGlassPane::CalculateShatterPolyColor(float camDist) {
    // exe 0x71B454-0x71B49B: ftol((1.0 - (camDist - 30.0) * 0.1f) * 140.0), the low byte
    if (camDist > 30.f) {
        const auto alphaI = NOTSA_FTOL(int32, ((x87d)1.0 - ((x87d)camDist - 30.0f) * 0.1f) * 140.0f);
        const auto alpha  = (uint8)alphaI;
        return { alpha, alpha, alpha, alpha };
    }
    return { 140u, 140u, 140u, 140u};
}

// 0x71B100
void CFallingGlassPane::Render() {
    constexpr RwImVertexIndex indices[]{ 0u, 1u, 2u, 0u, 2u, 1u };

    // exe 0x71B100-0x71B160: the distance to the camera, sqrt((dz^2 + dy^2) + dx^2) unrounded
    const auto& camPos  = TheCamera.GetPosition();
    const auto& panePos = m_Matrix.GetPosition();
    const x87d cdx = (x87d)camPos.x - panePos.x, cdy = (x87d)camPos.y - panePos.y, cdz = (x87d)camPos.z - panePos.z;
    const float camDist = (float)std::sqrt((cdz * cdz + cdy * cdy) + cdx * cdx);

    // Render HiLightPoly's
    CVector vertices[3]{};
    {
        const auto color = CalculateHiLightPolyColor();

        // exe 0x71B1D0: flush when the index buffer (>= 0x7F9) or the vertex buffer (>= 0x3FC) is nearly full
        if (CGlass::HiLightPolyIndicesIdx >= 0x7F9 || CGlass::H1iLightPolyVerticesIdx >= 0x3FC) {
            CGlass::RenderHiLightPolys();
        }

        constexpr RwTexCoords uv[3] = {
            { 0.5f, 0.5f },
            { 0.5f, 0.6f },
            { 0.6f, 0.6f },
        };
        for (auto i = 0u; i < 3u; i++) {
            auto& vertex = TempBufferVertices.m_3d[CGlass::H1iLightPolyVerticesIdx + i];

            const auto pos2DOS = CGlass::PanePolyPositions[m_nPieceIndex][i] - CGlass::PanePolyCenterPositions[m_nPieceIndex];
            vertices[i] = m_Matrix.TransformPoint({ pos2DOS.x, 0.f, pos2DOS.y });
            RxObjSpace3DVertexSetPos(&vertex, &vertices[i]);

            RxObjSpace3DVertexSetU(&vertex, uv[i].u);
            RxObjSpace3DVertexSetV(&vertex, uv[i].v);

            RxObjSpace3DVertexSetPreLitColor(&vertex, &color);
        }

        // Store indices
        uint32 i = CGlass::HiLightPolyIndicesIdx;
        for (const auto idx : indices) {
            aTempBufferIndices[i++] = (RwImVertexIndex)(CGlass::H1iLightPolyVerticesIdx + idx);
        }

        CGlass::H1iLightPolyVerticesIdx += 3;
        CGlass::HiLightPolyIndicesIdx += 6;
    }

    if (m_bRenderShatter) {
        // exe 0x71B43F: flush when the index buffer (>= 0xBF9) or the vertex buffer (>= 0x5FC) is nearly full
        if (CGlass::ShatteredIndicesBaseIdx >= 0xBF9 || CGlass::ShatteredVerticesBaseIdx >= 0x5FC) {
            CGlass::RenderShatteredPolys();
        }

        const auto color = CalculateShatterPolyColor(camDist);
        for (auto i = 0u; i < 3u; i++) {
            auto vertex = &TempBufferVertices.m_3d[CGlass::ShatteredVerticesBaseIdx + i];

            RxObjSpace3DVertexSetPos(vertex, &vertices[i]);
            RxObjSpace3DVertexSetPreLitColor(vertex, &color);

            const auto uv = CGlass::PanePolyPositions[m_nPieceIndex][i] * m_fSize * 4.f;
            RxObjSpace3DVertexSetU(vertex, uv.x);
            RxObjSpace3DVertexSetV(vertex, uv.y);
        }

        // exe 0x71B53E-0x71B5F0: the indices are relative to the start of the shattered vertex buffer (1024), the cursors advance by 3 vertices / 6 indices
        uint32 i = CGlass::ShatteredIndicesBaseIdx;
        for (const auto idx : indices) {
            aTempBufferIndices[i++] = (RwImVertexIndex)(CGlass::ShatteredVerticesBaseIdx - 1024u + idx);
        }
        CGlass::ShatteredVerticesBaseIdx += 3;
        CGlass::ShatteredIndicesBaseIdx += 6;
    }
}
