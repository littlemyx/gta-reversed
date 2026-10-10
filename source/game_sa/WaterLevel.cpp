#include "StdInc.h"
#include "WaterLevel.h"
#include "PostEffects.h"
#include <sstream>
#include <numbers>

#define TRIANGLE_ARGS_OUT X1, Y1, P1, X2, Y2, P2, X3, Y3, P3

namespace {
// The originals are cdecl with `this` as the first stack argument => can't be hooked as members
bool CWaterQuad_GetWaterLevel(const CWaterQuad* self, float x, float y, float z, float* outWaterLevel, float* outBigWaves, float* outSmallWaves) {
    return self->GetWaterLevel(x, y, z, outWaterLevel, outBigWaves, outSmallWaves);
}
bool CWaterTriangle_GetWaterLevel(const CWaterTriangle* self, float x, float y, float z, float* outWaterLevel, float* outBigWaves, float* outSmallWaves) {
    return self->GetWaterLevel(x, y, z, outWaterLevel, outBigWaves, outSmallWaves);
}
}

void CWaterLevel::InjectHooks() {
    RH_ScopedClass(CWaterLevel);
    RH_ScopedCategoryGlobal();

    RH_ScopedGlobalInstall(WaterLevelInitialise, 0x6EAE80);
    RH_ScopedGlobalInstall(Shutdown, 0x6E59E0);
    RH_ScopedGlobalInstall(RenderWaterTriangle, 0x6EE240);
    RH_ScopedGlobalInstall(RenderFlatWaterTriangle_OneLayer, 0x6E8ED0);
    RH_ScopedGlobalInstall(RenderFlatWaterTriangle, 0x6EE080);
    RH_ScopedGlobalInstall(SplitWaterTriangleAlongXLine, 0x6ECF00);
    RH_ScopedGlobalInstall(SplitWaterTriangleAlongYLine, 0x6EE5A0);

    RH_ScopedGlobalInstall(RenderWaterRectangle, 0x6EC5D0);
    RH_ScopedGlobalInstall(RenderFlatWaterRectangle_OneLayer, 0x6E9940);
    RH_ScopedGlobalInstall(RenderFlatWaterRectangle, 0x6EBEC0);

    RH_ScopedGlobalInstall(SplitWaterRectangleAlongXLine, 0x6E73A0);
    RH_ScopedGlobalInstall(SplitWaterRectangleAlongYLine, 0x6ED6D0);

    RH_ScopedGlobalInstall(PreRenderWater, 0x6EB710);
    RH_ScopedGlobalInstall(MarkQuadsAndPolysToBeRendered, 0x6E5810);
    RH_ScopedGlobalInstall(ScanThroughBlocks, 0x6E6D10);
    RH_ScopedGlobalInstall(BlockHit, 0x6E6CA0);

    // This one doesn't seem to work properly for whatever reason
    // It works for some quads, not for others... But then it works for that one too if only it's loaded from the file (eg.: you delete all others)
    // no clue what is the issue
    // one quad that doesn't load can be seen from -1610, 168
    // it's at water.dat:252
    RH_ScopedGlobalInstall(AddWaterLevelQuad, 0x6E7EF0);
    RH_ScopedGlobalInstall(AddWaterLevelTriangle, 0x6E7D40);
    RH_ScopedGlobalInstall(AddWaterLevelVertex, 0x6E5A40);

    RH_ScopedGlobalInstall(RenderBoatWakes, 0x6ED9A0);
    RH_ScopedGlobalInstall(RenderWakeSegment, 0x6EA260);

    RH_ScopedOverloadedInstall(GetWaterLevel, "", 0x6EB690, bool(*)(float, float, float, float&, uint8, CVector*));
    RH_ScopedGlobalInstall(SetUpWaterFog, 0x6EA9F0);
    RH_ScopedGlobalInstall(FindNearestWaterAndItsFlow, 0x6E9D70);
    RH_ScopedGlobalInstall(GetWaterLevelNoWaves, 0x6E8580);
    RH_ScopedGlobalInstall(RenderWaterFog, 0x6E7760);
    RH_ScopedGlobalInstall(CalculateWavesOnlyForCoordinate, 0x6E6EF0);
    RH_ScopedGlobalInstall(RenderWater, 0x6EF650);
    RH_ScopedGlobalInstall(RenderSeaBedSegment, 0x6E6870);
    RH_ScopedGlobalInstall(RenderDetailedSeaBedSegment, 0x6E6A10);
    RH_ScopedGlobalInstall(AddWaveToResult, 0x6E81E0);
    RH_ScopedGlobalInstall(SetCameraRange, 0x6E9C80);
    RH_ScopedGlobalInstall(CalculateWavesOnlyForCoordinate2, 0x6E7210);
    RH_ScopedGlobalInstall(FillQuadsAndTrianglesList, 0x6E7B30);
    RH_ScopedGlobalInstall(AddPolyToBlock, 0x6E5750);
    RH_ScopedGlobalInstall(RenderHighDetailWaterRectangle, 0x6EB810);
    RH_ScopedGlobalInstall(RenderHighDetailWaterRectangle_OneLayer, 0x6E91D0);
    RH_ScopedGlobalInstall(GetWaterDepth, 0x6EA960);
    RH_ScopedGlobalInstall(GetGroundLevel, 0x6EA8A0);
    RH_ScopedGlobalInstall(RenderHighDetailWaterTriangle_OneLayer, 0x6E8780);
    RH_ScopedGlobalInstall(TestLineAgainstWater, 0x6E61B0);
    RH_ScopedGlobalInstall(RenderHighDetailWaterTriangle, 0x6EDDC0);
    RH_ScopedNamedGlobalInstall(CWaterQuad_GetWaterLevel, "CWaterQuad::GetWaterLevel", 0x6E5BB0);
    RH_ScopedNamedGlobalInstall(CWaterTriangle_GetWaterLevel, "CWaterTriangle::GetWaterLevel", 0x6E5E90);
}

// NOTSA
bool CWaterLevel::LoadDataFile() {
    const auto file = CFileMgr::OpenFile(m_nWaterConfiguration == 1 ? "DATA//water1.dat" : "DATA//water.dat", "r");

    const notsa::ScopeGuard autoCloser{ [&] { CFileMgr::CloseFile(file); } };

    uint32 nline{}, ntri{}, nquad{};
    for (;; nline++) {
        const auto line = CFileLoader::LoadLine(file);
        if (!line) {
            break;
        }
        std::stringstream liness{ line };

        auto nvertices{0u};

        struct {
            CVector   pos{};
            CVector2D flow{};
            float     bigWaves{}, smallWaves{};
        } vertices[4]{};

        // Helper function to read a vertex from the stream
        const auto ReadNextVertex = [&]() {
            const auto orgpos = liness.tellg();

            auto& vtx = vertices[nvertices];
            liness
                >> vtx.pos.x
                >> vtx.pos.y
                >> vtx.pos.z
                >> vtx.flow.x
                >> vtx.flow.y
                >> vtx.bigWaves
                >> vtx.smallWaves;

            if (liness.good()) {
                nvertices++;
                return true;
            } else {
                liness.clear(); // reset error flags
                liness.seekg(orgpos); // go back to before
                return false;
            }

        };

        // If can't read first vertex just ignore line
        if (!ReadNextVertex()) {
            continue;
        }

        // Read 2/3 more vertices
        while (ReadNextVertex() && nvertices < 4);

        // Check if we have enough vertices
        if (nvertices < 3) {
            NOTSA_LOG_DEBUG("[Warning]: Not enough vertices, got {}, expected 3 or 4. [Line: {}]", nvertices, nline);
            continue;
            //return false; // Just stop here, this parser is way too primitive to be able to recover from errors
        }

        // Optional flag after vertices
        uint32 flags{};
        liness >> flags;

        // I'm sorry, but don't blame me I HAD NO OTHER CHOICE!
        #define VertexUnpack(n) \
            (int32)vertices[n].pos.x, (int32)vertices[n].pos.y, \
            CRenPar{vertices[n].pos.z, vertices[n].bigWaves, vertices[n].smallWaves, (int8)(vertices[n].flow.x * 64.f), (int8)(vertices[n].flow.y * 64.f)}

        // Add quad/triangle
        if (nvertices == 4) {
            CWaterLevel::AddWaterLevelQuad(
                VertexUnpack(0),
                VertexUnpack(1),
                VertexUnpack(2),
                VertexUnpack(3),
                flags
            );
            nquad++;
        } else {
            CWaterLevel::AddWaterLevelTriangle(
                VertexUnpack(0),
                VertexUnpack(1),
                VertexUnpack(2),
                flags
            );
            ntri++;
        }
        #undef ArgUnpack
    }
    NOTSA_LOG_DEBUG("Successfully loaded! [Quads: {}; Tris: {}]", nquad, ntri);
    return true;
}

// NOTSA: Code @ 0x6EB5F4
void CWaterLevel::LoadTextures() {
    CTxdStore::PushCurrentTxd();    
    CTxdStore::SetCurrentTxd(CTxdStore::FindTxdSlot("particle"));

    const auto DoTex = [](auto& inOutTex, auto& outRaster, const char* name) {
        if (!inOutTex) {
            inOutTex = RwTextureRead(name, nullptr);
        }
        outRaster = RwTextureGetRaster(inOutTex);
    };
    DoTex(texWaterclear256, waterclear256Raster, "waterclear256");
    DoTex(texSeabd32,       seabd32Raster,       "seabd32"      );
    DoTex(texWaterwake,     waterwakeRaster,     "waterwake"    );

    CTxdStore::PopCurrentTxd();
}

// 0x6EAE80
void CWaterLevel::WaterLevelInitialise() {
    NumWaterTriangles = 0;
    NumWaterQuads = 0;
    NumWaterVertices = 0;
    NumWaterZonePolys = 0;
    
    (void)LoadDataFile();
    FillQuadsAndTrianglesList();

    LoadTextures();
}

// 0x6E59E0
void CWaterLevel::Shutdown() {
    // Unload Textures
    for (auto tex : { &texWaterclear256, &texSeabd32, &texWaterwake }) {
        if (*tex) {
            RwTextureDestroy(*tex);
            *tex = nullptr;
        }
    }
}

// 0x6E81E0
void CWaterLevel::AddWaveToResult(float x, float y, float* pfWaterLevel, float fUnkn1, float fUnkn2, CVector* pVecNormal) {
    float h0 = 0.f, h1 = 0.f, h2 = 0.f; // Wave heights at the 3 corners of the triangle the point is in

    const float scaledX = x * 0.5f;
    const float scaledY = y * 0.5f;
    const float fracX   = scaledX - (float)std::floor(scaledX);
    const float fracY   = scaledY - (float)std::floor(scaledY);
    const int32 ix      = (int32)((float)std::floor(scaledX) * 2.0f);
    const int32 iy      = (int32)((float)std::floor(scaledY) * 2.0f);

    if (fracY + fracX < 1.0f) { // Lower triangle
        CalculateWavesOnlyForCoordinate2(ix,     iy,     fUnkn1, fUnkn2, &h0);
        CalculateWavesOnlyForCoordinate2(ix + 2, iy,     fUnkn1, fUnkn2, &h1);
        CalculateWavesOnlyForCoordinate2(ix,     iy + 2, fUnkn1, fUnkn2, &h2);

        const float dz1 = h2 - h0;
        const float dz2 = h1 - h0;
        if (!pVecNormal) {
            *pfWaterLevel = (dz1 * fracY + dz2 * fracX + *pfWaterLevel) + h0;
            return;
        }

        *pfWaterLevel = (dz2 * fracX + dz1 * fracY + *pfWaterLevel) + h0;

        CVector a{ 0.f, 2.f, dz1 }, b{ 2.f, 0.f, dz2 };
        *pVecNormal = CrossProduct(b, a);
        pVecNormal->Normalise();
    } else { // Upper triangle
        CalculateWavesOnlyForCoordinate2(ix + 2, iy + 2, fUnkn1, fUnkn2, &h0);
        CalculateWavesOnlyForCoordinate2(ix,     iy + 2, fUnkn1, fUnkn2, &h1);
        CalculateWavesOnlyForCoordinate2(ix + 2, iy,     fUnkn1, fUnkn2, &h2);

        const float dz2 = h2 - h0;
        const float dz1 = h1 - h0;
        if (!pVecNormal) {
            *pfWaterLevel = (dz2 * (1.0f - fracY) + dz1 * (1.0f - fracX) + *pfWaterLevel) + h0;
            return;
        }

        *pfWaterLevel = (dz2 * (1.0f - fracY) + dz1 * (1.0f - fracX) + *pfWaterLevel) + h0;

        CVector p{ 0.f, -2.f, dz2 }, q{ -2.f, 0.f, dz1 };
        *pVecNormal = CrossProduct(q, p);
        pVecNormal->Normalise();
    }
}

// 0x6EE240
void CWaterLevel::RenderWaterTriangle(int32 X1, int32 Y1, CRenPar P1, int32 X2, int32 Y2, CRenPar P2, int32 X3, int32 Y3, CRenPar P3) {
    const auto [minX, maxX] = std::make_pair(X1, X2); // Assumes: Starting in top left vertex with clockwise order
    const auto [minY, maxY] = std::minmax(Y1, Y3);
    if (minX >= CameraRangeMaxX || maxX <= CameraRangeMinX || minY >= CameraRangeMaxY || maxY <= CameraRangeMinY) { // Lies outside (of camera) fully
        RenderFlatWaterTriangle(TRIANGLE_ARGS_OUT);
    } else if (minX < CameraRangeMinX || maxX > CameraRangeMaxX) { // Lies inside on X 
        SplitWaterTriangleAlongXLine(minX < CameraRangeMinX ? CameraRangeMinX : CameraRangeMaxX, TRIANGLE_ARGS_OUT);
    } else if (minY < CameraRangeMinY || maxY > CameraRangeMaxY) { // Lies inside of Y
        SplitWaterTriangleAlongYLine(minY < CameraRangeMinY ? CameraRangeMinY : CameraRangeMaxY, TRIANGLE_ARGS_OUT);
    } else { // Lies inside of camera fully
        RenderHighDetailWaterTriangle(TRIANGLE_ARGS_OUT);
    }
}

// NOTSA
auto CWaterLevel::GetWaterLayerTexInfo(int32 layer) -> WaterLayerTexInfo {
    switch (layer) {
    case 0: return { { TextureShiftFirstU,  TextureShiftFirstV  }, 25.0f };
    case 1: return { { TextureShiftSecondU, TextureShiftSecondV }, 12.5f };
    default: NOTSA_UNREACHABLE();
    }
}

// NOTSA
CRGBA CWaterLevel::GetWaterColorForRendering(CRGBA real, DebugWaterColor debug, int32 WaterLayer) {
    if (debug.active) {
        return debug.color;
    } else {
        real *= 0.577f; // AKA 1/sqrt3 OR E_CONST OR neither, but just a coincidence?
        real.a = WaterLayerAlpha[WaterLayer];
        return real;
    }
}

// notsa
auto CWaterLevel::GetTextureUV(int32 X1, int32 Y1, int32 Y3, int32 WaterLayer) -> TexUV {
    const auto txinfo = GetWaterLayerTexInfo(WaterLayer);
    const auto posUV  = CVector2D{ (float)X1, (float)Y1 } / txinfo.size + txinfo.shift;

    const auto CalcShift = [](float p, bool dir) {
        return p - std::floor(p) + (dir ? 7.f : -7.f);
    };

    return {
        .size      = txinfo.size,
        .pos       = posUV,
        .baseShift = CVector2D{
            CalcShift(posUV.x, false),
            CalcShift(posUV.y, Y3 - Y1 <= 0)
        }
    };
}

// 0x6E8ED0
void CWaterLevel::RenderFlatWaterTriangle_OneLayer(int32 X1, int32 Y1, CRenPar P1, int32 X2, int32 Y2, CRenPar P2, int32 X3, int32 Y3, CRenPar P3, int32 WaterLayer) {
    RenderBuffer::RenderIfDoesntFit(3, 3);

    // First(!) push indices
    RenderBuffer::PushIndices({ 0, 1, 2 }, true);

    // And push vertices into the buffer
    const auto PushVertex = [
        &,
        tex       = GetTextureUV(X1, Y1, Y3, WaterLayer),
        pos2DVtx1 = CVector2D{ (float)X1, (float)Y2 },
        color     = GetWaterColorForRendering(WaterColorTriangle, DebugWaterColors[DebugWaterColor::TRI], WaterLayer)
    ](int32 x, int32 y, CRenPar p) {
        const auto pos2DThis = CVector2D{ (float)x, (float)y };
        RenderBuffer::PushVertex(
            CVector{ pos2DThis, p.z },
            (pos2DThis - pos2DVtx1) / tex.size + tex.baseShift,
            color
        );
    };

    PushVertex(X1, Y1, P1);
    PushVertex(X2, Y2, P2);
    PushVertex(X3, Y3, P3);
}

// 0x6EE080
void CWaterLevel::RenderFlatWaterTriangle(int32 X1, int32 Y1, CRenPar P1, int32 X2, int32 Y2, CRenPar P2, int32 X3, int32 Y3, CRenPar P3) {
    if (bSplitBigPolys && X2 - X1 > BigPolySize) {
        SplitWaterTriangleAlongXLine((X1 + X2) / 2, X1, Y1, P1, X2, Y2, P2, X3, Y3, P3);
    } else {
        RenderFlatWaterTriangle_OneLayer(X1, Y1, P1, X2, Y2, P2, X3, Y3, P3, 0);
        RenderFlatWaterTriangle_OneLayer(X1, Y1, P1, X2, Y2, P2, X3, Y3, P3, 1);
    }
}

// 0x6EA260
void CWaterLevel::RenderWakeSegment(
    const CVector2D& vecA, const CVector2D& vecB,
    const CVector2D& vecC, const CVector2D& vecD,
    const float& widthA, const float& widthB,
    const float& alphaA, const float& alphaB,
    const float& wakeZ
) {
    constexpr auto  NUM_PARTS = 4;
    constexpr float ALPHA_MULTS[]{ 0.4f, 1.f, 0.2f, 1.f, 0.4f }; // 0x8D390C

    const auto angle      = (float)(CTimer::GetTimeInMS() % 4096) / (4096.f / (2.f * PI));
    const auto windRadius = CWeather::WindClipped * 0.4f + 0.2f;

    for (auto partIdx = 0; partIdx < NUM_PARTS; partIdx++) {
        RenderBuffer::RenderIfDoesntFit(6, 4);

        RenderBuffer::PushIndices({ 0, 2, 1, 0, 3, 2 }, true);

        const CVector2D corners[]{
            lerpBlend(vecB, vecA, (float)(partIdx + 0) / (float)(NUM_PARTS)),
            lerpBlend(vecB, vecA, (float)(partIdx + 1) / (float)(NUM_PARTS)),
            lerpBlend(vecC, vecD, (float)(partIdx + 1) / (float)(NUM_PARTS)),
            lerpBlend(vecC, vecD, (float)(partIdx + 0) / (float)(NUM_PARTS)),
        };

        CVector2D uvs[4]{};
        rng::transform(corners, uvs, [](const CVector2D& pos) -> CVector2D {
            return { pos.x / (float)(NUM_PARTS), pos.y / (float)(NUM_PARTS) };
        });
        rng::transform(uvs, uvs, // Isn't it beautiful?
            [
                minUV = CVector2D{
                    std::floor(rng::min(uvs, {}, &CVector2D::x).x),
                    std::floor(rng::min(uvs, {}, &CVector2D::y).y)
                }
            ](auto& uv) {
                return uv - minUV;
            }
        );

        const float alphas[]{
            alphaA * ALPHA_MULTS[partIdx + 0],
            alphaA * ALPHA_MULTS[partIdx + 1],
            alphaB * ALPHA_MULTS[partIdx + 1],
            alphaB * ALPHA_MULTS[partIdx + 0],
        };

        for (auto i = 0; i < 4; i++) {
            const auto CalcAngleOfPos = [&](float p) {
                p += 3072.f; // TODO: Magic number, but I think it's meaningless (as the integer part is discarded below)
                p /= 32.f;   // TODO: Magic number (maybe meaningful this time) 
                return p - std::floor(p); // Extract fractional part
            };
            const float  z   = wakeZ + x87::sin((CalcAngleOfPos(corners[i].x) + CalcAngleOfPos(corners[i].y)) * PI * 2.f + angle) * windRadius;
            const auto& rgb = WakeSegmentPartColors[i];
            RenderBuffer::PushVertex(
                CVector{ corners[i], z },
                uvs[i],
                { (uint8)(rgb.r * 255.f), (uint8)(rgb.g * 255.f), (uint8)(rgb.b * 255.f), (uint8)(alphas[i]) }
            );
        }
    }
}

// 0x6ED9A0
void CWaterLevel::RenderBoatWakes() {
    CBoat::RenderAllWakePointBoats();
}

// 0x6ECF00
void CWaterLevel::SplitWaterTriangleAlongXLine(int32 splitAtX, int32 X1, int32 Y1, CRenPar P1, int32 X2, int32 Y2, CRenPar P2, int32 X3, int32 Y3, CRenPar P3) {
    assert(Y1 == Y2); 

    // Ease of life
    const auto XS = splitAtX;

    const auto splitWidth = XS - X1;
    const auto triWidth   = X2 - X1;

    // Calculate position of split along Y axis
    const auto CalcSplitPosY = [&](int32 fromY, int32 toY) {
        return fromY + (toY - fromY) * splitWidth / triWidth;
    };

    // Interpolation value
    const auto t = (float)splitWidth / (float)triWidth;

    // New interpolations of RenPar's along a few segments
    const auto P12 = lerpBlend(P1, P2, t);
    const auto P13 = lerpBlend(P1, P3, t);
    const auto P23 = lerpBlend(P2, P3, t);

    // Vertex 1 and 2 are always (top left), (top right)
    // Also the triangles always contain a 90deg corner at either the left or right side.

    if (X1 == X3) { // Vertex 3 => (bottom left)
        const auto YS = CalcSplitPosY(Y3, Y1);

        // Bottom
        RenderWaterTriangle(
            X1, YS, P12,
            XS, YS, P13,
            X3, Y3, P3
        );

        // Left
        RenderWaterRectangle(
            X1, XS,
            Y1, YS,
            P1, P12, P23, P13
        );

        // Right
        RenderWaterTriangle(
            XS, Y1, P12,
            X2, Y1, P2,
            XS, YS, P23
        );
    } else if (X2 == X3) { // Vertex 3 => (bottom right)
        const auto YS = CalcSplitPosY(Y1, Y3);

        // Left
        RenderWaterTriangle(
            X1, Y1, P1,
            XS, Y1, P12,
            XS, YS, P13
        );

        // Right
        RenderWaterRectangle(
            XS, X2,
            Y1, YS,
            P12, P2, P23, P12
        );

        // Bottom
        RenderWaterTriangle(
            XS, YS, P13,
            X2, YS, P23,
            X3, Y3, P3
        );
    } else {
        NOTSA_UNREACHABLE("Triangle has no 90deg corner => Very bad");
    }
}

// 0x6EE5A0
void CWaterLevel::SplitWaterTriangleAlongYLine(int32 splitAtY, int32 X1, int32 Y1, CRenPar P1, int32 X2, int32 Y2, CRenPar P2, int32 X3, int32 Y3, CRenPar P3) {
    if (DontRenderYSplitTri) { // NOTSA
        return;
    }

    const auto [minY, maxY] = std::minmax(Y1, Y3);
    const auto height = maxY - minY;
    const auto width  = X2 - X1;
    
    // Calulcate the X position where the Y line intersects the hypot
    // and using that we split the triangle. 
    // Same result as original code, but much easier.

    SplitWaterTriangleAlongXLine(
        X1 + (maxY - splitAtY) * width / height,
        X1, Y1, P1,
        X2, Y2, P2,
        X3, Y3, P3
    );
}

// 0x6EC5D0
void CWaterLevel::RenderWaterRectangle(int32 minX, int32 maxX, int32 Y1, int32 Y2, CRenPar P1, CRenPar P2, CRenPar P3, CRenPar P4) {
    const auto [minY, maxY] = std::minmax(Y1, Y2);
    if (minX >= CameraRangeMaxX || maxX <= CameraRangeMinX || minY >= CameraRangeMaxY || maxY <= CameraRangeMinY) { // Lies outside (of camera) fully
        RenderFlatWaterRectangle(minX, maxX, Y1, Y2, P1, P2, P3, P4);
    } else if (minX < CameraRangeMinX || maxX > CameraRangeMaxX) { // Lies inside on X
        SplitWaterRectangleAlongXLine(minX < CameraRangeMinX ? CameraRangeMinX : CameraRangeMaxX, minX, maxX, Y1, Y2, P1, P2, P3, P4);
    } else if (minY < CameraRangeMinY || maxY > CameraRangeMaxY) { // Lies inside of Y
        SplitWaterRectangleAlongYLine(minY < CameraRangeMinY ? CameraRangeMinY : CameraRangeMaxY, minX, maxX, Y1, Y2, P1, P2, P3, P4);
    } else { // Lies inside of camera fully
        RenderHighDetailWaterRectangle(minX, maxX, Y1, Y2, P1, P2, P3, P4);
    }
}

// 0x6EBEC0
void CWaterLevel::RenderFlatWaterRectangle(int32 minX, int32 maxX, int32 Y1, int32 Y2, CRenPar P1, CRenPar P2, CRenPar P3, CRenPar P4) {
    if (bSplitBigPolys && maxX - minX > BigPolySize) {
        SplitWaterRectangleAlongXLine((minX + maxX) / 2,  minX, maxX, Y1, Y2, P1, P2, P3, P4);
#ifdef FIX_BUGS
    } else if (const auto [minY, maxY] = std::minmax(Y1, Y2); bSplitBigPolys && (maxY - minY) > BigPolySize) {
#else
    } else if (bSplitBigPolys && Y2 - Y1 > BigPolySize) {
#endif
        SplitWaterRectangleAlongYLine((Y2 + Y1) / 2, minX, maxX, Y1, Y2, P1, P2, P3, P4);
    } else {
        for (int32 lyr = 0; lyr < 2; lyr++) {
            RenderFlatWaterRectangle_OneLayer(minX, maxX, Y1, Y2, P1, P2, P3, P4, lyr);
        }
    }
}

// 0x6E9940
void CWaterLevel::RenderFlatWaterRectangle_OneLayer(int32 minX, int32 maxX, int32 Y1, int32 Y2, CRenPar P1, CRenPar P2, CRenPar P3, CRenPar P4, int32 WaterLayer) {
    RenderBuffer::RenderIfDoesntFit(6, 4);

    // First(!) push indices
    RenderBuffer::PushIndices({ 0, 1, 2, 2, 3, 0 }, true);

    // Get texture UV stuff
    const auto texuv = GetTextureUV(minX, Y1, Y2, WaterLayer);

    const auto PushVertex = [
        &,
        color = GetWaterColorForRendering(WaterColor, DebugWaterColors[DebugWaterColor::RECT], WaterLayer)
    ](int32 x, int32 y, const CRenPar& p, CVector2D vtxUVOffset) {
        RenderBuffer::PushVertex({ (float)x, (float)y, p.z }, texuv.baseShift + vtxUVOffset, color);
    };

    // Bottom right corner position on texture (In UV coords)
    const auto bruv{ CVector2D{ (float)(maxX - minX), (float)(Y2 - Y1) } / texuv.size };

    PushVertex(minX, Y1, P1, { 0.f,    0.f }); // Top Left
    PushVertex(maxX, Y1, P2, { bruv.x, 0.f    }); // Top Right
    PushVertex(maxX, Y2, P3, { bruv.x, bruv.y }); // Bottom Right
    PushVertex(minX, Y2, P4, { 0.f,    bruv.y }); // Bottom Left
} 

// 0x6EB810
void CWaterLevel::RenderHighDetailWaterRectangle(int32 minX, int32 maxX, int32 Y1, int32 Y2, CRenPar P1, CRenPar P2, CRenPar P3, CRenPar P4) {
    // Bounding sphere of the rectangle (in 2D, Z is the one of the 1st vertex)
    const int32  dx     = maxX - minX;
    const double halfDX = (double)dx * 0.5;
    const double halfDY = (double)(Y1 - Y2) * 0.5;
    const CVector center{
        (float)((double)(minX + maxX) * 0.5),
        (float)((double)(Y1 + Y2) * 0.5),
        P1.z
    };
    const float radius = (float)std::sqrt(halfDY * halfDY + halfDX * halfDX);

    // 0x420C40 and (if the mirror is active) the same for the mirrored view
    if (!TheCamera.IsSphereVisible(center, radius)) {
        return;
    }

    const auto [minY, maxY] = std::minmax(Y1, Y2);

    // Number of cells (each cell is 2x2 units)
    const int32 numCellsX = dx / 2;
    const int32 numCellsY = (maxY - minY) / 2;
    const int32 numTris   = numCellsY * numCellsX * 2;
    const int32 numVerts  = (numCellsY + 1) * (numCellsX + 1);

    if (numTris * 3 < 0x1000 && numVerts < 0x800) { // Fits into the render buffer
        SetUpWaterFog(minX, minY, maxX, maxY);
        for (int32 layer = 0; layer < 2; layer++) {
            RenderHighDetailWaterRectangle_OneLayer(minX, maxX, Y1, Y2, P1, P2, P3, P4, layer, numTris, numVerts, numCellsX, numCellsY);
        }
        return;
    }

    if (numCellsX > numCellsY) { // Too big => split along the longer side
        SplitWaterRectangleAlongXLine(minX + (numCellsX / 2) * 2, minX, maxX, Y1, Y2, P1, P2, P3, P4);
        return;
    }

    // Split along Y. Note: This isn't the same as `SplitWaterRectangleAlongYLine`, as `t` is calculated using `Y1` and `Y2` directly
    const int32 splitAtY = minY + (numCellsY / 2) * 2;
    const float t        = (float)((double)(splitAtY - Y1) / (double)(Y2 - Y1));
    const auto  P13      = lerpBlend(P1, P3, t);
    const auto  P24      = lerpBlend(P2, P4, t);

    // Top
    RenderWaterRectangle(minX, maxX, Y1, splitAtY, P1, P2, P13, P24);

    // Bottom
    RenderWaterRectangle(minX, maxX, splitAtY, Y2, P13, P24, P3, P4);
}

namespace {
// 0xC1F960 - Index of the vertex being generated (indexes the color caches below)
auto& s_VtxColorCacheIdx = StaticRef<int32>(0xC1F960);

// Per-vertex color (the first layer's) cache, so the second layer can reuse the exact same colors
auto& s_VtxColorCacheB = StaticRef<std::array<uint8, 0x800>>(0xC1F968);
auto& s_VtxColorCacheG = StaticRef<std::array<uint8, 0x800>>(0xC20168);
auto& s_VtxColorCacheR = StaticRef<std::array<uint8, 0x800>>(0xC20968);

// 0xC278D4 - Normal output of `CalculateWavesOnlyForCoordinate` (never read)
auto& s_WaveNormalSink = StaticRef<CVector>(0xC278D4);

// 0xC278E0 - If set, no indices are generated (and the vertex counter isn't advanced). Never written by anything known => always false
auto& s_bDontGenerateIndices = StaticRef<bool>(0xC278E0);
}

// 0x6E91D0
// Generates the `(numCellsX + 1) * (numCellsY + 1)` vertex grid (and the 2 triangles per cell) of the rectangle, with waves applied.
// Layer 0 generates the vertices (position, UV, color) while layer 1 only overwrites the UV and color of the vertices
// the previous call left in the (already rendered out) buffer.
// NOTE: `numTris` and `numVerts` are unused by the original.
void CWaterLevel::RenderHighDetailWaterRectangle_OneLayer(int32 minX, int32 maxX, int32 Y1, int32 Y2, CRenPar P1, CRenPar P2, CRenPar P3, CRenPar P4, int32 WaterLayer, int32 numTris, int32 numVerts, int32 numCellsX, int32 numCellsY) {
    s_VtxColorCacheIdx = 0;
    RenderBuffer::RenderAndEmptyRenderBuffer(); // 0x6E7680

    const float invX = (float)(1.0 / (double)numCellsX);
    const int32 stepX = (maxX - minX) / numCellsX;
    const float invY = (float)(1.0 / (double)numCellsY);
    const int32 stepY = (Y2 - Y1) / numCellsY;

    const auto Grad = [](float a, float b, float inv) {
        return (float)(((double)a - (double)b) * (double)inv);
    };

    // Gradients of the "upper" triangle (P1, P2, P3) - origin is P1
    const float zX_A = Grad(P2.z,          P1.z,          invX), zY_A = Grad(P3.z,          P1.z,          invY);
    const float bX_A = Grad(P2.bigWaves,   P1.bigWaves,   invX), bY_A = Grad(P3.bigWaves,   P1.bigWaves,   invY);
    const float sX_A = Grad(P2.smallWaves, P1.smallWaves, invX), sY_A = Grad(P3.smallWaves, P1.smallWaves, invY);

    // Gradients of the "lower" triangle - origin is P4 (and we're going backwards)
    const float zX_B = Grad(P3.z,          P4.z,          invX), zY_B = Grad(P2.z,          P4.z,          invY);
    const float bX_B = Grad(P3.bigWaves,   P4.bigWaves,   invX), bY_B = Grad(P2.bigWaves,   P4.bigWaves,   invY);
    const float sX_B = Grad(P3.smallWaves, P4.smallWaves, invX), sY_B = Grad(P2.smallWaves, P4.smallWaves, invY);

    // Base texture coordinates (just the fractional part)
    float baseU{}, baseV{};
    float uvScale{};
    switch (WaterLayer) {
    case 0:
        uvScale = 0.08f; // 0x859018
        baseU   = (float)((double)minX * (double)uvScale + (double)TextureShiftSecondU);
        baseV   = (float)((double)Y1   * (double)uvScale + (double)TextureShiftSecondV);
        break;
    case 1:
        uvScale = 0.04f; // 0x858CEC
        baseU   = (float)((double)minX * (double)uvScale + (double)TextureShiftFirstU);
        baseV   = (float)((double)Y1   * (double)uvScale + (double)TextureShiftFirstV);
        break;
    }
    if (WaterLayer == 0 || WaterLayer == 1) {
        baseU = (float)((double)baseU - std::floor((double)baseU)); // 0x8219F0 = floor
        baseV = (float)((double)baseV - std::floor((double)baseV));
    }

    const CVector camPos = TheCamera.GetPosition();

    // The positions of the "lower" triangle are calculated starting from the opposite corner (which is off by the division's remainder)
    const int32 startX_B = maxX - stepX * numCellsX;
    const int32 startY_B = Y2   - stepY * numCellsY;

    for (int32 j = 0; j <= numCellsY; j++) {
        const float fj = (float)j;
        const float jInvY = (float)((double)fj * (double)invY);

        for (int32 i = 0; i <= numCellsX; i++) {
            const float fi = (float)i;

            int32  X, Y;
            float  z, smallWaves;
            double bigWaves; // Stays in extended precision
            if ((double)fi * (double)invX + (double)jInvY < 1.0) {
                X = minX + i * stepX;
                Y = Y1   + j * stepY;
                z          = (float)((double)fj * (double)zY_A + (double)fi * (double)zX_A + (double)P1.z);
                bigWaves   = (double)fj * (double)bY_A + (double)fi * (double)bX_A + (double)P1.bigWaves;
                smallWaves = (float)((double)fi * (double)sX_A + (double)fj * (double)sY_A + (double)P1.smallWaves);
            } else {
                const float rj = (float)(numCellsY - j);
                const float ri = (float)(numCellsX - i);
                X = startX_B + i * stepX;
                Y = startY_B + j * stepY;
                z          = (float)((double)rj * (double)zY_B + (double)ri * (double)zX_B + (double)P4.z);
                bigWaves   = (double)rj * (double)bY_B + (double)ri * (double)bX_B + (double)P4.bigWaves;
                smallWaves = (float)((double)rj * (double)sY_B + (double)ri * (double)sX_B + (double)P4.smallWaves);
            }

            const float fX = (float)X;
            const float fY = (float)Y;

            // Fade the waves out with the distance from the camera
            const float dx = (float)((double)camPos.x - (double)X);
            const float dy = (float)((double)camPos.y - (double)Y);
            const double dist = std::sqrt((double)dy * (double)dy + (double)dx * (double)dx) / (double)DETAILEDWATERDIST;
            double fade;
            if (dist > 1.0) {
                fade = 0.0; // 0x858B50
            } else {
                const double distF = (double)(float)dist;
                fade = distF > 0.75 // 0x858F34
                    ? (1.0 - distF) * 4.0 // 0x858B90
                    : 1.0;
            }

            const int32 uOff = i * stepX;
            const int32 vOff = j * stepY;

            switch (WaterLayer) {
            case 0: {
                float colorMult, glare;
                CalculateWavesOnlyForCoordinate(
                    X, Y,
                    (float)(fade * bigWaves),
                    (float)((double)smallWaves * fade),
                    z, colorMult, glare, s_WaveNormalSink
                );

                auto* const vtx = &TempBufferVertices.m_3d[uiTempBufferVerticesStored];
                RwIm3DVertexSetPos(vtx, fX, fY, z);
                RwIm3DVertexSetU(vtx, (float)((double)uOff * (double)uvScale + (double)baseU));
                RwIm3DVertexSetV(vtx, (float)((double)vOff * (double)uvScale + (double)baseV));

                // Truncated to 8 bits (no clamping)
                const auto r = (uint8)(int32)((double)WaterColor.r * (double)colorMult);
                const auto g = (uint8)(int32)((double)WaterColor.g * (double)colorMult);
                const auto b = (uint8)(int32)((double)WaterColor.b * (double)colorMult);
                RwIm3DVertexSetRGBA(vtx, r, g, b, (uint8)WaterLayerAlpha[0]);

                s_VtxColorCacheG[s_VtxColorCacheIdx] = g;
                s_VtxColorCacheR[s_VtxColorCacheIdx] = r;
                s_VtxColorCacheB[s_VtxColorCacheIdx] = b;
                s_VtxColorCacheIdx++;
                break;
            }
            case 1: {
                auto* const vtx = &TempBufferVertices.m_3d[uiTempBufferVerticesStored]; // Position is already set by the previous layer
                RwIm3DVertexSetU(vtx, (float)((double)uOff * (double)uvScale + (double)baseU));
                RwIm3DVertexSetV(vtx, (float)((double)vOff * (double)uvScale + (double)baseV));
                RwIm3DVertexSetRGBA(
                    vtx,
                    s_VtxColorCacheR[s_VtxColorCacheIdx],
                    s_VtxColorCacheG[s_VtxColorCacheIdx],
                    s_VtxColorCacheB[s_VtxColorCacheIdx],
                    (uint8)WaterLayerAlpha[1]
                );
                s_VtxColorCacheIdx++;
                break;
            }
            }

            if (!s_bDontGenerateIndices) {
                const int32 vtxIdx = uiTempBufferVerticesStored;
                if (j != 0 && i != 0) {
                    // Triangles: (above-left, above, left) and (this, above, left)
                    const int32 above = vtxIdx - numCellsX;
                    auto        idx   = (int32)uiTempBufferIndicesStored;
                    aTempBufferIndices[idx++] = (RxVertexIndex)(above - 2);
                    aTempBufferIndices[idx++] = (RxVertexIndex)(above - 1);
                    aTempBufferIndices[idx++] = (RxVertexIndex)(vtxIdx - 1);
                    aTempBufferIndices[idx++] = (RxVertexIndex)vtxIdx;
                    aTempBufferIndices[idx++] = (RxVertexIndex)(above - 1);
                    aTempBufferIndices[idx++] = (RxVertexIndex)(vtxIdx - 1);
                    uiTempBufferIndicesStored = (uint16)idx;
                }
                uiTempBufferVerticesStored = (uint16)(vtxIdx + 1);
            }
        }
    }
}

// 0x6E8780
// Generates the vertices (and the triangles) of the `numCells`-sided right triangle, with waves applied. Row `j` has `numCells - j + 1` vertices.
// The origin of the grid is `P1` if `X1 == X3`, and `P2` otherwise (the 2 triangles `SplitWaterTriangleAlong[XY]Line` produce); `P3` is always along the Y axis.
// Layer 0 generates the vertices (position, UV, color), layer 1 only overwrites the UV and color of those, layer 2 (never used) is a glare layer.
// NOTE: `numTris` and `numVerts` are unused by the original.
// NOTE: Unlike in the rectangle's version, the wave amplitudes are rounded to float and the color doesn't depend on the `colorMult` output of
//       `CalculateWavesOnlyForCoordinate` (the original overwrites it with the constant 0.577f), and indices aren't gated by `s_bDontGenerateIndices`.
void CWaterLevel::RenderHighDetailWaterTriangle_OneLayer(int32 X1, int32 Y1, CRenPar P1, int32 X2, int32 Y2, CRenPar P2, int32 X3, int32 Y3, CRenPar P3, int32 WaterLayer, int32 numTris, int32 numVerts, int32 numCells) {
    s_VtxColorCacheIdx = 0;
    RenderBuffer::RenderAndEmptyRenderBuffer(); // 0x6E7680

    const double inv = 1.0 / (double)numCells; // Stays on the x87 stack => extended precision

    const bool       isFirstOrigin = X1 == X3;
    const CRenPar&   O             = isFirstOrigin ? P1 : P2; // Origin
    const CRenPar&   Pa            = isFirstOrigin ? P2 : P1; // Along X
    const int32      X0            = isFirstOrigin ? X1 : X2;
    const int32      Y0            = isFirstOrigin ? Y1 : Y2;
    const int32      stepX         = ((isFirstOrigin ? X2 : X1) - X0) / numCells;
    const int32      stepY         = (Y3 - Y0) / numCells;

    const auto Grad = [&](float a, float b) {
        return (float)(((double)a - (double)b) * inv);
    };
    const float zX = Grad(Pa.z,          O.z),          zY = Grad(P3.z,          O.z);
    const float bX = Grad(Pa.bigWaves,   O.bigWaves),   bY = Grad(P3.bigWaves,   O.bigWaves);
    const float sX = Grad(Pa.smallWaves, O.smallWaves), sY = Grad(P3.smallWaves, O.smallWaves);

    // Base texture coordinates (just the fractional part)
    float baseU{}, baseV{};
    float uvScale{};
    switch (WaterLayer) {
    case 0:
        uvScale = 0.08f; // 0x859018
        baseU   = (float)((double)X0 * (double)uvScale + (double)TextureShiftSecondU);
        baseV   = (float)((double)Y0 * (double)uvScale + (double)TextureShiftSecondV);
        break;
    case 1:
        uvScale = 0.04f; // 0x858CEC
        baseU   = (float)((double)X0 * (double)uvScale + (double)TextureShiftFirstU);
        baseV   = (float)((double)Y0 * (double)uvScale + (double)TextureShiftFirstV);
        break;
    }
    if (WaterLayer == 0 || WaterLayer == 1) {
        baseU = (float)((double)baseU - std::floor((double)baseU)); // 0x8219F0 = floor
        baseV = (float)((double)baseV - std::floor((double)baseV));
    }

    const CVector camPos = TheCamera.GetPosition();

    float glare{}; // NOTE: Layer 2 reads this without it ever being written in the original (uninitialized stack)
    int32 uOff{}, vOff{}; // Offsets (in units) from the origin
    int32 X{}, Y = Y0;
    for (int32 j = 0; j <= numCells; j++) {
        const int32 numInRow = numCells - j;
        const float zJ = (float)((double)j * (double)zY);
        const float bJ = (float)((double)j * (double)bY);
        const float sJ = (float)((double)j * (double)sY);
        const float fY = (float)Y;

        uOff = 0;
        X    = X0;
        for (int32 i = 0; i <= numInRow; i++) {
            const float z0         = (float)((double)zX * (double)i + (double)zJ + (double)O.z);
            const float bigWaves   = (float)((double)bX * (double)i + (double)bJ + (double)O.bigWaves);
            const float smallWaves = (float)((double)i * (double)sX + (double)sJ + (double)O.smallWaves);
            float       z          = z0;

            const float fX = (float)X;

            // Fade the waves out with the distance from the camera
            const float dx = (float)((double)camPos.x - (double)X);
            const float dy = (float)((double)camPos.y - (double)fY);
            const double dist = std::sqrt((double)dy * (double)dy + (double)dx * (double)dx) / (double)DETAILEDWATERDIST;
            double fade;
            if (dist > 1.0) {
                fade = 0.0; // 0x858B50
            } else {
                const double distF = (double)(float)dist;
                fade = distF > 0.75 // 0x858F34
                    ? (1.0 - distF) * 4.0 // 0x858B90
                    : 1.0;
            }

            auto* const vtx = &TempBufferVertices.m_3d[uiTempBufferVerticesStored];
            switch (WaterLayer) {
            case 0: {
                float colorMult;
                CalculateWavesOnlyForCoordinate(
                    X, Y,
                    (float)(fade * (double)bigWaves),
                    (float)((double)smallWaves * fade),
                    z, colorMult, glare, s_WaveNormalSink
                );
                (void)colorMult; // The original overwrites it with 0.577f (0x872004) and doesn't use it

                RwIm3DVertexSetPos(vtx, fX, fY, z);
                RwIm3DVertexSetU(vtx, (float)((double)uOff * (double)uvScale + (double)baseU));
                RwIm3DVertexSetV(vtx, (float)((double)vOff * (double)uvScale + (double)baseV));

                // Truncated to 8 bits (no clamping)
                const auto r = (uint8)(int32)((double)WaterColor.r * (double)0.577f); // 0x872004
                const auto g = (uint8)(int32)((double)WaterColor.g * (double)0.577f);
                const auto b = (uint8)(int32)((double)WaterColor.b * (double)0.577f);
                RwIm3DVertexSetRGBA(vtx, r, g, b, (uint8)WaterLayerAlpha[0]);

                s_VtxColorCacheR[s_VtxColorCacheIdx] = r;
                s_VtxColorCacheG[s_VtxColorCacheIdx] = g;
                s_VtxColorCacheB[s_VtxColorCacheIdx] = b;
                s_VtxColorCacheIdx++;
                break;
            }
            case 1:
                RwIm3DVertexSetU(vtx, (float)((double)uOff * (double)uvScale + (double)baseU));
                RwIm3DVertexSetV(vtx, (float)((double)vOff * (double)uvScale + (double)baseV));
                RwIm3DVertexSetRGBA(
                    vtx,
                    s_VtxColorCacheR[s_VtxColorCacheIdx],
                    s_VtxColorCacheG[s_VtxColorCacheIdx],
                    s_VtxColorCacheB[s_VtxColorCacheIdx],
                    (uint8)WaterLayerAlpha[1]
                );
                s_VtxColorCacheIdx++;
                break;
            case 2: { // Never used
                RwIm3DVertexSetU(vtx, (float)((double)uOff * (double)TextureJitterMult + (double)TextureShiftThirdU));
                RwIm3DVertexSetV(vtx, (float)((double)vOff * (double)TextureJitterMult + (double)TextureShiftThirdV));
                RwIm3DVertexSetPos(vtx, fX, fY, (float)((double)z + (double)0.1f)); // 0x858B1C
                const auto a = (uint8)(int32)((double)StaticRef<int32>(0x8D3810) * (double)glare);
                RwIm3DVertexSetRGBA(vtx, a, a, a, 0xFF);
                break;
            }
            }

            // NOTE: Not gated by `s_bDontGenerateIndices`, unlike the rectangle's
            const int32 vtxIdx = uiTempBufferVerticesStored;
            if (j != 0 && i != 0) {
                // Triangles: (this, left, above-left) and (this, above, above-left)
                const int32 w   = vtxIdx + (j - numCells);
                auto        idx = (int32)uiTempBufferIndicesStored;
                aTempBufferIndices[idx++] = (RxVertexIndex)vtxIdx;
                aTempBufferIndices[idx++] = (RxVertexIndex)(vtxIdx - 1);
                aTempBufferIndices[idx++] = (RxVertexIndex)(w - 3);
                aTempBufferIndices[idx++] = (RxVertexIndex)vtxIdx;
                aTempBufferIndices[idx++] = (RxVertexIndex)(w - 2);
                aTempBufferIndices[idx++] = (RxVertexIndex)(w - 3);
                uiTempBufferIndicesStored = (uint16)idx;
            }
            uiTempBufferVerticesStored = (uint16)(vtxIdx + 1);

            uOff += stepX;
            X    += stepX;
        }

        if (j != 0) { // The triangle closing the row (the last vertex, the last of the previous row and the one before it)
            const int32 next = uiTempBufferVerticesStored; // One past the last vertex of this row
            const int32 w    = next + (j - numCells);
            auto        idx  = (int32)uiTempBufferIndicesStored;
            aTempBufferIndices[idx++] = (RxVertexIndex)(next - 1);
            aTempBufferIndices[idx++] = (RxVertexIndex)(w - 2);
            aTempBufferIndices[idx++] = (RxVertexIndex)(w - 3);
            uiTempBufferIndicesStored = (uint16)idx;
        }

        vOff += stepY;
        Y    += stepY;
    }
}

// 0x6E73A0
void CWaterLevel::SplitWaterRectangleAlongXLine(int32 splitAtX, int32 minX, int32 maxX, int32 Y1, int32 Y2, CRenPar P1, CRenPar P2, CRenPar P3, CRenPar P4) {
    const auto t = (float)(splitAtX - minX) / (float)(maxX - minX);

    const auto P12 = lerpBlend(P1, P2, t);
    const auto P34 = lerpBlend(P3, P4, t);

    // Left
    RenderWaterRectangle(
        minX, splitAtX,
        Y1, Y2,
        P1, P12, P3, P34
    );

    // Right
    RenderWaterRectangle(
        splitAtX, maxX,
        Y1, Y2,
        P12, P2, P34, P4
    );
}

// 0x6ED6D0 - Though fully inlined into `RenderWaterRectangle`
void CWaterLevel::SplitWaterRectangleAlongYLine(int32 splitAtY, int32 minX, int32 maxX, int32 Y1, int32 Y2, CRenPar P1, CRenPar P2, CRenPar P3, CRenPar P4) {
    const auto [minY, maxY] = std::minmax(Y1, Y2);

    const auto t = (float)(splitAtY - minY) / (float)(maxY - minY);

    const auto P13 = lerpBlend(P1, P3, t);
    const auto P24 = lerpBlend(P2, P4, t);

    // Top
    RenderWaterRectangle(
        minX, maxX,
        Y1, splitAtY,
        P1, P2, P13, P24
    );

    // Bottom
    RenderWaterRectangle(
        minX,     maxX,
        splitAtY, Y2,
        P13, P24, P3, P4
    );
}

// 0x6EB710
void CWaterLevel::PreRenderWater() {
    ZoneScoped;

    if (CGame::CanSeeWaterFromCurrArea()) {
        ScanThroughBlocks();
        UpdateFlow();
        HandleBeachToysStuff();
    }
}

// NOTSA: From PreRenderWater()
void CWaterLevel::UpdateFlow() {
    if (CTimer::m_FrameCounter % 32 == 29) {
        CWaterLevel::FindNearestWaterAndItsFlow();
    }

    const auto CalculateFlowOnAxis = [
        step = CTimer::GetTimeStep() / 1000.f
    ](float desired, float curr) {
        const auto delta = desired - curr;
        return std::abs(delta) < step
            ? desired
            : curr + std::copysign(step, delta);
    };

    m_CurrentFlow = {
        CalculateFlowOnAxis(m_CurrentDesiredFlow.x, m_CurrentFlow.x),
        CalculateFlowOnAxis(m_CurrentDesiredFlow.y, m_CurrentFlow.y)
    };
}

// 0x6EB690
bool CWaterLevel::GetWaterLevel(float x, float y, float z, float& pOutWaterLevel, uint8 bTouchingWater, CVector* pVecNormals) {
    float smallWaves, bigWaves;
    if (!GetWaterLevelNoWaves({x, y, z}, &pOutWaterLevel, &smallWaves, &bigWaves)) {
        return false;
    }
     
    if ((pOutWaterLevel - z > 3.0F) && !bTouchingWater) {
        pOutWaterLevel = 0.0F;
        return false;
    }

    AddWaveToResult(x, y, &pOutWaterLevel, smallWaves, bigWaves, pVecNormals);

    return true;
}

// 0x6EA9F0
void CWaterLevel::SetUpWaterFog(int32 minX, int32 minY, int32 maxX, int32 maxY) {
    if (!CWaterLevel::m_bWaterFog || gWaterFogIndex >= 70) {
        return;
    }

    const auto fogZ = [&] {
        if (float waterLvl, bigWaves, smallWaves; GetWaterLevelNoWaves({(float)minX, (float)minY, 0.f}, & waterLvl, & bigWaves, & smallWaves)) {
            if (bigWaves != 0.f || smallWaves != 0.f) {
                return waterLvl;
            }
        }
        return 0.f;
    }();

    const auto plyrPos = FindPlayerCoors();
    gbPlayerIsInsideWaterFog = m_fWaterFogHeight + fogZ > plyrPos.z && CRect{ (float)minX, (float)minY, (float)maxX, (float)maxY }.IsPointInside(plyrPos);

    const auto idx = gWaterFogIndex++;
    ms_WaterFog.minX[idx] = minX;
    ms_WaterFog.minY[idx] = minY;
    ms_WaterFog.maxX[idx] = maxX;
    ms_WaterFog.maxY[idx] = maxY;
    ms_WaterFog.z[idx]    = fogZ;
}

// 0x6E9D70
void CWaterLevel::FindNearestWaterAndItsFlow() {
    const auto& camPos = TheCamera.GetPosition();
    const float camX   = camPos.x;
    const float camY   = camPos.y;

    // Camera is outside of the world
    if (!(camX > -3000.f && camX < 3000.f && camY > -3000.f && camY < 3000.f)) {
        TheCamera.m_fDistanceToWater      = 0.f;
        TheCamera.m_fHeightOfNearestWater = 0.f;
        m_CurrentDesiredFlow.y            = 0.f;
        m_CurrentDesiredFlow.x            = 0.f;
        return;
    }

    float closestWavyDist = 1e7f; // Distance to the closest quad that has waves
    float closestDist     = 1e7f; // Distance to the closest quad
    float nearestWaterZ   = 0.f;

    // Distance of `cam` from the range [lo, hi]
    const auto GetDistToRange = [](float cam, float lo, float hi) {
        if (cam < lo) {
            return lo - cam;
        }
        return hi < cam ? cam - hi : 0.f;
    };

    for (const auto& quad : std::span{ WaterQuads }.first(NumWaterQuads)) {
        const auto& v0 = m_aVertices[(int16)quad.verts[0]];
        const auto& v1 = m_aVertices[(int16)quad.verts[1]];
        const auto& v2 = m_aVertices[(int16)quad.verts[2]];
        const auto& v3 = m_aVertices[(int16)quad.verts[3]];

        const float fv0x = (float)v0.x, fv0y = (float)v0.y;

        const float distX = GetDistToRange(camX, fv0x, (float)v1.x);
        const float distY = GetDistToRange(camY, fv0y, (float)v2.y);
        const float dist  = std::sqrt(distX * distX + distY * distY);

        if (dist < closestWavyDist) {
            if (v0.rp.bigWaves != 0.f || v0.rp.smallWaves != 0.f
                || v1.rp.bigWaves != 0.f || v1.rp.smallWaves != 0.f
                || v2.rp.bigWaves != 0.f || v2.rp.smallWaves != 0.f
                || v3.rp.bigWaves != 0.f || v3.rp.smallWaves != 0.f
            ) {
                nearestWaterZ   = v0.rp.z;
                closestWavyDist = dist;
            }
        }

        if (dist < closestDist) {
            closestDist = dist;

            // Use the flow of the closest vertex
            const auto SqDistTo = [&](const CWaterVertex& v) {
                const float dy = camY - (float)v.y;
                const float dx = camX - (float)v.x;
                return dy * dy + dx * dx;
            };
            const float d0 = SqDistTo(v0), d1 = SqDistTo(v1), d2 = SqDistTo(v2), d3 = SqDistTo(v3);

            const CWaterVertex* closest;
            if (d0 < d1 && d0 < d2 && d0 < d3) {
                closest = &v0;
            } else if (d1 < d2 && d1 < d3) {
                closest = &v1;
            } else if (d2 < d3) {
                closest = &v2;
            } else {
                closest = &v3;
            }
            m_CurrentDesiredFlow.x = (float)closest->rp.flowX * (1.f / 64.f);
            m_CurrentDesiredFlow.y = (float)closest->rp.flowY * (1.f / 64.f);
        }
    }

    TheCamera.m_fDistanceToWater      = closestWavyDist;
    TheCamera.m_fHeightOfNearestWater = nearestWaterZ;
}

// 0x6E5BB0
// NOTE: x87 extended precision in the original => `double` intermediates. `u` is rounded to float (spilled), `v` is NOT (stays on the x87 stack)
// NOTE: All the early-outs are written like the original's FCOMP + flags tests (NaN of the inputs => rejected)
template<>
bool CWaterQuad::GetWaterLevel(float x, float y, float z, float* outWaterLevel, float* outBigWaves, float* outSmallWaves) const {
    const auto& v0 = CWaterLevel::m_aVertices[verts[0]];
    const auto& v1 = CWaterLevel::m_aVertices[verts[1]];
    const auto& v2 = CWaterLevel::m_aVertices[verts[2]];
    const auto& v3 = CWaterLevel::m_aVertices[verts[3]];

    // Bounding box test (v0 = min corner, v1 = max X, v2 = max Y)
    const float x0 = (float)v0.x;
    if (!(x >= x0)) {
        return false;
    }
    if (!((float)v1.x >= x)) {
        return false;
    }
    const float y0 = (float)v0.y;
    if (!(y >= y0)) {
        return false;
    }
    if (!((float)v2.y >= y)) {
        return false;
    }

    const float  u = (float)(((double)x - (double)x0) / (double)(v1.x - v0.x));
    const double v = ((double)y - (double)y0) / (double)(v2.y - v0.y);

    // Interpolate `Get` (a member of `CRenPar`) in the triangle `base`, `a`, `b`: `base + (a - base) * wa + (b - base) * wb`
    const auto Interpolate = [](auto get, const CWaterVertex& base, const CWaterVertex& a, double wa, const CWaterVertex& b, double wb) {
        return ((double)get(a.rp) - (double)get(base.rp)) * wa + ((double)get(b.rp) - (double)get(base.rp)) * wb + (double)get(base.rp);
    };
    const auto GetZ     = [](const CRenPar& rp) { return rp.z; };
    const auto GetBig   = [](const CRenPar& rp) { return rp.bigWaves; };
    const auto GetSmall = [](const CRenPar& rp) { return rp.smallWaves; };

    if (!((double)u + v <= 1.0)) { // Upper triangle (v1, v2, v3)
        const double a = 1.0 - (double)u;
        const double b = 1.0 - v;
        *outWaterLevel = (float)Interpolate(GetZ, v3, v2, a, v1, b);
        if (!outBigWaves) {
            return true; // NOTE: The original returns here (skipping the depth tests below), unlike the triangle's
        }
        *outBigWaves   = (float)Interpolate(GetBig,   v3, v1, b, v2, a);
        *outSmallWaves = (float)Interpolate(GetSmall, v3, v1, b, v2, a);
    } else { // Lower triangle (v0, v1, v2)
        *outWaterLevel = (float)Interpolate(GetZ, v0, v2, v, v1, (double)u);
        if (!outBigWaves) {
            return true; // See above
        }
        *outBigWaves   = (float)Interpolate(GetBig,   v0, v2, v, v1, (double)u);
        *outSmallWaves = (float)Interpolate(GetSmall, v0, v2, v, v1, (double)u);
    }

    // Limited depth polys only apply to the water close to `z`
    if ((double)*outWaterLevel - (double)6.f > (double)z && bLimitedDepth) {
        return false;
    }
    return !((double)*outWaterLevel + (double)20.f < (double)z);
}

// 0x6E5E90
// NOTE: Same notes as the quad's. Here both `u` and `v` are rounded to float
template<>
bool CWaterTriangle::GetWaterLevel(float x, float y, float z, float* outWaterLevel, float* outBigWaves, float* outSmallWaves) const {
    const auto& v0 = CWaterLevel::m_aVertices[verts[0]];
    const auto& v1 = CWaterLevel::m_aVertices[verts[1]];
    const auto& v2 = CWaterLevel::m_aVertices[verts[2]];

    // Bounding box test
    const float x0 = (float)v0.x;
    if (!(x >= x0)) {
        return false;
    }
    if (!((float)v1.x >= x)) {
        return false;
    }
    const auto [minY, maxY] = std::minmax(v0.y, v2.y); // Original: compares them as int16, then loads them as float
    if (!(y >= (float)minY)) {
        return false;
    }
    if (!(y <= (float)maxY)) {
        return false;
    }

    const float y0 = (float)v0.y;
    const float u  = (float)(((double)x - (double)x0) / (double)(v1.x - v0.x));
    const float v  = (float)(((double)y - (double)y0) / (double)(v2.y - v0.y));

    // `base + (a - base) * wa + (b - base) * wb`
    const auto Interpolate = [](auto get, const CWaterVertex& base, const CWaterVertex& a, double wa, const CWaterVertex& b, double wb) {
        return ((double)get(a.rp) - (double)get(base.rp)) * wa + ((double)get(b.rp) - (double)get(base.rp)) * wb + (double)get(base.rp);
    };
    const auto GetZ     = [](const CRenPar& rp) { return rp.z; };
    const auto GetBig   = [](const CRenPar& rp) { return rp.bigWaves; };
    const auto GetSmall = [](const CRenPar& rp) { return rp.smallWaves; };

    if (v0.x == v2.x) { // Vertical left edge => (v0, v1, v2) with (u, v) as is
        if (!((double)v + (double)u <= 1.0)) {
            return false;
        }
        *outWaterLevel = (float)Interpolate(GetZ, v0, v1, (double)u, v2, (double)v);
        if (outBigWaves) {
            *outBigWaves   = (float)Interpolate(GetBig,   v0, v1, (double)u, v2, (double)v);
            *outSmallWaves = (float)Interpolate(GetSmall, v0, v1, (double)u, v2, (double)v);
        }
    } else { // Vertical right edge => base is v1
        if (!(u >= v)) {
            return false;
        }
        const double a = 1.0 - (double)u;
        *outWaterLevel = (float)Interpolate(GetZ, v1, v2, (double)v, v0, a);
        if (outBigWaves) {
            *outBigWaves   = (float)Interpolate(GetBig,   v1, v0, a, v2, (double)v);
            *outSmallWaves = (float)Interpolate(GetSmall, v1, v0, a, v2, (double)v);
        }
    }

    if ((double)*outWaterLevel - (double)6.f > (double)z && bLimitedDepth) {
        return false;
    }
    return !((double)*outWaterLevel + (double)20.f < (double)z);
}

// 0x6E8580
bool CWaterLevel::GetWaterLevelNoWaves(CVector pos, float* pOutWaterLevel, float* pOutBigWaves, float* pOutSmallWaves) {
    const int32 blockX = (int32)std::floor(pos.x * 0.002f + 6.0f);
    const int32 blockY = (int32)std::floor(pos.y * 0.002f + 6.0f);

    // Outside of the world => Flat water at 0
    if (blockX < 0 || blockX >= NUM_WATER_BLOCKS_ROWCOL || blockY < 0 || blockY >= NUM_WATER_BLOCKS_ROWCOL) {
        *pOutWaterLevel = 0.f;
        if (pOutBigWaves) {
            *pOutBigWaves = 1.f;
        }
        if (pOutSmallWaves) {
            *pOutSmallWaves = 0.f;
        }
        return true;
    }

    const auto GetLevelInQuad = [&](uint32 idx) { // 0x6E5BB0
        return WaterQuads[idx].GetWaterLevel(pos.x, pos.y, pos.z, pOutWaterLevel, pOutBigWaves, pOutSmallWaves);
    };
    const auto GetLevelInTri = [&](uint32 idx) { // 0x6E5E90
        return WaterTriangles[idx].GetWaterLevel(pos.x, pos.y, pos.z, pOutWaterLevel, pOutBigWaves, pOutSmallWaves);
    };

    const auto& info = m_BlockPolyInfo[blockX][blockY];
    switch (info.Type()) {
    case PolyInfo::PType::NONE:
        return false;
    case PolyInfo::PType::SINGLE_QUAD:
        return GetLevelInQuad(info.Id());
    case PolyInfo::PType::SINGLE_TRI:
        return GetLevelInTri(info.Id());
    case PolyInfo::PType::COMBO: {
        // Sequence of polys, terminated by an entry of type `NONE`
        for (auto* poly = &m_PolyCombos[info.Id()]; poly->Type() != PolyInfo::PType::NONE; poly++) {
            switch (poly->Type()) {
            case PolyInfo::PType::SINGLE_QUAD:
                if (GetLevelInQuad(poly->Id())) {
                    return true;
                }
                break;
            case PolyInfo::PType::SINGLE_TRI:
                if (GetLevelInTri(poly->Id())) {
                    return true;
                }
                break;
            default:
                break;
            }
        }
        return false;
    }
    default:
        NOTSA_UNREACHABLE();
    }
}

// 0x6EA960
bool CWaterLevel::GetWaterDepth(const CVector& vecPos, float* pOutWaterDepth, float* pOutWaterLevel, float* pOutGroundLevel) {
    float waterLevel;
    if (!GetWaterLevelNoWaves(vecPos, &waterLevel, nullptr, nullptr)) {
        return false;
    }

    float groundLevel;
    if (!GetGroundLevel(vecPos, &groundLevel, nullptr, 30.f)) { // 0x6EA8A0
        groundLevel = -100.f; // 0x859014
    }

    if (pOutWaterDepth) {
        *pOutWaterDepth = waterLevel - groundLevel;
    }
    if (pOutWaterLevel) {
        *pOutWaterLevel = waterLevel;
    }
    if (pOutGroundLevel) {
        *pOutGroundLevel = groundLevel;
    }
    return true;
}

// 0x6EA8A0
bool CWaterLevel::GetGroundLevel(const CVector& pos, float* outGroundZ, ColData* outColData, float radius) {
    // NOTE: Starts `radius` above `pos`, and looks `radius` downwards
    const CVector origin{ pos.x, pos.y, (float)((double)radius + (double)pos.z) };

    CColPoint colPoint;
    CEntity*  entity{}; // The original reuses the `radius` argument's stack slot for this
    if (!CWorld::ProcessVerticalLine(origin, -radius, colPoint, entity, true, false, false, false, true, false, nullptr)) { // 0x5674E0
        return false;
    }

    *outGroundZ = colPoint.m_vecPoint.z;
    if (outColData) {
        outColData->surfaceType = (uint8)colPoint.m_nSurfaceTypeB;
        outColData->pieceType   = colPoint.m_nPieceTypeB;
    }
    return true;
}

// 0x6E61B0
// Tests whether the line `start` -> `end` crosses the z = 0 plane (the caller passes the points relative to the water level) inside a water block
// that has water there. If so, the crossing point is written to `outHitPos` and `true` is returned.
// NOTE: Only quads are considered (a block of type `SINGLE_TRI` is skipped, as are triangles in combos), as in the original
// NOTE: The original uses truncation (and not `floor`) to calculate the block coordinates, so negative coordinates in (-500, 0) end up in block 6
// NOTE: x87 extended precision in the original => `double` intermediates, with the float spills (`t`, `dx * t`, `dz`) of the original
bool CWaterLevel::TestLineAgainstWater(CVector start, CVector end, CVector* outHitPos) {
    // Original selects with FCOMP + flags tests => NaN handling as `a < b ? a : b` / `a > b ? a : b`
    const float minZ = start.z < end.z ? start.z : end.z;
    const float minY = start.y < end.y ? start.y : end.y;
    const float minX = start.x < end.x ? start.x : end.x;
    const float maxZ = start.z > end.z ? start.z : end.z;
    const float maxY = start.y > end.y ? start.y : end.y;
    const float maxX = start.x > end.x ? start.x : end.x;

    // 0x821B40 (ftol => truncation) of `v * 0.002 + 6`
    const auto ToBlock = [](float v) {
        return (int32)((double)v * (double)0.002f + (double)6.f); // 0x858F44, 0x858B44
    };
    const int32 minBlockY = ToBlock(minY);
    const int32 maxBlockX = ToBlock(maxX);
    const int32 maxBlockY = ToBlock(maxY);
    const int32 minBlockX = ToBlock(minX);

    // Calculates the point where the line crosses z = 0 (`t` is the fraction of the way) and writes it to the output
    const auto CalcHitPos = [&] {
        const float t  = (float)(std::fabs((double)start.z) / ((double)maxZ - (double)minZ));
        const float dx = (float)(((double)end.x - (double)start.x) * (double)t);      // Rounded to float
        const double dy = ((double)end.y - (double)start.y) * (double)t;              // NOT rounded
        const float dz = (float)((double)end.z - (double)start.z);                    // Rounded to float
        outHitPos->x = (float)((double)dx + (double)start.x);
        outHitPos->y = (float)(dy + (double)start.y);
        outHitPos->z = (float)((double)dz * (double)t + (double)start.z);
    };

    // Tests the quad: Does the line cross z = 0 here, and is the hit pos inside the quad?
    const auto TestQuad = [&](uint32 quadId) {
        if (!((double)start.z * (double)end.z < 0.0)) { // 0x858B50
            return false;
        }
        CalcHitPos();
        const auto& quad = WaterQuads[quadId];
        const auto& v0   = m_aVertices[quad.verts[0]];
        const auto& v1   = m_aVertices[quad.verts[1]];
        const auto& v2   = m_aVertices[quad.verts[2]];
        return (float)v0.x <= outHitPos->x
            && (float)v1.x >= outHitPos->x
            && (float)v0.y <= outHitPos->y
            && (float)v2.y >= outHitPos->y;
    };

    for (int32 blockX = minBlockX; blockX <= maxBlockX; blockX++) {
        for (int32 blockY = minBlockY; blockY <= maxBlockY; blockY++) {
            if (blockX < 0 || blockX >= NUM_WATER_BLOCKS_ROWCOL || blockY < 0 || blockY >= NUM_WATER_BLOCKS_ROWCOL) { // Outside of the world (flat water)
                if (!(minZ < 0.f) || !(maxZ > 0.f)) { // 0x858B50
                    continue;
                }
                CalcHitPos();
                if (blockX == ToBlock(outHitPos->x) && blockY == ToBlock(outHitPos->y)) {
                    return true;
                }
                continue;
            }

            const auto& info = m_BlockPolyInfo[blockX][blockY];
            switch (info.Type()) {
            case PolyInfo::PType::SINGLE_QUAD:
                if (TestQuad(info.Id())) {
                    return true;
                }
                break;
            case PolyInfo::PType::COMBO:
                for (auto* poly = &m_PolyCombos[info.Id()]; poly->Type() != PolyInfo::PType::NONE; poly++) {
                    if (poly->Type() == PolyInfo::PType::SINGLE_QUAD && TestQuad(poly->Id())) {
                        return true;
                    }
                }
                break;
            default: // NONE / SINGLE_TRI
                break;
            }
        }
    }
    return false;
}

// 0x6E7760
void CWaterLevel::RenderWaterFog() {
    ZoneScoped;

    if (!m_bWaterFog || !m_bWaterFogScript) {
        return;
    }

    if (!(CWeather::UnderWaterness < CPostEffects::m_fWaterFXStartUnderWaterness)) {
        gWaterFogIndex = 0;
        return;
    }

    const int32 numFogLayers = (int32)((float)m_WaterFogDensity * CWeather::WaterFogFXControl);
    if (numFogLayers == 0) {
        gWaterFogIndex = 0;
        return;
    }
    const float layerHeight = m_fWaterFogHeight / (float)numFogLayers;

    const auto numFogBoxes = (int32)gWaterFogIndex;
    gWaterFogIndex = 0;

    // Update the fade of the full-screen quad that's rendered when inside of the fog
    if (gbPlayerIsInsideWaterFog) {
        m_fWaterFogInsideFade = CTimer::GetTimeStep() * m_fWaterFogInsideFadeSpeed + m_fWaterFogInsideFade;
        if (m_fWaterFogInsideFade > 1.f) {
            m_fWaterFogInsideFade = 1.f;
        }
        m_fWaterFogInsideTimer = 40.f;
    } else {
        m_fWaterFogInsideTimer -= CTimer::GetTimeStep();
        if (!(m_fWaterFogInsideTimer > 0.f)) {
            m_fWaterFogInsideTimer = 0.f;
            m_fWaterFogInsideFade -= CTimer::GetTimeStep() * m_fWaterFogInsideFadeSpeed;
            if (m_fWaterFogInsideFade < 0.f) {
                m_fWaterFogInsideFade = 0.f;
            }
        }
    }
    gbPlayerIsInsideWaterFog = false;

    // Full-screen quad when inside of the fog
    if (m_fWaterFogInsideFade > 0.f) {
        const auto alpha = (int32)((float)(int32)((float)m_WaterFogInsideCol.a * m_fWaterFogInsideFade) * CWeather::WaterFogFXControl);

        CPostEffects::ImmediateModeRenderStatesStore();
        CPostEffects::ImmediateModeRenderStatesSet();
        CPostEffects::DrawQuad(
            0.f, 0.f,
            (float)RsGlobal.maximumWidth, (float)RsGlobal.maximumHeight,
            m_WaterFogInsideCol.r, m_WaterFogInsideCol.g, m_WaterFogInsideCol.b, (uint8)alpha,
            nullptr
        );
        CPostEffects::ImmediateModeRenderStatesReStore();

        if (m_fWaterFogInsideFade == 1.f) {
            return;
        }
    }

    // Fog layers
    CPostEffects::ImmediateModeRenderStatesStore();
    CPostEffects::ImmediateModeRenderStatesSet();
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,    RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,  RWRSTATE(NULL));

    const int32 numLayersToRender = (int32)((1.f - m_fWaterFogInsideFade) * (float)numFogLayers);
    const RwRGBA fogColor{ m_WaterFogCol.r, m_WaterFogCol.g, m_WaterFogCol.b, m_WaterFogCol.a };

    uint32 numVerts = 0;
    const auto Flush = [&] {
        if (RwIm3DTransform(TempBufferVertices.m_3d, numVerts, nullptr, rwIM3D_VERTEXXYZ)) {
            RwIm3DRenderPrimitive(rwPRIMTYPETRILIST);
            RwIm3DEnd();
        }
    };

    for (int32 box = 0; box < numFogBoxes; box++) {
        float z = ms_WaterFog.z[box];

        const float minX = (float)ms_WaterFog.minX[box];
        const float minY = (float)ms_WaterFog.minY[box];
        const float maxX = (float)ms_WaterFog.maxX[box];
        const float maxY = (float)ms_WaterFog.maxY[box];

        // 2 triangles
        const float xs[6] = { minX, maxX, maxX, minX, maxX, minX };
        const float ys[6] = { minY, minY, maxY, minY, maxY, maxY };

        for (int32 layer = 0; layer < numLayersToRender; layer++) {
            for (int32 i = 0; i < 6; i++) {
                auto* const vtx = &TempBufferVertices.m_3d[numVerts];
                const CVector pos{ xs[i], ys[i], z };
                RxObjSpace3DVertexSetPos(vtx, &pos);
                RxObjSpace3DVertexSetPreLitColor(vtx, &fogColor);

                if (++numVerts == 0x7FE) {
                    Flush();
                    numVerts = 0;
                }
            }
            z += layerHeight;
        }
    }

    if (numVerts > 0) {
        Flush();
    }

    CPostEffects::ImmediateModeRenderStatesReStore();
}

// 0x6E6EF0
// Oracle-proven rewrite (world_oracle_test): the exe uses the sine LUT, its own phase constants (0x872008..0x872020) and keeps every intermediate on the x87 stack (=> `double`s);
// the normal starts as (-d, -d, 1) (NOT 0 + d: the sign of a zero matters), the colour multiplier sums the normal as (z + y) + x and the glare as (z*k + y*k) + k*x
void CWaterLevel::CalculateWavesOnlyForCoordinate(
    int32 x, int32 y,
    float bigWavesAmplitude,
    float smallWavesAmplitude,
    float& outWave,
    float& colorMult,
    float& glare,
    CVector& vecNormal
)
{
    static auto& SIN_LUT = StaticRef<std::array<float, 256>>(0xBB3E00);

    // `neg` for negative values (INT_MIN stays negative), then (v - (v >> 31)) >> 1 & 7
    const auto Abs = [](int32 v) { return v < 0 ? (int32)(0u - (uint32)v) : v; };
    const int32 ax = Abs(x), ay = Abs(y);
    const double waveMult = (double)faWaveMultipliersX[(((ax - (ax >> 31)) >> 1) & 7)] * (double)faWaveMultipliersY[(((ay - (ay >> 31)) >> 1) & 7)] * (double)CWeather::Wavyness;
    const double fX = (double)ax, fY = (double)ay;

    constexpr float PHASE_TO_LUT = 40.7436638f; // 0x85A778
    const auto TimeStep = [](uint32 period) { return (CTimer::m_snTimeInMilliseconds - m_nWaterTimeOffset) % period; };
    const auto LutAt    = [&](double v) { return (double)SIN_LUT[(int32)v & 0xFF]; };

    // Wave 1
    {
        const double phase = (double)TimeStep(5000) * (double)0.00125663704f /* 0x872020 */ + (fY + fX) * (double)0.0981747732f /* 0x87201C */;
        const double scaled = phase * (double)PHASE_TO_LUT;
        outWave = (float)(LutAt(scaled) * (double)2.0f * waveMult * (double)bigWavesAmplitude + (double)outWave);
        const double d = LutAt(scaled + 64.0) * (double)2.0f * waveMult * (double)bigWavesAmplitude * (double)0.0981747732f;
        const float  n = (float)-d;
        vecNormal.x = n;
        vecNormal.y = n;
        vecNormal.z = 1.0f;
    }
    // Wave 2
    {
        const double phase = ((double)TimeStep(3500) * (double)0.00179519586f /* 0x872018 */ + fY * (double)0.120830491f /* 0x872014 */) + fX * (double)0.241660982f /* 0x872010 */;
        const double scaled = phase * (double)PHASE_TO_LUT;
        outWave = (float)(LutAt(scaled) * (double)1.0f * waveMult * (double)smallWavesAmplitude + (double)outWave);
        const double d = LutAt(scaled + 64.0) * (double)1.0f * waveMult * (double)smallWavesAmplitude * (double)0.241660982f;
        vecNormal.x = (float)(d + (double)vecNormal.x);
        vecNormal.y = (float)(d + (double)vecNormal.y);
    }
    // Wave 3
    {
        const double phase = (double)TimeStep(3000) * (double)0.00209439523f /* 0x87200C */ + fY * (double)0.314159274f /* 0x872008 */;
        const double scaled = phase * (double)PHASE_TO_LUT;
        outWave = (float)(LutAt(scaled) * (double)0.5f * waveMult * (double)smallWavesAmplitude + (double)outWave);
        const double d = LutAt(scaled + 64.0) * (double)0.5f * waveMult * (double)smallWavesAmplitude * (double)0.314159274f;
        vecNormal.x = (float)(d + (double)vecNormal.x);
    }

    vecNormal.Normalise();

    constexpr float K = 0.577f; // 0x872004
    // colour multiplier: ((z + y) + x) * K, max(., 0) (NaN passes), * 0.65f (0x8D3924) + 0.27f (0x8D3920)
    const double g1 = (((double)vecNormal.z + (double)vecNormal.y) + (double)vecNormal.x) * (double)K;
    colorMult = (float)((g1 < 0.0 ? 0.0 : g1) * (double)0.65f + (double)0.27f);

    // glare: (z*K + y*K) + K*x, 8*g - 5 (0x859000 / 0x858C80), clamped to [0, 0.99f] (0x862CD0), * SunGlare
    const double g2 = (((double)vecNormal.z * (double)K + (double)vecNormal.y * (double)K) + (double)K * (double)vecNormal.x);
    const double v  = g2 * 8.0 - 5.0;
    const double c  = (double)0.99f < v ? (double)0.99f : (v < 0.0 ? 0.0 : v);
    glare = (float)(c * (double)CWeather::SunGlare);
}

// 0x6E5810
void CWaterLevel::MarkQuadsAndPolysToBeRendered(int32 blockX, int32 blockY, bool isInInterior) {
    using PType = PolyInfo::PType;

    // Horrible naming, sorry.
    const auto ProcessPoly = [&](PolyInfo data) {
        switch (data.Type()) {
        case PType::SINGLE_QUAD:
            WaterQuads[data.Id()].DoMarkToBeRendered(isInInterior);
            break;
        case PType::SINGLE_TRI:
            WaterTriangles[data.Id()].DoMarkToBeRendered(isInInterior);
            break;
        }
    };

    auto& blockPolyInfo = m_BlockPolyInfo[blockX][blockY];
    switch (blockPolyInfo.Type()) {
    case PType::SINGLE_QUAD:
    case PType::SINGLE_TRI:
        ProcessPoly(blockPolyInfo);
        break;
    case PType::COMBO: {
        for (auto& comboPoly : m_PolyCombos | rng::views::drop(blockPolyInfo.Id())) {
            if (comboPoly.Type() == PType::NONE) {
                break; // End of sequence
            }
            ProcessPoly(comboPoly);
        }
        break;
    }
    }
}

// 0x6E7210
void CWaterLevel::CalculateWavesOnlyForCoordinate2( // TODO: Original name didn't have a 2 in it... I'm just lazy!
    int32 x, int32 y,
    float bigWavesAmpl,
    float smallWavesAmpl,
    float* pResultHeight
) {
    static auto& SIN_LUT = StaticRef<std::array<float, 256>>(0xBB3E00);

    // NOTE: The original keeps everything but the explicitly stored values on the x87 stack (extended precision) => `double`s
    x = std::abs(x);
    y = std::abs(y);

    const double waveMult = (double)faWaveMultipliersX[(x / 2) & 7] * (double)faWaveMultipliersY[(y / 2) & 7] * (double)CWeather::Wavyness;
    const float  yf       = (float)y; // Spilled to the stack as a float

    constexpr float PHASE_TO_LUT = 40.7436638f; // 0x85A778 (= 256 / 2pi)
    const auto TimeStep = [](uint32 period) { return (CTimer::m_snTimeInMilliseconds - m_nWaterTimeOffset) % period; };
    const auto LutSin   = [&](double phase) { // `phase * PHASE_TO_LUT` is truncated by `_ftol2`
        return (double)SIN_LUT[(int32)(phase * (double)PHASE_TO_LUT) & 0xFF];
    };

    // Wave 1 (big)
    const double phase1 = (double)TimeStep(5000) * (double)0.00125663704f /* 0x872020 */ + ((double)yf + (double)x) * (double)0.0981747732f /* 0x87201C */;
    *pResultHeight = (float)(LutSin(phase1) * (double)2.0f /* 0x871FF8 */ * waveMult * (double)bigWavesAmpl + (double)*pResultHeight);
    const float afterWave1 = *pResultHeight; // Spilled as a float

    // Wave 2 (small)
    const double phase2 = ((double)TimeStep(3500) * (double)0.00179519586f /* 0x872018 */ + (double)yf * (double)0.120830491f /* 0x872014 */) + (double)x * (double)0.241660982f /* 0x872010 */;
    const double afterWave2 = LutSin(phase2) * (double)1.0f /* 0x871FFC */ * waveMult * (double)smallWavesAmpl + (double)afterWave1;
    *pResultHeight = (float)afterWave2;

    // Wave 3 (small). Note: `afterWave2` is NOT rounded to a float here (it is still on the x87 stack)
    const double phase3 = (double)TimeStep(3000) * (double)0.00209439523f /* 0x87200C */ + (double)yf * (double)0.314159274f /* 0x872008 */;
    *pResultHeight = (float)(LutSin(phase3) * (double)0.5f /* 0x872000 */ * waveMult * (double)smallWavesAmpl + afterWave2);
}

// 0x6E6CA0
void CWaterLevel::BlockHit(int32 blockX, int32 blockY) {
    if (blockX >= 0 && blockX < NUM_WATER_BLOCKS_ROWCOL && blockY >= 0 && blockY < NUM_WATER_BLOCKS_ROWCOL) {
        MarkQuadsAndPolysToBeRendered(blockX, blockY, CGame::currArea != AREA_CODE_NORMAL_WORLD);
    }

    // Blocks at the edge of the world (index 0 and 11) need to be handled both ways, the quads and polys are to be rendered, but also the general ocean plane needs to be rendered on them
    if (blockX <= 0 || blockX >= (NUM_WATER_BLOCKS_ROWCOL - 1) || blockY <= 0 || blockY >= (NUM_WATER_BLOCKS_ROWCOL - 1)) {
        if (m_NumBlocksOutsideWorldToBeRendered < (uint32)m_MaxNumBlocksOutsideWorldToBeRendered) {
            const auto idx                         = m_NumBlocksOutsideWorldToBeRendered++;
            m_BlocksToBeRenderedOutsideWorldX[idx] = blockX;
            m_BlocksToBeRenderedOutsideWorldY[idx] = blockY;
        }
    }
}

// 0x6E6D10
void CWaterLevel::ScanThroughBlocks() {
    m_NumBlocksOutsideWorldToBeRendered = 0;

    const auto frustumPts = TheCamera.GetFrustumPoints();
    CVector2D scanPts[5]{};
    for (auto i = 0; i < 5; i++) {
        scanPts[i] = CVector2D{ frustumPts[i] } / (float)WATER_BLOCK_SIZE + CVector2D{6.f, 6.f};
    }
    CWorldScan::ScanWorld(scanPts, 5, BlockHit);
}

// 0x6EDDC0
void CWaterLevel::RenderHighDetailWaterTriangle(int32 X1, int32 Y1, CRenPar P1, int32 X2, int32 Y2, CRenPar P2, int32 X3, int32 Y3, CRenPar P3) {
    // Bounding sphere of the triangle (in 2D, Z is the one of the 1st vertex). NOTE: x87 => `double` intermediates, the radius is based on `X2 - X1` only
    const CVector center{
        (float)((double)(X1 + X2 + X3) * (double)std::bit_cast<float>(0x3EAAAAAAu)), // 0x8594EC (1/3, one ULP below the nearest float to 1/3)
        (float)((double)(Y1 + Y2 + Y3) * (double)std::bit_cast<float>(0x3EAAAAAAu)),
        P1.z
    };
    const float radius = (float)((double)(X2 - X1) * (double)0.71f); // 0x872168

    // 0x420C40 and (if the mirror is active) the same for the mirrored view
    if (!TheCamera.IsSphereVisible(center, radius)) {
        return;
    }

    // Number of cells along the X axis
    const int32 numCells = (X2 - X1) / 2;
    const int32 numTris  = numCells * numCells;
    int32 numVerts = 0;
    for (int32 i = 1; i <= numCells + 1; i++) {
        numVerts += i;
    }

    if (numTris * 3 < 0x1000 && numVerts < 0x800) { // Fits into the render buffer
        for (int32 layer = 0; layer < 2; layer++) {
            RenderHighDetailWaterTriangle_OneLayer(X1, Y1, P1, X2, Y2, P2, X3, Y3, P3, layer, numTris, numVerts, numCells); // 0x6E8780
        }
        return;
    }

    // Too big => split
    SplitWaterTriangleAlongXLine(X1 + (numCells / 2) * 2, X1, Y1, P1, X2, Y2, P2, X3, Y3, P3);
}

// 0x6E6870 - cdecl
void CWaterLevel::RenderSeaBedSegment(int32 blockX, int32 blockY, float minX, float maxX, float minY, float maxY) {
    constexpr auto SEABED_Z     = -70.f;
    const auto SEABED_COLOR = CRGBA{ 0x50, 0x50, 0x50, 0xFF }; // 0xFF505050
    constexpr auto UV_SCALE     = 8.f;

    // NOTE: No `RenderIfDoesntFit` here, the original doesn't check for overflow either
    RenderBuffer::PushIndices({ 0, 1, 2, 3, 1, 2 }, true);

    // The block coordinates are converted to float first, and the sums are done in extended precision
    const auto CalcPos = [](int32 block, float frac) {
        return (float)(((double)(float)block + (double)frac) * 500.0 - 3000.0);
    };

    const auto x0 = CalcPos(blockX, minX), x1 = CalcPos(blockX, maxX);
    const auto y0 = CalcPos(blockY, minY), y1 = CalcPos(blockY, maxY);

    RenderBuffer::PushVertex({ x0, y0, SEABED_Z }, { minX * UV_SCALE, minY * UV_SCALE }, SEABED_COLOR);
    RenderBuffer::PushVertex({ x0, y1, SEABED_Z }, { minX * UV_SCALE, maxY * UV_SCALE }, SEABED_COLOR);
    RenderBuffer::PushVertex({ x1, y0, SEABED_Z }, { maxX * UV_SCALE, minY * UV_SCALE }, SEABED_COLOR);
    RenderBuffer::PushVertex({ x1, y1, SEABED_Z }, { maxX * UV_SCALE, maxY * UV_SCALE }, SEABED_COLOR);
}

// 0x6E6A10 - cdecl
void CWaterLevel::RenderDetailedSeaBedSegment(int32 blockX, int32 blockY, float minX, float maxX, float minY, float maxY) {
    constexpr auto SEABED_Z         = -70.f;
    const auto SEABED_COLOR       = CRGBA{ 0x50, 0x50, 0x50, 0xFF }; // 0xFF505050
    constexpr auto UV_SCALE         = 8.f;
    constexpr auto CELLS_PER_BLOCK  = 4.f; // 0x858B90

    // Number of cells to split the segment into (At least 1 on each axis)
    const auto spanX = (double)maxX - (double)minX;
    const auto spanY = (double)maxY - (double)minY;
    const auto numX  = std::max(1, (int32)(spanX * CELLS_PER_BLOCK));
    const auto numY  = std::max(1, (int32)(spanY * CELLS_PER_BLOCK));
    const auto widthX  = (float)spanX;
    const auto heightY = (float)spanY;

    for (int32 ix = 0; ix < numX; ix++) {
        // The start is kept in extended precision, the end is rounded to float (as the original does)
        const auto fx0 = (double)ix * widthX / (double)numX + (double)minX;
        const auto fx1 = (float)((double)(ix + 1) * widthX / (double)numX + (double)minX);

        const auto u0 = (float)(UV_SCALE * fx0);
        const auto u1 = fx1 * UV_SCALE;

        const auto x0 = (float)(((double)(float)blockX + fx0) * 500.0 - 3000.0);
        const auto x1 = (float)(((double)(float)blockX + (double)fx1) * 500.0 - 3000.0);

        for (int32 iy = 0; iy < numY; iy++) {
            const auto fy0 = (float)((double)iy * heightY / (double)numY + (double)minY);
            const auto fy1 = (float)((double)(iy + 1) * heightY / (double)numY + (double)minY);

            const auto y0 = (float)(((double)(float)blockY + (double)fy0) * 500.0 - 3000.0);
            const auto y1 = (float)(((double)(float)blockY + (double)fy1) * 500.0 - 3000.0);

            RenderBuffer::PushIndices({ 0, 1, 2, 3, 1, 2 }, true);

            RenderBuffer::PushVertex({ x0, y0, SEABED_Z }, { u0, fy0 * UV_SCALE }, SEABED_COLOR);
            RenderBuffer::PushVertex({ x0, y1, SEABED_Z }, { u0, fy1 * UV_SCALE }, SEABED_COLOR);
            RenderBuffer::PushVertex({ x1, y0, SEABED_Z }, { u1, fy0 * UV_SCALE }, SEABED_COLOR);
            RenderBuffer::PushVertex({ x1, y1, SEABED_Z }, { u1, fy1 * UV_SCALE }, SEABED_COLOR);
        }
    }
}

// 0x6EF650
void CWaterLevel::RenderWater() {
    ZoneScoped;

    if (!CGame::CanSeeWaterFromCurrArea()) {
        return;
    }

    SetCameraRange();
    DefinedState();

    const auto FlushRenderBuffer = RenderBuffer::RenderAndEmptyRenderBuffer; // Renders out and clears the (immediate mode) buffers

    //
    // Sea bed (Outside of the world only)
    //
    RenderBuffer::ClearRenderBuffer();

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,       RWRSTATE(seabd32Raster));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,           RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,            RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,           RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,   RWRSTATE(TRUE));

    for (int32 i = 0; i < (int32)m_NumBlocksOutsideWorldToBeRendered; i++) {
        const int32 blockX = m_BlocksToBeRenderedOutsideWorldX[i];
        const int32 blockY = m_BlocksToBeRenderedOutsideWorldY[i];

        // Is the block close enough to the camera to use the detailed version?
        const auto& camPos = TheCamera.GetPosition();
        const auto  blockCenterX = ((double)blockX + 0.5) * (double)WATER_BLOCK_SIZE - 3000.0;
        const auto  blockCenterY = (float)(((double)blockY + 0.5) * (double)WATER_BLOCK_SIZE - 3000.0); // Rounded to float, but X isn't!
        const auto  distX = (double)camPos.x - blockCenterX;
        const auto  distY = (double)camPos.y - (double)blockCenterY;
        const bool  isNear = std::sqrt(distX * distX + distY * distY) < (double)DETAILEDSEABEDDIST;

        const auto RenderSegment = [&](float minX, float maxX, float minY, float maxY) {
            if (isNear) {
                RenderDetailedSeaBedSegment(blockX, blockY, minX, maxX, minY, maxY);
            } else {
                RenderSeaBedSegment(blockX, blockY, minX, maxX, minY, maxY);
            }
        };

        // Edge blocks of the world only have a thin strip rendered here (rest is rendered by the water polys)
        float minX = 0.f, maxX = 1.f;
        float minY = 0.f, maxY = 1.f;
        bool  renderStripX = true; // Out of the world: the whole block is rendered with this call
        bool  renderStripY = false;

        if (blockX >= 0 && blockY >= 0 && blockX < NUM_WATER_BLOCKS_ROWCOL && blockY < NUM_WATER_BLOCKS_ROWCOL) { // In the world
            renderStripX = false;
            if (blockX == 0) {
                maxX         = 0.04f;
                renderStripX = true;
            } else if (blockX == 11) {
                minX         = 0.96f;
                maxX         = 1.f;
                renderStripX = true;
            }
            if (blockY == 0) {
                maxY         = 0.04f;
                renderStripY = true;
            } else if (blockY == 11) {
                minY         = 0.96f;
                maxY         = 1.f;
                renderStripY = true;
            }
        }

        if (renderStripX) {
            RenderSegment(minX, maxX, 0.f, 1.f);
        }
        if (renderStripY) {
            RenderSegment(0.f, 1.f, minY, maxY);
        }
    }

    FlushRenderBuffer();

    //
    // Update texture shifts, and water colors
    //
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,   RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, RWRSTATE(WaterTextureAddressing));

    // The intermediate results are kept in extended precision by the original
    const double flowScrollX = (double)CTimer::GetTimeStep() * (double)m_CurrentFlow.x * (double)TextureFlowScrollMult;
    TextureScrollSecondU = (float)(0.08f * flowScrollX + (double)TextureScrollSecondU);

    const double flowScrollY = (double)CTimer::GetTimeStep() * (double)m_CurrentFlow.y * (double)TextureFlowScrollMult;
    TextureScrollSecondV = (float)(0.08f * flowScrollY + (double)TextureScrollSecondV);

    if (TextureScrollSecondU > 1.f) {
        TextureScrollSecondU -= 1.f;
    }
    if (TextureScrollSecondV > 1.f) {
        TextureScrollSecondV -= 1.f;
    }

    TextureScrollFirstU = (float)(flowScrollX * 0.04f + (double)TextureScrollFirstU);
    TextureScrollFirstV = (float)(flowScrollY * 0.04f + (double)TextureScrollFirstV);

    if (TextureScrollFirstU > 1.f) {
        TextureScrollFirstU -= 1.f;
    }
    if (TextureScrollFirstV > 1.f) {
        TextureScrollFirstV -= 1.f;
    }

    constexpr auto PI_F = std::numbers::pi_v<float>;

    const double angle1 = (double)(float)(CTimer::GetTimeInMS() & 0xFFF) * (2.f * PI_F / 4096.f);
    TextureShiftSecondU = (float)(x87::sin(angle1) * (double)CWeather::Wavyness * 0.08f + (double)TextureScrollSecondU);
    TextureShiftSecondV = (float)(x87::cos(angle1) * (double)CWeather::Wavyness * 0.08f + (double)TextureScrollSecondV);

    const float angle2 = (float)(CTimer::GetTimeInMS() & 0x1FFF) * (PI_F / 4096.f);
    TextureShiftFirstU = TextureScrollFirstU;
    const double cosAngle2d = x87::cos((double)angle2);
    const float  cosAngle2  = (float)cosAngle2d; // Original stores it as float too
    TextureShiftFirstV = (float)(cosAngle2d * 0.024f + (double)TextureScrollFirstV);

    constexpr auto RAND_NORM = 1.f / 32767.f; // 0x858C7C (Not 1 / RAND_MAX, but that is the original value)

    const int32 rand1 = rand();
    TextureShiftThirdU = (float)((double)(float)rand1 * RAND_NORM * (double)TextureRandomShiftMult);
    const int32 rand2 = rand();
    TextureShiftThirdU = (float)(x87::sin((double)angle2) * (double)TextureJitterMult + (double)TextureShiftThirdU);
    TextureShiftThirdV = (float)((double)(float)rand2 * RAND_NORM * (double)TextureRandomShiftMult + (double)cosAngle2 * (double)TextureJitterMult);

    // Water colors
    WaterColor.r = (uint8)(int32)CTimeCycle::m_CurrentColours.m_fWaterRed;
    WaterColor.g = (uint8)(int32)CTimeCycle::m_CurrentColours.m_fWaterGreen;
    WaterColor.b = (uint8)(int32)CTimeCycle::m_CurrentColours.m_fWaterBlue;
    // NOTSA: `WaterColor.a` isn't touched here (And not used, see `WaterLayerAlpha`)
    WaterColorTriangle = WaterColor;

    const auto secondLayerAlpha = (int32)(CTimeCycle::m_CurrentColours.m_fWaterAlpha * 0.5f);
    WaterLayerAlpha[1] = secondLayerAlpha;

    // BUG: Division by zero if `secondLayerAlpha == 256`
    const auto firstLayerAlpha = (secondLayerAlpha << 8) / (0x100 - secondLayerAlpha);
    WaterLayerAlpha[0] = firstLayerAlpha <= 0xFF ? (uint32)firstLayerAlpha : 0xFFu;

    //
    // Water polys
    //
    RenderBuffer::ClearRenderBuffer();

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(waterclear256Raster));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,     RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,      RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,     RWRSTATE(rwBLENDINVSRCALPHA));

    for (int32 i = 0; i < (int32)NumWaterTriangles; i++) {
        auto& tri = WaterTriangles[i];
        if (!tri.bToBeRendered) {
            continue;
        }
        const auto v0 = tri.GetVertex(0);
        const auto v1 = tri.GetVertex(1);
        const auto v2 = tri.GetVertex(2);
        RenderWaterTriangle(
            v0.x, v0.y, v0.rp,
            v1.x, v1.y, v1.rp,
            v2.x, v2.y, v2.rp
        );
        tri.bToBeRendered = false;
    }

    for (int32 i = 0; i < (int32)NumWaterQuads; i++) {
        auto& quad = WaterQuads[i];
        if (!quad.bToBeRendered) {
            continue;
        }

        if (bRainbowQuads) {
            const auto CalcChannel = [](int32 n) {
                return (uint8)(int32)((float)n * 0.0625f * 255.f);
            };
            WaterColor.r = CalcChannel(i & 0xF);
            WaterColor.g = CalcChannel((i / 16) & 0xF);
            WaterColor.b = CalcChannel((i / 256) & 0xF);
            WaterColor.a = (uint8)(int32)CTimeCycle::m_CurrentColours.m_fWaterAlpha;
            WaterColorTriangle = WaterColor;
        }

        const auto v0 = quad.GetVertex(0);
        const auto v1 = quad.GetVertex(1);
        const auto v2 = quad.GetVertex(2);
        const auto v3 = quad.GetVertex(3);
        RenderWaterRectangle(
            v0.x, v1.x, v0.y, v2.y, // NOTE: Y2 comes from the 3rd vertex (not the 4th), see 0x6EFE26
            v0.rp, v1.rp, v2.rp, v3.rp
        );
        quad.bToBeRendered = false;
    }

    // Blocks outside of the world: flat, "empty" water (Same params for each vertex)
    for (int32 i = 0; i < (int32)m_NumBlocksOutsideWorldToBeRendered; i++) {
        const int16 blockX = m_BlocksToBeRenderedOutsideWorldX[i];
        const int16 blockY = m_BlocksToBeRenderedOutsideWorldY[i];
        if (blockX >= 0 && blockX < NUM_WATER_BLOCKS_ROWCOL && blockY >= 0 && blockY < NUM_WATER_BLOCKS_ROWCOL) { // In the world => already rendered above
            continue;
        }

        const CRenPar par{ 0.f, 1.f, 0.f, 0, 0 };
        const auto CalcPos = [](int32 block) { return (int32)((float)block - 3000.f); };
        RenderWaterRectangle(
            CalcPos(blockX * WATER_BLOCK_SIZE), CalcPos(blockX * WATER_BLOCK_SIZE + WATER_BLOCK_SIZE),
            CalcPos(blockY * WATER_BLOCK_SIZE), CalcPos(blockY * WATER_BLOCK_SIZE + WATER_BLOCK_SIZE),
            par, par, par, par
        );
    }

    FlushRenderBuffer();

    RenderBoatWakes();
    DefinedState();
}

void CWaterLevel::SyncWater() {
    m_nWaterTimeOffset = CTimer::GetTimeInMS();
}

// NOTSA
bool CWaterLevel::IsPointUnderwaterNoWaves(const CVector& point) {
    float level{};
    if (GetWaterLevelNoWaves(point, &level, nullptr, nullptr))
        return level > point.z;
    return false;
}

bool CWaterLevel::GetWaterLevel(const CVector& pos, float& outWaterLevel, bool touchingWater, CVector* normals) {
    return GetWaterLevel(pos.x, pos.y, pos.z, outWaterLevel, touchingWater, normals);
}

// 0x6E5A40
uint32 CWaterLevel::AddWaterLevelVertex(int32 X, int32 Y, CRenPar P) {
    // Make sure point is inside world bounds
    if (CVector2D pt{ (float)X, (float)Y }; WORLD_BOUNDS.DoConstrainPoint(pt)) {
        X = (int32)pt.x;
        Y = (int32)pt.y;

        P = {};
    }

    // Try finding a vertex with the same coords, and use that
    for (auto&& [id, vtx] : rngv::enumerate(m_aVertices | rng::views::take(NumWaterVertices))) {
        if (vtx.x == X && vtx.y == Y && vtx.rp.z == P.z) {
            return id;
        }
    }

    const auto idx = NumWaterVertices++;
    m_aVertices[idx] = { (int16)X, (int16)Y, P };
    return idx;
}

struct SortableVtx {
    SortableVtx(int32 x, int32 y, const CRenPar& rp) :
        idx{ CWaterLevel::AddWaterLevelVertex(x, y, rp) },
        x{ CWaterLevel::m_aVertices[idx].x },
        y{ CWaterLevel::m_aVertices[idx].y }
    {
    }

    uint32 idx;
    int32  x, y;
};

//! NOTSA
//! Sort vertices in clockwise order (With a few assumptions)
template<size_t N>
auto DoVtxSortAndGetRange(SortableVtx (&verts)[N]) {
    const auto VertexComparator = [&](SortableVtx& a, SortableVtx& b) {
        if (a.y == b.y) {
            return a.x < b.x; // Sort by x if y is the same
        }
        return a.y < b.y; // Otherwise, sort by y
    };

    rng::sort(verts, VertexComparator);

    // Return a range of vertex indices that can be passed to the constructor of `CWaterPolygon`
    return verts | rng::views::transform([](auto& vtx) {
        return vtx.idx;
    });
}

// 0x6E7EF0
void CWaterLevel::AddWaterLevelQuad(int32 X1, int32 Y1, CRenPar P1, int32 X2, int32 Y2, CRenPar P2, int32 X3, int32 Y3, CRenPar P3, int32 X4, int32 Y4, CRenPar P4, uint32 Flags) {
    if ((X1 == X2 && X1 == X3 && X1 == X4) || (Y1 == Y2 && Y1 == Y3 && Y1 == Y4)) {
        return;
    }

    // Seemingly only axis aligned rectangles can be used as quads
    // NOTSA: to verify the above.
    assert(X1 == X2 || X1 == X3 || X1 == X4 || X2 == X3 || X2 == X4 || X3 == X4);
    assert(Y1 == Y2 || Y1 == Y3 || Y1 == Y4 || Y2 == Y3 || Y2 == Y4 || Y3 == Y4);

    SortableVtx verts[]{
        {X1,  Y1, P1},
        { X2, Y2, P2},
        { X3, Y3, P3},
        { X4, Y4, P4},
    };

    // Now actually create the quad
    WaterQuads[NumWaterQuads++] = CWaterQuad{
        (Flags & 1) == 0,
        (Flags & 2) != 0,
        DoVtxSortAndGetRange(verts)
    };
}

// 0x6E7D40
void CWaterLevel::AddWaterLevelTriangle(int32 X1, int32 Y1, CRenPar P1, int32 X2, int32 Y2, CRenPar P2, int32 X3, int32 Y3, CRenPar P3, uint32 Flags) {
    if ((X1 == X2 && X1 == X3) || (Y1 == Y2 && Y1 == Y3)) {
        return;
    }

    // Sorting seemingly always cares about only 2 of 3 vertices, looking at water.dat, in every single case 2 of 3 vertices have the same y coordinate,
    // I assume that's a limitation, and at least 2 of 3 vertices need to fall on the same x/y axis.
    // NOTSA: to verify the above.
    assert(X1 == X2 || X1 == X3 || X2 == X3);
    assert(Y1 == Y2 || Y1 == Y3 || Y2 == Y3);

    SortableVtx verts[]{
        {X1,  Y1, P1},
        { X2, Y2, P2},
        { X3, Y3, P3},
    };

    int16_t indices[3];
    if (verts[0].y == verts[1].y) {
        if (verts[0].x < verts[1].x) {
            indices[0] = verts[0].idx;
            indices[1] = verts[1].idx;
        } else {
            indices[0] = verts[1].idx;
            indices[1] = verts[0].idx;
        }
        indices[2] = verts[2].idx;
    } else if (verts[0].y == verts[2].y) {
        if (verts[0].x >= verts[2].x) {
            indices[0] = verts[2].idx;
            indices[1] = verts[0].idx;
        } else {
            indices[0] = verts[0].idx;
            indices[1] = verts[2].idx;
        }
        indices[2] = verts[1].idx;
    } else {
        if (verts[1].x >= verts[2].x) {
            indices[0] = verts[2].idx;
            indices[1] = verts[1].idx;
        } else {
            indices[0] = verts[1].idx;
            indices[1] = verts[2].idx;
        }
        indices[2] = verts[0].idx;
    }

    // Now actually create the triangle
    WaterTriangles[NumWaterTriangles++] = CWaterTriangle{
        (Flags & 1) == 0,
        (Flags & 2) != 0,
        indices | rng::views::all
    };
}

// 0x6E5750
void CWaterLevel::AddPolyToBlock(int32 blockX, int32 blockY, uint32 polyId, uint32 type) {
    using PType = PolyInfo::PType;

    auto& block = m_BlockPolyInfo[blockX][blockY];
    switch (block.Type()) {
    case PType::NONE: { // Block was empty => just a single poly
        block = PolyInfo{ (uint16)polyId, (PType)type };
        break;
    }
    case PType::SINGLE_QUAD:
    case PType::SINGLE_TRI: { // Block had a single poly => turn it into a combo of the old and the new
        const auto first = NumWaterZonePolys;
        m_PolyCombos[first + 0] = block;
        m_PolyCombos[first + 1] = PolyInfo{ (uint16)polyId, (PType)type };
        m_PolyCombos[first + 2] = PolyInfo{}; // End of sequence
        NumWaterZonePolys       = first + 3;
        block                   = PolyInfo{ (uint16)first, PType::COMBO };
        break;
    }
    default: { // Already a combo => overwrite the terminator (it's always the last entry!) and add a new one
        const auto first = NumWaterZonePolys;
        m_PolyCombos[first - 1] = PolyInfo{ (uint16)polyId, (PType)type };
        m_PolyCombos[first]     = PolyInfo{};
        NumWaterZonePolys       = first + 1;
        break;
    }
    }
}

// 0x6E7B30
void CWaterLevel::FillQuadsAndTrianglesList() {
    const auto VertX = [](uint16 idx) { return (float)m_aVertices[idx].x; };
    const auto VertY = [](uint16 idx) { return (float)m_aVertices[idx].y; };

    for (int32 blockX = 0; blockX < NUM_WATER_BLOCKS_ROWCOL; blockX++) {
        const float minX = (float)(blockX * WATER_BLOCK_SIZE) - 3000.f; // 0x859A94
        const float maxX = minX + 500.f;                                // 0x858B58
        for (int32 blockY = 0; blockY < NUM_WATER_BLOCKS_ROWCOL; blockY++) {
            const float minY = (float)(blockY * WATER_BLOCK_SIZE) - 3000.f;
            const float maxY = minY + 500.f;

            // Quads (Note: The original re-reads the count every iteration)
            for (int32 i = 0; i < (int32)NumWaterQuads; i++) {
                const auto& quad = WaterQuads[i];
                if (VertX(quad.verts[1]) > minX
                    && VertX(quad.verts[0]) < maxX
                    && VertY(quad.verts[2]) > minY
                    && VertY(quad.verts[0]) < maxY
                ) {
                    AddPolyToBlock(blockX, blockY, i, 1);
                }
            }

            // Triangles
            const int32 numTris = (int32)NumWaterTriangles;
            for (int32 i = 0; i < numTris; i++) {
                const auto& tri  = WaterTriangles[i];
                const auto  y0   = m_aVertices[tri.verts[0]].y;
                const auto  y2   = m_aVertices[tri.verts[2]].y;
                const float yMin = (float)std::min(y0, y2);
                const float yMax = (float)std::max(y0, y2);
                if (VertX(tri.verts[1]) > minX
                    && VertX(tri.verts[0]) < maxX
                    && yMax > minY
                    && yMin < maxY
                ) {
                    AddPolyToBlock(blockX, blockY, i, 2);
                }
            }
        }
    }
}

// 0x6E9C80
void CWaterLevel::SetCameraRange() {
    if (DontUpdateCameraRange) {
        return;
    }

    const auto& cmpos = TheCamera.GetPosition();

    const auto CalcMin = [](float p) { return 2 * (int32)std::floor((p - (float)DETAILEDWATERDIST) / 2.f); };
    const auto CalcMax = [](float p) { return 2 * (int32)std::ceil((p + (float)DETAILEDWATERDIST) / 2.f); };

    CameraRangeMinX = CalcMin(cmpos.x);
    CameraRangeMaxX = CalcMax(cmpos.x);

    CameraRangeMinY = CalcMin(cmpos.y);
    CameraRangeMaxY = CalcMax(cmpos.y);
}

// 0x6EAB50
void CWaterLevel::HandleBeachToysStuff() {
    /* nothing special (10 lines), but it uses 3 static variables, and they aren't used anywhere else, so I won't bother */
}

template<size_t NumVerts>
CWaterVertex CWaterPolygon<NumVerts>::GetVertex(uint16 idx) const {
    return CWaterLevel::m_aVertices[verts[idx]];
}
