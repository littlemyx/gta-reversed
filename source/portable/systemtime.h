#pragma once
/*
 * 16 byte Win32 SYSTEMTIME record for NON-Windows builds (slice P-2b).
 *
 * CSimpleVariablesSaveStructure (save-game block 0, 0x138 bytes) embeds a Win32 SYSTEMTIME (8 x WORD = 16 bytes, filled by GetLocalTime) and is
 * dumped raw. On a non-Windows host <minwinbase.h> does not exist, so the whole class body was lost (sizeof == 1).
 *
 * The record is deliberately NOT called SYSTEMTIME and lives in namespace notsa: portable/winapi_compat.h (MAC-W0) may declare the global
 * `struct SYSTEMTIME` for other consumers, two definitions of one name would not compile, and this header must work with or without it.
 * `GetLocalTime(&m_systemTime)` in SimpleVariablesSaveStructure.cpp stays unchanged: ADL on `notsa::SystemTime*` finds notsa::GetLocalTime.
 * On Windows this file only forwards to <minwinbase.h> (`notsa::SystemTime` is SYSTEMTIME itself, `GetLocalTime` the Win32 one).
 */
#ifdef _WIN32
    #include <minwinbase.h>
namespace notsa {
using SystemTime = SYSTEMTIME;
}
#else
    #include <cstdint>
    #include <ctime>
    #include <sys/time.h>

namespace notsa {
//! Exactly the Win32 layout (minwinbase.h): 8 x WORD, no padding. On-disk format.
struct SystemTime {
    uint16_t wYear;
    uint16_t wMonth;
    uint16_t wDayOfWeek;
    uint16_t wDay;
    uint16_t wHour;
    uint16_t wMinute;
    uint16_t wSecond;
    uint16_t wMilliseconds;
};
static_assert(sizeof(SystemTime) == 16 && alignof(SystemTime) == 2, "SYSTEMTIME is a 16 byte on-disk record (8 x WORD)");

//! Win32 GetLocalTime: wMonth 1..12, wDayOfWeek 0 = Sunday (struct tm's tm_wday)
inline void GetLocalTime(SystemTime* st) {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    const time_t secs = tv.tv_sec;
    tm t{};
    localtime_r(&secs, &t);
    st->wYear         = (uint16_t)(t.tm_year + 1900);
    st->wMonth        = (uint16_t)(t.tm_mon + 1);
    st->wDayOfWeek    = (uint16_t)t.tm_wday;
    st->wDay          = (uint16_t)t.tm_mday;
    st->wHour         = (uint16_t)t.tm_hour;
    st->wMinute       = (uint16_t)t.tm_min;
    st->wSecond       = (uint16_t)t.tm_sec;
    st->wMilliseconds = (uint16_t)(tv.tv_usec / 1000);
}
} // namespace notsa
#endif
