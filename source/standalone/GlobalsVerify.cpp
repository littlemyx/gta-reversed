#include "StdInc.h"

// NOTSA_VERIFY_GLOBALS, see GlobalsVerify.h. Compiled to nothing unless the option is on.
#if defined(NOTSA_VERIFY_GLOBALS) && defined(NOTSA_STANDALONE_RUN)
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include "GlobalsVerify.h"
#include "GlobalsVerifyTables.h"
#include "Fixups.h"

namespace notsa::globals {
namespace {
struct Entry {
    const char* Name;
    const char* File;
    int         Line;
    uint32_t    Addr;
    size_t      Size;
    const void* Storage;
};

struct TableEntry {
    const char*  Name;
    const char*  File;
    int          Line;
    TableCheckFn Fn;
};

std::vector<TableEntry>& TableRegistry() {
    static std::vector<TableEntry> s_Tables;
    return s_Tables;
}

std::vector<Entry>& Registry() {
    static std::vector<Entry> s_Registry; // function-local: the registrars run during other TUs' dynamic initialisation
    return s_Registry;
}

void Hex(FILE* f, const uint8_t* p, size_t n) {
    static const char digits[] = "0123456789abcdef";
    char              buf[2];
    for (size_t i = 0; i < n; i++) {
        buf[0] = digits[p[i] >> 4];
        buf[1] = digits[p[i] & 15];
        fwrite(buf, 1, 2, f);
    }
}

void Dump(const char* path, const char* tag) {
    FILE* f = fopen(path, "wb");
    if (!f) {
        notsa::standalone::Fixups::Log("NOTSA_VERIFY_GLOBALS: cannot open '%s'", path);
        return;
    }
    // Name/file:line contain no tab. Columns: addr, size, name, file:line, our bytes (function pointers are our own addresses: verify_globals.py names them through the linker .map)
    fprintf(f, "# NOTSA_VERIFY_GLOBALS %s: addr\tsize\tname\tfile:line\tours\n", tag);
    for (const auto& e : Registry()) {
        fprintf(f, "0x%08X\t%u\t%s\t%s:%d\t", e.Addr, (unsigned)e.Size, e.Name, e.File, e.Line);
        Hex(f, static_cast<const uint8_t*>(e.Storage), e.Size);
        fputc('\n', f);
    }
    fclose(f);
    notsa::standalone::Fixups::Log("NOTSA_VERIFY_GLOBALS: wrote %u globals (%s) to '%s'", (unsigned)Registry().size(), tag, path);
}
} // namespace

TableCheck::TableCheck(const char* name, const char* file, int line, TableCheckFn fn) noexcept {
    TableRegistry().push_back({ name, file, line, fn });
}

const uint8_t* PristineImage(uint32_t va) {
    static std::vector<uint8_t> s_Image;
    static bool                 s_Tried = false;
    if (!s_Tried) {
        s_Tried = true;
        char  path[MAX_PATH];
        DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
        while (len > 0 && path[len - 1] != '\\') {
            len--;
        }
        lstrcpyA(path + len, "original_data.bin");
        if (FILE* f = fopen(path, "rb")) {
            s_Image.resize(0x1000000);
            s_Image.resize(fread(s_Image.data(), 1, s_Image.size(), f));
            fclose(f);
        }
    }
    constexpr uint32_t kBase = 0x858000;
    return (va >= kBase && va - kBase < s_Image.size()) ? s_Image.data() + (va - kBase) : nullptr;
}

namespace {
//! Run the D2 table checks and append their results to the dump file (verify_globals.py reads the `# TABLE` lines)
void RunTableChecks(const char* path) {
    FILE* f = fopen(path, "ab");
    size_t ok = 0, bad = 0;
    for (const auto& t : TableRegistry()) {
        char why[256] = "";
        const bool same = t.Fn(why, sizeof(why));
        (same ? ok : bad) += 1;
        if (f) {
            fprintf(f, "# TABLE\t%s\t%s:%d\t%s\t%s\n", t.Name, t.File, t.Line, same ? "OK" : "FAIL", why);
        }
        if (!same) {
            notsa::standalone::Fixups::Log("NOTSA_VERIFY_GLOBALS: table %s DIFFERS from the image: %s", t.Name, why);
        }
    }
    if (f) {
        fclose(f);
    }
    notsa::standalone::Fixups::Log("NOTSA_VERIFY_GLOBALS: %u table checks, %u ok, %u differ", (unsigned)(ok + bad), (unsigned)ok, (unsigned)bad);
}
} // namespace

Reg::Reg(const char* name, const char* file, int line, uint32_t exeAddr, size_t size, const void* storage) noexcept {
    Registry().push_back({ name, file, line, exeAddr, size, storage });
}

void DumpAtBoot() {
    const char* path = std::getenv("NOTSA_VERIFY_GLOBALS");
    if (!path || !*path) {
        return;
    }
    Dump(path, "boot");
    RunTableChecks(path);
    if (const char* after = std::getenv("NOTSA_VERIFY_GLOBALS_AFTER")) {
        const int secs = std::atoi(after);
        if (secs > 0) {
            static char s_LatePath[512];
            snprintf(s_LatePath, sizeof(s_LatePath), "%s.late", path);
            std::thread([secs] {
                Sleep(secs * 1000);
                Dump(s_LatePath, "late"); // racy against the game thread by design: an informational snapshot
            }).detach();
        }
    }
    if (const char* ex = std::getenv("NOTSA_VERIFY_GLOBALS_EXIT"); ex && *ex == '1') {
        notsa::standalone::Fixups::Log("NOTSA_VERIFY_GLOBALS_EXIT=1: exiting after the boot dump");
        std::exit(0);
    }
}
} // namespace notsa::globals
#endif
