// MAC-W0a: portable replacements of small Win32 facilities used by the app/platform layer (see app_platform.h). Portable-only file.
#include "StdInc.h"

#include <SDL3/SDL.h>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <thread>
#include <cstring>
#include <filesystem>
#include <string_view>

#include <signal.h>
#ifdef _WIN32
// MAC-WIN64: real Windows SDK (StdInc.h -> winincl.h -> <Windows.h>); the POSIX facilities below have Win32 twins
#else
#include <pthread.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <execinfo.h>
#ifdef __APPLE__
#include <mach/mach.h>
#endif
#endif

#include "app_platform.h"
#include <extensions/debug.hpp>

namespace notsa::portable {

RwImage* GrabScreenBlank(RwCamera* camera) {
    Unsupported("psGrabScreen: no GL3 front-buffer read-back yet, returning a black image");
    const auto* raster = camera ? RwCameraGetRaster(camera) : nullptr;
    const int32 w = raster ? RwRasterGetWidth(raster) : RsGlobal.maximumWidth;
    const int32 h = raster ? RwRasterGetHeight(raster) : RsGlobal.maximumHeight;
    RwImage* image = RwImageCreate(w, h, 32);
    if (image) {
        RwImageAllocatePixels(image);
        if (auto* px = RwImageGetPixels(image)) {
            for (int32 i = 0; i < w * h; i++) { // opaque black (the memory from RwImageAllocatePixels is not guaranteed zeroed)
                reinterpret_cast<RwRGBA*>(px)[i] = RwRGBA{ 0, 0, 0, 255 };
            }
        }
    }
    return image;
}

#ifdef _WIN32
bool AcquireSingleInstance() {
    // Win32: a named mutex held until the process exits (the exe's own check is a named event + FindWindow)
    static HANDLE s_Mutex = nullptr;
    if (s_Mutex) {
        return true;
    }
    const char* own = std::getenv("NOTSA_LOCK_FILE");
    std::string name = "Local\\gta_sa_reversed_instance";
    if (own && *own) { // tests / the RR harness run several instances side by side, each with its own name
        name = "Local\\gta_sa_reversed_";
        for (const char* c = own; *c; c++) {
            name += (std::isalnum((unsigned char)*c) ? *c : '_');
        }
    }
    s_Mutex = ::CreateMutexA(nullptr, TRUE, name.c_str());
    if (!s_Mutex) {
        return true; // cannot tell: do not refuse to start
    }
    if (::GetLastError() == ERROR_ALREADY_EXISTS) {
        ::CloseHandle(s_Mutex);
        s_Mutex = nullptr;
        return false;
    }
    return true;
}
#else
bool AcquireSingleInstance() {
    // Win32: a named event + FindWindow. Here: an advisory lock on a file in the temp directory, held until the process exits.
    static int s_Fd = -1;
    if (s_Fd != -1) {
        return true;
    }
    auto path = (std::filesystem::temp_directory_path() / "gta_sa_reversed.lock").string();
    if (const char* own = std::getenv("NOTSA_LOCK_FILE"); own && *own) { // tests / the RR harness run several instances of different builds side by side, each with its own lock file
        path = own;
    }
    const int  fd   = ::open(path.c_str(), O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
        NOTSA_LOG_WARN("AcquireSingleInstance: cannot open '{}', assuming the only instance", path);
        return true; // cannot tell: do not refuse to start
    }
    if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd);
        return false;
    }
    s_Fd = fd;
    return true;
}
#endif

#ifdef _WIN32
char* FindDvdGamePath() {
    // Win32: scan the CD-ROM drives for a volume labelled GTA_SAN_ANDREAS
    DWORD drives = ::GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (!(drives & (1u << i))) {
            continue;
        }
        char root[] = { char('A' + i), ':', '\\', '\0' };
        if (::GetDriveTypeA(root) != DRIVE_CDROM) {
            continue;
        }
        char label[MAX_PATH]{};
        if (::GetVolumeInformationA(root, label, sizeof(label), nullptr, nullptr, nullptr, nullptr, 0) && std::string_view{label} == "GTA_SAN_ANDREAS") {
            const auto result = new char[4];
            std::memcpy(result, root, 4);
            return result;
        }
    }
    return nullptr;
}
#else
char* FindDvdGamePath() {
    // Win32: scan the CD-ROM drives for a volume labelled GTA_SAN_ANDREAS. macOS mounts optical media (and disk images) below /Volumes/<label>.
    static constexpr std::string_view mount = "/Volumes/GTA_SAN_ANDREAS/";
    std::error_code ec;
    if (!std::filesystem::is_directory(mount.substr(0, mount.size() - 1), ec)) {
        return nullptr;
    }
    const auto result = new char[mount.size() + 1];
    std::memcpy(result, mount.data(), mount.size());
    result[mount.size()] = '\0';
    return result;
}
#endif

LangIds GetLangIds() {
    // Win32 primary language ids: LANG_ENGLISH 0x09, GERMAN 0x07, SPANISH 0x0A, FRENCH 0x0C, ITALIAN 0x10
    uint16_t id = 0x09;
    int      n  = 0;
    if (SDL_Locale** locales = SDL_GetPreferredLocales(&n); locales) {
        if (n > 0 && locales[0] && locales[0]->language) {
            const std::string_view lang = locales[0]->language;
            if      (lang == "de") { id = 0x07; }
            else if (lang == "es") { id = 0x0A; }
            else if (lang == "fr") { id = 0x0C; }
            else if (lang == "it") { id = 0x10; }
        }
        SDL_free(locales);
    }
    return { id, id, id };
}

#ifdef _WIN32
void QueryPhysMemory(size_t& total, size_t& avail) {
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (::GlobalMemoryStatusEx(&ms)) {
        total = (size_t)ms.ullTotalPhys;
        avail = (size_t)ms.ullAvailPhys;
    } else {
        total = avail = 0;
    }
    if (!avail) {
        avail = total;
    }
}
#else
void QueryPhysMemory(size_t& total, size_t& avail) {
    const auto page = (size_t)::sysconf(_SC_PAGESIZE);
    total = (size_t)::sysconf(_SC_PHYS_PAGES) * page;
    avail = 0;
#ifdef __APPLE__
    vm_statistics64_data_t vm{};
    mach_msg_type_number_t cnt = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &cnt) == KERN_SUCCESS) {
        avail = (size_t)(vm.free_count + vm.inactive_count) * page;
    }
#endif
    if (!avail) { // could not tell: report all of it rather than 0 (a 0 would starve the streaming budget)
        avail = total;
    }
}
#endif

std::pair<uint32_t, uint32_t> GetNativeResolution() {
    if (const auto* m = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay())) {
        return { (uint32_t)m->w, (uint32_t)m->h };
    }
    return { 0, 0 };
}

uint32_t GetBestRefreshRate(uint32_t w, uint32_t h) {
    uint32_t best = UINT_MAX;
    int      n    = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), &n);
    for (int i = 0; modes && i < n; i++) {
        const auto* m = modes[i];
        if ((uint32_t)m->w != w || (uint32_t)m->h != h) {
            continue;
        }
        const auto rr = (uint32_t)(m->refresh_rate + 0.5f);
        if (rr == 0) {
            best = 0;
            break;
        }
        if (rr < best && rr >= 60) {
            best = rr;
        }
    }
    SDL_free(modes);
    return best;
}

#ifdef _WIN32
// ---- crash handler: SetUnhandledExceptionFilter (the exe's own filter lives in app_debug.cpp, which the portable target does not use) ----
static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep) {
    static volatile LONG s_Handled = 0; // a crash inside the handler must not recurse
    if (::InterlockedExchange(&s_Handled, 1)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    char line[160];
    const int len = std::snprintf(line, sizeof(line), "\n[crash] exception %08lx at %p\n", ep ? (unsigned long)ep->ExceptionRecord->ExceptionCode : 0ul,
                                  ep ? ep->ExceptionRecord->ExceptionAddress : nullptr);
    if (len > 0) { DWORD w; ::WriteFile(::GetStdHandle(STD_ERROR_HANDLE), line, (DWORD)len, &w, nullptr); }
    spdlog::apply_all([](auto&& logger) { logger->dump_backtrace(); });
    SPDLOG_INFO("*********UNHANDLED EXCEPTION**********");
    if (ep) {
        SPDLOG_INFO("Code: {:#010x} at {:#018x}", (uint32_t)ep->ExceptionRecord->ExceptionCode, (uintptr_t)ep->ExceptionRecord->ExceptionAddress);
#ifdef _M_X64
        // x64 has no frame chain to walk without unwind info lookups: log the image base, RIP and the stack words that look like return addresses into the exe
        // (a call through a null function pointer faults at RIP 0 with the caller's return address at [rsp]); resolve with the link map (gta_reversed.map)
        const auto base = (uintptr_t)::GetModuleHandleW(nullptr);
        SPDLOG_INFO("Image base {:#018x}, RIP {:#018x}, RSP {:#018x}", base, (uintptr_t)ep->ContextRecord->Rip, (uintptr_t)ep->ContextRecord->Rsp);
        const auto*     sp   = reinterpret_cast<const uintptr_t*>(ep->ContextRecord->Rsp);
        const auto      size = (uintptr_t)reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew)->OptionalHeader.SizeOfImage;
        int             n    = 0;
        for (int i = 0; i < 512 && n < 24; i++) {
            if (!::IsBadReadPtr(sp + i, sizeof(uintptr_t)) && sp[i] >= base && sp[i] < base + size) {
                SPDLOG_INFO("\tstack[{}] exe+{:#x}", i, sp[i] - base);
                n++;
            }
        }
#endif
    }
    spdlog::apply_all([](auto&& logger) { logger->flush(); });
    return EXCEPTION_CONTINUE_SEARCH;
}

void InstallCrashHandler() {
    ::SetUnhandledExceptionFilter(CrashFilter);
}
#else
// ---- crash handler (replaces SetUnhandledExceptionFilter + WindowsExceptionHandler of app_debug.cpp) ----
static void CrashHandler(int sig, siginfo_t* info, void* uctx) {
    static volatile sig_atomic_t s_Handled = 0; // a crash inside the handler must not recurse
    if (s_Handled) {
        _exit(128 + sig);
    }
    s_Handled = 1;

    // MAC-BOOT1: raw first (no logger, no allocation): the logger dump below has itself crashed on a corrupted heap
    {
        char line[160];
        const int len = std::snprintf(line, sizeof(line), "\n[crash] signal %d (%s), fault address %p\n", sig, strsignal(sig), info ? info->si_addr : nullptr);
        if (len > 0) { (void)!::write(2, line, (size_t)len); }
        void* raw[64];
        const int rn = ::backtrace(raw, 64);
        ::backtrace_symbols_fd(raw, rn, 2);
#if defined(__APPLE__) && defined(__aarch64__)
        // the faulting thread's own frames: pc, lr and the frame-pointer chain of the interrupted code (backtrace() above starts in the handler)
        if (uctx) {
            const auto& ss = ((ucontext_t*)uctx)->uc_mcontext->__ss;
            void* fr[40]; int fn = 0;
            fr[fn++] = (void*)__darwin_arm_thread_state64_get_pc(ss);
            fr[fn++] = (void*)__darwin_arm_thread_state64_get_lr(ss);
            auto* fp = (void**)__darwin_arm_thread_state64_get_fp(ss);
            while (fp && fn < 40 && ((uintptr_t)fp & 7) == 0 && (uintptr_t)fp > 0x1000) {
                fr[fn++] = fp[1];
                auto* next = (void**)fp[0];
                if (next <= fp) break;
                fp = next;
            }
            const char* hdr = "[crash] interrupted-code frames (pc, lr, fp chain):\n";
            (void)!::write(2, hdr, std::strlen(hdr));
            ::backtrace_symbols_fd(fr, fn, 2);
        }
#else
        (void)uctx;
#endif
    }

    spdlog::apply_all([](auto&& logger) { logger->dump_backtrace(); });
    SPDLOG_INFO("*********UNHANDLED SIGNAL**********");
    SPDLOG_INFO("Signal: {} ({})", sig, strsignal(sig));
    SPDLOG_INFO("Fault address: {:#018x}", (uintptr_t)(info ? info->si_addr : nullptr));
    SPDLOG_INFO("*********CALL STACK**********");
    void* frames[128];
    const int n = ::backtrace(frames, 128);
    for (int i = 0; i < n; i++) {
        NOTSA_LOG_INFO("\t{}", notsa::debug::GetFunctionInfoAtAddress((uintptr_t)frames[i]));
    }
    spdlog::apply_all([](auto&& logger) { logger->flush(); });

    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

void InstallCrashHandler() {
    struct sigaction sa{};
    sa.sa_sigaction = CrashHandler;
    sa.sa_flags     = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    for (const int sig : { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP }) {
        ::sigaction(sig, &sa, nullptr);
    }
    // MAC-BOOT1: debuggers cannot attach to this exe on the dev Mac (lldb/sample hang), so a hang is diagnosed in process:
    // NOTSA_HANG_DUMP_SECS=<n> makes the main thread take SIGABRT after n seconds, i.e. its call stack goes through the handler above and the process dies.
    if (const char* secs = std::getenv("NOTSA_HANG_DUMP_SECS"); secs && std::atoi(secs) > 0) {
        const auto mainThread = ::pthread_self();
        const int  n          = std::atoi(secs);
        std::thread([mainThread, n] {
            std::this_thread::sleep_for(std::chrono::seconds(n));
            ::pthread_kill(mainThread, SIGABRT);
        }).detach();
    }
}
#endif // _WIN32

} // namespace notsa::portable
