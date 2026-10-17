// ORACLE-W1 / surface: SurfaceInfos_c (the surface property table query functions, 0x55E5C0..0x55EB50) and GetSurfaceIdFromName (0x55D220).
// The table (g_surfaceInfos at 0xB79538, 0x8F4 bytes: the adhesion matrix + 179 x SurfaceInfo_c) is filled with random bits per case, so every bit-field position, every shift and every
// sign extension of the 12-byte record is exercised; both sides read the same statics. Ids 0..178 (the table has 179 records).
#include "SurfaceInfos_c.h"
#include "Weather.h"
#include "Collision/ColPoint.h"
#include <type_traits>

namespace w1s {
inline const char* const kNames[] = {
    "DEFAULT", "TARMAC", "TARMAC_FUCKED", "TARMAC_REALLYFUCKED", "PAVEMENT", "PAVEMENT_FUCKED", "GRAVEL", "FUCKED_CONCRETE", "PAINTED_GROUND",
    "GRASS_SHORT_LUSH", "GRASS_MEDIUM_LUSH", "GRASS_LONG_LUSH", "GRASS_SHORT_DRY", "GRASS_MEDIUM_DRY", "GRASS_LONG_DRY", "GOLFGRASS_ROUGH",
    "GOLFGRASS_SMOOTH", "STEEP_SLIDYGRASS", "STEEP_CLIFF", "FLOWERBED", "MEADOW", "WASTEGROUND", "WOODLANDGROUND", "VEGETATION", "MUD_WET", "MUD_DRY",
    "DIRT", "DIRTTRACK", "SAND_DEEP", "SAND_MEDIUM", "SAND_COMPACT", "SAND_ARID", "SAND_MORE", "SAND_BEACH", "CONCRETE_BEACH", "ROCK_DRY", "ROCK_WET",
    "ROCK_CLIFF", "WATER_RIVERBED", "WATER_SHALLOW", "CORNFIELD", "HEDGE", "WOOD_CRATES", "WOOD_SOLID", "WOOD_THIN", "GLASS", "GLASS_WINDOWS_LARGE",
    "GLASS_WINDOWS_SMALL", "EMPTY1", "EMPTY2", "GARAGE_DOOR", "THICK_METAL_PLATE", "SCAFFOLD_POLE", "LAMP_POST", "METAL_GATE", "METAL_CHAIN_FENCE",
    "GIRDER", "FIRE_HYDRANT", "CONTAINER", "NEWS_VENDOR", "WHEELBASE", "CARDBOARDBOX", "PED", "CAR", "CAR_PANEL", "CAR_MOVINGCOMPONENT",
    "TRANSPARENT_CLOTH", "RUBBER", "PLASTIC", "TRANSPARENT_STONE", "WOOD_BENCH", "CARPET", "FLOORBOARD", "STAIRSWOOD", "P_SAND", "P_SAND_DENSE",
    "P_SAND_ARID", "P_SAND_COMPACT", "P_SAND_ROCKY", "P_SANDBEACH", "P_GRASS_SHORT", "P_GRASS_MEADOW", "P_GRASS_DRY", "P_WOODLAND", "P_WOODDENSE",
    "P_ROADSIDE", "P_ROADSIDEDES", "P_FLOWERBED", "P_WASTEGROUND", "P_CONCRETE", "P_OFFICEDESK", "P_711SHELF1", "P_711SHELF2", "P_711SHELF3",
    "P_RESTUARANTTABLE", "P_BARTABLE", "P_UNDERWATERLUSH", "P_UNDERWATERBARREN", "P_UNDERWATERCORAL", "P_UNDERWATERDEEP", "P_RIVERBED", "P_RUBBLE",
    "P_BEDROOMFLOOR", "P_KIRCHENFLOOR", "P_LIVINGRMFLOOR", "P_CORRIDORFLOOR", "P_711FLOOR", "P_FASTFOODFLOOR", "P_SKANKYFLOOR", "P_MOUNTAIN", "P_MARSH",
    "P_BUSHY", "P_BUSHYMIX", "P_BUSHYDRY", "P_BUSHYMID", "P_GRASSWEEFLOWERS", "P_GRASSDRYTALL", "P_GRASSLUSHTALL", "P_GRASSGRNMIX", "P_GRASSBRNMIX",
    "P_GRASSLOW", "P_GRASSROCKY", "P_GRASSSMALLTREES", "P_DIRTROCKY", "P_DIRTWEEDS", "P_GRASSWEEDS", "P_RIVEREDGE", "P_POOLSIDE", "P_FORESTSTUMPS",
    "P_FORESTSTICKS", "P_FORRESTLEAVES", "P_DESERTROCKS", "P_FORRESTDRY", "P_SPARSEFLOWERS", "P_BUILDINGSITE", "P_DOCKLANDS", "P_INDUSTRIAL",
    "P_INDUSTJETTY", "P_CONCRETELITTER", "P_ALLEYRUBISH", "P_JUNKYARDPILES", "P_JUNKYARDGRND", "P_DUMP", "P_CACTUSDENSE", "P_AIRPORTGRND", "P_CORNFIELD",
    "P_GRASSLIGHT", "P_GRASSLIGHTER", "P_GRASSLIGHTER2", "P_GRASSMID1", "P_GRASSMID2", "P_GRASSDARK", "P_GRASSDARK2", "P_GRASSDIRTMIX",
    "P_RIVERBEDSTONE", "P_RIVERBEDSHALLOW", "P_RIVERBEDWEEDS", "P_SEAWEED", "DOOR", "PLASTICBARRIER", "PARKGRASS", "STAIRSSTONE", "STAIRSMETAL",
    "STAIRSCARPET", "FLOORMETAL", "FLOORCONCRETE", "BIN_BAG", "THIN_METAL_SHEET", "METAL_BARREL", "PLASTIC_CONE", "PLASTIC_DUMPSTER", "METAL_DUMPSTER",
    "WOOD_PICKET_FENCE", "WOOD_SLATTED_FENCE", "WOOD_RANCH_FENCE", "UNBREAKABLE_GLASS", "HAY_BALE", "GORE", "RAILTRACK",
};
template<class R> inline bool SameR(R a, R b) { if constexpr (std::is_floating_point_v<R>) return SameF(a, b); else return a == b; }
template<class R> inline std::string Str(R v) { if constexpr (std::is_floating_point_v<R>) return F(v); else return std::to_string((long long)v); }
inline void FillTable(Rng& r) {
    auto* b = reinterpret_cast<uint32_t*>(&g_surfaceInfos);
    for (size_t i = 0; i < sizeof(SurfaceInfos_c) / 4; ++i) {
        const int m = r.below(8);
        b[i] = m == 0 ? 0u : m == 1 ? 0xFFFFFFFFu : r.u32() * 2654435761u;
    }
    // the adhesion matrix is a float table (rubber/hard/road ...): sane values in the first 36 dwords, one case in eight keeps the random bits
    if (r.below(8)) for (int i = 0; i < 36; ++i) reinterpret_cast<float*>(b)[i] = GenF(r, 3.f);
    CWeather::WetRoads = r.below(3) ? r.f01() : GenF(r, 4.f);
}
inline uint32 PickId(Rng& r) { return (uint32)r.below(179); }   // the port indexes a std::array (Debug bounds assert): ids past the table are not compared
}

#define W1_SURF(name, va) {     Run("SurfaceInfos_c::" #name " " #va, [](Rng& r, std::string& d) {         w1s::FillTable(r); const uint32 id = w1s::PickId(r); auto* t = &g_surfaceInfos;         using R = decltype(t->name(0u));         const R a = t->name(id);         const R e = oracle::Fn<R __fastcall(SurfaceInfos_c*, int, uint32)>(va)(t, 0, id);         Hit((long long)a != 0);         if (w1s::SameR<R>(a, e)) return true;         d = "id " + std::to_string(id) + " got " + w1s::Str<R>(a) + " exe " + w1s::Str<R>(e) + " rec " + Hex(&t->m_surfaces[id < 179 ? id : 0], 12);         return false;     }); }

static void TestSurface() {
    W1_SURF(GetAdhesionGroup, 0x55E5C0) W1_SURF(GetTyreGrip, 0x55E5E0) W1_SURF(GetWetMultiplier, 0x55E600) W1_SURF(GetSkidmarkType, 0x55E630) W1_SURF(GetFrictionEffect, 0x55E650)
    W1_SURF(GetBulletFx, 0x55E670) W1_SURF(IsSoftLanding, 0x55E690) W1_SURF(IsSeeThrough, 0x55E6B0) W1_SURF(IsShootThrough, 0x55E6D0) W1_SURF(IsSand, 0x55E6F0)
    W1_SURF(IsWater, 0x55E710) W1_SURF(IsShallowWater, 0x55E730) W1_SURF(IsBeach, 0x55E750) W1_SURF(IsSteepSlope, 0x55E770) W1_SURF(IsGlass, 0x55E790)
    W1_SURF(IsStairs, 0x55E7B0) W1_SURF(IsSkateable, 0x55E7D0) W1_SURF(IsPavement, 0x55E7F0) W1_SURF(GetRoughness, 0x55E810) W1_SURF(GetFlammability, 0x55E830)
    W1_SURF(CreatesSparks, 0x55E850) W1_SURF(CantSprintOn, 0x55E870) W1_SURF(LeavesFootsteps, 0x55E890) W1_SURF(ProducesFootDust, 0x55E8B0) W1_SURF(MakesCarDirty, 0x55E8D0)
    W1_SURF(MakesCarClean, 0x55E8F0) W1_SURF(CreatesWheelGrass, 0x55E910) W1_SURF(CreatesWheelGravel, 0x55E930) W1_SURF(CreatesWheelMud, 0x55E950) W1_SURF(CreatesWheelDust, 0x55E970)
    W1_SURF(CreatesWheelSand, 0x55E990) W1_SURF(CreatesWheelSpray, 0x55E9B0) W1_SURF(CreatesPlants, 0x55E9D0) W1_SURF(CreatesObjects, 0x55E9F0) W1_SURF(CanClimb, 0x55EA10)
    W1_SURF(IsAudioConcrete, 0x55EA30) W1_SURF(IsAudioGrass, 0x55EA50) W1_SURF(IsAudioSand, 0x55EA70) W1_SURF(IsAudioGravel, 0x55EA90) W1_SURF(IsAudioWood, 0x55EAB0)
    W1_SURF(IsAudioWater, 0x55EAD0) W1_SURF(IsAudioMetal, 0x55EAF0) W1_SURF(IsAudioLongGrass, 0x55EB10) W1_SURF(IsAudioTile, 0x55EB30)

    Run("SurfaceInfos_c::GetAdhesiveLimit 0x55EB50", [](Rng& r, std::string& d) {
        w1s::FillTable(r);
        for (auto& s : g_surfaceInfos.m_surfaces) s.ucAdhesionGroup = r.below(6);   // the 6x6 matrix (a 3-bit field can say 6 / 7: the port asserts on its mdarray, the exe reads past it)
        CColPoint cp; std::memset(&cp, 0, sizeof cp);
        cp.m_nSurfaceTypeA = (eSurfaceType)w1s::PickId(r); cp.m_nSurfaceTypeB = (eSurfaceType)w1s::PickId(r);
        auto* t = &g_surfaceInfos;
        const float a = t->GetAdhesiveLimit(&cp);
        const float e = oracle::Fn<float __fastcall(SurfaceInfos_c*, int, CColPoint*)>(0x55EB50)(t, 0, &cp);
        if (SameF(a, e)) return true;
        d = "A " + std::to_string((int)cp.m_nSurfaceTypeA) + " B " + std::to_string((int)cp.m_nSurfaceTypeB) + " got " + F(a) + " exe " + F(e);
        return false;
    });

    // GetSurfaceIdFromName: every real name, mutated names (case, prefix, suffix, truncation), garbage; the exe's comparison is whatever its strcmp-style loop does
    Run("SurfaceInfos_c::GetSurfaceIdFromName 0x55D220", [](Rng& r, std::string& d) {
        constexpr int N = (int)(sizeof w1s::kNames / sizeof w1s::kNames[0]);
        std::string s = w1s::kNames[r.below(N)];
        switch (r.below(8)) {
        case 0: if (!s.empty()) s[r.below((int)s.size())] ^= 0x20; break;                   // case flip
        case 1: s += "_"; break;
        case 2: s = s.substr(0, r.below((int)s.size() + 1)); break;                          // truncation (incl. empty)
        case 3: s = "X" + s; break;
        case 4: { s.clear(); const int n = r.below(24); for (int i = 0; i < n; ++i) s += (char)('A' + r.below(26)); break; }
        case 5: { std::string t = w1s::kNames[r.below(N)]; for (auto& c : t) c = (char)std::tolower(c); s = t; break; }
        default: break;                                                                      // the exact name
        }
        auto* t = &g_surfaceInfos;
        const uint32 a = t->GetSurfaceIdFromName(s.c_str());
        const uint32 e = oracle::Fn<uint32 __fastcall(SurfaceInfos_c*, int, const char*)>(0x55D220)(t, 0, s.c_str());
        Hit(a != 0);
        if (a == e) return true;
        d = "name '" + s + "' got " + std::to_string(a) + " exe " + std::to_string(e);
        return false;
    });
}
