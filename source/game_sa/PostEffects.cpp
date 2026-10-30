#include "StdInc.h"

#include "PostEffects.h"
#include "CustomBuildingDNPipeline.h"
#include "Clouds.h"

NOTSA_GLOBAL(cc_vertices, 0xC400D8, (std::array<RwIm2DVertex, 4>), {});
NOTSA_GLOBAL(cc_indices, 0x8D5174, (std::array<RwImVertexIndex, 12>), { 0, 1, 2, 0, 2, 3, 0, 1, 2, 0, 2, 3 }); // { 0, 1, 2, 0, 2, 3, 0, 1, 2, 0, 2, 3 };

NOTSA_GLOBAL(hpX, 0xC3FE08, (std::array<int32, 180>), {});
NOTSA_GLOBAL(hpY, 0xC3FB38, (std::array<int32, 180>), {});
NOTSA_GLOBAL(hpS, 0xC3F868, (std::array<int32, 180>), {}); // speed

static inline NOTSA_GLOBAL(s_DayNightBalanceParamOld, 0xC3F860, (float), {});

static constexpr auto GRAIN_TEXTURE_DIM = 256u; // 256x256

void CPostEffects::InjectHooks() {
    RH_ScopedClass(CPostEffects);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Initialise, 0x704630);
    RH_ScopedInstall(Close, 0x7010C0);
    RH_ScopedInstall(DoScreenModeDependentInitializations, 0x7046D0);
    RH_ScopedInstall(SetupBackBufferVertex, 0x7043D0);
    RH_ScopedInstall(Update, 0x7046A0);
    RH_ScopedInstall(DrawQuad, 0x700EC0);
    RH_ScopedInstall(FilterFX_StoreAndSetDayNightBalance, 0x7034B0);
    RH_ScopedInstall(FilterFX_RestoreDayNightBalance, 0x7034D0);
    RH_ScopedInstall(ImmediateModeFilterStuffInitialize, 0x703CC0);
    RH_ScopedInstall(ImmediateModeRenderStatesSet, 0x700D70);
    RH_ScopedInstall(ImmediateModeRenderStatesStore, 0x700CC0, {.Locked = true}); // EAX is overriden when unhooked for some reason
    RH_ScopedInstall(ImmediateModeRenderStatesReStore, 0x700E00);
    RH_ScopedInstall(ScriptCCTVSwitch, 0x7011B0);
    RH_ScopedInstall(ScriptDarknessFilterSwitch, 0x701170);
    RH_ScopedInstall(ScriptHeatHazeFXSwitch, 0x701160);
    RH_ScopedInstall(ScriptInfraredVisionSwitch, 0x701140);
    RH_ScopedInstall(ScriptNightVisionSwitch, 0x701120);
    RH_ScopedInstall(ScriptResetForEffects, 0x7010F0);
    RH_ScopedInstall(UnderWaterRipple, 0x7039C0);
    RH_ScopedInstall(HeatHazeFXInit, 0x701450);
    RH_ScopedInstall(HeatHazeFX, 0x701780);
    RH_ScopedInstall(IsVisionFXActive, 0x7034F0);
    RH_ScopedInstall(NightVision, 0x7011C0);
    RH_ScopedInstall(NightVisionSetLights, 0x7012E0);
    RH_ScopedInstall(SetFilterMainColour, 0x703520);
    RH_ScopedInstall(InfraredVision, 0x703F80);
    RH_ScopedInstall(InfraredVisionSetLightsForDefaultObjects, 0x701430);
    RH_ScopedInstall(InfraredVisionStoreAndSetLightsForHeatObjects, 0x701320);
    RH_ScopedInstall(InfraredVisionRestoreLightsForHeatObjects, 0x701410);
    RH_ScopedInstall(Fog, 0x704150);
    RH_ScopedInstall(CCTV, 0x702F40);
    RH_ScopedInstall(Grain, 0x7037C0);
    RH_ScopedInstall(SpeedFX, 0x7030A0);
    RH_ScopedInstall(DarknessFilter, 0x702F00);
    RH_ScopedInstall(ColourFilter, 0x703650);
    RH_ScopedInstall(Radiosity, 0x702080);
    RH_ScopedInstall(SetSpeedFXManualSpeedCurrentFrame, 0x700BE0);
    RH_ScopedInstall(Render, 0x7046E0);
}

// 0x704630
void CPostEffects::Initialise() {
    SetupBackBufferVertex();
    if (m_pGrainRaster = RwRasterCreate(GRAIN_TEXTURE_DIM, GRAIN_TEXTURE_DIM, 32, rwRASTERFORMAT8888 | rwRASTERTYPETEXTURE)) {
        auto* pixels = RwRasterLock(m_pGrainRaster, 0, rwRASTERLOCKWRITE);
        // exe 0x704669-0x704684: 0x10000 iterations, one rand() per 32-bit pixel (written to all 4 bytes) = the whole 256x256x4 raster
        for (auto i = 0u; i < sq(GRAIN_TEXTURE_DIM) * 4; i += 4) {
            pixels[i + 0] = pixels[i + 1] = pixels[i + 2] = pixels[i + 3] = static_cast<uint8>(CGeneral::GetRandomNumber());
        }

        RwRasterUnlock(m_pGrainRaster);
    }
}

// 0x7010C0
void CPostEffects::Close() {
    RwRasterDestroy(m_pGrainRaster);
    if (pRasterFrontBuffer) {
        RwRasterDestroy(std::exchange(pRasterFrontBuffer, nullptr));
    }
}

// 0x7046D0
void CPostEffects::DoScreenModeDependentInitializations() {
    ImmediateModeFilterStuffInitialize();
    HeatHazeFXInit();
}

// 0x7043D9 / 0x703EB5: the exe sizes the front-buffer raster as 2^(ftol(ln(w) / ln(2)) + 1) = the next power of two strictly ABOVE w (1024 -> 2048, 1 -> 2), NOT std::bit_ceil
// (1024 -> 1024). ln(2) is computed as fldln2 * log2(2.0) (fyl2x) and every step is rounded at the CURRENT x87 precision control (PC24 after D3D CreateDevice): the quotient
// of a power of two is the exact integer k there, so ftol gives k and the size is 2^(k+1); it is also what makes non-powers of two round up (1000 -> 1024).
// ExeRasterLog2 is the ftol(...) part (_ftol on the extended quotient = truncation toward zero, `fistp qword` low dword on overflow / NaN like the CRT), the pow(2.0, n) part
// is exact for every integer n in the UCRT as well.
#if NOTSA_X87_ASM
__declspec(naked) static int __cdecl ExeRasterLog2(int) {
    __asm {
        fldln2
        fld1
        fadd  st(0), st(0)                  // 2.0 (the exe loads the constant at 0x86F990)
        fyl2x                               // ln(2) * log2(2)
        fldln2
        fild  dword ptr [esp + 4]
        fyl2x                               // ln(2) * log2(w)
        fdiv  st(0), st(1)                  // fdiv st(1) (D8 F1)
        sub   esp, 12
        fnstcw word ptr [esp]
        movzx eax, word ptr [esp]
        or    eax, 0xC00
        mov   word ptr [esp + 2], ax
        fldcw word ptr [esp + 2]
        fistp qword ptr [esp + 4]
        fldcw word ptr [esp]
        fstp  st(0)
        mov   eax, dword ptr [esp + 4]
        add   esp, 12
        ret
    }
}
#elif defined(NOTSA_PORTABLE)
static int ExeRasterLog2(int w) { return notsa::fp::sites::ExeRasterLog2(w); }
#else
static int ExeRasterLog2(int w) { return (int)(std::log((double)w) / std::log(2.0)); }
#endif

// notsa
int32 CPostEffects::ExeRasterSize(int32 w) {
    return NOTSA_FTOL(int32, x87::pow(2.0, (double)(ExeRasterLog2(w) + 1)));
}

// 0x7043D0
void CPostEffects::SetupBackBufferVertex() {
    RwRaster* raster = RwCameraGetRaster(Scene.m_pRwCamera);

    // width/height rounded up to the next 2^n STRICTLY above (1024 -> 2048), exactly like the exe's ln/pow arithmetic
    const auto width   = ExeRasterSize(RwRasterGetWidth(raster));
    const auto height  = ExeRasterSize(RwRasterGetHeight(raster));
    const auto fwidth  = float(width);
    const auto fheight = float(height);

    if (pRasterFrontBuffer && (width != RwRasterGetWidth(pRasterFrontBuffer) || height != RwRasterGetHeight(pRasterFrontBuffer))) {
        RwRasterDestroy(std::exchange(pRasterFrontBuffer, nullptr));
    }
    if (!pRasterFrontBuffer) {
        pRasterFrontBuffer = RasterCreatePostEffects(RwRect{ .w = (int32)width, .h = (int32)height });

        if (!pRasterFrontBuffer) {
            NOTSA_LOG_ERR("Error subrastering");
        }
    }

    cc_vertices[0].x   = 0.0f;
    cc_vertices[0].y   = 0.0f;
    cc_vertices[0].z   = RwIm2DGetNearScreenZ();
    cc_vertices[0].rhw = 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera);
    cc_vertices[0].u   = 0.5f / fwidth;
    cc_vertices[0].v   = 0.5f / fheight;

    cc_vertices[1].x   = 0.0f;
    cc_vertices[1].y   = fheight;
    cc_vertices[1].z   = RwIm2DGetNearScreenZ();
    cc_vertices[1].rhw = 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera);
    cc_vertices[1].u   = 0.5f / fwidth;
    cc_vertices[1].v   = (fheight + 0.5f) / fheight;

    cc_vertices[2].x   = fwidth;
    cc_vertices[2].y   = fheight;
    cc_vertices[2].z   = RwIm2DGetNearScreenZ();
    cc_vertices[2].rhw = 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera);
    cc_vertices[2].u   = (fwidth + 0.5f) / fwidth;
    cc_vertices[2].v   = (fheight + 0.5f) / fheight;

    cc_vertices[3].x   = fwidth;
    cc_vertices[3].y   = 0.0f;
    cc_vertices[3].z   = RwIm2DGetNearScreenZ();
    cc_vertices[3].rhw = 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera);
    cc_vertices[3].u   = (fwidth + 0.5f) / fwidth;
    cc_vertices[3].v   = 0.5f / fheight;

    if (pRasterFrontBuffer) {
        DoScreenModeDependentInitializations();
    }
}

// 0x7046A0
void CPostEffects::Update() {
    ZoneScoped;

    m_bRainEnable = CWeather::Rain > 0.0f;
    if (!pRasterFrontBuffer) {
        SetupBackBufferVertex();
    }
}

// 0x700EC0
void CPostEffects::DrawQuad(float x1, float y1, float x2, float y2, uint8 red, uint8 green, uint8 blue, uint8 alpha, RwRaster* raster) {
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, raster);

    const auto color = CRGBA(red, green, blue, alpha).ToIntARGB();

    ms_imf.quad[0].x = x1;
    ms_imf.quad[0].y = y1;
    ms_imf.quad[0].z = ms_imf.screenZ;
    ms_imf.quad[0].emissiveColor = color;

    ms_imf.quad[1].x = x1 + x2;
    ms_imf.quad[1].y = y1;
    ms_imf.quad[1].z = ms_imf.screenZ;
    ms_imf.quad[1].emissiveColor = color;

    ms_imf.quad[2].x = x1;
    ms_imf.quad[2].y = y1 + y2;
    ms_imf.quad[2].z = ms_imf.screenZ;
    ms_imf.quad[2].emissiveColor = color;

    ms_imf.quad[3].x = x1 + x2;
    ms_imf.quad[3].y = y1 + y2;
    ms_imf.quad[3].z = ms_imf.screenZ;
    ms_imf.quad[3].emissiveColor = color;

    RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, ms_imf.quad.data(), 4);
}

// 0x701060
void CPostEffects::DrawQuadSetDefaultUVs() {
    DrawQuadSetUVs(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f);
}

// 0x700F90
void CPostEffects::DrawQuadSetUVs(float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4) {
    ms_imf.quad[0].u = u1;
    ms_imf.quad[0].v = v1;
    ms_imf.quad[1].u = u2;
    ms_imf.quad[1].v = v2;
    // exe 0x700F90-0x700FD6: the 3rd pair goes to the 4th vertex of the strip and the 4th pair to the 3rd (same as DrawQuadSetPixelUVs)
    ms_imf.quad[3].u = u3;
    ms_imf.quad[3].v = v3;
    ms_imf.quad[2].u = u4;
    ms_imf.quad[2].v = v4;
}

// 0x700FE0
void CPostEffects::DrawQuadSetPixelUVs(float u0, float v0, float u1, float v1, float u3, float v3, float u2, float v2) {
    const float x = 1.0f / ms_imf.sizeDrawBufferX;
    const float y = 1.0f / ms_imf.sizeDrawBufferY;

    ms_imf.quad[0].u = x * u0;
    ms_imf.quad[0].v = y * v0;

    ms_imf.quad[1].u = x * u1;
    ms_imf.quad[1].v = y * v1;

    ms_imf.quad[2].u = x * u2;
    ms_imf.quad[2].v = y * v2;

    ms_imf.quad[3].u = x * u3;
    ms_imf.quad[3].v = y * v3;
}

// 0x7034B0
void CPostEffects::FilterFX_StoreAndSetDayNightBalance() {
    if (!m_bInCutscene) {
        s_DayNightBalanceParamOld = CCustomBuildingDNPipeline::m_fDNBalanceParam;
        CCustomBuildingDNPipeline::m_fDNBalanceParam = m_VisionFXDayNightBalance;
    }
}

// 0x7034D0
void CPostEffects::FilterFX_RestoreDayNightBalance() {
    if (!m_bInCutscene) {
        CCustomBuildingDNPipeline::m_fDNBalanceParam = s_DayNightBalanceParamOld;
    }
}

// 0x703CC0
void CPostEffects::ImmediateModeFilterStuffInitialize() {
    ms_imf.screenZ          = RwIm2DGetNearScreenZ();
    ms_imf.RasterDrawBuffer = pRasterFrontBuffer;
    ms_imf.uMinTri          = 0.0f;
    ms_imf.vMinTri          = 0.0f;
    ms_imf.uMaxTri          = 2.0f;
    ms_imf.vMaxTri          = 2.0f;
    ms_imf.recipCameraZ     = 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera);
    ms_imf.sizeDrawBufferX  = RwRasterGetWidth(ms_imf.RasterDrawBuffer);
    ms_imf.sizeDrawBufferY  = RwRasterGetHeight(ms_imf.RasterDrawBuffer);

    ms_imf.triangle[0] = {
        .x   = 0.0f,
        .y   = 0.0f,
        .z   = ms_imf.screenZ,
        .rhw = ms_imf.recipCameraZ,
        .u   = 0.0f,
        .v   = 0.0f
    };
    ms_imf.triangle[1] = {
        .x   = 2.0f * (float)ms_imf.sizeDrawBufferX,
        .y   = 0.0f,
        .z   = ms_imf.screenZ,
        .rhw = ms_imf.recipCameraZ,
        .u   = 2.0f,
        .v   = 0.0f
    };
    ms_imf.triangle[2] = {
        .x   = 0.0f,
        .y   = 2.0f * (float)ms_imf.sizeDrawBufferY,
        .z   = ms_imf.screenZ,
        .rhw = ms_imf.recipCameraZ,
        .u   = 0.0f,
        .v   = 2.0f
    };

    constexpr auto DEFAULT_QUAD_COLOR = 0xFF00C800;

    ms_imf.quad[0] = {
        .x             = 0.0f,
        .y             = 0.0f,
        .z             = ms_imf.screenZ,
        .rhw           = ms_imf.recipCameraZ,
        .emissiveColor = DEFAULT_QUAD_COLOR,
        .u             = 0.0f,
        .v             = 0.0f
    };
    ms_imf.quad[1] = {
        .x             = 0.0f,
        .y             = 0.0f,
        .z             = ms_imf.screenZ,
        .rhw           = ms_imf.recipCameraZ,
        .emissiveColor = DEFAULT_QUAD_COLOR,
        .u             = 1.0f,
        .v             = 0.0f
    };
    ms_imf.quad[2] = {
        .x             = 0.0f,
        .y             = 0.0f,
        .z             = ms_imf.screenZ,
        .rhw           = ms_imf.recipCameraZ,
        .emissiveColor = DEFAULT_QUAD_COLOR,
        .u             = 0.0f,
        .v             = 1.0f
    };
    ms_imf.quad[3] = {
        .x             = 0.0f,
        .y             = 0.0f,
        .z             = ms_imf.screenZ,
        .rhw           = ms_imf.recipCameraZ,
        .emissiveColor = DEFAULT_QUAD_COLOR,
        .u             = 1.0f,
        .v             = 1.0f
    };

    const auto* frameBuffer = RwCameraGetRaster(Scene.m_pRwCamera);
    ms_imf.fFrontBufferU1 = ms_imf.fFrontBufferV1 = 0.0f;
    ms_imf.fFrontBufferU2 = SCREEN_WIDTH / (float)ExeRasterSize(RwRasterGetWidth(frameBuffer));
    ms_imf.fFrontBufferV2 = SCREEN_HEIGHT / (float)ExeRasterSize(RwRasterGetHeight(frameBuffer));
}

// 0x700D70
void CPostEffects::ImmediateModeRenderStatesSet() {
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATECULLMODE,          RWRSTATE(rwCULLMODECULLNONE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,         RWRSTATE(rwSHADEMODEGOURAUD));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,    RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERNEAREST));
}

// 0x700CC0
void CPostEffects::ImmediateModeRenderStatesStore() {
    RwRenderStateGet(rwRENDERSTATESRCBLEND,          &ms_imf.blendSrc);
    RwRenderStateGet(rwRENDERSTATEDESTBLEND,         &ms_imf.blendDst);
    RwRenderStateGet(rwRENDERSTATEFOGENABLE,         &ms_imf.bFog);
    RwRenderStateGet(rwRENDERSTATECULLMODE,          &ms_imf.cullMode);
    RwRenderStateGet(rwRENDERSTATEZTESTENABLE,       &ms_imf.bZTest);
    RwRenderStateGet(rwRENDERSTATEZWRITEENABLE,      &ms_imf.bZWrite);
    RwRenderStateGet(rwRENDERSTATESHADEMODE,         &ms_imf.shadeMode);
    RwRenderStateGet(rwRENDERSTATEVERTEXALPHAENABLE, &ms_imf.bVertexAlpha);
    RwRenderStateGet(rwRENDERSTATETEXTUREADDRESS,    &ms_imf.textureAddress);
    RwRenderStateGet(rwRENDERSTATETEXTUREFILTER,     &ms_imf.textureFilter);
}

// 0x700E00
void CPostEffects::ImmediateModeRenderStatesReStore() {
    RwRenderStateSet(rwRENDERSTATESRCBLEND,           RWRSTATE(ms_imf.blendSrc));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,          RWRSTATE(ms_imf.blendDst));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,          RWRSTATE(ms_imf.bFog));
    RwRenderStateSet(rwRENDERSTATECULLMODE,           RWRSTATE(ms_imf.cullMode));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,        RWRSTATE(ms_imf.bZTest));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,       RWRSTATE(ms_imf.bZWrite));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,          RWRSTATE(ms_imf.shadeMode));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,  RWRSTATE(ms_imf.bVertexAlpha));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,     RWRSTATE(ms_imf.textureAddress));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,      RWRSTATE(ms_imf.textureFilter));
}

// 0x700C90
RwRaster* CPostEffects::RasterCreatePostEffects(RwRect rect) {
    return RwRasterCreate(rect.w, rect.h, RwRasterGetDepth(RwCameraGetRaster(Scene.m_pRwCamera)), rwRASTERTYPEZBUFFER | rwRASTERTYPETEXTURE);
}

// 0x7011B0
void CPostEffects::ScriptCCTVSwitch(bool enable) {
    m_bCCTV = enable;
}

// 0x701170
void CPostEffects::ScriptDarknessFilterSwitch(bool enable, int32 alpha) {
    m_bDarknessFilter = enable;
    m_DarknessFilterAlpha = alpha == -1
        ? m_DarknessFilterAlphaDefault
        : std::clamp(alpha, 0, 255);
}

// 0x701160
void CPostEffects::ScriptHeatHazeFXSwitch(bool enable) {
    m_bHeatHazeFX = enable;
}

// 0x701140
void CPostEffects::ScriptInfraredVisionSwitch(bool enable) {
    if (enable) {
        m_bInfraredVision = true;
        m_bNightVision = false;
    } else {
        m_bInfraredVision = false;
    }
}

// 0x701120
void CPostEffects::ScriptNightVisionSwitch(bool enable) {
    if (enable) {
        m_bNightVision = true;
        m_bInfraredVision = false;
    } else {
        m_bNightVision = false;
    }
}

// 0x7010F0
void CPostEffects::ScriptResetForEffects() {
    m_bNightVision = false;
    m_bInfraredVision = false;
    m_bHeatHazeFX = false;
    m_bDarknessFilter = false;
    m_bCCTV = false;
    CWaterLevel::m_bWaterFogScript = true;
}

// 0x7039C0
void CPostEffects::UnderWaterRipple(CRGBA color, float xoffset, float yoffset, float strength, float speed, float freq) {
    color.a = 255;

    RwCameraEndUpdate(Scene.m_pRwCamera);
    RsCameraBeginUpdate(Scene.m_pRwCamera);
    uiTempBufferVerticesStored = uiTempBufferIndicesStored = 0;
    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, CPostEffects::pRasterFrontBuffer);

    const auto rasterWidth   = RwRasterGetWidth(CPostEffects::pRasterFrontBuffer);
    const auto rasterHeight  = RwRasterGetHeight(CPostEffects::pRasterFrontBuffer);
    const auto fRasterWidth  = float(rasterWidth);
    const auto fRasterHeight = float(rasterHeight);
    const auto recipNearClip = 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera);

    const auto EmitVertex = [&](float wave, int32 y) {
        const auto i = uiTempBufferVerticesStored;

        TempBufferVertices.m_2d[i].x             = 0.0f;
        TempBufferVertices.m_2d[i].y             = float(y);
        TempBufferVertices.m_2d[i].z             = RwIm2DGetNearScreenZ();
        TempBufferVertices.m_2d[i].rhw           = recipNearClip;
        TempBufferVertices.m_2d[i].u             = (wave + xoffset) / fRasterWidth;
        TempBufferVertices.m_2d[i].v             = float(y) / fRasterHeight;
        TempBufferVertices.m_2d[i].emissiveColor = color.ToIntARGB(); // exe 0x7039F0-0x703A6E: 0xFF << 24 | r << 16 | g << 8 | b (alpha forced to 255)

        TempBufferVertices.m_2d[i + 1].x             = float(NOTSA_FTOL(int32, 2.0f * xoffset) + rasterWidth);
        TempBufferVertices.m_2d[i + 1].y             = float(y);
        TempBufferVertices.m_2d[i + 1].z             = RwIm2DGetNearScreenZ();
        TempBufferVertices.m_2d[i + 1].rhw           = recipNearClip;
        TempBufferVertices.m_2d[i + 1].u             = (fRasterWidth + wave - xoffset) / fRasterWidth;
        TempBufferVertices.m_2d[i + 1].v             = float(y) / fRasterHeight;
        TempBufferVertices.m_2d[i + 1].emissiveColor = color.ToIntARGB(); // exe 0x7039F0-0x703A6E: 0xFF << 24 | r << 16 | g << 8 | b (alpha forced to 255)

        uiTempBufferVerticesStored += 2;
    };

    if (rasterHeight > 0) {
        auto y = 0;
        for (; y < rasterHeight; y = NOTSA_FTOL(int32, (float)y + yoffset)) {
            EmitVertex(X87R(x87::sin(freq * (float)y + speed * (float)CTimer::GetTimeInMS()) * xoffset), y);
        }
        EmitVertex(X87R(x87::sin(freq * (float)y + speed * (float)CTimer::GetTimeInMS()) * xoffset), y);
    } else {
        // unexpected path?
        EmitVertex(X87R(x87::sin(speed * (float)CTimer::GetTimeInMS()) * xoffset), 0);
    }

    if (uiTempBufferVerticesStored > 2) {
        RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, TempBufferVertices.m_2d, uiTempBufferVerticesStored);
    }
    ImmediateModeRenderStatesReStore();
}

// unused
// 0x703CB0
void CPostEffects::UnderWaterRippleFadeToFX() {
    // NOP
}

// 0x701450
void CPostEffects::HeatHazeFXInit() {
    // m_HeatHazeFXType always HEAT_HAZE_0
    if (m_HeatHazeFXType == m_HeatHazeFXTypeLast)
        return;

    switch (m_HeatHazeFXType) {
    case HEAT_HAZE_0:
        m_HeatHazeFXIntensity   = 80;
        m_HeatHazeFXRandomShift = 0;
        m_HeatHazeFXSpeedMin    = 12;
        m_HeatHazeFXSpeedMax    = 18;
        m_HeatHazeFXScanSizeX   = NOTSA_FTOL(uint32, SCREEN_STRETCH_X(47.0f));
        m_HeatHazeFXScanSizeY   = NOTSA_FTOL(uint32, SCREEN_STRETCH_Y(47.0f));
        m_HeatHazeFXRenderSizeX = NOTSA_FTOL(uint32, SCREEN_STRETCH_X(50.0f));
        m_HeatHazeFXRenderSizeY = NOTSA_FTOL(uint32, SCREEN_STRETCH_Y(50.0f));
        break;
    case HEAT_HAZE_1:
        m_HeatHazeFXIntensity   = 32;
        m_HeatHazeFXRandomShift = 0;
        m_HeatHazeFXSpeedMin    = 6;
        m_HeatHazeFXSpeedMax    = 10;
        m_HeatHazeFXScanSizeX   = NOTSA_FTOL(uint32, SCREEN_STRETCH_X(100.0f));
        m_HeatHazeFXScanSizeY   = NOTSA_FTOL(uint32, SCREEN_STRETCH_Y(52.0f));
        m_HeatHazeFXRenderSizeX = NOTSA_FTOL(uint32, SCREEN_STRETCH_X(100.0f));
        m_HeatHazeFXRenderSizeY = NOTSA_FTOL(uint32, SCREEN_STRETCH_Y(60.0f));
        break;
    case HEAT_HAZE_2:
        m_HeatHazeFXIntensity   = 32;
        m_HeatHazeFXRandomShift = 0;
        m_HeatHazeFXSpeedMin    = 4;
        m_HeatHazeFXSpeedMax    = 8;
        m_HeatHazeFXScanSizeX   = NOTSA_FTOL(uint32, SCREEN_STRETCH_X(70.0f));
        m_HeatHazeFXScanSizeY   = NOTSA_FTOL(uint32, SCREEN_STRETCH_Y(70.0f));
        m_HeatHazeFXRenderSizeX = NOTSA_FTOL(uint32, SCREEN_STRETCH_X(80.0f));
        m_HeatHazeFXRenderSizeY = NOTSA_FTOL(uint32, SCREEN_STRETCH_Y(80.0f));
        break;
    case HEAT_HAZE_3:
        m_HeatHazeFXRandomShift = 0;
        m_HeatHazeFXIntensity   = 150;
        m_HeatHazeFXSpeedMin    = 5;
        m_HeatHazeFXSpeedMax    = 8;
        m_HeatHazeFXScanSizeX   = NOTSA_FTOL(uint32, SCREEN_STRETCH_X(60.0f));
        m_HeatHazeFXScanSizeY   = NOTSA_FTOL(uint32, SCREEN_STRETCH_Y(24.0f));
        m_HeatHazeFXRenderSizeX = NOTSA_FTOL(uint32, SCREEN_STRETCH_X(62.0f));
        m_HeatHazeFXRenderSizeY = NOTSA_FTOL(uint32, SCREEN_STRETCH_Y(24.0f));
        break;
    case HEAT_HAZE_4:
        m_HeatHazeFXRandomShift = 1;
        m_HeatHazeFXIntensity   = 150;
        m_HeatHazeFXSpeedMin    = 5;
        m_HeatHazeFXSpeedMax    = 8;
        m_HeatHazeFXScanSizeX   = NOTSA_FTOL(uint32, SCREEN_STRETCH_X(60.0f));
        m_HeatHazeFXScanSizeY   = NOTSA_FTOL(uint32, SCREEN_STRETCH_Y(24.0f));
        m_HeatHazeFXRenderSizeX = NOTSA_FTOL(uint32, SCREEN_STRETCH_X(62.0f));
        m_HeatHazeFXRenderSizeY = NOTSA_FTOL(uint32, SCREEN_STRETCH_Y(24.0f));
        break;
    default:
        break;
    }

    m_HeatHazeFXTypeLast = m_HeatHazeFXType;

    for (auto i = 0; i < 180; i++) {
        // exe (0x7016A0): the scan size is only the SUBTRAHEND of the range, the minimum (+ scan size) is not added back
        hpX[i] = CGeneral::GetRandomNumberInRange(0, (int32)RwRasterGetWidth(pRasterFrontBuffer) - m_HeatHazeFXScanSizeX);
        hpY[i] = CGeneral::GetRandomNumberInRange(0, (int32)RwRasterGetHeight(pRasterFrontBuffer) - m_HeatHazeFXScanSizeY);
        hpS[i] = CGeneral::GetRandomNumberInRange(m_HeatHazeFXSpeedMin,  m_HeatHazeFXSpeedMax);
    }
}

// 0x701780
void CPostEffects::HeatHazeFX(float fIntensity, bool bAlphaMaskMode) {
    // Debug flag: if set, a single full screen quad (with only the red channel) is drawn instead of the haze quads
    NOTSA_GLOBAL_LOCAL(s_DebugDrawMask, 0xC402BB, (bool), {});

    auto* const camera = Scene.m_pRwCamera;
    auto* const camRaster = RwCameraGetRaster(camera);
    const bool  useStencil = RwRasterGetDepth(camRaster) != 16;

    const auto SetStencilState = [&](RwRenderState state, int32 value) {
        RwRenderStateSet(state, RWRSTATE(value));
    };

    if (bAlphaMaskMode) {
        RwRGBA black{ 0, 0, 0, 0 };
        RwCameraClear(camera, &black, rwCAMERACLEARZ);

        ImmediateModeRenderStatesStore();
        ImmediateModeRenderStatesSet();

        RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDSRCALPHA));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDONE));
        if (useStencil) {
            SetStencilState(rwRENDERSTATESTENCILENABLE,      TRUE);
            SetStencilState(rwRENDERSTATESTENCILFAIL,        rwSTENCILOPERATIONKEEP);
            SetStencilState(rwRENDERSTATESTENCILZFAIL,       rwSTENCILOPERATIONKEEP);
            SetStencilState(rwRENDERSTATESTENCILPASS,        rwSTENCILOPERATIONREPLACE);
            SetStencilState(rwRENDERSTATESTENCILFUNCTIONREF, 0);
            SetStencilState(rwRENDERSTATESTENCILFUNCTION,    rwSTENCILFUNCTIONALWAYS);
        }
        DrawQuad(0.0f, 0.0f, (float)RsGlobal.maximumWidth, (float)RsGlobal.maximumHeight, 0, 0, 0, 255, nullptr);
        if (useStencil) {
            SetStencilState(rwRENDERSTATESTENCILFUNCTIONREF, 1);
        }
        g_fx.Render(TheCamera.m_pRwCamera, true);
        ImmediateModeRenderStatesReStore();
    } else {
        s_DebugDrawMask = false;
    }

    fIntensity = std::clamp(fIntensity, 0.0f, 1.0f);

    HeatHazeFXInit();

    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwCameraEndUpdate(camera);
    RwRasterPushContext(pRasterFrontBuffer);
    RwRasterRenderFast(RwCameraGetRaster(camera), 0, 0);
    RwRasterPopContext();
    RsCameraBeginUpdate(camera);

    uiTempBufferVerticesStored = 0;
    uiTempBufferIndicesStored  = 0;
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));

    const auto alpha = NOTSA_FTOL(int32, (float)m_HeatHazeFXIntensity * fIntensity);
    uiTempBufferVerticesStored = 0;

    if (useStencil) {
        SetStencilState(rwRENDERSTATESTENCILPASS,        rwSTENCILOPERATIONKEEP);
        SetStencilState(rwRENDERSTATESTENCILFUNCTIONREF, 1);
        SetStencilState(rwRENDERSTATESTENCILFUNCTION,    rwSTENCILFUNCTIONEQUAL);
    }
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,  RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(pRasterFrontBuffer));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));

    const auto  frontRaster = pRasterFrontBuffer;
    const auto  screenZ     = RwIm2DGetNearScreenZ();
    const auto  recipNear   = 1.0f / RwCameraGetNearClipPlane(camera);

    if (!s_DebugDrawMask) {
        const auto scanX   = m_HeatHazeFXScanSizeX;
        const auto scanY   = m_HeatHazeFXScanSizeY;
        const auto renderX = m_HeatHazeFXRenderSizeX;
        const auto renderY = m_HeatHazeFXRenderSizeY;
        const auto halfDX  = (renderX - scanX) / 2;
        const auto halfDY  = (renderY - scanY) / 2;
        const uint32 color = ((uint32)alpha << 24) | 0xFFFFFF;

        for (auto i = 0; i < (int32)hpX.size(); i++) {
            auto sx = hpX[i];
            auto sy = hpY[i];
            auto px = sx - halfDX;
            auto py = sy - halfDY;

            if (const auto shift = m_HeatHazeFXRandomShift; shift > 0) {
                px += NOTSA_FTOL(int32, (float)(CGeneral::GetRandomNumber() & 0xFFFF) * (1.0f / 32768.0f) * (float)(shift - -shift)) + -shift;
                py += NOTSA_FTOL(int32, (float)(CGeneral::GetRandomNumber() & 0xFFFF) * (1.0f / 32768.0f) * (float)(m_HeatHazeFXRandomShift - -m_HeatHazeFXRandomShift)) + -m_HeatHazeFXRandomShift;
            }

            const auto rasterW = RwRasterGetWidth(frontRaster);
            const auto rasterH = RwRasterGetHeight(frontRaster);
            if (px < 0) {
                sx += halfDX;
                px = 0;
            }
            if (px > rasterW - renderX) {
                sx -= halfDX;
                px = rasterW - renderX;
            }
            if (py < 0) {
                sy += halfDY;
                py = 0;
            }
            if (py > rasterH - renderY) {
                sy -= halfDY;
                py = rasterH - renderY;
            }

            const auto v  = (int32)uiTempBufferVerticesStored;
            auto*      vb = &aRadiosityVertexBuffer[v];

            vb[0].x = (float)px;
            vb[0].y = (float)py;
            vb[0].u = (float)sx / (float)rasterW;
            vb[0].v = (float)sy / (float)rasterH;

            vb[1].x = (float)(px + renderX);
            vb[1].y = (float)py;
            vb[1].u = (float)(sx + scanX) / (float)rasterW;
            vb[1].v = (float)sy / (float)rasterH;

            vb[2].x = (float)px;
            vb[2].y = (float)(py + renderY);
            vb[2].u = (float)sx / (float)rasterW;
            vb[2].v = (float)(sy + scanY) / (float)rasterH;

            vb[3].x = (float)(px + renderX);
            vb[3].y = (float)(py + renderY);
            vb[3].u = (float)(sx + scanX) / (float)rasterW;
            vb[3].v = (float)(sy + scanY) / (float)rasterH;

            for (auto j = 0; j < 4; j++) {
                vb[j].z             = screenZ;
                vb[j].rhw           = recipNear;
                vb[j].emissiveColor = color;
            }

            const auto idx = (int32)uiTempBufferIndicesStored;
            aTempBufferIndices[idx + 0] = (RxVertexIndex)(v + 0);
            aTempBufferIndices[idx + 1] = (RxVertexIndex)(v + 2);
            aTempBufferIndices[idx + 2] = (RxVertexIndex)(v + 1);
            aTempBufferIndices[idx + 3] = (RxVertexIndex)(v + 1);
            aTempBufferIndices[idx + 4] = (RxVertexIndex)(v + 2);
            aTempBufferIndices[idx + 5] = (RxVertexIndex)(v + 3);
            uiTempBufferIndicesStored += 6;
            uiTempBufferVerticesStored += 4;

            // Move this haze quad
            hpY[i] -= NOTSA_FTOL(int32, CTimer::GetTimeStep() * 0.5f * (float)hpS[i]);
            if (hpY[i] < 0) {
                hpX[i] = NOTSA_FTOL(int32, (float)(CGeneral::GetRandomNumber() & 0xFFFF) * (1.0f / 32768.0f) * (float)(RwRasterGetWidth(pRasterFrontBuffer) - scanX));
                hpY[i] = RwRasterGetHeight(pRasterFrontBuffer) - scanY;
                hpS[i] = NOTSA_FTOL(int32, (float)(CGeneral::GetRandomNumber() & 0xFFFF) * (1.0f / 32768.0f) * (float)(m_HeatHazeFXSpeedMax - m_HeatHazeFXSpeedMin)) + m_HeatHazeFXSpeedMin;
            }
        }
    } else {
        // Debug: Draw the whole screen with only the red channel (the alpha mask)
        const auto v  = (int32)uiTempBufferVerticesStored;
        auto*      vb = &aRadiosityVertexBuffer[v];
        const uint32 color = ((uint32)alpha << 24) | 0xFF0000;

        vb[0].x = 0.0f;
        vb[0].y = 0.0f;
        vb[1].x = (float)RwRasterGetWidth(frontRaster);
        vb[1].y = 0.0f;
        vb[2].x = 0.0f;
        vb[2].y = (float)RwRasterGetHeight(frontRaster);
        vb[3].x = (float)RwRasterGetWidth(frontRaster);
        vb[3].y = (float)RwRasterGetHeight(frontRaster);
        for (auto j = 0; j < 4; j++) {
            vb[j].z             = screenZ;
            vb[j].rhw           = recipNear;
            vb[j].emissiveColor = color;
        }

        const auto idx = (int32)uiTempBufferIndicesStored;
        aTempBufferIndices[idx + 0] = (RxVertexIndex)(v + 0);
        aTempBufferIndices[idx + 1] = (RxVertexIndex)(v + 2);
        aTempBufferIndices[idx + 2] = (RxVertexIndex)(v + 1);
        aTempBufferIndices[idx + 3] = (RxVertexIndex)(v + 1);
        aTempBufferIndices[idx + 4] = (RxVertexIndex)(v + 2);
        aTempBufferIndices[idx + 5] = (RxVertexIndex)(v + 3);
        uiTempBufferVerticesStored += 4;
        uiTempBufferIndicesStored += 6;
    }

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(pRasterFrontBuffer));
    if (uiTempBufferVerticesStored != 0) {
        RwIm2DRenderIndexedPrimitive(
            rwPRIMTYPETRILIST,
            reinterpret_cast<RwIm2DVertex*>(aRadiosityVertexBuffer),
            uiTempBufferVerticesStored,
            aTempBufferIndices,
            uiTempBufferIndicesStored
        );
    }
    uiTempBufferVerticesStored = 0;

    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    if (useStencil) {
        SetStencilState(rwRENDERSTATESTENCILENABLE, FALSE);
    }
}

// 0x7034F0
bool CPostEffects::IsVisionFXActive() {
    return !m_bInCutscene && (m_bNightVision || m_bInfraredVision);
}

// 0x7011C0
void CPostEffects::NightVision() {
    if (m_fNightVisionSwitchOnFXCount > 0.0f) {
        m_fNightVisionSwitchOnFXCount = std::max(m_fNightVisionSwitchOnFXCount - CTimer::GetTimeStep(), 0.0f); // exe 0x7011D6-0x7011FB: zeroed only when <= 0

        ImmediateModeRenderStatesStore();
        ImmediateModeRenderStatesSet();
        RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDONE));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDONE));

        for (auto i = 0, end = NOTSA_FTOL(int32, m_fNightVisionSwitchOnFXCount); i < end; i++) {
            DrawQuad(0.0f, 0.0f, (float)ms_imf.sizeDrawBufferX, (float)ms_imf.sizeDrawBufferY, 8, 8, 8, 255, ms_imf.RasterDrawBuffer);
        }

        ImmediateModeRenderStatesReStore();
    }

    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();
    RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDZERO));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDSRCCOLOR));
    DrawQuad(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 32, 255, 32, 255, nullptr);
    ImmediateModeRenderStatesReStore();
}

// 0x7012E0
void CPostEffects::NightVisionSetLights() {
    if (m_bNightVision && !m_bInCutscene) {
        SetLightsForNightVision();
    }
}

// 0x703520
void CPostEffects::SetFilterMainColour(RwRaster* raster, RwRGBA color) {
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERNEAREST));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(pRasterFrontBuffer));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));

    RwIm2DVertexSetRealRGBA(&cc_vertices[0], color.red, color.green, color.blue, color.alpha);
    RwIm2DVertexSetRealRGBA(&cc_vertices[1], color.red, color.green, color.blue, color.alpha);
    RwIm2DVertexSetRealRGBA(&cc_vertices[2], color.red, color.green, color.blue, color.alpha);
    RwIm2DVertexSetRealRGBA(&cc_vertices[3], color.red, color.green, color.blue, color.alpha);

    RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDDESTALPHA));
    RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, cc_vertices.data(), std::size(cc_vertices), cc_indices.data(), std::size(cc_indices) / 2); // size 4 and 6

    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
}

// 0x703F80
void CPostEffects::InfraredVision(RwRGBA color, RwRGBA colorMain) {
    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();

    RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDONE));

    float radius = m_fInfraredVisionFilterRadius * 100.0f;
    int32 i = 0, j = 0; // todo: use for loop?
    float xy;
    do {
        j = i + 1;
        DrawQuadSetUVs(
            ms_imf.fFrontBufferU1, ms_imf.fFrontBufferV1,
            ms_imf.fFrontBufferU2, ms_imf.fFrontBufferV1,
            ms_imf.fFrontBufferU2, ms_imf.fFrontBufferV2,
            ms_imf.fFrontBufferU1, ms_imf.fFrontBufferV2
        );

        xy = (float)(j) * radius;
        DrawQuad(
            -xy,
            -xy,
            SCREEN_WIDTH + (xy + xy),   // exe 0x703FE0-0x704055: W + (xy + xy)
            SCREEN_HEIGHT + (xy + xy),
            color.red,
            color.green,
            color.blue,
            255,
            ms_imf.RasterDrawBuffer
        );
        i = j;
    } while (j < 4);

    DrawQuadSetDefaultUVs();

    RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDZERO));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDSRCCOLOR));

    DrawQuad(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 255, 64, 255, 255, nullptr);
    ImmediateModeRenderStatesReStore();
    SetFilterMainColour(ms_imf.RasterDrawBuffer, colorMain);
}

// 0x701430
void CPostEffects::InfraredVisionSetLightsForDefaultObjects() {
    if (m_bInfraredVision && !m_bInCutscene) {
        SetLightsForInfraredVisionDefaultObjects();
    }
}

// unused
// 0x701300
void CPostEffects::InfraredVisionSetLightsForHeatObjects() {
    if (m_bInfraredVision && !m_bInCutscene) {
        SetLightsForInfraredVisionHeatObjects();
    }
}

// 0x701320
void CPostEffects::InfraredVisionStoreAndSetLightsForHeatObjects(CPed* ped) {
    if (!m_bInfraredVision || m_bInCutscene)
        return;

    // store color
    const auto red   = m_fInfraredVisionHeatObjectCol.red;
    const auto green = m_fInfraredVisionHeatObjectCol.green;
    const auto blue  = m_fInfraredVisionHeatObjectCol.blue;
    const auto alpha = m_fInfraredVisionHeatObjectCol.alpha;

    // here we go to fuck cold carbon (Explanation: https://sampik.ru/articles/468-pochemu-piratskij-perevod-gtasa-takoj-strannyj.html)
    // gradually changing from red to blue (dead)
    if (ped->m_nPedState == PEDSTATE_DEAD) {
        const auto time = std::abs((int32)(CTimer::GetTimeInMS() - ped->m_nDeathTimeMS));
        const auto delta = (float)time * 0.0001f; // exe 0x701320: fild + fmul by the float 0.0001f (0x858FC4), not a division by 10000

        m_fInfraredVisionHeatObjectCol.red = std::max(m_fInfraredVisionHeatObjectCol.red - delta, 0.0f);
        m_fInfraredVisionHeatObjectCol.green = 0.0f;
        m_fInfraredVisionHeatObjectCol.blue = std::min(m_fInfraredVisionHeatObjectCol.blue + delta, 1.0f);
    }

    StoreAndSetLightsForInfraredVisionHeatObjects();

    // restore color
    m_fInfraredVisionHeatObjectCol.red   = red;
    m_fInfraredVisionHeatObjectCol.green = green;
    m_fInfraredVisionHeatObjectCol.blue  = blue;
    m_fInfraredVisionHeatObjectCol.alpha = alpha;
}

// 0x701410
void CPostEffects::InfraredVisionRestoreLightsForHeatObjects() {
    if (m_bInfraredVision && !m_bInCutscene) {
        RestoreLightsForInfraredVisionHeatObjects();
    }
}

// 0x704150
void CPostEffects::Fog() {
    NOTSA_GLOBAL_LOCAL(s_FogRadius, 0xC402E4, (float), {});
    NOTSA_GLOBAL_LOCAL(s_FogAngle, 0xC402DC, (float), {});

    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));

    // 0x7041B1-0x70423A: the exe tests the speed MAGNITUDE (fsqrt) against 0.06f (0x859934), not its square; the step is ts * 0.25f
    s_FogRadius = !(FindPlayerSpeed().Magnitude() > 0.06f)
        ? std::max(s_FogRadius - CTimer::GetTimeStep() / 4.0f, 0.0f)
        : std::min(s_FogRadius + CTimer::GetTimeStep() / 4.0f, 160.0f);

    // 0x704175-0x7042A1: the quad size is W * 0.6666667f / H * 0.6666667f (0x862D68), the centre is W * 0.5f and the quad is centred with `- size * 0.5f`; W/4 and H/4 are (W * 0.5f) * 0.5f
    const float quadW = SCREEN_WIDTH * (2.0f / 3.0f);
    const float quadH = SCREEN_HEIGHT * (2.0f / 3.0f);
    const float halfW = SCREEN_WIDTH * 0.5f;
    const float halfH = SCREEN_HEIGHT * 0.5f;
    const auto skyBottom = CTimeCycle::GetCurrentSkyBottomColor();
    for (auto i = 0; i < 10; i++) {
        const auto angle = DegreesToRadians(36.0f * static_cast<float>(i) + s_FogAngle);

        DrawQuad(
            X87R(x87::cos(angle) * (halfW * 0.5f + s_FogRadius)) + halfW - quadW * 0.5f,
            X87R(x87::sin(angle) * (halfH * 0.5f + s_FogRadius)) + halfH - quadH * 0.5f,
            quadW,
            quadH,
            skyBottom.r,
            skyBottom.g,
            skyBottom.b,
            11,
            RwTextureGetRaster(CClouds::ms_vc.texture)
        );
    }

    for (auto i = 0; i < 10; i++) {
        const auto angle = -DegreesToRadians(36.0f * static_cast<float>(i) + s_FogAngle);

        DrawQuad(
            X87R(x87::cos(angle) * (halfW * 0.7f + s_FogRadius)) + halfW - quadW * 0.5f,
            X87R(x87::sin(angle) * (halfH * 0.7f + s_FogRadius)) + halfH - quadH * 0.5f,
            quadW,
            quadH,
            skyBottom.r,
            skyBottom.g,
            skyBottom.b,
            11,
            RwTextureGetRaster(CClouds::ms_vc.texture)
        );
    }

    s_FogAngle += CTimer::GetTimeStep() * ExeRecip(6.0f);
    ImmediateModeRenderStatesReStore();
}

// 0x702F40
void CPostEffects::CCTV() {
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwCameraEndUpdate(Scene.m_pRwCamera);
    RwRasterPushContext(CPostEffects::pRasterFrontBuffer);
    RwRasterRenderFast(RwCameraGetRaster(Scene.m_pRwCamera), 0, 0);
    RwRasterPopContext();
    RsCameraBeginUpdate(Scene.m_pRwCamera);

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(true));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE, RWRSTATE(false));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(true));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(false));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, CPostEffects::pRasterFrontBuffer);
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(true));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));
    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();

    const auto lineHeight  = NOTSA_FTOL(uint32, 2.0f * SCREEN_STRETCH_Y(1.0f));
    const auto linePadding = 2 * lineHeight;
    const auto numLines    = NOTSA_FTOL(uint32, SCREEN_HEIGHT) / linePadding;
    for (auto i = 0u, y = 0u; i < numLines; i++, y += linePadding) {
        DrawQuad(
            0.0f,
            (float)(y),
            SCREEN_WIDTH,
            static_cast<float>(lineHeight),
            m_CCTVcol.r,
            m_CCTVcol.g,
            m_CCTVcol.b,
            255,
            pRasterFrontBuffer
        );
    }
    ImmediateModeRenderStatesReStore();
}

// 0x7037C0
void CPostEffects::Grain(int32 strengthMask, bool update) {
    NOTSA_GLOBAL_LOCAL(s_NumberOfReseeds, 0xC4031C, (uint32), {});
    if (update) {
        auto* pixels = RwRasterLock(m_pGrainRaster, 0, rwRASTERLOCKWRITE);

        // srand(CTimer::GetTimeInMS() + OS_TimeMS())
        srand(CTimer::GetCurrentTimeInCycles() / CTimer::GetCyclesPerMillisecond());
        for (auto i = 0u, reSeedCounter = 0u; i < sq(GRAIN_TEXTURE_DIM); i++) {
            if (++reSeedCounter >= 100) {
                reSeedCounter = 0;
                srand(CTimer::GetTimeInMS() + OS_TimeMS() + ++s_NumberOfReseeds);
            }
            pixels[4 * i] = pixels[4 * i + 1] = pixels[4 * i + 2] = pixels[4 * i + 3] = static_cast<uint8>(CGeneral::GetRandomNumber());
        }
        RwRasterUnlock(m_pGrainRaster);
    }

    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, RWRSTATE(rwTEXTUREADDRESSWRAP));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));

    // exe 0x7038B8-0x7038E3: the 1.5 tiling is only stretched for screens wider than 640
    const auto ux = RsGlobal.maximumWidth > 640 ? SCREEN_STRETCH_X(1.5f) : 1.5f, uy = SCREEN_HEIGHT / SCREEN_WIDTH * ux;

    ms_imf.quad[0].u = 0.0f;
    ms_imf.quad[0].v = 0.0f;
    ms_imf.quad[1].u = ux;
    ms_imf.quad[1].v = 0.0f;
    ms_imf.quad[2].u = 0.0f;
    ms_imf.quad[2].v = uy;
    ms_imf.quad[3].u = ux;
    ms_imf.quad[3].v = uy;
    DrawQuad(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 255, 255, 255, strengthMask, m_pGrainRaster);
    ms_imf.quad[0].u = 0.0f;
    ms_imf.quad[0].v = 0.0f;
    ms_imf.quad[1].u = 1.0f;
    ms_imf.quad[1].v = 0.0f;
    ms_imf.quad[2].u = 0.0f;
    ms_imf.quad[2].v = 1.0f;
    ms_imf.quad[3].u = 1.0f;
    ms_imf.quad[3].v = 1.0f;
    ImmediateModeRenderStatesReStore();
}

// 0x7030A0
void CPostEffects::SpeedFX(float speed) {
    // 0x8D5190: { threshold, count, size, jitter }[7]
    struct tSpeedFXLevel {
        float threshold;
        int32 count;
        int32 size;
        int32 jitter;
    };
    NOTSA_GLOBAL_LOCAL(s_Levels, 0x8D5190, (std::array<tSpeedFXLevel, 7>), { {
        { 0.6f, 1, 4, 0 }, { 0.7f, 2, 4, 0 }, { 0.8f, 3, 4, 0 }, { 0.9f, 3, 4, 0 },
        { 0.93f, 4, 4, 1 }, { 0.96f, 4, 4, 2 }, { 1.0f, 5, 4, 3 },
    } });
#line 1064

    const auto lookDir       = TheCamera.GetActiveCam().m_nDirectionWasLooking;
    const bool isLookingLeft = lookDir == 1;
    const bool isLookingRight = lookDir == 2;

    int32 count{}, size{}, jitter{};
    for (auto i = (int32)s_Levels.size() - 1; i >= 0; i--) {
        if (speed >= s_Levels[i].threshold) {
            count  = s_Levels[i].count;
            size   = s_Levels[i].size;
            jitter = s_Levels[i].jitter;
            break;
        }
    }
    if (isLookingLeft || isLookingRight) {
        size   = size / 2;
        jitter = 0;
    }
    if (count <= 0) {
        return;
    }

    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();

    const float fU1 = ms_imf.fFrontBufferU1;
    const float fV1 = ms_imf.fFrontBufferV1;
    const float fU2 = ms_imf.fFrontBufferU2;
    const float fV2 = ms_imf.fFrontBufferV2;

    float uJit1{}, uJit2{}, vJit1{}, vJit2{};
    if (jitter > 0) {
        const auto jitterMult = (float)jitter * 0.004f; // 0x859CE0
        const auto uRange     = fU2 * jitterMult;
        const auto vRange     = fV2 * jitterMult;
        uJit1 = uJit2 = (float)rand() * RAND_MAX_FLOAT_RECIPROCAL * uRange;
        vJit1 = vJit2 = (float)rand() * RAND_MAX_FLOAT_RECIPROCAL * vRange;
    }

    const float su = fU2 * (float)size * 0.0025f; // 0x86C340
    const float sv = (float)size * fV2 * 0.0025f;

    // Per-vertex offsets (named after the quad vertex + component they're used for)
    float u0Off = su, u2Off = su;    // added to vertices 0 and 2's U
    float u1Off = -su, u3Off = -su;  // added to vertices 1 and 3's U
    float v0Off = sv, v1Off = sv;    // added to vertices 0 and 1's V
    float v2Off = -sv, v3Off = -sv;  // added to vertices 2 and 3's V

    for (auto n = count; n != 0; n--) {
        if (isLookingLeft) {
            uJit1 = uJit2 = vJit1 = vJit2 = 0.0f;
            v0Off = u1Off = v1Off = u3Off = v3Off = v2Off = 0.0f;
        }
        if (isLookingRight) {
            uJit1 = uJit2 = vJit1 = vJit2 = 0.0f;
            u0Off = v0Off = v1Off = v3Off = u2Off = v2Off = 0.0f;
        }

        ms_imf.quad[0].u = fU1 + u0Off + uJit1;
        ms_imf.quad[0].v = fV1 + v0Off + vJit1;
        ms_imf.quad[1].u = (fU2 + u1Off) - uJit2;
        ms_imf.quad[1].v = fV1 + v1Off + vJit2;
        ms_imf.quad[2].u = fU1 + u2Off + uJit1;
        ms_imf.quad[2].v = (fV2 + v2Off) - uJit2; // NOTE: Uses the U jitter (original code does this too)
        ms_imf.quad[3].u = (fU2 + u3Off) - uJit2;
        ms_imf.quad[3].v = (fV2 + v3Off) - vJit2;

        DrawQuad(0.0f, 0.0f, (float)RsGlobal.maximumWidth, (float)RsGlobal.maximumHeight, 255, 255, 255, (uint8)m_SpeedFXAlpha, pRasterFrontBuffer);

        u0Off += su;
        v0Off += sv;
        u1Off -= su;
        v1Off += sv;
        u2Off += su;
        u3Off -= su;
        v2Off -= sv;
        v3Off -= sv;
    }

    // Reset UVs
    ms_imf.quad[0].u = 0.0f;
    ms_imf.quad[0].v = 0.0f;
    ms_imf.quad[1].u = 1.0f;
    ms_imf.quad[1].v = 0.0f;
    ms_imf.quad[2].u = 0.0f;
    ms_imf.quad[2].v = 1.0f;
    ms_imf.quad[3].u = 1.0f;
    ms_imf.quad[3].v = 1.0f;

    ImmediateModeRenderStatesReStore();
}

// 0x702F00
void CPostEffects::DarknessFilter(int32 alpha) {
    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();
    DrawQuad(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 0, alpha, nullptr);
    ImmediateModeRenderStatesReStore();
}

// 0x703650
void CPostEffects::ColourFilter(RwRGBA pass1, RwRGBA pass2) {
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERNEAREST));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(pRasterFrontBuffer));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDONE));

    RwIm2DVertexSetRealRGBA(&cc_vertices[0], pass1.red, pass1.green, pass1.blue, pass1.alpha);
    RwIm2DVertexSetRealRGBA(&cc_vertices[1], pass1.red, pass1.green, pass1.blue, pass1.alpha);
    RwIm2DVertexSetRealRGBA(&cc_vertices[2], pass1.red, pass1.green, pass1.blue, pass1.alpha);
    RwIm2DVertexSetRealRGBA(&cc_vertices[3], pass1.red, pass1.green, pass1.blue, pass1.alpha);
    RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, cc_vertices.data(), std::size(cc_vertices), cc_indices.data(), std::size(cc_indices) / 2); // size 4 and 6

    RwIm2DVertexSetRealRGBA(&cc_vertices[0], pass2.red, pass2.green, pass2.blue, pass2.alpha);
    RwIm2DVertexSetRealRGBA(&cc_vertices[1], pass2.red, pass2.green, pass2.blue, pass2.alpha);
    RwIm2DVertexSetRealRGBA(&cc_vertices[2], pass2.red, pass2.green, pass2.blue, pass2.alpha);
    RwIm2DVertexSetRealRGBA(&cc_vertices[3], pass2.red, pass2.green, pass2.blue, pass2.alpha);
    RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, cc_vertices.data(), std::size(cc_vertices), cc_indices.data(), std::size(cc_indices) / 2); // size 4 and 6

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,   RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,  RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,      RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,     RWRSTATE(rwBLENDINVSRCALPHA));
}

// 0x702080
void CPostEffects::Radiosity(int32 intensityLimit, int32 filterPasses, int32 renderPasses, int32 intensity) {
    auto curW = m_RadiosityPixelsX;
    auto curH = m_RadiosityPixelsY;

    const auto RecipNearClip = [] { return 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera); };
    auto&      vb            = aRadiosityVertexBuffer;

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERNEAREST));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(pRasterFrontBuffer));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));

    // Initial 2 vertices
    vb[0].z = vb[0].y = vb[0].x = 0.0f;
    vb[0].v = vb[0].u = 0.0f;
    vb[0].rhw = RecipNearClip();
    vb[1].z = 0.0f;
    vb[1].v = vb[1].u = 1.0f;
    if (m_bRadiosityStripCopyMode) {
        vb[1].x = (float)RsGlobal.maximumWidth;
        vb[1].y = (float)RsGlobal.maximumHeight;
    } else {
        vb[1].x = (float)curW;
        vb[1].y = (float)curH;
    }
    vb[1].rhw = RecipNearClip();
    uiTempBufferVerticesStored = 0;

    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(TRUE));

    // Filter passes: Downscale the image by 2 each pass
    if (filterPasses > 0) {
        RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));

        auto idx = (int32)uiTempBufferVerticesStored;
        for (auto n = 0; n < filterPasses; n++) {
            const auto halfW   = curW / 2;
            const auto halfH   = curH / 2;
            const auto rasterW = (float)RwRasterGetWidth(pRasterFrontBuffer);
            const auto rasterH = (float)RwRasterGetHeight(pRasterFrontBuffer);

            auto& a = vb[idx + 0];
            a.u     = (float)m_RadiosityFilterUCorrection / rasterW;
            a.v     = (float)m_RadiosityFilterVCorrection / rasterH;
            a.x = a.y = a.z = 0.0f;
            a.rhw           = RecipNearClip();

            auto& b = vb[idx + 1];
            b.u     = (float)curW / rasterW;
            b.v     = (float)curH / rasterH;
            b.x     = (float)(halfW + 1);
            b.y     = (float)(halfH + 1);
            b.z     = 0.0f;
            b.rhw   = RecipNearClip();

            a.emissiveColor = b.emissiveColor = 0xFFFFFFFF;

            idx += 2;
            uiTempBufferVerticesStored = (uint16)idx;
            curW                       = halfW;
            curH                       = halfH;
        }
    }

    RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));

    // Intensity limit quad
    {
        const auto idx = (int32)uiTempBufferVerticesStored;
        const uint32 limit = (uint32)intensityLimit;
        const auto color = ((((limit | 0xFFFF8000u) << 8) | limit) << 8) | limit;

        auto& a = vb[idx + 0];
        a.x = a.y = a.z = 0.0f;
        a.rhw           = RecipNearClip();

        auto& b = vb[idx + 1];
        b.x     = (float)(curW + 1);
        b.y     = (float)(curH + 1);
        b.z     = 0.0f;
        b.rhw   = RecipNearClip();

        a.emissiveColor = b.emissiveColor = color;
        uiTempBufferVerticesStored = (uint16)(idx + 2);
    }

    const auto Flush = [&] {
        if (uiTempBufferVerticesStored > 2) {
            RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, reinterpret_cast<RwIm2DVertex*>(aRadiosityVertexBuffer), uiTempBufferVerticesStored);
        }
    };

    if (renderPasses > 0) {
        Flush();
        uiTempBufferVerticesStored = 0;

        RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(m_bRadiosityLinearFilter ? rwFILTERLINEAR : rwFILTERMIPNEAREST));

        const auto screenW = (float)RsGlobal.maximumWidth;
        const auto screenH = (float)RsGlobal.maximumHeight;
        const auto rasterW = (float)RwRasterGetWidth(pRasterFrontBuffer);
        const auto rasterH = (float)RwRasterGetHeight(pRasterFrontBuffer);

        auto idx = (int32)uiTempBufferVerticesStored;
        for (auto n = 0; n < renderPasses; n++) {
            uint32 color;
            if (m_bRadiosityStripCopyMode) {
                color = (uint32)intensity << 24;
            } else {
                color = m_bRadiosityDebug ? 0xFFFFFFFFu : ((uint32)intensity << 24);
            }

            auto& a = vb[idx + 0];
            a.u = a.v = 0.0f;
            a.x = a.y = a.z = 0.0f;
            a.rhw           = RecipNearClip();

            auto& b = vb[idx + 1];
            b.u     = (float)curW / rasterW;
            b.v     = (float)curH / rasterH;
            b.x     = screenW;
            b.y     = screenH;
            b.z     = 0.0f;
            b.rhw   = RecipNearClip();

            a.emissiveColor = b.emissiveColor = color;

            idx += 2;
            uiTempBufferVerticesStored = (uint16)idx;
        }
    }

    Flush();
    uiTempBufferVerticesStored = 0;

    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
}

// 0x700BE0
void CPostEffects::SetSpeedFXManualSpeedCurrentFrame(float value) {
    m_fSpeedFXManualSpeedCurrentFrame = std::clamp(value, 0.0f, 1.0f);
}

// 0x7046E0
void CPostEffects::Render() {
    ZoneScoped;

    NOTSA_GLOBAL_LOCAL(s_CurrentStrength, 0xC40328, (int32), {});
    NOTSA_GLOBAL_LOCAL(s_WaterGreen, 0xC40324, (float), {});
    NOTSA_GLOBAL_LOCAL(s_WaitForOneFrame, 0xC40321, (bool), {});
    NOTSA_GLOBAL_LOCAL(s_SavePhotoToGallery, 0xC40320, (bool), {});

    if (m_bDisableAllPostEffect) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwCameraEndUpdate(Scene.m_pRwCamera);
    RwRasterPushContext(CPostEffects::pRasterFrontBuffer);
    RwRasterRenderFast(RwCameraGetRaster(Scene.m_pRwCamera), 0, 0);
    RwRasterPopContext();
    RsCameraBeginUpdate(Scene.m_pRwCamera);

#ifdef NOTSA_GLOBALS_DETACHED
    NOTSA_SCOPED_GLOBAL(s_ExtraMult, 0xC4032C, 0xC40330, 1, (float), 0.35f);
#else
    auto& s_ExtraMult = ScopedStaticRef<float>(0xC4032C, 0xC40330, 1, 0.35f); // no `static` here (the macro would add one: different object code)
#endif
#line 1371

    if (m_bFog) {
        Fog();
    }

    auto [pass1, pass2] = CTimeCycle::GetPostFxColors();
    if (m_bNightVision || m_bInfraredVision) {
        pass1.Set(64, 64, 64);
        pass2.Set(64, 64, 64);
    }
    pass1.ScaleRGB(gfLaRiotsLightMult);

    auto colorMultFactor = 1.0f;
    if (auto* player = FindPlayerPed()) {
        const auto step   = CTimer::GetTimeStep() * SCREEN_EXTRA_MULT_CHANGE_RATE;
        auto lightFromCol = player->GetLightingFromCol(false);

        if (std::abs(lightFromCol - s_ExtraMult) >= step) {
            lightFromCol = lightFromCol <= s_ExtraMult 
                ? s_ExtraMult - step
                : s_ExtraMult + step;
        }
        s_ExtraMult = std::min(lightFromCol, SCREEN_EXTRA_MULT_BASE_CAP);
        colorMultFactor += (1.0f - s_ExtraMult / SCREEN_EXTRA_MULT_BASE_CAP) * SCREEN_EXTRA_MULT_BASE_MULT;
    }

    // exe 0x704952-0x704D02: r,g,b = ftol(clamp(c * mult, 0, 255)): v > 255 (or NaN) -> 255, v <= 0 -> 0 (the plain ScaleRGB would wrap the uint8)
    const auto ScaleRGBClamped = [](CRGBA& c, float mult) {
        const auto Scale = [mult](uint8 ch) {
            auto v = (float)ch * mult;
            if (!(v < 255.0f)) {
                v = 255.0f;
            } else if (v <= 0.0f) {
                v = 0.0f;
            }
            return NOTSA_FTOL(uint8, v);
        };
        c.r = Scale(c.r);
        c.g = Scale(c.g);
        c.b = Scale(c.b);
    };
    constexpr auto COLOR1_MULT = 1.0f, COLOR2_MULT = 1.0f;
    ScaleRGBClamped(pass1, COLOR1_MULT * colorMultFactor);
    ScaleRGBClamped(pass2, COLOR2_MULT * colorMultFactor);

    if (m_bColorEnable) {
        ColourFilter(pass1, pass2);
    }

    if (m_bDarknessFilter && !m_bNightVision && !m_bInfraredVision) {
        DarknessFilter(m_DarknessFilterAlpha);
        Radiosity(m_DarknessFilterRadiosityIntensityLimit, 0, 2, 255);
    }

    if (m_bSpeedFXTestMode) {
        SetSpeedFXManualSpeedCurrentFrame(1.0f);
    }

    if (m_bSpeedFX && m_bSpeedFXUserFlag && m_bSpeedFXUserFlagCurrentFrame) {
        auto* veh = FindPlayerVehicle();
        if (m_fSpeedFXManualSpeedCurrentFrame == 0.0f) {
            if (veh && !notsa::contains({ VEHICLE_TYPE_PLANE, VEHICLE_TYPE_HELI, VEHICLE_TYPE_BOAT, VEHICLE_TYPE_TRAIN }, veh->m_nVehicleType)) {
                bool fxDrawn{};
                if (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE && veh->handlingFlags.bNosInst && veh->AsAutomobile()->m_fTireTemperature < 0.0f) {
                    const auto dir = veh->GetMoveSpeed().Dot(veh->GetForward());
                    if (dir > 0.2f) {
                        SetSpeedFXManualSpeedCurrentFrame(2.0f * dir * (veh->m_GasPedal + 1.0f));
                        SpeedFX(m_fSpeedFXManualSpeedCurrentFrame);
                        fxDrawn = true;
                    }
                }

                if (!fxDrawn && !CCutsceneMgr::ms_running) {
                    SpeedFX(FindPlayerSpeed().Magnitude());
                }
            }
        } else {
            SpeedFX(m_fSpeedFXManualSpeedCurrentFrame);
        }
    }
    SetSpeedFXManualSpeedCurrentFrame(0.0f);
    m_bSpeedFXUserFlagCurrentFrame = true;
    m_bInCutscene                  = CCutsceneMgr::ms_running || CCutsceneMgr::ms_cutsceneProcessing;

    if (m_bNightVision) {
        if (!m_bInCutscene) {
            NightVision();
            Grain(m_NightVisionGrainStrength, true);
        }
    } else {
        m_fNightVisionSwitchOnFXCount = m_fNightVisionSwitchOnFXTime;
    }

    if (m_bInfraredVision && !m_bInCutscene) {
        InfraredVision(m_InfraredVisionCol, m_InfraredVisionMainCol);
        Grain(m_InfraredVisionGrainStrength, true);
    }

    if (m_bRadiosity && !m_bDarknessFilter) {
        if (m_bRadiosityBypassTimeCycleIntensityLimit) {
            Radiosity(
                m_RadiosityIntensityLimit,
                m_RadiosityFilterPasses,
                m_RadiosityRenderPasses,
                m_RadiosityIntensity
            );
        } else {
            Radiosity(
                CTimeCycle::GetHighLightMinIntensity(),
                m_RadiosityFilterPasses,
                m_RadiosityRenderPasses,
                m_RadiosityIntensity
            );
        }
    }

    if (m_bRainEnable || s_CurrentStrength != 0) {
        const auto rain = 128.0f * CWeather::Rain;
        // exe 0x704FD2-0x705010: the second compare runs on the already incremented counter, so the strength steps toward `rain` but never overshoots it
        auto strength = s_CurrentStrength;
        if (static_cast<float>(strength) < rain) {
            s_CurrentStrength = ++strength;
        }
        if (static_cast<float>(s_CurrentStrength) > rain) {
            strength--;
        }
        s_CurrentStrength = std::max(strength, 0);

        if (!CCullZones::CamNoRain() && !CCullZones::PlayerNoRain() && CWeather::UnderWaterness <= 0.0f && CGame::CanSeeOutSideFromCurrArea() && TheCamera.GetPosition().z <= 900.0f) {
            Grain(s_CurrentStrength / 4, true);
        }
    }

    if (m_bGrainEnable) {
        Grain(m_grainStrength[0], true);
    }

    // exe 0x705099-0x70511E: the script switch (and being underwater) draws the plain full-strength haze; `<` is the exe's C0 test (true for NaN too)
    if (m_bHeatHazeFX) {
        HeatHazeFX(1.0f, false);
    } else if (CWeather::HeatHaze > 0.0f || g_fxMan.m_bHeatHazeEnabled) {
        if (!(CWeather::UnderWaterness >= m_fWaterFXStartUnderWaterness)) {
            if (CWeather::HeatHaze > 0.0f) {
                HeatHazeFX(CWeather::HeatHazeFXControl, false);
            } else if (g_fxMan.m_bHeatHazeEnabled) {
                HeatHazeFX(1.0f, true);
            }
        } else {
            HeatHazeFX(1.0f, false);
        }
    } else if (CWeather::UnderWaterness >= m_fWaterFXStartUnderWaterness) {
        HeatHazeFX(1.0f, false);
    }

    if (m_waterEnable || CWeather::UnderWaterness >= m_fWaterFXStartUnderWaterness) {
        CRGBA color{m_waterCol};
        color.r = std::min(color.r + 184, 255);
        color.g = NOTSA_FTOL(uint8, std::min(float(color.g) + s_WaterGreen + 184.0f, 255.0f));
        color.b = std::min(color.b + 184, 255);

        const auto depthDarkness = m_bWaterDepthDarkness
            ? 1.0f - std::min(CWeather::WaterDepth, m_fWaterFullDarknessDepth) / m_fWaterFullDarknessDepth
            : 1.0f; // exe 0x705154: st(0) stays 1.0 when the depth-darkness switch is off
        color.ScaleRGB(depthDarkness);

        s_WaterGreen = std::min(s_WaterGreen + CTimer::GetTimeStep(), 24.0f);

        UnderWaterRipple(
            color,
            SCREEN_STRETCH_X(s_WaterGreen * ExeRecip(24.0f)) * m_xoffset, // exe 0x705247-0x705292: ((W * k) * (green / 24)) * xoffset
            SCREEN_STRETCH_Y(m_yoffset),
            m_waterStrength,
            m_waterSpeed,
            m_waterFreq
        );
    } else {
        s_WaterGreen = 0.0f;
    }

    if (m_bCCTV) {
        CCTV();
    }

    if (CWeapon::ms_bTakePhoto && FrontEndMenuManager.m_bIsSaveDone) {
        CWeapon::ms_bTakePhoto = false;
        m_bSavePhotoFromScript = false;
    }

    if (s_WaitForOneFrame) {
        if (CWeapon::ms_bTakePhoto) {
            s_WaitForOneFrame = false; // exe 0x705309
            CTimer::Suspend();
            if (s_SavePhotoToGallery) {
                CVisibilityPlugins::RenderWeaponPedsForPC();
                CVisibilityPlugins::ResetWeaponPedsForPC();
                CFileMgr::SetDirMyDocuments();

                auto photoIdx = 0;
                do {
                    notsa::format_to_sz(gString, "Gallery\\gallery{}.jpg", ++photoIdx);
                } while (fs::exists(gString));

                JPegCompressScreenToFile(TheCamera.m_pRwCamera, gString);
                CFileMgr::SetDir("");
            }
            CTimer::Resume();

            if (!FrontEndMenuManager.m_bActivateMenuNextFrame) {
                CSpecialFX::bSnapShotActive = true;
                CSpecialFX::SnapShotFrames  = false;
            }
            s_SavePhotoToGallery   = false;
            CWeapon::ms_bTakePhoto = false;
            m_bSavePhotoFromScript = false;
        }
    } else if (CWeapon::ms_bTakePhoto) {
        if (FrontEndMenuManager.m_bSavePhotos) {
            s_SavePhotoToGallery = true;
        }
        s_WaitForOneFrame = true;
    }
}
