/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "Vector.h"
#include "RGBA.h"
#include "Vector2D.h"
#include "FileRecord.hpp"
#include "FileRecords/RadarFile.hpp"

class CSprite2d;

// Thanks to Wesser for radar-related things
enum eBlipAppearance : uint8 {
    BLIP_FLAG_FRIEND, // It selects BLIP_COLOUR_BLUE. If unset together with BLIP_FLAG_THREAT, any color.
    BLIP_FLAG_THREAT,  // It selects BLIP_COLOUR_RED. If unset together with BLIP_FLAG_FRIEND, any color.
    BLIP_FLAG_UNK,

    BLIP_FLAG_NUM // Add above this
};

enum eBlipType : uint8 {
    BLIP_NONE,          // 0
    BLIP_CAR,           // 1
    BLIP_CHAR,          // 2
    BLIP_OBJECT,        // 3
    BLIP_COORD,         // 4 - Checkpoint.
    BLIP_CONTACT_POINT, // 5 - Sphere.
    BLIP_SPOTLIGHT,     // 6
    BLIP_PICKUP,        // 7
    BLIP_AIRSTRIP       // 8
};

enum eBlipDisplay : uint8 {
    BLIP_DISPLAY_NEITHER,    // 0
    BLIP_DISPLAY_MARKERONLY, // 1
    BLIP_DISPLAY_BLIPONLY,   // 2
    BLIP_DISPLAY_BOTH        // 3
};

// See <https://www.dropbox.com/s/oi3i4f0qsbe7z10/blip_marker_colors.html> to view these colors. (TODO: dead link, fix it.)
enum eBlipColour : uint32 {
    BLIP_COLOUR_RED,        // 0
    BLIP_COLOUR_GREEN,      // 1
    BLIP_COLOUR_BLUE,       // 2
    BLIP_COLOUR_WHITE,      // 3
    BLIP_COLOUR_YELLOW,     // 4
    BLIP_COLOUR_REDCOPY,    // 5 - What? It was BLIP_COLOUR_PURPLE.
    BLIP_COLOUR_BLUECOPY,   // 6 - Why? It was BLIP_COLOUR_CYAN.
    BLIP_COLOUR_THREAT,     // 7 - If BLIP_FLAG_FRIENDLY is not set (by default) it is BLIP_COLOUR_RED, else BLIP_COLOUR_BLUE.
    BLIP_COLOUR_DESTINATION // 8 - Default color.
};

// https://wiki.multitheftauto.com/index.php?title=Radar_Blips
enum eRadarSprite : int8 {
    RADAR_SPRITE_PLAYER_INTEREST = -5,
    RADAR_SPRITE_THREAT          = -4,
    RADAR_SPRITE_FRIEND          = -3,
    RADAR_SPRITE_OBJECT          = -2,
    RADAR_SPRITE_DESTINATION     = -1,
    RADAR_SPRITE_NONE            = 0,
    RADAR_SPRITE_WHITE,         // 1
    RADAR_SPRITE_CENTRE,        // 2
    RADAR_SPRITE_MAP_HERE,      // 3
    RADAR_SPRITE_NORTH,         // 4
    RADAR_SPRITE_AIRYARD,       // 5
    RADAR_SPRITE_AMMUGUN,       // 6
    RADAR_SPRITE_BARBERS,       // 7
    RADAR_SPRITE_BIGSMOKE,      // 8
    RADAR_SPRITE_BOATYARD,      // 9
    RADAR_SPRITE_BURGERSHOT,    // 10
    RADAR_SPRITE_BULLDOZER,     // 11
    RADAR_SPRITE_CATALINAPINK,  // 12
    RADAR_SPRITE_CESARVIAPANDO, // 13 - What? R* mistype?
    RADAR_SPRITE_CHICKEN,       // 14
    RADAR_SPRITE_CJ,            // 15
    RADAR_SPRITE_CRASH1,        // 16
    RADAR_SPRITE_DINER,         // 17
    RADAR_SPRITE_EMMETGUN,      // 18
    RADAR_SPRITE_ENEMYATTACK,   // 19
    RADAR_SPRITE_FIRE,          // 20
    RADAR_SPRITE_GIRLFRIEND,    // 21
    RADAR_SPRITE_HOSTPITAL,     // 22 - Again?
    RADAR_SPRITE_LOGOSYNDICATE, // 23
    RADAR_SPRITE_MADDOG,        // 24
    RADAR_SPRITE_MAFIACASINO,   // 25
    RADAR_SPRITE_MCSTRAP,       // 26
    RADAR_SPRITE_MODGARAGE,     // 27
    RADAR_SPRITE_OGLOC,         // 28
    RADAR_SPRITE_PIZZA,         // 29
    RADAR_SPRITE_POLICE,        // 30
    RADAR_SPRITE_PROPERTYG,     // 31
    RADAR_SPRITE_PROPERTYR,     // 32
    RADAR_SPRITE_RACE,          // 33
    RADAR_SPRITE_RYDER,         // 34
    RADAR_SPRITE_SAVEGAME,      // 35
    RADAR_SPRITE_SCHOOL,        // 36
    RADAR_SPRITE_QMARK,         // 37
    RADAR_SPRITE_SWEET,         // 38
    RADAR_SPRITE_TATTOO,        // 39
    RADAR_SPRITE_THETRUTH,      // 40
    RADAR_SPRITE_WAYPOINT,      // 41
    RADAR_SPRITE_TORENORANCH,   // 42
    RADAR_SPRITE_TRIADS,        // 43
    RADAR_SPRITE_TRIADSCASINO,  // 44
    RADAR_SPRITE_TSHIRT,        // 45
    RADAR_SPRITE_WOOZIE,        // 46
    RADAR_SPRITE_ZERO,          // 47
    RADAR_SPRITE_DATEDISCO,     // 48
    RADAR_SPRITE_DATEDRINK,     // 49
    RADAR_SPRITE_DATEFOOD,      // 50
    RADAR_SPRITE_TRUCK,         // 51
    RADAR_SPRITE_CASH,          // 52
    RADAR_SPRITE_FLAG,          // 53
    RADAR_SPRITE_GYM,           // 54
    RADAR_SPRITE_IMPOUND,       // 55
    RADAR_SPRITE_LIGHT,         // 56
    RADAR_SPRITE_RUNWAY,        // 57
    RADAR_SPRITE_GANGB,         // 58
    RADAR_SPRITE_GANGP,         // 59
    RADAR_SPRITE_GANGY,         // 60
    RADAR_SPRITE_GANGN,         // 61
    RADAR_SPRITE_GANGG,         // 62
    RADAR_SPRITE_SPRAY,         // 63
    RADAR_SPRITE_TORENO         // 64
};

enum eRadarTraceHeight : uint8 {
    RADAR_TRACE_LOW,   // 0 Up-pointing Triangle △
    RADAR_TRACE_HIGH,  // 1 Down-pointing Triangle ▽
    RADAR_TRACE_NORMAL // 2 Box □
};

/*
struct tBlipHandle {
    uint16 arrayIndex;
    uint16 number;
};
VALIDATE_SIZE(tBlipHandle, 0x4);
*/
using tBlipHandle = uint32; // TODO: Use struct above

struct airstrip_info {
    CVector2D position;
    float     direction; // angle
    float     radius; // not sure
};
VALIDATE_SIZE(airstrip_info, 0x10);

// NOTSA
enum eAirstripLocation : uint8 {
    AIRSTRIP_LS_AIRPORT,
    AIRSTRIP_SF_AIRPORT,
    AIRSTRIP_LV_AIRPORT,
    AIRSTRIP_VERDANT_MEADOWS,

    NUM_AIRSTRIPS
};

class CEntryExit;

struct tRadarTrace {
    eBlipColour  m_nColour;
    uint32       m_nEntityHandle;
    CVector      m_vPosition;
    uint16       m_nCounter;
    float        m_fSphereRadius;
    uint16       m_nBlipSize;

    union {
        CEntryExit* m_pEntryExit;       // Used for normal usage
        uint32      m_EntryExitPoolInd; // Used when saving/loading to save file
    };
    eRadarSprite m_nBlipSprite;

    bool         m_bBright : 1;              // It makes use of bright colors. Always set.
    bool         m_bTrackingBlip : 1;        // It is available.
    bool         m_bShortRange : 1;          // It doesn't show permanently on the radar.
    bool         m_bFriendly : 1;            // It is affected by BLIP_COLOUR_THREAT.
    bool         m_bBlipRemain : 1;          // It has the priority over the entity (it will still appear after the entity's deletion).
    bool         m_bBlipFade : 1;            // Possibly a leftover. Always unset (unused).
    uint8        m_nCoordBlipAppearance : 2; // see eBlipAppearance

    eBlipDisplay    m_nBlipDisplayFlag : 2;
    eBlipType       m_nBlipType : 4;
    eBlipAppearance m_nAppearance : 2;

    [[nodiscard]] auto HasSprite() const { return m_nBlipSprite != eRadarSprite::RADAR_SPRITE_NONE; }
    [[nodiscard]] CRGBA GetStaticColour() const;
    [[nodiscard]] CVector GetWorldPos() const;

    std::pair<CVector2D, CVector2D> GetRadarAndScreenPos(float* radarPointDist) const;
};
VALIDATE_SIZE(tRadarTrace, 0x28); // x86 in-memory (the pointer in the union widens it to 0x30 on LP64); the on-disk 0x28 is VALIDATE_SIZE_FILE(tRadarTrace_File) in FileRecords/RadarFile.hpp

// P-2f: file layout (0x28) of a radar trace (CRadar::Save / Load). The CEntryExit link is a pool index+1 that only CRadar can compute/resolve, so the 1-argument ToFile writes
// "no enex" (0) and FromFile leaves m_pEntryExit null; CRadar::Save/Load use ToFile(trace, poolIndexPlusOne) and read `entryExitPoolInd` from the record themselves.
// Bit-fields are packed byte-wise (no memcpy of the bit-field storage).
template<> struct notsa::FileRecord<tRadarTrace> {
    using Type = tRadarTrace_File;
    static Type ToFile(const tRadarTrace& t, uint32 entryExitPoolIndPlusOne) {
        Type f{};
        f.colour           = (uint32)t.m_nColour;
        f.entityHandle     = t.m_nEntityHandle;
        f.pos[0] = t.m_vPosition.x; f.pos[1] = t.m_vPosition.y; f.pos[2] = t.m_vPosition.z;
        f.counter          = t.m_nCounter;
        f.sphereRadius     = t.m_fSphereRadius;
        f.blipSize         = t.m_nBlipSize;
        f.entryExitPoolInd = entryExitPoolIndPlusOne;
        f.blipSprite       = (int8)t.m_nBlipSprite;
        f.flags0           = uint8((t.m_bBright ? kRadarFlagBright : 0) | (t.m_bTrackingBlip ? kRadarFlagTracking : 0) | (t.m_bShortRange ? kRadarFlagShortRange : 0)
                                 | (t.m_bFriendly ? kRadarFlagFriendly : 0) | (t.m_bBlipRemain ? kRadarFlagBlipRemain : 0) | (t.m_bBlipFade ? kRadarFlagBlipFade : 0)
                                 | ((t.m_nCoordBlipAppearance & kRadarCoordAppearanceMask) << kRadarCoordAppearanceShift));
        f.flags1           = uint8(((((unsigned)t.m_nBlipDisplayFlag) & kRadarDisplayMask) << kRadarDisplayShift) | ((((unsigned)t.m_nBlipType) & kRadarTypeMask) << kRadarTypeShift)
                                 | ((((unsigned)t.m_nAppearance) & kRadarAppearanceMask) << kRadarAppearanceShift));
        return f;
    }
    static Type ToFile(const tRadarTrace& t) { return ToFile(t, 0u); }
    static void FromFile(tRadarTrace& t, const Type& f) {
        t.m_nColour       = (eBlipColour)f.colour;
        t.m_nEntityHandle = f.entityHandle;
        t.m_vPosition     = CVector{ f.pos[0], f.pos[1], f.pos[2] };
        t.m_nCounter      = f.counter;
        t.m_fSphereRadius = f.sphereRadius;
        t.m_nBlipSize     = f.blipSize;
        t.m_pEntryExit    = nullptr; // resolved by CRadar::Load from f.entryExitPoolInd
        t.m_nBlipSprite   = (eRadarSprite)f.blipSprite;
        t.m_bBright       = (f.flags0 & kRadarFlagBright) != 0;
        t.m_bTrackingBlip = (f.flags0 & kRadarFlagTracking) != 0;
        t.m_bShortRange   = (f.flags0 & kRadarFlagShortRange) != 0;
        t.m_bFriendly     = (f.flags0 & kRadarFlagFriendly) != 0;
        t.m_bBlipRemain   = (f.flags0 & kRadarFlagBlipRemain) != 0;
        t.m_bBlipFade     = (f.flags0 & kRadarFlagBlipFade) != 0;
        t.m_nCoordBlipAppearance = (f.flags0 >> kRadarCoordAppearanceShift) & kRadarCoordAppearanceMask;
        t.m_nBlipDisplayFlag = (eBlipDisplay)((f.flags1 >> kRadarDisplayShift) & kRadarDisplayMask);
        t.m_nBlipType        = (eBlipType)((f.flags1 >> kRadarTypeShift) & kRadarTypeMask);
        t.m_nAppearance      = (eBlipAppearance)((f.flags1 >> kRadarAppearanceShift) & kRadarAppearanceMask);
    }
    // x86: every non-bit-field member of the raw object sits where the record has it (the bit-field bytes follow the sprite byte; layout_diff.py proves the bit positions)
    static_assert(sizeof(void*) != 4 || (sizeof(tRadarTrace) == sizeof(Type) && offsetof(tRadarTrace, m_nColour) == offsetof(Type, colour) && offsetof(tRadarTrace, m_nEntityHandle) == offsetof(Type, entityHandle)
        && offsetof(tRadarTrace, m_vPosition) == offsetof(Type, pos) && offsetof(tRadarTrace, m_nCounter) == offsetof(Type, counter) && offsetof(tRadarTrace, m_fSphereRadius) == offsetof(Type, sphereRadius)
        && offsetof(tRadarTrace, m_nBlipSize) == offsetof(Type, blipSize) && offsetof(tRadarTrace, m_EntryExitPoolInd) == offsetof(Type, entryExitPoolInd)
        && offsetof(tRadarTrace, m_nBlipSprite) == offsetof(Type, blipSprite)));
};

static constexpr uint32 MAX_RADAR_WIDTH_TILES = 12;
static constexpr uint32 MAX_RADAR_HEIGHT_TILES = 12;

class CRadar {
public:
    static constexpr uint32 MAX_RADAR_SPRITES = 64;
    static constexpr uint32 MAX_RADAR_TRACES = 175;

    static inline NOTSA_GLOBAL(m_fRadarOrientation, 0xBA8310, (float), {});
    static inline NOTSA_GLOBAL(cachedCos, 0xBA8308, (float), {});
    static inline NOTSA_GLOBAL(cachedSin, 0xBA830C, (float), {});

    static SpriteFileName RadarBlipFileNames[];

    static inline NOTSA_GLOBAL(m_radarRange, 0xBA8314, (float), {}); // 2990.0 by default
    static inline NOTSA_GLOBAL(MapLegendList, 0xBA8318, (std::array<int16, MAX_RADAR_TRACES>), {}); // do not convert int16 to eRadarSprite, they have different sizes.
    static inline NOTSA_GLOBAL(MapLegendCounter, 0xBA86B8, (uint16), {}); // num icons in legend
    static inline NOTSA_GLOBAL(ArrowBlipColour, 0xBA86D4, (std::array<CRGBA, 6>), {}); static_assert(sizeof(ArrowBlipColour) == 24);
    static inline NOTSA_GLOBAL(ms_RadarTrace, 0xBA86F0, (std::array<tRadarTrace, MAX_RADAR_TRACES>), {});
    static inline NOTSA_GLOBAL(vec2DRadarOrigin, 0xBAA248, (CVector2D), {});
    static inline NOTSA_GLOBAL(RadarBlipSprites, 0xBAA250, (std::array<CSprite2d, MAX_RADAR_SPRITES>), {}); VALIDATE_SIZE(RadarBlipSprites, 256);
    static inline NOTSA_GLOBAL(m_radarRect, 0x8D0920, (CRect), {}); // { 1000000.0f, -1000000.0f, -1000000.0f, 1000000.0f }

    static inline NOTSA_GLOBAL(airstrip_location, 0xBA8300, (eAirstripLocation), {}); // current airstrip index in airstrip_table
    static inline NOTSA_GLOBAL(airstrip_blip, 0xBA8304, (int32), {});     // blip handle

public:
    static void InjectHooks();

    static void Initialise();
    static void Shutdown();

    static void LoadTextures();
    
    static tBlipHandle GetNewUniqueBlipIndex(int32 blipIndex);
    static int32 GetActualBlipArrayIndex(tBlipHandle blip);

    static void DrawLegend(int32 x, int32 y, eRadarSprite blipType);
    static float LimitRadarPoint(CVector2D& point);
    static void LimitToMap(float& x, float& y);
    static uint8 CalculateBlipAlpha(float distance);
    static CVector2D TransformRadarPointToScreenSpace(const CVector2D& in);
    static CVector2D TransformRealWorldPointToRadarSpace(const CVector2D& in);
    static CVector2D TransformRadarPointToRealWorldSpace(const CVector2D& in);
    static CVector2D TransformRealWorldToTexCoordSpace(const CVector2D& in, int32 x, int32 y);
    static void CalculateCachedSinCos();
    static tBlipHandle SetCoordBlip(eBlipType type, CVector posn, eBlipColour color, eBlipDisplay blipDisplay, const char* scriptName = nullptr);
    static tBlipHandle SetShortRangeCoordBlip(eBlipType type, CVector posn, eBlipColour color, eBlipDisplay blipDisplay, const char* scriptName = nullptr);
    static tBlipHandle SetEntityBlip(eBlipType type, int32 entityHandle, uint32 arg2, eBlipDisplay blipDisplay);
    static void ChangeBlipColour(tBlipHandle blip, eBlipColour color);
    static bool HasThisBlipBeenRevealed(int32 blipIndex);
    static bool DisplayThisBlip(eRadarSprite spriteId, int8 priority);
    static void ChangeBlipBrightness(tBlipHandle blip, int32 brightness);
    static void ChangeBlipScale(tBlipHandle blip, int32 size);
    static void ChangeBlipDisplay(tBlipHandle blip, eBlipDisplay blipDisplay);
    static void SetBlipSprite(tBlipHandle blip, eRadarSprite spriteId);
    static void SetBlipAlwaysDisplayInZoom(tBlipHandle blip, bool display);
    static void SetBlipFade(tBlipHandle blip, bool fade);
    static void SetCoordBlipAppearance(tBlipHandle blip, eBlipAppearance appearance);
    static void SetBlipFriendly(tBlipHandle blip, bool friendly);
    static void SetBlipEntryExit(tBlipHandle blip, CEntryExit* enex);
    static void ShowRadarTrace(float x, float y, uint32 size, CRGBA color);
    static void ShowRadarTraceWithHeight(float x, float y, uint32 size, uint32 r, uint32 g, uint32 b, uint32 a, eRadarTraceHeight height);
    static void ShowRadarMarker(CVector posn, uint32 color, float radius);
    static uint32 GetRadarTraceColour(eBlipColour color, bool bright, bool friendly);
    static void DrawRotatingRadarSprite(CSprite2d& sprite, float x, float y, float angle, uint32 width, uint32 height, CRGBA color);
    static void DrawYouAreHereSprite(float x, float y);
    static void SetupRadarRect(int32 x, int32 y);
    static void RequestMapSection(int32 x, int32 y);
    static void RemoveMapSection(int32 x, int32 y);
    static void RemoveRadarSections();
    static void StreamRadarSections(const CVector& worldPosn);
    static void StreamRadarSections(int32 x, int32 y);
    static int32 ClipRadarPoly(CVector2D* out, const CVector2D* in);
    static void DrawAreaOnRadar(const CRect& rect, const CRGBA& color, bool inMenu);
    static void DrawRadarMask();
    static void InitFrontEndMap();
    static void AddBlipToLegendList(bool noSprite, int32 blipIndex);
    static void SetMapCentreToPlayerCoords();
    static void Draw3dMarkers();
    static void SetRadarMarkerState(int32 counter, bool flag);
    static void DrawRadarSprite(eRadarSprite spriteId, float x, float y, uint8 alpha);
    static void DrawRadarSection(int32 x, int32 y);
    static void DrawRadarSectionMap(int32 x, int32 y, CRect rect);
    static void DrawRadarGangOverlay(bool inMenu);
    static void DrawRadarMap();
    static void DrawMap();

    static void DrawCoordBlip(int32 blipIndex, bool isSprite);
    static void DrawEntityBlip(int32 blipIndex, uint8 arg1);
    static void ClearActualBlip(int32 blipIndex);
    static void ClearActualBlip(tRadarTrace& trace);
    static void ClearBlipForEntity(eBlipType blipType, int32 entityHandle);
    static void ClearBlipForEntity(CPed* ped);
    static void ClearBlip(tBlipHandle blip);
    static void SetupAirstripBlips();
    static void DrawBlips();

    static bool Load();
    static bool Save();

    // NOTSA
    static const GxtChar* GetBlipName(eRadarSprite sprite);
    static int32 FindTraceNotTrackingBlipIndex(); // Return the index of the first trace with the `TrackingBlip` flag NOT set

    static bool IsMapSectionInBounds(int32 x, int32 y) {
        return x >= 0 && x < MAX_RADAR_WIDTH_TILES && y >= 0 && y < MAX_RADAR_HEIGHT_TILES;
    }

    static auto CachedRotateCounterclockwise(const CVector2D& point) {
        return CVector2D{
            cachedCos * point.x - cachedSin * point.y,
            cachedSin * point.x + cachedCos * point.y
        };
    }

    static auto CachedRotateClockwise(const CVector2D& point) {
        return CVector2D{
            +cachedCos * point.x + cachedSin * point.y,
            -cachedSin * point.x + cachedCos * point.y
        };
    }
};

bool ClipRadarTileCoords(int32& x, int32& y);
bool IsPointInsideRadar(const CVector2D& point);
void GetTextureCorners(int32 x, int32 y, CVector2D* corners);
int32 LineRadarBoxCollision(CVector2D& result, const CVector2D& lineStart, const CVector2D& lineEnd);
