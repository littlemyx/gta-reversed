// MAC-W0a: portable replacements of small Win32 facilities used by the app/platform layer (see app_platform.h). Portable-only file.
#include "StdInc.h"

#include <SDL3/SDL.h>
#include <climits>
#include <cstring>
#include <filesystem>
#include <string_view>

#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <unistd.h>
#include <execinfo.h>
#ifdef __APPLE__
#include <mach/mach.h>
#endif

#include "app_platform.h"
#include <extensions/debug.hpp>

namespace notsa::portable {

bool AcquireSingleInstance() {
    // Win32: a named event + FindWindow. Here: an advisory lock on a file in the temp directory, held until the process exits.
    static int s_Fd = -1;
    if (s_Fd != -1) {
        return true;
    }
    const auto path = (std::filesystem::temp_directory_path() / "gta_sa_reversed.lock").string();
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

// ---- crash handler (replaces SetUnhandledExceptionFilter + WindowsExceptionHandler of app_debug.cpp) ----
static void CrashHandler(int sig, siginfo_t* info, void*) {
    static volatile sig_atomic_t s_Handled = 0; // a crash inside the handler must not recurse
    if (s_Handled) {
        _exit(128 + sig);
    }
    s_Handled = 1;

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
}

} // namespace notsa::portable
