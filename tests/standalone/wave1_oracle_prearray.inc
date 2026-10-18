// PT-MISSION / U14: CTaskSimpleFight reads BELOW CTaskSimpleFight::m_aComboData (m_nComboSet < 4: 0x624A40 `mov bl,[ecx+0xC17154]`, 0x61D6F0 `fld [eax+0xC1710C]`, ecx = (set-4)*0x88).
// A detached build gives the array its own storage; notsa::fight_prearray::Read (TaskSimpleFightPreArray.h) maps the same byte offsets onto the objects that sit below it in the exe.
// In THIS test the port's globals sit at the exe addresses, so the exe's raw read is simply the memory at 0xC170D0 + (set-4)*0x88 + offset. The memory [0xC16D00, 0xC17800) is filled with
// random bytes (incl. the exe-only variables in the gaps), the exe's own start-up thunk 0x851F10 stores the last float below the array, and then, for EVERY int8 comboSet:
//   * the two production forms (flags byte at +0x84, float at +0x3C) and every other (offset 0..0x87, size 1/2/4) are read with Read() over segments built from the real objects' addresses
//     and compared with the raw memory read;
//   * Read() must succeed exactly when the bytes lie completely inside the array or one mapped object (independent interval table below), and the production forms must be mapped for
//     every set in [-2, 16] (the sets the game can assign: -1 ctor, 0, 1, 4..15 styles, 16 pistol whip) except (3, float) = the heap pointer of CPlayerRelationshipRecorder.
#include "game_sa/Tasks/TaskTypes/TaskSimpleFightPreArray.h"

static void TestFightPreArray() {
    constexpr uintptr_t kArr = 0xC170D0, kLo = 0xC16D00, kHi = 0xC17800;
    struct Obj { uintptr_t va; size_t n; };
    static const Obj kObjs[] = { { 0xC16DB8, 256 }, { 0xC16EEC, 4 }, { 0xC16F18, 260 }, { 0xC17040, 60 }, { 0xC170CC, 4 } };   // ms_SaveFileName, ms_WorkBufferPos, DefaultPCSaveFileName, RsGlobal, last float
    constexpr size_t kArrSize = 13 * 0x88;
    auto mapped = [&](uintptr_t a, size_t n) {
        if (a >= kArr && a + n <= kArr + kArrSize) return true;
        for (const auto& o : kObjs) if (a >= o.va && a + n <= o.va + o.n) return true;
        return false;
    };
    Run("CTaskSimpleFight pre-array reads (all int8 comboSets) 0x624A40/0x61D6F0", [&](Rng& r, std::string& d) {
        FillGarbage(r, kLo, kHi - kLo);
        oracle::Fn<void __cdecl()>(0x851F10)();   // the exe's start-up code for the last float
        using notsa::fight_prearray::Segment;
        const Segment below[]{
            { 792, (const uint8*)0xC16DB8, 256 }, { 484, (const uint8*)0xC16EEC, 4 }, { 440, (const uint8*)0xC16F18, 260 }, { 144, (const uint8*)0xC17040, 60 },
            { 4, (const uint8*)&notsa::fight_prearray::LastFloatBelowBits, 4 },
        };
        char t[160];
        uint32_t last; std::memcpy(&last, (const void*)0xC170CC, 4);
        if (last != notsa::fight_prearray::LastFloatBelowBits) { std::snprintf(t, sizeof t, "exe thunk 0x851F10 stored %08X, LastFloatBelowBits = %08X", last, notsa::fight_prearray::LastFloatBelowBits); d = t; return false; }
        int checked = 0, mappedN = 0;
        for (int set = -128; set <= 127; ++set) {
            for (uint32_t off = 0; off < 0x88; ++off) {
                for (size_t size : { 1u, 2u, 4u }) {
                    const int32_t rel = (set - 4) * 0x88 + (int32_t)off;
                    const uintptr_t a = kArr + rel;
                    if (a < 0x1000 || a + size > 0x7FFFFFFF) continue;
                    uint32_t got = 0xDEADBEEF;
                    const bool ok = notsa::fight_prearray::Read(below, (const uint8*)kArr, kArrSize, rel, size, got);
                    const bool want = mapped(a, size);
                    ++checked;
                    if (ok != want) { std::snprintf(t, sizeof t, "set %d off 0x%X size %zu: Read %s but the exe address %08zX is %smapped", set, off, size, ok ? "succeeded" : "failed", (size_t)a, want ? "" : "un"); d = t; return false; }
                    if (!ok) continue;
                    ++mappedN;
                    uint32_t exe = 0; std::memcpy(&exe, (const void*)a, size);
                    if (got != exe) { std::snprintf(t, sizeof t, "set %d off 0x%X size %zu (exe address %08zX): got %08X exe %08X", set, off, size, (size_t)a, got, exe); d = t; return false; }
                }
            }
            if (set >= -2 && set <= 16) {   // the production forms must be mapped
                for (const auto& form : { std::pair<uint32_t, size_t>{ 0x84, 1 }, std::pair<uint32_t, size_t>{ 0x3C, 4 } }) {
                    if (set == 3 && form.second == 4) continue;   // 0xC17084 = heap pointer of CPlayerRelationshipRecorder (never assigned)
                    uint32_t got;
                    if (!notsa::fight_prearray::Read(below, (const uint8*)kArr, kArrSize, (set - 4) * 0x88 + (int32_t)form.first, form.second, got)) {
                        std::snprintf(t, sizeof t, "set %d: production form (off 0x%X size %zu) is not mapped", set, form.first, form.second); d = t; return false;
                    }
                }
            }
        }
        if (mappedN < 1000) { std::snprintf(t, sizeof t, "only %d of %d reads were mapped", mappedN, checked); d = t; return false; }
        return true;
    }, 1);
}

// PT-MISSION / U10: after a busted / wasted mission the script must continue at the OUTERMOST return address (exe 0x485A50 DoDeathArrestCheck: clamp the depth to 1, --depth, IP = stack[depth]).
// The port's ResetIP (header-inline, now shared with this test) read `m_IPStack[m_StackDepth--]` = the entry above the top and ended on stack[1].
#include "game_sa/Scripts/RunningScript.h"
static void TestScriptResetIP() {
    Run("CRunningScript::DoDeathArrestCheck IP stack reset 0x485A50", [&](Rng& r, std::string& d) {
        alignas(16) static uint8 obj[sizeof(CRunningScript)];
        FillGarbage(r, obj, sizeof obj);
        auto* s = reinterpret_cast<CRunningScript*>(obj);
        s->m_IsDeathArrestCheckEnabled = true;
        s->m_StackDepth                = (uint16)(1 + r.below(8));
        for (auto& ip : s->m_IPStack) ip = reinterpret_cast<uint8*>(0x10000u + r.below(0x100000) * 4);
        const uint32_t flagSlot = 4 * (1 + r.below(100));
        *reinterpret_cast<uint32_t*>(0xA476AC) = flagSlot;             // CTheScripts::OnAMissionFlag
        *reinterpret_cast<uint32_t*>(0xA49960 + flagSlot) = 1;         // ScriptSpace[OnAMissionFlag]: on a mission
        CWorld::PlayerInFocus = 0;
        CWorld::Players[0].m_nPlayerState = r.below(2) ? PLAYERSTATE_HAS_DIED : PLAYERSTATE_HAS_BEEN_ARRESTED;
        return Both(d, { Rg{ (uintptr_t)obj, sizeof obj }, Rg{ 0xA49960u + flagSlot, 4 } },
            [&] { s->ResetIP(); s->m_DoneDeathArrest = true; s->m_WakeTime = 0; *reinterpret_cast<uint32_t*>(0xA49960 + flagSlot) = 0; return 0ull; },
            [&] { oracle::Fn<void __fastcall(void*, int)>(0x485A50)(obj, 0); return 0ull; });
    }, 300);
}
