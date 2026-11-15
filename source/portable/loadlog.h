#pragma once
// SH2 NOTSA_SH_LOADLOG=<file>: resource load-order log of an RR run (files opened, streaming requests / reads / completions / removals), see .notes/STATEHASH_LOADORDER.md.
// Row: `seq fc=<CTimer frame counter> rec=<recorded frame or -1> <kind> <args>`. Needs NOTSA_STATEHASH; one getenv + one FILE*, thread safe (the file is written from any thread, in call order).
#ifdef NOTSA_STATEHASH
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <atomic>
#include <mutex>
namespace notsa::rr {
uint32_t LoadLogFrameCounter();  // CTimer::GetFrameCounter() (statehash.cpp)
int      LoadLogRecFrame();      // recorded frame number, -1 when not recording
inline void LoadLog(const char* fmt, ...) {
    static FILE* f = [] { const char* p = std::getenv("NOTSA_SH_LOADLOG"); return p && p[0] ? std::fopen(p, "w") : nullptr; }();
    if (!f) return;
    static std::mutex m;
    static std::atomic<unsigned long long> seq{ 0 };
    char b[600];
    va_list ap; va_start(ap, fmt); std::vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    std::lock_guard<std::mutex> g(m);
    std::fprintf(f, "%llu fc=%u rec=%d %s\n", seq++, LoadLogFrameCounter(), LoadLogRecFrame(), b);
    std::fflush(f);
}
} // namespace notsa::rr
#define NOTSA_LOADLOG(...) ::notsa::rr::LoadLog(__VA_ARGS__)
#else
#define NOTSA_LOADLOG(...) ((void)0)
#endif
