// Link stub for the rw_* shim unit tests: source/standalone/rw/{engine,camera}.cpp log through notsa::standalone::Fixups::Log (Fixups.cpp is not part of the tests).
#ifdef NOTSA_STANDALONE_RUN
#include <cstdarg>
#include <cstdio>
#include "standalone/Fixups.h"

namespace notsa::standalone::Fixups {
void Log(const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    std::vfprintf(stderr, fmt, va);
    std::fputc('\n', stderr);
    va_end(va);
}
void LogGameStateTrace(int, unsigned long, unsigned long, unsigned long) {} // game state is not available in the unit tests
} // namespace notsa::standalone::Fixups
#endif
