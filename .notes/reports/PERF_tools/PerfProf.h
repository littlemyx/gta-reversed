#pragma once
// TEMPORARY profiling (agent PERF): force-included (/FI) into selected TUs together with /DNOTSA_PERF_PROF. Redefines Tracy's ZoneScoped/ZoneScopedN as
// QPC scopes with inclusive/exclusive time and call counts, main thread only. Snapshot (cumulative) to perf.log in the cwd once per second.
// NOTE: all arithmetic on tick counts is integer (the game's main thread runs with the x87 at PC=24 after D3D9 CreateDevice, so doubles/floats of large
// values lose precision: a double of a QPC value there has a 128 ms ulp).
#include <windows.h>
#include <cstdio>
#include <cstring>
namespace perfprof {
struct Slot { const char* name; const char* file; int line; long long incl, excl; unsigned long long calls; };
inline Slot   g_pool[8192];
inline Slot*  g_slots[8192];
inline int    g_n = 0;
inline DWORD  g_mainTid = GetCurrentThreadId();
inline long long g_freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f.QuadPart; }();
inline long long Now() { LARGE_INTEGER q; QueryPerformanceCounter(&q); return q.QuadPart; }
inline long long g_wall0 = 0, g_q0 = 0; // wall0: FILETIME in ms
inline long long WallMs() {
    if (g_wall0 == 0) { FILETIME ft; GetSystemTimeAsFileTime(&ft); g_wall0 = (long long)(((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime) / 10000; g_q0 = Now(); }
    return g_wall0 + (Now() - g_q0) * 1000 / g_freq;
}
inline struct WallInit { WallInit() { WallMs(); } } g_wallInit;
inline Slot*  g_idle = nullptr;
inline unsigned long long g_frames = 0;
inline long long g_lastDump = 0;
inline FILE*  g_f = nullptr;
inline Slot* Reg(const char* name, const char* file, int line) {
    if (g_n >= 8192) return &g_pool[0];
    Slot* s = &g_pool[g_n]; *s = Slot{ name, file, line, 0, 0, 0 }; g_slots[g_n++] = s;
    if (!strcmp(name, "Idle")) g_idle = s;
    return s;
}
struct Scope;
inline Scope* g_cur = nullptr;
inline void Dump() {
    if (!g_f) g_f = fopen("perf.log", "a");
    if (!g_f) return;
    fprintf(g_f, "T %lld %llu\n", WallMs(), g_frames);
    for (int i = 0; i < g_n; i++) {
        const Slot* s = g_slots[i];
        if (!s->calls) continue;
        const char* f = strrchr(s->file, '\\');
        fprintf(g_f, "S %s:%d %s %lld %lld %llu\n", f ? f + 1 : s->file, s->line, s->name, s->incl * 1000000 / g_freq, s->excl * 1000000 / g_freq, s->calls); // microseconds
    }
    fflush(g_f);
}
struct Scope {
    Slot* s; long long t0; long long child = 0; Scope* parent = nullptr; bool on;
    explicit Scope(Slot* sl) : s(sl), on(GetCurrentThreadId() == g_mainTid) {
        if (on) { t0 = Now(); parent = g_cur; g_cur = this; }
    }
    ~Scope() {
        if (!on) return;
        const long long t = Now(); const long long d = t - t0;
        s->incl += d; s->excl += d - child; s->calls++;
        g_cur = parent;
        if (parent) parent->child += d;
        if (s == g_idle) {
            g_frames++;
            if (t - g_lastDump > g_freq) { g_lastDump = t; Dump(); }
        }
    }
};
}
#undef ZoneScoped
#undef ZoneScopedN
#define PERF_CAT2(a, b) a##b
#define PERF_CAT(a, b) PERF_CAT2(a, b)
#define ZoneScoped static ::perfprof::Slot* PERF_CAT(_ps_s, __LINE__) = ::perfprof::Reg(__FUNCTION__, __FILE__, __LINE__); ::perfprof::Scope PERF_CAT(_ps_c, __LINE__)(PERF_CAT(_ps_s, __LINE__))
#define ZoneScopedN(nm) static ::perfprof::Slot* PERF_CAT(_ps_s, __LINE__) = ::perfprof::Reg(nm, __FILE__, __LINE__); ::perfprof::Scope PERF_CAT(_ps_c, __LINE__)(PERF_CAT(_ps_s, __LINE__))
#define PERF_SCOPE(nm) ZoneScopedN(nm)
