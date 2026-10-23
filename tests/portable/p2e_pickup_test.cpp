// P-2e host test (portable arm64): CPickup_File (0x20) and the FileRecord trait of CPickup.
//   tools/portable/run_p2e_test.sh   (golden vectors come from tools/portable/gen_x86_golden.py, an independent generator; generated into $OUT/p2e_golden.inc)
// Real headers Pickup.h / GenericGameStorage.h / FileRecord.hpp / FileRecords/PickupFile.hpp on the tiny Base.h + rwplcore.h stub of tests/portable/p2c_stubs (StdInc is not linkable on the Mac yet).
#include <Base.h>
#define MAX_PATH 260
#define NOTSA_GLOBAL(name, addr, type, ...) std::type_identity_t<NOTSA_UNPAREN type> name __VA_ARGS__
#include <array>
#include "Pickup.h"
#include "GenericGameStorage.h"
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include "p2e_golden.inc"

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

// ---- layout ---------------------------------------------------------------------------------------------------------------------------------------------
static_assert(notsa::HasFileRecord<CPickup> && notsa::FileSizeOf<CPickup> == 0x20);
static_assert(sizeof(CPickup) == 0x28, "LP64: the CObject* widens CPickup (this is what the record hides)");
static_assert(sizeof(kGolden_CPickup_File_basic) == 0x20 && sizeof(kGolden_CPickup_File_noobj) == 0x20 && sizeof(kGolden_CPickup_File_none_obj) == 0x20);
static_assert(std::is_same_v<notsa::FileRecord<CPickup>::Type, CPickup_File> && std::is_trivially_copyable_v<CPickup_File>);

static CPickup_File rec(const uint8_t* g) { CPickup_File r; std::memcpy(&r, g, sizeof(r)); return r; }
static uint8 packFlags(const CPickup& p) { // independent of the trait: pack the in-memory bit-fields by hand
    return uint8(p.m_nFlags.bDisabled | p.m_nFlags.bEmpty << 1 | p.m_nFlags.bHelpMessageDisplayed << 2 | p.m_nFlags.bVisible << 3 | p.m_nFlags.nPropertyTextIndex << 4);
}

int main() {
    // ---- bit-field order of the compiler == the exe's (LSB first, one byte): set each bit alone and look at the byte --------------------------------------
    {
        static_assert(sizeof(decltype(CPickup::m_nFlags)) == 1);
        auto byteOf = [](auto setter) { CPickup p{}; setter(p); uint8 b; std::memcpy(&b, &p.m_nFlags, 1); return b; };
        CHECK(byteOf([](CPickup& p) { p.m_nFlags.bDisabled = 1; }) == 0x01);
        CHECK(byteOf([](CPickup& p) { p.m_nFlags.bEmpty = 1; }) == 0x02);
        CHECK(byteOf([](CPickup& p) { p.m_nFlags.bHelpMessageDisplayed = 1; }) == 0x04);
        CHECK(byteOf([](CPickup& p) { p.m_nFlags.bVisible = 1; }) == 0x08);
        CHECK(byteOf([](CPickup& p) { p.m_nFlags.nPropertyTextIndex = 5; }) == 0x50);
        CHECK(byteOf([](CPickup& p) { p.m_nFlags.nPropertyTextIndex = 7; }) == 0x70);
    }
    // ---- golden 'basic' (type ON_STREET, live object) file -> object -> file ----------------------------------------------------------------------------------
    {
        CPickup p; p.m_pObject = reinterpret_cast<CObject*>(uintptr_t(0x1122334455));
        notsa::FileRecord<CPickup>::FromFile(p, rec(kGolden_CPickup_File_basic));
        CHECK(p.m_fRevenueValue == 1.5f && p.m_pObject == nullptr && p.m_nAmmo == 77 && p.m_nRegenerationTime == 1000);
        CHECK(p.m_vecPos.x.Raw() == 100 && p.m_vecPos.y.Raw() == -200 && p.m_vecPos.z.Raw() == 300 && p.m_vecPos.x == 12.5f && p.m_vecPos.y == -25.f);
        CHECK(p.m_nMoneyPerDay == 5 && p.m_nModelIndex == 1212 && p.m_nReferenceIndex == 3 && p.m_nPickupType == PICKUP_ON_STREET);
        // flags 0x5D = disabled | help | visible | text 5 ; the loader rule (type != NONE && object != 0) clears bVisible
        CHECK(p.m_nFlags.bDisabled && !p.m_nFlags.bEmpty && p.m_nFlags.bHelpMessageDisplayed && !p.m_nFlags.bVisible && p.m_nFlags.nPropertyTextIndex == 5);
        CHECK(packFlags(p) == (0x5D & ~0x08));
        // writing it back: object slot is 0 (nothing live), visible stays false
        const auto out = notsa::FileRecord<CPickup>::ToFile(p);
        CHECK(out.object == 0 && out.flags == (0x5D & ~0x08));
        auto want = rec(kGolden_CPickup_File_basic); want.object = 0; want.flags &= ~0x08;
        CHECK(eq(&out, &want, sizeof(want)));
    }
    // ---- ToFile == golden for an object whose state is what the golden decodes to (live object present, visible) ---------------------------------------------
    {
        CPickup p{};
        p.m_fRevenueValue = 1.5f; p.m_pObject = reinterpret_cast<CObject*>(uintptr_t(0xABCDEF0123)); p.m_nAmmo = 77; p.m_nRegenerationTime = 1000;
        p.m_vecPos = CompressedLargeVector{ int16(100), int16(-200), int16(300) };
        p.m_nMoneyPerDay = 5; p.m_nModelIndex = 1212; p.m_nReferenceIndex = 3; p.m_nPickupType = PICKUP_ON_STREET;
        p.m_nFlags.bDisabled = 1; p.m_nFlags.bHelpMessageDisplayed = 1; p.m_nFlags.bVisible = 1; p.m_nFlags.nPropertyTextIndex = 5;
        const auto out = notsa::FileRecord<CPickup>::ToFile(p);
        CHECK(eq(&out, kGolden_CPickup_File_basic, 0x20));   // the live pointer is written as 1 (== golden obj=1), pad bytes 0
        reset(); CHECK(CGenericGameStorage::SaveDataToWorkBuffer(p)); CHECK(g_buf.size() == 0x20 && eq(g_buf.data(), kGolden_CPickup_File_basic, 0x20));
        p.m_pObject = nullptr; const auto out0 = notsa::FileRecord<CPickup>::ToFile(p); CHECK(out0.object == 0);
    }
    // ---- golden 'noobj' (type ONCE, no object): bVisible from the file is kept; extremes of the int16/uint16 fields -----------------------------------------
    {
        CPickup p; notsa::FileRecord<CPickup>::FromFile(p, rec(kGolden_CPickup_File_noobj));
        CHECK(p.m_pObject == nullptr && p.m_nPickupType == PICKUP_ONCE && p.m_nAmmo == 5);
        CHECK(p.m_vecPos.x.Raw() == -32768 && p.m_vecPos.y.Raw() == 32767 && p.m_vecPos.z.Raw() == 0 && p.m_nMoneyPerDay == 65535 && p.m_nModelIndex == -1 && p.m_nReferenceIndex == -2);
        CHECK(!p.m_nFlags.bDisabled && p.m_nFlags.bEmpty && !p.m_nFlags.bHelpMessageDisplayed && p.m_nFlags.bVisible && p.m_nFlags.nPropertyTextIndex == 0);   // 0x0A = empty | visible
        const auto out = notsa::FileRecord<CPickup>::ToFile(p); CHECK(eq(&out, kGolden_CPickup_File_noobj, 0x20));
    }
    // ---- golden 'none_obj' (PICKUP_NONE with a stale object slot): the exe loader leaves the entry alone => bVisible stays, pointer is null here -----------
    {
        CPickup p; notsa::FileRecord<CPickup>::FromFile(p, rec(kGolden_CPickup_File_none_obj));
        CHECK(p.m_nPickupType == PICKUP_NONE && p.m_pObject == nullptr && p.m_nFlags.bVisible);
        // control: the same bytes with a non-NONE type DO get bVisible cleared
        auto r = rec(kGolden_CPickup_File_none_obj); r.pickupType = PICKUP_ONCE; CPickup q; notsa::FileRecord<CPickup>::FromFile(q, r); CHECK(!q.m_nFlags.bVisible && q.m_pObject == nullptr);
        // ... and with object == 0 they do not
        r.object = 0; CPickup w; notsa::FileRecord<CPickup>::FromFile(w, r); CHECK(w.m_nFlags.bVisible);
    }
    // ---- exe-written save: garbage pointer value (any non-zero), garbage pad bytes and unused flag bit 7 ------------------------------------------------------
    {
        auto r = rec(kGolden_CPickup_File_basic); r.object = 0xDEADBEEF; r.pad1E[0] = 0x77; r.pad1E[1] = 0x88; r.flags |= 0x80;
        CPickup p; notsa::FileRecord<CPickup>::FromFile(p, r); CHECK(p.m_pObject == nullptr && !p.m_nFlags.bVisible);
        const auto out = notsa::FileRecord<CPickup>::ToFile(p); CHECK(out.object == 0 && out.pad1E[0] == 0 && out.pad1E[1] == 0 && (out.flags & 0x80) == 0);
    }
    // ---- the CPickups::Save/Load loop: 620 entries => 620 * 0x20 bytes, in order, and a truncated stream fails ---------------------------------------------
    {
        std::array<CPickup, 620> a{}, b{};
        std::mt19937 rng(0xFACE);
        for (auto& p : a) {
            p.m_fRevenueValue = float(rng() % 1000) / 8.f; p.m_pObject = (rng() & 1) ? reinterpret_cast<CObject*>(uintptr_t(rng()) << 8 | 8) : nullptr; p.m_nAmmo = rng(); p.m_nRegenerationTime = rng();
            p.m_vecPos = CompressedLargeVector{ int16(rng()), int16(rng()), int16(rng()) }; p.m_nMoneyPerDay = (uint16)rng(); p.m_nModelIndex = (int16)rng(); p.m_nReferenceIndex = (int16)rng();
            p.m_nPickupType = (ePickupType)(rng() % 23);
            p.m_nFlags.bDisabled = rng() & 1; p.m_nFlags.bEmpty = rng() & 1; p.m_nFlags.bHelpMessageDisplayed = rng() & 1; p.m_nFlags.bVisible = rng() & 1; p.m_nFlags.nPropertyTextIndex = rng() & 7;
        }
        reset(); for (auto& p : a) CGenericGameStorage::SaveDataToWorkBuffer(p);
        CHECK(g_buf.size() == 620 * 0x20);
        int bad = 0;
        for (auto& p : b) { if (!CGenericGameStorage::LoadDataFromWorkBuffer(p)) bad++; }
        CHECK(g_pos == g_buf.size());
        for (size_t i = 0; i < a.size(); ++i) {
            const auto &x = a[i], &y = b[i];
            const bool cleared = x.m_nPickupType != PICKUP_NONE && x.m_pObject != nullptr;   // the exe loader rule
            if (y.m_pObject != nullptr || x.m_fRevenueValue != y.m_fRevenueValue || x.m_nAmmo != y.m_nAmmo || x.m_nRegenerationTime != y.m_nRegenerationTime
                || x.m_vecPos.x.Raw() != y.m_vecPos.x.Raw() || x.m_vecPos.y.Raw() != y.m_vecPos.y.Raw() || x.m_vecPos.z.Raw() != y.m_vecPos.z.Raw()
                || x.m_nMoneyPerDay != y.m_nMoneyPerDay || x.m_nModelIndex != y.m_nModelIndex || x.m_nReferenceIndex != y.m_nReferenceIndex || x.m_nPickupType != y.m_nPickupType) bad++;
            const uint8 fx = packFlags(x), fy = packFlags(y);
            if (fy != (cleared ? (fx & ~0x08) : fx)) bad++;
        }
        CHECK(bad == 0);
        // saving the loaded array again never writes a pointer value (object slot 0 for every entry) and is stable
        auto first = g_buf; reset(); for (auto& p : b) CGenericGameStorage::SaveDataToWorkBuffer(p);
        CHECK(g_buf.size() == first.size());
        bool objZero = true; for (size_t i = 0; i < 620; ++i) { uint32_t o; std::memcpy(&o, &g_buf[i * 0x20 + 4], 4); objZero &= (o == 0); }
        CHECK(objZero);
        // truncated stream
        reset(); g_buf.assign(first.begin(), first.begin() + 0x1F); { CPickup p; CHECK(!CGenericGameStorage::LoadDataFromWorkBuffer(p)); }
    }
    // ---- source scan: Pickups.cpp Save/Load go through the typed overloads, nothing raw / sizeof(CPickup) -----------------------------------------------------
#define P2E_STR2(x) #x
#define P2E_STR(x) P2E_STR2(x)
    {
        auto slurp = [](const std::string& p) { std::ifstream in(p); std::stringstream ss; ss << in.rdbuf(); return ss.str(); };
        const std::string repo = P2E_STR(REPO_DIR);
        const auto src = slurp(repo + "/source/game_sa/Pickups.cpp");
        const auto lPos = src.find("void CPickups::Load()"), sPos = src.find("void CPickups::Save()"), ePos = src.find("void ModifyStringLabelForControlSetting");
        CHECK(lPos != std::string::npos && sPos != std::string::npos && ePos != std::string::npos && lPos < sPos && sPos < ePos);
        const auto body = src.substr(lPos, ePos - lPos);
        CHECK(body.find("sizeof(CPickup)") == std::string::npos && body.find("sizeof(pickup)") == std::string::npos);
        CHECK(body.find("LoadDataFromWorkBuffer(pickup)") != std::string::npos && body.find("SaveDataToWorkBuffer(pickup)") != std::string::npos);
    }
    std::printf(fails ? "p2e FAILED (%d)\n" : "p2e host test: ALL OK (fails=%d)\n", fails);
    return fails ? 1 : 0;
}
