#include "StdInc.h"

#ifdef NOTSA_STANDALONE_RUN
#include <unordered_map>
#include <mutex>
#include <atomic>
#include "Fixups.h"
#include "DataImage.h"

namespace notsa::standalone {
namespace {
struct FnEntry {
    void*       Ours;
    std::string Name;
};

// Maps are written only during hook registration (single threaded, before ApplyToDataImage) and are read-only afterwards.
std::unordered_map<uint32_t, FnEntry>& FnMap() {
    static std::unordered_map<uint32_t, FnEntry> m = [] { decltype(m) r; r.reserve(16384); return r; }();
    return m;
}
std::unordered_map<uint32_t, FnEntry>& SlotMap() { // key = address of the vtable slot in the data image
    static std::unordered_map<uint32_t, FnEntry> m = [] { decltype(m) r; r.reserve(8192); return r; }();
    return m;
}
FixupStats g_Stats{};

// ---------------------------------------------------------------- trap stubs
// stub:  68 <exeAddr>        push exeAddr
//        E8 <rel32>          call TrapHandler           (never returns)
// At TrapHandler entry: [esp] = return into stub, [esp+4] = exeAddr, [esp+8] = return address of whoever called the stub.
constexpr size_t STUB_SIZE   = 16;
constexpr size_t CHUNK_BYTES = 64 * 1024;

extern "C" [[noreturn]] void __cdecl NotsaStandaloneTrapHandler(uint32_t exeAddr, uint32_t callerRet) {
    const auto base = (uint32_t)GetModuleHandleA(nullptr);
    Fixups::Fatal("TRAP: original function/pointer 0x%08X was used but has no reimplementation. Caller return address: 0x%08X (exe base 0x%08X => +0x%X, resolve with the .map file).",
        exeAddr, callerRet, base, callerRet - base);
}

std::mutex                              g_StubMutex;
std::unordered_map<uint32_t, void*>     g_Stubs;
uint8_t*                                g_Chunk{};
size_t                                  g_ChunkUsed{CHUNK_BYTES};

void* MakeStub(uint32_t exeAddr) {
    std::scoped_lock lk{ g_StubMutex };
    if (const auto it = g_Stubs.find(exeAddr); it != g_Stubs.end()) {
        return it->second;
    }
    if (g_ChunkUsed + STUB_SIZE > CHUNK_BYTES) {
        g_Chunk = (uint8_t*)VirtualAlloc(nullptr, CHUNK_BYTES, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (!g_Chunk) {
            Fixups::Fatal("cannot allocate the trap stub page");
        }
        g_ChunkUsed = 0;
    }
    uint8_t* const stub = g_Chunk + g_ChunkUsed;
    g_ChunkUsed += STUB_SIZE;
    stub[0] = 0x68;
    memcpy(stub + 1, &exeAddr, 4);
    stub[5] = 0xE8;
    const int32_t rel = (int32_t)((uint32_t)&NotsaStandaloneTrapHandler - ((uint32_t)stub + 10));
    memcpy(stub + 6, &rel, 4);
    memset(stub + 10, 0xCC, STUB_SIZE - 10);
    FlushInstructionCache(GetCurrentProcess(), stub, STUB_SIZE);
    g_Stubs.emplace(exeAddr, stub);
    return stub;
}

// ---------------------------------------------------------------- redirect handler
LONG CALLBACK RedirectVEH(EXCEPTION_POINTERS* ep) {
    const auto* r = ep->ExceptionRecord;
    if (r->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || !DataImage::IsLoaded()) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const auto  pc = (uint32_t)ep->ContextRecord->Eip;
    const auto& info = DataImage::GetInfo();
    if (pc < info.CodeLo || pc >= info.CodeHi || r->ExceptionInformation[1] != pc) { // not an instruction fetch from the original code range
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const uint32_t retAddr = *(uint32_t*)ep->ContextRecord->Esp;
    if (void* ours = Fixups::FindKnown(pc)) {
        static std::atomic<uint32_t> s_logged{};
        if (s_logged++ < 64) {
            Fixups::Log("redirect: raw call to original 0x%08X -> ours %p (caller 0x%08X)", pc, ours, retAddr);
        }
        ep->ContextRecord->Eip = (DWORD)ours;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    Fixups::Fatal("TRAP: raw jump/call into the original code at 0x%08X which has no reimplementation. Return address on stack: 0x%08X (exe +0x%X).",
        pc, retAddr, retAddr - (uint32_t)GetModuleHandleA(nullptr));
}

// Keep in sync with text_like() in tools/standalone/extract_exe_data.py: isolated dwords that are really ASCII/UTF-16 text
bool TextLike(uint32_t v) {
    const uint8_t b0 = v & 255, b1 = (v >> 8) & 255, b2 = (v >> 16) & 255, b3 = v >> 24;
    const auto pr = [](uint8_t c) { return c >= 32 && c < 127; };
    return (b3 == 0 && pr(b0) && pr(b1) && pr(b2)) || (b1 == 0 && b3 == 0 && pr(b0) && pr(b2));
}
} // namespace

namespace Fixups {
// CRT free (wvsprintfA: user32, 1024 byte limit, no %z/%f) because `Log` is also used by the pre-CRT-init data image loader
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

void RegisterFunction(uint32_t exeAddr, void* ours, const char* name) {
    if (!ours || !exeAddr) {
        return; // `0x0` placeholders / hooks without a body
    }
    auto [it, inserted] = FnMap().try_emplace(exeAddr, FnEntry{ ours, name });
    if (inserted) {
        g_Stats.RegisteredFunctions++;
    } else if (it->second.Ours != ours) {
        g_Stats.Conflicts++;
        Log("fixup conflict: 0x%08X already -> %s, ignoring %s", exeAddr, it->second.Name.c_str(), name);
    }
}

void RegisterVMTSlot(uint32_t vtblAddr, size_t slot, uint32_t exeFn, void* ours, const char* name) {
    if (!ours) {
        return;
    }
    const auto slotAddr = vtblAddr + (uint32_t)(slot * sizeof(uint32_t));
    if (SlotMap().try_emplace(slotAddr, FnEntry{ ours, name }).second) {
        g_Stats.RegisteredVMTSlots++;
    } else if (SlotMap()[slotAddr].Ours != ours) {
        g_Stats.Conflicts++;
        Log("fixup conflict: vtable slot 0x%08X already registered, ignoring %s", slotAddr, name);
    }
    // Fallback for the same (inherited) exe function sitting in vtables of derived classes that did not install it themselves
    if (exeFn && !FnMap().contains(exeFn)) {
        FnMap().emplace(exeFn, FnEntry{ ours, name });
        g_Stats.RegisteredFunctions++;
    }
}

void* FindKnown(uint32_t exeAddr) {
    const auto it = FnMap().find(exeAddr);
    return it != FnMap().end() ? it->second.Ours : nullptr;
}

void* GetTrapStub(uint32_t exeAddr) { return MakeStub(exeAddr); }

void* Resolve(uint32_t exeAddr) {
    if (void* p = FindKnown(exeAddr)) {
        return p;
    }
    return GetTrapStub(exeAddr);
}

FixupStats ApplyToDataImage() {
    if (!DataImage::IsLoaded()) {
        Fatal("ApplyToDataImage: data image not loaded");
    }
    const auto& info = DataImage::GetInfo();
    auto* const words = reinterpret_cast<uint32_t*>(info.DataBase);
    const size_t n = info.InitializedSize / 4;
    const auto inCode = [&](uint32_t v) { return v >= info.CodeLo && v < info.CodeHi; };

    // Pass 1 classify on the ORIGINAL values: 1 = V (part of a run >= 2), 2 = C (isolated), 3 = S (isolated text-like), 0 = not a code pointer
    std::vector<uint8_t> cls(n, 0);
    for (size_t i = 0; i < n; i++) {
        if (!inCode(words[i])) {
            continue;
        }
        const bool run = (i > 0 && inCode(words[i - 1])) || (i + 1 < n && inCode(words[i + 1]));
        cls[i] = run ? 1 : TextLike(words[i]) ? 3 : 2;
    }

    // Pass 2 replace
    char dir[MAX_PATH];
    GetModuleFileNameA(nullptr, dir, MAX_PATH);
    if (char* slash = strrchr(dir, '\\')) {
        strcpy_s(slash + 1, dir + MAX_PATH - slash - 1, "standalone_unknown_pointers.txt");
    }
    FILE* unk = nullptr;
    fopen_s(&unk, dir, "w");
    for (size_t i = 0; i < n; i++) {
        if (!cls[i]) {
            continue;
        }
        if (cls[i] == 3) {
            g_Stats.TextLikeIgnored++;
            continue;
        }
        (cls[i] == 1 ? g_Stats.CodePointersV : g_Stats.CodePointersC)++;
        const uint32_t slotAddr = info.DataBase + (uint32_t)i * 4, orig = words[i];
        if (const auto it = SlotMap().find(slotAddr); it != SlotMap().end()) {
            words[i] = (uint32_t)it->second.Ours;
            g_Stats.FixedBySlot++;
        } else if (void* ours = FindKnown(orig)) {
            words[i] = (uint32_t)ours;
            g_Stats.FixedByFunction++;
        } else {
            words[i] = (uint32_t)GetTrapStub(orig);
            (cls[i] == 1 ? g_Stats.TrappedV : g_Stats.TrappedC)++;
            if (unk) {
                fprintf(unk, "0x%08X 0x%08X %c\n", slotAddr, orig, cls[i] == 1 ? 'V' : 'C');
            }
        }
    }
    if (unk) {
        fclose(unk);
    }
    const auto& s = g_Stats;
    Log("fixups: registered %u functions + %u vtable slots (%u conflicts). Data image code pointers: V=%u C=%u (text-like ignored %u). "
        "Fixed: by slot %u, by function %u; trapped (unknown): V=%u C=%u",
        (unsigned)s.RegisteredFunctions, (unsigned)s.RegisteredVMTSlots, (unsigned)s.Conflicts, (unsigned)s.CodePointersV, (unsigned)s.CodePointersC,
        (unsigned)s.TextLikeIgnored, (unsigned)s.FixedBySlot, (unsigned)s.FixedByFunction, (unsigned)s.TrappedV, (unsigned)s.TrappedC);
    return s;
}

void InstallRedirectHandler() {
    AddVectoredExceptionHandler(1, RedirectVEH);
}
} // namespace Fixups
} // namespace notsa::standalone
#endif
