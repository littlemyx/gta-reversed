// P4-S4 host test: the REAL source/game_sa/Text/KeyArray.cpp (TKEY chunk of a .gxt: 8 byte {offset, hash} records) on the stub tests/portable/p4_stubs/StdInc.h.
//   tools/portable/run_p4_keyarray_test.sh  (`mutate`: the pre-fix `length / sizeof(CKeyEntry)` / raw-chunk-read mutants must FAIL this test)
// The chunk is built here, independently of the code under test: records sorted by hash (the game binary-searches), strings in a TDAT-like block, offsets relative to it.
#include "StdInc.h"
#include "GxtChar.h"
#include "Text/KeyArray.h"
#include "Text/KeyArray.cpp"   // the unit under test
#include <string>
#include <map>

static int g_fails = 0;
#define CHECK(c, ...) do { if (!(c)) { g_fails++; std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

int main() {
    // ---- build the chunk
    const char* names[] = { "T1_A", "FEM_OK", "MAIN_1", "ZZ_LAST", "A", "CAR_1", "HELLO", "WORLD_9" };
    std::map<uint32_t, std::string> byHash; std::vector<uint8_t> tdat; std::map<uint32_t, uint32_t> offsetOf;
    for (auto* n : names) {
        const uint32_t h = CKeyGen::GetUppercaseKey(n);
        std::string text = std::string("text of ") + n;
        offsetOf[h] = (uint32_t)tdat.size();
        tdat.insert(tdat.end(), text.begin(), text.end()); tdat.push_back(0);
        byHash[h] = text;
    }
    std::vector<uint8_t> chunk;                         // records in ascending hash order (std::map order), little endian
    for (auto& [h, off] : offsetOf) {
        const uint32_t rec[2] = { off, h };
        chunk.insert(chunk.end(), (const uint8_t*)rec, (const uint8_t*)rec + 8);
    }
    chunk.insert(chunk.end(), 4, 0xAA); // NOT a record: the exe would count length/8 entries; trailing slack must be consumed from the file but not become an entry
    CHECK(chunk.size() % 8 == 4, "test setup");
    const uint32_t nRec = (uint32_t)(chunk.size() / 8);

    FILE* f = std::tmpfile();
    CHECK(f != nullptr, "tmpfile");
    // some bytes before the chunk and after it: Load must consume exactly `length` bytes
    const uint8_t pre[3] = { 1, 2, 3 }, post[3] = { 9, 8, 7 };
    std::fwrite(pre, 1, 3, f); std::fwrite(chunk.data(), 1, chunk.size(), f); std::fwrite(post, 1, 3, f);
    std::rewind(f); std::fseek(f, 3, SEEK_SET);

    CKeyArray ka;
    uint32_t offset = 100;
    CHECK(ka.Load((uint32_t)chunk.size(), f, &offset, 0), "Load");
    CHECK(ka.m_size == nRec, "entry count %u want %u (length/8: the 16 byte in-memory CKeyEntry must not decide it)", ka.m_size, nRec);
    CHECK(offset == 100 + chunk.size(), "offset advanced %u", offset);
    uint8_t after[3] = {}; CHECK(std::fread(after, 1, 3, f) == 3 && !std::memcmp(after, post, 3), "file position after Load is exactly after the chunk");
    for (uint32_t i = 1; i < ka.m_size; i++) CHECK(ka.m_data[i - 1].hash < ka.m_data[i].hash, "records %u: hash order broken (record layout misread)", i);
    uint32_t k = 0; for (auto& [h, off] : offsetOf) { CHECK(ka.m_data[k].hash == h, "hash[%u] %08x want %08x", k, ka.m_data[k].hash, h); k++; }

    // ---- Update: offset -> pointer into the TDAT block; Search by key
    ka.Update(reinterpret_cast<char*>(tdat.data()));
    int found = 0;
    for (auto* n : names) {
        bool ok = false; const GxtChar* s = ka.Search(n, ok);
        CHECK(ok && s, "Search(%s) not found", n);
        if (ok && s) { CHECK(std::string((const char*)s) == byHash[CKeyGen::GetUppercaseKey(n)], "Search(%s) -> '%s'", n, (const char*)s); found++; }
    }
    bool ok = true; CHECK(ka.Search("NOT_THERE", ok) == nullptr && !ok, "missing key must not be found");
    { bool ok2 = false; const GxtChar* s = ka.Search("hello", ok2); CHECK(ok2 && s && std::string((const char*)s) == "text of HELLO", "key search is case-insensitive"); } // GetUppercaseKey
    ka.Unload();
    CHECK(ka.m_data == nullptr && ka.m_size == 0, "Unload");
    std::fclose(f);
    std::printf("p4 keyarray test: found %d/%zu, %s (fails=%d)\n", found, sizeof(names) / sizeof(*names), g_fails ? "FAILED" : "ALL OK", g_fails);
    return g_fails != 0;
}
