// (g) FIX-AUDIT: functions that carry a `notsa::bugfixes::` switch. The run build (NOTSA_STANDALONE_RUN) has every switch OFF, so the port must equal the exe's machine code:
// CCustomCarPlateMgr::GeneratePlateText 0x6FD5B0 (plate letters A-W / digits 0-8, switch CCustomCarPlateMgr_GeneratePlateText_MissingLettersAndDigits) and
// CAnimBlendSequence::SetName 0x4D0C50 (the exe leaves the bone-tag flag alone, switch AnimBlendSequence_SetName_SetBoneTagFlag).
// With a switch ON the first test fails within a few cases (letters X-Z and digit 9 appear), the second as soon as the flag was set before SetName.
#include "CustomCarPlateMgr.h"
#include "Animation/AnimBlendSequence.h"

#include <reversiblebugfixes/Bugs.hpp>

static void TestFixes() {
    // mutation check: LOGIC_ORACLE_FIXES_ON=1 turns the two switches ON at run time (the objects are dynamically initialised `inline const`, they live in .data); both rows must then FAIL
    if (std::getenv("LOGIC_ORACLE_FIXES_ON")) {
        const_cast<bool&>(notsa::bugfixes::CCustomCarPlateMgr_GeneratePlateText_MissingLettersAndDigits.Enabled) = true;
        const_cast<bool&>(notsa::bugfixes::AnimBlendSequence_SetName_SetBoneTagFlag.Enabled)                    = true;
        std::puts("  [FIX-AUDIT mutation: the plate and SetName switches are ON, mismatches are expected]");
    }
    Run("CCustomCarPlateMgr::GeneratePlateText 0x6FD5B0", [&](Rng& r, std::string& d) {
        const uint32 len = r.below(10) == 0 ? (uint32)r.below(6) : 4 + 4 * r.below(3);   // 4 / 8 / 12 (plate lengths), sometimes tiny or odd
        const uint32 seed = r.u32();
        char a[40], e[40];
        std::memset(a, 0x55, sizeof a); std::memset(e, 0x55, sizeof e);
        SeedBoth(seed); const bool ra = CCustomCarPlateMgr::GeneratePlateText(a, len);
        SeedBoth(seed); const bool re = oracle::Fn<bool __cdecl(char*, int)>(0x6FD5B0)(e, (int)len) & 1;
        Hit(ra && len >= 8);
        if (ra == re && !std::memcmp(a, e, sizeof a)) return true;
        d = "len " + std::to_string(len) + " ret got " + std::to_string(ra) + " exe " + std::to_string(re) + " text got '" + std::string(a, std::min<size_t>(len, 12)) + "' exe '" + std::string(e, std::min<size_t>(len, 12)) + "'";
        return false;
    }, 3000);

    Run("CAnimBlendSequence::SetName 0x4D0C50", [&](Rng& r, std::string& d) {
        alignas(16) uint8 a[sizeof(CAnimBlendSequence)], e[sizeof(CAnimBlendSequence)];
        for (auto& b : a) b = (uint8)r.u32();
        std::memcpy(e, a, sizeof a);
        static const char* names[] = { "Root", "Pelvis", "Spine", "L Thigh", "swim_breast", "", "a", "WALK_PLAYER", "Head" };
        char name[24]; std::strncpy(name, names[r.below(9)], sizeof name);
        if (r.below(3) == 0) for (int i = 0; i < 8; ++i) name[i] = (char)('A' + r.below(26)), name[i + 1] = 0;
        reinterpret_cast<CAnimBlendSequence*>(a)->SetName(name);
        oracle::Fn<void __fastcall(void*, int, const char*)>(0x4D0C50)(e, 0, name);
        Hit((a[0] | a[1]) != 0);
        if (!std::memcmp(a, e, sizeof a)) return true;
        d = "name '" + std::string(name) + "'" + DiffDesc(a, e, sizeof a, "m");
        return false;
    }, 3000);
}
