#pragma once
/*
 * Standard-library gaps of libc++ (stream C-4). On MSVC (_WIN32) every name below IS the std:: one, so the Windows build is unchanged.
 *
 * Findings (see .notes/reports/MAC-C6.md): Homebrew LLVM 23's libc++ has views::enumerate and ranges::shift_right (Apple libc++ 21 of
 * Xcode does not: use brew llvm for the portable build). Neither libc++ has <stacktrace>; std::from_chars(floating point) is declared
 * but marked "introduced in macOS 26.0" by the libc++ availability annotations (the portable target is macOS 15).
 */
#include <cerrno>
#include <charconv>
#include <concepts>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <version>

// ---- <stacktrace> ----
#if defined(_WIN32) || defined(__cpp_lib_stacktrace)
    #include <stacktrace>
namespace notsa {
using StackTrace = std::stacktrace;
inline std::string StackTraceToString(const StackTrace& st) { return std::to_string(st); }
} // namespace notsa
#else
namespace notsa {
//! Stand-in for std::stacktrace: always empty
struct StackTrace {
    static StackTrace current() { return {}; }
    bool              empty() const { return true; }
};
inline std::string StackTraceToString(const StackTrace&) { return "(std::stacktrace is not available in this standard library)"; }
} // namespace notsa
#endif

// ---- std::from_chars for floating point ----
#if defined(_WIN32) || defined(__cpp_lib_to_chars)
namespace notsa::compat {
using std::from_chars;
} // namespace notsa::compat
#else
namespace notsa::compat {
// integers: the std function; floating point: strtod family on a NUL-terminated copy (decimal, general format; hex/inf/nan as strtod)
template<class T>
    requires std::is_floating_point_v<T>
std::from_chars_result from_chars(const char* first, const char* last, T& value, std::chars_format fmt = std::chars_format::general) {
    (void)fmt;
    const std::string buf(first, last);
    if (buf.empty() || buf[0] == ' ' || buf[0] == '\t' || buf[0] == '\n' || buf[0] == '+') { // from_chars skips no whitespace and accepts no '+'
        return { first, std::errc::invalid_argument };
    }
    char* end = nullptr;
    errno = 0;
    T out;
    if constexpr (std::is_same_v<T, float>) { out = std::strtof(buf.c_str(), &end); }
    else if constexpr (std::is_same_v<T, double>) { out = std::strtod(buf.c_str(), &end); }
    else { out = std::strtold(buf.c_str(), &end); }
    if (end == buf.c_str()) { return { first, std::errc::invalid_argument }; }
    if (errno == ERANGE) { return { first + (end - buf.c_str()), std::errc::result_out_of_range }; }
    value = out;
    return { first + (end - buf.c_str()), std::errc{} };
}
template<std::integral T>
std::from_chars_result from_chars(const char* first, const char* last, T& value, int base = 10) {
    return std::from_chars(first, last, value, base);
}
} // namespace notsa::compat
#endif
