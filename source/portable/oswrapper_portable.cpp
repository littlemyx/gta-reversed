// Portable (non-Windows) implementation of oswrapper.h, replaces oswrapper/oswrapper_win.cpp in the PortableMac build (MAC-W0).
// Built ONLY with GTASA_PORTABLE=ON (source/CMakeLists.txt globs source/portable/*.cpp for the portable target only).
//
// Straight equivalents: std::thread / std::mutex / std::condition_variable / stdio / std::chrono.
// Deviations from oswrapper_win.cpp (all deliberate):
//  * OS_File* work on a FILE* (the Win32 version passes `&file` (the address of the by-value parameter) as a HANDLE to ReadFile/WriteFile/
//    CloseHandle, which is a bug there; the functions have no caller in the game).
//  * W-B1: thread priorities are accepted and ignored (std::thread has none; pthread priorities on macOS are not comparable with Win32's).
//  * OS_TimeAccurate/OS_TimeMS keep the formulas of the Win32 version (counter / 1000, counter / frequency) on a 1 GHz steady clock.
#include "StdInc.h"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

#include "oswrapper.h"
#include "winshim/ShlObj.h"

namespace {
char g_BasePath[_MAX_PATH]{};

constexpr uint32 kMutexMagic = 0x4D58544Du, kThreadMagic = 0x44485448u;

struct OsMutex {
    uint32               magic{kMutexMagic};
    std::recursive_mutex m; // Win32 mutexes are recursive
};

struct OsSemaphore {
    std::mutex              m;
    std::condition_variable cv;
    int32                   count{};
    int32                   max{1};
};

struct OsThreadGate { // shared with the thread so that OS_ThreadClose may free the handle while the thread still runs
    std::mutex              m;
    std::condition_variable cv;
    bool                    started{};
    bool                    finished{};
};

struct OsThread {
    uint32                         magic{kThreadMagic};
    std::thread                    thread;
    std::shared_ptr<OsThreadGate> gate{std::make_shared<OsThreadGate>()};
};

void W_B1_PriorityIgnored() {
    static bool logged = false;
    if (!std::exchange(logged, true)) {
        NOTSA_LOG_WARN("W-B1: OS_ThreadLaunch thread priorities are ignored on this platform");
    }
}
} // namespace

uint64 GetOSWPerformanceTime() {
    return (uint64)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

int64 GetOSWPerformanceFrequency() {
    return 1'000'000'000;
}

void OS_DebugOut(const char* msg) {
    std::fprintf(stderr, "%s\n", msg);
}

const char* OS_FileGetArchiveName(int32 /*archive*/) {
    return nullptr; // as oswrapper_win.cpp (AND_FileGetArchiveName)
}

int32 OS_FileSize(void* file) {
    const auto f = (FILE*)file;
    const auto pos = std::ftell(f);
    std::fseek(f, 0, SEEK_END);
    const auto size = std::ftell(f);
    std::fseek(f, pos, SEEK_SET);
    return (int32)size;
}

int32 OS_FileDelete(const char* file) {
    return std::remove(file) == 0; // like DeleteFileA: nonzero on success
}

int32 OS_FileOpen(OSFileDataArea /*dataArea*/, void** pfile, const char* file, OSFileAccessType access) {
    std::string path = g_BasePath;
    if (!path.empty() && path.back() != '/' && path.back() != '\\') {
        path += '/';
    }
    path += file;
    const char* mode = access == FILE_ACCESS_WRITE ? "wb" : access == FILE_ACCESS_EXISTING ? "rb+" : "rb";
    const auto  f    = std::fopen(path.c_str(), mode);
    if (!f) {
        *pfile = nullptr;
        return errno ? errno : 1;
    }
    *pfile = f;
    errno  = 0;
    return 0;
}

int32 OS_FileGetPosition(void* file) {
    return (int32)std::ftell((FILE*)file);
}

void OS_FileSetPosition(void* file, int32 pos) {
    std::fseek((FILE*)file, pos, SEEK_SET);
}

int32 OS_FileRead(void* file, void* dest, int32 destSize) {
    return (int32)std::fread(dest, 1, destSize, (FILE*)file) == destSize ? 0 : 3;
}

int32 OS_FileWrite(void* file, const void* src, int32 srcSize) {
    return (int32)std::fwrite(src, 1, srcSize, (FILE*)file) == srcSize ? 0 : 3;
}

int32 OS_FileClose(void* file) {
    return file ? std::fclose((FILE*)file) == 0 : 0; // like CloseHandle: nonzero on success
}

void OS_DebugBreak() {
    __builtin_trap();
}

void OS_ThreadSleep(uint32 ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void* OS_SemaphoreCreate(int32 iMaximumCount, const char* /*name*/) {
    const auto s = new OsSemaphore;
    s->max       = iMaximumCount;
    return s; // initial count 0, as CreateSemaphore(nullptr, 0, max, ...)
}

bool OS_SemaphorePost(void* semaphore) {
    const auto s = (OsSemaphore*)semaphore;
    {
        std::lock_guard lock{s->m};
        if (s->count >= s->max) {
            return false; // ReleaseSemaphore fails past the maximum count
        }
        ++s->count;
    }
    s->cv.notify_one();
    return true;
}

void OS_SemaphoreWait(void* semaphore) {
    const auto      s = (OsSemaphore*)semaphore;
    std::unique_lock lock{s->m};
    s->cv.wait(lock, [s] { return s->count > 0; });
    --s->count;
}

void OS_SemaphoreDelete(void* semaphore) {
    delete (OsSemaphore*)semaphore;
}

void* OS_MutexCreate(const char* /*name*/) {
    return new OsMutex;
}

void OS_MutexRelease(void* mutex) {
    // CAEStreamThread::~CAEStreamThread passes the address of a CRITICAL_SECTION member here (not a mutex): ReleaseMutex fails on such a handle, so does this
    if (!mutex || ((OsMutex*)mutex)->magic != kMutexMagic) {
        // stderr, not the game logger: this runs from a static destructor at exit (CAEAudioHardware -> ~CAEStreamThread) when spdlog's logger is already gone
        // (MAC-W2: null deref -> SIGSEGV -> exit 139/134 after cmd:quit)
        static bool s_Warned = false;
        if (!std::exchange(s_Warned, true)) {
            std::fprintf(stderr, "OS_MutexRelease: not an OS mutex handle (%p), ignored (further occurrences are silent)\n", mutex);
        }
        return;
    }
    ((OsMutex*)mutex)->m.unlock();
}

void OS_MutexObtain(void* mutex) {
    assert(((OsMutex*)mutex)->magic == kMutexMagic);
    ((OsMutex*)mutex)->m.lock();
}

void OS_MutexDelete(void* mutex) {
    delete (OsMutex*)mutex;
}

void OS_SetFilePathOffset(const char* path) {
    if (!path) {
        g_BasePath[0] = '\0';
        return;
    }
    strcpy_s(g_BasePath, path);
}

void* OS_ThreadLaunch(OS_ThreadRoutine pfnStart, void* param, uint32 nFlags, const char* name, void* /*hUnknown*/, OSThreadPriority priority) {
    if (priority != OS_THREAD_PRIORITY_NORMAL) {
        W_B1_PriorityIgnored();
    }
    const auto t = new OsThread;
    t->gate->started = !(nFlags & OS_CREATE_SUSPENDED);
    t->thread        = std::thread([gate = t->gate, pfnStart, param] {
        {
            std::unique_lock lock{gate->m};
            gate->cv.wait(lock, [&] { return gate->started; }); // CREATE_SUSPENDED: wait for OS_ThreadResume
        }
        pfnStart(param);
        {
            std::lock_guard lock{gate->m};
            gate->finished = true;
        }
        gate->cv.notify_all();
    });
    NOTSA_LOG_DEBUG("OS: Thread ({}) created", name ? name : "");
    return t;
}

void OS_ThreadClose(void* thread) {
    const auto t = (OsThread*)thread;
    if (t->thread.joinable()) {
        t->thread.detach(); // CloseHandle does not stop or join the thread
    }
    delete t;
}

void OS_ThreadWait(void* thread) {
    const auto t = (OsThread*)thread;
    if (t->thread.joinable()) {
        t->thread.join();
    }
}

void OS_ThreadResume(void* thread) {
    const auto t = (OsThread*)thread;
    {
        std::lock_guard lock{t->gate->m};
        t->gate->started = true;
    }
    t->gate->cv.notify_all();
}

double OS_TimeAccurate() {
    return GetOSWPerformanceTime() / double(1000.f); // as oswrapper_win.cpp (counter / 1000)
}

uint32 OS_TimeMS() {
    return uint32(GetOSWPerformanceTime() / GetOSWPerformanceFrequency()); // as oswrapper_win.cpp (counter / frequency)
}

// ---- Win32 thread API (declared in portable/winapi_compat.h) over the same OsThread handles ----
static OsThread* AsThread(HANDLE h, const char* fn) {
    if (!h || h == INVALID_HANDLE_VALUE || ((OsThread*)h)->magic != kThreadMagic) {
        NOTSA_LOG_ERR("{}: {} is not a thread handle (only thread handles exist on this platform)", fn, h);
        return nullptr;
    }
    return (OsThread*)h;
}

HANDLE CreateThread(void*, SIZE_T, LPTHREAD_START_ROUTINE start, LPVOID param, DWORD flags, LPDWORD threadId) {
    if (threadId) {
        *threadId = 0; // W-B3: std::thread has no numeric id of the new thread before it runs
    }
    return OS_ThreadLaunch(start, param, (flags & CREATE_SUSPENDED) ? OS_CREATE_SUSPENDED : 0, nullptr);
}

DWORD ResumeThread(HANDLE thread) {
    if (!AsThread(thread, "ResumeThread")) {
        return (DWORD)-1;
    }
    OS_ThreadResume(thread);
    return 1; // previous suspend count of a thread created suspended (the only use)
}

DWORD SuspendThread(HANDLE) {
    NOTSA_LOG_ERR("W-B2: SuspendThread is not available on this platform (CAEStreamThread::Pause is a no-op failure)");
    return (DWORD)-1;
}

BOOL SetThreadPriority(HANDLE, int) {
    W_B1_PriorityIgnored();
    return TRUE;
}

DWORD WaitForSingleObject(HANDLE handle, DWORD ms) {
    const auto t = AsThread(handle, "WaitForSingleObject");
    if (!t) {
        return WAIT_FAILED;
    }
    std::unique_lock lock{t->gate->m};
    if (ms == INFINITE) {
        t->gate->cv.wait(lock, [&] { return t->gate->finished; });
        lock.unlock();
        if (t->thread.joinable()) {
            t->thread.join();
        }
        return WAIT_OBJECT_0;
    }
    return t->gate->cv.wait_for(lock, std::chrono::milliseconds(ms), [&] { return t->gate->finished; }) ? WAIT_OBJECT_0 : 0x102 /*WAIT_TIMEOUT*/;
}

BOOL CloseHandle(HANDLE handle) {
    if (!AsThread(handle, "CloseHandle")) {
        return FALSE;
    }
    OS_ThreadClose(handle);
    return TRUE;
}

BOOL TerminateThread(HANDLE, DWORD) {
    NOTSA_LOG_ERR("W-B6: TerminateThread is not available on this platform, the thread is left running");
    return FALSE;
}

// ---- critical sections ----
namespace {
std::recursive_mutex* CsMutex(CRITICAL_SECTION* cs) {
    static std::mutex s_lazyInit; // a zero-filled CS that is entered without InitializeCriticalSection (undefined on Win32) is initialised on first use
    std::lock_guard   lock{s_lazyInit};
    if (!cs->DebugInfo) {
        cs->DebugInfo = new std::recursive_mutex;
    }
    return (std::recursive_mutex*)cs->DebugInfo;
}
} // namespace

void InitializeCriticalSection(CRITICAL_SECTION* cs) {
    cs->DebugInfo = nullptr;
    CsMutex(cs);
}
void EnterCriticalSection(CRITICAL_SECTION* cs) { CsMutex(cs)->lock(); }
void LeaveCriticalSection(CRITICAL_SECTION* cs) { CsMutex(cs)->unlock(); }
void DeleteCriticalSection(CRITICAL_SECTION* cs) {
    delete (std::recursive_mutex*)std::exchange(cs->DebugInfo, nullptr);
}

// ---- code pages (UTF-8 <-> UTF-32 wchar_t) ----
namespace {
bool Utf8Supported(UINT cp, const char* fn) {
    if (cp == CP_UTF8 || cp == CP_ACP) {
        return true;
    }
    NOTSA_LOG_ERR("{}: code page {} is not supported on this platform", fn, cp);
    return false;
}
} // namespace

int MultiByteToWideChar(UINT codePage, DWORD flags, LPCSTR src, int srcLen, LPWSTR dst, int dstLen) {
    if (!Utf8Supported(codePage, "MultiByteToWideChar")) {
        return 0;
    }
    const size_t len = srcLen < 0 ? std::strlen(src) + 1 : (size_t)srcLen; // -1: NUL-terminated, the NUL is converted too
    int          out = 0;
    for (size_t i = 0; i < len;) {
        const uint8 c = (uint8)src[i];
        uint32      cp;
        size_t      n;
        if (c < 0x80)                { cp = c;        n = 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; n = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; n = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; n = 4; }
        else                         { cp = 0xFFFD;   n = 1; if (flags & MB_ERR_INVALID_CHARS) return 0; }
        if (n > 1) {
            if (i + n > len) {
                if (flags & MB_ERR_INVALID_CHARS) return 0;
                cp = 0xFFFD;
                n  = len - i;
            } else {
                for (size_t k = 1; k < n; k++) {
                    if (((uint8)src[i + k] & 0xC0) != 0x80) {
                        if (flags & MB_ERR_INVALID_CHARS) return 0;
                        cp = 0xFFFD;
                        n  = k;
                        break;
                    }
                    cp = (cp << 6) | ((uint8)src[i + k] & 0x3F);
                }
            }
        }
        if (dstLen) {
            if (out >= dstLen) return 0; // ERROR_INSUFFICIENT_BUFFER
            dst[out] = (wchar_t)cp;
        }
        out++;
        i += n;
    }
    return out;
}

int WideCharToMultiByte(UINT codePage, DWORD flags, LPCWSTR src, int srcLen, LPSTR dst, int dstLen, LPCSTR, LPBOOL usedDefault) {
    if (!Utf8Supported(codePage, "WideCharToMultiByte")) {
        return 0;
    }
    if (usedDefault) {
        *usedDefault = FALSE;
    }
    const size_t len = srcLen < 0 ? std::wcslen(src) + 1 : (size_t)srcLen;
    int          out = 0;
    const auto   put = [&](uint8 b) -> bool {
        if (dstLen) {
            if (out >= dstLen) return false;
            dst[out] = (char)b;
        }
        out++;
        return true;
    };
    for (size_t i = 0; i < len; i++) {
        uint32 cp = (uint32)src[i];
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            if (flags & WC_ERR_INVALID_CHARS) return 0;
            cp = 0xFFFD;
        }
        bool ok;
        if (cp < 0x80)         ok = put((uint8)cp);
        else if (cp < 0x800)   ok = put(0xC0 | (cp >> 6)) && put(0x80 | (cp & 0x3F));
        else if (cp < 0x10000) ok = put(0xE0 | (cp >> 12)) && put(0x80 | ((cp >> 6) & 0x3F)) && put(0x80 | (cp & 0x3F));
        else                   ok = put(0xF0 | (cp >> 18)) && put(0x80 | ((cp >> 12) & 0x3F)) && put(0x80 | ((cp >> 6) & 0x3F)) && put(0x80 | (cp & 0x3F));
        if (!ok) return 0;
    }
    return out;
}

HRESULT SHGetFolderPathW(HWND, int csidl, HANDLE, DWORD, LPWSTR path) {
    if (csidl != CSIDL_PERSONAL) {
        NOTSA_LOG_ERR("SHGetFolderPathW: CSIDL {:#x} is not supported on this platform", csidl);
        return E_INVALIDARG;
    }
    const char* home = std::getenv("HOME");
    if (!home || !*home) {
        return E_FAIL;
    }
    const std::string docs = std::string{home} + "/Documents";
    if (docs.size() >= MAX_PATH) {
        return E_FAIL;
    }
    // MAX_PATH wchar_t buffer, as the Win32 contract
    const int n = MultiByteToWideChar(CP_UTF8, 0, docs.c_str(), -1, path, MAX_PATH);
    return n ? S_OK : E_FAIL;
}
