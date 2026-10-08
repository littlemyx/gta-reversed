/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "Font.h"

#include "eLanguage.h"

auto& FontRenderStateBuf = StaticRef<std::array<CFontChar, 9>>(0xC716B0);
auto& pEmptyChar = StaticRef<CFontChar*>(0xC716A8);

auto& gFontData = StaticRef<std::array<tFontData, 2>>(0xC718B0);

void CFont::InjectHooks() {
    RH_ScopedClass(CFont);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Initialise, 0x5BA690);
    RH_ScopedInstall(Shutdown, 0x7189B0);
    RH_ScopedInstall(PrintChar, 0x718A10);
    RH_ScopedInstall(ParseToken, 0x718F00);

    // styling functions
    RH_ScopedInstall(SetScale, 0x719380);
    RH_ScopedInstall(SetScaleForCurrentLanguage, 0x7193A0);
    RH_ScopedInstall(SetSlantRefPoint, 0x719400);
    RH_ScopedInstall(SetSlant, 0x719420);
    RH_ScopedInstall(SetColor, 0x719430);
    RH_ScopedInstall(SetFontStyle, 0x719490);
    RH_ScopedInstall(SetWrapx, 0x7194D0);
    RH_ScopedInstall(SetCentreSize, 0x7194E0);
    RH_ScopedInstall(SetRightJustifyWrap, 0x7194F0);
    RH_ScopedInstall(SetAlphaFade, 0x719500);
    RH_ScopedInstall(SetDropColor, 0x719510);
    RH_ScopedInstall(SetDropShadowPosition, 0x719570);
    RH_ScopedInstall(SetEdge, 0x719590);
    RH_ScopedInstall(SetProportional, 0x7195B0);
    RH_ScopedInstall(SetBackground, 0x7195C0);
    RH_ScopedInstall(SetBackgroundColor, 0x7195E0);
    RH_ScopedInstall(SetJustify, 0x719600);
    RH_ScopedInstall(SetOrientation, 0x719610);

    RH_ScopedInstall(InitPerFrame, 0x719800);
    RH_ScopedInstall(RenderFontBuffer, 0x719840);
    RH_ScopedInstall(GetStringWidth, 0x71A0E0);
    RH_ScopedInstall(DrawFonts, 0x71A210);
    RH_ScopedInstall(ProcessCurrentString, 0x71A220);
    RH_ScopedInstall(GetNumberLines, 0x71A5E0);
    RH_ScopedInstall(ProcessStringToDisplay, 0x71A600);
    RH_ScopedInstall(GetTextRect, 0x71A620);
    RH_ScopedInstall(PrintString, 0x71A700);
    RH_ScopedInstall(PrintStringFromBottom, 0x71A820);
    RH_ScopedInstall(GetCharacterSize, 0x719750);
    RH_ScopedInstall(LoadFontValues, 0x7187C0);
    // Install("", "GetScriptLetterSize", 0x719670, &GetScriptLetterSize);
    RH_ScopedInstall(FindSubFontCharacter, 0x7192C0);
    RH_ScopedGlobalInstall(GetLetterIdPropValue, 0x718770);
}

// 0x7187C0
void CFont::LoadFontValues() {
    CFileMgr::SetDir("");
    auto* file = CFileMgr::OpenFile("DATA\\FONTS.DAT", "rb");

    char attrib[32];

    uint32 totalFonts = 0;
    uint32 fontId = 0;

    for (auto line = CFileLoader::LoadLine(file); line; line = CFileLoader::LoadLine(file)) {
        if (*line == '\0' || *line == '#')
            continue;

        if (sscanf_s(line, "%s", SCANF_S_STR(attrib)) == EOF)
            continue;

        if (!memcmp(attrib, "[TOTAL_FONTS]", 14)) {
            auto nextLine = CFileLoader::LoadLine(file);

            VERIFY(sscanf_s(nextLine, "%d", &totalFonts) == 1);
        }
        else if (!memcmp(attrib, "[FONT_ID]", 10)) {
            auto nextLine = CFileLoader::LoadLine(file);

            VERIFY(sscanf_s(nextLine, "%d", &fontId) == 1);
        }
        else if (!memcmp(attrib, "[REPLACEMENT_SPACE_CHAR]", 25)) {
            auto nextLine = CFileLoader::LoadLine(file);
            uint8 spaceValue;

            VERIFY(sscanf_s(nextLine, "%hhu", &spaceValue) == 1);
            gFontData[fontId].m_spaceValue = spaceValue;
        }
        else if (!memcmp(attrib, "[PROP]", 7)) {
            for (int32 i = 0; i < 26; i++) {
                auto nextLine = CFileLoader::LoadLine(file);
                int32 propValues[8]{};

                VERIFY(sscanf_s(nextLine, "%d  %d  %d  %d  %d  %d  %d  %d",
                    &propValues[0], &propValues[1], &propValues[2], &propValues[3],
                    &propValues[4], &propValues[5], &propValues[6], &propValues[7]
                ) == 8);

                for (auto j = 0u; j < std::size(propValues); j++) {
                    gFontData[fontId].m_propValues[i * 8 + j] = propValues[j];
                }
            }
        }
        else if (!memcmp(attrib, "[UNPROP]", 9)) {
            auto nextLine = CFileLoader::LoadLine(file);
            uint32 unpropValue;

            VERIFY(sscanf_s(nextLine, "%d", &unpropValue) == 1);
            gFontData[fontId].m_unpropValue = unpropValue;
        }
    }

    CFileMgr::CloseFile(file);
}

// 0x5BA690
void CFont::Initialise() {
    int32 fontsTxd = CTxdStore::AddTxdSlot("fonts");
    CTxdStore::LoadTxd(fontsTxd, "MODELS\\FONTS.TXD");
    CTxdStore::AddRef(fontsTxd);
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(fontsTxd);
    Sprite[0].SetTexture("font2", "font2m");
    Sprite[1].SetTexture("font1", "font1m");

    LoadFontValues();

    SetScale(1.0f, 1.0f);
    SetSlantRefPoint(SCREEN_WIDTH, 0.0f);
    SetSlant(0.0f);

    SetColor(CRGBA(255, 255, 255, 0));
    SetOrientation(eFontAlignment::ALIGN_LEFT);
    SetJustify(false);

    SetWrapx(SCREEN_WIDTH);

    SetCentreSize(SCREEN_WIDTH);
    SetBackground(false, false);

    SetBackgroundColor(CRGBA(128, 128, 128, 128));
    SetProportional(true);
    SetFontStyle(eFontStyle::FONT_GOTHIC);
    SetRightJustifyWrap(0.0f);
    SetAlphaFade(255.0f);
    SetDropShadowPosition(0);
    CTxdStore::PopCurrentTxd();

    int32 ps2btnsTxd = CTxdStore::AddTxdSlot("ps2btns");
    CTxdStore::LoadTxd(ps2btnsTxd, "MODELS\\PCBTNS.TXD");
    CTxdStore::AddRef(ps2btnsTxd);
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(ps2btnsTxd);

    ButtonSprite[1].SetTexture("up", "upA");
    ButtonSprite[2].SetTexture("down", "downA");
    ButtonSprite[3].SetTexture("left", "leftA");
    ButtonSprite[4].SetTexture("right", "rightA");

    CTxdStore::PopCurrentTxd();
}

// 0x7189B0
void CFont::Shutdown() {
    std::ranges::for_each(Sprite, [](CSprite2d& sprite) { sprite.Delete(); });
    CTxdStore::SafeRemoveTxdSlot("fonts"); // FIX_BUGS: Added check for is slot exists
    std::ranges::for_each(ButtonSprite, [](CSprite2d& sprite) { sprite.Delete(); });
    CTxdStore::SafeRemoveTxdSlot("ps2btns"); // FIX_BUGS: Added check for is slot exists
}

// this adds a single character into rendering buffer
// 0x718A10
void CFont::PrintChar(float x, float y, char character) {
    // Out of screen
    if (y < 0.0f || y > SCREEN_HEIGHT || x < 0.0f || x > SCREEN_WIDTH) {
        return;
    }

    const auto& h = RenderState.m_fHeight;
    const auto& w = RenderState.m_fWidth;

    if (PS2Symbol) {
        // Extra symbol to be drawn (e.g. PS2 buttons)
        // CRect ctor is (left, bottom, right, top)
        const CRect rect{ x, (h + h) + y, h * 17.0f + x, h * 19.0f + y };
        ButtonSprite[PS2Symbol].Draw(rect, CRGBA{ 255, 255, 255, RenderState.m_color.a });
        return;
    }

    auto ch     = (uint8)character;
    bool zeroed = false;
    if (ch == 0 || ch == '?') {
        ch     = 0;
        zeroed = true;
    }

    const float propValue = GetLetterIdPropValue(ch) * 0.03125f;

    if (RenderState.m_nFontStyle == 1 && ch == 0xD0) {
        ch = 0;
    }

    const float row = (float)(ch >> 4);
    const float u   = (float)(ch & 0xF) * 0.0625f;
    const auto& col = RenderState.m_color;

    CRect rect;
    if (RenderState.m_wFontTexture != 0 && RenderState.m_wFontTexture != 1) {
        const float v = row * 0.0625f;
        if (zeroed) {
            return;
        }

        rect.left   = x;
        rect.bottom = y;
        rect.right  = w * 32.0f * propValue + x;
        rect.top    = h * 32.0f * 0.5f + y;

        const float v1 = v + 0.0625f;
        const float u2 = propValue * 0.0625f + u;
        CSprite2d::AddToBuffer(rect, col, u, v, u2, v, u, v1, u2 - 0.0001f, v1 - 0.0001f);
        return;
    }

    const float v = (float)((double)row * 0.078125);
    if (zeroed) {
        return;
    }

    rect.left = x;
    if (RenderState.m_fSlant == 0.0f) {
        rect.bottom = y;
        rect.right  = w * 32.0f + x;
        const float u2 = (u + 0.0625f) - 0.001f;
        if (ch < 0xC0) {
            rect.top = h * 40.0f * 0.5f + y;

            const float v1 = v + 0.0021f;
            const float v3 = (v + 0.078125f) - 0.0021f;
            CSprite2d::AddToBuffer(rect, col, u, v1, u2, v1, u, v3, u2, v3);
        } else {
            rect.top = h * 32.0f * 0.5f + y;

            const float vb = v + 0.078125f;
            const float v1 = v + 0.0021f;
            CSprite2d::AddToBuffer(rect, col, u, v1, u2, v1, u, vb - 0.016f, u2, vb - 0.015f);
        }
    } else {
        // Slanted
        rect.bottom = y + 0.015f;
        rect.right  = w * 32.0f + x;
        rect.top    = h * 40.0f * 0.5f + y + 0.015f;

        const float vb = v + 0.078125f;
        const float u2 = (u + 0.0625f) - 0.001f;
        CSprite2d::AddToBuffer(rect, col, u, v + 0.00055f, u2, v + 0.0121f, u, vb - 0.009f, u2, (vb - 0.0021f) + 0.01f);
    }
}

// Tags processing
// 0x718F00
char* CFont::ParseToken(char* text, CRGBA& color, bool isBlip, char* tag) {
    // info about tokens: https://gtamods.com/wiki/GXT#Tokens

    char* next = ++text;

    auto ApplyStyle = [&](eHudColours hudColor) {
        if (!isBlip)
            color = HudColour.GetRGBA(hudColor, color.a);

        if (tag)
            *tag = *next;
    };

    switch (*next) {
    case '<':
        PS2Symbol = EXSYMBOL_DPAD_LEFT;
        break;
    case '>':
        PS2Symbol = EXSYMBOL_DPAD_RIGHT;
        break;
    case 'A':
    case 'a':
        PS2Symbol = EXSYMBOL_L3;
        break;
    case 'B':
    case 'b':
        ApplyStyle(HUD_COLOUR_DARK_BLUE);
        break;
    case 'C':
    case 'c':
        PS2Symbol = EXSYMBOL_R3;
        break;
    case 'D':
    case 'd':
        PS2Symbol = EXSYMBOL_DPAD_DOWN;
        break;
    case 'G':
    case 'g':
        ApplyStyle(HUD_COLOUR_GREEN);
        break;
    case 'H':
    case 'h':
        if (!isBlip) {
            color = {
                (uint8)std::min((float)color.r * 1.5f, 255.0f),
                (uint8)std::min((float)color.g * 1.5f, 255.0f),
                (uint8)std::min((float)color.b * 1.5f, 255.0f),
                color.a
            };
        }

        if (tag)
            *tag = *next;
        break;
    case 'J':
    case 'j':
        PS2Symbol = EXSYMBOL_R1;
        break;
    case 'K':
    case 'k':
        PS2Symbol = EXSYMBOL_KEY;
        break;
    case 'M':
    case 'm':
        PS2Symbol = EXSYMBOL_L2;
        break;
    case 'N':
    case 'n':
        m_bNewLine = true;
        break;
    case 'O':
    case 'o':
        PS2Symbol = EXSYMBOL_CIRCLE;
        break;
    case 'P':
    case 'p':
        ApplyStyle(HUD_COLOUR_PURPLE);
        break;
    case 'Q':
    case 'q':
        PS2Symbol = EXSYMBOL_SQUARE;
        break;
    case 'R':
    case 'r':
        ApplyStyle(HUD_COLOUR_RED);
        break;
    case 'S':
    case 's':
        ApplyStyle(HUD_COLOUR_LIGHT_GRAY);
        break;
    case 'T':
    case 't':
        PS2Symbol = EXSYMBOL_TRIANGLE;
        break;
    case 'U':
    case 'u':
        PS2Symbol = EXSYMBOL_DPAD_UP;
        break;
    case 'V':
    case 'v':
        PS2Symbol = EXSYMBOL_R2;
        break;
    case 'W':
    case 'w':
        ApplyStyle(HUD_COLOUR_LIGHT_GRAY);
        break;
    case 'X':
    case 'x':
        PS2Symbol = EXSYMBOL_CROSS;
        break;
    case 'Y':
    case 'y':
        ApplyStyle(HUD_COLOUR_CREAM);
        break;
    case 'l':
        ApplyStyle(HUD_COLOUR_BLACK);
        break;
    default:
        break;
    }

    if (*next != '~') {
        // skip text to the next '~' character.
        for(; *next && *next != '~'; next++);
    }

    if (*next)
        return next + 1;

    return next + 2;
}

// Text scaling
// 0x719380
void CFont::SetScale(float w, float h) {
    m_Scale.Set(w, h);
}

// Text scaling depends on current language
// 0x7193A0
void CFont::SetScaleForCurrentLanguage(float w, float h) {
    switch (FrontEndMenuManager.m_nPrefsLanguage) {
    case eLanguage::FRENCH:
    case eLanguage::GERMAN:
    case eLanguage::ITALIAN:
    case eLanguage::SPANISH:
        m_Scale.Set(w * 0.8f, h);
        break;
    default:
        m_Scale.Set(w, h);
    }
}

// Set text rotation point
// 0x719400
void CFont::SetSlantRefPoint(float x, float y) {
    m_fSlantRefPoint.Set(x, y);
}

// Set text rotation angle
// 0x719420
void CFont::SetSlant(float value) {
    m_fSlant = value;
}

// Set text color
// 0x719430
void CFont::SetColor(CRGBA color) {
    m_Color = color;

    if (m_fFontAlpha < 255.0f) {
        m_Color.a = (uint8)(float(color.a) * m_fFontAlpha / 255.0f);
    }
}

// Set text style
// 0x719490
void CFont::SetFontStyle(eFontStyle style) {
    switch (style) {
    case eFontStyle::FONT_PRICEDOWN:
        m_FontTextureId = 1;
        m_FontStyle = 1;
        break;
    case eFontStyle::FONT_MENU:
        m_FontTextureId = 0;
        m_FontStyle = 2;
        break;
    default:
        m_FontTextureId = static_cast<uint8>(style);
        m_FontStyle = 0;
    }
}

// Set line width at right
// 0x7194D0
void CFont::SetWrapx(float value) {
    m_fWrapx = value;
}

// Set line width at center
// 0x7194E0
void CFont::SetCentreSize(float value) {
    m_fFontCentreSize = value;
}

// 0x7194F0
void CFont::SetRightJustifyWrap(float value) {
    m_fRightJustifyWrap = value;
}

// Like a 'global' font alpha, multiplied with each text alpha (from SetColor)
// 0x719500
void CFont::SetAlphaFade(float alpha) {
    m_fFontAlpha = alpha;
}

// Drop color is used for text shadow and text outline
// 0x719510
void CFont::SetDropColor(CRGBA color) {
    m_FontDropColor = color;

    if (m_fFontAlpha < 255.0f) {
        m_FontDropColor.a = (uint8)(float(m_Color.a) * m_fFontAlpha);
    }
}

// Set shadow size
// 0x719570
void CFont::SetDropShadowPosition(int16 value) {
    m_nFontOutlineSize = 0;
    m_nFontOutlineOrShadow = 0;
    m_nFontShadow = (uint8)value;
}

// Set outline size
// 0x719590
void CFont::SetEdge(int8 value) {
    m_nFontShadow = 0;
    m_nFontOutlineSize = value;
    m_nFontOutlineOrShadow = value;
}

// Toggles character proportions in text
// 0x7195B0
void CFont::SetProportional(bool on) {
    m_bFontPropOn = on;
}

// Setups text background
// 0x7195C0
void CFont::SetBackground(bool enable, bool includeWrap) {
    m_bFontBackground = enable;
    m_bEnlargeBackgroundBox = includeWrap;
}

// Sets background color
// 0x7195E0
void CFont::SetBackgroundColor(CRGBA color) {
    m_FontBackgroundColor = color;
}

// 0x719600
void CFont::SetJustify(bool on) {
    m_bFontJustify = on;
}

// 0x719610
void CFont::SetOrientation(eFontAlignment alignment) {
    m_bFontCentreAlign = alignment == eFontAlignment::ALIGN_CENTER;
    m_bFontRightAlign = alignment == eFontAlignment::ALIGN_RIGHT;
}

// Need to call this each frame
// 0x719800
void CFont::InitPerFrame() {
    ZoneScoped;

    m_nFontOutline = 0;
    m_nFontOutlineOrShadow = 0;
    m_nFontShadow = 0;
    m_bNewLine = false;
    PS2Symbol = EXSYMBOL_NONE;
    RenderState.m_wFontTexture = 0; // todo: -1
    pEmptyChar = &FontRenderStateBuf[0]; // FontRenderStatePointer.pRenderState

    CSprite::InitSpriteBuffer();
}

// Draw text we have in buffer
// 0x719840
void CFont::RenderFontBuffer() {
    if (pEmptyChar == &FontRenderStateBuf[0]) {
        return;
    }

    Sprite[RenderState.m_wFontTexture].SetRenderState();
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));

    RenderState.Set(FontRenderStateBuf[0]);

    CRGBA color = RenderState.m_color;
    float x     = RenderState.m_vPosn.x;
    float y     = RenderState.m_vPosn.y;

    // The buffer is a sequence of [CFontChar][NUL terminated string][padding to 4 bytes]
    // The first CFontChar is at `FontRenderStateBuf[0]`, its string follows right after it.
    // NOTE: The real buffer (0xC716B0, 0x200 bytes) is larger than `std::array<CFontChar, 9>` (0x1B0 bytes),
    //       so go through an integer to make the walk past the end of that array object explicit.
    auto*       cur = reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(&FontRenderStateBuf[0])) + sizeof(CFontChar);
    const auto* end = reinterpret_cast<const char*>(reinterpret_cast<uintptr_t>(pEmptyChar));

    if (cur < end) {
        do {
            if (*cur == '\0') {
                // End of the current string => skip to the next (4 byte aligned) CFontChar
                cur++;
                while (reinterpret_cast<uintptr_t>(cur) & 3) {
                    cur++;
                }
                if (cur >= end) {
                    break;
                }

                RenderState.Set(*reinterpret_cast<CFontChar*>(cur));
                x     = RenderState.m_vPosn.x;
                y     = RenderState.m_vPosn.y;
                color = RenderState.m_color;
                cur += sizeof(CFontChar);
            }

            PS2Symbol = EXSYMBOL_NONE;
            while (*cur == '~' && PS2Symbol == EXSYMBOL_NONE) {
                cur = ParseToken(cur, color, RenderState.m_bContainImages, nullptr);
                if (!RenderState.m_bContainImages) {
                    RenderState.m_color = color;
                }
            }

            auto letter = (uint8)(*cur - ' ');
            if (RenderState.m_nFontStyle) {
                letter = FindSubFontCharacter(letter, RenderState.m_nFontStyle);
            } else if (letter == 0x91) {
                letter = '@';
            } else if (letter > 0x9B) {
                letter = 0;
            }

            if (RenderState.m_fSlant != 0.0f) {
                y = (RenderState.m_vSlanRefPoint.x - x) * RenderState.m_fSlant + RenderState.m_vSlanRefPoint.y;
            }

            if (PS2Symbol == EXSYMBOL_NONE || !RenderState.m_bContainImages) {
                PrintChar(x, y, (char)letter);
            }

            if (PS2Symbol != EXSYMBOL_NONE) {
                x = (RenderState.m_fHeight * 17.0f + (float)(int8)RenderState.m_nOutline) + x;
            } else {
                const auto idx = (letter == '?') ? 0 : letter;
                const auto& fontData = gFontData[RenderState.m_wFontTexture];
                const auto  charW = RenderState.m_bPropOn ? fontData.m_propValues[idx] : fontData.m_unpropValue;
                x = ((float)(int8)RenderState.m_nOutline + (float)charW) * RenderState.m_fWidth + x;
            }

            if (letter == 0) {
                x = RenderState.m_fWrap + x;
            }

            if (*cur == '\0') {
                if (PS2Symbol != EXSYMBOL_NONE) {
                    PS2Symbol = EXSYMBOL_NONE;
                    Sprite[RenderState.m_wFontTexture].SetRenderState();
                }
            } else if (PS2Symbol == EXSYMBOL_NONE) {
                cur++;
            } else {
                PS2Symbol = EXSYMBOL_NONE;
                Sprite[RenderState.m_wFontTexture].SetRenderState();
            }
        } while (cur < end);
    }

    CSprite::FlushSpriteBuffer();
    CSprite2d::RenderVertexBuffer();
    pEmptyChar = &FontRenderStateBuf[0];
}

// 0x71A0E0
float CFont::GetStringWidth(const GxtChar* string, bool full, bool scriptText) {
    size_t len = CMessages::GetStringLength(string);
    GxtChar data[400] = { 0 };

    strncpy_s((char*)data, sizeof(data), AsciiFromGxtChar(string), len);
    CMessages::InsertPlayerControlKeysInString(data);

    float width = 0.0f;
    bool lastWasTag = false, lastWasLetter = false;
    auto* pStr = data;

    while (true) {
        if (*pStr == ' ' && !full)
            break;
        if (*pStr == '\0')
            break;

        if (*pStr == '~') {
            if (!full && (lastWasTag || lastWasLetter))
                return width;

            auto* next = pStr + 1;

            if (*next != '~') {
                for (; *next && *next != '~'; next++);
            }

            pStr = next + 1;

            if (lastWasLetter || *pStr == '~')
                lastWasTag = true;
        }
        else {
            if (!full && *pStr == ' ' && lastWasTag)
                return width;

            char upper = *pStr - 0x20;

            pStr++;
            if (scriptText) {
                width += GetScriptLetterSize(upper);
            }
            else {
                width += GetCharacterSize(upper);
            }

            lastWasLetter = true;
        }
    }

    return width;
}

// same as RenderFontBuffer() (0x71A210)
void CFont::DrawFonts() {
    ZoneScoped;

    RenderFontBuffer();
}

// 0x71A220
int16 CFont::ProcessCurrentString(bool print, float x, float y, const GxtChar* text) {
    // 0x719B40 - Adds a (part of a) line to the font buffer (not reversed yet)
    const auto RenderString = [](float px, float py, const char* str, const char* strEnd, float spaceExtra) {
        plugin::Call<0x719B40, float, float, const char*, const char*, float>(px, py, str, strEnd, spaceExtra);
    };

    int32 spaceCount     = 0;                     // Number of spaces in the current line (used for justify)
    int32 lineCount      = 0;
    const CRGBA savedColor = m_Color;
    bool  isFirstWord    = true;
    char  tag            = '\0';
    float lastWordEnd    = 0.0f;
    float lineWidth      = (!m_bFontCentreAlign && !m_bFontRightAlign) ? x : 0.0f;
    float drawY          = y;

    char* lineStart = (char*)text;
    char* cur       = (char*)text;
    char  buf[256];

    while (*cur) {
        PS2Symbol = EXSYMBOL_NONE;
        float wordWidth = GetStringWidth((const GxtChar*)cur, false, false);

        if (*cur == '~') {
            CRGBA ignoredColor;
            cur = ParseToken(cur, ignoredColor, true, &tag);
        }

        float limit;
        if (m_bFontCentreAlign) {
            limit = m_fFontCentreSize;
        } else if (m_bFontRightAlign) {
            limit = x - m_fRightJustifyWrap;
        } else {
            limit = m_fWrapx;
        }

        wordWidth = wordWidth + lineWidth;

        if ((limit < wordWidth && !isFirstWord) || m_bNewLine) {
            // Line is full (or a new line was requested): flush it
            float spaceExtra = 0.0f;
            if (PS2Symbol != EXSYMBOL_NONE) {
                cur -= 3;
            }
            char* lineEnd = m_bNewLine ? cur - 3 : cur;

            if (m_bFontJustify && !m_bFontCentreAlign) {
                spaceExtra = (m_fWrapx - lastWordEnd) / (float)(int32)(int16)spaceCount;
            }

            float drawX;
            if (m_bFontCentreAlign) {
                drawX = x - lineWidth * 0.5f;
            } else if (m_bFontRightAlign) {
                drawX = x - (lineWidth - GetCharacterSize(0));
            } else {
                drawX = x;
            }

            lineCount++;
            if (print) {
                RenderString(drawX, drawY, lineStart, lineEnd, spaceExtra);
            }

            if (tag) {
                // Re-insert the colour tag at the beginning of the rest of the text
                sprintf_s(gString, "~%c~", tag);
                // BUG: The original copies without any bounds checking (`buf` is 256 bytes)
                const auto bufSize = notsa::IsFixBugs() ? sizeof(buf) : (size_t)-1;
                size_t     len     = 0;
                for (const char* src = gString; *src && len + 1 < bufSize; src++) {
                    buf[len++] = *src;
                }
                if (m_bNewLine) {
                    lineEnd += 3;
                }
                for (const char* src = lineEnd; *src && len + 1 < bufSize; src++) {
                    buf[len++] = *src;
                }
                buf[len] = '\0';
                cur = buf;
                tag = '\0';
            }

            m_bNewLine = false;
            drawY = (m_Scale.y * 32.0f * 0.5f + (m_Scale.y + m_Scale.y)) + drawY;
            lineWidth   = (!m_bFontCentreAlign && !m_bFontRightAlign) ? x : 0.0f;
            lineStart   = cur;
            spaceCount  = 0;
            lastWordEnd = 0.0f;
            isFirstWord = true;
        } else {
            // Word fits into the line
            lineWidth = wordWidth;
            while (*cur != ' ' && *cur != '\0' && *cur != '~') {
                cur++;
            }

            if (*cur == '\0') {
                // End of the text => flush the last line
                float drawX;
                if (m_bFontCentreAlign) {
                    drawX = x - lineWidth * 0.5f;
                } else if (m_bFontRightAlign) {
                    drawX = x - lineWidth;
                } else {
                    drawX = x;
                }

                lineCount++;
                if (print) {
                    RenderString(drawX, drawY, lineStart, cur, 0.0f);
                }
            } else {
                if (!isFirstWord) {
                    spaceCount++;
                }
                if (*cur != '~') {
                    lineWidth = GetCharacterSize(0) + lineWidth;
                    cur++;
                }
                lastWordEnd = lineWidth;
                isFirstWord = false;
            }
        }

        if (PS2Symbol != EXSYMBOL_NONE) {
            PS2Symbol = EXSYMBOL_NONE;
        }
    }

    if (print) {
        SetColor(savedColor);
    }

    return (int16)lineCount;
}

// 0x71A5E0
int16 CFont::GetNumberLines(float x, float y, const GxtChar* text) {
    return ProcessCurrentString(false, x, y, text);
}

// 0x71A600
int16 CFont::ProcessStringToDisplay(float x, float y, const GxtChar* text) {
    return ProcessCurrentString(true, x, y, text);
}

// 0x71A620
void CFont::GetTextRect(CRect* rect, float x, float y, const GxtChar* text) {
    if (m_bFontCentreAlign) {
        rect->left = x - (m_fFontCentreSize / 2.0f + 4.0f);
        rect->right = m_fFontCentreSize / 2.0f + x + 4.0f;
    }
    else if (m_bFontRightAlign) {
        rect->left = m_fRightJustifyWrap - 4.0f;
        rect->right = x;
    }
    else {
        rect->left = x - 4.0f;
        rect->right = m_fWrapx + 4.0f;
    }

    rect->top = y - 4.0f;
    rect->bottom = y + 4.0f + GetHeight() * (float)GetNumberLines(x, y, text);
}

// 0x71A700
void CFont::PrintString(float x, float y, const GxtChar* text) {
    if (*text == '\0' || *text == '*')
        return;

    if (m_bFontBackground) {
        CRect rt;

        RenderState.m_color = m_Color;
        GetTextRect(&rt, x, y, text);

        if (m_bEnlargeBackgroundBox) {
            rt.left -= 1.0f;
            rt.right += 1.0f;
            rt.bottom += 1.0f;
            rt.top -= 1.0f;

            FrontEndMenuManager.DrawWindow(rt, nullptr, 0, m_FontBackgroundColor, false, true);
        } else {
            CSprite2d::DrawRect(rt, m_FontBackgroundColor);
        }
        m_bFontBackground = false;
    }

    ProcessStringToDisplay(x, y, text);
}

// 0x71A820
void CFont::PrintStringFromBottom(float x, float y, const GxtChar* text) {
    float drawY = y - GetHeight() * (float)GetNumberLines(x, y, text);

    if (m_fSlant != 0.0f)
        drawY -= (m_fSlantRefPoint.x - x) * m_fSlant + m_fSlantRefPoint.y;

    PrintString(x, drawY, text);
}

// 0x719750
float CFont::GetCharacterSize(uint8 letterId) {
    uint8 propValueIdx = letterId;

    if (letterId == '?') {
        letterId = 0;
        propValueIdx = 0;
    }

    if (m_FontStyle)
        propValueIdx = FindSubFontCharacter(letterId, m_FontStyle);
    else if (propValueIdx == 145)
        propValueIdx = '@';
    else if (propValueIdx > 155)
        propValueIdx = 0;

    if (m_bFontPropOn) {
        return ((float)gFontData[m_FontTextureId].m_propValues[propValueIdx] + (float)m_nFontOutlineSize) * m_Scale.x;
    } else {
        return ((float)gFontData[m_FontTextureId].m_unpropValue + (float)m_nFontOutlineSize) * m_Scale.x;
    }
}

// Android
float CFont::GetHeight(bool a1) {
    assert(a1 == false && "NOT IMPLEMENTED");
    const float y = a1 ? 0.0f : m_Scale.y;
    return y * 32.0f / 2.0f + y + y;
}

// 0x719670, original name unknown
float GetScriptLetterSize(uint8 letterId) {
    return plugin::CallAndReturn<float, 0x719670, uint8>(letterId);
}

// 0x7192C0
uint8 CFont::FindSubFontCharacter(uint8 letterId, uint8 fontStyle) {
    if (fontStyle == 1) { // eFontStyle::FONT_PRICEDOWN
        switch (letterId) {
        case 1:  return 208;
        case 4:  return 93;
        case 7:  return 206;
        case 8:
        case 9:  return letterId + 86;
        case 14: return 207;
        case 26: return 154;
        }
    }

    if (letterId == 6)                      return 10;
    if (letterId >=  16 && letterId <=  25) return letterId - 128;
    if (letterId == 31)                     return 91;
    if (letterId >=  33 && letterId <=  58) return letterId + 122;
    if (letterId == 62)                     return 32;
    if (letterId >=  65 && letterId <=  90) return letterId + 90;
    if (letterId >=  96 && letterId <= 118) return letterId + 85;
    if (letterId >= 119 && letterId <= 140) return letterId + 62;
    if (letterId >= 141 && letterId <= 142) return 204;
    if (letterId == 143)                    return 205;

    return letterId;
}

// 0x718770
float GetLetterIdPropValue(uint8 letterId) {
    uint8 id = letterId;

    if (letterId == '?')
        id = 0;

    if (CFont::RenderState.m_bPropOn)
        return gFontData[CFont::RenderState.m_wFontTexture].m_propValues[id];
    else
        return gFontData[CFont::RenderState.m_wFontTexture].m_unpropValue;
}
