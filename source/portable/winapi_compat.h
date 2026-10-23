#pragma once
/*
 * Win32 API names that have a STRAIGHT portable equivalent (MAC-W0, stream W), for NON-Windows builds. Included from winincl.h and
 * winshim/Windows.h when !_WIN32. Anything without a faithful equivalent (windows, registry, DirectX, COM, Media Foundation ...) is NOT here:
 * it is excluded under NOTSA_PORTABLE or replaced by a logging stub that fails (see .notes/reports/MAC-W0.md, W-n list).
 */
#ifndef _WIN32

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

#include "wintypes.h"

// ---- geometry structs (plain data) ----
struct RECT  { LONG left, top, right, bottom; };
struct POINT { LONG x, y; };
struct SIZE  { LONG cx, cy; };
using LPRECT  = RECT*;
using LPPOINT = POINT*;
using PRECT   = RECT*;
using PPOINT  = POINT*;
using LPCRECT = const RECT*;

// ---- time ----
inline void Sleep(DWORD ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

//! Milliseconds since an arbitrary epoch (steady clock), wraps at 2^32 like the Win32 call
inline ULONGLONG GetTickCount64() {
    return (ULONGLONG)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline DWORD GetTickCount() { return (DWORD)GetTickCount64(); }

//! Nanosecond steady clock (QueryPerformanceFrequency = 1e9)
inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* f) {
    f->QuadPart = 1'000'000'000;
    return TRUE;
}
inline BOOL QueryPerformanceCounter(LARGE_INTEGER* c) {
    c->QuadPart = (LONGLONG)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return TRUE;
}

// ---- debug output ----
inline void OutputDebugStringA(const char* s) { std::fputs(s, stderr); }
inline void DebugBreak() { __builtin_trap(); }
//! No debugger probe on this platform: honest answer is "no"
inline BOOL IsDebuggerPresent() { return FALSE; }

#endif // !_WIN32
