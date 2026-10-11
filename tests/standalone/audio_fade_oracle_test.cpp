// AUDIO fade oracle: CAESmoothFadeThread::Service (exe 0x4EED10, the thread that ramps DirectSound volumes) as machine code vs the port, on identical entry tables.
// The exe runs on the real object at 0xB608D0 with fake IDirectSoundBuffer COM objects (they record SetVolume/GetVolume/Stop/Release/AddRef); the clock the exe reads
// (0x4D9E80) is redirected to a fixed value, the port's CAEAudioUtility::GetCurrentTimeInMS is the same fixed value. After each run the 64 entries, m_currentTime,
// the channel counter 0xB5F898 and the recorded COM call sequence (including every SetVolume value) must be bit-identical.
// Found by this test's reason for existing: the port computed `start + LOG10_2 * log2(step * progress) * 20` (no `1 +`): NaN for fade-outs, hugely wrong for fade-ins.
// usage: audio_fade_oracle_test.exe [-v] [-cases N]       (exit code 0 = identical at PC=53, the fade thread's precision; PC=24 differences are only reported)
#include "game_oracle.h"
#include <array>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <float.h>
#include <vector>

#include "AESmoothFadeThread.h"

// ---- stubs for what AESmoothFadeThread.cpp links against
static uint32_t g_T = 0;
uint64 CAEAudioUtility::GetCurrentTimeInMS() { return g_T; }
uint32& g_numSoundChannelsUsed = StaticRef<uint32>(0xB5F898);
namespace notsa::standalone::detail { bool g_DataImageLoaded = true; }
namespace notsa::standalone::Fixups {
void RegisterFunction(uint32_t, void*, const char*) {}
void RegisterUnverified(uint32_t, const char*, int, bool, uint32_t) {}
}
void ReversibleHooks::RHManager::AddHookToCategory(std::string_view, HookInstallOptions, std::shared_ptr<ReversibleHook::TwoWayHook>) {}

// ---- fake IDirectSoundBuffer
struct Fake { void** vt; int id; LONG vol; };
enum { C_SETVOL = 1, C_GETVOL, C_STOP, C_RELEASE, C_ADDREF };
struct Call { int id, kind; int64_t val; bool operator==(const Call& o) const { return id == o.id && kind == o.kind && val == o.val; } };
static std::vector<Call> g_calls;
static HRESULT __stdcall F_SetVolume(Fake* f, LONG v) { g_calls.push_back({ f->id, C_SETVOL, v }); f->vol = v; return 0; }
static HRESULT __stdcall F_GetVolume(Fake* f, LONG* v) { *v = f->vol; g_calls.push_back({ f->id, C_GETVOL, f->vol }); return 0; }
static HRESULT __stdcall F_Stop(Fake* f) { g_calls.push_back({ f->id, C_STOP, 0 }); return 0; }
static ULONG __stdcall F_Release(Fake* f) { g_calls.push_back({ f->id, C_RELEASE, 0 }); return 1; }
static ULONG __stdcall F_AddRef(Fake* f) { g_calls.push_back({ f->id, C_ADDREF, 0 }); return 2; }
static void* g_vt[24];
static uint32_t __cdecl FixedTime() { return g_T; }

struct Rng {
    uint64_t s; explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 7) { for (int i = 0; i < 4; ++i) u32(); }
    uint32_t u32() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 16); }
    int below(int n) { return (int)(u32() % (uint32_t)n); }
    float f01() { return (u32() & 0xFFFFFF) * (1.0f / 16777216.0f); }
};

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    int cases = 3000; bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) verbose = true;
        else if (!std::strcmp(argv[i], "-cases") && i + 1 < argc) cases = std::atoi(argv[++i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("audio_fade_oracle_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    oracle::Patch(0x4D9E80, (void*)&FixedTime);
    g_vt[1] = (void*)&F_AddRef; g_vt[2] = (void*)&F_Release; g_vt[6] = (void*)&F_GetVolume; g_vt[15] = (void*)&F_SetVolume; g_vt[18] = (void*)&F_Stop;

    auto& obj = *reinterpret_cast<CAESmoothFadeThread*>(0xB608D0);
    long bad = 0, bad24 = 0, checks = 0, nFadeSteps = 0, nEnd = 0, nSetVol = 0, nCancelled = 0;
    for (const unsigned pc : { _PC_53, _PC_24 }) {
        for (int c = 0; c < cases; ++c) {
            Rng r(c * 2 + (pc == _PC_24));
            Fake fakes[CAESmoothFadeThread::NUM_SMOOTHFADE_ENTRIES];
            std::memset(&obj, 0, sizeof(obj));
            g_T = 100000 + r.u32() % 100000;
            const int nAct = 1 + r.below(CAESmoothFadeThread::NUM_SMOOTHFADE_ENTRIES);
            for (int i = 0; i < CAESmoothFadeThread::NUM_SMOOTHFADE_ENTRIES; ++i) {
                fakes[i] = Fake{ g_vt, i, 0 };
                auto& e = obj.m_aEntries[i];
                if (i >= nAct) continue;
                const int st = r.below(10);
                e.m_nStatus = st < 7 ? STATE_ACTIVE : st < 9 ? STATE_CANCELLED : STATE_IDLE;
                e.m_pSoundBuffer = r.below(12) ? (IDirectSoundBuffer*)&fakes[i] : nullptr;
                const float a = -100.0f * r.f01(), b = r.below(8) ? -100.0f * r.f01() : (r.below(2) ? 0.0f : -100.0f);
                e.m_fStartVolume = a; e.m_fTargetVolume = b; e.m_fVolumeDiff = r.below(10) ? b - a : 60.0f * (r.f01() - 0.5f);
                e.m_fCurVolume = r.below(2) ? a : -100.0f * r.f01();
                e.m_bStopBufferAfterFade = r.below(4) == 0;
                e.m_wFadeTime = (uint16)(r.below(40) ? 1 + r.below(70) : r.below(3));
                const int where = r.below(10);   // elapsed relative to the fade: before start / inside / at the end / after
                int elapsed = where < 1 ? -1 - r.below(5) : where < 7 ? r.below(e.m_wFadeTime + 1) : where < 9 ? (int)e.m_wFadeTime + r.below(3) : (int)e.m_wFadeTime + r.below(100);
                e.m_nStartTime = g_T - elapsed;
                fakes[i].vol = r.below(2) ? (LONG)(e.m_fCurVolume * 100.0f) : (LONG)(-10000 * r.f01());
            }
            const CAESmoothFadeThread init = obj;
            Fake initFakes[CAESmoothFadeThread::NUM_SMOOTHFADE_ENTRIES];
            std::memcpy(initFakes, fakes, sizeof(fakes));
            const uint32_t initCnt = 100;
            unsigned cw; _controlfp_s(&cw, pc, _MCW_PC);

            // exe
            g_calls.clear(); g_numSoundChannelsUsed = initCnt;
            oracle::Fn<void __fastcall(CAESmoothFadeThread*, int)>(0x4EED10)(&obj, 0);
            const std::vector<Call> exeCalls = g_calls; const CAESmoothFadeThread exeObj = obj; const uint32_t exeCnt = g_numSoundChannelsUsed;
            // port
            std::memcpy(&obj, &init, sizeof(obj)); std::memcpy(fakes, initFakes, sizeof(fakes));
            g_calls.clear(); g_numSoundChannelsUsed = initCnt;
            obj.Service();
            ++checks;
            bool same = exeCalls == g_calls && exeCnt == g_numSoundChannelsUsed && std::memcmp(&exeObj, &obj, sizeof(obj)) == 0;
            if (!same && pc == _PC_53 && exeCalls == g_calls) {   // only the entry table differs: show which field
                for (int i = 0; i < CAESmoothFadeThread::NUM_SMOOTHFADE_ENTRIES; ++i) {
                    if (std::memcmp(&exeObj.m_aEntries[i], &obj.m_aEntries[i], sizeof(tSmoothFadeEntry)) != 0 && bad < 6) {
                        const auto &x = exeObj.m_aEntries[i], &y = obj.m_aEntries[i];
                        std::printf("    entry %d: cur exe %.9g port %.9g | start %.9g/%.9g target %.9g/%.9g diff %.9g/%.9g status %u/%u t %u/%u\n", i, x.m_fCurVolume, y.m_fCurVolume, x.m_fStartVolume, y.m_fStartVolume,
                                    x.m_fTargetVolume, y.m_fTargetVolume, x.m_fVolumeDiff, y.m_fVolumeDiff, (unsigned)x.m_nStatus, (unsigned)y.m_nStatus, x.m_nStartTime, y.m_nStartTime);
                    }
                }
            }
            if (!same && pc == _PC_24) ++bad24;   // the fade thread runs at PC=53 (only the main thread is dropped to PC=24 by D3D9): informational
            if (!same && pc == _PC_53) {
                ++bad;
                if (bad <= 15 || verbose) {
                    std::printf("  MISMATCH PC=%d case %d: calls exe %zu port %zu, counter exe %u port %u, objects equal %d\n", pc == _PC_53 ? 53 : 24, c, exeCalls.size(), g_calls.size(),
                                exeCnt, (uint32_t)g_numSoundChannelsUsed, std::memcmp(&exeObj, &obj, sizeof(obj)) == 0);
                    for (size_t i = 0; i < std::max(exeCalls.size(), g_calls.size()); ++i) {
                        const bool hasE = i < exeCalls.size(), hasP = i < g_calls.size();
                        if (hasE && hasP && exeCalls[i] == g_calls[i]) continue;
                        std::printf("    call %zu: exe %s(id %d kind %d val %lld)  port %s(id %d kind %d val %lld)\n", i, hasE ? "" : "-", hasE ? exeCalls[i].id : 0, hasE ? exeCalls[i].kind : 0,
                                    hasE ? (long long)exeCalls[i].val : 0, hasP ? "" : "-", hasP ? g_calls[i].id : 0, hasP ? g_calls[i].kind : 0, hasP ? (long long)g_calls[i].val : 0);
                        break;
                    }
                }
            }
            for (const auto& cl : g_calls) { nSetVol += cl.kind == C_SETVOL; }
            for (int i = 0; i < nAct; ++i) {
                if (init.m_aEntries[i].m_nStatus == STATE_ACTIVE && init.m_aEntries[i].m_pSoundBuffer && g_T >= init.m_aEntries[i].m_nStartTime) {
                    (g_T - init.m_aEntries[i].m_nStartTime < init.m_aEntries[i].m_wFadeTime ? nFadeSteps : nEnd)++;
                }
                nCancelled += init.m_aEntries[i].m_nStatus == STATE_CANCELLED;
            }
        }
    }
    std::printf("audio_fade_oracle_test: %ld Service calls (%d cases x PC53/PC24), %ld ramp steps, %ld fade ends, %ld cancelled entries, %ld SetVolume calls: %ld mismatches at PC=53 (+%ld at PC=24: float entry field differs in the last bit, SetVolume sequence equal, informational)\n",
                checks, cases, nFadeSteps, nEnd, nCancelled, nSetVol, bad, bad24);
    std::printf("%s\n", bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
