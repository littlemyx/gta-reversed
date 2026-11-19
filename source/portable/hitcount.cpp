// T3H GTASA_HITCOUNT: function hit counter. Every function of source/game_sa/** is compiled with /Gh (call _penter at entry, see hitcount_penter.asm);
// the hit is keyed by the return address inside the function. Table = open addressing, lock-free (CAS on the key, interlocked add on the count).
// Dump at exit (and on NOTSA_HITCOUNT_FILE: path, default hitcount.txt in cwd): "rva count" rows (hex rva relative to the exe image base, decimal count).
// NOT compiled with /Gh itself (the CMake option only sets it on game_sa sources).
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

namespace {
constexpr uint32_t kBits = 20, kSize = 1u << kBits;
struct Slot { volatile LONG64 key; volatile LONG64 count; };
Slot g_tab[kSize];
volatile LONG g_dropped = 0;

void Dump() {
    const char* p = std::getenv("NOTSA_HITCOUNT_FILE");
    FILE* f = std::fopen(p && *p ? p : "hitcount.txt", "w");
    if (!f) return;
    auto base = (uint64_t)GetModuleHandleW(nullptr);
    std::fprintf(f, "# base %llx dropped %ld\n", (unsigned long long)base, (long)g_dropped);
    for (auto& s : g_tab) if (s.key) std::fprintf(f, "%llx %llu\n", (unsigned long long)((uint64_t)s.key - base), (unsigned long long)s.count);
    std::fclose(f);
}
struct Init { Init() { std::atexit(Dump); } } g_init;
}

extern "C" void notsa_hitcount_hit(uint64_t ra) {
    uint32_t i = (uint32_t)((ra * 0x9E3779B97F4A7C15ull) >> (64 - kBits));
    for (uint32_t n = 0; n < kSize; ++n, i = (i + 1) & (kSize - 1)) {
        LONG64 k = g_tab[i].key;
        if (k == 0) k = InterlockedCompareExchange64(&g_tab[i].key, (LONG64)ra, 0) == 0 ? (LONG64)ra : g_tab[i].key;
        if (k == (LONG64)ra) { InterlockedIncrement64(&g_tab[i].count); return; }
    }
    InterlockedIncrement(&g_dropped);
}
