// P-2g host test (portable arm64): CRunningScript_File (0xE0), the FileRecord trait of CRunningScript and the REAL per-script loops of CTheScripts::Save / CTheScripts::Load.
//   tools/portable/run_p2g_test.sh   Generated into $OUT (all text is extracted from the REAL sources, nothing is retyped):
//     p2g_cut.h       the constants (MAIN/MISSION_SCRIPT_SIZE, MAX_STACK_DEPTH, ShortString) and `class CRunningScript` data members cut from Scripts/RunningScript.h (+ the real ScriptParam.h)
//     p2g_trait.inc   `template<> struct notsa::FileRecord<CRunningScript> {...}` verbatim from Scripts/RunningScript.h
//     p2g_snippets.inc  the `for (auto* s = lastScript; ...)` loop of CTheScripts::Save and the `auto numScripts = ...` loop of CTheScripts::Load verbatim from Scripts/TheScripts.cpp
//     p2g_golden.inc  golden vectors from tools/portable/gen_x86_golden.py (independent of the code under test)
//   Real main.scm (P2G_MAIN_SCM; the Steam copy, never committed): scripts are started at real entry offsets (goto/gosub/start_new_script targets found in the byte stream).
#include <Base.h>
#define MAX_PATH 260
#define NOTSA_GLOBAL(name, addr, type, ...) std::type_identity_t<NOTSA_UNPAREN type> name __VA_ARGS__
#include <array>
#include <span>
#include "GenericGameStorage.h"
#include "FileRecord.hpp"
#include "FileRecords/ScriptFile.hpp"
#include "p2g_cut.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <set>
#include <string>
#include <vector>
#include "p2g_golden.inc"

#define P2G_STR2(x) #x
#define P2G_STR(x) P2G_STR2(x)
#ifndef NOTSA_PORTABLE
#error "this test is for the portable build"
#endif

constexpr uint32 MAX_NUM_SCRIPTS = 96;
constexpr size_t SCRIPT_SPACE_SIZE_ = MAIN_SCRIPT_SIZE + MISSION_SCRIPT_SIZE;

struct CTheScripts {
    static constexpr uint32 SCRIPT_SPACE_SIZE = SCRIPT_SPACE_SIZE_;
    static inline std::array<uint8, SCRIPT_SPACE_SIZE_>                ScriptSpace{};
    static inline std::span<uint8, MAIN_SCRIPT_SIZE>                   MainSCMBlock{ ScriptSpace.data(), MAIN_SCRIPT_SIZE };
    static inline std::span<uint8, MISSION_SCRIPT_SIZE>                MissionBlock{ ScriptSpace.data() + MAIN_SCRIPT_SIZE, MISSION_SCRIPT_SIZE };
    static inline std::array<CRunningScript, MAX_NUM_SCRIPTS>          ScriptsArray{};
    static CRunningScript* StartNewScript(uint8* startIP, uint16 index);
    static int GetScriptIndexFromPointer(CRunningScript* s) { return int(s - ScriptsArray.data()); }
    static void SaveLoop(CRunningScript* lastScript);
    static void LoadLoop();
};
#include "p2g_trait.inc"

// the fresh state of an idle script as StartNewScript leaves it (links from the idle list, name "noname", nothing else we care about)
static void freshScript(CRunningScript& s, uint32 idx) {
    std::memset((void*)&s, 0, sizeof(s));
    s.m_pNext = idx + 1 < MAX_NUM_SCRIPTS ? &CTheScripts::ScriptsArray[idx + 1] : nullptr;
    s.m_pPrev = idx > 0 ? &CTheScripts::ScriptsArray[idx - 1] : nullptr;
    std::memcpy(s.m_szName, "noname", 7);
    s.m_BaseIP = reinterpret_cast<uint8*>(uintptr_t(0x7777));   // junk that must be replaced
    s.m_StackDepth = 0x7777;
    s.m_WakeTime = 0x7777;
}
CRunningScript* CTheScripts::StartNewScript(uint8* startIP, uint16 index) {
    auto* s = &ScriptsArray[index];
    freshScript(*s, index);
    if (startIP) { s->m_IP = startIP; }
    s->m_IsActive = true;
    return s;
}
#include "p2g_snippets.inc"

static std::vector<uint8> g_buf; static size_t g_pos = 0;
bool CGenericGameStorage::SaveDataToWorkBuffer(void* d, int32 n) { auto* p = (uint8*)d; g_buf.insert(g_buf.end(), p, p + n); return true; }
bool CGenericGameStorage::LoadDataFromWorkBuffer(void* d, int32 n) { if (g_pos + n > g_buf.size()) return false; std::memcpy(d, g_buf.data() + g_pos, n); g_pos += n; return true; }
static void reset() { g_buf.clear(); g_pos = 0; }

static int fails = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); fails++; } } while (0)
#define CHECKM(x, ...) do { if (!(x)) { std::printf("FAIL line %d: %s  ", __LINE__, #x); std::printf(__VA_ARGS__); std::printf("\n"); fails++; } } while (0)
static bool eq(const void* a, const void* b, size_t n) { return std::memcmp(a, b, n) == 0; }

// ---- layout ---------------------------------------------------------------------------------------------------------------------------------------------
static_assert(notsa::HasFileRecord<CRunningScript> && notsa::FileSizeOf<CRunningScript> == 0xE0);
static_assert(sizeof(tScriptParam) == 4);
static_assert(sizeof(CRunningScript) != 0xE0, "LP64: the pointer slots widen CRunningScript (this is what the record hides)");
static_assert(std::is_same_v<notsa::FileRecord<CRunningScript>::Type, CRunningScript_File> && std::is_trivially_copyable_v<CRunningScript_File>);
static_assert(sizeof(kGolden_CRunningScript_File_exe) == 0xE0 && sizeof(kGolden_CRunningScript_File_port) == 0xE0 && sizeof(kGolden_CRunningScript_File_mission) == 0xE0);
static_assert(MAX_STACK_DEPTH == 8 && MAX_LOCAL_VARS + MAX_NUM_TIMERS == 34 && MAIN_SCRIPT_SIZE == 200000);
static_assert(ScriptSpaceExeVA == 0xA49960 && ScriptSpaceExeVA + MAIN_SCRIPT_SIZE == 0xA7A6A0, "MissionBlock VA of the exe");

// ---- an independent description of one script + an independent (literal-offset) encoder of the exe record --------------------------------------------------
constexpr uint32 VA = 0xA49960;   // literal on purpose: not ScriptSpaceExeVA
struct Spec {
    char    name[8]{};
    int32   baseOff = -1, ipOff = -1, stackOff[8]{-1, -1, -1, -1, -1, -1, -1, -1};   // offsets into ScriptSpace, -1 = null
    uint16  depth = 0;
    uint32  locals[34]{};
    uint8   active = 0, cond = 0, cleanup = 0, external = 0, textOvr = 0; int8 extType = -1;
    int32   wake = 0; uint16 andor = 0; uint8 notFlag = 0, deathArrest = 0, doneDeath = 0; int32 sceneSkip = 0; uint8 mission = 0;
};
static uint8* P(int32 off) { return off < 0 ? nullptr : CTheScripts::ScriptSpace.data() + off; }
static void apply(CRunningScript& s, const Spec& sp) {
    std::memcpy(s.m_szName, sp.name, 8);
    s.m_BaseIP = P(sp.baseOff); s.m_IP = P(sp.ipOff);
    for (int i = 0; i < 8; i++) { s.m_IPStack[i] = P(sp.stackOff[i]); }
    s.m_StackDepth = sp.depth;
    for (int i = 0; i < 34; i++) { s.m_LocalVars[i].uParam = sp.locals[i]; }
    s.m_IsActive = sp.active; s.m_CondResult = sp.cond; s.m_UsesMissionCleanup = sp.cleanup; s.m_IsExternal = sp.external; s.m_IsTextBlockOverride = sp.textOvr;
    s.m_ExternalType = sp.extType; s.m_WakeTime = sp.wake; s.m_AndOrState = sp.andor; s.m_NotFlag = sp.notFlag; s.m_IsDeathArrestCheckEnabled = sp.deathArrest;
    s.m_DoneDeathArrest = sp.doneDeath; s.m_SceneSkipIP = sp.sceneSkip; s.m_ThisMustBeTheOnlyMissionRunning = sp.mission;
}
template<class T> static void put(std::vector<uint8>& b, size_t off, T v) { std::memcpy(b.data() + off, &v, sizeof(T)); }
static uint32 vaOf(int32 off) { return off < 0 ? 0 : VA + (uint32)off; }
// what the PORT must write (links 0, pointer slots = exe VA of the offset or 0)
static std::vector<uint8> encRecord(const Spec& sp, uint32 next = 0, uint32 prev = 0) {
    std::vector<uint8> b(0xE0, 0);
    put<uint32>(b, 0x00, next); put<uint32>(b, 0x04, prev);
    std::memcpy(b.data() + 0x08, sp.name, 8);
    put<uint32>(b, 0x10, vaOf(sp.baseOff)); put<uint32>(b, 0x14, vaOf(sp.ipOff));
    for (int i = 0; i < 8; i++) { put<uint32>(b, 0x18 + 4 * i, vaOf(sp.stackOff[i])); }
    put<uint16>(b, 0x38, sp.depth);
    for (int i = 0; i < 34; i++) { put<uint32>(b, 0x3C + 4 * i, sp.locals[i]); }
    put<uint8>(b, 0xC4, sp.active); put<uint8>(b, 0xC5, sp.cond); put<uint8>(b, 0xC6, sp.cleanup); put<uint8>(b, 0xC7, sp.external); put<uint8>(b, 0xC8, sp.textOvr);
    put<int8>(b, 0xC9, sp.extType); put<int32>(b, 0xCC, sp.wake); put<uint16>(b, 0xD0, sp.andor); put<uint8>(b, 0xD2, sp.notFlag); put<uint8>(b, 0xD3, sp.deathArrest);
    put<uint8>(b, 0xD4, sp.doneDeath); put<int32>(b, 0xD8, sp.sceneSkip); put<uint8>(b, 0xDC, sp.mission);
    return b;
}
static void streamEntry(std::vector<uint8>& out, uint16 idx, const std::vector<uint8>& rec, const Spec& sp) {   // Save's per-script stream: int16 index, record, u32 ip offset, 8 x u32 stack offsets
    auto add = [&](const void* p, size_t n) { out.insert(out.end(), (const uint8*)p, (const uint8*)p + n); };
    add(&idx, 2); add(rec.data(), rec.size());
    uint32 v = sp.ipOff < 0 ? 0 : (uint32)sp.ipOff; add(&v, 4);   // the exe wrote (uint32)(m_IP - ScriptSpace)
    for (int i = 0; i < 8; i++) { v = sp.stackOff[i] < 0 ? 0 : (uint32)sp.stackOff[i]; add(&v, 4); }
}

static void expectScript(const CRunningScript& s, const Spec& sp, const char* what, int line) {
    auto bad = [&](const char* f) { std::printf("FAIL line %d (%s): field %s\n", line, what, f); fails++; };
    if (std::memcmp(s.m_szName, sp.name, 8)) bad("name");
    if (s.m_IP != P(sp.ipOff)) bad("m_IP");
    for (int i = 0; i < 8; i++) { if (s.m_IPStack[i] != P(sp.stackOff[i])) bad("m_IPStack"); }
    if (s.m_StackDepth != sp.depth) bad("depth");
    for (int i = 0; i < 34; i++) { if (s.m_LocalVars[i].uParam != sp.locals[i]) bad("locals"); }
    if (s.m_IsActive != (bool)sp.active) bad("active");
    if (s.m_CondResult != (bool)sp.cond) bad("cond");
    if (s.m_UsesMissionCleanup != (bool)sp.cleanup) bad("cleanup");
    if (s.m_IsExternal != (bool)sp.external) bad("external");
    if (s.m_IsTextBlockOverride != (bool)sp.textOvr) bad("textOvr");
    if (s.m_ExternalType != sp.extType) bad("extType");
    if (s.m_WakeTime != sp.wake) bad("wake");
    if (s.m_AndOrState != sp.andor) bad("andor");
    if (s.m_NotFlag != (bool)sp.notFlag) bad("notFlag");
    if (s.m_IsDeathArrestCheckEnabled != (bool)sp.deathArrest) bad("deathArrest");
    if (s.m_DoneDeathArrest != (bool)sp.doneDeath) bad("doneDeath");
    if (s.m_SceneSkipIP != sp.sceneSkip) bad("sceneSkip");
    if (s.m_ThisMustBeTheOnlyMissionRunning != (bool)sp.mission) bad("mission");
}
// the BaseIP rule: VA inside ScriptSpace -> that pointer; else (null / foreign) mission ? MissionBlock : ScriptSpace
static uint8* expectedBase(const Spec& sp) { return (sp.baseOff >= 0 && (size_t)sp.baseOff < SCRIPT_SPACE_SIZE_) ? P(sp.baseOff) : (sp.mission ? CTheScripts::MissionBlock.data() : CTheScripts::ScriptSpace.data()); }

static std::vector<uint32> g_mentries;   // real mission-script entry offsets relative to MissionBlock (filled from main.scm)
static Spec randomSpec(std::mt19937& r, const std::vector<uint32>& entries, bool mission) {
    Spec sp;
    static const char* names[] = { "MAIN", "BCESAR", "LAS1A", "noname", "X" };
    std::memset(sp.name, 0, 8); std::strncpy(sp.name, names[r() % 5], 7);
    auto entry = [&]() { return (int32)entries[r() % entries.size()]; };
    auto mentry = [&]() { return (int32)(g_mentries.empty() ? r() % MISSION_SCRIPT_SIZE : g_mentries[r() % g_mentries.size()]); };
    if (mission) {
        sp.baseOff = MAIN_SCRIPT_SIZE; sp.ipOff = MAIN_SCRIPT_SIZE + mentry();
        sp.cleanup = 1; sp.mission = 1;
    } else {
        sp.baseOff = (r() & 3) ? 0 : -1; sp.ipOff = entry();
    }
    sp.depth = r() % 9;
    for (int i = 0; i < 8; i++) { sp.stackOff[i] = (i < (int)sp.depth + 2 && (r() & 7)) ? (mission ? MAIN_SCRIPT_SIZE + mentry() : entry()) : -1; if (sp.stackOff[i] == 0) { sp.stackOff[i] = 1; } }
    for (auto& l : sp.locals) { l = r(); }
    sp.active = r() & 1; sp.cond = r() & 1; if (!mission) { sp.cleanup = r() & 1; } sp.external = 0; sp.textOvr = r() & 1; sp.extType = -1;
    sp.wake = (int32)r(); sp.andor = r() % 30; sp.notFlag = r() & 1; sp.deathArrest = r() & 1; sp.doneDeath = r() & 1; sp.sceneSkip = (int32)r();
    return sp;
}

// Save N scripts through the REAL loop, then check the stream bytes against the independent encoder, Load them through the REAL loop into fresh scripts, compare.
static void roundTrip(const std::vector<std::pair<uint16, Spec>>& scripts, bool keepStream, const char* what, int line) {
    for (uint32 i = 0; i < MAX_NUM_SCRIPTS; i++) { freshScript(CTheScripts::ScriptsArray[i], i); }
    // active list: scripts in the given order, m_pNext chain; Save walks from the last one backwards through m_pPrev
    std::vector<CRunningScript*> list;
    for (auto& [idx, sp] : scripts) { auto* s = &CTheScripts::ScriptsArray[idx]; apply(*s, sp); list.push_back(s); }
    for (size_t i = 0; i < list.size(); i++) { list[i]->m_pPrev = i ? list[i - 1] : nullptr; list[i]->m_pNext = i + 1 < list.size() ? list[i + 1] : nullptr; }
    reset();
    CTheScripts::SaveLoop(list.back());
    std::vector<uint8> want;
    for (size_t i = list.size(); i-- > 0;) { streamEntry(want, scripts[i].first, encRecord(scripts[i].second), scripts[i].second); }
    CHECKM(g_buf.size() == want.size() && eq(g_buf.data(), want.data(), std::min(g_buf.size(), want.size())), "%s: Save stream != independent encoding (%zu vs %zu bytes)", what, g_buf.size(), want.size());
    CHECKM(g_buf.size() == scripts.size() * (2 + 0xE0 + 4 + 32), "%s: stream size", what);
    // Load: numScripts first (the caller of the loop reads it from the stream; Save writes it just before the loop)
    const auto saved = g_buf;
    std::vector<uint8> in; const uint32 n = (uint32)scripts.size(); in.insert(in.end(), (const uint8*)&n, (const uint8*)&n + 4); in.insert(in.end(), saved.begin(), saved.end());
    for (uint32 i = 0; i < MAX_NUM_SCRIPTS; i++) { freshScript(CTheScripts::ScriptsArray[i], i); }
    g_buf = in; g_pos = 0;
    CTheScripts::LoadLoop();
    CHECKM(g_pos == g_buf.size(), "%s: Load consumed %zu of %zu bytes", what, g_pos, g_buf.size());
    // the loop loads in stream order = reverse list order
    for (size_t i = list.size(); i-- > 0;) {
        auto& s = CTheScripts::ScriptsArray[scripts[i].first];
        expectScript(s, scripts[i].second, what, line);
        CHECKM(s.m_BaseIP == expectedBase(scripts[i].second), "%s: BaseIP rule", what);
        const auto idx = scripts[i].first;
        CHECKM(s.m_pNext == (idx + 1 < MAX_NUM_SCRIPTS ? &CTheScripts::ScriptsArray[idx + 1] : nullptr) && s.m_pPrev == (idx > 0 ? &CTheScripts::ScriptsArray[idx - 1] : nullptr), "%s: list links must be the fresh script's", what);
    }
    // re-save of the loaded scripts: identical bytes (BaseIP aside: the rule may rebuild it, but that only changes the baseIP slot when the original was null/foreign)
    if (keepStream) {
        std::vector<CRunningScript*> l2; for (auto& [idx, sp] : scripts) { l2.push_back(&CTheScripts::ScriptsArray[idx]); }
        for (size_t i = 0; i < l2.size(); i++) { l2[i]->m_pPrev = i ? l2[i - 1] : nullptr; l2[i]->m_pNext = i + 1 < l2.size() ? l2[i + 1] : nullptr; }
        reset(); CTheScripts::SaveLoop(l2.back());
        std::vector<uint8> want2;
        for (size_t i = l2.size(); i-- > 0;) { Spec sp2 = scripts[i].second; sp2.baseOff = (int32)(expectedBase(sp2) - CTheScripts::ScriptSpace.data()); streamEntry(want2, scripts[i].first, encRecord(sp2), sp2); }
        CHECKM(g_buf == want2, "%s: re-save differs", what);
    }
}

int main() {
    // ---- the cut class on LP64 is NOT the file layout; the record is -------------------------------------------------------------------------------------
    std::printf("LP64 sizeof(CRunningScript)=0x%zx, sizeof(CRunningScript_File)=0x%zx\n", sizeof(CRunningScript), sizeof(CRunningScript_File));
    // ---- golden 'exe' : what the exe wrote -> object ----------------------------------------------------------------------------------------------------
    {
        CRunningScript s; freshScript(s, 5); const auto links = std::pair{ s.m_pNext, s.m_pPrev };
        s.m_IP = P(5); for (auto& q : s.m_IPStack) { q = P(7); }   // stale pointers: FromFile must clear them (Load sets them from the trailing offsets)
        CRunningScript_File f; std::memcpy(&f, kGolden_CRunningScript_File_exe, sizeof(f));
        notsa::FileRecord<CRunningScript>::FromFile(s, f);
        CHECK(s.m_pNext == links.first && s.m_pPrev == links.second);   // the exe's heap garbage must NOT reach the live list
        CHECK(!std::memcmp(s.m_szName, "MAIN\0\0\0\0", 8));
        CHECK(s.m_BaseIP == CTheScripts::ScriptSpace.data());           // VA 0xA49960 -> ScriptSpace
        CHECK(s.m_IP == nullptr);
        for (auto* p : s.m_IPStack) { CHECK(p == nullptr); }
        CHECK(s.m_StackDepth == 3 && s.m_WakeTime == 123456 && s.m_AndOrState == 22 && s.m_SceneSkipIP == -1 && s.m_ExternalType == -1);
        CHECK(s.m_IsActive && s.m_CondResult && !s.m_UsesMissionCleanup && !s.m_IsExternal && s.m_IsTextBlockOverride && s.m_NotFlag && s.m_IsDeathArrestCheckEnabled && !s.m_DoneDeathArrest && !s.m_ThisMustBeTheOnlyMissionRunning);
        for (int i = 0; i < 34; i++) { CHECK(s.m_LocalVars[i].uParam == uint32((i * 0x9E3779B1u) & 0xFFFFFFFFu)); }
        // restore the IPs from the offsets (what Load does) and write it back: == golden 'port' (links 0, VAs live)
        s.m_IP = P(0x1234);
        const int32 st[8] = { 0x2000, 0x2468, 0x10, -1, 0x7FFF, 1, 2, 3 };
        for (int i = 0; i < 8; i++) { s.m_IPStack[i] = P(st[i]); }
        const auto out = notsa::FileRecord<CRunningScript>::ToFile(s);
        CHECK(eq(&out, kGolden_CRunningScript_File_port, sizeof(out)));
    }
    // ---- golden 'port' : object -> ToFile ---------------------------------------------------------------------------------------------------------------
    {
        Spec sp; std::memcpy(sp.name, "MAIN\0\0\0\0", 8); sp.baseOff = 0; sp.ipOff = 0x1234;
        const int32 st[8] = { 0x2000, 0x2468, 0x10, -1, 0x7FFF, 1, 2, 3 }; for (int i = 0; i < 8; i++) { sp.stackOff[i] = st[i]; }
        sp.depth = 3; for (int i = 0; i < 34; i++) { sp.locals[i] = uint32((i * 0x9E3779B1u) & 0xFFFFFFFFu); }
        sp.active = sp.cond = sp.textOvr = sp.notFlag = sp.deathArrest = 1; sp.extType = -1; sp.wake = 123456; sp.andor = 22; sp.sceneSkip = -1;
        CRunningScript s; freshScript(s, 3); s.m_pNext = reinterpret_cast<CRunningScript*>(uintptr_t(0x123456789)); s.m_pPrev = reinterpret_cast<CRunningScript*>(uintptr_t(0x987654321)); apply(s, sp);
        const auto out = notsa::FileRecord<CRunningScript>::ToFile(s);
        CHECK(eq(&out, kGolden_CRunningScript_File_port, sizeof(out)));
        const auto indep = encRecord(sp);
        CHECK(eq(indep.data(), kGolden_CRunningScript_File_port, 0xE0));   // the test's own encoder agrees with the python golden
        CHECK(out.next == 0 && out.prev == 0);
        // typed Save/Load through the trait: 0xE0 bytes, no more
        reset(); CGenericGameStorage::SaveDataToWorkBuffer(s);
        CHECK(g_buf.size() == 0xE0 && eq(g_buf.data(), kGolden_CRunningScript_File_port, 0xE0));
    }
    // ---- golden 'mission' -------------------------------------------------------------------------------------------------------------------------------
    {
        CRunningScript s; freshScript(s, 9);
        CRunningScript_File f; std::memcpy(&f, kGolden_CRunningScript_File_mission, sizeof(f));
        notsa::FileRecord<CRunningScript>::FromFile(s, f);
        CHECK(s.m_ThisMustBeTheOnlyMissionRunning && s.m_UsesMissionCleanup && s.m_BaseIP == CTheScripts::MissionBlock.data());
        CHECK(!std::memcmp(s.m_szName, "FLASH\0\0\0", 8) && s.m_StackDepth == 1 && s.m_WakeTime == -5 && s.m_SceneSkipIP == 0x4321 && s.m_DoneDeathArrest && !s.m_IsActive == false);
        s.m_IP = P(MAIN_SCRIPT_SIZE + 0x400); s.m_IPStack[0] = P(MAIN_SCRIPT_SIZE + 0x100);
        const auto out = notsa::FileRecord<CRunningScript>::ToFile(s);
        CHECK(eq(&out, kGolden_CRunningScript_File_mission, sizeof(out)));
    }
    // ---- base IP rule: file value 0 / foreign VA -> by the mission flag; VA in range -> exact; boundaries of the range ----------------------------------------------
    {
        CRunningScript s; freshScript(s, 1); CRunningScript_File f; std::memcpy(&f, kGolden_CRunningScript_File_port, sizeof(f));
        auto base = [&](uint32 va, uint8 mission) { f.baseIP = va; f.thisMustBeTheOnlyMissionRunning = mission; notsa::FileRecord<CRunningScript>::FromFile(s, f); return s.m_BaseIP; };
        auto* sp = CTheScripts::ScriptSpace.data();
        CHECK(base(0, 0) == sp); CHECK(base(0, 1) == CTheScripts::MissionBlock.data());
        CHECK(base(0xDEADBEEF, 0) == sp); CHECK(base(0xDEADBEEF, 1) == CTheScripts::MissionBlock.data());
        CHECK(base(VA - 1, 0) == sp); CHECK(base(VA - 1, 1) == CTheScripts::MissionBlock.data());
        CHECK(base(VA, 1) == sp);   // exact mapping wins over the mission rule
        CHECK(base(VA + 0x30D40, 0) == sp + 0x30D40);
        CHECK(base(VA + (uint32)SCRIPT_SPACE_SIZE_ - 1, 0) == sp + SCRIPT_SPACE_SIZE_ - 1);
        CHECK(base(VA + (uint32)SCRIPT_SPACE_SIZE_, 0) == sp); CHECK(base(VA + (uint32)SCRIPT_SPACE_SIZE_, 1) == CTheScripts::MissionBlock.data());
        // ToFile boundaries: first/last byte of ScriptSpace map, one before/after and foreign pointers are 0
        CRunningScript t; freshScript(t, 2);
        auto ipSlot = [&](uint8* p) { t.m_IP = p; return notsa::FileRecord<CRunningScript>::ToFile(t).ip; };
        CHECK(ipSlot(sp) == VA); CHECK(ipSlot(sp + SCRIPT_SPACE_SIZE_ - 1) == VA + SCRIPT_SPACE_SIZE_ - 1); CHECK(ipSlot(sp + SCRIPT_SPACE_SIZE_) == 0); CHECK(ipSlot(sp - 1) == 0);
        uint8 foreign[4]; CHECK(ipSlot(foreign) == 0); CHECK(ipSlot(nullptr) == 0);
    }
    // ---- exe-written stream: garbage in every pointer slot, IPs only from the trailing offsets -------------------------------------------------------------------------
    {
        std::mt19937 r(7);
        for (int round = 0; round < 50; round++) {
            Spec sp = randomSpec(r, { 0, 0x10, 0x1234, 0x2000, 1999 }, round & 1);
            for (uint32 i = 0; i < MAX_NUM_SCRIPTS; i++) { freshScript(CTheScripts::ScriptsArray[i], i); }
            auto rec = encRecord(sp, r(), r());
            put<uint32>(rec, 0x10, r()); put<uint32>(rec, 0x14, r());                       // baseIP / ip: garbage VAs
            for (int i = 0; i < 8; i++) { put<uint32>(rec, 0x18 + 4 * i, r()); }            // ipStack: garbage
            for (int i : { 0x3A, 0x3B, 0xCA, 0xCB, 0xD5, 0xD6, 0xD7, 0xDD, 0xDE, 0xDF }) { rec[i] = uint8(r()); }   // padding garbage
            for (int i : { 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xD2, 0xD3, 0xD4, 0xDC }) { if (rec[i]) { rec[i] = 1 + r() % 255; } }   // non-canonical bool bytes
            Spec expect = sp; expect.baseOff = -1;   // garbage baseIP: the rule applies (random u32 is outside the 269000-byte window except with probability ~6e-5; skipped below)
            const uint32 b = *(uint32*)(rec.data() + 0x10);
            if (b >= VA && b - VA < SCRIPT_SPACE_SIZE_) { continue; }
            reset(); const uint32 n = 1; const uint16 idx = (uint16)(r() % MAX_NUM_SCRIPTS);
            g_buf.insert(g_buf.end(), (const uint8*)&n, (const uint8*)&n + 4);
            std::vector<uint8> e; streamEntry(e, idx, rec, sp); g_buf.insert(g_buf.end(), e.begin(), e.end());
            g_pos = 0; CTheScripts::LoadLoop();
            CHECKM(g_pos == g_buf.size(), "exe stream consumed");
            auto& s = CTheScripts::ScriptsArray[idx];
            expectScript(s, sp, "exe-written", __LINE__);
            CHECK(s.m_BaseIP == expectedBase(expect));
            CHECK(s.m_pNext == (idx + 1 < MAX_NUM_SCRIPTS ? &CTheScripts::ScriptsArray[idx + 1] : nullptr) && s.m_pPrev == (idx > 0 ? &CTheScripts::ScriptsArray[idx - 1] : nullptr));
        }
    }
    // ---- REAL main.scm ----------------------------------------------------------------------------------------------------------------------------------------
    const char* scmPath = std::getenv("P2G_MAIN_SCM");
    std::ifstream in(scmPath ? scmPath : "", std::ios::binary);
    std::vector<uint8> scm((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (scm.empty()) {
        if (std::getenv("P2G_ALLOW_NO_SCM")) { std::printf("SKIP real main.scm (%s)\n", scmPath ? scmPath : "P2G_MAIN_SCM unset"); }
        else { std::printf("FAIL: real main.scm not readable (%s)\n", scmPath ? scmPath : "P2G_MAIN_SCM unset"); fails++; }
    } else {
        // like CTheScripts::Init: the first MAIN_SCRIPT_SIZE bytes of main.scm are the main block (the file is the multi-script file: mission scripts follow, chunk 1 lists their offsets)
        CHECK(scm.size() > MAIN_SCRIPT_SIZE);
        const size_t mainSize = std::min<size_t>(scm.size(), MAIN_SCRIPT_SIZE);
        std::memset(CTheScripts::ScriptSpace.data(), 0, SCRIPT_SPACE_SIZE_);
        std::memcpy(CTheScripts::ScriptSpace.data(), scm.data(), mainSize);
        // real entry offsets: targets of goto (0x0002), gosub (0x0050), start_new_script (0x004F), goto_if_false (0x004D), each with a 32-bit operand (type byte 0x01).
        // In the main block targets are absolute offsets; in a mission script they are negative = relative to the script start.
        std::set<uint32> ent; ent.insert(0);
        size_t byOp[4] = {};
        const uint16 ops[4] = { 0x0002, 0x0050, 0x004F, 0x004D };
        auto scan = [&](const uint8* d, size_t n, bool mission, std::set<uint32>& dst) {
            for (size_t i = 0; i + 7 <= n; i++) {
                for (int k = 0; k < 4; k++) {
                    if (d[i] == (ops[k] & 0xFF) && d[i + 1] == (ops[k] >> 8) && d[i + 2] == 0x01) {
                        int32 t; std::memcpy(&t, &d[i + 3], 4);
                        if (mission) { t = -t; }
                        if (t > 0 && (size_t)t < n) { dst.insert((uint32)t); if (!mission) { byOp[k]++; } }
                    }
                }
            }
        };
        scan(scm.data(), mainSize, false, ent);
        // chunk 1 (script file info) -> a real mission script copied into MissionBlock
        uint32 chunkMain = 0, chunkLargest = 0; uint16 nMiss = 0; std::vector<uint32> missOffs;
        for (size_t off = 0, guard = 0; guard < 8 && off + 0x18 <= scm.size(); guard++) {
            if (scm[off] != 0x02 || scm[off + 1] != 0x00 || scm[off + 2] != 0x01) { break; }
            uint32 next; std::memcpy(&next, &scm[off + 3], 4);
            if (scm[off + 7] == 1) {
                std::memcpy(&chunkMain, &scm[off + 8], 4); std::memcpy(&chunkLargest, &scm[off + 12], 4); std::memcpy(&nMiss, &scm[off + 16], 2);
                for (uint16 m = 0; m < nMiss && off + 0x18 + 4 * (m + 1) <= scm.size(); m++) { uint32 o; std::memcpy(&o, &scm[off + 0x18 + 4 * m], 4); missOffs.push_back(o); }
                break;
            }
            off = next;
        }
        std::set<uint32> ment; size_t missionsLoaded = 0;
        for (size_t m = 0; m < missOffs.size() && ment.size() < 400; m++) {   // use the first missions that fit and have entries (one mission lives in MissionBlock at a time: take the one with most entries)
            if (missOffs[m] >= scm.size()) { continue; }
            const size_t n = std::min<size_t>(MISSION_SCRIPT_SIZE, scm.size() - missOffs[m]);
            std::set<uint32> cand; scan(&scm[missOffs[m]], n, true, cand);
            if (cand.size() > ment.size()) { ment = cand; std::memset(CTheScripts::MissionBlock.data(), 0, MISSION_SCRIPT_SIZE); std::memcpy(CTheScripts::MissionBlock.data(), &scm[missOffs[m]], n); missionsLoaded = m + 1; }
        }
        g_mentries.assign(ment.begin(), ment.end());
        std::printf("main.scm chunk 1: mainScriptSize=%u largestMission=%u missions=%u; mission #%zu in MissionBlock has %zu real entry offsets\n", chunkMain, chunkLargest, nMiss, missionsLoaded, g_mentries.size());
        CHECK(nMiss > 50 && g_mentries.size() > 20);
        const bool startsWithGoto = mainSize > 7 && scm[0] == 0x02 && scm[1] == 0x00 && scm[2] == 0x01;
        std::printf("main.scm: %zu bytes (main block %zu), starts with GOTO: %d, entry offsets: %zu (goto %zu, gosub %zu, start_new_script %zu, goto_if_false %zu)\n", scm.size(), mainSize, startsWithGoto, ent.size(), byOp[0], byOp[1], byOp[2], byOp[3]);
        CHECK(startsWithGoto);
        CHECK(ent.size() > 1000);
        std::vector<uint32> entries(ent.begin(), ent.end());
        std::mt19937 r(2026);
        // (1) one script at EVERY entry offset (ip = entry k, stack = neighbours): exhaustive over the real offsets, in batches of 24 scripts
        size_t total = 0;
        for (size_t base = 0; base < entries.size(); base += 24) {
            std::vector<std::pair<uint16, Spec>> sc;
            for (size_t j = base; j < std::min(entries.size(), base + 24); j++) {
                Spec sp = randomSpec(r, entries, false); sp.ipOff = (int32)entries[j];
                for (int i = 0; i < 8; i++) { if (sp.stackOff[i] >= 0) { sp.stackOff[i] = (int32)entries[(j + 1 + i * 7) % entries.size()]; if (sp.stackOff[i] == 0) { sp.stackOff[i] = 1; } } }
                sc.push_back({ (uint16)(sc.size() * 3 + 1), sp }); total++;
            }
            roundTrip(sc, true, "real-all-entries", __LINE__);
        }
        // (2) random mixes incl. mission scripts (IP inside MissionBlock), random indices, 1..30 scripts
        for (int t = 0; t < 400; t++) {
            std::vector<std::pair<uint16, Spec>> sc; std::set<uint16> used; const int n = 1 + r() % 30;
            for (int j = 0; j < n; j++) { uint16 idx; do { idx = r() % MAX_NUM_SCRIPTS; } while (!used.insert(idx).second); sc.push_back({ idx, randomSpec(r, entries, (r() % 5) == 0) }); }
            roundTrip(sc, (t & 1) == 0, "real-random", __LINE__);
        }
        // (3) extremes: IP at the very first and very last byte of ScriptSpace, stack entry at the last byte, mission IP at the last byte of MissionBlock
        {
            std::vector<std::pair<uint16, Spec>> sc; Spec a = randomSpec(r, entries, false); a.ipOff = 0; a.stackOff[0] = (int32)SCRIPT_SPACE_SIZE_ - 1; a.stackOff[1] = MAIN_SCRIPT_SIZE - 1;
            Spec b = randomSpec(r, entries, true); b.ipOff = (int32)SCRIPT_SPACE_SIZE_ - 1; b.baseOff = MAIN_SCRIPT_SIZE; Spec c = randomSpec(r, entries, true); c.ipOff = MAIN_SCRIPT_SIZE; c.baseOff = -1;
            sc.push_back({ 0, a }); sc.push_back({ 95, b }); sc.push_back({ 40, c });
            roundTrip(sc, true, "real-extremes", __LINE__);
        }
        std::printf("real main.scm: %zu scripts at distinct entry offsets + 400 random mixes round-tripped\n", total);
    }
    // ---- source scans: the loops must keep using the typed overloads (no raw sizeof(CRunningScript)) ----------------------------------------------------------------
    {
        auto slurp = [](const std::string& p) { std::ifstream f(p); return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); };
        const std::string ts = slurp(std::string(P2G_STR(REPO_DIR)) + "/source/game_sa/Scripts/TheScripts.cpp");
        CHECK(ts.find("SaveDataToWorkBuffer(*s)") != std::string::npos && ts.find("LoadDataFromWorkBuffer(*script)") != std::string::npos);
        CHECK(ts.find("sizeof(CRunningScript)") == std::string::npos && ts.find("sizeof(*s)") == std::string::npos);
        const char* groups[] = { "Group05_08", "Group13_15", "Group16_17c", "Group20a", "Group24a", "Group24b" };
        for (auto* g : groups) { const auto t = slurp(std::string(P2G_STR(REPO_DIR)) + "/source/game_sa/Scripts/Commands/Ported/" + g + ".cpp"); CHECKM(t.find("_FILE(CRunningScript") == std::string::npos && t.find("VALIDATE_OFFSET(CRunningScript") != std::string::npos, "%s", g); }
        const std::string rs = slurp(std::string(P2G_STR(REPO_DIR)) + "/source/game_sa/Scripts/RunningScript.h");
        CHECK(rs.find("VALIDATE_SIZE_FILE(CRunningScript,") == std::string::npos);
    }
    std::printf(fails ? "\n%d FAILURES\n" : "\nALL OK\n", fails);
    return fails ? 1 : 0;
}
