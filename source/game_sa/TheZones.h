/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "Vector.h"
#include "eLevelName.h"
#include "Zone.h"
#include "ZoneInfo.h"

class CTheZones {
public:
    static inline NOTSA_GLOBAL(ZonesVisited, 0xBA3730, (notsa::mdarray<bool, 10, 10>), {}); // Explored territories. Count: 100

    static inline NOTSA_GLOBAL(m_CurrLevel, 0xBA6718, (eLevelName), {});
    static inline NOTSA_GLOBAL(ZonesRevealed, 0xBA372C, (int32), {}); // Number of explored territories
    static inline NOTSA_GLOBAL(TotalNumberOfNavigationZones, 0xBA3794, (int16), {}); // Info zones
    static inline NOTSA_GLOBAL(NavigationZoneArray, 0xBA3798, (std::array<CZone, 380>), {});
    static inline NOTSA_GLOBAL(TotalNumberOfMapZones, 0xBA1900, (int16), {}); // Map zones
    static inline NOTSA_GLOBAL(MapZoneArray, 0xBA1908, (std::array<CZone, 39>), {});
    static inline NOTSA_GLOBAL(TotalNumberOfZoneInfos, 0xBA1DE8, (int16), {});
    
    NOTSA_GLOBAL_ZERO(ZoneInfoArray, 0xBA1DF0, (std::array<CZoneInfo, 380>)); static_assert(sizeof(ZoneInfoArray) == 6460); // zero .bss in the exe (not the repo default ctor values)

public:
    static void InjectHooks();

    static void InitZonesPopulationSettings();
    static void ResetZonesRevealed();
    static void AssignZoneInfoForThisZone(int16 index);
    static bool ZoneIsEntirelyContainedWithinOtherZone(CZone* zone1, CZone* zone2);
    static bool& GetZoneWasVisited(CVector2D pos); // NOTSA
    static bool SetZoneWasVisited(CVector2D pos, bool locked); // NOTSA
    static bool GetCurrentZoneLockedOrUnlocked(CVector2D pos);
    // Returns true if point lies within zone
    static bool PointLiesWithinZone(const CVector* point, CZone* zone);
    // Returns eLevelName from position
    static eLevelName GetLevelFromPosition(const CVector& point);
    // Returns pointer to zone by a point
    static CZone* FindSmallestZoneForPosition(const CVector& point, bool FindOnlyZonesType0);
    static CZoneInfo* GetZoneInfo(const CVector& point, CZone** outZone);
    static void FillZonesWithGangColours(bool disableRadarGangColors);
    // Returns pointer to zone by index
    static CZone* GetNavigationZone(uint16 index);
    // Returns pointer to zone by index
    static CZone* GetMapZone(uint16 index);
    static float Calc2DDistanceBetween2Zones(CZone* zone1, CZone* zone2);

    static void Init();
    static void SetCurrentZoneAsUnlocked();
    static void CreateZone(const char* name, eZoneType type, CVector pos1, CVector pos2, eLevelName level, const char* GXT_key);

    static bool FindZone(CVector* point, uint64_t zoneName, eZoneType type);
    static bool FindZone(const CVector& point, std::string_view name, eZoneType type);
    static int16 FindZoneByLabel(const char* name, eZoneType type);
    static void SetZoneRadarColours(int16 index, char flag, uint8 red, uint8 green, uint8 blue);

    static void Update();
    static void Save();
    static void Load();
    static void PostZoneCreation();

    // NOTSA
    static const GxtChar* GetZoneName(const CVector& point);



    [[deprecated]] static auto GetNaviZones() { return NavigationZoneArray | rngv::take(TotalNumberOfNavigationZones); }

    static auto GetNavigationZones() { return NavigationZoneArray | rngv::take(TotalNumberOfNavigationZones); }

    static auto GetMapZones() { return MapZoneArray | rngv::take(TotalNumberOfMapZones); }

    static auto GetZoneInfos() { return ZoneInfoArray | rngv::take(TotalNumberOfZoneInfos); }

    static CZoneInfo* GetZoneInfo(const CZone* zone) {
        auto idx = zone->m_ZoneInfoIndex;

        if (!idx)
            return nullptr;

        return &GetZoneInfos()[idx];
    }
};
