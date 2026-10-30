#include "StdInc.h"

#include "MenuManager.h"
#include "MenuManager_Internal.h"
#include "World.h"
#include "MenuSystem.h"
#include "Pad.h"
#include "Streaming.h"
#include "AudioEngine.h"
#include "Radar.h"
#include "TheZones.h"
#include "PlaceName.h"

/**
 * @addr 0x573C90
 */
void CMenuManager::CalculateMapLimits(float& bottom, float& top, float& left, float& right) {
    bottom = m_vMapOrigin.y - m_fMapZoom;
    top    = m_vMapOrigin.y + m_fMapZoom;
    left   = m_vMapOrigin.x - m_fMapZoom;
    right  = m_vMapOrigin.x + m_fMapZoom;
}

// NOTSA
// from 0x577483 to 0x5775E5
void CMenuManager::PlaceRedMarker() {
    const auto pad = CPad::GetPad(m_nPlayerNumber);
    auto pressed = (
         pad->IsCirclePressed()
      || pad->IsMouseLButtonPressed()
      || pad->IsMouseRButtonPressed()
      || pad->IsStandardKeyJustPressed('T') || pad->IsStandardKeyJustPressed('t')
    );
    if (!pressed)
        return;

    if (CTheScripts::HideAllFrontEndMapBlips && CTheScripts::bPlayerIsOffTheMap)
        return;

    if (m_nTargetBlipIndex) {
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_BACK);
        CRadar::ClearBlip(m_nTargetBlipIndex);
        m_nTargetBlipIndex = 0;
    } else {
        AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_SELECT);
        m_nTargetBlipIndex = CRadar::SetCoordBlip(BLIP_COORD, { m_vMousePos.x, m_vMousePos.y, 0.0f }, BLIP_COLOUR_RED, BLIP_DISPLAY_BLIPONLY, "CODEWAY");
        CRadar::SetBlipSprite(m_nTargetBlipIndex, RADAR_SPRITE_WAYPOINT);
    }
}

// NOTSA 0x5775E5 to 0x577918
void CMenuManager::RadarZoomIn() {
    const auto pad = CPad::GetPad(m_nPlayerNumber);
    auto pressed = ( // todo:
           pad->NewState.LeftShoulder2
        && !pad->NewState.RightShoulder2
        && CPad::NewMouseControllerState.isMouseWheelMovedUp
        && CPad::NewKeyState.pgup
    );
    if (!pressed)
        return;

    // todo:
    auto v103 = 320.0f - m_vMapOrigin.x;
    auto v115 = 224.0f - m_vMapOrigin.y;
    auto v5 = 1.0f / m_fMapZoom;
    auto x = v5 * v103;
    auto y = v5 * v115;

    if (CTimer::GetTimeInMSPauseMode() - m_LastActionTime <= 20)
        return;

    if (m_fMapZoom >= FRONTEND_MAP_RANGE_MAX) {
        m_fMapZoom = FRONTEND_MAP_RANGE_MAX;
    } else {
        m_fMapZoom += 7.0f;
        if (CPad::NewMouseControllerState.isMouseWheelMovedUp) {
            m_fMapZoom += 21.0f;
        }
        m_vMapOrigin.x -= (x * m_fMapZoom - v103);
        m_vMapOrigin.y -= (y * m_fMapZoom - v115);
    }

    auto radar = CRadar::TransformRealWorldPointToRadarSpace(m_vMousePos);
    CRadar::LimitRadarPoint(radar);
    auto screen = CRadar::TransformRadarPointToScreenSpace(radar);

    while (screen.x > 576.0f) {
        m_vMousePos.x = m_vMousePos.x - 1.0f;
        radar = CRadar::TransformRealWorldPointToRadarSpace(m_vMousePos);
        CRadar::LimitRadarPoint(radar);
        screen = CRadar::TransformRadarPointToScreenSpace(radar);
    }

    while (screen.x < 64.0f) {
        m_vMousePos.x = m_vMousePos.x + 1.0f;
        radar = CRadar::TransformRealWorldPointToRadarSpace(m_vMousePos);
        CRadar::LimitRadarPoint(radar);
        screen = CRadar::TransformRadarPointToScreenSpace(radar);
    }

    while (screen.y < 64.0f) {
        m_vMousePos.y = m_vMousePos.y - 1.0f;
        radar = CRadar::TransformRealWorldPointToRadarSpace(m_vMousePos);
        CRadar::LimitRadarPoint(radar);
        screen = CRadar::TransformRadarPointToScreenSpace(radar);
    }

    while (screen.y > 384.0f) {
        m_vMousePos.y = m_vMousePos.y + 1.0f;
        radar = CRadar::TransformRealWorldPointToRadarSpace(m_vMousePos);
        CRadar::LimitRadarPoint(radar);
        screen = CRadar::TransformRadarPointToScreenSpace(radar);
    }
}

// 0x575130
void CMenuManager::PrintMap() {
    const auto pad = CPad::GetPad(m_nPlayerNumber);
    if (CPad::NewKeyState.standardKeys['Z'] || CPad::NewKeyState.standardKeys['z']) {
        m_bMapLoaded = false;
        m_DisplayTheMouse = false;
    }
    m_bDrawingMap = true;
    CRadar::InitFrontEndMap();
    if (!m_bMapLoaded) {
        if (m_nSysMenu != CMenuSystem::MENU_UNDEFINED) {
            CMenuSystem::SwitchOffMenu(m_nSysMenu);
            m_nSysMenu = CMenuSystem::MENU_UNDEFINED;
        }

        m_vMapOrigin = CVector2D(320.0f, 206.0f);
        m_fMapZoom = 140.0f;
        auto radar = CRadar::TransformRealWorldPointToRadarSpace(FindPlayerCentreOfWorld_NoSniperShift(0));
        CRadar::LimitRadarPoint(radar);
        const auto screen = CRadar::TransformRadarPointToScreenSpace(radar);

        const auto d{ screen - m_vMapOrigin };
        const auto BOUNDARY = 140.0f;

        if (d.x > BOUNDARY) {
            m_fMapZoom -= (d.x - BOUNDARY);
        } else if (d.x < -BOUNDARY) {
            m_fMapZoom -= (-BOUNDARY - d.x);
        } else if (d.y > BOUNDARY) {
            m_fMapZoom -= (d.y - BOUNDARY);
        } else if (d.y < -BOUNDARY) {
            m_fMapZoom -= (-BOUNDARY - d.y);
        }
        m_fMapZoom = std::max(m_fMapZoom, 70.0f);
    }
    // exe 0x5752xx: multiplies by the float constants 0x85F0A0 (1/6) and 0x858CC4 (0.2), no division
    const float zoomFactorForStretchX = m_fMapZoom * 0.1666666716337204f;
    const float zoomFactorForStretchY = m_fMapZoom * 0.20000000298023224f;
    // exe: the map window edges are ints: ftol of W * (60/640), H * (60/448), ... (one pre-folded float constant each, x87 product)
    const auto StretchRefInt = [](int32 size, int32 ref, float literal, float folded) {
        return size == ref ? NOTSA_FTOL(int32, literal) : NOTSA_FTOL(int32, (x87d)size * (x87d)folded);
    };
    const auto mapLeftI   = StretchRefInt(RsGlobal.maximumWidth,  DEFAULT_SCREEN_WIDTH,  60.0f,  0.09375f);                // X(60)
    const auto mapRightI  = StretchRefInt(RsGlobal.maximumWidth,  DEFAULT_SCREEN_WIDTH,  580.0f, 0.90625f);               // X(580)
    const auto mapTopI    = StretchRefInt(RsGlobal.maximumHeight, DEFAULT_SCREEN_HEIGHT, 60.0f,  0.1339285671710968f);    // Y(60)
    const auto mapBottomI = StretchRefInt(RsGlobal.maximumHeight, DEFAULT_SCREEN_HEIGHT, 388.0f, 0.8660714030265808f);    // Y(388)
    const CRect mapArea{ (float)mapLeftI, (float)mapTopI, (float)mapRightI, (float)mapBottomI };
    // legend rows: ftol(19 * H/448 + y)
    const auto LegendStepY = [](int32 y) {
        return RsGlobal.maximumHeight == DEFAULT_SCREEN_HEIGHT
            ? NOTSA_FTOL(int32, 19.0f + (float)y)
            : NOTSA_FTOL(int32, (x87d)RsGlobal.maximumHeight * (x87d)0.0424107126891613f + (x87d)(float)y);
    };
    const CVector2D mapOriginOffset{
        StretchX(m_vMapOrigin.x - m_fMapZoom),
        StretchY(m_vMapOrigin.y - m_fMapZoom)
    };

    if (m_bMapLoaded) {
        if (m_bStreamingDisabled && !m_bAllStreamingStuffLoaded) {
            m_RadarVisibilityChangeTime = CTimer::GetTimeInMSPauseMode();
            m_bViewRadar = false;
        }
        if (CTimer::GetTimeInMSPauseMode() - m_RadarVisibilityChangeTime > 400) {
            m_bViewRadar = true;
        }
    } else {
        m_RadarVisibilityChangeTime = CTimer::GetTimeInMSPauseMode();
        m_bViewRadar = false;
    }
    if (m_bMapLoaded) {
        if (m_bAllStreamingStuffLoaded) {
            m_bStreamingDisabled = false;
        }
        const CRect coords = { 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT };
        if (m_bViewRadar) {
            CSprite2d::DrawRect(coords, MENU_MAP_BACKGROUND);
            const auto stretchX = StretchX(zoomFactorForStretchX);
            const auto stretchY = StretchY(zoomFactorForStretchX);
            for (auto x = 0u; x < MAX_RADAR_WIDTH_TILES; x++) {
                for (auto y = 0u; y < MAX_RADAR_HEIGHT_TILES; y++) {
                    if (mapArea.left - stretchX < x * stretchX + mapOriginOffset.x) {
                        if (mapArea.bottom - stretchY < y * stretchY + mapOriginOffset.y) {
                            if (mapArea.right + stretchX > (x + 1) * stretchX + mapOriginOffset.x) {
                                if (mapArea.top + stretchY > (y + 1) * stretchY + mapOriginOffset.y) {
                                    CRadar::DrawRadarSectionMap(
                                        x, y,
                                        { StretchX(zoomFactorForStretchX) * x + mapOriginOffset.x,
                                          StretchY(zoomFactorForStretchX) * y + mapOriginOffset.y,
                                          StretchX(zoomFactorForStretchX) * (x + 1) + mapOriginOffset.x,
                                          StretchY(zoomFactorForStretchX) * (y + 1) + mapOriginOffset.y }
                                    );
                                }
                            }
                        }
                    }
                }
            }
        } else {
            CSprite2d::DrawRect(coords, MENU_BG);
            SmallMessageScreen("FEM_PWT");
            m_bAllStreamingStuffLoaded = true;
        }
    } else {
        CSprite2d::DrawRect(
            { StretchX(m_vMapOrigin.x - 145.0f),
              StretchY(m_vMapOrigin.y - 145.0f),
              StretchX(m_vMapOrigin.x + 145.0f),
              StretchY(m_vMapOrigin.y + 145.0f) },
            MENU_MAP_BORDER
        );

        CSprite2d::DrawRect(
            { StretchX(m_vMapOrigin.x - 141.0f),
              StretchY(m_vMapOrigin.y - 141.0f),
              StretchX(m_vMapOrigin.x + 141.0f),
              StretchY(m_vMapOrigin.y + 141.0f) },
            MENU_BG
        );

        CSprite2d::DrawRect(
            { StretchX(m_vMapOrigin.x - 140.0f),
              StretchY(m_vMapOrigin.y - 140.0f),
              StretchX(m_vMapOrigin.x + 140.0f),
              StretchY(m_vMapOrigin.y + 140.0f) },
            MENU_MAP_BACKGROUND
        );

        m_apBackgroundTextures[7].Draw(
            { StretchX(m_vMapOrigin.x - m_fMapZoom),
              StretchY(m_vMapOrigin.y - m_fMapZoom),
              StretchX(m_vMapOrigin.x + m_fMapZoom),
              StretchY(m_vMapOrigin.y + m_fMapZoom) },
            CRGBA(255, 255, 255, 255)
        );
    }

    if (m_bViewRadar || !m_bMapLoaded) {
        CRadar::DrawRadarGangOverlay(1);
        if (CTheZones::ZonesRevealed < 80) {
            for (auto x = 0u; x < MAX_RADAR_WIDTH_TILES - 2; x++) {
                for (auto y = 0u; y < MAX_RADAR_HEIGHT_TILES - 2; y++) {
                    if (!CTheZones::ZonesVisited[x][y]) {
                        CSprite2d::DrawRect(
                            { StretchX(zoomFactorForStretchY) * x + mapOriginOffset.x,
                              StretchY(zoomFactorForStretchY) * y + mapOriginOffset.y,
                              StretchX(zoomFactorForStretchY) * (x + 1) + mapOriginOffset.x,
                              StretchY(zoomFactorForStretchY) * (y + 1) + mapOriginOffset.y },
                            CRGBA(111, 137, 170, 200)
                        ); // radar fog
                    }
                }
            }
        }
        if (!CTheScripts::HideAllFrontEndMapBlips && !CTheScripts::bPlayerIsOffTheMap) {
            CRadar::DrawBlips();
        }
    }

    if ((m_bViewRadar || m_bMapLoaded) && m_bMapLoaded) {
        // exe: the border widths are ints (ftol of W * (4/640), H * (4/448), kept as bytes) and the background edges are int sums
        const auto borderX = static_cast<uint8>(StretchRefInt(RsGlobal.maximumWidth,  DEFAULT_SCREEN_WIDTH,  4.0f, 0.0062500000931322575f));
        const auto borderY = static_cast<uint8>(StretchRefInt(RsGlobal.maximumHeight, DEFAULT_SCREEN_HEIGHT, 4.0f, 0.008928571827709675f));

        // border between map and background
        CSprite2d::DrawRect(
            { 0.0f, 0.0f, SCREEN_WIDTH, mapArea.bottom },
            MENU_MAP_BORDER
        );

        CSprite2d::DrawRect(
            { 0.0f, mapArea.top, SCREEN_WIDTH, SCREEN_HEIGHT },
            MENU_MAP_BORDER
        );

        CSprite2d::DrawRect(
            { 0.0f, 0.0f, mapArea.left, SCREEN_HEIGHT },
            MENU_MAP_BORDER
        );

        CSprite2d::DrawRect(
            { mapArea.right, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT },
            MENU_MAP_BORDER
        );

        // background
        CSprite2d::DrawRect(
            { 0.0f, 0.0f, SCREEN_WIDTH, (float)(mapTopI - borderY) },
            MENU_BG
        );

        CSprite2d::DrawRect(
            { 0.0f, (float)(mapBottomI + borderY), SCREEN_WIDTH, SCREEN_HEIGHT },
            MENU_BG
        );

        CSprite2d::DrawRect(
            { 0.0f, 0.0f, (float)(mapLeftI - borderX), SCREEN_HEIGHT },
            MENU_BG
        );

        CSprite2d::DrawRect(
            { (float)(mapRightI + borderX), 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT },
            MENU_BG
        );
    }

    // 0x575E21
    if (m_bViewRadar) {
        if (CTheZones::ZonesRevealed >= 80
            || CTheZones::GetCurrentZoneLockedOrUnlocked(m_vMousePos)
                && !pad->NewMouseControllerState.isMouseLeftButtonPressed)
        {
            CPlaceName placeName;
            CFont::SetFontStyle(FONT_PRICEDOWN);
            CFont::SetWrapx(640.0);
            CFont::SetDropColor(MENU_BG);
            CFont::SetScale(StretchX(0.8f), StretchY(0.8f));
            CFont::SetColor(MENU_TEXT_LIGHT_GRAY);
            CFont::SetEdge(2);
            CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
            CFont::PrintString(
                mapArea.right - StretchX(30.0f), mapArea.top - StretchY(30.0f),
                placeName.GetForMap(m_vMousePos)
            );
        }

        if (m_bMapLegend) {
            // exe 0x575F98-0x576091: the window bottom is an int, ftol(H * (100/448)) then ftol(19 * H/448 + y) once per row (truncated every step);
            // the right edge is 555 (port: 550), the left 95 and the top 100 are the unrounded stretched values
            const auto midPoint = (CRadar::MapLegendCounter - 1) / 2;
            int32 y = StretchRefInt(RsGlobal.maximumHeight, DEFAULT_SCREEN_HEIGHT, 100.0f, 0.2232142835855484f);
            for (int32 i = midPoint + 3; i > 0; i--) {
                y = LegendStepY(y);
            }

            DrawWindow({
                    RsGlobal.maximumWidth == DEFAULT_SCREEN_WIDTH ? 95.0f : (float)((x87d)RsGlobal.maximumWidth * (x87d)0.1484375f),
                    RsGlobal.maximumHeight == DEFAULT_SCREEN_HEIGHT ? 100.0f : (float)((x87d)RsGlobal.maximumHeight * (x87d)0.2232142835855484f),
                    RsGlobal.maximumWidth == DEFAULT_SCREEN_WIDTH ? 555.0f : (float)((x87d)RsGlobal.maximumWidth * (x87d)0.8671875f),
                    (float)y
                }, "FE_MLG", 0, {0, 0, 0, 190}, true, true
            ); // Map Legend
            CFont::SetWrapx(SCREEN_WIDTH - 40.f);
            CFont::SetRightJustifyWrap(84.0f);
            CFont::SetDropShadowPosition(1);
            CFont::SetDropColor(MENU_BG);
            CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
            CFont::SetFontStyle(FONT_MENU);
            CFont::SetScale(StretchX(0.3f), StretchY(0.55f));
            CFont::SetColor(MENU_TEXT_SELECTED);
            if (CRadar::MapLegendCounter) {
                // exe 0x576190-0x57627B: x/y are ints (ftol of the exe-folded constants), the next row is ftol(19*H/448 + y)
                const auto row0Y = StretchRefInt(RsGlobal.maximumHeight, DEFAULT_SCREEN_HEIGHT, 127.0f, 0.2834821343421936f);
                auto currentY = row0Y;
                auto currentX = StretchRefInt(RsGlobal.maximumWidth, DEFAULT_SCREEN_WIDTH, 160.0f, 0.25f);
                for (auto legend = 0; legend < CRadar::MapLegendCounter; ++legend) {
                    CRadar::DrawLegend(
                        currentX,
                        currentY,
                        static_cast<eRadarSprite>(CRadar::MapLegendList[legend])
                    );

                    if (legend == midPoint) {
                        currentX = StretchRefInt(RsGlobal.maximumWidth, DEFAULT_SCREEN_WIDTH, 350.0f, 0.546875f);
                        currentY = row0Y;
                    } else {
                        currentY = LegendStepY(currentY);
                    }
                }
            }
        }
        if (m_nSysMenu != CMenuSystem::MENU_UNDEFINED) {
            CMenuSystem::Process(m_nSysMenu);
        }
    }
    m_bDrawingMap = false;
    CFont::SetWrapx(SCREEN_WIDTH - 10);
    CFont::SetRightJustifyWrap(10.0f);
    if (m_bMapLoaded) {
        if (m_nSysMenu != CMenuSystem::MENU_UNDEFINED) {
            // CURSORS - MOVE UP/DOWN
            // RETURN - TOGGLE OPTION
            DisplayHelperText("FEH_MPB"); 
        } else {
            // LMB/CURSORS - SCROLL
            // PGUP/PGDN/MSWHEEL - ZOOM
            // Z - OVERVIEW , L - LEGEND
            // RMB/T - TARGET , SPACEBAR - BLIPS MENU
            // ESC - BACK
            DisplayHelperText("FEH_MPH"); 
        }
        
    }
    m_bMapLoaded = true;
}

// 0x574900
void CMenuManager::PrintStats() {
    NOTSA_GLOBAL_LOCAL(scrollPos, 0x8CDFF4, (float), { -120.0f }); // -120.0f
    NOTSA_GLOBAL_LOCAL(prevScreenItem, 0x8CDFF8, (int8), { 15 });  // 15

    m_nCurrentScreenItem = std::min(m_nCurrentScreenItem, 8);
    // 0x574933
    if (prevScreenItem != m_nCurrentScreenItem) {
        scrollPos = -120.0f;
        prevScreenItem = m_nCurrentScreenItem;
    }

    const auto numStats = CStats::ConstructStatLine(99'999, m_nCurrentScreenItem);
    CFont::SetFontStyle(eFontStyle::FONT_MENU);
    CFont::SetScale(StretchX(0.3f), StretchY(0.75f));

    // 0x5749DA
    if (CTimer::GetTimeInMSPauseMode() - m_StatsScrollTime > 40) {
        if (m_fStatsScrollSpeed > 0.0f) {
            const float scrollDelta = StretchY(100.0f) / m_fStatsScrollSpeed;
            scrollPos += m_nStatsScrollDirection ? scrollDelta : -scrollDelta;
        }
        m_StatsScrollTime = CTimer::GetTimeInMSPauseMode();
    }

    const float minY = StretchY(50.0f);
    const float maxY = SCREEN_STRETCH_FROM_BOTTOM(50.0f);
    const float visibleTop = StretchY(50.0f);
    const float visibleBottom = StretchY(360.0f);

    // 0x574A4E
    for (auto i = 0; i < numStats; ++i) {
        float y = StretchY(54.0f) * (float)i + minY - scrollPos;

        const float dy = StretchY(float(numStats + 7) * 54.0f);
        while (y < minY) {
            y += dy;
        }

        while (y > maxY) {
            y -= dy;
        }

        if (y > visibleTop && y < visibleBottom) {
            CStats::ConstructStatLine(i, m_nCurrentScreenItem);

            if (!gGxtString[0]) {
                y -= StretchY(37.0f);
            }

            // 0x574BE1
            float alpha_f{};
            if (y > StretchY(80.0f) && y < StretchY(330.0f)) {
                if (y < StretchY(110.0f)) {
                    // Fade In
                    alpha_f = (y - StretchY(80.0f)) * 8.0f;
                } else if (y > StretchY(300.0f)) {
                    // Fade Out
                    alpha_f = (StretchY(330.0f) - y) * 8.0f;
                } else {
                    alpha_f = 255.0f;
                }
            }
            const uint8 alpha = NOTSA_FTOL(uint8, std::clamp(alpha_f, 0.0f, 255.0f));

            // 0x574D3B
            CFont::SetDropColor(CRGBA(MENU_BG, alpha));
            CFont::SetEdge(1);
            CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
            CFont::SetColor(CRGBA(MENU_TEXT_LIGHT_GRAY, alpha));
            CFont::PrintString(StretchX(450.0f), y, gGxtString);

            const auto color = CRGBA(MENU_TEXT_SELECTED, alpha);
            // 0x574DD2
            if (CStats::m_ThisStatIsABarChart) {
                float val = CStats::GetStatValue((eStats)CStats::m_ThisStatIsABarChart) * 0.001f * 100.0f;
                float clamped = std::min(val, 1000.0f);

                CSprite2d::DrawBarChart(
                    StretchX(400.0f), StretchY(17.0f) + y,
                    NOTSA_FTOL(uint16, StretchX(100.0f)), NOTSA_FTOL(uint8, StretchY(10.0f)),
                    clamped, 0, 0, 1, color, CRGBA()
                );
            } else {
                CFont::SetColor(color);
                CFont::PrintString(StretchX(450.0f), StretchY(17.0f) + y, gGxtString2);
            }
        }
    }

    // 0x574F7E
    CFont::SetEdge(1);
    CFont::SetDropColor(MENU_BG);
    CFont::SetColor(MENU_TEXT_LIGHT_GRAY);
    CFont::SetFontStyle(eFontStyle::FONT_PRICEDOWN);
    CFont::SetScale(StretchX(0.6f), StretchY(0.8f));
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);

    GxtChar ratingText[80];
    AsciiToGxtChar(std::format("{}", CStats::FindCriminalRatingNumber()).c_str(), ratingText);
    GxtCharStrcpy(gGxtString, CStats::FindCriminalRatingString());
    CFont::PrintString(StretchX(450.0f), StretchY(40.0f), gGxtString);
    CFont::PrintString(StretchX(450.0f), StretchY(57.0f), ratingText);
}

// 0x576320
void CMenuManager::PrintBriefs() {
    CFont::SetColor(MENU_TEXT_LIGHT_GRAY);
    CFont::SetDropColor(MENU_BG);
    CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
    CFont::SetFontStyle(eFontStyle::FONT_SUBTITLES);
    CFont::SetScaleForCurrentLanguage(StretchX(0.49f), StretchY(0.7f));
    CFont::SetWrapx(StretchX(560.0f));
    CFont::SetDropShadowPosition(1);
    CFont::SetJustify(false);

    static constexpr size_t BRIEFS_MAX_LINES_ON_SCREEN = 4u;

    // underflow check
    if (m_nSelectedRow >= m_nSelectedRow - (BRIEFS_MAX_LINES_ON_SCREEN - 1)) {
        float h = 100.0f;
        for (auto i = 0u; i < BRIEFS_MAX_LINES_ON_SCREEN; i++) {
            auto& brief = CMessages::PreviousBriefs[m_nSelectedRow - i];
            if (!brief.Text)
                continue;

            CMessages::InsertNumberInString2(brief.Text, brief.NumbersToInsert, gGxtString);
            CMessages::InsertStringInString(gGxtString, brief.StringToInsert);
            CMessages::InsertPlayerControlKeysInString(gGxtString);
            CFont::PrintString(StretchX(70.0f), StretchY(h), gGxtString);

            h += 70.0f; // 210 / (BRIEFS_MAX_LINES_ON_SCREEN - 1)
        }
    }
    CFont::SetJustify(false); // redundant

    NOTSA_GLOBAL_LOCAL(drawArrows, 0x8CDFF9, (bool), { true }); // true

    if (!m_bMapLoaded)
        return;

    if (CTimer::GetTimeInMSPauseMode() - m_BriefsArrowBlinkTime > 700) {
        m_BriefsArrowBlinkTime = CTimer::GetTimeInMSPauseMode();
        drawArrows = !drawArrows;
    }

    if (drawArrows) {
        // up arrow
        if (m_nSelectedRow < 19u && CMessages::PreviousBriefs[m_nSelectedRow + 1].Text) {
            CSprite2d::Draw2DPolygon(
                StretchX(50.0f), StretchY(100.0f),
                StretchX(55.0f), StretchY(110.0f),
                StretchX(45.0f), StretchY(110.0f),
                StretchX(50.0f), StretchY(100.0f),
                MENU_TEXT_LIGHT_GRAY
            );
        }

        // down arrow
        if (m_nSelectedRow > 3u) {
            CSprite2d::Draw2DPolygon(
                StretchX(50.0f), StretchY(348.0f),
                StretchX(55.0f), StretchY(338.0f),
                StretchX(45.0f), StretchY(338.0f),
                StretchX(50.0f), StretchY(348.0f),
                MENU_TEXT_LIGHT_GRAY
            );
        }
    }
}

// 0x5746F0
void CMenuManager::PrintRadioStationList() {
    // draw all, except current
    for (auto i = 1u; i < std::size(m_apRadioSprites); i++) {
        if (m_nRadioStation == i) {
            continue;
        }

        m_apRadioSprites[i].Draw(
            StretchX(float(47 * (i - 1) + 44)),
            StretchY(300.0f),
            StretchX(35.0f),
            StretchY(35.0f),
            CRGBA(MENU_TEXT_WHITE, 30) // todo: fix alpha
        );
    }

    // highlight current radio station
    if (m_nRadioStation > 0 && m_nRadioStation < (int8)std::size(m_apRadioSprites)) {
        m_apRadioSprites[m_nRadioStation].Draw(
            StretchX(float(47 * m_nRadioStation - 15)),
            StretchY(290.0f),
            StretchX(60.0f),
            StretchY(60.0f),
            MENU_TEXT_WHITE
        );
    }
}
