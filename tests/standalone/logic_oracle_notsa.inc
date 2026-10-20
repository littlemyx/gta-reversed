// (h) NOTSA-AUDIT (.notes/reports/NOTSA-AUDIT.md): exe oracle rows for the functions whose run-build behaviour was changed to follow the exe.
//  * the marker reset of C3dMarkers::Init (0x724E40, the inlined C3dMarker::C3dMarker loop 0x724E71..0x724EE3), run natively from a copy of the exe's bytes on a poisoned marker array;
//    the port's `notsa::audit::ResetMarkers3dLikeExe` (3dMarkers.cpp, NOTSA_STANDALONE_RUN) must produce the same 5120 bytes (incl. the untouched matrix / LastMapReadY / ResultZ / OnScreenTestTime);
//  * CGangWars::UpdateTerritoryUnderControlPercentage (0x443DE0): zone classification ties (`ballas + vagos <= grove`, `ballas > vagos` else vagos) and the rank order (bubble sort, ties keep
//    grove, ballas, vagos; GangRatings[rank] = gang).
// Both fail against the upstream (DLL) behaviour, so this row only passes in a NOTSA_STANDALONE_RUN build.
#include "3dMarkers.h"
#include "GangWars.h"
#include "Stats.h"

namespace notsa::audit { void ResetMarkers3dLikeExe(C3dMarker* markers, size_t count); }

static void TestNotsa() {
    Run("C3dMarkers::Init marker reset 0x724E40", [&](Rng& r, std::string& d) {
        constexpr size_t kBytes = 32 * 0xA0;
        static_assert(sizeof(C3dMarker) == 0xA0);
        static uint8_t poison[kBytes], a[kBytes], e[kBytes];
        static uint8_t* code = nullptr;
        if (!code) { // the exe's code from `sub esp, 0x10` to the end of the 32x loop, followed by the epilogue of Init (pop edi / esi / ebp / ebx; add esp, 0x10; ret)
            code = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x100, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
            std::memcpy(code, reinterpret_cast<const void*>(0x724E40), 0x724EE5 - 0x724E40);
            static const uint8_t epilogue[] = { 0x5F, 0x5E, 0x5D, 0x5B, 0x83, 0xC4, 0x10, 0xC3 };
            std::memcpy(code + (0x724EE5 - 0x724E40), epilogue, sizeof epilogue);
        }
        for (auto& b : poison) b = static_cast<uint8_t>(r.u32());
        auto* const arr = reinterpret_cast<uint8_t*>(0xC7DD58);
        std::memcpy(arr, poison, kBytes);
        reinterpret_cast<void(__cdecl*)()>(code)();
        std::memcpy(e, arr, kBytes);
        std::memcpy(arr, poison, kBytes);
        notsa::audit::ResetMarkers3dLikeExe(reinterpret_cast<C3dMarker*>(arr), 32);
        std::memcpy(a, arr, kBytes);
        Hit(true);
        if (!std::memcmp(a, e, kBytes)) return true;
        d = DiffDesc(a, e, kBytes, "marker");
        return false;
    }, 200);

    Run("CGangWars::UpdateTerritoryUnderControlPercentage 0x443DE0", [&](Rng& r, std::string& d) {
        auto* const totalZones    = reinterpret_cast<int16*>(0xBA3794);
        auto* const zones         = reinterpret_cast<uint8_t*>(0xBA3798);   // 380 x 0x20, the zone info index is the word at +0x1C
        auto* const totalInfos    = reinterpret_cast<int16*>(0xBA1DE8);
        auto* const infos         = reinterpret_cast<uint8_t*>(0xBA1DF0);   // 0x11 per info, GangStrength[0..2] = ballas, grove, vagos
        auto* const ratingStrength = reinterpret_cast<int32*>(0x96AB2C);    // [3]
        auto* const ratings       = reinterpret_cast<int32*>(0x96AB38);     // [3]
        auto* const percentage    = reinterpret_cast<float*>(0x96AB9C);
        static uint8_t zonesSave[380 * 0x20], infosSave[380 * 0x11];
        std::memcpy(zonesSave, zones, sizeof zonesSave); std::memcpy(infosSave, infos, sizeof infosSave);
        const int16 totalSave = *totalZones, totalInfosSave = *totalInfos;

        const int nInfos = 1 + r.below(12);
        *totalInfos = static_cast<int16>(nInfos + 1);
        for (int i = 0; i < nInfos + 1; ++i) {
            uint8_t* inf = infos + i * 0x11;
            std::memset(inf, 0, 0x11);
            const int maxv = r.below(4) == 0 ? 255 : 3;           // small values make ties likely
            for (int g = 0; g < 3; ++g) inf[g] = static_cast<uint8_t>(r.below(maxv + 1));
        }
        const int nZones = r.below(10) == 0 ? 0 : 1 + r.below(380);
        *totalZones = static_cast<int16>(nZones);
        // ZONEINFO: the exe loops over the first TotalNumberOfNavigationZones zones only (0x443DE0: count word at 0xBA3794); slots past the count keep stale zone info indices half of the time
        for (int i = 0; i < 380; ++i) *reinterpret_cast<uint16_t*>(zones + i * 0x20 + 0x1C) = static_cast<uint16_t>(i < nZones || r.below(2) ? r.below(nInfos + 1) : 0);

        auto poisonOut = [&](uint32_t seed) {
            Rng p(seed);
            for (int i = 0; i < 3; ++i) { ratingStrength[i] = static_cast<int32>(p.u32()); ratings[i] = static_cast<int32>(p.u32()); }
            *percentage = BitsF(p.u32());
            CStats::SetStatValue(STAT_TERRITORIES_HELD, static_cast<float>(p.below(200)));
            CStats::SetStatValue(STAT_HIGHEST_NUMBER_OF_TERRITORIES_HELD, static_cast<float>(p.below(200)));
        };
        struct Out { int32 rs[3], rt[3]; uint32 pct; uint32 held, record; };
        auto read = [&] { Out o; for (int i = 0; i < 3; ++i) { o.rs[i] = ratingStrength[i]; o.rt[i] = ratings[i]; } o.pct = FBits(*percentage);
                          o.held = FBits(CStats::GetStatValue(STAT_TERRITORIES_HELD)); o.record = FBits(CStats::GetStatValue(STAT_HIGHEST_NUMBER_OF_TERRITORIES_HELD)); return o; };
        const uint32_t seed = r.u32();
        // CStats::CheckForStatsMessage (pad / HUD / text) is not linked in this test: turn the PORT function into a no-op (the exe's own one is skipped by bShowUpdateStats = false)
        static bool statsPatched = false;
        if (!statsPatched) {
            statsPatched = true;
            auto* const fn = reinterpret_cast<uint8_t*>(&CStats::CheckForStatsMessage);
            DWORD old = 0; VirtualProtect(fn, 8, PAGE_EXECUTE_READWRITE, &old);
            fn[0] = 0xC3;   // ret
        }
        const bool showStatsSave = CStats::bShowUpdateStats; CStats::bShowUpdateStats = false;
        poisonOut(seed);
        oracle::Fn<void __cdecl()>(0x443DE0)();
        const Out e = read();
        poisonOut(seed);
        CGangWars::UpdateTerritoryUnderControlPercentage();
        const Out a = read();

        CStats::bShowUpdateStats = showStatsSave;
        std::memcpy(zones, zonesSave, sizeof zonesSave); std::memcpy(infos, infosSave, sizeof infosSave); *totalZones = totalSave; *totalInfos = totalInfosSave;
        Hit(e.rs[0] + e.rs[1] + e.rs[2] > 0);
        if (!std::memcmp(&a, &e, sizeof a)) return true;
        d = "zones " + std::to_string(nZones) + " ratings got {" + std::to_string(a.rt[0]) + "," + std::to_string(a.rt[1]) + "," + std::to_string(a.rt[2]) + "} strength {" + std::to_string(a.rs[0]) + "," + std::to_string(a.rs[1]) + "," + std::to_string(a.rs[2]) +
            "} exe ratings {" + std::to_string(e.rt[0]) + "," + std::to_string(e.rt[1]) + "," + std::to_string(e.rt[2]) + "} strength {" + std::to_string(e.rs[0]) + "," + std::to_string(e.rs[1]) + "," + std::to_string(e.rs[2]) + "}";
        return false;
    }, 3000);
}
