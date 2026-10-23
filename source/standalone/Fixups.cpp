#include "StdInc.h"

#if defined(NOTSA_STANDALONE_RUN) && !defined(NOTSA_PORTABLE) // (portable build: see the #elif at the end of the file)
#include <atomic>
#include <crtdbg.h>
#include <float.h>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <cstring>
#include <vector>
#include "Fixups.h"

namespace notsa::standalone {
namespace {
// (slice F3 of .notes/DETACH_DATA_PLAN.md: no hook registry, no image fixups, no trap stubs: the run build has nothing of the original exe to patch)

void __cdecl AbortHandler(int) {
    // Standalone S5: make the symbolising unhandled-exception filter (app_debug.cpp) print the call stack of the failing assert()/terminate() into logs/log.log
    if (!std::getenv("NOTSA_STANDALONE_NO_ABORT_TRACE")) {
        static bool s_Raised = false;
        if (!s_Raised) {
            s_Raised = true;
            Fixups::Log("abort(): raising exception 0xE0AB0001 for a stack trace");
            RaiseException(0xE0AB0001u, EXCEPTION_NONCONTINUABLE, 0, nullptr);
        }
    }
    Fixups::Fatal("abort() called (failed assert()/terminate). ");
}

#ifdef _DEBUG
// The debug CRT's assert() would open a modal message box (invisible/hanging under Wine, and in a headless run): log it and terminate instead
int __cdecl CrtReportHook(int type, char* message, int* returnValue) {
    if ((type == _CRT_ASSERT || type == _CRT_ERROR) && message && std::strstr(message, "array subscript out of range")) {
        // Known original bugs read std::array/arrays out of range (e.g. CTaskManager::GetTaskSecondary(-1)); the exe just reads the neighbour: continue
        static int s_N = 0;
        if (s_N++ < 20) {
            Fixups::Log("tolerated: %.200s", message);
        }
        if (returnValue) {
            *returnValue = 0;
        }
        return TRUE;
    }
    if (type == _CRT_ASSERT || type == _CRT_ERROR) {
        static bool s_Raised = false; // symbolised stack via the unhandled-exception filter (app_debug.cpp), see AbortHandler
        if (!s_Raised && !std::getenv("NOTSA_STANDALONE_NO_ABORT_TRACE")) {
            s_Raised = true;
            Fixups::Log("CRT assertion/error: %.500s", message ? message : "?");
            RaiseException(0xE0AB0002u, EXCEPTION_NONCONTINUABLE, 0, nullptr);
        }
        Fixups::Fatal("CRT assertion/error: %.700s", message ? message : "?");
    }
    if (returnValue) {
        *returnValue = 0;
    }
    return FALSE; // other report types: default handling
}
int __cdecl CrtReportHookW(int type, wchar_t* message, int* returnValue) {
    char narrow[800];
    narrow[0] = 0;
    if (message) {
        WideCharToMultiByte(CP_ACP, 0, message, -1, narrow, sizeof(narrow) - 1, nullptr, nullptr);
        narrow[sizeof(narrow) - 1] = 0;
    }
    return CrtReportHook(type, narrow, returnValue);
}
#endif

} // namespace

namespace Fixups {
// CRT free (wvsprintfA: user32, 1024 byte limit, no %z/%f) because `Log` is also used by the pre-CRT-init image loader of the dev-only GTASA_IMAGE_DIAG config
void Log(const char* fmt, ...) {
    char line[1100];
    va_list va;
    va_start(va, fmt);
    int n = wvsprintfA(line, fmt, va);
    va_end(va);
    OutputDebugStringA(line);
    OutputDebugStringA("\n");
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
    while (len > 0 && path[len - 1] != '\\') {
        len--;
    }
    lstrcpyA(path + len, "standalone.log");
    line[n] = '\r';
    line[n + 1] = '\n';
    const HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD w;
    if (h != INVALID_HANDLE_VALUE) {
        WriteFile(h, line, n + 2, &w, nullptr);
        CloseHandle(h);
    }
    if (const HANDLE e = GetStdHandle(STD_ERROR_HANDLE); e && e != INVALID_HANDLE_VALUE) {
        WriteFile(e, line, n + 2, &w, nullptr);
    }
}

void LogFpuState(const char* where) {
    const unsigned cw = _controlfp(0, 0);
    const unsigned sw = _statusfp();
    Log("FPU[%s]: PC=%s (cw=0x%05x) RC=0x%x EM=0x%02x sw=0x%x", where,
        (cw & _MCW_PC) == _PC_24 ? "24" : (cw & _MCW_PC) == _PC_53 ? "53" : "64",
        cw, cw & _MCW_RC, cw & _MCW_EM, sw);
}

void LogGameStateTrace(int n, unsigned long crtHeapBytes, unsigned long crtHeapBlocks, unsigned long committedKB) {
    if (FindPlayerPed(0) && CWorld::Players[0].m_pPed) {
        auto* const pv = FindPlayerVehicle(0, false);
        auto* const pp = FindPlayerPed(0);
        const auto* pad = CPad::GetPad(0);
        Log("playerlog veh=%p ped=%p inVeh=%d speed=%d cross=%d stickY=%d stickX=%d", (void*)pv, (void*)pp, pp ? (int)pp->bInVehicle : -1, pv ? (int)(pv->m_vecMoveSpeed.Magnitude() * 100.f) : -1, (int)pad->NewState.ButtonCross, (int)pad->NewState.LeftStickY, (int)pad->NewState.LeftStickX);
    }
    Log("clocklog %02d:%02d:%02d day=%d month=%d timerMs=%u lastTick=%u msPerMin=%u", (int)CClock::ms_nGameClockHours, (int)CClock::ms_nGameClockMinutes, (int)CClock::ms_nGameClockSeconds, (int)CClock::ms_nGameClockDays, (int)CClock::ms_nGameClockMonth, (unsigned)CTimer::GetTimeInMS(), (unsigned)CClock::ms_nLastClockTick, (unsigned)CClock::ms_nMillisecondsPerGameMinute);
    {   // world-entity pools (dummy pool = 2500 in the exe, a full pool makes `new CDummyObject` return null: the exe has no check either) + loaded IPL count
        unsigned loadedIpls = 0;
        if (CIplStore::GetPool()) { for (auto&& def : CIplStore::GetPool()->GetAllValid()) { loadedIpls += def.loaded ? 1u : 0u; } }
        Log("poollog frame %d building=%u dummy=%u object=%u ipls_loaded=%u", n, GetBuildingPool() ? (unsigned)GetBuildingPool()->GetNoOfUsedSpaces() : 0u, GetDummyPool() ? (unsigned)GetDummyPool()->GetNoOfUsedSpaces() : 0u, GetObjectPool() ? (unsigned)GetObjectPool()->GetNoOfUsedSpaces() : 0u, loadedIpls);
    }
    Log("memlog t=%lu ms frame %d pools task=%u route=%u event=%u ped=%u veh=%u: crt heap %lu bytes in %lu blocks, committed %lu KB", (unsigned long)GetTickCount(), n, GetTaskPool() ? (unsigned)GetTaskPool()->GetNoOfUsedSpaces() : 0u, GetPointRoutePool() ? (unsigned)GetPointRoutePool()->GetNoOfUsedSpaces() : 0u, GetEventPool() ? (unsigned)GetEventPool()->GetNoOfUsedSpaces() : 0u, GetPedPool() ? (unsigned)GetPedPool()->GetNoOfUsedSpaces() : 0u, GetVehiclePool() ? (unsigned)GetVehiclePool()->GetNoOfUsedSpaces() : 0u, crtHeapBytes, crtHeapBlocks, committedKB);
}

// A CPool::Delete of a pointer that is not inside the pool's storage (the exe would flip a random byte and set first-free to a wild index; diagnosing: who deletes it)
void PoolBadDelete(const void* pool, const void* obj, const void* storage, unsigned capacity, unsigned objSize, const void* caller) {
    Log("POOLBAD: Delete(0x%08X) outside pool %08X [storage %08X, %u x 0x%X]; caller 0x%08X (exe +0x%X)", (unsigned)(uintptr_t)obj, (unsigned)(uintptr_t)pool,
        (unsigned)(uintptr_t)storage, capacity, objSize, (unsigned)(uintptr_t)caller, (unsigned)((uintptr_t)caller - (uintptr_t)GetModuleHandleA(nullptr)));
}

// NOTSA_UNREACHABLE in the run build: log each site once (first 512 distinct sites) and continue (see Base.h)
void UnreachableHit(const char* fn, const char* file, int line) {
    struct Site { const char* File; int Line; };
    static Site s_seen[512];
    static int  s_n;
    static long s_lock;
    while (InterlockedExchange(&s_lock, 1)) {
        Sleep(0);
    }
    bool known = false;
    for (int i = 0; i < s_n && !known; i++) {
        known = s_seen[i].File == file && s_seen[i].Line == line;
    }
    if (!known && s_n < 512) {
        s_seen[s_n++] = { file, line };
    } else {
        known = true; // table full: stay silent
    }
    InterlockedExchange(&s_lock, 0);
    if (!known) {
        Log("UNREACHABLE hit: %s (%s:%d)", fn, file, line);
    }
}

[[noreturn]] void Fatal(const char* fmt, ...) {
    char msg[1024];
    va_list va;
    va_start(va, fmt);
    wvsprintfA(msg, fmt, va);
    va_end(va);
    Log("FATAL: %s", msg);
    if (IsDebuggerPresent()) {
        __debugbreak();
    }
    TerminateProcess(GetCurrentProcess(), 3);
    for (;;) {}
}

#ifdef _DEBUG
// S5 diagnostics: NOTSA_STANDALONE_ALLOCTRACE=<min bytes> logs (module-relative return addresses) every 200th allocation >= min bytes (find per-frame leaks)
static int s_AllocMin = 0;
static int __cdecl AllocTraceHook(int allocType, void*, size_t size, int, long, const unsigned char*, int) {
    if (allocType == _HOOK_ALLOC && (int)size >= s_AllocMin) {
        static std::atomic<int> s_N{0};
        if (s_N.fetch_add(1) % 200 == 0) {
            void* bt[10];
            const USHORT n = CaptureStackBackTrace(1, 10, bt, nullptr);
            char line[400];
            int  o = wsprintfA(line, "alloc %u:", (unsigned)size);
            for (USHORT i = 0; i < n && o < 380; i++) {
                o += wsprintfA(line + o, " %08X", (unsigned)(uintptr_t)bt[i]);
            }
            Log("%s", line);
        }
    }
    return TRUE;
}
#endif

// S5 diagnostics: silent process ends (exit(), purecall, invalid CRT parameter, std::terminate) are logged with a stack via the exception filter
static void RaiseNamed(const char* what, DWORD code) {
    static bool s_Raised = false;
    Fixups::Log("process end requested: %s", what);
    if (!s_Raised) {
        s_Raised = true;
        RaiseException(code, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    }
}
static void __cdecl OnAtExit() { Fixups::Log("atexit handler ran (exit() or normal return from WinMain)"); }
static void __cdecl OnPurecall() { RaiseNamed("pure virtual function call", 0xE0AB0003u); }
static void __cdecl OnInvalidParam(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t) { RaiseNamed("invalid CRT parameter", 0xE0AB0004u); }
static void __cdecl OnTerminate() { RaiseNamed("std::terminate", 0xE0AB0005u); }

// WARNING: suspending the game thread (SuspendThread/GetThreadContext/ResumeThread) under Wine on Apple silicon corrupts its x87 register stack (TOP drifts; the
// next fcomp/fld/fstp then hit empty registers => NaN positions/speeds, junk comparisons, wanted level 5 via the military zone). Use for profiling only.
// S5 diagnostics: NOTSA_STANDALONE_SAMPLER=1 samples the main thread's EIP / caller chain (EBP walk, 12 frames) every ~10 ms and logs the hottest
// return addresses every 15 s (resolve against the .map: "profile" lines, address = absolute VA; subtract the base logged by the "image base" line at start-up)
#include <map>
#include <thread>
static void SamplerThread(HANDLE mainThread) {
    std::map<uint32_t, uint32_t> hist;       // innermost 'interesting' frame (first address in the exe image)
    std::map<uint32_t, uint32_t> inclusive;  // every frame of the chain (inclusive time)
    int samples = 0, sysSamples = 0;
    // the exe image (ASLR: the base is wherever the loader put it)
    const auto imgLo = (uint32_t)GetModuleHandleA(nullptr);
    const auto imgHi = imgLo + ((const IMAGE_NT_HEADERS32*)((const uint8_t*)imgLo + ((const IMAGE_DOS_HEADER*)imgLo)->e_lfanew))->OptionalHeader.SizeOfImage;
    Sleep(35000); // not during device creation: suspending the main thread inside wined3d init deadlocks it
    ULONGLONG last = GetTickCount64();
    for (;;) {
        Sleep(10);
        if (SuspendThread(mainThread) == (DWORD)-1) {
            continue;
        }
        CONTEXT c{};
        c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        uint32_t frames[14];
        int n = 0;
        // no CRT / heap calls while the main thread is suspended (it may hold the heap lock)
        if (GetThreadContext(mainThread, &c)) {
            frames[n++] = c.Eip;
            uint32_t ebp = c.Ebp;
            for (int i = 0; i < 13 && ebp && !(ebp & 3) && ebp > 0x10000; i++) {
                uint32_t pair[2];
                SIZE_T rd = 0;
                if (!ReadProcessMemory(GetCurrentProcess(), (void*)(uintptr_t)ebp, pair, 8, &rd) || rd != 8) break;
                frames[n++] = pair[1];
                if (pair[0] <= ebp) break;
                ebp = pair[0];
            }
        }
        ResumeThread(mainThread);
        if (n) {
            hist[frames[0]]++;
            if (frames[0] < imgLo || frames[0] >= imgHi) sysSamples++; // EIP in a system DLL (wined3d / ntdll / ...)
            for (int i = 0; i < n; i++) inclusive[frames[i]]++;
            samples++;
        }
        if (GetTickCount64() - last > 15000 && samples) {
            last = GetTickCount64();
            std::multimap<uint32_t, uint32_t, std::greater<>> top;
            for (auto& [a, k] : inclusive) { if (a >= imgLo && a < imgHi) top.insert({ k, a }); }
            Fixups::Log("profile: %d samples (%d with EIP outside the exe image = system DLLs / wined3d); top inclusive return addresses:", samples, sysSamples);
            int shown = 0;
            for (auto& [k, a] : top) {
                if (shown++ >= 25) break;
                Fixups::Log("  profile %5u/%u %08X", (unsigned)k, (unsigned)samples, a);
            }
            hist.clear(); inclusive.clear(); samples = 0; sysSamples = 0;
        }
    }
}

void InstallDiagnostics() {
    {   // F3: the exe is a normal relocatable image (no /FIXED, no placeholder): record where the loader put it (ASLR) so a profile/crash address can be resolved against the .map
        const auto base = (const uint8_t*)GetModuleHandleA(nullptr);
        const auto* nt  = (const IMAGE_NT_HEADERS32*)(base + ((const IMAGE_DOS_HEADER*)base)->e_lfanew);
        Log("image base 0x%08X (ASLR %s, size 0x%X)", (unsigned)(uintptr_t)base,
            (nt->OptionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE) ? "on" : "off", (unsigned)nt->OptionalHeader.SizeOfImage);
    }
    if (std::getenv("NOTSA_STANDALONE_SAMPLER")) {
        HANDLE h = nullptr;
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &h, 0, FALSE, DUPLICATE_SAME_ACCESS);
        std::thread(SamplerThread, h).detach();
    }
    std::atexit(OnAtExit);
    _set_purecall_handler(OnPurecall);
    _set_invalid_parameter_handler(OnInvalidParam);
    std::set_terminate(OnTerminate);
#ifdef _DEBUG
    if (const char* e = std::getenv("NOTSA_STANDALONE_ALLOCTRACE")) {
        s_AllocMin = std::atoi(e);
        _CrtSetAllocHook(AllocTraceHook);
    }
#endif
    // assert()/abort() must not open a modal message box (it hangs a headless/Wine run forever): print to stderr and end in AbortHandler -> log + exit
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    signal(SIGABRT, AbortHandler);
#ifdef _DEBUG
    _CrtSetReportHook(CrtReportHook);
    _CrtSetReportHookW2(_CRT_RPTHOOK_INSTALL, CrtReportHookW);
#endif
}
} // namespace Fixups
} // namespace notsa::standalone
#elif defined(NOTSA_STANDALONE_RUN) // NOTSA_PORTABLE (GTASA_PORTABLE, preset PortableMac, slice A-1 of .notes/STREAM_C_PLAN.md)
// The Win32 / SEH / CRT-debug diagnostics above are Windows-only. The portable build keeps the API of Fixups.h (Log, Fatal, ...), implemented with the C++ standard
// library only: lines go to stderr and to `standalone.log` in the working directory (NOTSA_STANDALONE_LOG=<path> overrides). No x87, no process-wide exception filter.
#include <atomic>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <mutex>
#include "Fixups.h"

namespace notsa::standalone {
namespace {
std::mutex& LogMutex() {
    static std::mutex s_Mutex;
    return s_Mutex;
}
void OnAtExit() { Fixups::Log("atexit handler ran (exit() or normal return from main)"); }
void OnTerminate() {
    Fixups::Log("std::terminate called");
    std::abort();
}
} // namespace

namespace Fixups {
void Log(const char* fmt, ...) {
    char line[1100];
    va_list va;
    va_start(va, fmt);
    const int n = std::vsnprintf(line, sizeof(line), fmt, va);
    va_end(va);
    if (n < 0) {
        return;
    }
    const std::lock_guard lock{ LogMutex() };
    std::fprintf(stderr, "%s\n", line);
    const char* path = std::getenv("NOTSA_STANDALONE_LOG");
    if (std::FILE* f = std::fopen(path ? path : "standalone.log", "ab")) {
        std::fprintf(f, "%s\n", line);
        std::fclose(f);
    }
}

void LogFpuState(const char* where) {
    Log("FPU[%s]: portable build (no x87 precision control; float semantics come from the compiler flags)", where);
}

void LogGameStateTrace(int, unsigned long, unsigned long, unsigned long) { /* Windows run build diagnostics only */ }

void PoolBadDelete(const void* pool, const void* obj, const void* storage, unsigned capacity, unsigned objSize, const void* caller) {
    Log("POOLBAD: Delete(%p) outside pool %p [storage %p, %u x 0x%X]; caller %p", obj, pool, storage, capacity, objSize, caller);
}

// NOTSA_UNREACHABLE in the run build: log each site once (first 512 distinct sites) and continue (see Base.h)
void UnreachableHit(const char* fn, const char* file, int line) {
    struct Site { const char* File; int Line; };
    static Site s_seen[512];
    static int  s_n;
    bool known = false;
    {
        const std::lock_guard lock{ LogMutex() };
        for (int i = 0; i < s_n && !known; i++) {
            known = s_seen[i].File == file && s_seen[i].Line == line;
        }
        if (!known && s_n < 512) {
            s_seen[s_n++] = { file, line };
        } else {
            known = true; // table full: stay silent
        }
    }
    if (!known) {
        Log("UNREACHABLE hit: %s (%s:%d)", fn, file, line);
    }
}

[[noreturn]] void Fatal(const char* fmt, ...) {
    char msg[1024];
    va_list va;
    va_start(va, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, va);
    va_end(va);
    Log("FATAL: %s", msg);
    std::abort();
}

void InstallDiagnostics() {
    std::atexit(OnAtExit);
    std::set_terminate(OnTerminate);
}
} // namespace Fixups
} // namespace notsa::standalone
#endif
