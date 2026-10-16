/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "RGBA.h"
#include "eFontAlignment.h"
#include <extensions/WEnum.hpp>

class CSprite2d;


// todo: move
struct CFontChar {
    uint8     m_cLetter;
    uint8     m_dLetter;
    CVector2D m_vPosn;
    float     m_fWidth;
    float     m_fHeight;
    CRGBA     m_color;
    float     m_fWrap;
    float     m_fSlant;
    CVector2D m_vSlanRefPoint;
    bool      m_bContainImages;
    uint8     m_nFontStyle;
    bool      m_bPropOn;
    uint16    m_wFontTexture;
    uint8     m_nOutline;

public:
    // 0x718E50
    void Set(const CFontChar& setup) {
        m_cLetter        = setup.m_cLetter;
        m_dLetter        = setup.m_dLetter;
        m_vPosn          = setup.m_vPosn;
        m_fWidth         = setup.m_fWidth;
        m_fHeight        = setup.m_fHeight;
        m_color          = setup.m_color;
        m_fWrap          = setup.m_fWrap;
        m_fSlant         = setup.m_fSlant;
        m_vSlanRefPoint  = setup.m_vSlanRefPoint;
        m_bContainImages = setup.m_bContainImages;
        m_nFontStyle     = setup.m_nFontStyle;
        m_bPropOn        = setup.m_bPropOn;
        m_wFontTexture   = setup.m_wFontTexture;
        m_nOutline       = setup.m_nOutline;
    }
};
VALIDATE_SIZE(CFontChar, 0x30);

struct tFontData {
    std::array<uint8, 208> m_propValues;
    uint8 m_spaceValue;
    uint8 m_unpropValue;
};

enum eExtraFontSymbol : uint8 {
    EXSYMBOL_NONE       = 0, // invalid
    EXSYMBOL_DPAD_UP    = 1,
    EXSYMBOL_DPAD_DOWN  = 2,
    EXSYMBOL_DPAD_LEFT  = 3,
    EXSYMBOL_DPAD_RIGHT = 4,
    EXSYMBOL_CROSS      = 5,
    EXSYMBOL_CIRCLE     = 6,
    EXSYMBOL_SQUARE     = 7,
    EXSYMBOL_TRIANGLE   = 8,
    EXSYMBOL_KEY        = 9, // followed by buttons, L1?
    EXSYMBOL_L2         = 10,
    EXSYMBOL_L3         = 11,
    EXSYMBOL_R1         = 12,
    EXSYMBOL_R2         = 13,
    EXSYMBOL_R3         = 14
};

enum eFontStyle : uint8 {
    FONT_GOTHIC,
    FONT_SUBTITLES,
    FONT_MENU,
    FONT_PRICEDOWN
};
NOTSA_WENUM_DEFS_FOR(eFontStyle);

class CFont {
public:
    // static variables
    static constexpr size_t MAX_FONT_SPRITES = 2;
    static constexpr size_t MAX_FONT_BUTTON_SPRITES = 15;

    static inline NOTSA_GLOBAL(RenderState, 0xC71AA0, (CFontChar), {});
    // font textures array
    static inline auto& Sprite = StaticRef<CSprite2d[MAX_FONT_SPRITES]>(0xC71AD0);
    // button textures array
    static inline auto& ButtonSprite = StaticRef<CSprite2d[MAX_FONT_BUTTON_SPRITES]>(0xC71AD8);
    static inline NOTSA_GLOBAL(PS2Symbol, 0xC71A54, (eExtraFontSymbol), {});
    static inline NOTSA_GLOBAL(m_bNewLine, 0xC71A55, (bool), {});
    static inline NOTSA_GLOBAL(m_Color, 0xC71A60, (CRGBA), {});
    static inline NOTSA_GLOBAL(m_Scale, 0xC71A64, (CVector2D), {});
    static inline NOTSA_GLOBAL(m_fSlant, 0xC71A6C, (float), {});
    static inline NOTSA_GLOBAL(m_fSlantRefPoint, 0xC71A70, (CVector2D), {});
    static inline NOTSA_GLOBAL(m_bFontJustify, 0xC71A78, (bool), {});
    static inline NOTSA_GLOBAL(m_bFontCentreAlign, 0xC71A79, (bool), {});
    static inline NOTSA_GLOBAL(m_bFontRightAlign, 0xC71A7A, (bool), {});
    static inline NOTSA_GLOBAL(m_bFontBackground, 0xC71A7B, (bool), {});
    static inline NOTSA_GLOBAL(m_bEnlargeBackgroundBox, 0xC71A7C, (bool), {});
    static inline NOTSA_GLOBAL(m_bFontPropOn, 0xC71A7D, (bool), {});
    static inline NOTSA_GLOBAL(m_bFontIsBlip, 0xC71A7E, (bool), {});
    static inline NOTSA_GLOBAL(m_fFontAlpha, 0xC71A80, (float), {});
    static inline NOTSA_GLOBAL(m_FontBackgroundColor, 0xC71A84, (CRGBA), {});
    static inline NOTSA_GLOBAL(m_fWrapx, 0xC71A88, (float), {});
    static inline NOTSA_GLOBAL(m_fFontCentreSize, 0xC71A8C, (float), {});
    static inline NOTSA_GLOBAL(m_fRightJustifyWrap, 0xC71A90, (float), {});
    static inline NOTSA_GLOBAL(m_FontTextureId, 0xC71A94, (uint8), {});
    static inline NOTSA_GLOBAL(m_FontStyle, 0xC71A95, (uint8), {});
    static inline NOTSA_GLOBAL(m_nFontShadow, 0xC71A96, (uint8), {});
    static inline NOTSA_GLOBAL(m_FontDropColor, 0xC71A97, (CRGBA), {});
    static inline NOTSA_GLOBAL(m_nFontOutlineSize, 0xC71A9B, (uint8), {});
    static inline NOTSA_GLOBAL(m_nFontOutline, 0xC71A9C, (uint8), {});
    static inline NOTSA_GLOBAL_ALIAS(m_nFontOutlineOrShadow, 0xC71A9C, (uint8), CFont::m_nFontOutline);

public:
    static void InjectHooks();

    static void Initialise();
    static void LoadFontValues();
    static void Shutdown();
    static void PrintChar(float x, float y, char character);
    // Get next ' ' character in a string
    static char* GetNextSpace(char* string);
    static char* ParseToken(char* text, CRGBA& color, bool isBlip, char* tag);
    static void FilterOutTokensFromString(GxtChar* str);
    static void RenderString(float x, float y, const char* text, const char* textEnd, float wrap);
    static void SetScale(float w, float h);
    static void SetScaleForCurrentLanguage(float w, float h);
    static void SetSlantRefPoint(float x, float y);
    static void SetSlant(float value);
    static void SetColor(CRGBA color);
    static void SetFontStyle(eFontStyle style);
    static void SetWrapx(float value);
    static void SetCentreSize(float value);
    static void SetRightJustifyWrap(float value);
    static void SetAlphaFade(float alpha);
    static void SetDropColor(CRGBA color);
    static void SetDropShadowPosition(int16 value);
    static void SetEdge(int8 value);
    static void SetProportional(bool on);
    static void SetBackground(bool enable, bool includeWrap);
    static void SetBackgroundColor(CRGBA color);
    static void SetJustify(bool on);
    static void SetOrientation(eFontAlignment alignment);
    static void InitPerFrame();
    static void RenderFontBuffer();
    static float GetHeight(bool a1 = false);
    static float GetStringWidth(const GxtChar* string, bool full, bool scriptText);
    static void DrawFonts();
    static int16 ProcessCurrentString(bool print, float x, float y, const GxtChar* text);
    static int16 GetNumberLines(float x, float y, const GxtChar* text);
    static int16 ProcessStringToDisplay(float x, float y, const GxtChar* text);
    static void GetTextRect(CRect* rect, float x, float y, const GxtChar* text);
    static void PrintString(float x, float y, const GxtChar* text);
    static void PrintStringFromBottom(float x, float y, const GxtChar* text);
    static float GetCharacterSize(uint8 ch);
    static uint8 FindSubFontCharacter(uint8 letterId, uint8 fontStyle);
};

static void ReadFontsDat();
static float GetScriptLetterSize(uint8 letterId);
static float GetLetterIdPropValue(uint8 letterId);
