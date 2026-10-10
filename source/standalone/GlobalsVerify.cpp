#include "StdInc.h"

// NOTSA_VERIFY_GLOBALS, see GlobalsVerify.h. Compiled to nothing unless the option is on.
#if defined(NOTSA_VERIFY_GLOBALS) && defined(NOTSA_STANDALONE_RUN)
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include "GlobalsVerify.h"
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
    // Name/file:line contain no tab. `img` = the data image at the ORIGINAL address (after Fixups::ApplyToDataImage: code pointers replaced by ours / trap stubs):
    // detached globals no longer read it, so it is still the state the original code would have found there.
    fprintf(f, "# NOTSA_VERIFY_GLOBALS %s: addr\tsize\tname\tfile:line\tours\timage\n", tag);
    for (const auto& e : Registry()) {
        fprintf(f, "0x%08X\t%u\t%s\t%s:%d\t", e.Addr, (unsigned)e.Size, e.Name, e.File, e.Line);
        Hex(f, static_cast<const uint8_t*>(e.Storage), e.Size);
        fputc('\t', f);
        Hex(f, reinterpret_cast<const uint8_t*>((uintptr_t)e.Addr), e.Size);
        fputc('\n', f);
    }
    fclose(f);
    notsa::standalone::Fixups::Log("NOTSA_VERIFY_GLOBALS: wrote %u globals (%s) to '%s'", (unsigned)Registry().size(), tag, path);
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
