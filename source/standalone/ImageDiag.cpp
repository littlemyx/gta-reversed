#include "StdInc.h"

// A3 of .notes/DETACH_DATA_PLAN.md: diagnostics on the original data image of the standalone run build. Armed once from WinMain (after
// Fixups::ApplyToDataImage), both are off unless their env variable is set:
//   NOTSA_POISON_CONVERTED=<file>   fills the image bytes of already-converted globals with 0xDD. The file is a list of `0xADDR<ws>SIZE...` lines
//                                   (`#` lines skipped); the NOTSA_VERIFY_GLOBALS boot dump of the detached build is exactly that format. Only useful in a
//                                   detached build: a converted global no longer lives in the image, so any code still reading the old address now sees 0xDD.
//   NOTSA_IMAGE_CENSUS=1            protects the image page-wise (PAGE_NOACCESS), the first access to a page is logged (file NOTSA_IMAGE_CENSUS_FILE,
//                                   default image_census.tsv next to the exe: page, address, access, tick, thread, IP, two return-address candidates) and the
//                                   page gets its original protection back. NOTSA_IMAGE_CENSUS_REARM_MS=<ms> protects all pages again every <ms> (new
//                                   (address, IP) pairs only are logged), tools/standalone/census.py turns the file into .notes/DETACH_census.tsv.
// Caveat of the census: a kernel-side access to a protected page (ReadFile/GetKeyboardState into a global buffer) fails with an error code instead of faulting.
#ifdef NOTSA_STANDALONE_RUN
#include <cstdlib>
#include <thread>
#include "DataImage.h"
#include "Fixups.h"

namespace notsa::standalone::DataImage {
namespace {
constexpr uint32_t PAGE = 0x1000;

// ---- poison -----------------------------------------------------------------------------------------------------------------------------------------
void PoisonConverted(const char* listPath) {
    const auto& info = GetInfo();
    const HANDLE h = CreateFileA(listPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        Fixups::Log("NOTSA_POISON_CONVERTED: cannot open '%s'", listPath);
        return;
    }
    const DWORD size = GetFileSize(h, nullptr);
    std::string text(size, '\0');
    DWORD       got = 0;
    ReadFile(h, text.data(), size, &got, nullptr);
    CloseHandle(h);
    text.resize(got);

    unsigned ranges = 0, bytes = 0, skipped = 0;
    size_t   pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) {
            end = text.size();
        }
        const std::string line = text.substr(pos, end - pos);
        pos                    = end + 1;
        if (line.empty() || line[0] == '#') {
            continue;
        }
        char*          p;
        const uint32_t addr = (uint32_t)strtoul(line.c_str(), &p, 0);
        const uint32_t sz   = (uint32_t)strtoul(p, &p, 10);
        if (addr < info.DataBase || sz == 0 || addr + sz > info.DataEnd) {
            skipped++;
            continue;
        }
        DWORD old = 0, tmp;
        VirtualProtect((void*)addr, sz, PAGE_READWRITE, &old); // .rdata is read-only by now
        memset((void*)addr, 0xDD, sz);
        if (old && old != PAGE_READWRITE) {
            VirtualProtect((void*)addr, sz, old, &tmp);
        }
        ranges++;
        bytes += sz;
    }
    Fixups::Log("NOTSA_POISON_CONVERTED: %u ranges / %u bytes of the image filled with 0xDD (%u lines outside the data image skipped)", ranges, bytes, skipped);
}

// ---- census -----------------------------------------------------------------------------------------------------------------------------------------
struct Census {
    uint32_t Lo{}, Hi{}, NumPages{};
    uint32_t TextLo{}, TextHi{}; // our own code (the original-code placeholder is NOACCESS and never on a stack)
    HANDLE   File{INVALID_HANDLE_VALUE};
    volatile LONG Lines{};
    std::unique_ptr<volatile char[]> State;  // 1 = armed (protected)
    std::unique_ptr<DWORD[]>         Orig;   // protection before arming
    std::unique_ptr<uint64_t[]>      Seen;   // open addressing set of (address << 32 | IP)
    static constexpr uint32_t SEEN_SIZE = 1u << 18;
    volatile LONG SeenCount{};
    volatile LONG Faults{};
} g_C;

bool SeenBefore(uint64_t key) {
    // key 0 can not happen (address >= 0x858000)
    uint32_t h = (uint32_t)((key * 0x9E3779B97F4A7C15ull) >> 40) & (Census::SEEN_SIZE - 1);
    for (uint32_t n = 0; n < Census::SEEN_SIZE; n++, h = (h + 1) & (Census::SEEN_SIZE - 1)) {
        const uint64_t cur = g_C.Seen[h];
        if (cur == key) {
            return true;
        }
        if (cur == 0) {
            g_C.Seen[h] = key; // racy between threads by design: worst case one duplicate line
            InterlockedIncrement(&g_C.SeenCount);
            return false;
        }
    }
    return true; // table full: stop logging
}

bool LooksLikeReturnAddress(uint32_t ra) {
    if (ra < g_C.TextLo + 8 || ra >= g_C.TextHi) {
        return false;
    }
    const auto* c = (const uint8_t*)ra;
    if (c[-5] == 0xE8) {
        return true; // call rel32
    }
    for (int back : {2, 3, 6, 7}) { // call r/m32 (FF /2): reg, [reg+disp8], [abs32]/[reg+disp32], [reg+sib+disp8]/...
        if (c[-back] == 0xFF && (c[-back + 1] & 0x38) == 0x10) {
            return true;
        }
    }
    return false;
}

void Write(const char* line, int n) {
    DWORD w;
    WriteFile(g_C.File, line, n, &w, nullptr);
}

LONG CALLBACK CensusVEH(EXCEPTION_POINTERS* ep) {
    const EXCEPTION_RECORD* r = ep->ExceptionRecord;
    if (r->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || r->NumberParameters < 2) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const uint32_t addr = (uint32_t)r->ExceptionInformation[1];
    if (addr < g_C.Lo || addr >= g_C.Hi) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const uint32_t page   = (addr - g_C.Lo) / PAGE;
    const uint32_t access = (uint32_t)r->ExceptionInformation[0]; // 0 read, 1 write, 8 execute
    const DWORD    orig   = g_C.Orig[page];
    const uint32_t pageVA = g_C.Lo + page * PAGE;
    if (InterlockedExchange8((volatile char*)&g_C.State[page], 0) == 0) {
        // Not armed (or another thread just opened it): a fault the original protection allows is that race, anything else is a real violation
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery((void*)pageVA, &mbi, sizeof(mbi)) && mbi.Protect != PAGE_NOACCESS && (access == 0 || (access == 1 && mbi.Protect == PAGE_READWRITE))) {
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
    DWORD old;
    VirtualProtect((void*)pageVA, PAGE, orig, &old);
    InterlockedIncrement(&g_C.Faults);

    const uint32_t ip = (uint32_t)(uintptr_t)r->ExceptionAddress;
    if (SeenBefore(((uint64_t)addr << 32) | ip)) {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    uint32_t callers[2]{}, found = 0;
    const auto* sp = (const uint32_t*)ep->ContextRecord->Esp;
    for (int i = 0; i < 48 && found < 2; i++) {
        if (LooksLikeReturnAddress(sp[i])) {
            callers[found++] = sp[i];
        }
    }
    char line[200];
    const int n = wsprintfA(line, "%u\t0x%08X\t%c\t%u\t%u\t0x%08X\t0x%08X\t0x%08X\r\n", page, addr, access == 1 ? 'W' : access == 8 ? 'X' : 'R', GetTickCount(),
        GetCurrentThreadId(), ip, callers[0], callers[1]);
    Write(line, n);
    InterlockedIncrement(&g_C.Lines);
    return EXCEPTION_CONTINUE_EXECUTION;
}

void ArmAll() {
    for (uint32_t p = 0; p < g_C.NumPages; p++) {
        if (g_C.State[p]) {
            continue;
        }
        g_C.State[p] = 1;
        DWORD old;
        VirtualProtect((void*)(g_C.Lo + p * PAGE), PAGE, PAGE_NOACCESS, &old);
    }
}

void StartCensus() {
    const auto& info = GetInfo();
    g_C.Lo = info.DataBase;
    g_C.Hi = info.DataEnd;
    g_C.NumPages = (g_C.Hi - g_C.Lo + PAGE - 1) / PAGE;
    g_C.State.reset(new volatile char[g_C.NumPages]());
    g_C.Orig.reset(new DWORD[g_C.NumPages]());
    g_C.Seen.reset(new uint64_t[Census::SEEN_SIZE]());

    // our own code range = .text of this module minus the original-code placeholder at its start
    {
        const auto* base = (const uint8_t*)GetModuleHandleA(nullptr);
        const auto* nt   = (const IMAGE_NT_HEADERS32*)(base + ((const IMAGE_DOS_HEADER*)base)->e_lfanew);
        const auto* sec  = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; i++) {
            if (memcmp(sec[i].Name, ".text", 6) == 0) {
                g_C.TextLo = (uint32_t)(uintptr_t)base + sec[i].VirtualAddress + ORIG_PAD_SIZE;
                g_C.TextHi = (uint32_t)(uintptr_t)base + sec[i].VirtualAddress + sec[i].Misc.VirtualSize;
            }
        }
    }
    for (uint32_t p = 0; p < g_C.NumPages; p++) {
        MEMORY_BASIC_INFORMATION mbi{};
        VirtualQuery((void*)(g_C.Lo + p * PAGE), &mbi, sizeof(mbi));
        g_C.Orig[p] = mbi.Protect ? mbi.Protect : PAGE_READWRITE;
    }
    char path[MAX_PATH] = "image_census.tsv";
    if (const char* f = std::getenv("NOTSA_IMAGE_CENSUS_FILE"); f && *f) {
        lstrcpynA(path, f, MAX_PATH);
    }
    g_C.File = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_C.File == INVALID_HANDLE_VALUE) {
        Fixups::Log("NOTSA_IMAGE_CENSUS: cannot create '%s'", path);
        return;
    }
    char head[160];
    Write(head, wsprintfA(head, "# image census: image 0x%08X..0x%08X, %u pages\r\n# page\taddr\tacc\ttick\ttid\tip\tcaller1\tcaller2\r\n", g_C.Lo, g_C.Hi, g_C.NumPages));
    AddVectoredExceptionHandler(1, CensusVEH);
    ArmAll();
    uint32_t rearmMs = 0;
    if (const char* e = std::getenv("NOTSA_IMAGE_CENSUS_REARM_MS")) {
        rearmMs = (uint32_t)std::atoi(e);
    }
    if (rearmMs) {
        std::thread([rearmMs] {
            for (;;) {
                Sleep(rearmMs);
                ArmAll();
                Fixups::Log("census: re-armed, %ld distinct (address, IP) pairs logged so far, %ld faults", g_C.SeenCount, g_C.Faults);
            }
        }).detach();
    }
    Fixups::Log("census: %u pages of 0x%08X..0x%08X protected, log '%s', re-arm %u ms, own code 0x%08X..0x%08X", g_C.NumPages, g_C.Lo, g_C.Hi, path, rearmMs, g_C.TextLo, g_C.TextHi);
}
} // namespace

void ArmDiagnostics() {
    if (!IsLoaded()) {
        return;
    }
    if (const char* f = std::getenv("NOTSA_POISON_CONVERTED"); f && *f) {
        PoisonConverted(f);
    }
    if (const char* c = std::getenv("NOTSA_IMAGE_CENSUS"); c && *c && *c != '0') {
        StartCensus();
    }
}
} // namespace notsa::standalone::DataImage
#endif
