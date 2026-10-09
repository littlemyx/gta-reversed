#include "StdInc.h"

#ifdef NOTSA_STANDALONE_RUN
#include "DataImage.h"
#include "Fixups.h"

namespace notsa::standalone::detail {
bool g_DataImageLoaded = false;
}

extern "C" const unsigned char notsa_orig_pad[]; // tools/standalone/make_orig_pad.py (plain `.text`, first object)

namespace notsa::standalone::DataImage {
namespace {
Info g_Info{};
bool g_ImageMode{};


constexpr uint32_t ALLOC_GRANULARITY = 0x10000;
constexpr uint32_t AlignDown(uint32_t v, uint32_t a) { return v & ~(a - 1); }
constexpr uint32_t AlignUp(uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); }

// NOTE: everything on the early-load path (up to `EarlyLoad`) deliberately avoids the C/C++ runtime (no malloc, no stdio, no strtoul,
// no std::string): the `.CRT$XIB` initializer may run before parts of the CRT (locks, locale, heap) are set up. Win32 + user32's wsprintf only.

// Directory of the running exe with trailing backslash
void GetExeDir(char (&out)[MAX_PATH]) {
    DWORD n = GetModuleFileNameA(nullptr, out, MAX_PATH);
    while (n > 0 && out[n - 1] != '\\') {
        n--;
    }
    out[n] = 0;
}

void Join(char (&out)[MAX_PATH], const char* dir, const char* file) {
    wsprintfA(out, "%s%s", dir, file);
}

// Reads at most `cap` bytes, returns the number of bytes read or -1
int ReadSmallFile(const char* path, char* buf, DWORD cap) {
    const HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return -1;
    }
    DWORD got = 0;
    const BOOL ok = ReadFile(h, buf, cap, &got, nullptr);
    CloseHandle(h);
    return ok ? (int)got : -1;
}

// Tiny `"key": number` lookup (the json is written by tools/standalone/extract_exe_data.py, flat top-level numbers)
bool JsonUInt(const char* text, int len, const char* key, uint32_t& out) {
    const int klen = lstrlenA(key);
    for (int i = 0; i + klen + 3 < len; i++) {
        if (text[i] != '"' || text[i + klen + 1] != '"' || text[i + klen + 2] != ':') {
            continue;
        }
        int k = 0;
        while (k < klen && text[i + 1 + k] == key[k]) {
            k++;
        }
        if (k != klen) {
            continue;
        }
        int p = i + klen + 3;
        while (p < len && text[p] == ' ') {
            p++;
        }
        if (p >= len || text[p] < '0' || text[p] > '9') {
            return false;
        }
        uint32_t v = 0;
        for (; p < len && text[p] >= '0' && text[p] <= '9'; p++) {
            v = v * 10 + (uint32_t)(text[p] - '0');
        }
        out = v;
        return true;
    }
    return false;
}

[[noreturn]] void LoadFailed(const char* what, uint32_t addr = 0) {
    char msg[1024];
    wsprintfA(msg, "GTA:SA standalone: cannot map the original data image: %s (address 0x%08X).", what, addr);
    Fixups::Log("%s", msg);
    MessageBoxA(nullptr, msg, "gta_reversed standalone", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

void DescribeOccupant(uint32_t at, char (&out)[256]) {
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery((void*)at, &mbi, sizeof(mbi))) {
        wsprintfA(out, "occupied: AllocationBase=%08X RegionSize=0x%X state=0x%X type=0x%X", (unsigned)mbi.AllocationBase, (unsigned)mbi.RegionSize, mbi.State, mbi.Type);
    } else {
        out[0] = 0;
    }
}

void LoadImpl() {
    char dir[MAX_PATH], path[MAX_PATH];
    GetExeDir(dir);

    static char json[16384]; // BSS: the exe's own, mapped by the loader
    Join(path, dir, "original_data.json");
    const int jlen = ReadSmallFile(path, json, sizeof(json));
    if (jlen < 0) {
        LoadFailed("original_data.json not found next to the exe (build with -DGTASA_ORIGINAL_EXE=<gta_sa.exe>)");
    }
    Info i{};
    uint32_t committed{};
    if (!JsonUInt(json, jlen, "image_base", i.ImageBase) || !JsonUInt(json, jlen, "code_lo", i.CodeLo) || !JsonUInt(json, jlen, "code_hi", i.CodeHi) ||
        !JsonUInt(json, jlen, "data_base", i.DataBase) || !JsonUInt(json, jlen, "data_end", i.DataEnd) || !JsonUInt(json, jlen, "bin_size", i.InitializedSize) ||
        !JsonUInt(json, jlen, "committed_size", committed) || committed != i.DataEnd - i.DataBase || i.InitializedSize > committed) {
        LoadFailed("original_data.json is malformed");
    }
    // Optional (older json): .rdata range for the read-only protection, extractor statistics for the log
    JsonUInt(json, jlen, "rdata_lo", i.RdataLo);
    JsonUInt(json, jlen, "rdata_hi", i.RdataHi);
    JsonUInt(json, jlen, "skipped_text_like", i.SkippedTextLike);
    JsonUInt(json, jlen, "skipped_unaligned", i.SkippedUnaligned);

    const uint32_t dataGranule = AlignDown(i.DataBase, ALLOC_GRANULARITY);
    const uint32_t reserveEnd = AlignUp(i.DataEnd, ALLOC_GRANULARITY);
    const uint32_t padLo = (uint32_t)notsa_orig_pad, padHi = padLo + ORIG_PAD_SIZE;
    if (padLo <= i.DataBase && padHi >= i.DataEnd) {
        // A. The main image (linked at the original base, with the placeholder from tools/standalone/make_orig_pad.py merged into .text) already owns the range:
        //    just make it writable. The placeholder pages in the original CODE range become NOACCESS so a raw jump/call to an
        //    original address faults with the exact address (see Fixups::InstallRedirectHandler).
        // S2: the pad must be the FIRST code of .text, otherwise [0x401000, padLo) holds OUR code which would run silently when called by an original address.
        if (padLo != i.CodeLo) {
            char what[400];
            wsprintfA(what, "the placeholder starts at 0x%08X, not at the original code start 0x%08X: code of this exe lies in the original code range "
                "[0x%08X, 0x%08X) and raw calls to original addresses would run it silently. The pad (tools/standalone/make_orig_pad.py) must be the first object on the link line; "
                "link with /INCREMENTAL:NO.", padLo, i.CodeLo, i.CodeLo, padLo);
            LoadFailed(what, padLo);
        }
        DWORD old;
        if (!VirtualProtect((void*)i.DataBase, committed, PAGE_READWRITE, &old)) {
            LoadFailed("VirtualProtect(RW) on the placeholder failed", i.DataBase);
        }
        if (!VirtualProtect((void*)padLo, i.DataBase - padLo, PAGE_NOACCESS, &old)) {
            LoadFailed("VirtualProtect(NOACCESS) on the original code range failed", padLo);
        }
        g_ImageMode = true;
    } else {
        // B. Fallback: reserve+commit at the original address now. NOT reliable (see P2A_DESIGN.md section 4): fails loudly if taken.
        if (i.ImageBase < dataGranule && !VirtualAlloc((void*)i.ImageBase, dataGranule - i.ImageBase, MEM_RESERVE, PAGE_NOACCESS)) {
            char occ[256]; DescribeOccupant(i.ImageBase, occ);
            Fixups::Log("warning: could not reserve the original code range 0x%08X..0x%08X (%s)", i.ImageBase, dataGranule, occ);
        }
        if (!VirtualAlloc((void*)dataGranule, reserveEnd - dataGranule, MEM_RESERVE, PAGE_NOACCESS)) {
            char occ[256]; DescribeOccupant(dataGranule, occ);
            char what[600];
            wsprintfA(what, "the range 0x%08X..0x%08X is not free (%s) and the exe has no placeholder (linked without orig_image_pad?).", dataGranule, reserveEnd, occ);
            LoadFailed(what, dataGranule);
        }
        if (VirtualAlloc((void*)i.DataBase, committed, MEM_COMMIT, PAGE_READWRITE) != (void*)i.DataBase) {
            LoadFailed("commit failed", i.DataBase);
        }
    }

    // 3. Initial data (everything past InitializedSize is BSS: fresh pages are zero)
    Join(path, dir, "original_data.bin");
    const HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        LoadFailed("original_data.bin not found next to the exe");
    }
    DWORD got = 0;
    const bool ok = GetFileSize(h, nullptr) == i.InitializedSize && ReadFile(h, (void*)i.DataBase, i.InitializedSize, &got, nullptr) && got == i.InitializedSize;
    CloseHandle(h);
    if (!ok) {
        LoadFailed("original_data.bin has the wrong size / is unreadable (re-run the extractor)");
    }

    g_Info = i;
    detail::g_DataImageLoaded = true;
    Fixups::Log("data image (%s) mapped: 0x%08X..0x%08X (%u KB read from original_data.bin, post-_initterm snapshot incl. BSS), code range 0x%08X..0x%08X",
        g_ImageMode ? "in-image placeholder" : "VirtualAlloc", i.DataBase, i.DataEnd, i.InitializedSize / 1024, i.CodeLo, i.CodeHi);
}

// Guarantee of order: C initializers (`.CRT$XI*`, run by the CRT startup via _initterm_e) execute strictly before every C++ dynamic
// initializer (`.CRT$XC*`), i.e. before the first `StaticRef` in any static constructor. `XIB` sorts after the CRT's own `XIA` marker.
int __cdecl EarlyLoad() {
    DataImage::Load();
    return 0;
}
} // namespace

void Load() {
    if (!detail::g_DataImageLoaded) {
        LoadImpl();
    }
}

bool IsLoaded() { return detail::g_DataImageLoaded; }
const Info& GetInfo() { return g_Info; }
} // namespace notsa::standalone::DataImage

#pragma section(".CRT$XIB", read)
#pragma comment(linker, "/include:_notsa_standalone_data_image_early_init")
extern "C" __declspec(allocate(".CRT$XIB")) int(__cdecl* const notsa_standalone_data_image_early_init)() = notsa::standalone::DataImage::EarlyLoad;
#endif
