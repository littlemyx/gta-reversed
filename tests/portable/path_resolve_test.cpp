// MAC-BOOT1 / W-7 host test: source/portable/path_resolve.cpp (the portable path layer) on a scratch directory tree.
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

#define NOTSA_PORTABLE 1
#include "../../source/portable/path_resolve.h"
#include "../../source/portable/path_resolve.cpp"

namespace fs = std::filesystem;
using namespace notsa::portable;

static int g_fail = 0, g_n = 0;
#define CHECK(c) do { ++g_n; if (!(c)) { ++g_fail; std::printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
#define CHECK_EQ(a, b) do { ++g_n; const std::string _a = (a), _b = (b); if (_a != _b) { ++g_fail; std::printf("FAIL line %d: %s -> '%s' != '%s'\n", __LINE__, #a, _a.c_str(), _b.c_str()); } } while (0)

static void Touch(const fs::path& p, const char* content = "x") { fs::create_directories(p.parent_path()); std::ofstream(p) << content; }

int main() {
    const fs::path root = fs::canonical(fs::temp_directory_path()) / ("pathres_" + std::to_string(::getpid()));
    fs::remove_all(root);
    fs::create_directories(root);
    Touch(root / "models" / "coll" / "Peds.COL", "peds");
    Touch(root / "Data" / "Paths" / "NODES0.DAT");
    Touch(root / "AUDIO" / "CONFIG" / "BankLkup.dat");
    Touch(root / "gta3.IMG");
    const std::string R = root.string();

    // --- backslashes + mixed case, relative to a base dir
    CHECK_EQ(ResolvePathFrom(R, "models\\coll\\peds.col"), R + "/models/coll/Peds.COL");
    CHECK_EQ(ResolvePathFrom(R, "MODELS\\COLL\\PEDS.COL"), R + "/models/coll/Peds.COL");
    CHECK_EQ(ResolvePathFrom(R, "data/PATHS/nodes0.dat"), R + "/Data/Paths/NODES0.DAT");
    CHECK_EQ(ResolvePathFrom(R, "audio\\config\\banklkup.DAT"), R + "/AUDIO/CONFIG/BankLkup.dat");
    CHECK_EQ(ResolvePathFrom(R, "GTA3.img"), R + "/gta3.IMG");
    CHECK_EQ(ResolvePathFrom(R, "MODELS\\GTA3.IMG"), R + "/models/GTA3.IMG"); // models/ exists, the file is missing: spelling kept from there on
    // --- missing: kept verbatim (also deeper components), no exception
    CHECK_EQ(ResolvePathFrom(R, "nodir\\Sub\\File.TXT"), R + "/nodir/Sub/File.TXT");
    CHECK_EQ(ResolvePathFrom(R, "models\\NEW\\Deep\\x.bin"), R + "/models/NEW/Deep/x.bin");
    // --- '..' , '.', '//' and trailing separators
    CHECK_EQ(ResolvePathFrom(R, "MODELS\\coll\\..\\COLL\\peds.col"), R + "/models/coll/../coll/Peds.COL");
    CHECK_EQ(ResolvePathFrom(R, ".\\models\\.\\coll"), R + "/models/coll");
    CHECK_EQ(ResolvePathFrom(R, "models//coll\\\\"), R + "/models/coll/");
    CHECK_EQ(ResolvePathFrom(R, "DATA\\"), R + "/Data/");
    CHECK_EQ(ResolvePathFrom("", ""), "");
    CHECK_EQ(ResolvePathFrom("", "."), ".");
    CHECK_EQ(ResolvePathFrom(R, "models\\Zz\\..\\coll\\peds.col"), R + "/models/Zz/../coll/peds.col"); // after a missing component: verbatim
    // --- Win32 trailing space / dot normalisation
    CHECK_EQ(ResolvePathFrom(R, "models\\coll\\peds.col  "), R + "/models/coll/Peds.COL");
    CHECK_EQ(ResolvePathFrom(R, "models. \\COLL.\\peds.col."), R + "/models/coll/Peds.COL");
    CHECK_EQ(ResolvePathFrom(R, "new thing .txt "), R + "/new thing .txt");
    CHECK_EQ(ResolvePathFrom(R, "models\\.. \\x"), R + "/models/x"); // ".. " is empty after trimming: dropped
    // --- absolute path (resolved from "/")
    CHECK_EQ(ResolvePath(R + "/MODELS/COLL/peds.col"), R + "/models/coll/Peds.COL");
    // --- cwd-relative semantics (the exe's ChangeDir/SetDir chdir)
    const fs::path oldCwd = fs::current_path();
    CHECK(ChdirResolved((R + "/DATA").c_str()) == 0);
    CHECK_EQ(ResolvePath("paths\\nodes0.dat"), "Paths/NODES0.DAT");
    CHECK(ExistsResolved("PATHS\\NODES0.DAT"));
    CHECK(!ExistsResolved("PATHS\\NODES1.DAT"));
    CHECK(ChdirResolved(".\\..\\MODELS\\Coll") == 0);
    CHECK(fs::current_path() == root / "models" / "coll");
    CHECK(ChdirResolved(R.c_str()) == 0);
    CHECK(ChdirResolved("nope\\x") != 0);
    // --- fopen
    {
        std::FILE* f = OpenResolved("Models\\Coll\\PEDS.col", "rb");
        CHECK(f != nullptr);
        char b[8]{};
        if (f) { CHECK(std::fread(b, 1, 4, f) == 4 && !std::memcmp(b, "peds", 4)); std::fclose(f); }
        void* rf = RwFopenResolved("MODELS/COLL/peds.col", "rb");
        CHECK(rf != nullptr);
        if (rf) std::fclose((std::FILE*)rf);
        CHECK(OpenResolved("models\\coll\\nothere.col", "rb") == nullptr);
        CHECK(OpenResolved(nullptr, "rb") == nullptr);
    }
    // --- text mode (MSVC CRT): "r" translates CRLF and stops at Ctrl-Z; "rb" does not
    {
        Touch(root / "data" / "txt.dat", "a\r\n\r\nb\rc\r\n\x1a" "hidden");
        std::FILE* f = OpenResolvedText("DATA\\TXT.DAT", "r");
        CHECK(f != nullptr);
        if (f) {
            char l[16];
            CHECK(std::fgets(l, sizeof l, f) && !std::strcmp(l, "a\n"));
            CHECK(std::ftell(f) == 2);
            CHECK(std::fgets(l, sizeof l, f) && !std::strcmp(l, "\n"));
            CHECK(std::fgets(l, sizeof l, f) && !std::strcmp(l, "b\rc\n"));
            CHECK(std::fgets(l, sizeof l, f) == nullptr && std::feof(f));
            std::fseek(f, 0, SEEK_END); CHECK(std::ftell(f) == 7);
            std::fseek(f, 2, SEEK_SET); CHECK(std::fgetc(f) == '\n');
            std::fseek(f, -2, SEEK_END); CHECK(std::fgetc(f) == 'c');
            std::rewind(f); int a = 0; char w[8]; CHECK(std::fscanf(f, "%7s %d", w, &a) == 1 && !std::strcmp(w, "a"));
            std::fclose(f);
        }
        f = OpenResolvedText("data\\txt.dat", "rt"); CHECK(f != nullptr); if (f) { CHECK(std::fgetc(f) == 'a' && std::fgetc(f) == '\n'); std::fclose(f); }
        f = OpenResolvedText("data\\txt.dat", "rb"); CHECK(f != nullptr); if (f) { CHECK(std::fgetc(f) == 'a' && std::fgetc(f) == '\r'); std::fclose(f); }
        f = OpenResolvedText("data\\txt.dat", "r+"); CHECK(f != nullptr); if (f) { CHECK(std::fgetc(f) == 'a' && std::fgetc(f) == '\r'); std::fclose(f); }
        CHECK(OpenResolvedText("data\\nothere.dat", "r") == nullptr);
        Touch(root / "data" / "empty.dat", "");
        f = OpenResolvedText("data\\empty.dat", "r"); CHECK(f != nullptr); if (f) { CHECK(std::fgetc(f) == EOF); std::fclose(f); }
    }
    // --- write modes: existing file keeps its real spelling (no second file), new file gets the game's spelling in the real directory
    {
        std::FILE* f = OpenResolved("MODELS\\COLL\\peds.col", "ab");
        CHECK(f != nullptr);
        if (f) { std::fputs("+", f); std::fclose(f); }
        f = OpenResolved("DATA\\paths\\Fresh.TXT", "wb");
        CHECK(f != nullptr);
        if (f) { std::fputs("n", f); std::fclose(f); }
        CHECK(fs::exists(root / "Data" / "Paths" / "Fresh.TXT"));
        CHECK(fs::exists(root / "models" / "coll" / "Peds.COL"));
        size_t cnt = 0; for (auto& e : fs::directory_iterator(root / "models" / "coll")) { (void)e; ++cnt; }
        CHECK(cnt == 1);
    }
    // --- the directory cache notices files created behind its back (mtime) and removed ones
    {
        CHECK_EQ(ResolvePath("DATA\\paths\\late.dat"), "Data/Paths/late.dat"); // missing: tail verbatim
        Touch(root / "Data" / "Paths" / "LATE.DAT");
        CHECK_EQ(ResolvePath("data\\paths\\late.dat"), "Data/Paths/LATE.DAT");
        fs::remove(root / "Data" / "Paths" / "LATE.DAT");
        CHECK(!ExistsResolved("data\\paths\\late.dat"));
    }
    // --- cache cleared == same answers
    ClearPathCache();
    CHECK_EQ(ResolvePath("audio\\CONFIG\\BANKLKUP.dat"), "AUDIO/CONFIG/BankLkup.dat");
    // --- threads (the CdStream thread resolves too)
    {
        std::vector<std::thread> ts;
        std::atomic<int> bad{0};
        for (int t = 0; t < 4; t++) ts.emplace_back([&] { for (int i = 0; i < 500; i++) if (ResolvePath("MODELS\\COLL\\PEDS.COL") != "models/coll/Peds.COL") bad++; });
        for (auto& t : ts) t.join();
        CHECK(bad == 0);
    }
    fs::current_path(oldCwd);
    fs::remove_all(root);
    std::printf("%d checks, %d failed\n", g_n, g_fail);
    std::printf(g_fail ? "RESULT PATH_RESOLVE FAILED\n" : "RESULT PATH_RESOLVE OK\n");
    return g_fail ? 1 : 0;
}
