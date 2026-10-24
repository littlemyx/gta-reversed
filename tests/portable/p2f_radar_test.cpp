// P-2f host test (portable arm64): tRadarTrace_File (0x28), the FileRecord trait of tRadarTrace and the REAL CRadar::Save / CRadar::Load bodies of Radar.cpp.
//   tools/portable/run_p2f_test.sh   (goldens: tools/portable/gen_x86_golden.py, independent of the code under test -> $OUT/p2f_golden.inc;
//                                     Radar.h is the real header cut before `class CRadar` with its RGBA.h/Vector2D.h includes dropped; CRadar::Load/Save are extracted verbatim from Radar.cpp -> $OUT/p2f_saveload.inc)
#include <Base.h>
#define MAX_PATH 260
#define NOTSA_GLOBAL(name, addr, type, ...) std::type_identity_t<NOTSA_UNPAREN type> name __VA_ARGS__
#include <array>
#include "Vector.h"
struct CRGBA { unsigned char r, g, b, a; };
#include "Radar.h"
#include "GenericGameStorage.h"
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include "p2f_golden.inc"

#ifndef NOTSA_PORTABLE
#error "this test is for the portable build"
#endif

static std::vector<uint8> g_buf; static size_t g_pos = 0;
bool CGenericGameStorage::SaveDataToWorkBuffer(void* d, int32 n) { auto* p = (uint8*)d; g_buf.insert(g_buf.end(), p, p + n); return true; }
bool CGenericGameStorage::LoadDataFromWorkBuffer(void* d, int32 n) { if (g_pos + n > g_buf.size()) return false; std::memcpy(d, g_buf.data() + g_pos, n); g_pos += n; return true; }
static void reset() { g_buf.clear(); g_pos = 0; }

static int fails = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); fails++; } } while (0)
static bool eq(const void* a, const void* b, size_t n) { return std::memcmp(a, b, n) == 0; }

// ---- stubs for what the extracted CRadar::Load/Save use --------------------------------------------------------------------------------------------------
class CEntryExit { public: char pad[16]; };
struct EnexPool {
    static constexpr int N = 8;
    CEntryExit slots[N]; bool freeSlot[N]{};
    ptrdiff_t GetIndex(CEntryExit* p) const { return ((intptr_t)p - (intptr_t)slots) / (intptr_t)sizeof(CEntryExit); }   // like the real pool: plain pointer arithmetic, no range check
    bool IsIndexInBounds(ptrdiff_t i) const { return i >= 0 && i < N; }
    bool IsFreeSlotAtIndex(ptrdiff_t i) const { return freeSlot[i]; }
} g_pool;
struct CEntryExitManager {
    static EnexPool* GetPool() { return &g_pool; }
    static CEntryExit* GetInSlot(int32 slot) { return (slot >= 0 && slot < EnexPool::N) ? &g_pool.slots[slot] : nullptr; }
};
struct CRadar {
    static constexpr uint32 MAX_RADAR_TRACES = 175;
    static inline std::array<tRadarTrace, MAX_RADAR_TRACES> ms_RadarTrace{};
    static void Initialise() { ms_RadarTrace = {}; }
    static bool Load();
    static bool Save();
};
#include "p2f_saveload.inc"   // the verbatim bodies of CRadar::Load / CRadar::Save from source/game_sa/Radar.cpp

// ---- helpers -------------------------------------------------------------------------------------------------------------------------------------------
template<size_t N> static tRadarTrace_File rec(const uint8_t (&g)[N]) { static_assert(N == sizeof(tRadarTrace_File)); tRadarTrace_File r; std::memcpy(&r, g, N); return r; }
using FR = notsa::FileRecord<tRadarTrace>;
template<size_t N> static bool bytesEq(const tRadarTrace_File& r, const uint8_t (&g)[N]) { return N == sizeof(r) && eq(&r, g, N); }

static void sameTrace(const tRadarTrace& a, const tRadarTrace& b, int line) {
    const bool ok = a.m_nColour == b.m_nColour && a.m_nEntityHandle == b.m_nEntityHandle && a.m_vPosition.x == b.m_vPosition.x && a.m_vPosition.y == b.m_vPosition.y && a.m_vPosition.z == b.m_vPosition.z
        && a.m_nCounter == b.m_nCounter && a.m_fSphereRadius == b.m_fSphereRadius && a.m_nBlipSize == b.m_nBlipSize && a.m_nBlipSprite == b.m_nBlipSprite
        && a.m_bBright == b.m_bBright && a.m_bTrackingBlip == b.m_bTrackingBlip && a.m_bShortRange == b.m_bShortRange && a.m_bFriendly == b.m_bFriendly && a.m_bBlipRemain == b.m_bBlipRemain
        && a.m_bBlipFade == b.m_bBlipFade && a.m_nCoordBlipAppearance == b.m_nCoordBlipAppearance && a.m_nBlipDisplayFlag == b.m_nBlipDisplayFlag && a.m_nBlipType == b.m_nBlipType && a.m_nAppearance == b.m_nAppearance;
    if (!ok) { std::printf("FAIL line %d: traces differ\n", line); fails++; }
}
static void randomTrace(tRadarTrace& t, std::mt19937& r) {
    t = {};
    t.m_nColour = (eBlipColour)(r() % 9); t.m_nEntityHandle = r(); t.m_vPosition = CVector{ float(int(r() % 20000) - 10000) / 4.f, float(int(r() % 20000) - 10000) / 4.f, float(r() % 800) / 8.f };
    t.m_nCounter = (uint16)r(); t.m_fSphereRadius = float(r() % 1000) / 16.f; t.m_nBlipSize = (uint16)r(); t.m_nBlipSprite = (eRadarSprite)(int(r() % 70) - 5);
    t.m_bBright = r() & 1; t.m_bTrackingBlip = r() & 1; t.m_bShortRange = r() & 1; t.m_bFriendly = r() & 1; t.m_bBlipRemain = r() & 1; t.m_bBlipFade = r() & 1; t.m_nCoordBlipAppearance = r() & 3;
    t.m_nBlipDisplayFlag = (eBlipDisplay)(r() & 3); t.m_nBlipType = (eBlipType)(r() % 9); t.m_nAppearance = (eBlipAppearance)(r() & 3);
}

// ---- layout ---------------------------------------------------------------------------------------------------------------------------------------------
static_assert(notsa::HasFileRecord<tRadarTrace> && notsa::FileSizeOf<tRadarTrace> == 0x28);
static_assert(sizeof(tRadarTrace_File) == 0x28);
static_assert(sizeof(tRadarTrace) == 0x30, "LP64: the CEntryExit* union widens tRadarTrace (this is what the record hides)");

int main() {
    // ---- 'basic' golden: ToFile with enex index 7 == the independent vector byte for byte (pads zero); FromFile gives every field ---------------------------------------
    {
        tRadarTrace t{};
        t.m_nColour = (eBlipColour)8; t.m_nEntityHandle = 0x00A1B2C3; t.m_vPosition = CVector{ -2500.5f, 1000.25f, 12.125f }; t.m_nCounter = 300; t.m_fSphereRadius = 2.5f; t.m_nBlipSize = 3;
        t.m_pEntryExit = reinterpret_cast<CEntryExit*>(uintptr_t(0x1122334455)); t.m_nBlipSprite = (eRadarSprite)27;
        t.m_bBright = true; t.m_bShortRange = true; t.m_bFriendly = true; t.m_nBlipDisplayFlag = BLIP_DISPLAY_BLIPONLY; t.m_nBlipType = BLIP_CHAR; t.m_nAppearance = BLIP_FLAG_THREAT;
        CHECK(bytesEq(FR::ToFile(t, 7), kGolden_tRadarTrace_File_basic));
        CHECK(FR::ToFile(t).entryExitPoolInd == 0);                      // 1-argument form: no enex link, never a pointer value
        tRadarTrace u{}; u.m_pEntryExit = reinterpret_cast<CEntryExit*>(uintptr_t(0x1122334455));
        FR::FromFile(u, rec(kGolden_tRadarTrace_File_basic));
        sameTrace(t, u, __LINE__); CHECK(u.m_pEntryExit == nullptr);     // m_pEntryExit is null until CRadar::Load resolves the index
        CHECK(rec(kGolden_tRadarTrace_File_basic).entryExitPoolInd == 7);
    }
    // ---- every bit-field set once (and ONLY it): ToFile == golden, FromFile sets exactly that member ------------------------------------------------------------
    {
        struct Bit { const uint8_t* g; void (*set)(tRadarTrace&); };
        #define BIT(name, stmt) { kGolden_tRadarTrace_File_##name, [](tRadarTrace& t) { stmt; } }
        const Bit bits[] = {
            BIT(b_bright, t.m_bBright = true), BIT(b_tracking, t.m_bTrackingBlip = true), BIT(b_short, t.m_bShortRange = true), BIT(b_friendly, t.m_bFriendly = true), BIT(b_remain, t.m_bBlipRemain = true),
            BIT(b_fade, t.m_bBlipFade = true), BIT(b_coord1, t.m_nCoordBlipAppearance = 1), BIT(b_coord2, t.m_nCoordBlipAppearance = 2), BIT(b_coord3, t.m_nCoordBlipAppearance = 3),
            BIT(b_disp1, t.m_nBlipDisplayFlag = BLIP_DISPLAY_MARKERONLY), BIT(b_disp2, t.m_nBlipDisplayFlag = BLIP_DISPLAY_BLIPONLY), BIT(b_disp3, t.m_nBlipDisplayFlag = BLIP_DISPLAY_BOTH),
            BIT(b_type1, t.m_nBlipType = BLIP_CAR), BIT(b_type2, t.m_nBlipType = BLIP_CHAR), BIT(b_type4, t.m_nBlipType = BLIP_COORD), BIT(b_type8, t.m_nBlipType = BLIP_AIRSTRIP), BIT(b_type15, t.m_nBlipType = (eBlipType)15),
            BIT(b_app1, t.m_nAppearance = BLIP_FLAG_THREAT), BIT(b_app2, t.m_nAppearance = BLIP_FLAG_UNK), BIT(b_app3, t.m_nAppearance = (eBlipAppearance)3),
        };
        #undef BIT
        int n = 0;
        for (const auto& b : bits) {
            tRadarTrace t{}; b.set(t);
            tRadarTrace_File want; std::memcpy(&want, b.g, sizeof(want));
            const auto got = FR::ToFile(t);
            CHECK(eq(&got, &want, sizeof(got)));
            tRadarTrace u{}; u.m_bBright = true; u.m_nBlipType = BLIP_PICKUP; FR::FromFile(u, want);   // dirty destination: FromFile must overwrite every bit
            sameTrace(t, u, __LINE__);
            ++n;
        }
        CHECK(n == 20);
        // extreme: all-ones fields, sprite -5, flags 0xFF/0xFF
        tRadarTrace u{}; FR::FromFile(u, rec(kGolden_tRadarTrace_File_extreme));
        CHECK(u.m_nColour == (eBlipColour)0xFFFFFFFF && u.m_nEntityHandle == 0xFFFFFFFF && u.m_nBlipSprite == (eRadarSprite)-5 && u.m_nCounter == 65535 && u.m_nBlipSize == 65535 && u.m_nCoordBlipAppearance == 3
              && u.m_nBlipDisplayFlag == BLIP_DISPLAY_BOTH && u.m_nBlipType == (eBlipType)15 && u.m_nAppearance == (eBlipAppearance)3 && u.m_bBright && u.m_bTrackingBlip && u.m_bShortRange && u.m_bFriendly && u.m_bBlipRemain && u.m_bBlipFade);
        const auto back = FR::ToFile(u, 0xFFFFFFFF);
        CHECK(bytesEq(back, kGolden_tRadarTrace_File_extreme));
    }
    // ---- all 65536 flag-byte pairs survive FromFile -> ToFile unchanged (no bit lost / aliased) ---------------------------------------------------------------------
    {
        int bad = 0;
        for (unsigned v = 0; v < 65536; ++v) {
            tRadarTrace_File f{}; f.flags0 = uint8(v & 0xFF); f.flags1 = uint8(v >> 8);
            tRadarTrace t{}; FR::FromFile(t, f);
            const auto o = FR::ToFile(t);
            if (o.flags0 != f.flags0 || o.flags1 != f.flags1) bad++;
        }
        CHECK(bad == 0);
    }
    // ---- exe-written garbage in the pad bytes is ignored and normalised ---------------------------------------------------------------------------------------------
    {
        auto r = rec(kGolden_tRadarTrace_File_basic); r.pad16[0] = 0xAA; r.pad16[1] = 0xBB; r.pad1E[0] = 0xCC; r.pad1E[1] = 0xDD; r.pad27 = 0xEE;
        tRadarTrace t{}; FR::FromFile(t, r);
        CHECK(bytesEq(FR::ToFile(t, 7), kGolden_tRadarTrace_File_basic));
    }

    // ---- the REAL CRadar::Save / CRadar::Load (extracted from Radar.cpp) -----------------------------------------------------------------------------------------
    {
        std::mt19937 rng(0xB11B);
        auto& traces = CRadar::ms_RadarTrace;
        uint8_t outsideBlob[64]{};
        std::array<uint32, 175> expectInd{};
        g_pool.freeSlot[5] = true;                                    // slot 5 is a free pool slot
        for (size_t i = 0; i < traces.size(); ++i) {
            randomTrace(traces[i], rng);
            switch (i % 6) {                                          // enex link kinds: none / live slot 0..7 (5 is free) / pointer outside the pool
            case 0: traces[i].m_pEntryExit = nullptr; expectInd[i] = 0; break;
            case 1: case 2: case 3: { const int s = int(rng() % EnexPool::N); traces[i].m_pEntryExit = &g_pool.slots[s]; expectInd[i] = g_pool.freeSlot[s] ? 0 : uint32(s + 1); break; }
            case 4: traces[i].m_pEntryExit = &g_pool.slots[EnexPool::N - 1]; expectInd[i] = EnexPool::N; break;
            default: traces[i].m_pEntryExit = reinterpret_cast<CEntryExit*>(outsideBlob + 8); expectInd[i] = 0; break;
            }
        }
        const auto before = traces;
        reset(); CHECK(CRadar::Save());
        CHECK(g_buf.size() == 175 * 0x28);
        // the live traces are untouched (no union patching, tracking flag intact), pointers still the pointers
        for (size_t i = 0; i < traces.size(); ++i) { sameTrace(before[i], traces[i], __LINE__); CHECK(before[i].m_pEntryExit == traces[i].m_pEntryExit); }
        int bad = 0;
        for (size_t i = 0; i < traces.size(); ++i) {
            tRadarTrace_File f; std::memcpy(&f, &g_buf[i * 0x28], 0x28);
            const bool tracking = before[i].m_nBlipType == BLIP_COORD || before[i].m_nBlipType == BLIP_CONTACT_POINT ? before[i].m_bTrackingBlip : false;   // exe: tracking cleared for the other types
            auto want = before[i]; want.m_bTrackingBlip = tracking;
            if (f.entryExitPoolInd != expectInd[i]) bad++;
            const auto w = FR::ToFile(want, expectInd[i]);
            if (!eq(&f, &w, 0x28)) bad++;
            if (f.pad16[0] | f.pad16[1] | f.pad1E[0] | f.pad1E[1] | f.pad27) bad++;
            if (f.entryExitPoolInd > EnexPool::N) bad++;               // never a pointer value
        }
        CHECK(bad == 0);
        // Load back: every field, enex pointer resolved from index+1, null for 0
        const auto stream = g_buf; g_pos = 0;
        for (auto& t : traces) { t = {}; t.m_bBright = true; t.m_pEntryExit = reinterpret_cast<CEntryExit*>(outsideBlob); }   // dirty
        CHECK(CRadar::Load());   // Load() calls Initialise() first
        CHECK(g_pos == g_buf.size());
        bad = 0;
        for (size_t i = 0; i < traces.size(); ++i) {
            auto want = before[i];
            want.m_bTrackingBlip = (before[i].m_nBlipType == BLIP_COORD || before[i].m_nBlipType == BLIP_CONTACT_POINT) ? before[i].m_bTrackingBlip : false;
            sameTrace(want, traces[i], __LINE__);
            const CEntryExit* expectPtr = expectInd[i] ? &g_pool.slots[expectInd[i] - 1] : nullptr;
            if (traces[i].m_pEntryExit != expectPtr) bad++;
        }
        CHECK(bad == 0);
        // second save of the loaded array is byte-identical (stable)
        reset(); CRadar::Save(); CHECK(g_buf == stream);
    }
    // ---- loading an exe-written stream: slot index beyond the pool => null (no crash), pad garbage and wild flag bytes accepted ----------------------------------------------------
    {
        reset();
        for (int i = 0; i < 175; ++i) { auto r = rec(kGolden_tRadarTrace_File_basic); r.entryExitPoolInd = i == 3 ? 200 : (i % 2 ? 7 : 0); r.pad16[0] = 0x55; r.pad27 = 0x66; const auto* p = (const uint8*)&r; g_buf.insert(g_buf.end(), p, p + 0x28); }
        CHECK(CRadar::Load());
        CHECK(CRadar::ms_RadarTrace[0].m_pEntryExit == nullptr && CRadar::ms_RadarTrace[1].m_pEntryExit == &g_pool.slots[6] && CRadar::ms_RadarTrace[3].m_pEntryExit == nullptr);
        CHECK(CRadar::ms_RadarTrace[2].m_nEntityHandle == 0x00A1B2C3 && CRadar::ms_RadarTrace[2].m_nBlipSprite == (eRadarSprite)27 && CRadar::ms_RadarTrace[2].m_bFriendly);
    }
    // ---- source scan: the portable branches of Radar.cpp Save/Load go through the record; the MSVC (#else) branch is the original raw code ------------------------------------
#define P2F_STR2(x) #x
#define P2F_STR(x) P2F_STR2(x)
    {
        auto slurp = [](const std::string& p) { std::ifstream in(p); std::stringstream ss; ss << in.rdbuf(); return ss.str(); };
        const std::string repo = P2F_STR(REPO_DIR);
        const auto src = slurp(repo + "/source/game_sa/Radar.cpp");
        const auto lPos = src.find("bool CRadar::Load()"), sPos = src.find("bool CRadar::Save()"), ePos = src.find("const GxtChar* CRadar::GetBlipName");
        CHECK(lPos != std::string::npos && sPos != std::string::npos && ePos != std::string::npos && lPos < sPos && sPos < ePos);
        const auto body = src.substr(lPos, ePos - lPos);
        const auto elsePos = body.find("#else", body.find("#ifdef NOTSA_PORTABLE", sPos - lPos));
        const auto portableSave = body.substr(body.find("#ifdef NOTSA_PORTABLE", sPos - lPos), elsePos - body.find("#ifdef NOTSA_PORTABLE", sPos - lPos));
        CHECK(portableSave.find("sizeof(trace)") == std::string::npos && portableSave.find("m_EntryExitPoolInd") == std::string::npos && portableSave.find("SaveDataToWorkBuffer(rec)") != std::string::npos);
        CHECK(body.find("SaveDataToWorkBuffer(&trace, sizeof(trace))") != std::string::npos);   // MSVC branch kept
    }
    std::printf(fails ? "p2f FAILED (%d)\n" : "p2f host test: ALL OK (fails=%d)\n", fails);
    return fails ? 1 : 0;
}
