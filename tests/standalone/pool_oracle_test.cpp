// CPool fidelity oracle: the exe's CPool<CPlayerPed> instance (object size 0x7C4: New 0x5E45E0, Delete 0x5E4650, IsObjectValid 0x5E4690) runs as machine code on a
// fake pool struct laid out like the exe's CPool; the port's CPool<T> from Core/Pool.h (NOTSA_STANDALONE_RUN) runs the same random op sequences.
// Byte map, first-free index, every returned pointer (as slot index) and every validity answer must be identical.
// usage: pool_oracle_test.exe [-v] [-seqs N] [-ops N]       (exit code 0 = all identical)
#include "game_oracle.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <bit>
#include <utility>
#include <span>
#include <memory>
#include <type_traits>

#include "Core/Pool.h"

struct Obj { uint8_t b[0x7C4]; };
struct ExePool {              // the exe's CPool layout
    void*    objects;         // 0x00
    uint8_t* flags;           // 0x04
    int32_t  size;            // 0x08
    int32_t  firstFree;       // 0x0C
    uint8_t  owns, locked;    // 0x10, 0x11
};
using NewFn   = void*   (__fastcall*)(ExePool*, int);
using DelFn   = void    (__stdcall*)(void*);       // thiscall with ecx = pool: use a thunk below
using ValidFn = uint8_t (__stdcall*)(const void*);

static void* ExeNew(ExePool* p) { return reinterpret_cast<NewFn>(0x5E45E0)(p, 0); }
static void __declspec(naked) ExeDeleteThunk(ExePool*, void*) { __asm { mov ecx, [esp+4]
                                                                       push dword ptr [esp+8]
                                                                       mov eax, 0x5E4650
                                                                       call eax
                                                                       ret } }
static void __declspec(naked) ExeValidThunk(ExePool*, const void*) { __asm { mov ecx, [esp+4]
                                                                         push dword ptr [esp+8]
                                                                         mov eax, 0x5E4690
                                                                         call eax
                                                                         ret } }
static void ExeDelete(ExePool* p, void* o) { ((void (__cdecl*)(ExePool*, void*))ExeDeleteThunk)(p, o); }
static bool ExeValid(ExePool* p, const void* o) { return ((uint8_t (__cdecl*)(ExePool*, const void*))ExeValidThunk)(p, o) != 0; }

struct Rng {
    uint64_t s; explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 7) { for (int i = 0; i < 4; ++i) u32(); }
    uint32_t u32() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 16); }
    int below(int n) { return (int)(u32() % (uint32_t)n); }
};

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    int seqs = 200, ops = 10000; bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) verbose = true;
        else if (!std::strcmp(argv[i], "-seqs") && i + 1 < argc) seqs = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "-ops") && i + 1 < argc) ops = std::atoi(argv[++i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("pool_oracle_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }

    long bad = 0, checks = 0, nNew = 0, nFull = 0, nDel = 0, nDouble = 0, nWrap127 = 0;
    for (int sq = 0; sq < seqs; ++sq) {
        Rng r(sq + 1);
        static const int caps[] = { 1, 2, 3, 5, 8, 16, 64, 130 };
        const int cap = caps[sq % 8];
        std::vector<Obj> exeMem(cap), portMem(cap);
        std::vector<uint8_t> exeFlags(cap, 0x80), portFlags(cap);
        ExePool ep{ exeMem.data(), exeFlags.data(), cap, -1, 0, 0 };
        using PortPool = CPool<Obj>;
        alignas(PortPool) unsigned char poolBuf[sizeof(PortPool)];
        auto* pp = new (poolBuf) PortPool(cap, portMem.data(), portFlags.data());
        // the port ctor starts with firstFree -1 and an empty map exactly like the exe's ctor 0x550A30
        std::vector<int> live;                      // slot indices believed allocated (either side agrees if no divergence)
        auto compare = [&](const char* what, int op) {
            ++checks;
            const int32_t pFirst = *reinterpret_cast<int32_t*>(poolBuf + 12);
            if (std::memcmp(exeFlags.data(), portFlags.data(), cap) != 0 || ep.firstFree != pFirst) {
                if (bad < 20 || verbose) std::printf("  MISMATCH seq %d op %d (%s) cap %d: firstFree exe %d port %d, map equal %d\n", sq, op, what, cap, ep.firstFree, pFirst,
                                                      std::memcmp(exeFlags.data(), portFlags.data(), cap) == 0);
                ++bad; return false;
            }
            return true;
        };
        if (!compare("init", -1)) continue;
        for (int op = 0; op < ops; ++op) {
            const int k = r.below(100);
            if (k < 45) {                            // New
                void* e = ExeNew(&ep); Obj* p = pp->New(); ++nNew;
                const long ei = e ? ((Obj*)e - exeMem.data()) : -1, pi = p ? (p - portMem.data()) : -1;
                if (ei == -1) ++nFull;
                if (ei != pi) { ++bad; if (bad < 20 || verbose) std::printf("  MISMATCH seq %d op %d New: exe slot %ld port slot %ld\n", sq, op, ei, pi); break; }
                if (ei >= 0 && (exeFlags[ei] & 0x7F) == 0) ++nWrap127;
                if (ei >= 0) live.push_back((int)ei);
                if (!compare("New", op)) break;
            } else if (k < 80) {                     // Delete (live slot, or a free slot / double delete)
                int slot;
                if (!live.empty() && k < 70) { const int li = r.below((int)live.size()); slot = live[li]; live.erase(live.begin() + li); ++nDel; }
                else { slot = r.below(cap); ++nDouble; if (std::find(live.begin(), live.end(), slot) != live.end()) { live.erase(std::find(live.begin(), live.end(), slot)); } }
                ExeDelete(&ep, &exeMem[slot]); pp->Delete(&portMem[slot]);
                if (!compare("Delete", op)) break;
            } else if (k < 90) {                     // IsObjectValid on a slot pointer, a pointer one object below the base, and a pointer past the end
                const int variant = r.below(4); const int slot = r.below(cap);
                const uint8_t* eb = (const uint8_t*)exeMem.data(); const uint8_t* pb = (const uint8_t*)portMem.data();
                long off = variant == 0 ? (long)slot * 0x7C4 : variant == 1 ? (long)slot * 0x7C4 + r.below(0x7C4) : variant == 2 ? -(long)r.below(0x7C4 * 2) : (long)cap * 0x7C4 + r.below(0x7C4 * 2);
                const bool e = ExeValid(&ep, eb + off), p = pp->IsObjectValid((const Obj*)(pb + off)); ++checks;
                if (e != p) { ++bad; if (bad < 20 || verbose) std::printf("  MISMATCH seq %d op %d IsObjectValid off %ld: exe %d port %d\n", sq, op, off, e, p); break; }
            } else if (k < 95) {                     // IsFreeSlotAtIndex / GetAt / GetRef
                const int slot = r.below(cap); ++checks;
                const bool eFree = (exeFlags[slot] & 0x80) != 0;
                const int32_t eRef = (slot << 8) | exeFlags[slot];
                if (pp->IsFreeSlotAtIndex(slot) != eFree || (pp->GetAt(slot) == nullptr) != eFree || (!eFree && pp->GetAt(slot) != &portMem[slot]) || pp->GetRef(&portMem[slot]) != eRef
                    || (pp->GetAtRef(eRef) != nullptr) != true) {
                    ++bad; if (bad < 20 || verbose) std::printf("  MISMATCH seq %d op %d GetAt/GetRef slot %d\n", sq, op, slot); break;
                }
            } else {                                 // force ref counter near 127 on a random slot to exercise the 7-bit wrap
                const int slot = r.below(cap);
                if (exeFlags[slot] & 0x80) { const uint8_t v = (uint8_t)(0x80 | (125 + r.below(3))); exeFlags[slot] = v; portFlags[slot] = v; }
            }
        }
        pp->~PortPool();
    }
    std::printf("pool_oracle_test: %d sequences x %d ops, %ld checks, New %ld (full %ld, ref wrap to 0: %ld), deletes %ld live + %ld arbitrary: %ld mismatches\n",
                seqs, ops, checks, nNew, nFull, nWrap127, nDel, nDouble, bad);
    std::printf("%s\n", bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
