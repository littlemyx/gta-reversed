#include "StdInc.h"

#ifdef NOTSA_STANDALONE_RUN
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <crtdbg.h>
#include <float.h>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <cstring>
#include <unordered_set>
#include <vector>
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
struct VtableClass {
    uint32_t           ExeVtbl;
    size_t             N;
    void* const*       Ours;
    std::string        Name;
};
std::vector<VtableClass>& VtableClasses() {
    static std::vector<VtableClass> v;
    return v;
}
struct UnverifiedHook {
    uint32_t    ExeAddr;
    uint32_t    VtblSlot;
    int         State;
    bool        Reversed;
    std::string Name;
};
std::vector<UnverifiedHook>& Unverified() {
    static std::vector<UnverifiedHook> v;
    return v;
}
std::unordered_set<uint32_t>& NoCopySlots() { // exe vtable slots of unverified virtual hooks: the whole-vtable copy leaves them alone
    static std::unordered_set<uint32_t> s;
    return s;
}
FixupStats g_Stats{};
char       g_LastHook[160]; // last hook name handed to us: a CRT assertion during hook installation (e.g. duplicate item in a category) names it

void NoteHook(uint32_t exeAddr, const char* name) {
    wsprintfA(g_LastHook, "0x%08X %.100s", exeAddr, name ? name : "?");
}

void __cdecl AbortHandler(int) {
    // Standalone S5: make the symbolising unhandled-exception filter (app_debug.cpp) print the call stack of the failing assert()/terminate() into logs/log.log
    if (!std::getenv("NOTSA_STANDALONE_NO_ABORT_TRACE")) {
        static bool s_Raised = false;
        if (!s_Raised) {
            s_Raised = true;
            Fixups::Log("abort(): raising exception 0xE0AB0001 for a stack trace (last hook %s)", g_LastHook);
            RaiseException(0xE0AB0001u, EXCEPTION_NONCONTINUABLE, 0, nullptr);
        }
    }
    Fixups::Fatal("abort() called (failed assert()/terminate). Last hook registered: %s", g_LastHook);
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
        Fixups::Fatal("CRT assertion/error: %.700s (last hook registered: %s)", message ? message : "?", g_LastHook);
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
// SEH-guarded read: a trap taken on a stack overflow path must not double fault silently
bool SafeReadU32(uint32_t addr, uint32_t& out) {
    __try {
        out = *(volatile uint32_t*)addr;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

LONG CALLBACK RedirectVEH(EXCEPTION_POINTERS* ep) {
    const auto* r = ep->ExceptionRecord;
    if (r->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || !DataImage::IsLoaded()) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const auto  pc = (uint32_t)ep->ContextRecord->Eip;
    const auto& info = DataImage::GetInfo();
    const auto  kind = (uint32_t)r->ExceptionInformation[0]; // 0 read, 1 write, 8 execute (DEP)
    const auto  addr = (uint32_t)r->ExceptionInformation[1];
    if (pc < info.CodeLo || pc >= info.CodeHi || addr != pc) { // not an instruction fetch from the original code range
        if (kind != 8 && addr >= info.CodeLo && addr < info.DataBase) {
            Fixups::Fatal("TRAP: data %s access to the original code range at 0x%08X from 0x%08X (exe +0x%X). The original code bytes are not part of the standalone image.",
                kind ? "write" : "read", addr, pc, pc - (uint32_t)GetModuleHandleA(nullptr));
        }
        if (kind == 1 && info.RdataLo && addr >= info.RdataLo && addr < info.RdataHi) {
            Fixups::Fatal("TRAP: write into the read-only original .rdata at 0x%08X from 0x%08X (exe +0x%X): a vtable/constant/string table is being modified.",
                addr, pc, pc - (uint32_t)GetModuleHandleA(nullptr));
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }
    uint32_t retAddr = 0;
    if (!SafeReadU32(ep->ContextRecord->Esp, retAddr)) {
        retAddr = 0xDEADDEAD; // unreadable stack
    }
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

void LogFpuState(const char* where) {
    const unsigned cw = _controlfp(0, 0);
    const unsigned sw = _statusfp();
    Log("FPU[%s]: PC=%s (cw=0x%05x) RC=0x%x EM=0x%02x sw=0x%x", where,
        (cw & _MCW_PC) == _PC_24 ? "24" : (cw & _MCW_PC) == _PC_53 ? "53" : "64",
        cw, cw & _MCW_RC, cw & _MCW_EM, sw);
}

// A CPool::Delete of a pointer that is not inside the pool's storage (the exe would flip a random byte and set first-free to a wild index; diagnosing: who deletes it)
void PoolBadDelete(const void* pool, const void* obj, const void* storage, unsigned capacity, unsigned objSize, const void* caller) {
    Log("POOLBAD: Delete(0x%08X) outside pool %08X [storage %08X, %u x 0x%X]; caller 0x%08X (exe +0x%X)", (unsigned)(uintptr_t)obj, (unsigned)(uintptr_t)pool,
        (unsigned)(uintptr_t)storage, capacity, objSize, (unsigned)(uintptr_t)caller, (unsigned)((uintptr_t)caller - (uintptr_t)GetModuleHandleA(nullptr)));
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
    NoteHook(exeAddr, name);
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
    NoteHook(exeFn, name);
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

void RegisterUnverified(uint32_t exeAddr, const char* name, int state, bool reversed, uint32_t vtblSlot) {
    if (!exeAddr) {
        return;
    }
    NoteHook(exeAddr, name);
    Unverified().push_back({ exeAddr, vtblSlot, state, reversed, name });
    if (vtblSlot) {
        NoCopySlots().insert(vtblSlot);
    }
    g_Stats.UnverifiedHooks++;
}

void RegisterVMTClass(uint32_t exeVtbl, size_t n, void* const* ourVtbl, const char* cls) {
    if (!ourVtbl) {
        g_Stats.VtableClassesNoExport++;
        Log("vtable copy: class %s has no exported vtable (add NOTSA_EXPORT_VTABLE), its virtuals stay trapped", cls);
        return;
    }
    VtableClasses().push_back({ exeVtbl, n, ourVtbl, cls });
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

    // The classification (which dwords are code pointers, V = run / C = isolated) is done ONCE, by tools/standalone/extract_exe_data.py
    // (function-start set + alignment + text filter, see its docstring) and shipped as data_pointers.bin: {u32 addr, u32 class(1=V,2=C)}.
    std::vector<std::pair<uint32_t, uint8_t>> ptrs;
    {
        char listPath[MAX_PATH];
        GetModuleFileNameA(nullptr, listPath, MAX_PATH);
        if (char* slash = strrchr(listPath, '\\')) {
            strcpy_s(slash + 1, listPath + MAX_PATH - slash - 1, "data_pointers.bin");
        }
        FILE* lf = nullptr;
        if (fopen_s(&lf, listPath, "rb") || !lf) {
            Fatal("ApplyToDataImage: cannot open data_pointers.bin next to the exe (re-run the extractor / build)");
        }
        uint32_t rec[2];
        while (fread(rec, sizeof(rec), 1, lf) == 1) {
            if (rec[0] < info.DataBase || rec[0] + 4 > info.DataBase + info.InitializedSize || (rec[0] & 3) || rec[1] < 1 || rec[1] > 2) {
                Fatal("ApplyToDataImage: data_pointers.bin record 0x%08X/%u out of range", rec[0], rec[1]);
            }
            ptrs.emplace_back(rec[0], (uint8_t)rec[1]);
        }
        fclose(lf);
    }
    g_Stats.TextLikeIgnored = info.SkippedTextLike;
    g_Stats.UnalignedIgnored = info.SkippedUnaligned;
    const std::vector<uint32_t> original(words, words + n); // for the self-check

    // Whole-vtable copy: for every class with an exported vtable all N slots of OUR vtable replace the exe vtable (our slot layout mirrors the exe's)
    std::vector<bool> covered(n, false);
    {
        std::unordered_set<uint32_t> listed;
        for (const auto& [a, k] : ptrs) {
            listed.insert(a);
        }
        for (const auto& c : VtableClasses()) {
            if (c.ExeVtbl < info.DataBase || c.ExeVtbl + c.N * 4 > info.DataBase + info.InitializedSize) {
                Log("vtable copy: %s vtable 0x%08X+%u slots outside the data image, skipped", c.Name.c_str(), c.ExeVtbl, (unsigned)c.N);
                continue;
            }
            g_Stats.VtableClasses++;
            for (size_t k = 0; k < c.N; k++) {
                const uint32_t slotAddr = c.ExeVtbl + (uint32_t)(k * 4);
                void* const ours = c.Ours[k];
                const size_t i = (slotAddr - info.DataBase) / 4;
                if (!ours || covered[i] || NoCopySlots().contains(slotAddr)) {
                    continue;
                }
                words[i] = (uint32_t)ours;
                covered[i] = true;
                g_Stats.FixedByVtableCopy++;
                g_Stats.VtableCopyOverlap += listed.contains(slotAddr);
            }
        }
    }
    // Pass 2 replace
    char dir[MAX_PATH];
    GetModuleFileNameA(nullptr, dir, MAX_PATH);
    if (char* slash = strrchr(dir, '\\')) {
        strcpy_s(slash + 1, dir + MAX_PATH - slash - 1, "standalone_unknown_pointers.txt");
    }
    size_t skippedNotExeCode = 0;
    FILE* unk = nullptr;
    fopen_s(&unk, dir, "w");
    for (const auto& [slotAddr, klass] : ptrs) {
        const size_t i = (slotAddr - info.DataBase) / 4;
        if (covered[i]) {
            continue; // already written by the whole-vtable copy
        }
        const uint32_t orig = words[i];
        if (orig < info.CodeLo || orig >= info.CodeHi) { // already rewritten by C++ (e.g. CCheat::m_aCheatFunctions filled by Cheat.cpp InjectHooks): not an exe code pointer any more
            skippedNotExeCode++;
            continue;
        }
        (klass == 1 ? g_Stats.CodePointersV : g_Stats.CodePointersC)++;
        if (const auto it = SlotMap().find(slotAddr); it != SlotMap().end()) {
            words[i] = (uint32_t)it->second.Ours;
            g_Stats.FixedBySlot++;
        } else if (void* ours = FindKnown(orig)) {
            words[i] = (uint32_t)ours;
            g_Stats.FixedByFunction++;
        } else {
            words[i] = (uint32_t)GetTrapStub(orig);
            (klass == 1 ? g_Stats.TrappedV : g_Stats.TrappedC)++;
            if (unk) {
                fprintf(unk, "0x%08X 0x%08X %c\n", slotAddr, orig, klass == 1 ? 'V' : 'C');
            }
        }
    }
    if (unk) {
        fclose(unk);
    }
    for (size_t i = 0; i < n; i++) {
        g_Stats.ChangedDwords += words[i] != original[i];
    }
    {   // the porting TODO list: hooks the authors disabled / never reversed. They are not ours and trap with the exe address.
        char up[MAX_PATH];
        GetModuleFileNameA(nullptr, up, MAX_PATH);
        if (char* slash = strrchr(up, '\\')) {
            strcpy_s(slash + 1, up + MAX_PATH - slash - 1, "standalone_unverified_hooks.txt");
        }
        FILE* uf = nullptr;
        fopen_s(&uf, up, "w");
        for (const auto& h : Unverified()) {
            const char* why = !h.Reversed ? "Reversed=false" : h.State == 1 ? "RedirectToGTA" : "Unhooked";
            Log("unverified hook (not ours, traps): 0x%08X %s [%s]%s", h.ExeAddr, h.Name.c_str(), why, h.VtblSlot ? " (virtual)" : "");
            if (uf) {
                fprintf(uf, "0x%08X %s %s%s\n", h.ExeAddr, h.Name.c_str(), why, h.VtblSlot ? " virtual" : "");
            }
        }
        if (uf) {
            fclose(uf);
        }
    }
    const auto& s = g_Stats;
    Log("fixups: unverified hooks excluded from the maps: %u (list in standalone_unverified_hooks.txt)", (unsigned)s.UnverifiedHooks);
    Log("fixups: registered %u functions + %u vtable slots (%u conflicts). Data image code pointers: V=%u C=%u (skipped, NOT rewritten: text-like %u, unaligned/u16-pair %u). "
        "Fixed: by slot %u, by function %u; trapped (unknown): V=%u C=%u (V/C exclude the %u listed slots covered by the vtable copy)",
        (unsigned)s.RegisteredFunctions, (unsigned)s.RegisteredVMTSlots, (unsigned)s.Conflicts, (unsigned)s.CodePointersV, (unsigned)s.CodePointersC,
        (unsigned)s.TextLikeIgnored, (unsigned)s.UnalignedIgnored, (unsigned)s.FixedBySlot, (unsigned)s.FixedByFunction, (unsigned)s.TrappedV, (unsigned)s.TrappedC, (unsigned)s.VtableCopyOverlap);
    Log("fixups: skipped %u listed dwords whose current value is no longer an exe code pointer (already rewritten by C++ code)", (unsigned)skippedNotExeCode);
    Log("fixups vtable copy: %u classes copied whole (%u slots written, %u of them listed pointers), %u classes without exported vtable",
        (unsigned)s.VtableClasses, (unsigned)s.FixedByVtableCopy, (unsigned)s.VtableCopyOverlap, (unsigned)s.VtableClassesNoExport);
    if (info.DataBase <= 0x860E2C && info.DataBase + info.InitializedSize > 0x8A2A18) { // regression probes of the S1 classifier (1.0 US compact)
        Log("fixups probes: 0x860E2C=0x%08X (data, must stay 0x004F0000) 0x8A2A18=0x%08X (data, must stay 0x004D0000) 0x85DA64=0x%08X (pointer to 0x425F70, must be rewritten)",
            *(uint32_t*)0x860E2C, *(uint32_t*)0x8A2A18, *(uint32_t*)0x85DA64);
    }
    const size_t expectedChanged = s.FixedBySlot + s.FixedByFunction + s.FixedByVtableCopy + s.TrappedV + s.TrappedC;
    Log("fixups self-check: changed dwords %u, fixed+trapped %u: %s", (unsigned)s.ChangedDwords, (unsigned)expectedChanged, s.ChangedDwords == expectedChanged ? "OK" : "MISMATCH");
    if (s.ChangedDwords != expectedChanged) {
        Fatal("ApplyToDataImage self-check failed: %u dwords changed but %u were fixed/trapped (something else wrote into the image)", (unsigned)s.ChangedDwords, (unsigned)expectedChanged);
    }
    // .rdata (vtables, constants, string tables) is read-only in the original exe: everything above was the last legitimate write.
    // A write from a port now raises an AV that RedirectVEH reports with the address (see "write into the read-only original .rdata").
    if (info.RdataLo && info.RdataHi > info.RdataLo) {
        DWORD old;
        if (VirtualProtect((void*)info.RdataLo, info.RdataHi - info.RdataLo, PAGE_READONLY, &old)) {
            Log("fixups: .rdata 0x%08X..0x%08X is now PAGE_READONLY", info.RdataLo, info.RdataHi);
        } else {
            Log("fixups: WARNING VirtualProtect(READONLY) on .rdata failed (error %u)", GetLastError());
        }
    }
    return s;
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

// S5 diagnostics: NOTSA_STANDALONE_SAMPLER=1 samples the main thread's EIP / caller chain (EBP walk, 12 frames) every ~10 ms and logs the hottest
// return addresses every 15 s (resolve against the .map: "profile" lines, address = absolute VA)
#include <map>
#include <thread>
static void SamplerThread(HANDLE mainThread) {
    std::map<uint32_t, uint32_t> hist;       // innermost 'interesting' frame (first address in the exe image)
    std::map<uint32_t, uint32_t> inclusive;  // every frame of the chain (inclusive time)
    int samples = 0, sysSamples = 0;
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
            if (frames[0] < 0x401000 || frames[0] >= 0x3400000) sysSamples++; // EIP in a system DLL (wined3d / ntdll / ...)
            for (int i = 0; i < n; i++) inclusive[frames[i]]++;
            samples++;
        }
        if (GetTickCount64() - last > 15000 && samples) {
            last = GetTickCount64();
            std::multimap<uint32_t, uint32_t, std::greater<>> top;
            for (auto& [a, k] : inclusive) { if (a >= 0x401000 && a < 0x3400000) top.insert({ k, a }); }
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

void InstallRedirectHandler() {
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
    AddVectoredExceptionHandler(1, RedirectVEH);
}
} // namespace Fixups
} // namespace notsa::standalone
#endif
