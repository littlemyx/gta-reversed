// ORACLE-W3-MISC (second pass, rules v4): exe-oracle rows for the pure / global-state functions of app/platform/platform.cpp (Rs* wrappers, key tables, input device attach / event dispatch, the "-vms"
// pre-init command line), app/platform/win/WinPs.cpp (trivial ps* functions) and game_sa/Pipelines/CustomBuilding/CustomBuildingRenderer.cpp (day-night balance, per-frame update). Included at the end of
// wave3_oracle_misc.inc (executable wave3_misc_oracle_test = the W2 harness: deterministic arena, the whole exe data image + a small fixture block compared after every case).
// Event handlers installed by the rows are host stubs shared by both sides: they record (event, param) and return a value from the fixture.
#include "app/platform/platform.h"
#include "Pipelines/CustomBuilding/CustomBuildingRenderer.h"
#include "Pipelines/CustomBuilding/CustomBuildingDNPipeline.h"
#include "Clock.h"

namespace w3m2 {
using namespace w2;
constexpr size_t kScSize = 0x200;
alignas(16) static uint8_t g_sc[kScSize];
constexpr size_t kStr = 0x000 /*0x40*/, kRec = 0x040 /*0x40*/;
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
template<class R, class... A> static R ExeC(uint32_t addr, A... a) { return oracle::Fn<R __cdecl(A...)>(addr)(a...); }
static uint32_t __cdecl HandlerStub(uint32_t event, void* param) {
    uint32_t& n = SAt<uint32_t>(kRec);
    if (n < 4) { uint32_t* p = &SAt<uint32_t>(kRec + 4 + 12 * n); p[0] = (uint32_t)(uintptr_t)&HandlerStub; p[1] = event; p[2] = (uint32_t)(uintptr_t)param; }
    ++n;
    return SAt<uint32_t>(kRec + 0x30);
}
static std::vector<std::pair<size_t, size_t>> g_unc;
template<class PF, class EF>
static bool PCase(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&)) {
    const uint32_t seed = r.u32() | 1;
    FillGarbage(r, g_sc, kScSize);
    SAt<uint32_t>(kRec) = 0;
    SAt<uint32_t>(kRec + 0x30) = r.u32();
    build(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    Reg img{ kImgLo, kImgHi - kImgLo };
    return BothA(d, seed, { sc, img }, std::forward<PF>(pf), std::forward<EF>(ef));
}
static void BuildPlain(Rng&) {}
// the exe takes the argument in eax and returns on a plain ret: load it and jump (the stack is left as the cdecl caller built it)
static __declspec(naked) uint32_t __cdecl ExePreInit(const char*) { __asm { mov eax, dword ptr [esp + 4] } __asm { mov ecx, 0x619530 } __asm { jmp ecx } }
} // namespace w3m2

static void TestW3MiscPlatform() {
    using namespace w2; using namespace w3m2;
    // RsKeyFromScanCode 0x6193F0: the two 256-byte tables at 0x8D2C00 / 0x8D2D00 (the exe reads the byte argument and tests the whole shift dword)
    MROW("RsKeyFromScanCode", 0x6193F0)
        const uint32_t scan = r.below(4) ? r.below(256) : (r.u32() & 0xFF);
        const uint32_t shift = r.below(3) == 0 ? 0u : r.below(2) ? 1u : r.u32();
        return PCase(r, d, [&]() -> uint64_t { return (uint8_t)RsKeyFromScanCode((uint8)scan, (int32)shift); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, uint32_t, uint32_t>(0x6193F0, scan, shift); }, &BuildPlain);
    MEND(1500)
    MROW("RsPathGetSeparator", 0x619430)
        return PCase(r, d, [&]() -> uint64_t { return (uint8_t)RsPathGetSeparator(); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t>(0x619430); }, &BuildPlain);
    MEND(20)
    MROW("RsRegisterImageLoader", 0x619470)
        return PCase(r, d, [&]() -> uint64_t { return (uint8_t)RsRegisterImageLoader(); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t>(0x619470); }, &BuildPlain);
    MEND(20)
    MROW("RsSetDebug", 0x619480)
        return PCase(r, d, [&]() -> uint64_t { RsSetDebug(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x619480); return 0; }, &BuildPlain);
    MEND(20)
    MROW("psPathGetSeparator", 0x745500)
        return PCase(r, d, [&]() -> uint64_t { return (uint8_t)psPathGetSeparator(); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t>(0x745500); }, &BuildPlain);
    MEND(20)
    MROW("psInstallFileSystem", 0x745520)
        return PCase(r, d, [&]() -> uint64_t { return (uint8_t)psInstallFileSystem(); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t>(0x745520); }, &BuildPlain);
    MEND(20)
    MROW("psGetMemoryFunctions", 0x745510)
        return PCase(r, d, [&]() -> uint64_t { return (uint64_t)(uintptr_t)psGetMemoryFunctions(); }, [&]() -> uint64_t { return (uint64_t)(uintptr_t)ExeC<void*>(0x745510); }, &BuildPlain);
    MEND(20)
#ifndef NOTSA_USE_SDL3
    MROW("psTerminate", 0x7458A0)
        return PCase(r, d, [&]() -> uint64_t { psTerminate(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x7458A0); return 0; }, &BuildPlain);
    MEND(20)
    MROW("RsTerminate", 0x6195F0)
        return PCase(r, d, [&]() -> uint64_t { RsTerminate(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x6195F0); return 0; }, &BuildPlain);
    MEND(20)
#endif
    // RsInputDeviceAttach 0x6194C0: type 0 keyboard, 1 mouse, 2 pad (anything else: 0, nothing stored); the handler is stored as is, `used` = 1
    MROW("RsInputDeviceAttach", 0x6194C0)
        const uint32_t type = r.below(5) ? r.below(4) : r.u32();
        const uint32_t h = r.below(3) ? (uint32_t)(uintptr_t)&HandlerStub : r.u32();
        return PCase(r, d, [&]() -> uint64_t { return (uint32_t)RsInputDeviceAttach((RsInputDeviceType)type, (RsInputEventHandler)(uintptr_t)h); },
                           [&]() -> uint64_t { return ExeC<uint32_t, uint32_t, uint32_t>(0x6194C0, type, h); }, &BuildPlain);
    MEND(600)
    // the three event dispatchers: `used` (a canonical 0 / 1 bool as RsInputDeviceAttach stores it) selects between the installed handler (a tail call) and rsEVENTNOTPROCESSED (2)
    static const auto Dev = [](Rng& rr) {
        for (RsInputDevice* dv : { &RsGlobal.keyboard, &RsGlobal.mouse, &RsGlobal.pad }) { dv->used = rr.below(2); dv->inputEventHandler = (RsInputEventHandler)(uintptr_t)&HandlerStub; }
    };
#define EVH(NAME, ADDR) \
    MROW(#NAME, ADDR) \
        const uint32_t ev = r.below(4) ? r.below(32) : r.u32(); void* const par = (void*)(uintptr_t)r.u32(); \
        return PCase(r, d, [&]() -> uint64_t { return (uint32_t)NAME((RsEvent)ev, par); }, [&]() -> uint64_t { return ExeC<uint32_t, uint32_t, void*>(ADDR, ev, par); }, +Dev); \
    MEND(600)
    EVH(RsKeyboardEventHandler, 0x619560)
    EVH(RsMouseEventHandler, 0x619580)
    EVH(RsPadEventHandler, 0x6195A0)
    // rsPreInitCommandLine 0x619530: the argument is passed in eax (the exe compares 5 bytes with the "-vms" literal)
    MROW("rsPreInitCommandLine", 0x619530)
        static const char* const pick[] = { "-vms", "-vm", "-vmsx", "", "-VMS", "-vms ", "vms", "-vmz", "-vms\0x" };
        char* const s = (char*)S(kStr);
        if (r.below(4)) { const char* p = pick[r.below(sizeof pick / sizeof pick[0])]; std::memset(s, 'a', 0x40); std::strcpy(s, p); }
        else { for (int i = 0; i < 6; ++i) s[i] = (char)(r.below(3) ? "-vms\0"[i % 5] : (char)(1 + r.below(255))); s[6] = 0; }
        return PCase(r, d, [&]() -> uint64_t { return (uint8_t)rsPreInitCommandLine(s); },
            [&]() -> uint64_t { return (uint8_t)ExePreInit(s); }, &BuildPlain);
    MEND(400)
}

static void TestW3MiscCustomBuilding() {
    using namespace w2; using namespace w3m2;
    // UpdateDayNightBalanceParam 0x5D7F80: minutes = hours * 60 + minutes (int) + seconds / 60 (x87: the product and the sums are not rounded to float before the compares / the final store)
    static const auto Clk = [](Rng& rr) {
        CClock::ms_nGameClockHours = (uint8)(rr.below(12) ? (4 + rr.below(19)) : rr.below(256));
        CClock::ms_nGameClockMinutes = (uint8)(rr.below(12) ? rr.below(60) : rr.below(256));
        CClock::ms_nGameClockSeconds = (uint16)(rr.below(12) ? rr.below(60) : rr.below(65536));
        if (rr.below(3) == 0) {   // exactly on or next to the thresholds 6:00, 7:00, 20:00, 21:00
            static const int th[] = { 360, 420, 1200, 1260 };
            const int m = th[rr.below(4)] + (int)rr.below(3) - 1;
            CClock::ms_nGameClockHours = (uint8)(m / 60); CClock::ms_nGameClockMinutes = (uint8)(m % 60);
            CClock::ms_nGameClockSeconds = (uint16)(rr.below(2) ? 0 : rr.below(2) ? 59 : rr.below(60));
            if (rr.below(2) && CClock::ms_nGameClockMinutes > 0) { --CClock::ms_nGameClockMinutes; CClock::ms_nGameClockSeconds = (uint16)(55 + rr.below(10)); }
        }
    };
    MROW("CCustomBuildingRenderer::UpdateDayNightBalanceParam", 0x5D7F80)
        return PCase(r, d, [&]() -> uint64_t { CCustomBuildingRenderer::UpdateDayNightBalanceParam(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5D7F80); return 0; }, +Clk);
    MEND(6000)
    MROW("CCustomBuildingRenderer::Update", 0x5D8050)
        return PCase(r, d, [&]() -> uint64_t { CCustomBuildingRenderer::Update(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5D8050); return 0; }, +Clk);
    MEND(3000)
}
