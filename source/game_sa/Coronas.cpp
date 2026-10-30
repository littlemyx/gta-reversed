#include "StdInc.h"
#include "Fx/FxFtol.h"
#include <reversiblebugfixes/Bugs.hpp>
#include "Coronas.h"

constexpr std::array<const char[26], eCoronaType::CORONATYPE_COUNT> aCoronaSpriteNames = {
    "coronastar",
    "coronastar",
    "coronamoon",
    "coronareflect",
    "coronaheadlightline",
    "",
    "",
    "",
    "",
    "coronaringb"
}; // 0x8D4950

constexpr std::array<const char[26], eCoronaType::CORONATYPE_COUNT> aCoronaSpriteNamesm = {
    "",
    "",
    "",
    "coronareflectm",
    "",
    "",
    "",
    "",
    "",
    ""
}; // 0x8D4A58

constexpr CFlareDefinition HeadLightsFlareDef[27]{
    {  4.00f, 5.0f, { 60,  60,  60  }, 200, 4 },
    {  3.00f, 7.0f, { 40,  40,  40  }, 200, 4 },
    {  2.00f, 5.0f, { 40,  40,  40  }, 200, 4 },
    {  1.50f, 6.0f, { 90,  90,  90  }, 200, 4 },
    {  1.25f, 5.0f, { 40,  40,  40  }, 200, 4 },
    {  0.80f, 14.f, { 60,  60,  60  }, 200, 4 },
    {  0.60f, 4.0f, { 40,  40,  40  }, 200, 4 },
    {  0.25f, 10.f, { 60,  60,  60  }, 200, 4 },
    {  0.10f, 6.0f, { 30,  30,  30  }, 200, 4 },
    {  0.05f, 14.f, { 50,  50,  50  }, 200, 4 },
    { -0.03f, 3.0f, { 30,  30,  30  }, 200, 4 },
    { -0.10f, 6.0f, { 60,  60,  60  }, 200, 4 },
    { -0.30f, 5.0f, { 30,  30,  30  }, 200, 4 },
    { -0.40f, 60.f, { 30,  30,  30  }, 200, 4 },
    { -0.55f, 4.0f, { 40,  40,  40  }, 200, 4 },
    { -0.75f, 14.f, { 50,  50,  50  }, 200, 4 },
    { -0.90f, 5.2f, { 35,  35,  35  }, 200, 4 },
    { -1.00f, 11.f, { 55,  55,  55  }, 200, 4 },
    { -1.20f, 3.5f, { 35,  35,  35  }, 200, 4 },
    { -1.35f, 9.0f, { 50,  50,  50  }, 200, 4 },
    { -1.70f, 54.f, { 35,  35,  35  }, 200, 4 },
    { -2.00f, 5.0f, { 50,  50,  50  }, 200, 4 },
    { -2.50f, 4.5f, { 35,  35,  35  }, 200, 4 },
    { -3.00f, 14.f, { 50,  50,  50  }, 200, 4 },
    { -6.00f, 24.f, { 70,  70,  70  }, 200, 4 },
    { -9.00f, 14.f, { 70,  50,  70  }, 200, 4 },
    {  0.00f, 0.0f, { 255, 255, 255 }, 255, 0 }
}; // 0x8D4D88

constexpr CFlareDefinition SunFlareDef[27]{
    {  4.00f, 8.00f, { 36,  30,  24  }, 200, 4 },
    {  3.00f, 11.2f, { 24,  18,  15  }, 200, 4 },
    {  2.00f, 8.00f, { 24,  12,  12  }, 200, 4 },
    {  1.50f, 9.60f, { 54,  54,  48  }, 200, 4 },
    {  1.25f, 8.00f, { 24,  24,  18  }, 200, 4 },
    {  0.80f, 22.4f, { 36,  30,  24  }, 200, 4 },
    {  0.60f, 6.40f, { 24,  15,  12  }, 200, 4 },
    {  0.25f, 16.0f, { 36,  30,  30  }, 200, 4 },
    {  0.10f, 9.60f, { 18,  18,  18  }, 200, 4 },
    {  0.05f, 22.4f, { 36,  30,  24  }, 200, 4 },
    { -0.03f, 4.80f, { 18,  18,  18  }, 200, 4 },
    { -0.10f, 9.60f, { 42,  42,  42  }, 200, 4 },
    { -0.30f, 8.00f, { 18,  6,   6   }, 200, 4 },
    { -0.40f, 96.0f, { 18,  12,  9   }, 200, 4 },
    { -0.55f, 6.40f, { 18,  18,  12  }, 200, 4 },
    { -0.75f, 22.4f, { 42,  24,  18  }, 200, 4 },
    { -0.90f, 8.32f, { 21,  12,  18  }, 200, 4 },
    { -1.00f, 17.6f, { 42,  13,  18  }, 200, 4 },
    { -1.20f, 5.60f, { 21,  12,  12  }, 200, 4 },
    { -1.35f, 14.4f, { 42,  42,  24  }, 200, 4 },
    { -1.70f, 86.8f, { 21,  15,  15  }, 200, 4 },
    { -2.00f, 8.00f, { 48,  30,  30  }, 200, 4 },
    { -2.50f, 7.20f, { 21,  15,  12  }, 200, 4 },
    { -3.00f, 22.4f, { 42,  30,  24  }, 200, 4 },
    { -6.00f, 38.4f, { 42,  42,  30  }, 200, 4 },
    { -9.00f, 22.4f, { 42,  30,  36  }, 200, 4 },
    {  0.00f, 0.00f, { 255, 255, 255 }, 255, 0 }
}; // 0x8D4B68

constexpr std::array<uint16, MAX_NUM_CORONAS> RandomWindowVals = {
     2984, 38394, 49320,   292, 10295, 29434, 40292, 65304,
    55555, 30249, 59303, 10234, 13405, 24949, 52045, 49624,
    22984, 34394,   320, 10292, 20295, 39434, 20292, 63304,
    46555, 20249,  9303, 50234, 28405, 14949, 59045, 62624,
    12984, 34394, 55320, 12292, 14295, 39434, 30292, 55304,
    15555, 60249, 51303, 19234, 19405, 44949, 52045, 59624,
    42984, 64394, 50320, 30292,   295, 19434, 54292, 43304,
    56555, 60249, 39303, 10234, 39405, 19949, 19045, 32624
}; // 0x8D4FA8, unused

void CCoronas::InjectHooks() {
    RH_ScopedClass(CCoronas);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(RenderOutGeometryBufferForReflections, 0x6FAA10);
    RH_ScopedInstall(Init, 0x6FAA70);
    RH_ScopedInstall(Shutdown, 0x6FAB00);
    RH_ScopedInstall(Update, 0x6FADF0);
    RH_ScopedInstall(Render, 0x6FAEC0);
    RH_ScopedInstall(RenderReflections, 0x6FB630);
    RH_ScopedInstall(RenderSunReflection, 0x6FBAA0);
    RH_ScopedOverloadedInstall(RegisterCorona, "type", 0x6FC180, void(*)(uint32, CEntity*, uint8, uint8, uint8, uint8, const CVector&, float, float, RwTexture*, eCoronaFlareType, eCoronaReflType, eCoronaLOSCheck, eCoronaTrail, float, bool, float, bool, float, bool, bool));
    RH_ScopedOverloadedInstall(RegisterCorona, "texture", 0x6FC580, void(*)(uint32, CEntity*, uint8, uint8, uint8, uint8, const CVector&, float, float, eCoronaType, eCoronaFlareType, eCoronaReflType, eCoronaLOSCheck, eCoronaTrail, float, bool, float, bool, float, bool, bool));

    RH_ScopedInstall(UpdateCoronaCoors, 0x6FC4D0);
    RH_ScopedInstall(DoSunAndMoon, 0x6FC5A0);
}

// Initialises coronas
// 0x6FAA70
void CCoronas::Init() {
    {
        CTxdStore::ScopedTXDSlot txd{ "particle" };
        for (auto&& [tex, name, maskName] : rng::zip_view{ gpCoronaTexture, aCoronaSpriteNames, aCoronaSpriteNamesm }) {
            // 0x6FAA97: the exe creates the texture only for table entries with a non-empty name
            if (!tex && name[0]) {
                tex = RwTextureRead(name, maskName);
            }
        }
    }
    rng::fill(aCoronas, CRegisteredCorona{});
    NumCoronas = 0; // 0x6FAB00 - 8: `mov [0xC3E038], 0`
}

// Terminates coronas
// 0x6FAB00
void CCoronas::Shutdown() {
    for (auto& t : gpCoronaTexture) {
        if (t) {
            RwTextureDestroy(std::exchange(t, nullptr));
        }
    }
}

enum CameraLookFlags : int32 {
    LOOK_LEFT      = 1 << 0,
    LOOK_RIGHT     = 1 << 1,
    LOOK_BACK      = 1 << 2,
    LOOK_DIRECTION = 1 << 3
};

// Updates coronas
// 0x6FADF0
void CCoronas::Update() {
    ZoneScoped;

    static int32 LastCamLook; // 0xC3EF58
    
    LightsMult = std::min(CTimer::GetTimeStep() * 0.03f + LightsMult, 1.f);

    const auto& cc = TheCamera.m_aCams[TheCamera.m_nActiveCam];

    int32 CamLook = 0;
    if (cc.m_bLookingLeft) {
        CamLook |= LOOK_LEFT;
    }
    if (cc.m_bLookingRight) {
        CamLook |= LOOK_RIGHT;
    }
    if (cc.m_bLookingBehind) {
        CamLook |= LOOK_BACK;
    }
    if (!TheCamera.GetLookDirection()) {
        CamLook |= LOOK_DIRECTION;
    }

    if (std::exchange(LastCamLook, CamLook) == CamLook) {
        bChangeBrightnessImmediately = std::max(bChangeBrightnessImmediately - 1, 0);
    } else {
        bChangeBrightnessImmediately = 3;
    }

    for (auto& corona : aCoronas) {
        if (corona.IsActive()) {
            corona.Update();
        }
    }
}

// The exe's inlined corona fade (0x6FB07x in Render, 0x6FB8xx in RenderReflections): `z < farClip/2 ? 1 : 1 - (z - half) / half` (half = farClip * 0.5),
// optionally clamped to [0, 1] with the exe's NaN arms. NOT `invLerp(farClip, farClip / 2, z)` (a different rounding).
static float ExeCoronaFade(float z, float farClip, bool clamp01) {
    const float half = farClip * 0.5f;
    float f = z < half ? 1.0f : 1.0f - (z - half) / half;
    if (clamp01) {
        if (1.0f < f) {
            f = 1.0f;
        } else if (0.0f > f) { // 0x6FB8ED: `0 > f` -> 0 (NaN keeps itself)
            f = 0.0f;
        }
    }
    return f;
}

// 0x6FAEC0
void CCoronas::Render() {
    ZoneScoped;

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));

    const auto raster     = RwCameraGetRaster(Scene.m_pRwCamera);
    const int32 rasterW   = RwRasterGetWidth(raster), rasterH = RwRasterGetHeight(raster);
    // 0x6FAEC9..: the half sizes are INTEGER divisions (`cdq / sub / sar 1`), the odd pixel is dropped
    const auto rasterHalf = CVector2D{ (float)(rasterW / 2), (float)(rasterH / 2) };

    bool zTestEnable = true;
    for (auto& c : aCoronas) {
        if (!c.m_dwId) {
            continue;
        }

        if (!c.m_FadedIntensity && !c.m_Color.a) {
            continue;
        }

        const auto covidPos = c.GetPosition();

        //< 0x6FB009 - Get on-screen position, and check if it's on it
        CVector   onScrPos;
        CVector2D onScrSize;
        if (c.m_bOffScreen = !CSprite::CalcScreenCoors(covidPos, &onScrPos, &onScrSize.x, &onScrSize.y, true, true)) {
            continue;
        }

        // 0x6FB02x: set when x < 0 || y < 0 || width < x || height < y (all strict, NaN never sets it)
        c.m_bOffScreen = onScrPos.x < 0.f || onScrPos.y < 0.f || (float)rasterW < onScrPos.x || (float)rasterH < onScrPos.y;

        // Already faded out
        if (c.m_FadedIntensity == 0) {
            continue;
        }

        // If outside farclip range, just ignore
        if (!(onScrPos.z < c.m_fFarClip)) { // 0x6FB074: fcomp + `test ah, 5` / jp (NaN skips too)
            continue;
        }

        const auto rz = 1.f / onScrPos.z;

        //< 0x6FB0A7 - Start fading out at half the far clip distance
        const auto intensity = NOTSA_FTOL(int16, (float)c.m_FadedIntensity * ExeCoronaFade(onScrPos.z, c.m_fFarClip, false));

        //< 0x6FB0D9 - Enable/disable Z test if necessary
        if (c.m_bCheckObstacles == zTestEnable) {
            zTestEnable = !zTestEnable;
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(zTestEnable));
        }

        const auto LerpColorC = [](uint8 cc, float t) {
            return NOTSA_FTOL(uint8, (float)cc * t);
        };

        //< 0x6FB131 - Render with texture
        if (c.m_pTexture) {
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE,   RWRSTATE(TRUE));
            RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(c.m_pTexture)));

            //< 0x6FB122
            const auto scale = std::min(40.f, onScrPos.z) * CWeather::Foggyness * ExeRecip(40.f) + 1.f;

            // NOP - The coordinates are later overwritten
            //if (c.m_dwId == 1) {
            //    scrPos.z = RwCameraGetFarClipPlane(Scene.m_pRwCamera) * 0.95f;
            //}

            //< 0x6FB24E
            if (CSprite::CalcScreenCoors(
                    covidPos - (covidPos - TheCamera.GetPosition()).Normalized() * c.m_fNearClip,
                    &onScrPos, // NOTE/BUG: Yeah, overwrites the previously calculated value. Not sure if it's intentional.
                    &onScrSize.x,
                    &onScrSize.y,
                    true,
                    true
                )) {
                CSprite::RenderOneXLUSprite_Rotate_Aspect(
                    onScrPos,
                    onScrSize * c.m_fSize * CVector2D{ 1.f, scale },
                    LerpColorC(c.m_Color.r, 1.f / scale),
                    LerpColorC(c.m_Color.g, 1.f / scale),
                    LerpColorC(c.m_Color.b, 1.f / scale),
                    intensity,
                    rz * 20.f,
                    0.f,
                    255
                );
            }
        }

        //< 0x6FB2F3 - Render flare
        if (c.m_nFlareType != FLARETYPE_NONE) {
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(FALSE));
            RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(gpCoronaTexture[CORONATYPE_SHINYSTAR])));

            //< 0x6FB35B
            // 0x6FB319..0x6FB35B: (rand() * 1/32767 * 0.3 + 0.7) * FadedIntensity * 2^-16 (0x859D5C), stored as a float
            const auto colorVariationMult = CGeneral::GetRandomNumberInRange(0.7f, 1.f) * (float)c.m_FadedIntensity * ExeRecip(65536.f);

            //< 0x6FB2FC [Moved here]
            auto it = [&] {
                switch (c.m_nFlareType) {
                case FLARETYPE_SUN:        return &SunFlareDef[0];
                case FLARETYPE_HEADLIGHTS: return &HeadLightsFlareDef[0];
                default:                   NOTSA_UNREACHABLE_RET(&SunFlareDef[0]);
                }
            }();

            //< 0x6FB46C
            for (; it->Sprite; it++) {
                CEntity* hitEntity;
                CColPoint hitCP;
                if (!CWorld::ProcessLineOfSight(
                        covidPos,
                        TheCamera.GetPosition(),
                        hitCP,
                        hitEntity,
                        false,
                        true,
                        true,
                        false,
                        false,
                        false,
                        false,
                        true
                    )) { //< 0x6FB409
                    // 0x6FB370: the product with the raw table value is an INTEGER multiplication (`imul`), converted to float afterwards
                    CRGBA color = {
                        NOTSA_FTOL(uint8, static_cast<float>(it->ColorMult.x * (int32)c.m_Color.r) * colorVariationMult),
                        NOTSA_FTOL(uint8, static_cast<float>(it->ColorMult.y * (int32)c.m_Color.g) * colorVariationMult),
                        NOTSA_FTOL(uint8, static_cast<float>(it->ColorMult.z * (int32)c.m_Color.b) * colorVariationMult),
                        255
                    };
                    CSprite::RenderBufferedOneXLUSprite2D(
                        lerp(rasterHalf, CVector2D{ onScrPos }, it->Position),
                        CVector2D{ it->Size, it->Size } * 4.f,
                        color,
                        255,
                        255
                    );
                }
            }

            RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(TRUE));

            //< 0x6FB483
            if (c.m_nFlareType == FLARETYPE_HEADLIGHTS && CWeather::HeadLightsSpectrum != 0.f && CGame::CanSeeOutSideFromCurrArea()) {
                for (auto it = HeadLightsFlareDef; it->Sprite; it++) {
                    const auto RenderFlareSprite = [&,
                                                    spriteIntensity = (int16)(((int32)it->IntensityMult * (int32)intensity) >> 8)](RwRGBA clr, float posOffset) { // 0x6FB510: imul; sar 8
                        CSprite::RenderBufferedOneXLUSprite2D(
                            lerp(rasterHalf, CVector2D{ onScrPos }, it->Position + posOffset),
                            CVector2D{ it->Size, it->Size },
                            clr,
                            spriteIntensity,
                            255
                        );
                    };
                    // 0x6FB4F3: ftol(float(Raw * c) * HeadLightsSpectrum * 2^-8)
                    const auto HeadLightColor = [](int16 raw, uint8 cc) { return NOTSA_FTOL(uint8, (float)(((x87d)(float)(raw * (int32)cc) * (x87d)CWeather::HeadLightsSpectrum) * (x87d)ExeRecip(256.f))); };
                    RenderFlareSprite({ HeadLightColor(it->ColorMult.x, c.m_Color.r), 0, 0, 255 }, +0.05f); // 0x6FB561
                    RenderFlareSprite({ 0, 0, HeadLightColor(it->ColorMult.z, c.m_Color.b), 255 }, -0.05f); // 0x6FB5EA
                }
            }
        }
    }
    CSprite::FlushSpriteBuffer();

    /* NOTE/BUG: Renderstates not restored? */
}

// 0x6FB630
void CCoronas::RenderReflections() {
    if (s_DebugSettings.DisableWetRoadReflections) {
        return;
    }

    if (!s_DebugSettings.AlwaysRenderWetRoadReflections) {
        // Check if the roads are wet enough
        if (CWeather::WetRoads <= 0.f) {
            for (auto& c : aCoronas) {
                c.m_bHasValidHeightAboveGround = false;
            }
            return;
        }
    }

    CSprite::InitSpriteBuffer(); // 0x6FB66A

    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(RwTextureGetRaster(gpCoronaTexture[CORONATYPE_REFLECTION])));

    const auto camPos = TheCamera.GetPosition();
    for (auto&& [i, c] : rngv::enumerate(aCoronas)) {
        if (!c.m_dwId) {
            continue;
        }
        if (!c.m_FadedIntensity && !c.m_Color.a) {
            continue;
        }
        if (!c.m_bUsesReflection) { // 0x6FB6E0: `cmp byte [corona + 0x27], 0 / je`
            continue;
        }

        // 0x6FB6E8: attached coronas use the entity matrix, also for bikes (no lean matrix here, unlike Render)
        const auto covidPos = c.m_pAttachedTo ? c.m_pAttachedTo->GetMatrix().TransformPoint(c.m_vPosn) : c.m_vPosn;
        if (!c.m_bHasValidHeightAboveGround || NOTSA_EXE_OR((((uint32)i + CTimer::GetFrameCounter()) & 0xF) == 0 /* 0x6FB763: `(frame + i) & 0xF` - upstream's "simplified" form was `(i & 0xFF) + (frame & 0xFF) % 16 == 0`, i.e. only coronas 0, 256.. every 16th frame */, ((i & 0xFF) + (CTimer::GetFrameCounter() & 0xFF) % 16) == 0)) { //< Simplified code
            bool bGroundFound;
            const auto groundZ = CWorld::FindGroundZFor3DCoord(covidPos, &bGroundFound);
            if (bGroundFound) { // NOTE/BUG: Weird.. Why not set it according to `bGroundFound` directly?
                c.m_bHasValidHeightAboveGround = true;
                c.m_fHeightAboveGround         = covidPos.z - groundZ;
            }
        }
        if (!c.m_bHasValidHeightAboveGround) {
            continue;
        }
        // 0x6FB7F0: `height < 20` is required (the port compared the FLAG with 20, which never skipped)
        const float height = c.m_fHeightAboveGround;
        if (!(height < 20.f)) {
            continue;
        }
        // 0x6FB81A: skip when `pos.z - height > cam.z` or NaN (equal does NOT skip)
        if (!(covidPos.z - height <= camPos.z)) {
            continue;
        }
        CVector   onScrPos;
        CVector2D onScrSize;
        if (!CSprite::CalcScreenCoors(covidPos - CVector{ 0.f, 0.f, 2 * height }, &onScrPos, &onScrSize.x, &onScrSize.y, true, true)) {
            continue;
        }
        // 0x6FB87x: `55 < v ? 55 : v` (a NaN stays NaN; std::min would give 55)
        const float farClip75     = c.m_fFarClip * 0.75f;
        const float clampedFarClip = 55.f < farClip75 ? 55.f : farClip75;
        if (!(onScrPos.z < clampedFarClip)) { // 0x6FB8A4
            continue;
        }
        // 0x6FB904..0x6FB92x: ONE integer multiplier `(int16)ftol(((WetRoads * fade) * (20 - height)) * 230 * 0.05)`, then (colour * mult) >> 8 in integers
        const float wet = s_DebugSettings.AlwaysRenderWetRoadReflections ? 1.f : CWeather::WetRoads;
        const int16 mult = NOTSA_FTOL(int16, ((wet * ExeCoronaFade(onScrPos.z, clampedFarClip, true)) * (20.f - height)) * 230.f * 0.05f);
        const auto LerpColorC = [mult](uint8 cc) {
            return (uint8)(((int32)cc * (int32)mult) >> 8);
        };
        // 0x6FB93x: width = (sizeX * size) * 0.75, height = (sizeY * size) * 2
        CSprite::RenderBufferedOneXLUSprite(
            { onScrPos.x, onScrPos.y, notsa::bugfixes::PS2CoronaRendering ? onScrPos.z : RwIm2DGetNearScreenZMacro() },
            { onScrSize.x * c.m_fSize * 0.75f, onScrSize.y * c.m_fSize * 2.f },
            LerpColorC(c.m_Color.r),
            LerpColorC(c.m_Color.g),
            LerpColorC(c.m_Color.b),
            128,
            1.f / RwCameraGetNearClipPlane(Scene.m_pRwCamera),
            255
        );
    }
    CSprite::FlushSpriteBuffer();

    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
}

// 0x6FAA10
void CCoronas::RenderOutGeometryBufferForReflections() {
    if (uiTempBufferIndicesStored) {
        LittleTest();
        if (RwIm3DTransform(TempBufferVertices.m_3d, uiTempBufferVerticesStored, 0, 1u)) {
            RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, aTempBufferIndices, uiTempBufferIndicesStored);
            RwIm3DEnd();
        }
        uiTempBufferVerticesStored = 0;
        uiTempBufferIndicesStored  = 0;
    }
}

// 0x6FBAA0
void CCoronas::RenderSunReflection() {
    constexpr auto REFLECTION_SIZE   = 60.f;
    constexpr auto REFLECTION_PERIOD = 2'048; // Code is faster if this is a power-of-2
    constexpr auto ANGLE_STEP = TWO_PI / REFLECTION_PERIOD; // 0x87252C
    constexpr auto SUN_INTENSITY_SCALE = 3.333f; // 0x872530: the literal 3.333f (0x40554FDF), NOT 1.0f / 0.3f

    const auto vecToSun3D = CTimeCycle::m_VectorToSun[CTimeCycle::m_CurrentStoredValue];

    if (!(vecToSun3D.z > -0.05f)) { // 0x6FBAC3: fcomp + `test ah, 0x41` / jne (NaN returns too)
        return;
    }

    const auto camPos = TheCamera.GetPosition();

    auto t =
        (0.3f - std::abs(vecToSun3D.z - 0.25f)) * SUN_INTENSITY_SCALE
        * (1.f - CWeather::CloudCoverage)
        * (1.f - CWeather::Foggyness)
        * (1.f - CWeather::Wind);
    if (!(t > 0.f)) { // 0x6FBB5x: fcomp + `test ah, 0x41` / jne
        return;
    }
    // 0x6FA... helper 0x6FBA20(pos, radius): d = 2D distance to the camera; `radius + 100 <= d (or NaN) ? 1 : d <= radius ? 0 : (d - radius) * 0.01f`
    // (the 4th spot has the radius 350, the others 250; the ramp is a multiplication by 0.01f, not a division by 100)
    const auto SpotFade = [&](float x, float y, float radius) -> float {
        const float dx = camPos.x - x, dy = camPos.y - y;
        const float d  = std::sqrt(dy * dy + dx * dx);
        if (!(d < radius + 100.f)) {
            return 1.f;
        }
        if (d <= radius) {
            return 0.f;
        }
        return (d - radius) * 0.01f;
    };
    t = SpotFade(611.0f,  875.0f,  250.f) * t;
    t = SpotFade(-929.0f, 2364.f,  250.f) * t;
    t = SpotFade(-1034.f, 2640.f,  250.f) * t;
    t = SpotFade(2372.f,  -1854.f, 350.f) * t;
    t = SpotFade(-1633.f, 106.0f,  250.f) * t;
    t *= 0.25f;

    const auto center2D         = CVector2D{ vecToSun3D } * 40.f + CVector2D{ camPos };
    const auto vecToSun2D       = Normalized2D(vecToSun3D);
    const auto vecToSunCore2D   = vecToSun2D * (REFLECTION_SIZE / 2.f);
    const auto vecToSunCorona2D = vecToSun2D * REFLECTION_SIZE;

#define CalcColorC(_color) \
    NOTSA_FTOL(uint8, (float)(CTimeCycle::m_CurrentColours.m_nSunCorona##_color + CTimeCycle::m_CurrentColours.m_nSunCore##_color) * t)
    const auto PushVertex = [
        &,
        color = CRGBA{ CalcColorC(Red), CalcColorC(Green), CalcColorC(Blue), 255 },
        posZ = CWeather::Wind * 0.5f + 0.1f
    ](CVector2D pos, CVector2D uv) {
        RenderBuffer::PushVertex(CVector{ pos, posZ }, uv, color);
    };
#undef CalcColorC
    // 0x6FBD9x..0x6FBFxx: the exe's association of the vertex sums: right = ((perp.y + off.x) + c.x, (off.y + c.y) - perp.x), left = ((off.x + c.x) - perp.y, (perp.x + off.y) + c.y)
    const auto Right = [&](CVector2D off, CVector2D perp) { return CVector2D{ (perp.y + off.x) + center2D.x, (off.y + center2D.y) - perp.x }; };
    const auto Left  = [&](CVector2D off, CVector2D perp) { return CVector2D{ (off.x + center2D.x) - perp.y, (perp.x + off.y) + center2D.y }; };

    RenderBuffer::ClearRenderBuffer();

    // The part closer to the camera
    {
        RenderBuffer::PushIndices({ 2, 1, 0, 2, 3, 1 }, false);

        PushVertex(Right({ 0.f, 0.f }, vecToSunCore2D),   { 0.0f, 1.0f });
        PushVertex(Left({ 0.f, 0.f }, vecToSunCore2D),    { 1.0f, 1.0f });
        PushVertex(Right(vecToSunCorona2D, vecToSunCore2D), { 0.0f, 0.5f });
        PushVertex(Left(vecToSunCorona2D, vecToSunCore2D),  { 1.0f, 0.5f });
    }

    //< 0x6FBF36 - The part that is changing position [To imitate how the water is flowing]
    const auto time = CTimer::GetTimeInMS();
    for (auto i = 0u; i < 20u; i++) {
        RenderBuffer::PushIndices({ 0, -1, -2, 0, 1, -1 }, true); // Same as the above indices, but with 2 subtracted from each

        const auto forward = vecToSun2D * (
              X87R(x87::sin((float)((time + 900 * i) % REFLECTION_PERIOD) / (float)REFLECTION_PERIOD * TWO_PI) * 10.f)
            + (float)(i * 970 / 20 + 30)
        );
        const auto offset = vecToSun2D * (float)(i * 1'440 / 20 + 60);

        PushVertex(Right(offset, forward), { 0.0f, 0.5f });
        PushVertex(Left(offset, forward),  { 1.0f, 0.5f });
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEFOGTYPE,           RWRSTATE(rwFOGTYPELINEAR));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(RwTextureGetRaster(gpCoronaTexture[CORONATYPE_HEADLIGHTLINE])));

    RenderBuffer::RenderStuffInBuffer();

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
}

// Registers a corona effect with a custom texture.
// Creates or updates a light corona in the game world with specified properties.
// 0x6FC180
void CCoronas::RegisterCorona(
    uint32           id,
    CEntity*         attachTo,
    uint8            red,
    uint8            green,
    uint8            blue,
    uint8            intensity,
    const CVector&   pos,
    float            size,
    float            range,
    RwTexture*       texture,
    eCoronaFlareType flareType,
    eCoronaReflType  reflType,
    eCoronaLOSCheck  checkLOS,
    eCoronaTrail     usesTrails, // unused
    float            normalAngle,
    bool             neonFade,
    float            pullTowardsCam,
    bool             fullBrightAtStart,
    float            fadeSpeed,
    bool             onlyFromBelow,
    bool             whiteCore
) {
    const auto worldPos = attachTo
        ? attachTo->GetMatrix().TransformPoint(pos)
        : pos;

    // Exact x87 form of the exe (0x6FC180): the squared 2D distance is (y delta)^2 + (x delta)^2 in extended precision and a NaN does NOT reject
    const auto& camPos = TheCamera.GetPosition();
    {
        const x87d dy = (x87d)camPos.y - (x87d)worldPos.y, dx = (x87d)camPos.x - (x87d)worldPos.x;
        if ((x87d)range * (x87d)range < dy * dy + dx * dx) {
            return; // Corona is beyond far clip distance
        }
    }

    // 0x6FC24A
    // Adjust intensity for neon fade effect if enabled
    uint8 adjustedAlpha = intensity;
    if (neonFade) {
        const x87d dz = (x87d)camPos.z - (x87d)worldPos.z, dy = (x87d)camPos.y - (x87d)worldPos.y, dx = (x87d)camPos.x - (x87d)worldPos.x;
        const float distance3d = (float)x87::sqrt(dz * dz + dy * dy + dx * dx);
        if (distance3d < 35.0f) {
            return;
        }
        if (distance3d < 50.0f) {
            // alpha = ftol((dist - 35) * intensity * (1/15)) (the old port multiplied the alpha by the truncated factor)
            adjustedAlpha = (uint8)notsa::detail::Ftol(((x87d)distance3d - (x87d)35.0) * (x87d)intensity * (x87d)ExeRecip(15.0f));
        }
    }

    // Find or allocate a corona slot
    CRegisteredCorona* corona = GetCoronaByID(id);
    if (corona) {
        if (!corona->m_FadedIntensity && !adjustedAlpha) { // 0x6FC470: the FADED intensity (+0x30), not the colour alpha
            corona->SetInactive();
            --NumCoronas;
            return;
        }
    } else { /* allocate new */
        if (!adjustedAlpha) { // 0x6FC301: a new corona is only created for a non-zero intensity
            return;
        }
        corona = GetFree();
        if (!corona) {
            return;
        }

        // 0x6FC33D
        // Initialize new corona in free slot
        corona->m_FadedIntensity             = fullBrightAtStart ? 255 : 0;
        corona->m_bOffScreen                 = true;
        corona->m_bHasValidHeightAboveGround = false;
        corona->m_bJustCreated               = true;
        corona->m_dwId                       = id;
        NumCoronas++;
    }

    // Update corona properties
    corona->m_Color.Set(red, green, blue, adjustedAlpha);
    corona->m_vPosn                = pos;
    corona->m_fSize                = size;
    corona->m_fAngle               = normalAngle;
    corona->m_bRegisteredThisFrame = true;
    corona->m_fFarClip             = range;
    corona->m_fNearClip            = pullTowardsCam;
    corona->m_pTexture             = texture;
    corona->m_nFlareType           = flareType;
    corona->m_bUsesReflection      = reflType;
    corona->m_bCheckObstacles      = checkLOS;
    corona->m_fFadeSpeed           = fadeSpeed;

    // 0x6FC401
    corona->m_bFlashWhileFading  = neonFade;
    corona->m_bOnlyFromBelow     = onlyFromBelow;
    corona->m_bDrawWithWhiteCore = whiteCore;

    // Handle attachment to entity
    if (attachTo) {
        corona->m_bAttached   = true;
        corona->m_pAttachedTo = attachTo;
        CEntity::SetEntityReference(corona->m_pAttachedTo, attachTo);
    } else {
        corona->m_bAttached   = false;
        corona->m_pAttachedTo = nullptr;
    }
}

// Registers a corona effect using a predefined corona type.
// Delegates to the main RegisterCorona function with a texture from the type.
// 0x6FC580
void CCoronas::RegisterCorona(uint32 id, CEntity* attachTo, uint8 red, uint8 green, uint8 blue, uint8 intensity, const CVector& pos, float size, float range, eCoronaType coronaType, eCoronaFlareType flareType, eCoronaReflType reflType, eCoronaLOSCheck checkLOS, eCoronaTrail usesTrails, float normalAngle, bool neonFade, float pullTowardsCam, bool fullBrightAtStart, float fadeSpeed, bool onlyFromBelow, bool whiteCore) {
    RegisterCorona(id, attachTo, red, green, blue, intensity, pos, size, range, gpCoronaTexture[coronaType], flareType, reflType, checkLOS, usesTrails, normalAngle, neonFade, pullTowardsCam, fullBrightAtStart, fadeSpeed, onlyFromBelow, whiteCore);
}

// 0x6FC4D0
void CCoronas::UpdateCoronaCoors(uint32 id, const CVector& pos, float range, float normalAngle) {
    // 0x6FC4D0: (y delta)^2 + (x delta)^2 in extended precision, NaN does not reject
    const auto& cam = TheCamera.GetPosition();
    const x87d dy = (x87d)cam.y - (x87d)pos.y, dx = (x87d)cam.x - (x87d)pos.x;
    if (!((x87d)range * (x87d)range < dy * dy + dx * dx)) {
        if (auto* corona = GetCoronaByID(id)) {
            corona->m_vPosn = pos;
            corona->m_fAngle = normalAngle;
        }
    }
}

// Draw sun (Moon went to CClouds since SA)
// 0x6FC5A0
void CCoronas::DoSunAndMoon() {
    ZoneScoped;

    if (!CGame::CanSeeOutSideFromCurrArea()) {
        return;
    }

    // Constants for sun corona parts
    constexpr float sunGlowSizeMultiplier = 2.7335f; // 0x872534
    constexpr float sunFlareSizeMultiplier = 6.0f;

    const auto vecToSun  = CTimeCycle::GetVectorToSun();
    const auto coronaPos = vecToSun * (CDraw::GetFarClipZ() * 0.95f) + TheCamera.GetPosition();

    if (vecToSun.z > -0.1f) {
        const auto DoRegisterCorona = [coronaPos](uint32 id, eCoronaFlareType flareType, float size, eCoronaLOSCheck checkLOS) {
            const auto& cc = CTimeCycle::m_CurrentColours;
            RegisterCorona(
                id,
                nullptr,
                checkLOS ? (uint8)cc.m_nSunCoronaRed : (uint8)cc.m_nSunCoreRed,
                checkLOS ? (uint8)cc.m_nSunCoronaGreen : (uint8)cc.m_nSunCoreGreen,
                checkLOS ? (uint8)cc.m_nSunCoronaBlue : (uint8)cc.m_nSunCoreBlue,
                255u,
                coronaPos,
                cc.m_fSunSize * size,
                999999.88f,
                gpCoronaTexture[CORONATYPE_SHINYSTAR],
                flareType,
                CORREFL_NONE,
                checkLOS,
                TRAIL_OFF,
                0.f,
                false,
                1.5f,
                0,
                15.f,
                false,
                false
            );
        };

        DoRegisterCorona(1, FLARETYPE_NONE, sunGlowSizeMultiplier, LOSCHECK_OFF);
        if (vecToSun.z > 0.f) { // Removed redudant check
            DoRegisterCorona(2, FLARETYPE_SUN, sunFlareSizeMultiplier, LOSCHECK_ON);
        }
    }

    // Debug code here

    CVector screenPos;
    float pX, pY;
    if (CSprite::CalcScreenCoors(coronaPos, &screenPos, &pX, &pY, true, true)) {
        SunScreenX = screenPos.x;
        SunScreenY = screenPos.y;
    } else {
        SunScreenY = 1000000.0f;
        SunScreenX = 1000000.0f;
    }
}

// Inlined, code from 0x6FC524 to 0x6FC53C
inline CRegisteredCorona* CCoronas::GetCoronaByID(int32 id) {
    for (auto& corona : aCoronas) {
        if (corona.m_dwId == id) {
            return &corona;
        }
    }
    return nullptr;
}

// Inlined, code from 0x6FC309 to 0x6FC31E
inline CRegisteredCorona* CCoronas::GetFree() {
    for (auto& corona : aCoronas) {
        if (!corona.IsActive()) {
            return &corona; /* Caller has to increase `NumCoronas` */
        }
    }
    return nullptr;
}
