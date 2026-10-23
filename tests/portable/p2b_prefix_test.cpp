// P-2b host test: the 10 fixed save-game writes emit 4-byte little-endian scalars, the sources contain no unguarded 64-bit write any more, SystemTime/GetLocalTime.
//   tools/portable/run_p2b_test.sh  (compiled with -DNOTSA_FILE_SCALAR_STRICT: a size_t / ptrdiff_t write would not even compile)
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <type_traits>

// ---- stubs for the engine types GenericGameStorage.h needs -------------------------------------------------------------------------------------------------
using uint8 = uint8_t; using uint16 = uint16_t; using uint32 = uint32_t; using uint64 = uint64_t;
using int8 = int8_t; using int16 = int16_t; using int32 = int32_t; using int64 = int64_t;
using GxtChar = uint16;
#define MAX_PATH 260
#define NOTSA_UNPAREN(...) __VA_ARGS__
#define NOTSA_GLOBAL(name, addr, type, ...) std::type_identity_t<NOTSA_UNPAREN type> name __VA_ARGS__
#include "GenericGameStorage.h"

// in-memory "work buffer" instead of the engine's file-backed one
static std::vector<uint8> g_buf; static size_t g_pos = 0;
bool CGenericGameStorage::SaveDataToWorkBuffer(void* data, int32 size) { auto* p = (uint8*)data; g_buf.insert(g_buf.end(), p, p + size); return true; }
bool CGenericGameStorage::LoadDataFromWorkBuffer(void* data, int32 size) {
    if (g_pos + size > g_buf.size()) return false;
    std::memcpy(data, g_buf.data() + g_pos, size); g_pos += size; return true;
}
static void reset() { g_buf.clear(); g_pos = 0; }

#include "portable/systemtime.h"

static int fails = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); fails++; } } while (0)

static bool bytesAre(std::initializer_list<uint8> v) { return g_buf.size() == v.size() && std::equal(v.begin(), v.end(), g_buf.begin()); }

int main() {
    // (a) sizeof prefixes (GangWars/PlayerInfo/PlayerPed/Ped/Object): `SaveDataToWorkBuffer<uint32>(sizeof(X))`; X = 0x18C, a typical record size
    reset(); CGenericGameStorage::SaveDataToWorkBuffer<uint32>(sizeof(std::array<uint8, 0x18C>));
    CHECK(bytesAre({ 0x8C, 0x01, 0x00, 0x00 }));
    // (b) GetIndex()+1 (ptrdiff_t) in TheScripts.cpp
    { std::ptrdiff_t idx = 0x1234; reset(); CGenericGameStorage::SaveDataToWorkBuffer<uint32>(idx + 1); CHECK(bytesAre({ 0x35, 0x12, 0x00, 0x00 })); }
    // (c) IplStore::Save count (size_t -> int32; the loader reads int32)
    { size_t num = 0x100; reset(); CGenericGameStorage::SaveDataToWorkBuffer<int32>(num); CHECK(bytesAre({ 0x00, 0x01, 0x00, 0x00 })); }
    // the write is read back by the loader's type
    { reset(); CGenericGameStorage::SaveDataToWorkBuffer<uint32>(sizeof(std::array<uint8, 0x68>)); g_pos = 0; CHECK(CGenericGameStorage::LoadDataFromWorkBuffer<uint32>() == 0x68); CHECK(g_pos == 4); }

    // (d) source scan: no `SaveDataToWorkBuffer(sizeof(...))` / `SaveDataToWorkBuffer(<pool>->GetIndex(..) ..)` / `SaveDataToWorkBuffer(num)` left in the fixed files
    #define P2B_STR2(x) #x
#define P2B_STR(x) P2B_STR2(x)
    const std::string repo = P2B_STR(REPO_DIR);
    const char* files[] = { "GangWars.cpp", "PlayerInfo.cpp", "Entity/Ped/PlayerPed.cpp", "Entity/Ped/Ped.cpp", "Entity/Object/Object.cpp", "Scripts/TheScripts.cpp", "IplStore.cpp" };
    const std::regex bad(R"(SaveDataToWorkBuffer\(\s*(sizeof\b|[A-Za-z_>()-]*GetIndex\b|num\s*\)))");
    const std::regex good(R"(SaveDataToWorkBuffer<(uint32|int32)>\()");
    int goodHits = 0;
    for (auto* f : files) {
        std::ifstream in(repo + "/source/game_sa/" + f); CHECK(in.good());
        std::stringstream ss; ss << in.rdbuf(); const auto t = ss.str();
        if (std::regex_search(t, bad)) { std::printf("FAIL: unguarded write left in %s\n", f); fails++; }
        for (auto it = std::sregex_iterator(t.begin(), t.end(), good); it != std::sregex_iterator(); ++it) goodHits++;
    }
    CHECK(goodHits == 10);   // 5 prefixes + 4 GetIndex + IplStore

    // (e) SYSTEMTIME / GetLocalTime
    static_assert(sizeof(notsa::SystemTime) == 16 && offsetof(notsa::SystemTime, wMilliseconds) == 14);
    notsa::SystemTime st{}; GetLocalTime(&st);   // ADL, as in SimpleVariablesSaveStructure.cpp
    CHECK(st.wYear >= 2024 && st.wMonth >= 1 && st.wMonth <= 12 && st.wDay >= 1 && st.wDay <= 31 && st.wDayOfWeek <= 6);
    CHECK(st.wHour <= 23 && st.wMinute <= 59 && st.wSecond <= 60 && st.wMilliseconds <= 999);

    std::printf("p2b prefix test: fails=%d\n", fails);
    return fails != 0;
}
