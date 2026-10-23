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
#include <cstdlib>
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

// ---- plain-data Win32 structs that only appear as members / signatures (no behaviour) ----
struct OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    DWORD     Offset;
    DWORD     OffsetHigh;
    HANDLE    hEvent;
};
using LPOVERLAPPED = OVERLAPPED*;
struct CONDITION_VARIABLE { PVOID Ptr; };
struct _RTL_CRITICAL_SECTION { // layout only (Win64 field set): there is NO Enter/Leave/Initialize here, a use of one is a compile error on purpose
    PVOID     DebugInfo;
    LONG      LockCount;
    LONG      RecursionCount;
    HANDLE    OwningThread;
    HANDLE    LockSemaphore;
    ULONG_PTR SpinCount;
};
using CRITICAL_SECTION = _RTL_CRITICAL_SECTION;

// ---- local memory: LocalAlloc(LPTR, n) is zero-initialised fixed memory == calloc ----
#ifndef LPTR
#define LPTR 0x0040
#endif
inline HANDLE LocalAlloc(UINT flags, SIZE_T bytes) {
    (void)flags; // only LPTR (zeroed) is used by the game; calloc zeroes in every case
    return std::calloc(1, bytes);
}
inline HANDLE LocalFree(HANDLE mem) {
    std::free(mem);
    return nullptr;
}

// ---- threads: implemented over std::thread in portable/oswrapper_portable.cpp (thread HANDLEs only) ----
using LPTHREAD_START_ROUTINE = DWORD (*)(LPVOID);
#ifndef CREATE_SUSPENDED
#define CREATE_SUSPENDED 0x00000004
#endif
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#endif
#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0 0
#endif
#ifndef WAIT_FAILED
#define WAIT_FAILED ((DWORD)0xFFFFFFFF)
#endif
#ifndef THREAD_PRIORITY_LOWEST
#define THREAD_PRIORITY_LOWEST (-2)
#endif
#ifndef THREAD_PRIORITY_BELOW_NORMAL
#define THREAD_PRIORITY_BELOW_NORMAL (-1)
#endif
#ifndef THREAD_PRIORITY_NORMAL
#define THREAD_PRIORITY_NORMAL 0
#endif
#ifndef THREAD_PRIORITY_ABOVE_NORMAL
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#endif
#ifndef THREAD_PRIORITY_HIGHEST
#define THREAD_PRIORITY_HIGHEST 2
#endif
#ifndef THREAD_PRIORITY_TIME_CRITICAL
#define THREAD_PRIORITY_TIME_CRITICAL 15
#endif
HANDLE CreateThread(void* attrs, SIZE_T stackSize, LPTHREAD_START_ROUTINE start, LPVOID param, DWORD flags, LPDWORD threadId);
DWORD  ResumeThread(HANDLE thread);
//! W-B2: there is no portable suspend: logs and returns (DWORD)-1 (the Win32 failure value)
DWORD  SuspendThread(HANDLE thread);
//! W-B1: priorities are ignored (logged once), returns TRUE
BOOL   SetThreadPriority(HANDLE thread, int priority);
//! Only thread handles are waitable here (anything else: logs, WAIT_FAILED)
DWORD  WaitForSingleObject(HANDLE handle, DWORD milliseconds);
BOOL   CloseHandle(HANDLE handle);
//! W-B6: forced thread termination has no portable equivalent: logs and returns FALSE (the thread keeps running)
BOOL   TerminateThread(HANDLE thread, DWORD exitCode);

// ---- critical sections (recursive mutex kept behind DebugInfo; no spin count / debug info semantics) ----
void InitializeCriticalSection(CRITICAL_SECTION* cs);
void EnterCriticalSection(CRITICAL_SECTION* cs);
void LeaveCriticalSection(CRITICAL_SECTION* cs);
void DeleteCriticalSection(CRITICAL_SECTION* cs);

// ---- code pages: UTF-8 <-> wchar_t (UTF-32 here) ----
#ifndef CP_ACP
    #define CP_ACP 0
#endif
#ifndef CP_UTF8
    #define CP_UTF8 65001
#endif
#ifndef MB_ERR_INVALID_CHARS
    #define MB_ERR_INVALID_CHARS 0x00000008
#endif
#ifndef WC_ERR_INVALID_CHARS
    #define WC_ERR_INVALID_CHARS 0x00000080
#endif
//! CP_UTF8 (and CP_ACP, which is UTF-8 on macOS): the real conversion. Any other code page: logs and returns 0 (failure), like an unknown code page does.
//! GetACP: macOS file names / narrow strings are UTF-8 (common.h WindowsCharset == CP_UTF8 selects the no-conversion paths of FileMgr)
inline UINT GetACP() { return CP_UTF8; }
int MultiByteToWideChar(UINT codePage, DWORD flags, LPCSTR src, int srcLen, LPWSTR dst, int dstLen);
int WideCharToMultiByte(UINT codePage, DWORD flags, LPCWSTR src, int srcLen, LPSTR dst, int dstLen, LPCSTR defaultChar, LPBOOL usedDefault);

struct SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
};

// ---- COM: not used by the portable build (no COM object can be created, see winshim/dsound.h); initialising an apartment that nothing uses trivially succeeds ----
inline HRESULT CoInitialize(void*) { return S_OK; }
inline void    CoUninitialize() {}

// ---- debug output ----
inline void OutputDebugStringA(const char* s) { std::fputs(s, stderr); }
inline void DebugBreak() { __builtin_trap(); }
//! No debugger probe on this platform: honest answer is "no"
inline BOOL IsDebuggerPresent() { return FALSE; }

#include "audio_portable.h"

#endif // !_WIN32
