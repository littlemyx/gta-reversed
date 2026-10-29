#pragma once

#include "Base.h"
#include "Vector.h"
#include "RGBA.h"
#include "Vector.h"
#include "Vector2D.h"

#include <extensions/utility.hpp>

struct CRenPar {
    float z{};                        // Z pos of this thing. x, y can be found in the containing vertex, see `CWaterVertex`.
    float bigWaves{}, smallWaves{};   // Height of waves
    int8  flowX{}, flowY{};           // Fixed-point float. Divide by 64
};

// 0x6E5280 - `(1 - t) * a + b * t` per component (the exe's blend form, see `lerpBlend` in common.h)
static CRenPar lerpBlend(CRenPar rp1, CRenPar rp2, float t) {
    return {
        lerpBlend(rp1.z, rp2.z, t),
        lerpBlend(rp1.bigWaves, rp2.bigWaves, t),
        lerpBlend(rp1.smallWaves, rp2.smallWaves, t),
        0, 0
    };
}

struct CWaterVertex {
    int16  x;
    int16  y;
    CRenPar rp;
};

template<size_t NumVerts>
struct CWaterPolygon { CWaterPolygon() = default; /* detached `WaterQuads` / `WaterTriangles` are value-initialised */ // NOTSA - I hate duplicate code
    CWaterPolygon(bool isInvisible, bool isLimitedDepth, auto vertIdxs) :
        bInvisible{isInvisible},
        bLimitedDepth{isLimitedDepth}
    {
        rng::copy(vertIdxs, verts);
    }

    uint16 verts[NumVerts];

    bool bToBeRendered : 1{};
    bool bInvisible : 1{};
    bool bLimitedDepth : 1{};

    auto GetVertex(uint16 idx) const -> CWaterVertex;

    //! 0x6E5BB0 (quad) / 0x6E5E90 (triangle) - Original is a `cdecl` with `this` as the first stack argument (so it can't be hooked as a member)
    //! Returns whether (x, y) is inside the polygon (and, if limited depth, `z` is close enough to the water). Specialized for `CWaterQuad` and `CWaterTriangle`, see below.
    bool GetWaterLevel(float x, float y, float z, float* outWaterLevel, float* outBigWaves, float* outSmallWaves) const;
    auto IsInInterior()        const { return GetVertex(0).rp.z > 950.f; }

    auto DoMarkToBeRendered(bool isInInterior) {
        if (!bInvisible && isInInterior == IsInInterior()) {
            bToBeRendered = true;
        }
    }
};
using CWaterTriangle = CWaterPolygon<3>;
VALIDATE_SIZE(CWaterTriangle, 0x8);

using CWaterQuad = CWaterPolygon<4>;
VALIDATE_SIZE(CWaterQuad, 0xA);

template<> bool CWaterQuad::GetWaterLevel(float x, float y, float z, float* outWaterLevel, float* outBigWaves, float* outSmallWaves) const;
template<> bool CWaterTriangle::GetWaterLevel(float x, float y, float z, float* outWaterLevel, float* outBigWaves, float* outSmallWaves) const;

class CWaterLevel {
 public:
    /* Missing (in no particular order):
    m_ElementsOnQuadsAndTrianglesList
    m_NumBlocksOutsideWorldToBeRendered
    m_QuadsAndTrianglesInEachBlock
    m_WaterFogCol
    m_WaterFogDensity
    m_WaterFogHourOfDayEnd
    m_WaterFogHourOfDayStart
    m_WaterFogInsideCol
    m_fWaterFogHeight
    m_fWaterFogInsideFadeSpeed
    m_nNumOfWaterQuads
    m_nNumOfWaterTriangles
    */
    static inline NOTSA_GLOBAL(m_nWaterConfiguration, 0xC228A0, (uint32), {});
    static inline NOTSA_GLOBAL(m_nWaterTimeOffset, 0xC228A4, (uint32), {});
    static inline NOTSA_GLOBAL(m_bWaterFog, 0x8D37D4, (bool), { true });
    static inline NOTSA_GLOBAL(m_bWaterFogScript, 0x8D37D5, (bool), { true });
    static inline NOTSA_GLOBAL(m_WaterFogDensity, 0x8D37E0, (int32), { 26 });
    static inline NOTSA_GLOBAL(m_WaterFogCol, 0x8D37E8, (CRGBA), { std::bit_cast<CRGBA>(0x5FFFFFFU) });                 // Color of the fog layers (outside of it)
    static inline NOTSA_GLOBAL(m_WaterFogInsideCol, 0x8D37EC, (CRGBA), { std::bit_cast<CRGBA>(0x28FFFFFFU) });           // Color of the full-screen quad when inside the fog
    static inline NOTSA_GLOBAL(m_fWaterFogInsideFadeSpeed, 0x8D37F0, (float), { 0.04f });    // Default: 0.04
    static inline NOTSA_GLOBAL(m_fWaterFogInsideFade, 0xC228EC, (float), {});         // [0, 1] How much the inside-fog quad is faded in
    static inline NOTSA_GLOBAL(m_fWaterFogInsideTimer, 0xC228F0, (float), {});        // Time (in game ticks) left until the inside fade starts fading out

    static inline NOTSA_GLOBAL(faWaveMultipliersX, 0x8D38C8, (std::array<float, 8>), { 1.0f, 0.85f, 0.73f, 0.77f, 0.75f, 0.8f, 0.73f, 0.8f });
    static inline NOTSA_GLOBAL(faWaveMultipliersY, 0x8D38E8, (std::array<float, 8>), { 0.75f, 0.9f, 0.95f, 0.82f, 0.7f, 0.75f, 0.9f, 1.0f });
    static inline NOTSA_GLOBAL(ms_WakePointAlphaMults, 0x8D390C, (std::array<float, 5>), { 0.4f, 1.0f, 0.2f, 1.0f, 0.4f }); // 5 entries: RenderWakeSegment reads [partIdx + 1] for partIdx 0..3 (aliases.json "extend")

    static inline NOTSA_GLOBAL(waterclear256Raster, 0xC228A8, (RwRaster*), {});
    static inline NOTSA_GLOBAL(texWaterclear256, 0xC228AC, (RwTexture*), {});

    static inline NOTSA_GLOBAL(seabd32Raster, 0xC228B0, (RwRaster*), {});
    static inline NOTSA_GLOBAL(texSeabd32, 0xC228B4, (RwTexture*), {});

    static inline NOTSA_GLOBAL(waterwakeRaster, 0xC228B8, (RwRaster*), {});
    static inline NOTSA_GLOBAL(texWaterwake, 0xC228BC, (RwTexture*), {});

    static inline NOTSA_GLOBAL(NumWaterTriangles, 0xC22884, (uint32), {});
    static inline NOTSA_GLOBAL(NumWaterQuads, 0xC22888, (uint32), {});
    static inline NOTSA_GLOBAL(NumWaterVertices, 0xC2288C, (uint32), {});
    static inline NOTSA_GLOBAL(NumWaterZonePolys, 0xC215F0, (uint32), {});

    static inline NOTSA_GLOBAL(m_aVertices, 0xC22910, (std::array<CWaterVertex, 1021>), {});

    static inline NOTSA_GLOBAL(CameraRangeMaxY, 0xC1F950, (int32), {});
    static inline NOTSA_GLOBAL(CameraRangeMinY, 0xC1F954, (int32), {});

    static inline NOTSA_GLOBAL(CameraRangeMaxX, 0xC1F958, (int32), {});
    static inline NOTSA_GLOBAL(CameraRangeMinX, 0xC1F95C, (int32), {});

    static inline NOTSA_GLOBAL(DETAILEDWATERDIST, 0x8D37D0, (int32), { 48 }); // Default: 48
    static inline NOTSA_GLOBAL(bSplitBigPolys, 0x8D37F4, (bool), { true });     // Default: true
    static inline auto  BigPolySize = 168; // NOTSA variable, but value is OG.

    static inline NOTSA_GLOBAL(TextureShiftFirstV, 0xC21178, (float), {});
    static inline NOTSA_GLOBAL(TextureShiftFirstU, 0xC2117C, (float), {});

    static inline NOTSA_GLOBAL(TextureShiftSecondV, 0xC21180, (float), {});
    static inline NOTSA_GLOBAL(TextureShiftSecondU, 0xC21184, (float), {});

    // Third layer, applied with random jitter every frame by `RenderWater`. Not used by any reversed code (yet) => names are a guess (by analogy with the above)
    static inline NOTSA_GLOBAL(TextureShiftThirdV, 0xC21170, (float), {});
    static inline NOTSA_GLOBAL(TextureShiftThirdU, 0xC21174, (float), {});

    // Scrolling accumulators (flow-driven), updated by `RenderWater`. Initial value: 0.5
    static inline NOTSA_GLOBAL(TextureScrollSecondU, 0x8D3824, (float), { 0.5f });
    static inline NOTSA_GLOBAL(TextureScrollSecondV, 0x8D3828, (float), { 0.5f });
    static inline NOTSA_GLOBAL(TextureScrollFirstU, 0x8D382C, (float), { 0.5f });
    static inline NOTSA_GLOBAL(TextureScrollFirstV, 0x8D3830, (float), { 0.5f });

    static inline NOTSA_GLOBAL(TextureJitterMult, 0x8D3834, (float), { 0.1f });           // Default: 0.1
    static inline NOTSA_GLOBAL(TextureRandomShiftMult, 0x8D3928, (float), { 0.01f });      // Default: 0.01
    static inline NOTSA_GLOBAL(TextureFlowScrollMult, 0x8D392C, (float), { 0.04f });       // Default: 0.04
    static inline NOTSA_GLOBAL(WaterTextureAddressing, 0x8D3930, (uint32), { 1 });     // Default: 1 (rwTEXTUREADDRESSWRAP)
    static inline NOTSA_GLOBAL(DETAILEDSEABEDDIST, 0x8D3934, (float), { 600.0f });          // Default: 600. Sea bed blocks closer than this are drawn using `RenderDetailedSeaBedSegment`

    // Read (and added to the wake segment UVs) by `RenderWakeSegment` (0x6EA445), never written by the exe => 0.0f
    static inline NOTSA_GLOBAL(WakeTextureOffsetU, 0xC228D4, (float), {});
    static inline NOTSA_GLOBAL(WakeTextureOffsetV, 0xC228D8, (float), {});

    static inline NOTSA_GLOBAL(bRainbowQuads, 0xC228DC, (bool), {});                // Debug leftover? If set each water quad is colored based on it's index

    struct WaterFog {
        std::array<int16, 70> minX;
        std::array<int16, 70> minY;
        std::array<int16, 70> maxX;
        std::array<int16, 70> maxY;
        std::array<float, 70> z;
    };
    static inline NOTSA_GLOBAL(ms_WaterFog, 0xC21188, (WaterFog), {});
    static inline NOTSA_GLOBAL(gWaterFogIndex, 0xC228E4, (uint32), {});
    static inline NOTSA_GLOBAL(m_fWaterFogHeight, 0x8D37E4, (float), { 12.0f });
    static inline NOTSA_GLOBAL(gbPlayerIsInsideWaterFog, 0xC228E8, (bool), {});


    // In reality the alpha component isn't used and instead `WaterLayerAlpha` is used
    static inline NOTSA_GLOBAL(WaterColorTriangle, 0xC21168, (CRGBA), {});
    static inline NOTSA_GLOBAL(WaterColor, 0xC2116C, (CRGBA), {});

    static inline NOTSA_GLOBAL(WaterLayerAlpha, 0x8D3808, (std::array<uint32, 3>), { 160, 90, 255 }); // 3 layers: the exe also reads [2] (0x6E8BE1)

    static inline NOTSA_GLOBAL(m_CurrentFlow, 0xC22890, (CVector2D), {});
    static inline NOTSA_GLOBAL(m_CurrentDesiredFlow, 0xC22898, (CVector2D), {});

    static constexpr int32 WATER_BLOCK_SIZE                       = 500;
    static constexpr int32 NUM_WATER_BLOCKS_ROWCOL                = 6000 / WATER_BLOCK_SIZE; // 6000 => map size
    static inline    int32  m_MaxNumBlocksOutsideWorldToBeRendered = 70; // NOTSA: We just want a variable for the debug tool. Value may never be higher than the below array's size.
    static inline    NOTSA_GLOBAL(m_NumBlocksOutsideWorldToBeRendered, 0xC215EC, (uint32), {});
    static inline    NOTSA_GLOBAL(m_BlocksToBeRenderedOutsideWorldX, 0xC21560, (std::array<int16, 70>), {});
    static inline    NOTSA_GLOBAL(m_BlocksToBeRenderedOutsideWorldY, 0xC214D0, (std::array<int16, 70>), {});

    // NOTSA: Color of wake segment parts (So we can make rainbows)
    struct WakeSegmentPartColor {
        float r{ 1.f }, g{ 1.f }, b{ 1.f };
    }; static WakeSegmentPartColor WakeSegmentPartColors[4]; // NOTSA: defined at the end of the header (clang: default member initializers of a nested type inside the enclosing class)

    //! Used to describe a block/combo's content
    struct PolyInfo {
    public:
        enum class PType {
            NONE,        //< Nothingness
            SINGLE_QUAD, //< A single quad (ID => Index into `WaterQuads`)
            SINGLE_TRI,  //< A single tri (ID => Index into `WaterTriangles`)
            COMBO        //< A combination of both (ID => Index of first entry in `m_PolyCombos`)
        };

    public:
#ifdef __clang__
        constexpr PolyInfo() : m_id(0), m_type(0) {} // NOTSA: clang rejects the bit-field DMIs being needed inside the enclosing class (m_BlockPolyInfo below); not used with cl.exe (a user ctor makes m_BlockPolyInfo / m_PolyCombos dynamically initialised there)
#else
        PolyInfo() = default;
#endif
        PolyInfo(uint16 id, PType type) : m_id(id), m_type((uint16)type) {} // NOTSA

        auto Id()   const { return m_id; }
        auto Type() const { return (PType)(m_type); }

    private:
#ifdef __clang__
        uint16 m_id   : 14;
        uint16 m_type : 2;  //< See PType
#else
        uint16 m_id   : 14{};
        uint16 m_type : 2{};  //< See PType
#endif
    };

    //! A block might have a simple shape (tri/quad) or a combo (That is just a combination of quads and triangles)
    //! AKA `m_QuadsAndTrianglesInEachBlock`
    static inline NOTSA_GLOBAL(m_BlockPolyInfo, 0xC21B70, (notsa::mdarray<PolyInfo, NUM_WATER_BLOCKS_ROWCOL, NUM_WATER_BLOCKS_ROWCOL>), {});

    //! Triangles used for blocks (Or poly combos)
    static inline NOTSA_GLOBAL(WaterTriangles, 0xC22854, (std::array<CWaterTriangle, 6>), {});

    //! Quads used for blocks (Or poly combos)
    static inline NOTSA_GLOBAL(WaterQuads, 0xC21C90, (std::array<CWaterQuad, 301>), {});

    //! Contains sequences of poly's, each seq. is used for a block. The end of a sequence is indicated by a `PolyInfo` with it's type set to `NONE`.
    //! AKA `m_QuadsAndTrianglesList`
    static inline NOTSA_GLOBAL(m_PolyCombos, 0xC215F8, (std::array<PolyInfo, 700>), {});

    // NOTSA Section - Used for debugging //
    
    //! Stop `SetCameraRange()` from running
    static inline bool DontUpdateCameraRange = false;

    struct DebugWaterColor {
        enum {
            TRI,
            RECT
        };

        bool  active{};
        CRGBA color{};
    }; static DebugWaterColor DebugWaterColors[2];

    static inline bool DontRenderYSplitTri = false;

public:
    static void InjectHooks();

    static void Shutdown();
    static void AddWaveToResult(float x, float y, float* pfWaterLevel, float fUnkn1, float fUnkn2, CVector* pVecNormal);
    static void RenderWaterTriangle(int32 a1, int32 a2, CRenPar a3, int32 a4, int32 a5, CRenPar a6, int32 a7, int32 a8, CRenPar a9);

    // NOTSA
    struct WaterLayerTexInfo {
        CVector2D shift;
        float     scale; // UV units per world unit (the exe MULTIPLIES by 0.08f / 0.04f, it never divides by a texture size)
    };
    /*!
    * @addr notsa
    * @brief Get texture info for a given layer
    */
    static auto GetWaterLayerTexInfo(int32 WaterLayer) -> WaterLayerTexInfo;

    // NOTSA
    struct TexUV {
        float     scale;     // UV units per world unit
        CVector2D pos;       // Pos of the 0th vertex (top left) on the texture 
        CVector2D baseShift; // Shift from the 0th vertex
    };
    /*!
    * @addr notsa
    * @brief Calculate texture UV mapping coords at a given position for a given layer
    */
    static auto GetTextureUV(int32 X1, int32 Y1, int32 Y3, int32 WaterLayer) -> TexUV;

    /*!
    * @addr notsa
    * @brief Choose color to use for rendering
    *
    * @param real       The real color (As it should be originally)
    * @param debug      Debug color
    * @param WaterLayer The water layer for which this color is. If the choosen color is the `real` the associated alpha will be applied.
    *
    * @return The color that should be used for rendering
    */
    static CRGBA GetWaterColorForRendering(CRGBA real, DebugWaterColor debug, int32 WaterLayer);

    static void RenderFlatWaterTriangle_OneLayer(int32 X1, int32 Y1, CRenPar P1, int32 X2, int32 Y2, CRenPar P2, int32 X3, int32 Y3, CRenPar P3, int32 WaterLayer);
    static void RenderFlatWaterTriangle(int32 a1, int32 a2, CRenPar a3, int32 a4, int32 a5, CRenPar a6, int32 a7, int32 a8, CRenPar a9);
    static void RenderBoatWakes();
    static void SplitWaterTriangleAlongXLine(int32 a7, int32 a1, int32 a2, CRenPar a4, int32 a5, int32 a6, CRenPar arg18, int32 a8, int32 a9, CRenPar a10);

    static void RenderWaterRectangle(int32 minX, int32 maxX, int32 Y1, int32 Y2, CRenPar P1, CRenPar P2, CRenPar P3, CRenPar P4);
    static void RenderFlatWaterRectangle(int32 minX, int32 maxX, int32 Y1, int32 Y2, CRenPar P1, CRenPar P2, CRenPar P3, CRenPar P4);
    static void RenderFlatWaterRectangle_OneLayer(int32 minX, int32 maxX, int32 Y1, int32 Y2, CRenPar P1, CRenPar P2, CRenPar P3, CRenPar P4, int32 WaterLayer);
    static void RenderHighDetailWaterRectangle(int32 minX, int32 maxX, int32 Y1, int32 Y2, CRenPar P1, CRenPar P2, CRenPar P3, CRenPar P4);
    static void SplitWaterRectangleAlongXLine(int32 a1, int32 a2, int32 a3, int32 a4, int32 a5, CRenPar a6, CRenPar a7, CRenPar a8, CRenPar a9);
    static void SplitWaterRectangleAlongYLine(int32 splitAtY, int32 minX, int32 maxX, int32 Y1, int32 Y2, CRenPar P1, CRenPar P2, CRenPar P3, CRenPar P4);

    static void PreRenderWater();
    static bool GetWaterDepth(const CVector& vecPos, float* pOutWaterDepth, float* pOutWaterLevel, float* pOutGroundLevel);

    //! The surface info (of the hit col. shape B) `GetGroundLevel` can output
    struct ColData {
        uint8 surfaceType; // CColPoint::m_nSurfaceTypeB
        uint8 pieceType;   // CColPoint::m_nPieceTypeB
    };
    //! 0x6EA8A0 - Finds the ground (buildings and dummies only) below `pos` (looking `radius` above and below it)
    static bool GetGroundLevel(const CVector& pos, float* outGroundZ, ColData* outColData, float radius);
    static bool TestLineAgainstWater(CVector start, CVector end, CVector* outHitPos); // 0x6E61B0 - Tests the line against the z = 0 plane, only considers quads (!)
    static bool GetWaterLevel(float x, float y, float z, float& pOutWaterLevel, uint8 bTouchingWater, CVector* pVecNormals);
    static bool LoadDataFile();
    static void LoadTextures();
    static void WaterLevelInitialise();
    static void SetUpWaterFog(int32 a1, int32 a2, int32 a3, int32 a4);
    static void RenderWakeSegment(const CVector2D& a1, const CVector2D& a2, const CVector2D& a3, const CVector2D& a4, const float& widthA, const float& widthB, const float& alphaA, const float& alphaB, const float& wakeZ);
    static void FindNearestWaterAndItsFlow();
    static bool GetWaterLevelNoWaves(CVector pos, float * pOutWaterLevel, float * fUnkn1 = nullptr, float * fUnkn2 = nullptr);
    static void RenderWaterFog();
    static void CalculateWavesOnlyForCoordinate(int32 x, int32 y, float bigWavesAmplitude, float smallWavesAmplitude, float& outWave, float& colorMult, float& glare, CVector& vecNormal);
    static void MarkQuadsAndPolysToBeRendered(int32 blockX, int32 blockY, bool isInInterior);
    static void BlockHit(int32 X, int32 Y);

    //! 0x6E7210 - NOTE: The argument order is the one of the exe (the old declaration had `pResultHeight` in the middle)
    static void CalculateWavesOnlyForCoordinate2(
        int32 x, int32 y,
        float bigWavesAmpl,
        float smallWavesAmpl,
        float* pResultHeight // in/out variable => in is the "water level" from `GetWaterLevelNoWaves`/out is the Z coordinate of the wave
    );

    static float CalculateWavesOnlyForCoordinate2_Direct( // TODO: Once the OG function is reversed, we should use this instead of it (once we've verified that the reversed version works as expected)
        int32 x, int32 y,
        float waterLevel, // "water level" from `GetWaterLevelNoWaves`
        float bigWavesAmpl,
        float smallWavesAmpl
    ) {
        CalculateWavesOnlyForCoordinate2(x, y, bigWavesAmpl, smallWavesAmpl, &waterLevel);
        return waterLevel; // Result of the above function is stored in this variable.
    }

    static void ScanThroughBlocks();
    static void SplitWaterTriangleAlongYLine(int32 a0, int32 a1, int32 a2, CRenPar a3, int32 a4, int32 a5, CRenPar a6, int32 a7, int32 a8, CRenPar a9);
    static void RenderHighDetailWaterTriangle(int32 X1, int32 Y1, CRenPar P1, int32 X2, int32 Y2, CRenPar P2, int32 X3, int32 Y3, CRenPar P3);
    static void RenderWater();

    //! Render a (part of a) block of the sea bed as a single quad
    static void RenderSeaBedSegment(int32 blockX, int32 blockY, float minX, float maxX, float minY, float maxY);

    //! Same as above, but the quad is subdivided into cells (4 per block)
    static void RenderDetailedSeaBedSegment(int32 blockX, int32 blockY, float minX, float maxX, float minY, float maxY);
    static void SyncWater();

    static bool IsPointUnderwaterNoWaves(const CVector& point);
    static bool GetWaterLevel(const CVector& pos, float& outWaterLevel, bool touchingWater, CVector* normals = nullptr);

    static uint32 AddWaterLevelVertex(int32 X, int32 Y, CRenPar P);

    static void AddWaterLevelQuad(int32 X1, int32 Y1, CRenPar P1, int32 X2, int32 Y2, CRenPar P2, int32 X3, int32 Y3, CRenPar P3, int32 X4, int32 Y4, CRenPar P4, uint32 Flags);
    static void AddWaterLevelTriangle(int32 X1, int32 Y1, CRenPar P1, int32 X2, int32 Y2, CRenPar P2, int32 X3, int32 Y3, CRenPar P3, uint32 Flags);

    static void FillQuadsAndTrianglesList();

    //! 0x6E5750 (NOTSA name) - Adds a quad (`type` = 1) / triangle (`type` = 2) with index `polyId` to the poly info of the given block (turning it into a combo if needed)
    static void AddPolyToBlock(int32 blockX, int32 blockY, uint32 polyId, uint32 type);

    //! 0x6E91D0 - Renders one layer of the tessellated rectangle. The last 4 arguments are computed by `RenderHighDetailWaterRectangle` (`numTris` and `numVerts` are unused)
    static void RenderHighDetailWaterRectangle_OneLayer(int32 minX, int32 maxX, int32 Y1, int32 Y2, CRenPar P1, CRenPar P2, CRenPar P3, CRenPar P4, int32 WaterLayer, int32 numTris, int32 numVerts, int32 numCellsX, int32 numCellsY);

    //! 0x6E8780 - Renders one layer of the tessellated triangle. The last 3 arguments are computed by `RenderHighDetailWaterTriangle` (`numTris` and `numVerts` are unused)
    static void RenderHighDetailWaterTriangle_OneLayer(int32 X1, int32 Y1, CRenPar P1, int32 X2, int32 Y2, CRenPar P2, int32 X3, int32 Y3, CRenPar P3, int32 WaterLayer, int32 numTris, int32 numVerts, int32 numCells);

    static void SetCameraRange();
    static void HandleBeachToysStuff();
    static void UpdateFlow();

    /* Missing (In no particular order):
    static void AddWaveToResult(float x, float y, float z, float* pLevel, uint8 bTouchingWater, CVector* normalVec);
    AddWaveToResult(int32, int32, float*, float, float)
    BlockHit(int32, int32)
    CalculateWavesForCoordinate(int32, int32, float, float, float*, float*, float*, CVector*)
    ChangeWaterConfiguration(int32)
    CreateBeachToy(const CVector&, eBeachToy)
    FindNearestWaterAndItsFlow()
    FixVertexOnToLine(CWaterVertex*, CWaterVertex*, CWaterVertex*, float*)
    GetGroundLevel_WS(const CVector&, float*, ColData*, float)
    RenderAndEmptyRenderBuffer()
    AddToQuadsAndTrianglesList(int32, int32, int32, uint32)
    AddWaterLevelQuad(int32, int32, CRenPar, int32, int32, CRenPar, int32, int32, CRenPar, int32, int32, CRenPar, uint32)
    AddWaterLevelTriangle(int32, int32, CRenPar, int32, int32, CRenPar, int32, int32, CRenPar, uint32)
    IsLocationOutOfWorldBounds_WS(const CVector&, int32)
    MarkQuadsAndPolysToBeRendered(int32, int32, bool)
    RenderBoatWakes()

    RenderFlatWaterTriangle(int32, int32, CRenPar, int32, int32, CRenPar, int32, int32, CRenPar)

    RenderFlatWaterRectangle(int32, int32, int32, int32, CRenPar, CRenPar, CRenPar, CRenPar)
    RenderFlatWaterRectangle_OneLayer(int32, int32, int32, int32, CRenPar, CRenPar, CRenPar, CRenPar, int32)

    RenderHighDetailWaterRectangle_OneLayer(int32, int32, int32, int32, CRenPar, CRenPar, CRenPar, CRenPar, int32, int32, int32, int32, int32)

    RenderHighDetailWaterTriangle(int32, int32, CRenPar, int32, int32, CRenPar, int32, int32, CRenPar)

    RenderShipsOnHorizon()
    RenderTransparentWater()
    RenderWakeSegment(CVector2D&, CVector2D&, CVector2D&, CVector2D&, float&, float&, float&, float&, float&)
    HandleBeachToysStuff()
    IsLocationOutOfWorldBounds_WS(const CVector&, int32) // WS = World Space
    MarkQuadsAndPolysToBeRendered(int32, int32, bool)
    RenderWaterFog()
    RenderWaterRectangle(int32, int32, int32, int32, CRenPar, CRenPar, CRenPar, CRenPar)
    RenderWaterTriangle(int32, int32, CRenPar, int32, int32, CRenPar, int32, int32, CRenPar)
    ScanThroughBlocks()
    SetUpWaterFog(int32, int32, int32, int32)
    SplitWaterRectangleAlongXLine(int32, int32, int32, int32, int32, CRenPar, CRenPar, CRenPar, CRenPar)
    SplitWaterRectangleAlongYLine(int32, int32, int32, int32, int32, CRenPar, CRenPar, CRenPar, CRenPar)
    TestQuadToGetWaterLevel(CWaterQuad*, float, float, float, float*, float*, float*)
    TestTriangleToGetWaterLevel(CWaterTriangle*, float, float, float, float*, float*, float*)
    */
};

// NOTSA: out-of-class definitions of the debug statics
inline CWaterLevel::WakeSegmentPartColor CWaterLevel::WakeSegmentPartColors[4]{};
inline CWaterLevel::DebugWaterColor CWaterLevel::DebugWaterColors[2]{};
