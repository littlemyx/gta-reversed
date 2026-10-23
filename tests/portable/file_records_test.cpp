// Host test of source/game_sa/FileRecord.hpp + the typed CGenericGameStorage::Save/LoadDataFromWorkBuffer<T> dispatch (real GenericGameStorage.h, stubbed globals). Not wired into CMake.
//   tools/portable/run_file_records_test.sh            (prints fails=0; also compiles the negative cases below and requires every one to FAIL to compile)
// Negative cases (-DFR_NEG=n, with NOTSA_FILE_SCALAR_STRICT): 1 size_t prefix, 2 pointer, 3 long, 4 ptrdiff_t (GetIndex), 5 enum class : size_t, 6 array of pointers.
// Warning mode (default): the same hits compile and emit one [[deprecated]] diagnostic each (WARN_CASES below; the script counts them).
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <vector>

// ---- stubs for the engine types GenericGameStorage.h needs -------------------------------------------------------------------------------------------------
using uint8 = uint8_t; using uint16 = uint16_t; using uint32 = uint32_t; using uint64 = uint64_t;
using int8 = int8_t; using int16 = int16_t; using int32 = int32_t; using int64 = int64_t;
using GxtChar = uint16;
#define MAX_PATH 260
#define NOTSA_UNPAREN(...) __VA_ARGS__
#define NOTSA_GLOBAL(name, addr, type, ...) std::type_identity_t<NOTSA_UNPAREN type> name __VA_ARGS__
#ifdef FR_NEG
#define NOTSA_FILE_SCALAR_STRICT 1
#endif
#include "GenericGameStorage.h"

// in-memory "work buffer" instead of the engine's file-backed one
static std::vector<uint8> g_buf; static size_t g_pos = 0;
bool CGenericGameStorage::SaveDataToWorkBuffer(void* data, int32 size) { auto* p = (uint8*)data; g_buf.insert(g_buf.end(), p, p + size); return true; }
bool CGenericGameStorage::LoadDataFromWorkBuffer(void* data, int32 size) {
    if (g_pos + size > g_buf.size()) return false;
    std::memcpy(data, g_buf.data() + g_pos, size); g_pos += size; return true;
}
static void reset() { g_buf.clear(); g_pos = 0; }

// ---- a record shaped like the P-2 cases: a pointer inside a raw-dumped struct ----------------------------------------------------------------------------------
struct Fake { float rev; void* obj; uint32 ammo; uint8 type; uint8 flags; };       // 24 B on arm64, 16 B on x86 (obj = pointer)
struct Fake_File { float rev; uint32 obj; uint32 ammo; uint8 type; uint8 flags; uint8 pad[2]; };  // 16 B everywhere
static_assert(sizeof(Fake_File) == 16);
template<> struct notsa::FileRecord<Fake> {
    using Type = Fake_File;
    static Type ToFile(const Fake& f) { Type r{}; r.rev = f.rev; r.obj = f.obj ? 1u : 0u; r.ammo = f.ammo; r.type = f.type; r.flags = f.flags; return r; }
    static void FromFile(Fake& f, const Type& r) { f.rev = r.rev; f.obj = nullptr; f.ammo = r.ammo; f.type = r.type; f.flags = r.flags; }
};
// the plan's x86 equivalence proof (compiled in both builds; vacuous on x86 means: when pointers are 4 B the record IS the in-memory layout)
static_assert(sizeof(void*) != 4 || (sizeof(Fake_File) == sizeof(Fake) && offsetof(Fake_File, ammo) == offsetof(Fake, ammo)));

struct Plain { uint32 a; uint16 b; uint16 c; };   // no record: raw sizeof copy
struct Packed3 { uint8 x[3]; };

static int fails = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); fails++; } } while (0)

int main() {
    using namespace notsa;
    // trait
    static_assert(HasFileRecord<Fake> && HasFileRecord<const Fake&> && !HasFileRecord<Plain> && !HasFileRecord<int>);
    static_assert(FileSizeOf<Fake> == 16 && FileSizeOf<Plain> == sizeof(Plain) && FileSizeOf<const Fake&> == 16);
    static_assert(std::is_same_v<FileRecord<Fake>::Type, Fake_File>);

    // 64-bit scalar guard truth table
    static_assert(FileScalarOk<uint32> && FileScalarOk<int32> && FileScalarOk<float> && FileScalarOk<bool> && FileScalarOk<char> && FileScalarOk<uint8> && FileScalarOk<int16>);
    static_assert(FileScalarOk<long long> && FileScalarOk<unsigned long long> && FileScalarOk<double>);   // explicit 64-bit stays allowed (not size_t-sized by name)
    static_assert(FileScalarOk<Fake> && FileScalarOk<Plain> && FileScalarOk<uint32[4]>);                  // structs/arrays of fixed-width: not the guard's business
    enum class E8 : uint8 {}; enum E32 : uint32 {}; enum class ESz : size_t {};
    static_assert(FileScalarOk<E8> && FileScalarOk<E32>);
    if constexpr (sizeof(void*) == 8) {
        static_assert(!FileScalarOk<size_t> && !FileScalarOk<ptrdiff_t> && !FileScalarOk<intptr_t> && !FileScalarOk<uintptr_t> && !FileScalarOk<long> && !FileScalarOk<unsigned long>);
        static_assert(!FileScalarOk<void*> && !FileScalarOk<const char*> && !FileScalarOk<Fake*> && !FileScalarOk<void* [2]> && !FileScalarOk<ESz> && !FileScalarOk<void (*)()>);
        static_assert(!FileScalarOk<const size_t&>);
    }

    // typed Save/Load through the REAL CGenericGameStorage templates
    {
        reset();
        Fake f{ 1.5f, (void*)0x1234, 77u, 3, 9 };
        CHECK(CGenericGameStorage::SaveDataToWorkBuffer(f));
        CHECK(g_buf.size() == 16);   // file size, not sizeof(Fake)
        const uint8 golden[16] = { 0x00,0x00,0xC0,0x3F,  1,0,0,0,  77,0,0,0,  3, 9, 0, 0 };   // x86 little-endian: float 1.5, obj=1, ammo, type, flags, 2 pad
        CHECK(std::memcmp(g_buf.data(), golden, 16) == 0);
        Fake g{ 0, (void*)0x99, 0, 0, 0 };
        CHECK(CGenericGameStorage::LoadDataFromWorkBuffer(g));
        CHECK(g.rev == 1.5f && g.obj == nullptr && g.ammo == 77u && g.type == 3 && g.flags == 9 && g_pos == 16);
        reset();
        g_buf.assign(8, 0);            // truncated: Load must fail and leave the target alone
        Fake h{ 7.f, nullptr, 5, 1, 1 };
        CHECK(!CGenericGameStorage::LoadDataFromWorkBuffer(h) && h.rev == 7.f);
        reset();
        CGenericGameStorage::SaveDataToWorkBuffer(f);
        auto v = CGenericGameStorage::LoadDataFromWorkBuffer<Fake>();   // value-returning overload
        CHECK(v.ammo == 77u && v.obj == nullptr);
    }
    // no record => raw copy
    {
        reset();
        Plain p{ 0x11223344u, 0x5566, 0x7788 };
        CGenericGameStorage::SaveDataToWorkBuffer(p);
        CHECK(g_buf.size() == 8);
        Plain q{};
        CGenericGameStorage::LoadDataFromWorkBuffer(q);
        CHECK(q.a == p.a && q.b == p.b && q.c == p.c);
        reset();
        Packed3 x{ { 1, 2, 3 } };
        CGenericGameStorage::SaveDataToWorkBuffer(x);
        CHECK(g_buf.size() == 3);
    }
    // 32-bit prefixes (the P-2b fix shape) are exactly 4 bytes LE
    {
        reset();
        CGenericGameStorage::SaveDataToWorkBuffer<uint32>(sizeof(Fake_File));
        CHECK(g_buf.size() == 4 && g_buf[0] == 16 && g_buf[1] == 0);
        CHECK(CGenericGameStorage::LoadDataFromWorkBuffer<uint32>() == 16);
    }

#ifdef FR_WARN_CASES   // each line must produce exactly one -Wdeprecated-declarations warning (script counts 4 on LP64, 0 with -m32 style targets)
    reset();
    CGenericGameStorage::SaveDataToWorkBuffer(sizeof(Fake));                         // size_t prefix, the P-2b bug
    CGenericGameStorage::SaveDataToWorkBuffer(ptrdiff_t(5));                         // GetIndex()+1
    void* ptr = nullptr; CGenericGameStorage::SaveDataToWorkBuffer(ptr);
    unsigned long ul = 0; CGenericGameStorage::LoadDataFromWorkBuffer(ul);
    CHECK(g_buf.size() == 3 * 8);   // the broken 8-byte records the guard exists to prevent
#endif
#if defined(FR_NEG)
    reset();
#if FR_NEG == 1
    CGenericGameStorage::SaveDataToWorkBuffer(sizeof(Fake));
#elif FR_NEG == 2
    void* p = nullptr; CGenericGameStorage::SaveDataToWorkBuffer(p);
#elif FR_NEG == 3
    unsigned long l = 0; CGenericGameStorage::LoadDataFromWorkBuffer(l);
#elif FR_NEG == 4
    CGenericGameStorage::SaveDataToWorkBuffer(ptrdiff_t(1));
#elif FR_NEG == 5
    CGenericGameStorage::SaveDataToWorkBuffer(ESz{});
#elif FR_NEG == 6
    void* a[2] = {}; CGenericGameStorage::SaveDataToWorkBuffer(a);
#endif
#endif
    std::printf("fails=%d\n", fails);
    return fails ? 1 : 0;
}
