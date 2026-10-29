/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Radar.h"
#include <numbers>
#include "EntryExitManager.h"
#include "Fx/FxFtol.h"
#include "ExeZero.h"
#line 13

constexpr std::array<airstrip_info, NUM_AIRSTRIPS> airstrip_table = { // 0x8D06E0
    airstrip_info{ { +1750.0f,  -2494.0f }, 180.0f, 1000.0f }, // AIRSTRIP_LS_AIRPORT
    airstrip_info{ { -1373.0f,  +120.00f }, 315.0f, 1500.0f }, // AIRSTRIP_SF_AIRPORT
    airstrip_info{ { +1478.0f,  +1461.0f }, 90.0f,  1200.0f }, // AIRSTRIP_LV_AIRPORT
    airstrip_info{ { +175.00f,  +2502.0f }, 180.0f, 1000.0f }  // AIRSTRIP_VERDANT_MEADOWS
};

// Array of TXD slot indices for each radar section's texture
// Index using y, x (In that order)
static NOTSA_GLOBAL(gRadarTextures, 0xBA8478, (std::array<std::array<int32, MAX_RADAR_WIDTH_TILES>, MAX_RADAR_HEIGHT_TILES>), {});

// 0x8D0720
SpriteFileName CRadar::RadarBlipFileNames[] = {
    // name,                 maskName
    { nullptr,               nullptr },
    { nullptr,               nullptr },
    { "radar_centre",        nullptr },
    { "arrow",               nullptr },
    { "radar_north",         nullptr },
    { "radar_airYard",       nullptr },
    { "radar_ammugun",       nullptr },
    { "radar_barbers",       nullptr },
    { "radar_BIGSMOKE",      nullptr },
    { "radar_boatyard",      nullptr },
    { "radar_burgerShot",    nullptr },
    { "radar_bulldozer",     nullptr },
    { "radar_CATALINAPINK",  nullptr },
    { "radar_CESARVIAPANDO", nullptr },
    { "radar_chicken",       nullptr },
    { "radar_CJ",            nullptr },
    { "radar_CRASH1",        nullptr },
    { "radar_diner",         nullptr },
    { "radar_emmetGun",      nullptr },
    { "radar_enemyAttack",   nullptr },
    { "radar_fire",          nullptr },
    { "radar_girlfriend",    nullptr },
    { "radar_hostpitaL",     nullptr },
    { "radar_LocoSyndicate", nullptr },
    { "radar_MADDOG",        nullptr },
    { "radar_mafiaCasino",   nullptr },
    { "radar_MCSTRAP",       nullptr },
    { "radar_modGarage",     nullptr },
    { "radar_OGLOC",         nullptr },
    { "radar_pizza",         nullptr },
    { "radar_police",        nullptr },
    { "radar_propertyG",     nullptr },
    { "radar_propertyR",     nullptr },
    { "radar_race",          nullptr },
    { "radar_RYDER",         nullptr },
    { "radar_saveGame",      nullptr },
    { "radar_school",        nullptr },
    { "radar_qmark",         nullptr },
    { "radar_SWEET",         nullptr },
    { "radar_tattoo",        nullptr },
    { "radar_THETRUTH",      nullptr },
    { "radar_waypoint",      nullptr },
    { "radar_TorenoRanch",   nullptr },
    { "radar_triads",        nullptr },
    { "radar_triadsCasino",  nullptr },
    { "radar_tshirt",        nullptr },
    { "radar_WOOZIE",        nullptr },
    { "radar_ZERO",          nullptr },
    { "radar_dateDisco",     nullptr },
    { "radar_dateDrink",     nullptr },
    { "radar_dateFood",      nullptr },
    { "radar_truck",         nullptr },
    { "radar_cash",          nullptr },
    { "radar_flag",          nullptr },
    { "radar_gym",           nullptr },
    { "radar_impound",       nullptr },
    { "radar_light",         nullptr },
    { "radar_runway",        nullptr },
    { "radar_gangB",         nullptr },
    { "radar_gangP",         nullptr },
    { "radar_gangY",         nullptr },
    { "radar_gangN",         nullptr },
    { "radar_gangG",         nullptr },
    { "radar_spray",         nullptr }
};

constexpr float RADAR_MIN_RANGE = 180.0f;
constexpr float RADAR_MAX_RANGE = 350.0f;
constexpr float RADAR_MIN_SPEED = 0.3f;
constexpr float RADAR_MAX_SPEED = 0.9f;

void CRadar::InjectHooks() {
    RH_ScopedClass(CRadar);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Initialise, 0x587FB0);
    RH_ScopedInstall(LoadTextures, 0x5827D0);
    RH_ScopedInstall(DrawLegend, 0x5828A0);
    RH_ScopedInstall(LimitRadarPoint, 0x5832F0);
    RH_ScopedInstall(Shutdown, 0x585940);
    RH_ScopedInstall(SetMapCentreToPlayerCoords, 0x585B20);
    RH_ScopedInstall(InitFrontEndMap, 0x585960);
    RH_ScopedInstall(CalculateBlipAlpha, 0x583420);
    RH_ScopedInstall(TransformRadarPointToScreenSpace, 0x583480);
    RH_ScopedInstall(TransformRealWorldPointToRadarSpace, 0x583530);
    RH_ScopedInstall(CalculateCachedSinCos, 0x583670);
    RH_ScopedInstall(SetBlipSprite, 0x583D70);
    RH_ScopedInstall(SetBlipAlwaysDisplayInZoom, 0x583DB0);
    RH_ScopedInstall(DrawYouAreHereSprite, 0x584960);
    RH_ScopedInstall(ChangeBlipColour, 0x583AB0);
    RH_ScopedOverloadedInstall(ClearActualBlip, "OG", 0x587C10, void (*)(int32));
    RH_ScopedInstall(ClearBlip, 0x587CE0);
    RH_ScopedOverloadedInstall(ClearBlipForEntity, "OG", 0x587C60, void(*)(eBlipType, int32));
    RH_ScopedInstall(RequestMapSection, 0x584B50);
    RH_ScopedInstall(RemoveMapSection, 0x584BB0);
    RH_ScopedInstall(RemoveRadarSections, 0x584BF0);
    RH_ScopedInstall(DrawRadarSprite, 0x585FF0);
    RH_ScopedInstall(DrawMap, 0x586B00);
    RH_ScopedInstall(DrawRadarMap, 0x586880);
    RH_ScopedOverloadedInstall(StreamRadarSections, "vec", 0x5858D0, void (*)(const CVector&));
    RH_ScopedInstall(SetupRadarRect, 0x584A80);
    RH_ScopedInstall(GetActualBlipArrayIndex, 0x582870);
    RH_ScopedInstall(LimitToMap, 0x583350);
    RH_ScopedInstall(DrawRotatingRadarSprite, 0x584850);
    RH_ScopedInstall(SetBlipFriendly, 0x583EB0);
    RH_ScopedInstall(ChangeBlipDisplay, 0x583D20);
    RH_ScopedInstall(SetBlipEntryExit, 0x583F00);
    RH_ScopedInstall(SetShortRangeCoordBlip, 0x583920);
    RH_ScopedInstall(ChangeBlipScale, 0x583CC0);
    RH_ScopedInstall(GetRadarTraceColour, 0x584770);
    RH_ScopedInstall(ShowRadarMarker, 0x584480);
    RH_ScopedInstall(DrawRadarMask, 0x585700);
    RH_ScopedInstall(Load, 0x5D53C0);
    RH_ScopedInstall(Save, 0x5D5860);
    RH_ScopedInstall(SetBlipFade, 0x583E00); // unused
    RH_ScopedInstall(SetCoordBlipAppearance, 0x583E50);
    RH_ScopedInstall(SetCoordBlip, 0x583820);
    RH_ScopedInstall(SetEntityBlip, 0x5839A0);
    RH_ScopedInstall(DisplayThisBlip, 0x583B40);
    RH_ScopedInstall(ChangeBlipBrightness, 0x583C70);
    RH_ScopedInstall(ShowRadarTrace, 0x583F40);
    RH_ScopedInstall(ShowRadarTraceWithHeight, 0x584070);
    RH_ScopedInstall(DrawCoordBlip, 0x586D60);

    RH_ScopedInstall(SetupAirstripBlips, 0x587D20); // TEST
    RH_ScopedInstall(DrawBlips, 0x588050);
    RH_ScopedInstall(ClipRadarPoly, 0x585040);
    RH_ScopedInstall(DrawAreaOnRadar, 0x5853D0);
    RH_ScopedOverloadedInstall(StreamRadarSections, "xy", 0x584C50, void (*)(int32, int32));
    RH_ScopedInstall(AddBlipToLegendList, 0x5859F0);
    RH_ScopedInstall(Draw3dMarkers, 0x585BF0);
    RH_ScopedInstall(DrawRadarSection, 0x586110);
    RH_ScopedInstall(DrawRadarSectionMap, 0x586520);
    RH_ScopedInstall(DrawRadarGangOverlay, 0x586650);
    RH_ScopedInstall(DrawEntityBlip, 0x587000);
    RH_ScopedGlobalInstall(LineRadarBoxCollision, 0x584E00);

    // unused
    RH_ScopedInstall(GetNewUniqueBlipIndex, 0x582820);
    RH_ScopedInstall(TransformRadarPointToRealWorldSpace, 0x5835A0);
    RH_ScopedGlobalInstall(IsPointInsideRadar, 0x584D40);
    RH_ScopedGlobalInstall(GetTextureCorners, 0x584D90);
    RH_ScopedGlobalInstall(ClipRadarTileCoords, 0x584B00);    
}

// 0x587FB0
void CRadar::Initialise() {
    airstrip_blip = 0;
    airstrip_location = AIRSTRIP_LS_AIRPORT;

    for (auto& trace : ms_RadarTrace) {
        ClearActualBlip(trace);
        trace.m_nCounter = 1; // 0x587FF2 (`mov [eax-8], cx` in the init loop): the handle counter starts at 1, so the first blip handle is (2 << 16) | index. ClearActualBlip (0x587C10) leaves it alone. Found by REPLAY-DIFF: script space (blip handles) differed at frame 1
    }

    m_radarRange = 350.0f;

    for (auto y = 0u; y < MAX_RADAR_HEIGHT_TILES; y++) {
        for (auto x = 0u; x < MAX_RADAR_WIDTH_TILES; x++) {
            gRadarTextures[y][x] = CTxdStore::FindTxdSlot(std::format("radar{:02d}", y * MAX_RADAR_WIDTH_TILES + x).c_str());
        }
    }
}

// 0x585940
void CRadar::Shutdown() {
    rng::for_each(RadarBlipSprites, &CSprite2d::Delete);
    RemoveRadarSections();
}

// 0x5827D0
void CRadar::LoadTextures() {
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(CTxdStore::FindTxdSlot("hud"));

    for (auto i = 0; i < MAX_RADAR_SPRITES; i++) {
        const auto [texture, mask] = RadarBlipFileNames[i];
        RadarBlipSprites[i].SetTexture(texture); // mask unused
    }

    CTxdStore::PopCurrentTxd();
}

/*!
 * @brief Creates a unique blip handle by blip index from `ms_RadarTrace`. (Inlined)
 * @addr 0x582820
 * @returns Unique blip handle
 */
tBlipHandle CRadar::GetNewUniqueBlipIndex(int32 index) {
    auto& trace = ms_RadarTrace[index];
    if (trace.m_nCounter >= std::numeric_limits<uint16>::max() - 1)
        trace.m_nCounter = 1; // Wrap back to 1
    else
        trace.m_nCounter++; // Increment
    return index | (trace.m_nCounter << 16);
}

/*!
 * @param blip Blip handle
 * @addr 0x582870
 * @returns Index of the handle in `ms_RadarTrace`
 */
int32 CRadar::GetActualBlipArrayIndex(tBlipHandle blip) {
    if (blip == -1)
        return -1;

    const auto  traceIndex = static_cast<uint16>(blip);
    const auto& trace      = ms_RadarTrace[traceIndex];
    const auto  counter    = static_cast<uint16>(blip >> 16);
    if (counter != trace.m_nCounter || !trace.m_bTrackingBlip)
        return -1;

    return traceIndex;
}

// 0x5828A0
void CRadar::DrawLegend(int32 x, int32 y, eRadarSprite blipType) {
    CFont::PrintString(
        (float)x + SCREEN_STRETCH_X(20.0f),
        (float)y + SCREEN_STRETCH_Y(3.0f),
        GetBlipName(blipType)
    );

    if (blipType > -1) { // The blip is a sprite, so just draw it.
        RadarBlipSprites[blipType].Draw(
            {
                (float)x,                           (float)y,
                (float)x + SCREEN_STRETCH_X(16.0f), (float)y + SCREEN_STRETCH_X(16.0f)
            },
            // NOTE: `y + SCREEN_STRETCH_X(16.0f)` is correct. It is here to make sprites
            // square instead of dependent on the aspect ratio.
            { 255, 255, 255, 255 }
        );
        return;
    }

    NOTSA_GLOBAL_LOCAL(legendTraceHeight, 0xBAA350, (eRadarTraceHeight), {}); // = eRadarTraceHeight::RADAR_TRACE_LOW;
    NOTSA_GLOBAL_LOCAL(legendTraceTimer, 0xBAA354, (uint32), {}); // = CTimer::GetTimeInMS();

    if (CTimer::GetTimeInMSPauseMode() - legendTraceTimer > 600) {
        legendTraceTimer = CTimer::GetTimeInMSPauseMode();

        legendTraceHeight = [] {
            switch (legendTraceHeight) {
            case RADAR_TRACE_LOW:    return RADAR_TRACE_HIGH;
            case RADAR_TRACE_HIGH:   return RADAR_TRACE_NORMAL;
            case RADAR_TRACE_NORMAL: return RADAR_TRACE_LOW;
            default:
                NOTSA_UNREACHABLE();
                return (eRadarTraceHeight)(legendTraceHeight + 1); // 0x582EA4: the exe does `if (h == 2) h = 0; else h++;`
            }
        }();
    }

    // exe 0x582EB0-0x582EEC: `_ftol` (truncation) of stretch(8) + x, then back to float
    const auto posX = (float)NOTSA_FTOL(int32, SCREEN_STRETCH_X(8.0f) + (float)x);
    const auto posY = (float)NOTSA_FTOL(int32, SCREEN_STRETCH_Y(8.0f) + (float)y);

    const auto x4px = SCREEN_STRETCH_X(4.0f), y4px = SCREEN_STRETCH_Y(4.0f);
    const auto x5px = SCREEN_STRETCH_X(5.0f), y5px = SCREEN_STRETCH_Y(5.0f);
    const auto x7px = SCREEN_STRETCH_X(7.0f), y7px = SCREEN_STRETCH_Y(7.0f);

    const auto& arrowColor   = ArrowBlipColour[-blipType];
    const auto  outlineColor = CRGBA{0, 0, 0, 255};

    switch (legendTraceHeight) {
    case RADAR_TRACE_LOW: {
        CSprite2d::Draw2DPolygon(
            posX - x7px, posY + y7px,
            posX + x7px, posY + y7px,
            posX,        posY - x7px,
            posX,        posY - x7px,
            outlineColor
        );

        CSprite2d::Draw2DPolygon(
            posX + x5px, posY + y5px,
            posX - x5px, posY + y5px,
            posX,        posY - x5px,
            posX,        posY - x5px,
            arrowColor
        );
        break;
    }
    case RADAR_TRACE_HIGH: {
        CSprite2d::Draw2DPolygon(
            posX + x7px, posY - y7px,
            posX - x7px, posY - y7px,
            posX,        posY + y7px,
            posX,        posY + y7px,
            outlineColor
        );

        CSprite2d::Draw2DPolygon(
            posX + x5px, posY - y5px,
            posX - x5px, posY - y5px,
            posX,        posY + y5px,
            posX,        posY + y5px,
            arrowColor
        );
        break;
    }
    case RADAR_TRACE_NORMAL: {
        // The exe's rects are {left, bottom, right, top} = {x - dx, y + dy, x + dx, y - dy} with dy from the Y stretch (0x582F0E-0x582FCB)
        CSprite2d::DrawRect(
            {
                posX - x5px,
                posY + y5px,
                posX + x5px,
                posY - y5px
            },
            outlineColor
        );

        CSprite2d::DrawRect(
            {
                posX - x4px,
                posY + y4px,
                posX + x4px,
                posY - y4px
            },
            arrowColor
        );
        break;
    }
    }
}

/*!
 * @brief Limits a 2D vector to the radar. (which is a unit circle)
 * @brief This function does not effect the vector if the map is being drawn.
 * @param point The vector to be limited.
 * @addr 0x5832F0
 * @returns Magnitude of the vector before limiting.
 */
float CRadar::LimitRadarPoint(CVector2D& point) {
    // Exact x87 form of the exe (0x5832F0): x*x + y*y and the sqrt stay in extended range (float math would overflow to inf for |x| > 1.8e19) and the
    // magnitude is returned unrounded (the caller stores it); the normalisation multiplies by the extended-precision 1/mag, only the stores round to float
    const x87d x   = point.x;
    const x87d y   = point.y;
    const x87d mag = x87::sqrt(x * x + y * y);

    if (FrontEndMenuManager.m_bDrawingMap)
        return (float)mag;

    if (mag > 1.0) {
        // not in unit circle (NaN: no normalisation)
        const x87d r = (x87d)1.0 / mag;
        point.x = (float)(r * x);
        point.y = (float)(r * y);
    }

    return (float)mag;
}

// 0x583350
void CRadar::LimitToMap(float& x, float& y) {
    // Exact x87 form of the exe (0x583350): the zoom is truncated to an int16 (ftol), the bounds are `(origin -+ zoom) * (W * (1/640))`
    // and compared/stored without std::clamp (no assert on min > max, NaN keeps the input)
    using notsa::detail::Ftol;
    const x87d zoom = FrontEndMenuManager.m_bMapLoaded ? (x87d)(int16)Ftol((double)FrontEndMenuManager.m_fMapZoom) : (x87d)140.0;
    const x87d wk   = (x87d)RsGlobal.maximumWidth * (x87d)SCREEN_RECIPROCAL_X; // 0x859520
    const x87d hk   = (x87d)RsGlobal.maximumHeight * (x87d)SCREEN_RECIPROCAL_Y; // 0x859524

    const x87d minX = ((x87d)FrontEndMenuManager.m_vMapOrigin.x - zoom) * wk;
    if (minX > (double)x) {
        x = (float)minX;
    }
    const x87d maxX = (zoom + (x87d)FrontEndMenuManager.m_vMapOrigin.x) * wk;
    if (maxX < (double)x) {
        x = (float)maxX;
    }

    const x87d minY = ((x87d)FrontEndMenuManager.m_vMapOrigin.y - zoom) * hk;
    if (minY > (double)y) {
        y = (float)minY;
    }
    const x87d maxY = (zoom + (x87d)FrontEndMenuManager.m_vMapOrigin.y) * hk;
    if (maxY < (double)y) {
        y = (float)maxY;
    }
}

/*!
 * @brief Calculates transparency of a blip based on distance from the player.
 * @brief This function returns full opacity value if the map is being drawn.
 * @brief Blip transparency is not used in SA. It's a VC feature.
 * @param distance Distance from the player
 * @addr 0x583420
 * @returns Transparency value in range of [70,255].
 */
uint8 CRadar::CalculateBlipAlpha(float distance) {
    if (FrontEndMenuManager.m_bDrawingMap) {
        return 255;
    }

    // Exact form of the exe (0x583420): _ftol of dist * (1/6) * 255, `255 - that` as a signed int converted back to float (+2^32 when negative, i.e. as uint32),
    // max with 70.0, _ftol again (low byte returned)
    using notsa::detail::Ftol;
    const int32  alpha = 255 - Ftol((x87d)distance * (x87d)ExeRecip(6.0f) * (x87d)255.0);
    x87d       value = (x87d)alpha;
    if (alpha < 0) {
        value += (x87d)4294967296.0;
    }
    if (70.0 > value) {
        value = (x87d)70.0;
    }
    return (uint8)Ftol(value);
}

/*!
 * @brief Transforms a radar point to screen.
 * @addr 0x583480
 */
CVector2D CRadar::TransformRadarPointToScreenSpace(const CVector2D& in) {
    // CRadar::DrawRadarSection assumes this function won't spoil EDX, but it does, so we need to keep it up to data.
#ifndef NOTSA_PORTABLE
    _asm { push edx };
#endif
    if (FrontEndMenuManager.m_bDrawingMap) {
#ifndef NOTSA_PORTABLE
        _asm { pop edx };
#endif
        return {
            FrontEndMenuManager.m_vMapOrigin.x + FrontEndMenuManager.m_fMapZoom * in.x,
            FrontEndMenuManager.m_vMapOrigin.y - FrontEndMenuManager.m_fMapZoom * in.y
        };
    } else {
#ifndef NOTSA_PORTABLE
        _asm { pop edx };
#endif
        // Exact x87 form of the exe (0x583480), everything stays in extended precision: wk = W * (1/640), hk = (1/448) * H
        const x87d wk = (x87d)RsGlobal.maximumWidth * (x87d)SCREEN_RECIPROCAL_X; // 0x859520
        const x87d hk = (x87d)SCREEN_RECIPROCAL_Y * (x87d)RsGlobal.maximumHeight; // 0x859524
        const x87d a  = (x87d)94.0 * wk;
        const x87d g  = (x87d)76.0 * hk;
        return {
            (float)(((x87d)0.5 * a + wk * (x87d)40.0) + (a * (x87d)in.x) * (x87d)0.5),
            (float)(((x87d)RsGlobal.maximumHeight - hk * (x87d)104.0 + g * (x87d)0.5) - (g * (x87d)in.y) * (x87d)0.5)
        };
    }
}

/*!
 * @brief Transforms a real coordinate to radar coordinate.
 * @addr 0x583530
 */
CVector2D CRadar::TransformRealWorldPointToRadarSpace(const CVector2D& in) {
    // Exact x87 form of the exe (0x583530): k = 1.0 / range (extended); k * dx is STORED as a float temp, k * dy stays extended; both rotated with the cached sin / cos
    const x87d k  = (x87d)1.0 / (x87d)m_radarRange;
    const x87d dx = (x87d)in.x - (x87d)vec2DRadarOrigin.x;
    const x87d dy = (x87d)in.y - (x87d)vec2DRadarOrigin.y;
    const float  tx = (float)(k * dx);
    const x87d ty = k * dy;
    const x87d sn = cachedSin, cs = cachedCos;
    return {
        (float)(sn * ty + cs * (x87d)tx),
        (float)(cs * ty - sn * (x87d)tx)
    };
}

/*!
 * @brief Transforms a radar coordinate to real coordinate. (Unused)
 * @addr 0x5835A0
 */
CVector2D CRadar::TransformRadarPointToRealWorldSpace(const CVector2D& in) {
    return CachedRotateCounterclockwise(in) * m_radarRange + vec2DRadarOrigin;
}

/*!
 * @brief Transforms a radar coordinate to texture coordinate. (Inlined, see CRadar::DrawRadarSection)
 * @addr 0x583600
 */
CVector2D CRadar::TransformRealWorldToTexCoordSpace(const CVector2D& in, int32 x, int32 y) {
    // exe 0x5863DE-0x586401: multiplied by the float constants 0.002 / -0.002 (0x858F44 / 0x866B7C), not divided by 500
    return CVector2D{
        (in.x - (float(500 * x) - 3000.0f)) * 0.002f,
        (in.y - ((500 * float(MAX_RADAR_HEIGHT_TILES - y)) - 3000.0f)) * -0.002f
    };
}

// 0x583670
namespace {
// atan2(y, x) via fpatan (the extended result stays on the FPU stack), stored as the radar orientation, and its sin / cos computed from the UNROUNDED angle (0x5837B5 / 0x5837E9)
#ifdef NOTSA_PORTABLE
// portable: the same fld/fpatan/fst/fsin/fcos sequence on the soft x87 (X87Sites.h, PC24): fpatan's unrounded result is stored as the orientation (rounded to float) and its sin / cos are
// taken from the UNROUNDED value (tests/portable/f4a_lift_test.cpp rows Radar_SetRadarAngle*: bit-exact vs the asm)
float* const s_pRadarOrientation = &CRadar::m_fRadarOrientation;
float* const s_pRadarSin         = &CRadar::cachedSin;
float* const s_pRadarCos         = &CRadar::cachedCos;
void SetRadarAngleAtan2(float y, float x) { notsa::fp::sites::RadarAngleAtan2(y, x, s_pRadarOrientation, s_pRadarSin, s_pRadarCos); }
void SetRadarAngle(float a) { notsa::fp::sites::RadarAngle(a, s_pRadarOrientation, s_pRadarSin, s_pRadarCos); }
#else
#ifdef NOTSA_GLOBALS_DETACHED
// detached: the asm stores through pointers to the globals (same fld/fst/fsin/fstp sequence; eax is scratch in cdecl)
float* const s_pRadarOrientation = &CRadar::m_fRadarOrientation;
float* const s_pRadarSin         = &CRadar::cachedSin;
float* const s_pRadarCos         = &CRadar::cachedCos;
__declspec(naked) void __cdecl SetRadarAngleAtan2(float y, float x) {
    __asm {
        fld  dword ptr [esp + 4]
        fld  dword ptr [esp + 8]
        fpatan
        mov  eax, s_pRadarOrientation
        fst  dword ptr [eax]
        fld  st(0)
        fsin
        mov  eax, s_pRadarSin
        fstp dword ptr [eax]
        fcos
        mov  eax, s_pRadarCos
        fstp dword ptr [eax]
        ret
    }
}
__declspec(naked) void __cdecl SetRadarAngle(float a) {
    __asm {
        fld  dword ptr [esp + 4]
        mov  eax, s_pRadarOrientation
        fst  dword ptr [eax]
        fld  st(0)
        fsin
        mov  eax, s_pRadarSin
        fstp dword ptr [eax]
        fcos
        mov  eax, s_pRadarCos
        fstp dword ptr [eax]
        ret
    }
}
#else
__declspec(naked) void __cdecl SetRadarAngleAtan2(float y, float x) {
    __asm {
        fld  dword ptr [esp + 4]
        fld  dword ptr [esp + 8]
        fpatan
        fst  dword ptr ds:[0xBA8310]
        fld  st(0)
        fsin
        fstp dword ptr ds:[0xBA830C]
        fcos
        fstp dword ptr ds:[0xBA8308]
        ret
    }
}
// a float angle (0x5837F5)
__declspec(naked) void __cdecl SetRadarAngle(float a) {
    __asm {
        fld  dword ptr [esp + 4]
        fst  dword ptr ds:[0xBA8310]
        fld  st(0)
        fsin
        fstp dword ptr ds:[0xBA830C]
        fcos
        fstp dword ptr ds:[0xBA8308]
        ret
    }
}
#endif
#endif // NOTSA_PORTABLE
#line 529
}

// 0x583670
void CRadar::CalculateCachedSinCos() {
    if (FrontEndMenuManager.m_bDrawingMap) {
        cachedSin = x87::sin(0.0f);
        cachedCos = x87::cos(0.0f);

        return;
    }

    if (TheCamera.GetLookDirection() == LOOKING_DIRECTION_FORWARD) {
        if (const auto* m = TheCamera.m_matrix) { // heading of the camera matrix = atan2(-top.x, top.y)
            SetRadarAngleAtan2(-m->GetForward().x, m->GetForward().y);
        } else {
            SetRadarAngle(TheCamera.m_placement.m_fHeading);
        }
        return;
    }

    const auto& activeCam = CCamera::GetActiveCamera();
    const auto targetEntity = activeCam.m_pCamTargetEntity;

    const auto directionToTarget = [&] {
        if (activeCam.m_nMode == MODE_1STPERSON || TheCamera.GetLookDirection() == LOOKING_DIRECTION_UNKNOWN_1) {
            return targetEntity->GetMatrix().GetForward().Normalized();
        } else {
            return targetEntity->GetPosition() - activeCam.m_vecSourceBeforeLookBehind;
        }
    }();

    SetRadarAngleAtan2(-directionToTarget.x, directionToTarget.y);
}

/*!
 * @brief Creates a new coordinate blip (i.e. tracing blip with no sprite)
 * @param type Type
 * @param posn Position
 * @param color Color (Unused)
 * @param blipDisplay Display option
 * @param scriptName Script name (Unused) (from Android)
 * @addr 0x583820
 * @returns Handle of the blip created.
 */
tBlipHandle CRadar::SetCoordBlip(eBlipType type, CVector posn, eBlipColour color, eBlipDisplay blipDisplay, const char* scriptName) {
    auto index = FindTraceNotTrackingBlipIndex();
    if (index == -1)
        return -1;

    auto& t = ms_RadarTrace[index];

    t.m_vPosition        = posn;
    t.m_nBlipDisplayFlag = blipDisplay;
    t.m_nBlipType        = type;
    t.m_nColour          = BLIP_COLOUR_DESTINATION;
    t.m_fSphereRadius    = 1.f;
    t.m_nEntityHandle    = 0;
    t.m_nBlipSize        = 1;
    t.m_nBlipSprite      = eRadarSprite::RADAR_SPRITE_NONE;
    t.m_bBright          = true;
    t.m_bTrackingBlip    = true;
    t.m_bShortRange      = false; // the exe stores the flag byte [+0x25] = 3: every other flag of the reused slot is cleared too
    t.m_bFriendly        = false;
    t.m_bBlipRemain      = false;
    t.m_bBlipFade        = false;
    t.m_nCoordBlipAppearance = BLIP_FLAG_FRIEND;
    t.m_pEntryExit       = nullptr;

    return GetNewUniqueBlipIndex(index);
}

/*!
 * @brief Creates a new short-range coordinate blip (i.e. tracing blip with no sprite)
 * @param type Type
 * @param posn Position
 * @param color Color
 * @param blipDisplay Display option
 * @param scriptName Script name (Unused) (from Android)
 * @addr 0x583920
 * @returns Handle of the blip created.
 */
tBlipHandle CRadar::SetShortRangeCoordBlip(eBlipType type, CVector posn, eBlipColour color, eBlipDisplay blipDisplay, const char* scriptName) {
    if (const auto blip = SetCoordBlip(type, posn, color, blipDisplay, scriptName); blip != -1) {
        ms_RadarTrace[GetActualBlipArrayIndex(blip)].m_bShortRange = true;
        return blip;
    }

    return -1;
}

/*!
 * @brief Creates a new coordinate blip (i.e. tracing blip with no sprite) bound to an entity.
 * @param type Type
 * @param entityHandle Opaque entity handle? (CEntity*?)
 * @param arg2 Unknown argument (Unused)
 * @param blipDisplay Display option
 * @addr 0x5839A0
 * @returns Handle of the blip created.
 */
tBlipHandle CRadar::SetEntityBlip(eBlipType type, int32 entityHandle, uint32 arg2, eBlipDisplay blipDisplay) {
    if (const auto index = FindTraceNotTrackingBlipIndex(); index != -1) {
        auto& t = ms_RadarTrace[index];

        t.m_nBlipDisplayFlag = blipDisplay;
        t.m_nColour = (type == BLIP_CHAR || type == BLIP_CAR) ? BLIP_COLOUR_THREAT : BLIP_COLOUR_GREEN;
        t.m_nEntityHandle = entityHandle;
        t.m_fSphereRadius = 1.f;
        t.m_nBlipSize = 1;
        t.m_nBlipDisplayFlag = blipDisplay;
        t.m_nBlipType = type;
        t.m_nBlipSprite = eRadarSprite::RADAR_SPRITE_NONE;
        t.m_bBright = true;
        t.m_bTrackingBlip = true;
        t.m_pEntryExit = nullptr;

        return GetNewUniqueBlipIndex(index);
    }

    return -1;
}

/*!
 * @brief Change blip color to one of the color of a radar color set.
 * @addr 0x583AB0
 */
void CRadar::ChangeBlipColour(tBlipHandle blip, eBlipColour color) {
    if (const auto index = GetActualBlipArrayIndex(blip); index != -1) {
        ms_RadarTrace[index].m_nColour = color;
    }
}

/*!
 * @brief Checks if the blip is revealed.
 * @param blipIndex Index of the blip in `ms_RadarTrace`
 * @addr 0x583AF0
 * @returns True if it's revealed to the player.
 */
bool CRadar::HasThisBlipBeenRevealed(int32 blipIndex) {
    if (!FrontEndMenuManager.m_bDrawingMap
        || !ms_RadarTrace[blipIndex].m_bShortRange
        || CTheZones::ZonesRevealed > 80
        || CTheZones::GetCurrentZoneLockedOrUnlocked(ms_RadarTrace[blipIndex].m_vPosition)
    ) {
        return true;
    }
    return false;
}

/*!
 * @brief Checks if a blip sprite is allowed to be drawn right now.
 * @param spriteId Blip sprite
 * @param priority Importance of displaying the blip.
 * @addr 0x583B40
 * @returns True if it's allowed to be drawn.
 */
bool CRadar::DisplayThisBlip(eRadarSprite spriteId, int8 priority) {
    if (CGame::CanSeeOutSideFromCurrArea() && FindPlayerPed()->GetAreaCode() == AREA_CODE_NORMAL_WORLD) {
        switch (spriteId) {
        case RADAR_SPRITE_NONE:
        case RADAR_SPRITE_WHITE:
        case RADAR_SPRITE_CENTRE:
        case RADAR_SPRITE_MAP_HERE:
        case RADAR_SPRITE_NORTH:
            return true;

        case RADAR_SPRITE_AIRYARD:
        case RADAR_SPRITE_AMMUGUN:
        case RADAR_SPRITE_BARBERS:
        case RADAR_SPRITE_BOATYARD:
        case RADAR_SPRITE_BURGERSHOT:
        case RADAR_SPRITE_BULLDOZER:
        case RADAR_SPRITE_CHICKEN:
        case RADAR_SPRITE_DINER:
        case RADAR_SPRITE_HOSTPITAL:
        case RADAR_SPRITE_MODGARAGE:
        case RADAR_SPRITE_PIZZA:
        case RADAR_SPRITE_POLICE:
        case RADAR_SPRITE_RACE:
        case RADAR_SPRITE_SAVEGAME:
        case RADAR_SPRITE_SCHOOL:
        case RADAR_SPRITE_TATTOO:
        case RADAR_SPRITE_TSHIRT:
        case RADAR_SPRITE_DATEDISCO:
        case RADAR_SPRITE_DATEDRINK:
        case RADAR_SPRITE_DATEFOOD:
        case RADAR_SPRITE_TRUCK:
        case RADAR_SPRITE_CASH:
        case RADAR_SPRITE_FLAG:
        case RADAR_SPRITE_GYM:
        case RADAR_SPRITE_IMPOUND:
        case RADAR_SPRITE_SPRAY:
            return (FrontEndMenuManager.m_ShowLocationsBlips && priority < 0) || priority == 1;

        case RADAR_SPRITE_BIGSMOKE:
        case RADAR_SPRITE_CATALINAPINK:
        case RADAR_SPRITE_CESARVIAPANDO:
        case RADAR_SPRITE_CJ:
        case RADAR_SPRITE_CRASH1:
        case RADAR_SPRITE_EMMETGUN:
        case RADAR_SPRITE_GIRLFRIEND:
        case RADAR_SPRITE_LOGOSYNDICATE:
        case RADAR_SPRITE_MADDOG:
        case RADAR_SPRITE_MAFIACASINO:
        case RADAR_SPRITE_MCSTRAP:
        case RADAR_SPRITE_OGLOC:
        case RADAR_SPRITE_RYDER:
        case RADAR_SPRITE_QMARK:
        case RADAR_SPRITE_SWEET:
        case RADAR_SPRITE_THETRUTH:
        case RADAR_SPRITE_TORENORANCH:
        case RADAR_SPRITE_TRIADS:
        case RADAR_SPRITE_TRIADSCASINO:
        case RADAR_SPRITE_WOOZIE:
        case RADAR_SPRITE_ZERO:
        case RADAR_SPRITE_GANGB:
        case RADAR_SPRITE_GANGP:
        case RADAR_SPRITE_GANGY:
        case RADAR_SPRITE_GANGN:
        case RADAR_SPRITE_GANGG:
            return (FrontEndMenuManager.m_ShowContactsBlips && priority < 0) || priority == 3;

        default:
            return (FrontEndMenuManager.m_ShowOtherBlips && priority < 0) || priority == 2;
        }
    }

    switch (spriteId) {
    case RADAR_SPRITE_NONE:
    case RADAR_SPRITE_WHITE:
    case RADAR_SPRITE_CENTRE:
    case RADAR_SPRITE_MAP_HERE:
    case RADAR_SPRITE_NORTH:
    case RADAR_SPRITE_MAFIACASINO:
    case RADAR_SPRITE_SCHOOL:
    case RADAR_SPRITE_WAYPOINT:
    case RADAR_SPRITE_TRIADSCASINO:
    case RADAR_SPRITE_CASH:
        return true;

    default:
        return false;
    }
}

/*!
 * @brief Changes the brightness of a blip. (Unused)
 * @param blip Blip handle
 * @param brightness Brightness flag. (=1: not bright (dim), ≠1: bright)
 * @addr 0x583C70
 */
void CRadar::ChangeBlipBrightness(tBlipHandle blip, int32 brightness) {
    if (const auto index = GetActualBlipArrayIndex(blip); index != -1) {
        ms_RadarTrace[index].m_bBright = brightness == 1 ? false : true; // 0x583CA1: brightness == 1 CLEARS the bright bit (and byte [+0x25], 0xFE), anything else sets it
    }
}

/*!
 * @brief Changes the size of a blip.
 * @brief This function does not effect the blip size if the map is being drawn.
 * @addr 0x583CC0
 */
void CRadar::ChangeBlipScale(tBlipHandle blip, int32 size) {
    if (const auto index = GetActualBlipArrayIndex(blip); index != -1) {
        ms_RadarTrace[index].m_nBlipSize = FrontEndMenuManager.m_bDrawingMap ? 1 : size;
    }
}

/*!
 * @brief Changes the display option of a blip.
 * @addr 0x583D20
 */
void CRadar::ChangeBlipDisplay(tBlipHandle blip, eBlipDisplay blipDisplay) {
    if (const auto index = GetActualBlipArrayIndex(blip); index != -1) {
        ms_RadarTrace[index].m_nBlipDisplayFlag = blipDisplay;
    }
}

/*!
 * @brief Changes the sprite of a blip.
 * @param blip Blip
 * @param spriteId Blip sprite
 * @addr 0x583D70
 */
void CRadar::SetBlipSprite(tBlipHandle blip, eRadarSprite spriteId) {
    if (const auto index = GetActualBlipArrayIndex(blip); index != -1 && ms_RadarTrace[index].m_bTrackingBlip) {
        ms_RadarTrace[index].m_nBlipSprite = spriteId;
    }
}

/*!
 * @brief Sets a blip to shown always in zoom.
 * @addr 0x583DB0
 */
void CRadar::SetBlipAlwaysDisplayInZoom(tBlipHandle blip, bool display) {
    if (const auto index = GetActualBlipArrayIndex(blip); index != -1 && ms_RadarTrace[index].m_bTrackingBlip) {
        ms_RadarTrace[index].m_bBlipRemain = display;
    }
}

/*!
 * @brief Sets a blip to fade. (Unused?)
 * @addr 0x583E00
 */
void CRadar::SetBlipFade(tBlipHandle blip, bool fade) {
    if (const auto index = GetActualBlipArrayIndex(blip); index != -1) {
        ms_RadarTrace[index].m_bBlipFade = fade;
    }
}

/*!
 * @brief Changes the appearnace of a blip.
 * @addr 0x583E50
 */
void CRadar::SetCoordBlipAppearance(tBlipHandle blip, eBlipAppearance appearance) {
    if (const auto index = GetActualBlipArrayIndex(blip); index != -1) {
        auto& trace = ms_RadarTrace[index];
        if (trace.m_nBlipType != eBlipType::BLIP_COORD) { // exe: (flags[+0x26] & 0x3C) == 0x10
            return;
        }

        switch (appearance) { // Seems like there's a 3rd appearance type?
        case BLIP_FLAG_FRIEND:
        case BLIP_FLAG_THREAT:
        case BLIP_FLAG_UNK:
            trace.m_nCoordBlipAppearance = appearance; // exe: the top 2 bits of the flag byte +0x25 (read by DrawCoordBlip)
            break;
        }
    }
}

/*!
 * @brief Sets a blip as friendly.
 * @addr 0x583EB0
 */
void CRadar::SetBlipFriendly(tBlipHandle blip, bool friendly) {
    if (const auto index = GetActualBlipArrayIndex(blip); index != -1) {
        ms_RadarTrace[index].m_bFriendly = friendly;
    }
}

/*!
 * @brief Sets an entry point to a blip.
 * @addr 0x583F00
 */
void CRadar::SetBlipEntryExit(tBlipHandle blip, CEntryExit* enex) {
    if (const auto index = GetActualBlipArrayIndex(blip); index != -1) {
        auto& trace = ms_RadarTrace[index];
        if (trace.m_bTrackingBlip) {
            trace.m_pEntryExit = enex;
        }
    }
}

/*!
 * @brief Limits stretched X, Y values to the map
 * @brief This function does nothing if the map is not being drawn.
 * @brief This code piece seems fairly common.. Perhaps its inlined?
 */
void Limit(float& x, float& y) {
    if (FrontEndMenuManager.m_bDrawingMap) {
        x = SCREEN_STRETCH_X(x);
        y = SCREEN_STRETCH_Y(y);

        CRadar::LimitToMap(x, y);
    }
}

/*!
 * @brief Draws a square coordinate blip to the map.
 * @addr 0x583F40
 */
void CRadar::ShowRadarTrace(float x, float y, uint32 size, CRGBA color) {
    Limit(x, y);
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));

    // The exe stores both rects in memory as +0 x - s, +4 y + s, +8 x + s, +0xC y - s (screen y grows downwards); the CRect ctor
    // (identical to the exe's 0x4041C0) is (left, bottom(+0xC), right, top(+4)), so the literal is {x - s, y - s, x + s, y + s}.
    // The border one is 1px bigger on every side (x87 does (x - s) - 1 in float steps) and uses black with the trace alpha.
    const auto s = float(size);
    CSprite2d::DrawRect({
        (x - s) - 1.f,
        (y - s) - 1.f,
        (x + s) + 1.f,
        (y + s) + 1.f
    }, { 0, 0, 0, color.a });

    CSprite2d::DrawRect({
        x - s,
        y - s,
        x + s,
        y + s
    }, color);
}

/*!
 * @brief Draws a coordinate blip to the map with height information.
 * @addr 0x584070
 */
void CRadar::ShowRadarTraceWithHeight(float x, float y, uint32 size, uint32 R, uint32 G, uint32 B, uint32 A, eRadarTraceHeight height) {
    Limit(x, y);
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));

    // NOTE: Those RGBA parameters are 4-bytes per channel.
    // then we construct 1-byte per channel CRGBA. It's stupid isn't it?
    // NOTE: {R,G,B,A} > 255 is possible, so we can't assert for x <= UCHAR_MAX.
    const auto r = static_cast<uint8>(R), g = static_cast<uint8>(G), b = static_cast<uint8>(B), a = static_cast<uint8>(A);

    const auto size0 = float(size + 0), size1 = float(size + 1);
    const auto size2 = float(size + 2), size3 = float(size + 3);

    switch (height) {
    case RADAR_TRACE_HIGH:
        CSprite2d::Draw2DPolygon( // draw black border
            x,                           y + SCREEN_STRETCH_Y(size3),
            x,                           y + SCREEN_STRETCH_Y(size3),
            x + SCREEN_STRETCH_X(size3), y - SCREEN_STRETCH_Y(size2),
            x - SCREEN_STRETCH_X(size3), y - SCREEN_STRETCH_Y(size2),
            { 0, 0, 0, a }
        );
        CSprite2d::Draw2DPolygon( // draw triangle
            x,                           y + SCREEN_STRETCH_Y(size1),
            x,                           y + SCREEN_STRETCH_Y(size1),
            x + SCREEN_STRETCH_X(size1), y - SCREEN_STRETCH_Y(size1),
            x - SCREEN_STRETCH_X(size1), y - SCREEN_STRETCH_Y(size1),
            { r, g, b, a }
        );
        break;
    case RADAR_TRACE_NORMAL: // exe rects in memory: +0 x - dx, +4 y + dy, +8 x + dx, +0xC y - dy (screen y grows downwards); CRect ctor is (left, bottom(+0xC), right, top(+4))
        CSprite2d::DrawRect( // draw black border
            {
                x - SCREEN_STRETCH_X(size1), y - SCREEN_STRETCH_Y(size1),
                x + SCREEN_STRETCH_X(size1), y + SCREEN_STRETCH_Y(size1)
            },
            { 0, 0, 0, a }
        );

        CSprite2d::DrawRect( // draw box
            {
                x - SCREEN_STRETCH_X(size0), y - SCREEN_STRETCH_Y(size0),
                x + SCREEN_STRETCH_X(size0), y + SCREEN_STRETCH_Y(size0)
            },
            { r, g, b, a }
        );
        break;
    case RADAR_TRACE_LOW:
        CSprite2d::Draw2DPolygon( // draw black border
            x + SCREEN_STRETCH_X(size3), y + SCREEN_STRETCH_Y(size2),
            x - SCREEN_STRETCH_X(size3), y + SCREEN_STRETCH_Y(size2),
            x,                           y - SCREEN_STRETCH_Y(size3),
            x,                           y - SCREEN_STRETCH_Y(size3),
            { 0, 0, 0, a }
        );

        CSprite2d::Draw2DPolygon( // draw triangle
            x + SCREEN_STRETCH_X(size1), y + SCREEN_STRETCH_Y(size1),
            x - SCREEN_STRETCH_X(size1), y + SCREEN_STRETCH_Y(size1),
            x,                           y - SCREEN_STRETCH_Y(size1),
            x,                           y - SCREEN_STRETCH_Y(size1),
            { r, g, b, a }
        );
        break;
    }
}

// show debug line at this position
// 0x584480
void CRadar::ShowRadarMarker(CVector p, uint32 color, float radius) {
    auto& mat   = TheCamera.m_mCameraMatrix;
    auto& up    = mat.GetUp();
    auto& right = mat.GetRight();

    auto r0 = radius * 0.5f;
    auto r1 = radius * 1.4f;

    CTheScripts::ScriptDebugLine3D(p + r1 * up,    p + r0 * up,    color, color);
    CTheScripts::ScriptDebugLine3D(p - r1 * up,    p - r0 * up,    color, color);
    CTheScripts::ScriptDebugLine3D(p + r1 * right, p + r0 * right, color, color);
    CTheScripts::ScriptDebugLine3D(p - r1 * right, p - r0 * right, color, color);
}

/*!
 * @brief Get game color from the blip color.
 * @addr 0x584770
 * @returns Game color
 */
uint32 CRadar::GetRadarTraceColour(eBlipColour color, bool bright, bool friendly) {
    switch (color) {
    case BLIP_COLOUR_RED:
    case BLIP_COLOUR_REDCOPY:
        return HudColour.GetIntColour(bright ? HUD_COLOUR_RED : HUD_COLOUR_DARK_RED);
    case BLIP_COLOUR_GREEN:
        return HudColour.GetIntColour(bright ? HUD_COLOUR_GREEN : HUD_COLOUR_DARK_GREEN);
    case BLIP_COLOUR_BLUE:
    case BLIP_COLOUR_BLUECOPY:
        return HudColour.GetIntColour(bright ? HUD_COLOUR_LIGHT_BLUE : HUD_COLOUR_BLUE);
    case BLIP_COLOUR_WHITE:
        return HudColour.GetIntColour(bright ? HUD_COLOUR_LIGHT_GRAY : HUD_COLOUR_DARK_GRAY);
    case BLIP_COLOUR_YELLOW:
        return HudColour.GetIntColour(bright ? HUD_COLOUR_CREAM : HUD_COLOUR_GOLD);
    case BLIP_COLOUR_THREAT:
        return HudColour.GetIntColour(friendly ? HUD_COLOUR_BLUE : HUD_COLOUR_RED);
    case BLIP_COLOUR_DESTINATION:
        return HudColour.GetIntColour(HUD_COLOUR_CREAM);
    default:
        return color;
    }
}

/*
 * @brief Draw a sprite to that is rotated.
 * @addr 0x584850
 */
void CRadar::DrawRotatingRadarSprite(CSprite2d& sprite, float x, float y, float angle, uint32 width, uint32 height, CRGBA color) {
    Limit(x, y);
    CVector2D verts[4];
    for (auto i = 0u; i < std::size(verts); i++) {
        const auto theta = (float)i * HALF_PI + (angle - FRAC_PI_4);

        verts[i].x = X87R(x87::sin(theta) * (float)width) + x;
        verts[i].y = X87R(x87::cos(theta) * (float)height) + y;
    }

    sprite.Draw(verts[3].x, verts[3].y, verts[2].x, verts[2].y, verts[0].x, verts[0].y, verts[1].x, verts[1].y, color);
}

// 0x584960
void CRadar::DrawYouAreHereSprite(float x, float y) {
    NOTSA_GLOBAL_LOCAL(mapYouAreHereTimer, 0xBAA358, (uint32), {});
    NOTSA_GLOBAL_LOCAL(mapYouAreHereDisplay, 0x8D0930, (bool), { true });

    // exe 0x584960-0x5849AF: the marker is shown for 500 ms and hidden for 200 ms (the port toggled every 700 ms)
    if (CTimer::GetTimeInMSPauseMode() - mapYouAreHereTimer > (mapYouAreHereDisplay ? 500u : 200u)) {
        mapYouAreHereTimer = CTimer::GetTimeInMSPauseMode();
        mapYouAreHereDisplay = !mapYouAreHereDisplay;
    }

    if (mapYouAreHereDisplay) {
        const float angle = (float)(X87R(FindPlayerHeadingExt(0) + (double)DegreesToRadians(180.0f))); // 0x5849B8..0x5849D0: FindPlayerHeading leaves the heading unrounded in ST0; `+ PI` [0x858CB8], then the store to float
        const auto circleAngle = angle + DegreesToRadians(90.0f);

        DrawRotatingRadarSprite(
            RadarBlipSprites[RADAR_SPRITE_MAP_HERE],
            x + X87R(17.0f * x87::cos(circleAngle)),
            y - X87R(17.0f * x87::sin(circleAngle)),
            angle,
            NOTSA_FTOL(uint32, SCREEN_STRETCH_X(25.0f)),
            NOTSA_FTOL(uint32, SCREEN_STRETCH_Y(25.0f)),
            { 255, 255, 255, 255 }
        );
    }

    MapLegendList[MapLegendCounter++] = RADAR_SPRITE_MAP_HERE;
}

// 0x584A80
void CRadar::SetupRadarRect(int32 x, int32 y) {
    m_radarRect.left   = float(500 * (x - 7));
    m_radarRect.top    = float(500 * (7 - y));
    m_radarRect.right  = float(500 * (x - 4));
    m_radarRect.bottom = float(500 * (4 - y));
}

// unused
// 0x584B00
// Returns true if either coords had to be clipped
bool ClipRadarTileCoords(int32& x, int32& y) {
    // Not quite the way they've done it, but nicer.

    const auto ox = x, oy = y;

    x = std::clamp(x, 0, (int32)MAX_RADAR_WIDTH_TILES - 1);
    y = std::clamp(y, 0, (int32)MAX_RADAR_HEIGHT_TILES - 1);

    return ox != x || oy != y;
}

// 0x584B50
void CRadar::RequestMapSection(int32 x, int32 y) {
    if (!IsMapSectionInBounds(x, y))
        return;

    if (const auto texture = gRadarTextures[y][x]; texture != -1) {
        CStreaming::RequestTxdModel(texture, STREAMING_GAME_REQUIRED | STREAMING_KEEP_IN_MEMORY);
    }
}

// 0x584BB0
void CRadar::RemoveMapSection(int32 x, int32 y) {
    if (!IsMapSectionInBounds(x, y))
        return;

    if (const auto texture = gRadarTextures[y][x]; texture != -1) {
        CStreaming::RemoveTxdModel(texture);
    }
}

// 0x584BF0
void CRadar::RemoveRadarSections() {
    // exe 0x584BF0-0x584C46: x is the outer loop, y the inner one, and unassigned slots (-1) are skipped
    for (auto x = 0u; x < MAX_RADAR_WIDTH_TILES; x++) {
        for (auto y = 0u; y < MAX_RADAR_HEIGHT_TILES; y++) {
            if (gRadarTextures[y][x] != -1) {
                CStreaming::RemoveTxdModel(gRadarTextures[y][x]);
            }
        }
    }
}

/*
 * @brief Checks if a 2D point is inside the radar. (i.e. a rect that spans -1 to 1 in both directions)
 * @addr 0x584D40
 */
bool IsPointInsideRadar(const CVector2D& point) {
    // exe (0x584D40): fcomp / test ah,5 / test ah,0x41 -> only a real x < -1 or x > 1 fails; NaN and exactly +-1 pass
    return !(point.x < -1.0f) && !(point.x > 1.0f)
        && !(point.y < -1.0f) && !(point.y > 1.0f);
}

// 0x584D90
void GetTextureCorners(int32 x, int32 y, CVector2D* corners) {
    // Magic numbers probably come from `MAX_RADAR_WIDTH_TILES` and `MAX_RADAR_HEIGHT_TILES`
    // (the products are int multiplications that wrap, converted to float afterwards)
    const auto X0 = float((int32)((uint32)(x - 6) * 500u)), X1 = float((int32)((uint32)(x - 6) * 500u + 500u));
    const auto Y0 = float((int32)((uint32)(5 - y) * 500u)), Y1 = float((int32)((uint32)(5 - y) * 500u + 500u));

    corners[0] = {X0, Y0};
    corners[1] = {X1, Y0};
    corners[2] = {X1, Y1};
    corners[3] = {X0, Y1};
}

// Returns number of intersections
// 0x584E00
int32 LineRadarBoxCollision(CVector2D& result, const CVector2D& lineStart, const CVector2D& lineEnd) {
    // Exact x87 form of the exe (0x584E00): the distances, their product and the interpolation stay in extended range; the parameter of the first three edges is
    // stored as a float (and becomes the new closest), the last edge keeps it unrounded and does not update anything; there is no `param >= 0` test and no early return
    x87d closest = (x87d)1.0;
    int32  side    = -1;

    const auto Edge = [&](double a, double b, bool edgeIsX, float edgeCoord, int32 edgeSide, bool storeT) {
        if (!(X87R(a * b) < 0.0)) {
            return false;
        }
        x87d t = X87R(a / (X87R(a - b)));
        if (storeT) {
            t = (x87d)(float)t;
        }
        const x87d s   = edgeIsX ? lineStart.y : lineStart.x;
        const x87d e   = edgeIsX ? lineEnd.y : lineEnd.x;
        const x87d val = (e - s) * t + s;
        const float  v   = (float)val;
        if (val < -1.0 || !(v <= 1.0f) || !(t <= closest)) {
            return false;
        }
        if (edgeIsX) {
            result.x = edgeCoord;
            result.y = v;
        } else {
            result.x = v;
            result.y = edgeCoord;
        }
        if (storeT) {
            closest = t;
        }
        side = edgeSide;
        return true;
    };

    Edge(-(x87d)1.0 - (x87d)lineStart.x, -(x87d)1.0 - (x87d)lineEnd.x, true, -1.0f, 3, true);
    Edge((x87d)lineStart.x - (x87d)1.0, (x87d)lineEnd.x - (x87d)1.0, true, 1.0f, 1, true);
    Edge(-(x87d)1.0 - (x87d)lineStart.y, -(x87d)1.0 - (x87d)lineEnd.y, false, -1.0f, 0, true);
    Edge((x87d)lineStart.y - (x87d)1.0, (x87d)lineEnd.y - (x87d)1.0, false, 1.0f, 2, false);

    return side;
}

// 0x585040
int32 CRadar::ClipRadarPoly(CVector2D* out, const CVector2D* in) {
    int32 numOut   = 0;
    int32 lastSide = -1;

    // Unit square corners, indexed by side
    const CVector2D corners[4] = {{1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}, {-1.0f, -1.0f}};

    // NaN passes the check (original: FCOMP + JNP/JZ patterns)
    const auto IsInside = [](const CVector2D& p) {
        return !(p.x < -1.0f) && !(p.x > 1.0f) && !(p.y < -1.0f) && !(p.y > 1.0f);
    };
    const bool inside[4] = {IsInside(in[0]), IsInside(in[1]), IsInside(in[2]), IsInside(in[3])};

    for (int32 i = 0; i < 4; i++) {
        const CVector2D* cur = &in[i];
        if (inside[i]) {
            out[numOut++] = *cur;
            continue;
        }

        const int32 next = (i + 1) & 3;
        const int32 side1 = LineRadarBoxCollision(out[numOut], *cur, in[(i - 1) & 3]);
        if (side1 != -1) {
            numOut++;
            lastSide = side1;
        }

        CVector2D* const outPoint = &out[numOut];
        const int32 side2 = LineRadarBoxCollision(*outPoint, *cur, in[next]);
        if (side2 != -1) {
            if (side1 == -1) {
                if (lastSide == -1) {
                    for (int32 k = 3; k >= i; k--) { // i <= 3 is always true
                        CVector2D unused; // 0x585040 stores the result over its loop counter
                        const int32 side = k == 0
                            ? LineRadarBoxCollision(unused, in[0], in[3])
                            : LineRadarBoxCollision(unused, in[k], in[k - 1]);
                        if (side != -1) {
                            lastSide = side;
                            break;
                        }
                    }
                }

                const CVector2D savedPoint = *outPoint;
                CVector2D*      dst        = outPoint;
                for (int32 side = lastSide; side != side2; side = (side + 1) & 3) {
                    numOut++;
                    // The original indexes its corner table with -1 here and reads two stack slots (pointers) as floats:
                    // such bit patterns are denormals/tiny values, i.e. the point (~0, ~0)
                    *dst++ = side >= 0 ? corners[side] : CVector2D{0.0f, 0.0f};
                }
                out[numOut] = savedPoint;
            }
            numOut++;
        }
    }

    if (numOut == 0) {
        // Polygon encloses the whole radar? Slopes stay in extended precision in the original
        const x87d slope1 = (x87d(in[0].y) - x87d(in[1].y)) / (x87d(in[0].x) - x87d(in[1].x));
        const float  slope2 = (float)((x87d(in[0].y) - x87d(in[3].y)) / (x87d(in[0].x) - x87d(in[3].x)));
        if ((slope1 * in[3].x - in[3].y) * (slope1 * in[0].x - in[0].y) < 0.0
         && (x87d(slope2) * in[1].x - in[1].y) * (x87d(slope2) * in[0].x - in[0].y) < 0.0) {
            for (int32 k = 0; k < 4; k++) {
                out[k] = corners[k];
            }
            return 4;
        }
    }

    return numOut;
}

// 0x5853D0
void CRadar::DrawAreaOnRadar(const CRect& rect, const CRGBA& color, bool inMenu) {
    // The exe inlines the separation tests against m_radarRect (slots: left +0, top +4, right +8, bottom +0xc; 0x5853D4-0x58542B) and draws only if the rects overlap
    // (NaN falls through to drawing, as in the exe). CRect::IsRectInside had the polarity and the top/bottom pairing wrong, so the early-out never fired.
    if (!inMenu && (rect.left > m_radarRect.right || rect.right < m_radarRect.left || rect.bottom > m_radarRect.top || rect.top < m_radarRect.bottom)) {
        return;
    }

    // Corner positions - Not transformed
    const CVector2D rectCorners[]{
        { rect.left,  rect.bottom},
        { rect.right, rect.bottom},
        { rect.right, rect.top   },
        { rect.left,  rect.top   }
    };

    // Corner positions - Transformed, not yet clipped
    CVector2D polyUnclippedVertices[4];
    rng::transform(rectCorners, polyUnclippedVertices, [](auto&& v) {
        return TransformRealWorldPointToRadarSpace(v);
    });

    CVector2D polyVertices[8];
    const auto numVerts = ClipRadarPoly(polyVertices, polyUnclippedVertices);
    if (!numVerts) {
        return;
    }

    CVector2D texCoords[8]{};
    const auto scaleX = SCREEN_STRETCH_X(1.0f);
    const auto scaleY = SCREEN_STRETCH_Y(1.0f);
    for (auto i = 0; i < numVerts; i++) {
        texCoords[i] = TransformRadarPointToScreenSpace(polyVertices[i]);
        if (FrontEndMenuManager.m_bDrawingMap) {
            texCoords[i] *= CVector2D(scaleX, scaleY);
            polyVertices[i] *= CVector2D(scaleX, scaleY);
        }
    }
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(NULL));
    CSprite2d::SetVertices(numVerts, texCoords, polyVertices, color);
    if (numVerts > 2) {
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, CSprite2d::GetVertices(), numVerts);
    }
}

// 0x585700
void CRadar::DrawRadarMask() {
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,         RWRSTATE(rwSHADEMODEFLAT));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, RWRSTATE(rwALPHATESTFUNCTIONALWAYS));

    CVector2D out[8]{};
    CVector2D in;
    // exe 0x585715-0x58573D: the corners are (-1, 1), (1, 1), (1, -1), (-1, -1) in this order
    CVector2D corners[4] = {
        { -1.0f, 1.0f  },
        { 1.0f,  1.0f  },
        { 1.0f,  -1.0f },
        { -1.0f, -1.0f }
    };

    // exe 0x5857D0-0x58580E: first the whole radar square is drawn at the FAR screen z (clears the mask area), then the corner segments at the near z
    {
        CVector2D square[4];
        for (auto i = 0; i < 4; i++) {
            square[i] = TransformRadarPointToScreenSpace(corners[i]);
        }
        CSprite2d::SetMaskVertices(std::size(square), square, RwIm2DGetFarScreenZ());
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, CSprite2d::GetVertices(), std::size(square));
    }

    // Draw the shape we want to mask out from the radar in four segments
    for (auto& corner : corners) {
        // First point is always the corner itself
        in.x = corner.x;
        in.y = corner.y;
        out[0] = TransformRadarPointToScreenSpace(in);

        // Then generate a quarter of the circle
        for (auto j = 0; j < 7; j++) {
            in.x = X87R(corner.x * x87::cos(float(j) * (FRAC_PI_2 / 6.0f)));
            in.y = X87R(corner.y * x87::sin(float(j) * (FRAC_PI_2 / 6.0f)));
            out[j + 1] = TransformRadarPointToScreenSpace(in);
        };

        CSprite2d::SetMaskVertices(std::size(out), out, CSprite2d::GetNearScreenZ());
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, CSprite2d::GetVertices(), std::size(out));
    }

    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, RWRSTATE(rwALPHATESTFUNCTIONGREATER));
}

// 0x5858D0
void CRadar::StreamRadarSections(const CVector& worldPosn) {
    if (!CStreaming::ms_disableStreaming) {
        StreamRadarSections(
            NOTSA_FTOL(int32, std::floor((worldPosn.x + 3000.0f) * ExeRecip(500.0f))),
            NOTSA_FTOL(int32, std::ceil(11.0f - (worldPosn.y + 3000.0f) * ExeRecip(500.0f)))
        );
    }
}

// 0x584C50
void CRadar::StreamRadarSections(int32 x, int32 y) {
    // exe 0x584C50-0x584D37: x is the outer loop and y the inner one, so the request / remove calls reach CStreaming in column order
    for (auto curX = 0; curX < MAX_RADAR_WIDTH_TILES; curX++) {
        for (auto curY = 0; curY < MAX_RADAR_HEIGHT_TILES; curY++) {
            if (gRadarTextures[curY][curX] != -1 && curY >= 0 && curY < MAX_RADAR_HEIGHT_TILES && curX >= 0 && curX < MAX_RADAR_WIDTH_TILES) {
                if (curY >= y - 1 && curY <= y + 1 && curX >= x - 1 && curX <= x + 1) {
                    CStreaming::RequestModel(TXDToModelId(gRadarTextures[curY][curX]), STREAMING_KEEP_IN_MEMORY | STREAMING_GAME_REQUIRED);
                } else {
                    CStreaming::RemoveModel(TXDToModelId(gRadarTextures[curY][curX]));
                }
            }
        }
    }
}

// 0x585960
void CRadar::InitFrontEndMap() {
    CalculateCachedSinCos();
    rng::fill(MapLegendList, RADAR_SPRITE_NONE);

    vec2DRadarOrigin.Set(0.0f, 0.0f);
    m_radarRange = WORLD_BOUND_RANGE - 10.0f;
    MapLegendCounter = 0;

    rng::fill(ArrowBlipColour, CRGBA(0, 0, 0, 0));
}

// 0x5859F0
void CRadar::AddBlipToLegendList(bool noSprite, int32 blipIndex) {
    if (!FrontEndMenuManager.m_bDrawingMap)
        return;

    auto& trace = ms_RadarTrace[blipIndex];

    const auto sprite = [&] {
        if (!noSprite)
            return eRadarSprite(blipIndex);

        switch (trace.m_nBlipType) {
        case BLIP_CAR:
        case BLIP_CHAR:
            return trace.m_bFriendly ? RADAR_SPRITE_FRIEND : RADAR_SPRITE_THREAT;
        case BLIP_OBJECT:
            return RADAR_SPRITE_OBJECT;
        case BLIP_COORD:
            return RADAR_SPRITE_DESTINATION;
        default:
            return RADAR_SPRITE_PLAYER_INTEREST;
        }
    }();

    // check if it's not already added.
    if (!notsa::contains(MapLegendList, sprite)) {
        MapLegendList[MapLegendCounter++] = sprite;

        if (noSprite) {
            const CRGBA color = GetRadarTraceColour(trace.m_nColour, trace.m_bBright, trace.m_bFriendly);

            ArrowBlipColour[-sprite].Set(color, 255);
        }
    }
}

// 0x585B20
void CRadar::SetMapCentreToPlayerCoords() {
    if (FindPlayerPed() == nullptr)
        return;

    FrontEndMenuManager.m_bDrawingMap = true;

    InitFrontEndMap();

    CVector2D posReal = FindPlayerCentreOfWorldForMap(0);

    if (CTheScripts::HideAllFrontEndMapBlips || CTheScripts::bPlayerIsOffTheMap)
        posReal.Set(0.0f, 0.0f);

    auto posRadar = TransformRealWorldPointToRadarSpace(posReal);
    LimitRadarPoint(posRadar);

    FrontEndMenuManager.m_vMousePos = posReal;
    FrontEndMenuManager.m_vMapOrigin.x = DEFAULT_SCREEN_WIDTH  / 2.0f - FrontEndMenuManager.m_fMapZoom * posRadar.x;
    FrontEndMenuManager.m_vMapOrigin.y = DEFAULT_SCREEN_HEIGHT / 2.0f + FrontEndMenuManager.m_fMapZoom * posRadar.y;
    FrontEndMenuManager.m_bDrawingMap = false;
}

// 0x585BF0
void CRadar::Draw3dMarkers() {
    const auto PutMarkerCone = [](uint32 id, CVector pos, float size, CRGBA color) {
        C3dMarkers::PlaceMarkerCone(id, pos, size, color.r, color.g, color.b, 255, 1024u, 0.2f, 5, true);
    };

    for (auto&& [i, trace] : rngv::enumerate(ms_RadarTrace)) {
        if (!trace.m_bTrackingBlip) {
            continue;
        }

        const auto color = CRGBA{GetRadarTraceColour(trace.m_nColour, trace.m_bBright, trace.m_bFriendly)};
        // TODO: make a tConeHandle or something for this.
        const auto coneHandle = (uint32)i | ((uint32)trace.m_nCounter << (sizeof(uint16) * 8));

        if (trace.m_nBlipDisplayFlag != BLIP_DISPLAY_BOTH && trace.m_nBlipDisplayFlag != BLIP_DISPLAY_MARKERONLY) {
            continue;
        }

        switch (trace.m_nBlipType) {
        case BLIP_CAR: {
            const auto vehicle = GetVehiclePool()->GetAtRef(trace.m_nEntityHandle);
            assert(vehicle); // NOTSA

            const auto posn = [vehicle] {
                const auto bbMaxZ = vehicle->GetColModel()->GetBoundingBox().m_vecMax.z;
                const auto vehPos = vehicle->GetPosition();

                // exe 0x585D5A-0x585D75: z = (bbMaxZ * (Nevada ? 0.6 : 1.2) + pos.z) + 2.0
                const auto scaledMaxZ = bbMaxZ * (ModelIndices::IsNevada(vehicle->m_nModelIndex) ? 0.6f : 1.2f);
                return CVector{vehPos.x, vehPos.y, (scaledMaxZ + vehPos.z) + 2.0f};
            }();

            PutMarkerCone(coneHandle, posn, 2.0f, color);
            break;
        }
        case BLIP_CHAR: {
            const auto ped = GetPedPool()->GetAtRef(trace.m_nEntityHandle);
            assert(ped); // NOTSA

            PutMarkerCone(coneHandle, ped->GetRealPosition() + CVector{0.0f, 0.0f, 2.7f}, 1.2f, color);
            break;
        }
        case BLIP_OBJECT:
        case BLIP_PICKUP: {
            const auto posn = [&trace]() {
                CVector ret{};
                if (trace.m_nBlipType == BLIP_OBJECT) {
                    if (const auto obj = GetObjectPool()->GetAtRef(trace.m_nEntityHandle)) {
                        const auto bbMaxZ = obj->GetColModel()->GetBoundingBox().m_vecMax.z;
                        ret = obj->GetPosition() + CVector{0.0f, 0.0f, bbMaxZ};
                    } else {
                        NOTSA_UNREACHABLE("Couldn't get the object!");
                    }
                } else { // OBJECT_PICKUP
                    if (const auto idx = CPickups::GetActualPickupIndex(trace.m_nEntityHandle); idx >= 0) {
                        ret = CPickups::aPickUps[idx].GetPosn() + CVector{0.0f, 0.0f, 2.0f};
                    } else {
                        // exe 0x585EFC: a missing pickup is not skipped, the marker is drawn at (0, 0, 2.0 + 1.8|1.6)
                        ret = CVector{0.0f, 0.0f, 2.0f};
                    }
                }
                ret.z += (CGame::currArea != 0 || FindPlayerPed()->GetAreaCode() != AREA_CODE_NORMAL_WORLD) ? 1.6f : 1.8f;

                return ret;
            }();

            PutMarkerCone(coneHandle, posn, 0.8f, color);
            break;
        }
        case BLIP_CONTACT_POINT: {
            if (CTheScripts::IsPlayerOnAMission() || !FindPlayerPed())
                break;

            // exe 0x585C75-0x585CAF: with an enex link the marker shows only while the player is inside (area != 0), without one only outside
            if ((trace.m_pEntryExit != nullptr) != (FindPlayerPed()->GetAreaCode() != AREA_CODE_NORMAL_WORLD))
                break;

            C3dMarkers::PlaceMarkerSet(coneHandle, MARKER3D_CYLINDER, trace.m_vPosition, 2.0f, 255, 0, 0, 228, 2048u, 0.2f, 0);
            break;
        }
        default:
            break;
        }
    }
}

// unused
// 0x585FE0
void CRadar::SetRadarMarkerState(int32 counter, bool flag) {
    // NOP

    /* III/VC leftover
    const auto GetEntity = [=]() -> CEntity* {
        const auto handle = ms_RadarTrace[counter].m_nEntityHandle;
        switch (ms_RadarTrace[counter].m_nBlipType) {
        case BLIP_CAR:
            return GetVehiclePool()->GetAt(handle);
        case BLIP_CHAR:
            return GetPedPool()->GetAt(handle);
        case BLIP_OBJECT:
            return GetObjectPool()->GetAt(handle);
        default:
            return nullptr;
        }
    };

    if (auto* entity = GetEntity()) {
        entity->bHasBlip = flag;
    }
    */
}

/*
 * @brief Draw a sprite to specific location and opacity.
 * @addr 0x585FF0
 */
void CRadar::DrawRadarSprite(eRadarSprite spriteId, float x, float y, uint8 alpha) {
    Limit(x, y);

    const auto width  = std::floor(SCREEN_STRETCH_X(8.0f));
    const auto height = std::floor(SCREEN_STRETCH_Y(8.0f));

    if (DisplayThisBlip(spriteId, -99)) {
        RadarBlipSprites[(size_t)spriteId].Draw(
            { x - width, y - height, x + width, y + height },
            { 255, 255, 255, alpha }
        );
        AddBlipToLegendList(false, spriteId);
    }
}

// 0x586110
void CRadar::DrawRadarSection(int32 x, int32 y) {
    CVector2D corners[8]{};
    GetTextureCorners(x, y, corners);

    // exe 0x586138-0x5861BB: the section's texture is looked up before anything is drawn; a missing slot (-1), txd or texture ends the function
    // (the port used to fall through and draw stale vertices, and treated index 0 as "no texture")
    const bool inBounds = IsMapSectionInBounds(x, y);
    RwTexture* texture{};
    if (inBounds && !CTheScripts::bPlayerIsOffTheMap) {
        const auto txdIndex = gRadarTextures[y][x];
        if (txdIndex == -1) {
            return;
        }
        const auto txd = CTxdStore::GetTxd(txdIndex);
        if (!txd) {
            return;
        }
        texture = GetFirstTexture(txd);
        if (!texture) {
            return;
        }
    }

    // exe 0x5861C1-0x586325: the inlined CRadar::TransformRealWorldPointToRadarSpace (k = 1/range, not a division by range)
    CVector2D rotated[4]{};
    for (auto i = 0; i < 4; i++) {
        rotated[i] = TransformRealWorldPointToRadarSpace(corners[i]);
    }
    CVector2D clipped[8]{};
    const auto numVerts = ClipRadarPoly(clipped, rotated);
    if (numVerts == 0) { // exe 0x586331: leaves without touching the render states
        return;
    }

    CVector2D texCoords[8]{};
    CVector2D verts[8]{};
    for (auto i = 0; i < numVerts; i++) {
        texCoords[i] = TransformRealWorldToTexCoordSpace(vec2DRadarOrigin + CachedRotateCounterclockwise(clipped[i]) * m_radarRange, x, y);
        verts[i]     = TransformRadarPointToScreenSpace(clipped[i]);
    }

    if (!inBounds) { // there is no land here, draw the sea.
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
        CSprite2d::SetVertices(numVerts, verts, texCoords, { 111, 137, 170, 255 });
    } else if (CTheScripts::bPlayerIsOffTheMap) { // Draw blank
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
        CSprite2d::SetVertices(numVerts, verts, texCoords, { 204, 204, 204, 255 });
    } else { // Or if it has a txture, draw that
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RwTextureGetRaster(texture));
        CSprite2d::SetVertices(numVerts, verts, texCoords, { 255, 255, 255, 255 });
    }

    // Now draw what we have
    if (numVerts > 2) {
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, CSprite2d::maVertices.data(), numVerts);
    }
}

// 0x586520
void CRadar::DrawRadarSectionMap(int32 x, int32 y, CRect rect) {
    if (!IsMapSectionInBounds(x, y))
        return;

    const auto txdIndex = gRadarTextures[y][x];
    if (txdIndex == -1)
        return;

    if (const auto txd = CTxdStore::GetTxd(txdIndex)) {
        if (const auto texture = GetFirstTexture(txd)) {
            const auto bg = CRGBA{255, 255, 255, 255};
            RwRenderStateSet(rwRENDERSTATETEXTURERASTER, texture->raster);

            CSprite2d::SetVertices(rect, bg, bg, bg, bg);
            RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, CSprite2d::maVertices.data(), 4);
        }
    }
}

// 0x586650
void CRadar::DrawRadarGangOverlay(bool inMenu) {
    NOTSA_GLOBAL_LOCAL(g_RadarGangResetOverlay, 0xBAA36C, (uint32), {}); // bool?
    NOTSA_GLOBAL_LOCAL(g_RadarGangOverlay, 0xBAA35C, (CRect), = ExeZero<CRect>()); // zero in the exe image (CRect() would seed the 1e6 bounds)

    if ((g_RadarGangResetOverlay & 1) == 0) {
        g_RadarGangResetOverlay |= 1u;
        g_RadarGangOverlay = CRect{};
    }

    if (!CGangWars::bGangWarsActive || !FrontEndMenuManager.m_abPrefsMapBlips[4])
        return;

    for (auto& zone : CTheZones::GetNavigationZones()) {
        const auto info = CTheZones::GetZoneInfo(&zone);

        if (!info || !info->RadarMode || !CGangWars::CanPlayerStartAGangWarHere(info))
            continue;

        g_RadarGangOverlay = zone.GetRect();

        // todo: enum
        switch (info->RadarMode) {
        case 1:
            DrawAreaOnRadar(g_RadarGangOverlay, info->ZoneColor, inMenu);
            break;
        case 2: {
            const auto color = [&]() -> CRGBA {
                const auto timeInMS = FrontEndMenuManager.m_bDrawingMap ? CTimer::GetTimeInMSPauseMode() : CTimer::GetTimeInMS();

                auto zoneColor = info->ZoneColor;
                // exe 0x586791: angle = (t & 0x3FF) * 0.00613592332 (= 2pi/1024); the port multiplied by 1024/2pi
                zoneColor.a = NOTSA_FTOL(uint8, (X87R(x87::sin((float)(timeInMS % 1024) * (TWO_PI / 1024.f)) + 1.0f)) / 2.0f * (float)zoneColor.a);

                return zoneColor;
            }();
            DrawAreaOnRadar(g_RadarGangOverlay, color, inMenu);
            break;
        }
        default:
            break;
        }
    }
}

// 0x586880
void CRadar::DrawRadarMap() {
    DrawRadarMask();

    auto x = NOTSA_FTOL(int32, std::floor((vec2DRadarOrigin.x + 3000.0f) * ExeRecip(500.0f)));  // todo: inlined
    auto y = NOTSA_FTOL(int32, std::ceil(11.0f - (vec2DRadarOrigin.y + 3000.0f) * ExeRecip(500.0f)));

    SetupRadarRect(x, y);
    StreamRadarSections(x, y);

    RwRenderStateSet(rwRENDERSTATEFOGENABLE,          RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,           RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,          RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,      RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,          RWRSTATE(D3DSHADE_FLAT));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,        RWRSTATE(rwRENDERSTATETEXTURERASTER));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,       RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,  RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,     RWRSTATE(D3DTADDRESS_CLAMP));
    RwRenderStateSet(rwRENDERSTATETEXTUREPERSPECTIVE, RWRSTATE(FALSE));

    DrawRadarSection(x - 1,     y - 1);
    DrawRadarSection(x,         y - 1);
    DrawRadarSection(x + 1,     y - 1);
    DrawRadarSection(x - 1,     y    );
    DrawRadarSection(x,         y    );
    DrawRadarSection(x + 1,     y    );
    DrawRadarSection(x - 1,     y + 1);
    DrawRadarSection(x,         y + 1);
    DrawRadarSection(x + 1,     y + 1);

    DrawRadarGangOverlay(false);

    const auto vehicle = FindPlayerVehicle();

    // Draw green rectangle when in plane
    if (vehicle && vehicle->IsSubPlane() && !ModelIndices::IsVortex(vehicle->m_nModelIndex)) {
        CVector playerPos = FindPlayerCentreOfWorldForMap(0);

        const auto cSin = cachedSin;
        const auto cCos = cachedCos;
        cachedSin = x87::sin(PI);
        cachedCos = x87::cos(PI);

        // Pitch and roll combined
        const float angle = x87::atan2(-vehicle->m_matrix->GetForward().z, vehicle->m_matrix->GetUp().z);

        DrawAreaOnRadar(
            {
                playerPos.x - 1000.0f,
                playerPos.y - (2.0f * RadiansToDegrees(angle)),
                playerPos.x + 1000.0f,
                playerPos.y + 2000.0f
            },
            { 20, 175, 20, 200 },
            false
        );

        cachedSin = cSin;
        cachedCos = cCos;
    }

    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(FALSE));
}

// 0x586B00
void CRadar::DrawMap() {
    const auto player = FindPlayerPed();
    const auto mapShouldDrawn = !CGame::currArea && player->GetAreaCode() == AREA_CODE_NORMAL_WORLD && FrontEndMenuManager.m_nRadarMode != 1;

    CalculateCachedSinCos();

    const auto vehicle = FindPlayerVehicle();
    const auto playerInfo = player->GetPlayerInfoForThisPlayerPed();

    if (!vehicle || playerInfo->IsPlayerInRemoteMode()) {
        if (CTheScripts::RadarZoomValue)
            m_radarRange = RADAR_MIN_RANGE - (float)CTheScripts::RadarZoomValue;
        else
            m_radarRange = RADAR_MIN_RANGE;
    } else {
        if (vehicle && vehicle->IsSubPlane() && !ModelIndices::IsVortex(vehicle->m_nModelIndex)) {
            const auto speedZ = vehicle->GetPosition().z * ExeRecip(200.0f);

            if (speedZ < RADAR_MIN_SPEED)
                m_radarRange = RADAR_MAX_RANGE - 10.0f;
            else if (speedZ < RADAR_MAX_SPEED)
                m_radarRange = (speedZ - RADAR_MIN_SPEED) * (1.0f / 60.0f) + (RADAR_MAX_RANGE - 10.0f);
            else
                m_radarRange = RADAR_MAX_RANGE;
        } else {
            const auto speed = FindPlayerSpeed().Magnitude();

            if (speed < RADAR_MIN_SPEED)
                m_radarRange = RADAR_MIN_RANGE;
            else if (speed >= RADAR_MAX_SPEED)
                m_radarRange = RADAR_MAX_RANGE;
            else
                m_radarRange = (speed - RADAR_MIN_SPEED) * (850.0f / (RADAR_MIN_SPEED * 10.0f)) + RADAR_MIN_RANGE;
        }
    }

    vec2DRadarOrigin = []() -> CVector2D {
        if (!CGameLogic::IsCoopGameGoingOn()) [[likely]] {
            return FindPlayerCentreOfWorldForMap(0);
        } else if (CGameLogic::n2PlayerPedInFocus == eFocusedPlayer::PLAYER2) {
            return FindPlayerCentreOfWorldForMap(1);
        } else {
            return (FindPlayerCentreOfWorldForMap(0) + FindPlayerCentreOfWorldForMap(1)) / 2.0f; // Halfway between the two player's positions
        }
    }();

    if (mapShouldDrawn) {
        DrawRadarMap();
    }
}

/*
 * @brief Draw a blip by its index in `ms_RadarTrace`
 * @addr 0x586D60
 */
void CRadar::DrawCoordBlip(int32 blipIndex, bool isSprite) {
    auto& trace = ms_RadarTrace[blipIndex];
    if (trace.m_nBlipType == BLIP_CONTACT_POINT && CTheScripts::IsPlayerOnAMission())
        return;

    const auto color = trace.GetStaticColour(); // exe 0x586D90-0x586DC4

    // Debug marker (exe 0x586DC8-0x586E23): runs before the sprite/display checks, so it runs on both the sprite and the non-sprite pass
    if ((trace.m_nBlipDisplayFlag == BLIP_DISPLAY_BOTH || trace.m_nBlipDisplayFlag == BLIP_DISPLAY_MARKERONLY) && CTheScripts::DbgFlag) {
        ShowRadarMarker(trace.m_vPosition, color.ToInt(), trace.m_fSphereRadius);
        trace.m_fSphereRadius = (float)((x87d)trace.m_fSphereRadius - (x87d)0.1f); // 0x858B1C
        if (trace.m_fSphereRadius < 1.f) {                                              // 0x858624
            trace.m_fSphereRadius = 5.f;
        }
    }

    if (isSprite == !trace.HasSprite())
        return;

    if (trace.m_nBlipDisplayFlag != BLIP_DISPLAY_BOTH && trace.m_nBlipDisplayFlag != BLIP_DISPLAY_BLIPONLY)
        return;

    float realDist{};
    const auto [radarPos, screenPos] = trace.GetRadarAndScreenPos(&realDist);
    const auto zoomedDist = CTheScripts::RadarZoomValue != 0u ? 255.0f : realDist;

    if (isSprite) {
        // either the blip is close to the player or we're drawing the whole map.
        const auto canBeDrawn = !trace.m_bShortRange || zoomedDist <= 1.0f || FrontEndMenuManager.m_bDrawingMap;

        if (trace.HasSprite() && canBeDrawn && HasThisBlipBeenRevealed(blipIndex)) {
            DrawRadarSprite(trace.m_nBlipSprite, screenPos.x, screenPos.y, 255);
        }
        return;
    }

    if (trace.HasSprite())
        return;

    // The exe compares `z - 2.0` / `z + 4.0` (float steps) against the player's z, not the difference against 2 / -4.
    // NaN takes the same branches as the exe: not LOW, and NORMAL (the HIGH test is `z + 4 < playerZ`).
    const float blipZ   = trace.GetWorldPos().z;
    const float playerZ = FindPlayerCentreOfWorldForMap(PED_TYPE_PLAYER1).z;
    const float zLow    = blipZ - 2.0f;  // 0x858CA0
    const float zHigh   = blipZ + 4.0f;  // 0x858B90
    const auto height = [&] {
        if (zLow > playerZ) {
            // trace is higher
            return RADAR_TRACE_LOW;
        } else if (!(zHigh < playerZ)) {
            // they are at the around the same elevation.
            return RADAR_TRACE_NORMAL;
        } else {
            // player is higher
            return RADAR_TRACE_HIGH;
        }
    }();

    if (FrontEndMenuManager.m_bDrawingMap && !FrontEndMenuManager.m_ShowMissionBlips) {
        // drawing the whole map but mission blips are hidden by the player.
        return;
    }

    // Blip fade (flag bit 0x20) selects the distance alpha; the exe otherwise uses the alpha of the colour (bl of the packed RGBA).
    const uint8 alpha = trace.m_bBlipFade ? CalculateBlipAlpha(realDist) : color.a;
    CRadar::ShowRadarTraceWithHeight(
        screenPos.x,
        screenPos.y,
        (uint32)(int32)(int16)trace.m_nBlipSize, // movsx word [esi+0x1c]
        color.r,
        color.g,
        color.b,
        alpha,
        height
    );

    AddBlipToLegendList(true, blipIndex);
}

/*
 * @brief Draw a blip that is attached to an entity/script thing/pickup/airstrip by its index in `ms_RadarTrace`
 * @param isSprite (arg1) If true only the sprite blips are drawn, otherwise only the non-sprite ones
 * @addr 0x587000
 *
 * x87 notes: float expressions are evaluated in extended precision (double here) and only rounded on float stores.
 * `_ftol` is 0x821B40.
 */
void CRadar::DrawEntityBlip(int32 blipIndex, uint8 arg1) {
    using notsa::detail::Ftol;
    const bool isSprite = arg1 != 0;

    // 0x859CF8 / 0xA444A2 / 0xA444A3 / 0xBA67A1 / 0xBA678F are named globals, see CTheScripts / FrontEndMenuManager
    VALIDATE_SIZE(CSprite2d, 4);
    constexpr float kInv640   = 1.f / 640.f;                       // 0x859520
    constexpr float kInv448   = 1.f / 448.f;                       // 0x859524
    constexpr float kDegToRad = std::bit_cast<float>(0x3C8EFA35u); // 0x8595EC
    static_assert(offsetof(CMenuManager, m_bDrawingMap) == 89); // 0xBA67A1 - FrontEndMenuManager (0xBA6748)
    static_assert(offsetof(CMenuManager, m_ShowMissionBlips) == 71); // 0xBA678F - FrontEndMenuManager (0xBA6748)

    // Function-local statics of the original (the guard bits live in `s_initFlags`)
    NOTSA_GLOBAL_LOCAL(s_initFlags, 0xBAA380, (uint32), {});
    NOTSA_GLOBAL_LOCAL(s_currRange, 0xBAA37C, (int16), {});
    NOTSA_GLOBAL_LOCAL(s_prevRange, 0xBAA378, (int16), {});
    NOTSA_GLOBAL_LOCAL(s_lastTime, 0xBAA374, (uint32), {});
    NOTSA_GLOBAL_LOCAL(s_airstripSeen, 0xBAA370, (bool), {});

    auto& trace = ms_RadarTrace[blipIndex];

    CEntity*            entity{};
    tScriptSearchlight* searchlight{};
    bool                isAirstrip{};
    float               airstripDist{};
    CVector             pos{};

    switch (trace.m_nBlipType) {
    case BLIP_CAR:
    case BLIP_CHAR:
    case BLIP_OBJECT: {
        if (trace.m_nBlipType == BLIP_CAR) {
            entity = CPools::GetVehicle(trace.m_nEntityHandle);
        } else if (trace.m_nBlipType == BLIP_CHAR) {
            auto* const ped = CPools::GetPed(trace.m_nEntityHandle);
            if (!ped) {
                return;
            }
            entity = ped->bInVehicle ? (CEntity*)ped->m_pVehicle : ped;
        } else {
            entity = CPools::GetObject(trace.m_nEntityHandle);
        }
        if (!entity) {
            return;
        }
        pos = entity->GetPosition();
        if (entity->GetIsTypePed()) {
            if (auto* const enex = entity->AsPed()->m_pEnex) {
                enex->GetPositionRelativeToOutsideWorld(pos);
            }
        }
        break;
    }
    case BLIP_SPOTLIGHT: {
        const auto idx = CTheScripts::GetActualScriptThingIndex(trace.m_nEntityHandle, SCRIPT_THING_SEARCH_LIGHT);
        if (idx < 0) {
            return;
        }
        searchlight = &CTheScripts::ScriptSearchLightArray[idx];
        pos         = searchlight->m_Target;
        break;
    }
    case BLIP_PICKUP: {
        const auto idx = CPickups::GetActualPickupIndex(trace.m_nEntityHandle);
        if (idx < 0) {
            return;
        }
        pos = CPickups::aPickUps[idx].GetPosn();
        break;
    }
    case BLIP_AIRSTRIP: {
        isAirstrip = true;

        // BUG: The original doesn't check if the player is in a vehicle
        const auto& vehPos = FindPlayerVehicle()->GetPosition();
        const auto& strip  = airstrip_table[airstrip_location];

        const x87d dy = (x87d)strip.position.y - (x87d)vehPos.y;
        const x87d dx = (x87d)strip.position.x - (x87d)vehPos.x;
        airstripDist    = (float)std::sqrt(dy * dy + dx * dx);

        // BUG: In the original `pos` is left uninitialised (a stale stack slot, whatever the caller's previous calls left there,
        // not reproducible) if the player is >= 500 units away. In practice this is only observable via the short range check and
        // the debug marker, as the airstrip drawing below recomputes everything from the trace's position.
        // The non-fixed path keeps `pos` zeroed (deterministic stand-in for the garbage).
        if (notsa::IsFixBugs()) {
            pos = trace.m_vPosition;
        }

        if (airstripDist < 500.f) {
            int16 range{};
            if (!(s_initFlags & 1)) {
                s_initFlags |= 1;
                range = (int16)Ftol(-(double)strip.radius);
                s_currRange = range;
            } else {
                range = s_currRange;
            }
            if (!(s_initFlags & 2)) {
                s_initFlags |= 2;
                s_prevRange = (int16)Ftol(-(double)strip.radius);
            }

            pos = trace.m_vPosition;
            (void)TransformRealWorldPointToRadarSpace(pos); // NOTE: Result is overwritten below

            const x87d theta = (x87d)strip.direction * (x87d)kDegToRad; // 0x8595EC
            pos.x = (float)(X87R(x87::cos(theta) * (double)range) + (x87d)pos.x);
            pos.y = (float)((x87d)pos.y - X87R(x87::sin(theta) * (double)range));

            auto radarPos = TransformRealWorldPointToRadarSpace(pos);

            const auto frame = CTimer::m_snTimeInMillisecondsPauseMode;
            const x87d dist = std::sqrt((x87d)radarPos.y * (x87d)radarPos.y + (x87d)radarPos.x * (x87d)radarPos.x);
            if (!(dist < (x87d)0.9f) || (uint32)(frame - s_lastTime) > 3) { // 0x858C20; exe jumps to the time test only if C0 (dist < 0.9) is set, so NaN takes the update branch
                range       = (int16)(range + 100);
                s_currRange = range;
                if ((double)range > (double)strip.radius) {
                    range       = (int16)Ftol(-(double)strip.radius);
                    s_currRange = range;
                }
                if (s_prevRange == range) {
                    s_airstripSeen = false;
                }
                if (dist < (double)0.9f) {
                    s_prevRange    = range;
                    s_airstripSeen = true;
                }
                s_lastTime = frame;
            }
            (void)LimitRadarPoint(radarPos);
        } else {
            s_airstripSeen = false;
        }
        break;
    }
    default: // BLIP_NONE, BLIP_COORD, BLIP_CONTACT_POINT
        return;
    }

    const auto color = GetRadarTraceColour(trace.m_nColour, trace.m_bBright, trace.m_bFriendly);

    if ((trace.m_nBlipDisplayFlag == BLIP_DISPLAY_BOTH || trace.m_nBlipDisplayFlag == BLIP_DISPLAY_MARKERONLY) && CTheScripts::DbgFlag) {
        ShowRadarMarker(pos, color, trace.m_fSphereRadius);
        trace.m_fSphereRadius = (float)((x87d)trace.m_fSphereRadius - (x87d)0.1f); // 0x858B1C
        if (trace.m_fSphereRadius < 1.f) {                                              // 0x858624
            trace.m_fSphereRadius = 5.f;
        }
    }

    if (trace.m_pEntryExit) {
        trace.m_pEntryExit->GetPositionRelativeToOutsideWorld(pos);
    }

    if (trace.m_nBlipDisplayFlag != BLIP_DISPLAY_BOTH && trace.m_nBlipDisplayFlag != BLIP_DISPLAY_BLIPONLY) {
        return;
    }

    auto  radarPos  = TransformRealWorldPointToRadarSpace(pos);
    float radarDist = LimitRadarPoint(radarPos);
    auto  screenPos = TransformRadarPointToScreenSpace(radarPos);

    const bool isDrawingMap = FrontEndMenuManager.m_bDrawingMap;

    if (trace.m_bShortRange && radarDist > 1.f && !isDrawingMap) {
        return;
    }

    const auto RadarVecLength = [&] {
        return std::sqrt((x87d)radarPos.y * (x87d)radarPos.y + (x87d)radarPos.x * (x87d)radarPos.x);
    };
    // x87: `maximum{Width|Height} * 1/{640|448} * scale` is kept in extended precision until `_ftol`
    const auto ScaledWidth  = [&](float scale) { return (uint32)Ftol((x87d)RsGlobal.maximumWidth  * (x87d)kInv640  * (x87d)scale); }; // 0x859520
    const auto ScaledHeight = [&](float scale) { return (uint32)Ftol((x87d)RsGlobal.maximumHeight * (x87d)kInv448 * (x87d)scale); }; // 0x859524
    const auto Size         = (uint32)(int32)(int16)trace.m_nBlipSize;

    if (CTheScripts::RadarZoomValue && !FindPlayerVehicle() && trace.m_nBlipType != BLIP_OBJECT && trace.m_nBlipType != BLIP_PICKUP) {
        if (isSprite) {
            return;
        }
        if (!trace.m_bBlipRemain && !(RadarVecLength() <= 1.0)) {
            return;
        }

        float heading = 0.f;
        if (entity) {
            heading = entity->GetHeading();
        }

        if (isDrawingMap && !FrontEndMenuManager.m_ShowMissionBlips) {
            return;
        }

        const auto playerZ = FindPlayerCentreOfWorldForMap(0).z;
        if ((x87d)pos.z - (x87d)2.0 > (double)playerZ) {
            if (!CTheScripts::RadarShowBlipOnAllLevels) {
                return;
            }
            ShowRadarTraceWithHeight(screenPos.x, screenPos.y, Size, color >> 24, color >> 16, color >> 8, color, RADAR_TRACE_LOW);
            return;
        }
        if ((x87d)pos.z + (x87d)4.0 < (double)playerZ) {
            if (!CTheScripts::RadarShowBlipOnAllLevels) {
                return;
            }
            ShowRadarTraceWithHeight(screenPos.x, screenPos.y, Size, color >> 24, color >> 16, color >> 8, color, RADAR_TRACE_HIGH);
            return;
        }

        const CRGBA arrowColor{ (uint8)(color >> 24), (uint8)(color >> 16), (uint8)(color >> 8), 255 };
        if (TheCamera.GetActiveCam().m_nMode == MODE_TOPDOWN) {
            DrawRotatingRadarSprite(
                RadarBlipSprites[RADAR_SPRITE_CENTRE], screenPos.x, screenPos.y,
                (float)((x87d)heading + (x87d)std::numbers::pi_v<float>),
                ScaledWidth(8.f), ScaledHeight(8.f), arrowColor
            );
        } else {
            DrawRotatingRadarSprite(
                RadarBlipSprites[RADAR_SPRITE_CENTRE], screenPos.x, screenPos.y,
                (float)((x87d)heading - ((x87d)m_fRadarOrientation + (x87d)std::numbers::pi_v<float>)),
                ScaledWidth(8.f), ScaledHeight(8.f), arrowColor
            );
        }
        return;
    }

    if (isSprite) {
        if (trace.m_nBlipSprite == RADAR_SPRITE_NONE) {
            return;
        }
        if (!HasThisBlipBeenRevealed(blipIndex)) {
            return;
        }
        DrawRadarSprite(trace.m_nBlipSprite, screenPos.x, screenPos.y, 255);
        return;
    }

    if (trace.m_nBlipSprite != RADAR_SPRITE_NONE) {
        return;
    }
    if (isDrawingMap && !FrontEndMenuManager.m_ShowMissionBlips) {
        return;
    }

    if (searchlight) {
        if (RadarVecLength() <= (double)0.9f && !isDrawingMap) {
            const float radius = (float)(
                (x87d)RsGlobal.maximumWidth * (x87d)kInv640 * (x87d)94.f / (x87d)m_radarRange
                * (x87d)searchlight->m_fTargetRadius * (x87d)0.6f
            ); // 0x859520, 0x866B78, 0x858CC8
            CSprite2d::DrawCircleAtNearClip(screenPos, radius, CRGBA{ 0, 0, 0, 150 }, 15);
            CSprite2d::DrawCircleAtNearClip(
                screenPos,
                (float)((x87d)radius - (x87d)RsGlobal.maximumWidth * (x87d)kInv640),
                CRGBA{ 220, 220, 220, 200 },
                15
            );
        }
    } else if (isAirstrip) {
        const CRGBA white{ 255, 255, 255, 255 };
        if (!isDrawingMap) {
            if (RadarVecLength() < (double)0.9f && airstripDist < 500.f) {
                DrawRotatingRadarSprite(
                    RadarBlipSprites[RADAR_SPRITE_LIGHT], screenPos.x, screenPos.y, 0.f,
                    ScaledWidth(8.f), ScaledHeight(8.f), white
                );
            }
        }

        // Recompute using the trace's own position
        pos       = trace.m_vPosition;
        radarPos  = TransformRealWorldPointToRadarSpace(pos);
        (void)LimitRadarPoint(radarPos);
        screenPos = TransformRadarPointToScreenSpace(radarPos);

        const auto& strip = airstrip_table[airstrip_location];
        if (isDrawingMap) {
            float dir = strip.direction;
            if (airstrip_location != AIRSTRIP_SF_AIRPORT) {
                dir = (float)((x87d)dir - (x87d)90.f); // 0x85991C
            }
            DrawRotatingRadarSprite(
                RadarBlipSprites[RADAR_SPRITE_RUNWAY], screenPos.x, screenPos.y,
                (float)((x87d)dir * (x87d)kDegToRad),
                ScaledWidth(16.f), ScaledHeight(16.f), white // 0x8599D0
            );
            AddBlipToLegendList(false, RADAR_SPRITE_RUNWAY);
        } else {
            if (s_airstripSeen) {
                return;
            }
            DrawRotatingRadarSprite(
                RadarBlipSprites[RADAR_SPRITE_RUNWAY], screenPos.x, screenPos.y,
                (float)((x87d)(-m_fRadarOrientation) - ((x87d)strip.direction - (x87d)90.f) * (x87d)kDegToRad),
                ScaledWidth(16.f), ScaledHeight(16.f), white
            );
        }
    } else {
        const auto playerZ = FindPlayerCentreOfWorldForMap(0).z;
        eRadarTraceHeight height;
        if ((x87d)pos.z - (x87d)2.0 > (double)playerZ) {
            height = RADAR_TRACE_LOW;
        } else {
            height = ((x87d)pos.z + (x87d)4.0 < (double)playerZ) ? RADAR_TRACE_HIGH : RADAR_TRACE_NORMAL;
        }
        // NOTE: The original passes a dword whose upper 3 bytes are stack garbage (only the low byte is set), the callee uses the low byte
        ShowRadarTraceWithHeight(screenPos.x, screenPos.y, Size, color >> 24, color >> 16, color >> 8, color, height);
    }

    if (isDrawingMap && !searchlight && !isAirstrip) {
        AddBlipToLegendList(true, blipIndex);
    }
}

/*
 * @brief Clear a blip by its index in `ms_RadarTrace`
 * @addr 0x587C10
 */
void CRadar::ClearActualBlip(int32 blipIndex) {
    if (blipIndex < 0 || blipIndex >= MAX_RADAR_TRACES)
        return;

    ClearActualBlip(ms_RadarTrace[blipIndex]);
}

/*
 * @notsa
 * @brief Clear a blip by its trace reference.
 */
void CRadar::ClearActualBlip(tRadarTrace& trace) {
    trace.m_nBlipSize = 1;
    trace.m_fSphereRadius = 1.0f;
    trace.m_pEntryExit = nullptr;
    trace.m_nBlipSprite = RADAR_SPRITE_NONE;

    trace.m_bBright       = true;
    trace.m_bTrackingBlip = false;
    trace.m_bShortRange   = false;
    trace.m_bFriendly     = false;
    trace.m_bBlipRemain   = false;
    trace.m_bBlipFade     = false;

    trace.m_nCoordBlipAppearance = BLIP_FLAG_FRIEND;
    trace.m_nBlipDisplayFlag = BLIP_DISPLAY_NEITHER;
    trace.m_nBlipType = BLIP_NONE;
}

/*
 * @brief Clear a blip by type and handle of the entity.
 * @addr 0x587C60
 */
void CRadar::ClearBlipForEntity(eBlipType blipType, int32 entityHandle) {
    for (auto& trace : ms_RadarTrace) {
        if (trace.m_nBlipType == blipType && trace.m_nEntityHandle == entityHandle) {
            ClearActualBlip(trace);
        }
    }
}

void CRadar::ClearBlipForEntity(CPed* ped) {
    ClearBlipForEntity(BLIP_CHAR, GetPedPool()->GetIndex(ped));
}

/*
 * @brief Clear a blip
 * @addr 0x587CE0
 */
void CRadar::ClearBlip(tBlipHandle blip) {
    if (const auto index = GetActualBlipArrayIndex(blip); index != -1) {
        ClearActualBlip(index);
    }
}

CVector GetAirStripLocation(eAirstripLocation location) {
    return CVector(airstrip_table[location].position);
}

// 0x587D20
void CRadar::SetupAirstripBlips() {
    if (const auto veh = FindPlayerVehicle(); veh && veh->IsSubPlane() && !ModelIndices::IsVortex(veh->m_nModelIndex)) {
#ifdef FIX_BUGS
        // FIX_BUGS: FPS independent counter. SA: (CTimer::GetFrameCounter() & 4) == 0
        static auto airstripBlipCounter = CTimer::GetTimeInMS() + 200;
        if (CTimer::GetTimeInMS() >= airstripBlipCounter) {
            airstripBlipCounter = CTimer::GetTimeInMS() + 200;
#else
        if ((CTimer::GetFrameCounter() & 4) == 0) { // 0x587D72: test [CTimer::m_FrameCounter], 4
#endif

            if (airstrip_blip)
                return;

            const auto pos = GetAirStripLocation(airstrip_location);
            airstrip_blip = SetCoordBlip(BLIP_AIRSTRIP, pos, BLIP_COLOUR_RED, BLIP_DISPLAY_BLIPONLY);
            return;
        }

        // The exe (0x587E3B-0x587ECE) picks the nearest airstrip with strict `<` tests against all the others (not std::min_element):
        // on a tie (or NaN) the first strip that is not strictly the nearest is skipped, and the last one is the fallback.
        const auto location = [veh] {
            float d[NUM_AIRSTRIPS]{};
            for (auto&& [i, table] : rngv::enumerate(airstrip_table)) {
                d[i] = DistanceBetweenPoints2D(table.position, veh->GetPosition());
            }
            if (d[0] < d[1] && d[0] < d[2] && d[0] < d[3]) {
                return (eAirstripLocation)0;
            }
            if (d[1] < d[0] && d[1] < d[2] && d[1] < d[3]) {
                return (eAirstripLocation)1;
            }
            if (d[2] < d[0] && d[2] < d[1] && d[2] < d[3]) {
                return (eAirstripLocation)2;
            }
            return (eAirstripLocation)3;
        }();

        if (airstrip_blip) {
            if (airstrip_location == location)
                return;

            ClearBlip(airstrip_blip);
            airstrip_location = location;
        }

        const auto pos = GetAirStripLocation(airstrip_location);
        airstrip_blip = SetCoordBlip(BLIP_AIRSTRIP, pos, BLIP_COLOUR_RED, BLIP_DISPLAY_BLIPONLY);
        return;
    }

    if (airstrip_blip) {
        // player is not flying anymore.
        ClearBlip(airstrip_blip);
        airstrip_blip = 0;
    }
}

// 0x588050
void CRadar::DrawBlips() {
    SetupAirstripBlips();
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(false));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(false));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(true));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(false));

    const auto TransformRealWorldPointToBlipSpace = [](const CVector2D& pos) {
        auto radar = TransformRealWorldPointToRadarSpace(pos);
        LimitRadarPoint(radar);
        return TransformRadarPointToScreenSpace(radar);
    };

    if (FrontEndMenuManager.m_bDrawingMap) {
        if (FrontEndMenuManager.m_bMapLoaded) {
            // draw the menu map ruler.
            // The exe truncates the 4 bounds with _ftol (prologue 0x588059-0x5880AF) and compares the int mouse position against the ints.
            const auto mouseX = FrontEndMenuManager.m_nMousePosX, mouseY = FrontEndMenuManager.m_nMousePosY;
            const auto minX = NOTSA_FTOL(int32, SCREEN_STRETCH_X(60.0f)),           maxX = NOTSA_FTOL(int32, SCREEN_STRETCH_FROM_RIGHT(60.0f));
            const auto minY = NOTSA_FTOL(int32, SCREEN_STRETCH_Y(60.0f)),           maxY = NOTSA_FTOL(int32, SCREEN_STRETCH_FROM_BOTTOM(60.0f));

            if (!CPad::IsMouseLButton() &&
                mouseX > minX && mouseX < maxX &&
                mouseY > minY && mouseY < maxY ||
                !FrontEndMenuManager.m_DisplayTheMouse)
            {
                auto drawPos = TransformRealWorldPointToBlipSpace(FrontEndMenuManager.m_vMousePos);
                LimitToMap(drawPos.x, drawPos.y);

                // The exe's rects are {left, bottom, right, top} (screen y grows downwards): vertical {x - 1, H, x + 1, 0}, horizontal {0, y + 1, W, y - 1}
                CSprite2d::DrawRect(
                    {
                        SCREEN_STRETCH_X(drawPos.x) - 1.0f,
                        SCREEN_HEIGHT,
                        SCREEN_STRETCH_X(drawPos.x) + 1.0f,
                        0.0f
                    },
                    HudColour.GetRGB(HUD_COLOUR_GOLD)
                );

                CSprite2d::DrawRect(
                    {
                        0.0f,
                        SCREEN_STRETCH_Y(drawPos.y) + 1.0f,
                        SCREEN_WIDTH,
                        SCREEN_STRETCH_Y(drawPos.y) - 1.0f,
                    },
                    HudColour.GetRGB(HUD_COLOUR_GOLD)
                );
            }
        }
    } else {
        // draw the (N) blip.
        const auto drawPos = TransformRealWorldPointToBlipSpace(CVector2D{0.0f, m_radarRange * SQRT_2} + vec2DRadarOrigin);
        DrawRadarSprite(RADAR_SPRITE_NORTH, drawPos.x, drawPos.y, 255);
    }

    // we first do whole thing with isSprite = true, then = false (the exe starts the pass flag at 1: 0x5882E3, toggled at 0x588425).
    for (const auto isSprite : {true, false}) {
        for (auto priority = 1; priority < 4; priority++) {
            for (auto&& [i, trace] : rngv::enumerate(ms_RadarTrace)) { // todo: check if looping all
                if (!trace.m_bTrackingBlip)
                    continue;

                switch (trace.m_nBlipType) {
                case BLIP_CAR:
                case BLIP_CHAR:
                case BLIP_OBJECT:
                case BLIP_PICKUP:
                    if (DisplayThisBlip(trace.m_nBlipSprite, priority)) {
                        DrawEntityBlip(i, isSprite);
                    }
                    break;
                case BLIP_COORD:
                case BLIP_CONTACT_POINT:
                    if (trace.m_nBlipSprite != RADAR_SPRITE_WAYPOINT && DisplayThisBlip(trace.m_nBlipSprite, priority)) {
                        DrawCoordBlip(i, isSprite);
                    }
                    break;
                case BLIP_SPOTLIGHT:
                case BLIP_AIRSTRIP:
                    if (priority == 3 && (!CTheScripts::bPlayerIsOffTheMap || !FrontEndMenuManager.m_bDrawingMap)) {
                        DrawEntityBlip(i, isSprite);
                    }
                    break;
                default:
                    break;
                }
            }
        }

        for (auto&& [i, trace] : rngv::enumerate(ms_RadarTrace)) { // todo: check if looping all, same thing with above.
            if (!trace.m_bTrackingBlip)
                continue;

            switch (trace.m_nBlipType) {
            case BLIP_COORD:
            case BLIP_CONTACT_POINT:
                if (trace.m_nBlipSprite == RADAR_SPRITE_WAYPOINT) {
                    DrawCoordBlip(i, isSprite); // the exe passes the pass flag (0x58840F), so the waypoint sprite is drawn in the isSprite pass only
                }
                break;
            default:
                break;
            }
        }
    }

    // FIX_BUGS: Originally 2 player blips both drawing P1's position.
    // https://github.com/CookiePLMonster/SilentPatch/issues/209
    const auto GetPlayerMarkerPosition = [](int32 playerIndex = 0) {
        // The exe multiplies by the float reciprocal (fld1; fdiv range) instead of dividing each component (0x58845C-0x58847F).
        const auto  centre = FindPlayerCentreOfWorldForMap(playerIndex);
        const float inv    = 1.0f / m_radarRange;
        const CVector2D playerDirection{ (centre.x - vec2DRadarOrigin.x) * inv, (centre.y - vec2DRadarOrigin.y) * inv };

        CVector2D rotatedPos = {
            cachedSin * playerDirection.y + cachedCos * playerDirection.x,
            cachedCos * playerDirection.y - cachedSin * playerDirection.x
        };
        LimitRadarPoint(rotatedPos);
        return TransformRadarPointToScreenSpace(rotatedPos);
    };

    if (!FrontEndMenuManager.m_bDrawingMap) {
        for (auto i = 0; i < 2; i++) {
            const auto player = FindPlayerPed(i);
            if (!player)
                continue;

            // we don't draw the player marker while flying.
            if (auto veh = FindPlayerVehicle(i); veh && veh->IsSubPlane() && !ModelIndices::IsVortex(veh->m_nModelIndex))
                continue;

            const auto pos = GetPlayerMarkerPosition(notsa::IsFixBugs() ? i : 0);

            const auto angle = [i] {
                const auto heading = FindPlayerHeading(i); // the exe passes the player index (push esi at 0x58863D)

                // 0x858CB8 = float pi; the non-topdown form is heading - (orientation + pi)
                if (CCamera::GetActiveCamera().m_nMode == MODE_TOPDOWN) {
                    return heading + std::numbers::pi_v<float>;
                } else {
                    return heading - (m_fRadarOrientation + std::numbers::pi_v<float>);
                }
            }();

            DrawRotatingRadarSprite(
                RadarBlipSprites[RADAR_SPRITE_CENTRE],
                pos.x,
                pos.y,
                angle,
                NOTSA_FTOL(uint32, SCREEN_STRETCH_X(8.0f)),
                (notsa::IsFixBugs() ? NOTSA_FTOL(uint32, SCREEN_STRETCH_Y(8.0f)) : NOTSA_FTOL(uint32, SCREEN_STRETCH_X(8.0f))),
                player->IsHidden() ? CRGBA{50, 50, 80, 255} : CRGBA{255, 255, 255, 255} // 0x58865A: 0x32, 0x32, 0x50, 0xFF
            );
        }
    } else {
        const auto pos = GetPlayerMarkerPosition(0);
        DrawYouAreHereSprite(pos.x, pos.y);
    }
}

/*
 * @brief Load radar blips from save file
 * @addr 0x5D53C0
 */
bool CRadar::Load() {
    Initialise();

#ifdef NOTSA_PORTABLE
    // P-2f: the file holds tRadarTrace_File (0x28, enex pool index+1 in a plain u32); the in-memory trace (pointer in the union, 0x30 on LP64) is never read from the stream.
    for (auto& trace : ms_RadarTrace) {
        tRadarTrace_File rec{};
        CGenericGameStorage::LoadDataFromWorkBuffer(rec);
        notsa::FileRecord<tRadarTrace>::FromFile(trace, rec);
        if (rec.entryExitPoolInd) {
            trace.m_pEntryExit = CEntryExitManager::GetInSlot(rec.entryExitPoolInd - 1);
        }
    }
#else
    for (auto& trace : ms_RadarTrace) {
        CGenericGameStorage::LoadDataFromWorkBuffer(trace);
        if (trace.m_EntryExitPoolInd) {
            trace.m_pEntryExit = CEntryExitManager::GetInSlot(trace.m_EntryExitPoolInd - 1);
        }
    }
#endif

    return true;
}

/*
 * @brief Save radar blips to save file
 * @addr 0x5D5860
 */
bool CRadar::Save() {
#ifdef NOTSA_PORTABLE
    // P-2f: no in-place union patching: serialise a copy through tRadarTrace_File. Differences from the exe: a trace whose enex is not a live pool slot writes index 0
    // (the exe wrote the raw pointer's low 32 bits there); m_bTrackingBlip is cleared in the record only (the live trace is never touched).
    for (auto& trace : ms_RadarTrace) {
        uint32 enexInd = 0;
        if (trace.m_pEntryExit) {
            const auto index = CEntryExitManager::GetPool()->GetIndex(trace.m_pEntryExit);
            if (CEntryExitManager::GetPool()->IsIndexInBounds(index) && !CEntryExitManager::GetPool()->IsFreeSlotAtIndex(index)) {
                enexInd = (uint32)(index + 1);
            }
        }

        auto copy = trace;
        if (copy.m_nBlipType != BLIP_COORD && copy.m_nBlipType != BLIP_CONTACT_POINT) {
            copy.m_bTrackingBlip = false;
        }

        const auto rec = notsa::FileRecord<tRadarTrace>::ToFile(copy, enexInd);
        CGenericGameStorage::SaveDataToWorkBuffer(rec);
    }

    return true;
#else
    for (auto& trace : ms_RadarTrace) {
        CEntryExit* savedEnex = nullptr;
        if (trace.m_pEntryExit) {
            const auto index = CEntryExitManager::GetPool()->GetIndex(trace.m_pEntryExit);
            if (CEntryExitManager::GetPool()->IsIndexInBounds(index) && !CEntryExitManager::GetPool()->IsFreeSlotAtIndex(index)) {
                savedEnex = trace.m_pEntryExit;
                trace.m_EntryExitPoolInd = index + 1; // Assign the pool index for save duration, restore it later
            }
        }

        const auto& blipType = trace.m_nBlipType;
        auto bIsTracking = false;
        if (blipType != BLIP_COORD && blipType != BLIP_CONTACT_POINT) {
            if (trace.m_bTrackingBlip) {
                trace.m_bTrackingBlip = false;
                bIsTracking = true;
            }
        }

        CGenericGameStorage::SaveDataToWorkBuffer(&trace, sizeof(trace));
        if (savedEnex) {
            trace.m_pEntryExit = savedEnex;
        }
        if (bIsTracking) {
            trace.m_bTrackingBlip = true;
        }
    }

    return true;
#endif
}

/*!
 * @notsa
 * @brief Get name of a blip type. (See CRadar::DrawLegend)
 * @returns The name of the blip type
 */
const GxtChar* CRadar::GetBlipName(eRadarSprite blipType) {
    switch (blipType) {
    case RADAR_SPRITE_PLAYER_INTEREST:  return TheText.Get("LG_56");
    case RADAR_SPRITE_THREAT:           return TheText.Get("LG_55");
    case RADAR_SPRITE_FRIEND:           return TheText.Get("LG_54");
    case RADAR_SPRITE_OBJECT:           return TheText.Get("LG_50");
    case RADAR_SPRITE_DESTINATION:      return TheText.Get("LG_49");
    case RADAR_SPRITE_NONE:             break;
    case RADAR_SPRITE_WHITE:            break;
    case RADAR_SPRITE_CENTRE:           break;
    case RADAR_SPRITE_MAP_HERE:         return TheText.Get("LG_01");
    case RADAR_SPRITE_NORTH:            break; // NOTSA
    case RADAR_SPRITE_AIRYARD:          return TheText.Get("LG_02");
    case RADAR_SPRITE_AMMUGUN:          return TheText.Get("LG_03");
    case RADAR_SPRITE_BARBERS:          return TheText.Get("LG_04");
    case RADAR_SPRITE_BIGSMOKE:         return TheText.Get("LG_05");
    case RADAR_SPRITE_BOATYARD:         return TheText.Get("LG_06");
    case RADAR_SPRITE_BURGERSHOT:       return TheText.Get("LG_07");
    case RADAR_SPRITE_BULLDOZER:        return TheText.Get("LG_66");
    case RADAR_SPRITE_CATALINAPINK:     return TheText.Get("LG_09");
    case RADAR_SPRITE_CESARVIAPANDO:    return TheText.Get("LG_10");
    case RADAR_SPRITE_CHICKEN:          return TheText.Get("LG_11");
    case RADAR_SPRITE_CJ:               return TheText.Get("LG_12");
    case RADAR_SPRITE_CRASH1:           return TheText.Get("LG_13");
    case RADAR_SPRITE_DINER:            return TheText.Get("LG_67");
    case RADAR_SPRITE_EMMETGUN:         return TheText.Get("LG_15");
    case RADAR_SPRITE_ENEMYATTACK:      return TheText.Get("LG_16");
    case RADAR_SPRITE_FIRE:             return TheText.Get("LG_17");
    case RADAR_SPRITE_GIRLFRIEND:       return TheText.Get("LG_18");
    case RADAR_SPRITE_HOSTPITAL:        return TheText.Get("LG_19");
    case RADAR_SPRITE_LOGOSYNDICATE:    return TheText.Get("LG_20");
    case RADAR_SPRITE_MADDOG:           return TheText.Get("LG_21");
    case RADAR_SPRITE_MAFIACASINO:      return TheText.Get("LG_22");
    case RADAR_SPRITE_MCSTRAP:          return TheText.Get("LG_23");
    case RADAR_SPRITE_MODGARAGE:        return TheText.Get("LG_24");
    case RADAR_SPRITE_OGLOC:            return TheText.Get("LG_25");
    case RADAR_SPRITE_PIZZA:            return TheText.Get("LG_26");
    case RADAR_SPRITE_POLICE:           return TheText.Get("LG_27");
    case RADAR_SPRITE_PROPERTYG:        return TheText.Get("LG_28");
    case RADAR_SPRITE_PROPERTYR:        return TheText.Get("LG_29");
    case RADAR_SPRITE_RACE:             return TheText.Get("LG_30");
    case RADAR_SPRITE_RYDER:            return TheText.Get("LG_31");
    case RADAR_SPRITE_SAVEGAME:         return TheText.Get("LG_32");
    case RADAR_SPRITE_SCHOOL:           return TheText.Get("LG_33");
    case RADAR_SPRITE_QMARK:            return TheText.Get("LG_63");
    case RADAR_SPRITE_SWEET:            return TheText.Get("LG_35");
    case RADAR_SPRITE_TATTOO:           return TheText.Get("LG_36");
    case RADAR_SPRITE_THETRUTH:         return TheText.Get("LG_37");
    case RADAR_SPRITE_WAYPOINT:         return TheText.Get("LG_64");
    case RADAR_SPRITE_TORENORANCH:      return TheText.Get("LG_39");
    case RADAR_SPRITE_TRIADS:           return TheText.Get("LG_40");
    case RADAR_SPRITE_TRIADSCASINO:     return TheText.Get("LG_41");
    case RADAR_SPRITE_TSHIRT:           return TheText.Get("LG_42");
    case RADAR_SPRITE_WOOZIE:           return TheText.Get("LG_43");
    case RADAR_SPRITE_ZERO:             return TheText.Get("LG_44");
    case RADAR_SPRITE_DATEDISCO:        return TheText.Get("LG_45");
    case RADAR_SPRITE_DATEDRINK:        return TheText.Get("LG_46");
    case RADAR_SPRITE_DATEFOOD:         return TheText.Get("LG_47");
    case RADAR_SPRITE_TRUCK:            return TheText.Get("LG_48");
    case RADAR_SPRITE_CASH:             return TheText.Get("LG_51");
    case RADAR_SPRITE_FLAG:             return TheText.Get("LG_52");
    case RADAR_SPRITE_GYM:              return TheText.Get("LG_53");
    case RADAR_SPRITE_IMPOUND:          return TheText.Get("LG_57");
    case RADAR_SPRITE_LIGHT:            break;
    case RADAR_SPRITE_RUNWAY:           return TheText.Get("LG_65");
    case RADAR_SPRITE_GANGB:            return TheText.Get("LG_58");
    case RADAR_SPRITE_GANGP:            return TheText.Get("LG_59");
    case RADAR_SPRITE_GANGY:            return TheText.Get("LG_60");
    case RADAR_SPRITE_GANGN:            return TheText.Get("LG_61");
    case RADAR_SPRITE_GANGG:            return TheText.Get("LG_62");
    case RADAR_SPRITE_SPRAY:            return TheText.Get("LG_34");
    case RADAR_SPRITE_TORENO:           break;
    default:
        break;
    }
    NOTSA_UNREACHABLE("Bug in GetBlipName");
    return nullptr;
}

/*!
 * @notsa
 * @brief Returns the first index in `ms_RadarTrace` that is not a tracking blip.
 */
int32 CRadar::FindTraceNotTrackingBlipIndex() {
    for (auto&& [i, v] : rngv::enumerate(ms_RadarTrace)) {
        if (!v.m_bTrackingBlip) {
            return (int32)i;
        }
    }
    return -1;
}


/*!
 * @notsa
 * @brief Returns the color of the blip's apperance (Opaque)
 */
CRGBA tRadarTrace::GetStaticColour() const {
    // exe 0x586D8E-0x586DC6: the 2 bits at flag byte +0x25 (m_nCoordBlipAppearance): 0 = trace colour, 1 = HUD colour 13 (blue), else HUD colour 0 (red)
    switch (m_nCoordBlipAppearance) {
    case eBlipAppearance::BLIP_FLAG_FRIEND:
        return CRadar::GetRadarTraceColour(m_nColour, m_bBright, false);

    case eBlipAppearance::BLIP_FLAG_THREAT:
        return HudColour.GetIntColour(HUD_COLOUR_BLUE);

    default: // BLIP_FLAG_UNK (the setter stores 0..2 only)
        return HudColour.GetIntColour(HUD_COLOUR_RED);
    }
}

/*!
 * @notsa
 * @brief Returns the position of the blip always relative to the world.
 */
CVector tRadarTrace::GetWorldPos() const {
    CVector pos = { m_vPosition };
    if (m_pEntryExit) {
        m_pEntryExit->GetPositionRelativeToOutsideWorld(pos);
    }
    return pos;
}

/*!
 * @notsa
 * @brief Returns radar and screen relative positions of the trace.
 * @param radarPointDist Distance to the point in radar. (Optional)
 */
std::pair<CVector2D, CVector2D> tRadarTrace::GetRadarAndScreenPos(float* radarPointDist) const {
    const auto world = GetWorldPos();

    auto radar = CRadar::TransformRealWorldPointToRadarSpace(world);

    const auto dist = CRadar::LimitRadarPoint(radar); // Normalises vector `radar`
    if (radarPointDist) {
        *radarPointDist = dist;
    }

    const auto screen = CRadar::TransformRadarPointToScreenSpace(radar);

    return std::make_pair(radar, screen);
}
