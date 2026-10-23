#pragma once
/*
 * MSVC CRT names for NON-Windows builds (stream C-6). Included from app/platform/win/winincl.h when !_WIN32.
 *
 * The list is driven by the native clang census (.notes/stream_c/native_arm64_unique_diagnostics.tsv, category "W/C: MSVC CRT name"
 * + the CRT-name rows of "other: undeclared identifier"), not by guesswork. The "_s" functions keep the destination-size semantics of
 * the Microsoft CRT: nothing is written past `size`, on a violation the destination becomes "" (dst[0] = 0), the invalid-parameter
 * handler runs (default: message + abort(), like the Release CRT) and the errno_t is returned. Both forms exist, as in <string.h>:
 * `f(dst, size, ...)` and the array template `f(char (&dst)[N], ...)`.
 *
 * Known deviations (documented in .notes/reports/MAC-C6.md):
 *  - sscanf_s/fscanf_s: a `%s` / `%[` without a width is truncated to size-1 characters (the CRT fails the conversion instead).
 *  - fopen_s ignores the ",ccs=" mode suffix; `wchar_t` is 32 bit here (wide-string helpers exist only as far as the census needs them).
 */
#ifndef _WIN32

#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <string>
#include <type_traits>
#include <utility>
#include <strings.h> // strcasecmp, strncasecmp

#include "wintypes.h"

#ifndef _MAX_PATH
    #define _MAX_PATH 260
#endif
#ifndef _TRUNCATE
    #define _TRUNCATE ((size_t)-1)
#endif
#ifndef STRUNCATE
    #define STRUNCATE 80
#endif

using errno_t = int;
using rsize_t = size_t;

// ---- invalid-parameter handler (the _s functions call it on a violation) ----
using _invalid_parameter_handler = void (*)(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned line, uintptr_t reserved);

namespace notsa::portable {
inline _invalid_parameter_handler& InvalidParameterHandler() {
    static _invalid_parameter_handler s_handler = nullptr;
    return s_handler;
}
inline void InvalidParameter(const char* what) {
    if (const auto h = InvalidParameterHandler()) {
        h(nullptr, nullptr, nullptr, 0, 0);
        return;
    }
    std::fprintf(stderr, "Invalid parameter detected in a *_s CRT function: %s\n", what);
    std::abort();
}
} // namespace notsa::portable

inline _invalid_parameter_handler _set_invalid_parameter_handler(_invalid_parameter_handler h) {
    auto& cur = notsa::portable::InvalidParameterHandler();
    const auto prev = cur;
    cur = h;
    return prev;
}

// ---- case-insensitive compare, in-place case change ----
inline int _stricmp(const char* a, const char* b) { return ::strcasecmp(a, b); }
inline int stricmp(const char* a, const char* b) { return ::strcasecmp(a, b); }
inline int _strnicmp(const char* a, const char* b, size_t n) { return ::strncasecmp(a, b, n); }
inline int strnicmp(const char* a, const char* b, size_t n) { return ::strncasecmp(a, b, n); }
inline char* _strlwr(char* s) { for (char* p = s; *p; ++p) { if (*p >= 'A' && *p <= 'Z') { *p = (char)(*p + ('a' - 'A')); } } return s; }
inline char* _strupr(char* s) { for (char* p = s; *p; ++p) { if (*p >= 'a' && *p <= 'z') { *p = (char)(*p - ('a' - 'A')); } } return s; }
inline int _strlwr_s(char* s, size_t size) {
    if (!s || !std::memchr(s, 0, size)) { notsa::portable::InvalidParameter("_strlwr_s: string not terminated within size"); return EINVAL; }
    _strlwr(s);
    return 0;
}
inline int _strupr_s(char* s, size_t size) {
    if (!s || !std::memchr(s, 0, size)) { notsa::portable::InvalidParameter("_strupr_s: string not terminated within size"); return EINVAL; }
    _strupr(s);
    return 0;
}
template<size_t N> int _strlwr_s(char (&s)[N]) { return _strlwr_s(s, N); }
template<size_t N> int _strupr_s(char (&s)[N]) { return _strupr_s(s, N); }

// ---- strcpy_s / strcat_s / strncpy_s (+ wide) ----
inline errno_t strcpy_s(char* dst, size_t size, const char* src) {
    if (!dst || size == 0) { notsa::portable::InvalidParameter("strcpy_s: dst/size"); return EINVAL; }
    if (!src) { dst[0] = 0; notsa::portable::InvalidParameter("strcpy_s: src"); return EINVAL; }
    const size_t len = std::strlen(src);
    if (len >= size) { dst[0] = 0; notsa::portable::InvalidParameter("strcpy_s: buffer too small"); return ERANGE; }
    std::memcpy(dst, src, len + 1);
    return 0;
}
template<size_t N> errno_t strcpy_s(char (&dst)[N], const char* src) { return strcpy_s(dst, N, src); }

inline errno_t strcat_s(char* dst, size_t size, const char* src) {
    if (!dst || size == 0) { notsa::portable::InvalidParameter("strcat_s: dst/size"); return EINVAL; }
    if (!src) { dst[0] = 0; notsa::portable::InvalidParameter("strcat_s: src"); return EINVAL; }
    const char* nul = static_cast<const char*>(std::memchr(dst, 0, size));
    if (!nul) { dst[0] = 0; notsa::portable::InvalidParameter("strcat_s: dst not terminated within size"); return EINVAL; }
    const size_t used = (size_t)(nul - dst), len = std::strlen(src);
    if (used + len >= size) { dst[0] = 0; notsa::portable::InvalidParameter("strcat_s: buffer too small"); return ERANGE; }
    std::memcpy(dst + used, src, len + 1);
    return 0;
}
template<size_t N> errno_t strcat_s(char (&dst)[N], const char* src) { return strcat_s(dst, N, src); }

inline errno_t strncpy_s(char* dst, size_t size, const char* src, size_t count) {
    if (!dst || size == 0) { notsa::portable::InvalidParameter("strncpy_s: dst/size"); return EINVAL; }
    if (count == 0) { dst[0] = 0; return 0; }
    if (!src) { dst[0] = 0; notsa::portable::InvalidParameter("strncpy_s: src"); return EINVAL; }
    const size_t limit = (count == _TRUNCATE) ? size : count; // _TRUNCATE: copy as much as fits
    size_t n = 0;
    while (n < limit && src[n]) { ++n; }
    if (n >= size) {
        if (count == _TRUNCATE) { std::memcpy(dst, src, size - 1); dst[size - 1] = 0; return STRUNCATE; }
        dst[0] = 0; notsa::portable::InvalidParameter("strncpy_s: buffer too small"); return ERANGE;
    }
    std::memcpy(dst, src, n);
    dst[n] = 0;
    return 0;
}
template<size_t N> errno_t strncpy_s(char (&dst)[N], const char* src, size_t count) { return strncpy_s(dst, N, src, count); }

inline errno_t wcscpy_s(wchar_t* dst, size_t size, const wchar_t* src) {
    if (!dst || size == 0) { notsa::portable::InvalidParameter("wcscpy_s: dst/size"); return EINVAL; }
    if (!src) { dst[0] = 0; notsa::portable::InvalidParameter("wcscpy_s: src"); return EINVAL; }
    const size_t len = std::wcslen(src);
    if (len >= size) { dst[0] = 0; notsa::portable::InvalidParameter("wcscpy_s: buffer too small"); return ERANGE; }
    std::wmemcpy(dst, src, len + 1);
    return 0;
}
template<size_t N> errno_t wcscpy_s(wchar_t (&dst)[N], const wchar_t* src) { return wcscpy_s(dst, N, src); }

inline errno_t wcscat_s(wchar_t* dst, size_t size, const wchar_t* src) {
    if (!dst || size == 0) { notsa::portable::InvalidParameter("wcscat_s: dst/size"); return EINVAL; }
    if (!src) { dst[0] = 0; notsa::portable::InvalidParameter("wcscat_s: src"); return EINVAL; }
    const wchar_t* nul = std::wmemchr(dst, 0, size);
    if (!nul) { dst[0] = 0; notsa::portable::InvalidParameter("wcscat_s: dst not terminated within size"); return EINVAL; }
    const size_t used = (size_t)(nul - dst), len = std::wcslen(src);
    if (used + len >= size) { dst[0] = 0; notsa::portable::InvalidParameter("wcscat_s: buffer too small"); return ERANGE; }
    std::wmemcpy(dst + used, src, len + 1);
    return 0;
}
template<size_t N> errno_t wcscat_s(wchar_t (&dst)[N], const wchar_t* src) { return wcscat_s(dst, N, src); }

// ---- memcpy_s, _memicmp, date/time strings ----
inline errno_t memcpy_s(void* dst, size_t dstSize, const void* src, size_t count) {
    if (count == 0) { return 0; }
    if (!dst) { notsa::portable::InvalidParameter("memcpy_s: dst"); return EINVAL; }
    if (!src) { std::memset(dst, 0, dstSize); notsa::portable::InvalidParameter("memcpy_s: src"); return EINVAL; }
    if (dstSize < count) { std::memset(dst, 0, dstSize); notsa::portable::InvalidParameter("memcpy_s: buffer too small"); return ERANGE; }
    std::memcpy(dst, src, count);
    return 0;
}
inline int _memicmp(const void* a, const void* b, size_t n) {
    const auto* x = static_cast<const unsigned char*>(a);
    const auto* y = static_cast<const unsigned char*>(b);
    for (; n; --n, ++x, ++y) {
        const int cx = (*x >= 'A' && *x <= 'Z') ? *x + 32 : *x, cy = (*y >= 'A' && *y <= 'Z') ? *y + 32 : *y;
        if (cx != cy) { return cx < cy ? -1 : 1; }
    }
    return 0;
}
//! "MM/DD/YY" (needs 9 bytes) / "HH:MM:SS" (needs 9 bytes)
inline errno_t _strdate_s(char* buf, size_t size) {
    if (!buf || size < 9) { notsa::portable::InvalidParameter("_strdate_s: buffer"); return size ? (buf[0] = 0, ERANGE) : EINVAL; }
    const std::time_t t = std::time(nullptr); std::tm tm{}; localtime_r(&t, &tm);
    std::strftime(buf, size, "%m/%d/%y", &tm);
    return 0;
}
inline errno_t _strtime_s(char* buf, size_t size) {
    if (!buf || size < 9) { notsa::portable::InvalidParameter("_strtime_s: buffer"); return size ? (buf[0] = 0, ERANGE) : EINVAL; }
    const std::time_t t = std::time(nullptr); std::tm tm{}; localtime_r(&t, &tm);
    std::strftime(buf, size, "%H:%M:%S", &tm);
    return 0;
}
template<size_t N> errno_t _strdate_s(char (&buf)[N]) { return _strdate_s(buf, N); }
template<size_t N> errno_t _strtime_s(char (&buf)[N]) { return _strtime_s(buf, N); }

// ---- printf family ----
inline int vsprintf_s(char* dst, size_t size, const char* fmt, va_list ap) {
    if (!dst || size == 0 || !fmt) { notsa::portable::InvalidParameter("vsprintf_s: dst/size/format"); return -1; }
    const int n = std::vsnprintf(dst, size, fmt, ap);
    if (n < 0 || (size_t)n >= size) { dst[0] = 0; notsa::portable::InvalidParameter("vsprintf_s: buffer too small"); return -1; }
    return n;
}
template<size_t N> int vsprintf_s(char (&dst)[N], const char* fmt, va_list ap) { return vsprintf_s(dst, N, fmt, ap); }
inline int sprintf_s(char* dst, size_t size, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    const int r = vsprintf_s(dst, size, fmt, ap);
    va_end(ap);
    return r;
}
template<size_t N, class... A>
int sprintf_s(char (&dst)[N], const char* fmt, A... a) {
    return sprintf_s(static_cast<char*>(dst), N, fmt, a...);
}
#define fprintf_s fprintf
#define printf_s printf
#define vfprintf_s vfprintf

// ---- scanf family: the `_s` forms take an extra size argument after every %s / %c / %[ ----
namespace notsa::portable::scanf_s_detail {
inline constexpr size_t kMaxOut = 48; // VehicleModelInfo.cpp reads 32 values in one call

template<class T> uintptr_t ToWord(T v) {
    if constexpr (std::is_pointer_v<T> || std::is_null_pointer_v<T>) { return reinterpret_cast<uintptr_t>(v); }
    else { return static_cast<uintptr_t>(v); }
}

template<bool IsFile, size_t N>
int CallN(const void* src, const char* fmt, void* const* a) {
    return [&]<size_t... I>(std::index_sequence<I...>) {
        if constexpr (IsFile) { return std::fscanf(static_cast<FILE*>(const_cast<void*>(src)), fmt, a[I]...); }
        else                  { return std::sscanf(static_cast<const char*>(src), fmt, a[I]...); }
    }(std::make_index_sequence<N>{});
}
template<bool IsFile, size_t... N>
constexpr auto MakeTable(std::index_sequence<N...>) {
    return std::array<int (*)(const void*, const char*, void* const*), sizeof...(N)>{ &CallN<IsFile, N>... };
}

// Walks `fmt`, rewrites it (a width is added to an unbounded %s / %[), collects the output pointers WITHOUT the size arguments.
// Returns false when an argument is missing or a size is 0.
inline bool Rewrite(const char* fmt, const uintptr_t* args, size_t nargs, std::string& out, void** ptrs, size_t& nptrs) {
    size_t ai = 0;
    nptrs = 0;
    const char* p = fmt;
    while (*p) {
        if (*p != '%') { out += *p++; continue; }
        out += *p++;
        if (*p == '%') { out += *p++; continue; }
        const bool suppress = (*p == '*');
        if (suppress) { out += *p++; }
        const char* ws = p;
        while (*p >= '0' && *p <= '9') { ++p; }
        std::string width(ws, p);
        const char* ms = p;
        while (*p && std::strchr("hlLjztq", *p)) { ++p; }
        const std::string mods(ms, p);
        const char conv = *p;
        if (!conv) { out += width; out += mods; break; }
        ++p;
        const bool isSet = (conv == '[');
        const bool isStr = (conv == 's' || conv == 'S' || isSet);
        const bool isChr = (conv == 'c' || conv == 'C');
        const bool wide  = (conv == 'S' || conv == 'C' || ((mods == "l" || mods == "L") && (isStr || isChr)));
        std::string scanset;
        if (isSet) { // `[` `^`? `]`? ... `]`, copied verbatim
            const char* ss = p;
            if (*p == '^') { ++p; }
            if (*p == ']') { ++p; }
            while (*p && *p != ']') { ++p; }
            if (*p == ']') { ++p; }
            scanset.assign(ss, p);
        }
        if (!suppress) {
            if (isStr || isChr) {
                if (ai + 2 > nargs || nptrs >= kMaxOut) { return false; }
                ptrs[nptrs++] = reinterpret_cast<void*>(args[ai]);
                const uintptr_t size = args[ai + 1];
                ai += 2;
                if (!wide && !isChr && width.empty()) {
                    if (size == 0) { return false; }
                    width = std::to_string(size - 1);
                }
            } else {
                if (ai >= nargs || nptrs >= kMaxOut) { return false; }
                ptrs[nptrs++] = reinterpret_cast<void*>(args[ai++]);
            }
        }
        out += width; out += mods; out += conv; out += scanset;
    }
    return true;
}

template<bool IsFile>
int Scan(const void* src, const char* fmt, const uintptr_t* args, size_t nargs) {
    static constexpr auto s_table = MakeTable<IsFile>(std::make_index_sequence<kMaxOut + 1>{});
    std::string newFmt;
    void*       ptrs[kMaxOut + 1]{};
    size_t      nptrs = 0;
    if (!Rewrite(fmt, args, nargs, newFmt, ptrs, nptrs)) {
        notsa::portable::InvalidParameter("sscanf_s/fscanf_s: missing size argument or zero size");
        return EOF;
    }
    return s_table[nptrs](src, newFmt.c_str(), ptrs);
}
} // namespace notsa::portable::scanf_s_detail

template<class... A>
int sscanf_s(const char* src, const char* fmt, A... a) {
    const uintptr_t args[sizeof...(A) + 1] = { notsa::portable::scanf_s_detail::ToWord(a)..., 0 };
    return notsa::portable::scanf_s_detail::Scan<false>(src, fmt, args, sizeof...(A));
}
template<class... A>
int fscanf_s(FILE* f, const char* fmt, A... a) {
    const uintptr_t args[sizeof...(A) + 1] = { notsa::portable::scanf_s_detail::ToWord(a)..., 0 };
    return notsa::portable::scanf_s_detail::Scan<true>(f, fmt, args, sizeof...(A));
}

// ---- tokenizer, files, directories ----
inline char* strtok_s(char* s, const char* delim, char** ctx) { return ::strtok_r(s, delim, ctx); }

inline errno_t fopen_s(FILE** f, const char* path, const char* mode) {
    if (!f) { notsa::portable::InvalidParameter("fopen_s: file"); return EINVAL; }
    *f = std::fopen(path, mode);
    return *f ? 0 : errno;
}
inline errno_t freopen_s(FILE** f, const char* path, const char* mode, FILE* stream) {
    if (!f) { notsa::portable::InvalidParameter("freopen_s: file"); return EINVAL; }
    *f = std::freopen(path, mode, stream);
    return *f ? 0 : errno;
}

extern "C" char* getcwd(char*, size_t);
extern "C" int   chdir(const char*);
inline char* _getcwd(char* buf, int size) { return ::getcwd(buf, size > 0 ? (size_t)size : 0); }
inline int   _chdir(const char* path) { return ::chdir(path); }
inline int   _wchdir(const wchar_t* path) {
    char buf[4096];
    if (std::wcstombs(buf, path, sizeof(buf)) == (size_t)-1) { errno = EINVAL; return -1; }
    return ::chdir(buf);
}

inline wchar_t* _wgetcwd(wchar_t* buf, int size) {
    char tmp[4096];
    if (!::getcwd(tmp, sizeof(tmp)) || size <= 0) { return nullptr; }
    if (std::mbstowcs(buf, tmp, (size_t)size) == (size_t)-1) { return nullptr; }
    buf[size - 1] = 0;
    return buf;
}
inline errno_t _wfopen_s(FILE** f, const wchar_t* path, const wchar_t* mode) {
    char p[4096], m[64];
    if (std::wcstombs(p, path, sizeof(p)) == (size_t)-1 || std::wcstombs(m, mode, sizeof(m)) == (size_t)-1) { if (f) { *f = nullptr; } return EINVAL; }
    return fopen_s(f, p, m);
}

// ---- Interlocked* over the compiler's atomic builtins (sequentially consistent, like the MSVC intrinsics) ----
inline LONG   InterlockedExchange(volatile LONG* p, LONG v)             { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
inline CHAR   InterlockedExchange8(volatile CHAR* p, CHAR v)            { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
inline SHORT  InterlockedExchange16(volatile SHORT* p, SHORT v)         { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
inline LONG64 InterlockedExchange64(volatile LONG64* p, LONG64 v)       { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
inline LONG   InterlockedExchangeAdd(volatile LONG* p, LONG v)          { return __atomic_fetch_add(p, v, __ATOMIC_SEQ_CST); }
inline LONG64 InterlockedExchangeAdd64(volatile LONG64* p, LONG64 v)    { return __atomic_fetch_add(p, v, __ATOMIC_SEQ_CST); }
inline LONG   InterlockedIncrement(volatile LONG* p)                    { return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST); }
inline LONG   InterlockedDecrement(volatile LONG* p)                    { return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST); }
inline LONG64 InterlockedIncrement64(volatile LONG64* p)                { return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST); }
inline LONG64 InterlockedDecrement64(volatile LONG64* p)                { return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST); }
inline LONG   InterlockedOr(volatile LONG* p, LONG v)                   { return __atomic_fetch_or(p, v, __ATOMIC_SEQ_CST); }
inline LONG   InterlockedAnd(volatile LONG* p, LONG v)                  { return __atomic_fetch_and(p, v, __ATOMIC_SEQ_CST); }
inline LONG   InterlockedCompareExchange(volatile LONG* p, LONG xchg, LONG cmp) {
    __atomic_compare_exchange_n(p, &cmp, xchg, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return cmp; // the value that was in *p
}
inline LONG64 InterlockedCompareExchange64(volatile LONG64* p, LONG64 xchg, LONG64 cmp) {
    __atomic_compare_exchange_n(p, &cmp, xchg, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return cmp;
}

#endif // !_WIN32
