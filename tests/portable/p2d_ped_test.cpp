// P-2d host test (portable arm64): CWeapon_File (0x1C) / CPedSaveStructure_File (0x18C) and the FileRecord traits of CWeapon / CPedSaveStructure.
//   tools/portable/run_p2d_test.sh   (golden vectors come from tools/portable/gen_x86_golden.py, an independent generator; they are generated into $OUT/p2d_golden.inc)
// Real headers Weapon.h / PedSaveStructure.h / GenericGameStorage.h / FileRecord.hpp / FileRecords/PedFile.hpp on the tiny Base.h + rwplcore.h stub of tests/portable/p2c_stubs
// (StdInc is not linkable on the Mac yet). PedSaveStructure.h is told not to pull the whole CPed (NOTSA_P2D_HOSTTEST_NO_PED_H); CPedSaveStructure::Construct/Extract need CPed and are not
// exercised here (they are in the wave-3 oracle, unchanged by P-2d); this test covers everything that decides the BYTES: the records, the traits, the typed Save/Load, the prefix.
#include <Base.h>
#define MAX_PATH 260
#define NOTSA_GLOBAL(name, addr, type, ...) std::type_identity_t<NOTSA_UNPAREN type> name __VA_ARGS__
struct CColModel { uint8 bytes[48]; };      // Weapon.h has a `static inline CColModel ms_PelletTestCol` (complete type needed)
enum ePedPieceTypes { PED_PIECE_TORSO = 3 };  // Weapon.h forward-declares it without an underlying type (the real build has the definition from the enums before)
struct CEntity; struct CVehicle; struct CColPoint; struct CMatrix;
#include "Weapon.h"
#include "Entity/Ped/PedSaveStructure.h"
#include "GenericGameStorage.h"
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include "p2d_golden.inc"

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
static_assert(notsa::HasFileRecord<CWeapon> && notsa::HasFileRecord<CPedSaveStructure>);
static_assert(notsa::FileSizeOf<CWeapon> == 0x1C && notsa::FileSizeOf<CPedSaveStructure> == 0x18C);
static_assert(sizeof(CWeapon) == 0x20 && sizeof(CPedSaveStructure) > 0x18C, "LP64: the FxSystem_c* widens CWeapon (this is what the record hides)");
static_assert(sizeof(kGolden_CWeapon_File_basic) == 0x1C && sizeof(kGolden_CPedSaveStructure_File_basic) == 0x18C);
static_assert(std::is_same_v<notsa::FileRecord<CPedSaveStructure>::Type, CPedSaveStructure_File> && std::is_trivially_copyable_v<CPedSaveStructure_File>);

static CWeapon_File wrec(const uint8_t* g) { CWeapon_File r; std::memcpy(&r, g, sizeof(r)); return r; }
static CPedSaveStructure_File prec(const uint8_t* g) { CPedSaveStructure_File r; std::memcpy(&r, g, sizeof(r)); return r; }

// the slot values of the golden generator (gen_x86_golden.py CASES 'basic' of CPedSaveStructure_File)
static void expectSlot(const CWeapon& w, int k) {
    CHECK((uint32)w.m_Type == (k == 0 ? 0u : 10u + k) && (uint32)w.m_State == (uint32)(k % 5) && w.m_AmmoInClip == (uint32)k && w.m_TotalAmmo == (uint32)(100 * k));
    CHECK(w.m_TimeForNextShotMs == (uint32)(1000 + k) && w.m_IsFirstPersonWeaponModeSelected == bool(k & 1) && w.m_DontPlaceInHand == (k == 12) && w.m_FxSystem == nullptr);
}

int main() {
    // ---- CWeapon: golden file -> object -> file --------------------------------------------------------------------------------------------------------
    {
        CWeapon w; notsa::FileRecord<CWeapon>::FromFile(w, wrec(kGolden_CWeapon_File_basic));
        CHECK((uint32)w.m_Type == 30 && (uint32)w.m_State == 0 && w.m_AmmoInClip == 30 && w.m_TotalAmmo == 500 && w.m_TimeForNextShotMs == 0 && !w.m_IsFirstPersonWeaponModeSelected && w.m_DontPlaceInHand && w.m_FxSystem == nullptr);
        const auto out = notsa::FileRecord<CWeapon>::ToFile(w);
        CHECK(eq(&out, kGolden_CWeapon_File_basic, 0x1C));
        // the live fx pointer never reaches the file (exe wrote it, nothing read it back): slot is 0, pads are 0
        w.m_FxSystem = reinterpret_cast<FxSystem_c*>(uintptr_t(0x123456789A));
        const auto out2 = notsa::FileRecord<CWeapon>::ToFile(w);
        CHECK(out2.fxSystem == 0 && out2.pad16[0] == 0 && out2.pad16[1] == 0 && eq(&out2, kGolden_CWeapon_File_basic, 0x1C));
        // a file with a (garbage) pointer slot: FromFile ignores it
        auto in = wrec(kGolden_CWeapon_File_basic); in.fxSystem = 0xDEADBEEF; CWeapon w2; w2.m_FxSystem = w.m_FxSystem;
        notsa::FileRecord<CWeapon>::FromFile(w2, in); CHECK(w2.m_FxSystem == nullptr);
    }
    // ---- CPedSaveStructure: golden -> object -> golden ---------------------------------------------------------------------------------------------------
    {
        CPedSaveStructure s; notsa::FileRecord<CPedSaveStructure>::FromFile(s, prec(kGolden_CPedSaveStructure_File_basic));
        CHECK(s.m_pos.x == -2500.5f && s.m_pos.y == 1000.25f && s.m_pos.z == 12.125f && s.m_health == 100.f && s.m_armor == 50.5f);
        CHECK((uint8)s.m_createdBy == 2 && s.m_activeWeaponSlot == 5 && s.m_areaCode == 1 && s.m_nExitIndex == -1 && s.m_nFightingStyle == 6 && s.m_nAllowedAttackMoves == 0x1F);
        for (int k = 0; k < 13; k++) expectSlot(s.m_weapons[k], k);
        const auto out = notsa::FileRecord<CPedSaveStructure>::ToFile(s);
        CHECK(sizeof(out) == 0x18C && eq(&out, kGolden_CPedSaveStructure_File_basic, 0x18C));

        // typed Save: exactly the 0x18C record on disk (the LP64 object is bigger), identical to the golden
        reset(); CHECK(CGenericGameStorage::SaveDataToWorkBuffer(s)); CHECK(g_buf.size() == 0x18C && eq(g_buf.data(), kGolden_CPedSaveStructure_File_basic, 0x18C));
        // CPed::Save stream = [uint32 prefix][record]; the prefix is the FILE size, the loader's assert compares the same constant
        reset(); CGenericGameStorage::SaveDataToWorkBuffer<uint32>(notsa::FileSizeOf<CPedSaveStructure>); CGenericGameStorage::SaveDataToWorkBuffer(s);
        CHECK(g_buf.size() == 4 + 0x18C && g_buf[0] == 0x8C && g_buf[1] == 0x01 && g_buf[2] == 0 && g_buf[3] == 0 && eq(g_buf.data() + 4, kGolden_CPedSaveStructure_File_basic, 0x18C));
        // ... and what CPed::Load does with it
        {
            const auto size = CGenericGameStorage::LoadDataFromWorkBuffer<uint32>(); CHECK(size == notsa::FileSizeOf<CPedSaveStructure>);
            CPedSaveStructure l; CHECK(CGenericGameStorage::LoadDataFromWorkBuffer(l)); CHECK(g_pos == g_buf.size());
            for (int k = 0; k < 13; k++) expectSlot(l.m_weapons[k], k);
        }
        // an exe-written save (garbage in the pointer slots and pads) loads: pointer slots ignored
        {
            reset(); g_buf.assign(kGolden_CPedSaveStructure_File_basic, kGolden_CPedSaveStructure_File_basic + 0x18C);
            for (int k = 0; k < 13; k++) { uint32_t junk = 0xCAFE0000u + k; std::memcpy(&g_buf[0x14 + 0x1C * k + 0x18], &junk, 4); g_buf[0x14 + 0x1C * k + 0x16] = 0x77; }
            g_buf[0x183] = 0x55; g_buf[0x18A] = 0x66;
            CPedSaveStructure l; CHECK(CGenericGameStorage::LoadDataFromWorkBuffer(l));
            for (int k = 0; k < 13; k++) expectSlot(l.m_weapons[k], k);
            reset(); CGenericGameStorage::SaveDataToWorkBuffer(l); CHECK(eq(g_buf.data(), kGolden_CPedSaveStructure_File_basic, 0x18C));   // junk normalised to 0 on re-save
        }
        // truncated stream fails (same contract as the raw path)
        reset(); g_buf.assign(kGolden_CPedSaveStructure_File_basic, kGolden_CPedSaveStructure_File_basic + 0x18B); { CPedSaveStructure l; CHECK(!CGenericGameStorage::LoadDataFromWorkBuffer(l)); }
    }
    // ---- 13-slot round trip keeps Type / Ammo / AmmoInClip (and every other member), 2000 random objects ------------------------------------------------------
    {
        std::mt19937 rng(0xBEEF); int bad = 0;
        for (int i = 0; i < 2000; ++i) {
            CPedSaveStructure a, b;
            a.m_pos = CVector{float(rng() % 10000) - 5000.f, float(rng() % 10000) / 7.f, float(rng() % 100)}; a.m_health = float(rng() % 1000) / 4.f; a.m_armor = float(rng() % 200);
            for (auto& w : a.m_weapons) {
                w.m_Type = (eWeaponType)(rng() % 47); w.m_State = (eWeaponState)(rng() % 5); w.m_AmmoInClip = rng(); w.m_TotalAmmo = rng(); w.m_TimeForNextShotMs = rng();
                w.m_IsFirstPersonWeaponModeSelected = rng() & 1; w.m_DontPlaceInHand = rng() & 1; w.m_FxSystem = reinterpret_cast<FxSystem_c*>(uintptr_t(rng()) << 12);
            }
            a.m_createdBy = (ePedCreatedBy)(rng() % 4); a.m_activeWeaponSlot = (int8)(rng() % 13); a.m_areaCode = (int8)(rng() % 22); a.m_nExitIndex = (int32)(rng() % 50) - 1;
            a.m_nFightingStyle = (uint8)(4 + rng() % 7); a.m_nAllowedAttackMoves = (uint8)rng();
            reset(); CGenericGameStorage::SaveDataToWorkBuffer(a); if (g_buf.size() != 0x18C) bad++;
            // every pointer slot is 0 on disk
            for (int k = 0; k < 13; k++) { uint32_t fx; std::memcpy(&fx, &g_buf[0x14 + 0x1C * k + 0x18], 4); if (fx != 0) bad++; }
            if (!CGenericGameStorage::LoadDataFromWorkBuffer(b)) bad++;
            for (int k = 0; k < 13; k++) {
                const auto &x = a.m_weapons[k], &y = b.m_weapons[k];
                if (x.m_Type != y.m_Type || x.m_State != y.m_State || x.m_AmmoInClip != y.m_AmmoInClip || x.m_TotalAmmo != y.m_TotalAmmo || x.m_TimeForNextShotMs != y.m_TimeForNextShotMs
                    || x.m_IsFirstPersonWeaponModeSelected != y.m_IsFirstPersonWeaponModeSelected || x.m_DontPlaceInHand != y.m_DontPlaceInHand || y.m_FxSystem != nullptr) bad++;
            }
            if (a.m_pos.x != b.m_pos.x || a.m_pos.y != b.m_pos.y || a.m_pos.z != b.m_pos.z || a.m_health != b.m_health || a.m_armor != b.m_armor || a.m_createdBy != b.m_createdBy
                || a.m_activeWeaponSlot != b.m_activeWeaponSlot || a.m_areaCode != b.m_areaCode || a.m_nExitIndex != b.m_nExitIndex || a.m_nFightingStyle != b.m_nFightingStyle
                || a.m_nAllowedAttackMoves != b.m_nAllowedAttackMoves) bad++;
            // second save of the loaded object is byte-equal
            auto first = g_buf; reset(); CGenericGameStorage::SaveDataToWorkBuffer(b); if (g_buf != first) bad++;
        }
        CHECK(bad == 0);
    }
    // ---- source scan: Ped.cpp prefix + assert use the FILE size, the raw sizeof(save) is gone ----------------------------------------------------------------
#define P2D_STR2(x) #x
#define P2D_STR(x) P2D_STR2(x)
    {
        auto slurp = [](const std::string& p) { std::ifstream in(p); std::stringstream ss; ss << in.rdbuf(); return ss.str(); };
        const std::string repo = P2D_STR(REPO_DIR);
        const auto ped = slurp(repo + "/source/game_sa/Entity/Ped/Ped.cpp");
        const auto lPos = ped.find("bool CPed::Load()"), sPos = ped.find("bool CPed::Save()"), ePos = ped.find("bool CPed::PedIsInvolvedInConversation()");
        CHECK(lPos != std::string::npos && sPos != std::string::npos && ePos != std::string::npos && lPos < sPos && sPos < ePos);
        std::string body; { std::istringstream ls(ped.substr(lPos, ePos - lPos)); std::string l; while (std::getline(ls, l)) { body += l.substr(0, l.find("//")); body += '\n'; } }   // comments stripped
        CHECK(body.find("sizeof(save)") == std::string::npos);
        CHECK(body.find("assert(size == notsa::FileSizeOf<CPedSaveStructure>)") != std::string::npos);
        CHECK(body.find("SaveDataToWorkBuffer<uint32>(notsa::FileSizeOf<CPedSaveStructure>)") != std::string::npos);
        CHECK(body.find("SaveDataToWorkBuffer(save);") != std::string::npos && body.find("LoadDataFromWorkBuffer(save);") != std::string::npos);
    }
    std::printf(fails ? "p2d FAILED (%d)\n" : "p2d host test: ALL OK (fails=%d)\n", fails);
    return fails ? 1 : 0;
}
