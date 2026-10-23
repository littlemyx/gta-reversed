// ORACLE-W3-MISC: exe-oracle rows for the A + B functions of game_sa/Localisation.cpp (region rules: nasty / German / French game, metric units ...) and game_sa/TagManager.cpp (gang graffiti tags:
// tag array at 0xA9A8C0, tag counts, alpha in the atomic's visibility-plugin user value, area queries). Included by wave2_oracle_all.inc when W3_MISC_ONLY is defined (executable
// wave3_misc_oracle_test = the W2 harness: deterministic arena, the whole exe data image + the fixture block compared after every case).
// Fixtures: the tag array (up to 150 entries; entities are raw CEntity blobs with position in the placement or in a matrix, an RpAtomic blob or none, model index 0 = a valid tag model info),
// the exe's ms_modelInfoPtrs entries (fake model infos whose vtable slots GetRwModelType / AsAtomicModelInfoPtr are host stubs, same on both sides), the CVisibilityPlugins plugin offsets, the
// camera wide-screen flag, FrontEndMenuManager's language byte and the three localisation globals. CGarages::TriggerMessage (0x447B80) is patched to a recording stub (the port's symbol of
// the same name is defined in this TU with the same body): what is compared is the key string (hash), the arguments and the number of calls.
#include "Localisation.h"
#include "TagManager.h"
#include "Garages.h"
#include "VisibilityPlugins.h"
#include "MenuManager.h"

// the three private statics of CTagManager and its private overloads: explicit-instantiation access trick (no #define private public)
#define TAG_STEAL(NAME, SIG, MEMBER) \
    struct T_##NAME { using type = SIG; friend type tag_steal(T_##NAME); }; \
    template<class Tag, typename Tag::type P> struct R_##NAME { friend typename Tag::type tag_steal(Tag) { return P; } }; \
    template struct R_##NAME<T_##NAME, MEMBER>;
TAG_STEAL(FindTagDesc, tTagDesc* (*)(const CEntity&), &CTagManager::FindTagDesc)
TAG_STEAL(SetAlphaAtomic, void (*)(RpAtomic&, uint8), &CTagManager::SetAlpha)
TAG_STEAL(GetAlphaAtomic, uint8 (*)(const RpAtomic&), &CTagManager::GetAlpha)
#define TSTL(NAME) tag_steal(T_##NAME{})

static_assert(offsetof(CMenuManager, m_nPrefsLanguage) == 0xBA67CC - 0xBA6748, "CLocalisation::Metric tests the byte 0xBA67CC");
static_assert(sizeof(tTagDesc) == 8 && sizeof(CRect) == 16, "tag array stride 8 / CRect floats at +0..+0xC");

namespace w3m {
using namespace w2;
constexpr size_t kScSize = 0x2000;
alignas(16) static uint8_t g_sc[kScSize];
constexpr int kNEnt = 12;
constexpr size_t kEnt = 0x000 /*12 x 0x40*/, kMat = 0x300 /*12 x 0x50*/, kAt = 0x800 /*12 x 0x100*/, kMi = 0x1400 /*4 x 0x60*/, kVt = 0x1600 /*0x80*/, kRec = 0x1700 /*0x100*/, kRect = 0x1800, kVec = 0x1820;
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static uint64_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
template<class R, class... A> static R ExeC(uint32_t addr, A... a) { return oracle::Fn<R __cdecl(A...)>(addr)(a...); }
static float Fe(Rng& r, float scale, int special = 10) {
    if (special && r.below((uint32_t)special) == 0) return GenF(r, scale, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF | S_HUGE);
    return GenF(r, scale, S_ZERO);
}
static CEntity* Ent(int i) { return reinterpret_cast<CEntity*>(S(kEnt + 0x40 * i)); }
static RpAtomic* Atm(int i) { return reinterpret_cast<RpAtomic*>(S(kAt + 0x100 * i)); }
static const uintptr_t kTagArr = 0xA9A8C0;
static tTagDesc* TagAt(int i) { return reinterpret_cast<tTagDesc*>(kTagArr + 8 * i); }
static int32_t& NumTags() { return StaticRef<int32_t>(0xA9AD70); }
static int32_t& NumTagged() { return StaticRef<int32_t>(0xA9AD74); }

// ---- recorded CGarages::TriggerMessage calls (the exe's and the port's are the same stub) ----
static void Rec(const char* key, int a, int b, int c) {
    uint32_t& n = SAt<uint32_t>(kRec);
    uint32_t h = 0; for (const char* p = key; *p; ++p) h = h * 31 + (uint8_t)*p;
    if (n < 8) { uint32_t* p = &SAt<uint32_t>(kRec + 4 + n * 16); p[0] = h; p[1] = (uint32_t)a; p[2] = (uint32_t)b; p[3] = (uint32_t)c; }
    ++n;
}
static void __cdecl ExeTrigger(const char* key, int msgMin, int time, int msgMax) { Rec(key, (int16_t)msgMin, (uint16_t)time, (int16_t)msgMax); }
static void InstallStubs() {
    static bool done = false; if (done) return; done = true;
    oracle::Patch(0x447B80, (void*)&ExeTrigger);
}

// ---- fake model infos: vtable slot 1 (AsAtomicModelInfoPtr) returns this, slot 9 (GetRwModelType) returns the dword at +0x40 ----
static void* __fastcall FakeAsAtomic(void* self, void*) { return self; }
static uint32_t __fastcall FakeRwType(void* self, void*) { return *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(self) + 0x40); }

static void BuildAll(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    SAt<uint32_t>(kRec) = 0;
    StaticRef<int>(0x8D608C) = 0x40 + 4 * (int)r.below(16);
    StaticRef<int>(0x8D6090) = 0x40 + 4 * (int)r.below(16);
    StaticRef<int>(0x8D6094) = 0x40 + 4 * (int)r.below(16);
    // vtable of the fake model infos
    for (int i = 0; i < 0x80 / 4; ++i) SAt<uint32_t>(kVt + 4 * i) = 0;
    SAt<uint32_t>(kVt + 4 * 1) = (uint32_t)(uintptr_t)&FakeAsAtomic;
    SAt<uint32_t>(kVt + 4 * 9) = (uint32_t)(uintptr_t)&FakeRwType;
    // model info 0 = a valid tag (atomic type, special type 6 = 0x3000, not disabled); 1..3: random flags / rw types (for IsTag)
    for (int m = 0; m < 4; ++m) {
        uint8_t* mi = S(kMi + 0x60 * m);
        SAt<uint32_t>(kMi + 0x60 * m) = (uint32_t)(uintptr_t)S(kVt);
        uint16_t fl = (uint16_t)r.u32(); uint32_t rwt = r.below(4);
        if (m == 0) { fl = (uint16_t)((fl & ~0xF800) | 0x3000); rwt = 1; }
        else if (r.below(2)) { fl = (uint16_t)((fl & ~0x7800) | 0x3000); rwt = r.below(4) ? 1 : (uint32_t)r.below(3); }
        std::memcpy(mi + 0x12, &fl, 2); std::memcpy(mi + 0x40, &rwt, 4);
        StaticRef<uint32_t>(0xA9B0C8 + 4 * m) = (uint32_t)(uintptr_t)mi;
    }
    // entities / matrices / atomics
    for (int i = 0; i < kNEnt; ++i) {
        uint8_t* e = S(kEnt + 0x40 * i);
        const bool useMat = r.below(2);
        float p[3]; for (int k = 0; k < 3; ++k) p[k] = Fe(r, 3000.f, 14);
        std::memcpy(e + 4, p, 12);                                                                  // placement
        std::memcpy(S(kMat + 0x50 * i) + 0x30, p, 12);                                              // matrix position (the same: only the pointer decides)
        if (r.below(3) == 0) { for (int k = 0; k < 3; ++k) { const float q = Fe(r, 3000.f, 14); std::memcpy(S(kMat + 0x50 * i) + 0x30 + 4 * k, &q, 4); } }
        SAt<uint32_t>(kEnt + 0x40 * i + 0x14) = useMat ? (uint32_t)(uintptr_t)S(kMat + 0x50 * i) : 0;
        const bool hasAtomic = r.below(2);
        SAt<uint32_t>(kEnt + 0x40 * i + 0x18) = hasAtomic ? (uint32_t)(uintptr_t)Atm(i) : 0;
        if (hasAtomic) *S(kAt + 0x100 * i) = 1;                                                     // RwObject::type = rpATOMIC
        SAt<uint16_t>(kEnt + 0x40 * i + 0x22) = 0;                                                  // model index 0: the valid tag model info
    }
    StaticRef<bool>(0xB6F065) = r.below(3) == 0;                                                    // TheCamera.m_bWideScreenOn
}
static const uint8_t kAlphaPick[] = { 0, 1, 100, 226, 227, 228, 229, 230, 254, 255 };
static uint8_t RAlpha(Rng& r) { return r.below(3) ? kAlphaPick[r.below(sizeof kAlphaPick)] : (uint8_t)r.u32(); }
// the tag array: n entries (entity pointers to the fixture entities: distinct, or with repeats when `dups`), all 150 slots hold something valid; the tag counters
static void BuildTags(Rng& r, int n, bool dups, int tagged = -1) {
    int perm[kNEnt]; for (int i = 0; i < kNEnt; ++i) perm[i] = i;
    for (int i = kNEnt - 1; i > 0; --i) std::swap(perm[i], perm[r.below(i + 1)]);
    for (int i = 0; i < (int)CTagManager::MAX_TAGS; ++i) {
        const int e = (dups || i >= kNEnt) ? (int)r.below(kNEnt) : perm[i];
        TagAt(i)->Entity = Ent(e); TagAt(i)->Alpha = RAlpha(r);
    }
    NumTags() = n;
    NumTagged() = tagged >= 0 ? tagged : (int)r.below(std::max(1, n) + 2);
}
static int g_n = 0; static bool g_dups = false; static int g_tg = -1;
static void BuildDefault(Rng& r) { BuildAll(r); BuildTags(r, g_n, g_dups, g_tg); }

static std::vector<std::pair<size_t, size_t>> g_unc;
template<class PF, class EF>
static bool VCase(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&) = &BuildDefault) {
    const uint32_t seed = r.u32() | 1;
    build(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    Reg img{ kImgLo, kImgHi - kImgLo };
    return BothA(d, seed, { sc, img }, std::forward<PF>(pf), std::forward<EF>(ef));
}
} // namespace w3m

void CGarages::TriggerMessage(const char* tagMsg, int16 msgMin, uint16 time, int16 msgMax) { w3m::Rec(tagMsg, msgMin, time, msgMax); }
CMenuManager& FrontEndMenuManager = StaticRef<CMenuManager>(0xBA6748);

#define MROW(NAME, ADDR) Run(NAME " " #ADDR, [&](Rng& r, std::string& d) -> bool {
#define MEND(CASES) }, std::max(10, (CASES) / w3m_div()));
static int w3m_div() { static const int v = std::getenv("W3M_DIV") ? std::max(1, std::atoi(std::getenv("W3M_DIV"))) : 1; return v; }

static void TestW3MiscLocalisation() {
    using namespace w2; using namespace w3m;
    // the three localisation flags are bools (0 / 1: what the game stores); the language byte is anything the exe can read (0..6 the menu values, 7..255 garbage from a damaged gta_sa.set)
    static const auto Fix = [](Rng& r) {
        BuildAll(r);
        StaticRef<uint8_t>(0xB9B7EC) = (uint8_t)r.below(2); StaticRef<uint8_t>(0xB9B7ED) = (uint8_t)r.below(2); StaticRef<uint8_t>(0xB9B7EE) = (uint8_t)r.below(2);
        StaticRef<uint8_t>(0xBA67CC) = r.below(3) ? (uint8_t)r.below(7) : (uint8_t)r.u32();
    };
#define LOC(NAME, ADDR, CALL) \
    MROW("CLocalisation::" NAME, ADDR) \
        return VCase(r, d, [&]() -> uint64_t { return (uint8_t)CLocalisation::CALL(); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t>(ADDR); }, +Fix); \
    MEND(60)
#define LOCV(NAME, ADDR, CALL) \
    MROW("CLocalisation::" NAME, ADDR) \
        return VCase(r, d, [&]() -> uint64_t { CLocalisation::CALL(); return 0; }, [&]() -> uint64_t { ExeC<void>(ADDR); return 0; }, +Fix); \
    MEND(60)
    LOCV("Initialise", 0x56D180, Initialise)
    LOCV("SetNormalGame", 0x56D1A0, SetNormalGame)
    LOCV("SetFrenchGame", 0x56D1C0, SetFrenchGame)
    LOCV("SetGermanGame", 0x56D1E0, SetGermanGame)
    LOC("GermanGame", 0x56D200, GermanGame)
    LOC("FrenchGame", 0x56D210, FrenchGame)
    LOC("Metric", 0x56D220, Metric)
    LOC("Blood", 0x56D230, Blood)
    LOC("Porn", 0x56D240, Porn)
    LOC("ScreamsFromKills", 0x56D250, ScreamsFromKills)
    LOC("Prostitutes", 0x56D260, Prostitutes)
    LOC("KickingWhenDown", 0x56D270, KickingWhenDown)
    LOC("ShootLimbs", 0x56D280, ShootLimbs)
    LOC("KnockDownPeds", 0x56D290, KnockDownPeds)
    LOC("KillFrenzy", 0x56D2A0, KillFrenzy)
    LOC("StealFromDeadPed", 0x56D2B0, StealFromDeadPed)
    LOC("KillPeds", 0x56D2C0, KillPeds)
    LOC("PedsOnFire", 0x56D2D0, PedsOnFire)
#undef LOC
#undef LOCV
}

static void TestW3MiscTags() {
    using namespace w2; using namespace w3m;
    using CTM = CTagManager;
    const auto Setup = [&](Rng& r, bool allowDup = true, int maxN = 13) { g_n = (int)r.below(maxN + 1); g_dups = allowDup && r.below(5) == 0; g_tg = r.below(3) ? -1 : (int)r.below(g_n + 3); };
    MROW("CTagManager::Init", 0x49CC50)
        Setup(r);
        return VCase(r, d, [&]() -> uint64_t { CTM::Init(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x49CC50); return 0; });
    MEND(60)
    MROW("CTagManager::ShutdownForRestart", 0x49CC60)
        Setup(r);
        if (r.below(8) == 0) g_n = (int)(CTM::MAX_TAGS - r.below(2));
        return VCase(r, d, [&]() -> uint64_t { CTM::ShutdownForRestart(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x49CC60); return 0; });
    MEND(150)
    MROW("CTagManager::GetTagPos", 0x49CEA0)
        Setup(r); if (g_n == 0) g_n = 1 + (int)r.below(12);
        const int32_t idx = (int32_t)r.below(g_n);
        return VCase(r, d, [&]() -> uint64_t { return U(&CTM::GetTagPos(idx)); }, [&]() -> uint64_t { return U(ExeC<const void*, int32_t>(0x49CEA0, idx)); });
    MEND(300)
    MROW("CTagManager::AddTag", 0x49CC90)
        Setup(r, true, 13); if (r.below(10) == 0) g_n = (int)CTM::MAX_TAGS - 1;
        const int e = (int)r.below(kNEnt);
        return VCase(r, d, [&]() -> uint64_t { CTM::AddTag(*Ent(e)); return 0; }, [&]() -> uint64_t { ExeC<void, CEntity*>(0x49CC90, Ent(e)); return 0; });
    MEND(300)
    MROW("CTagManager::FindTagDesc", 0x49CCB0)
        Setup(r);
        const int e = (int)r.below(kNEnt);
        return VCase(r, d, [&]() -> uint64_t { return U(TSTL(FindTagDesc)(*Ent(e))); }, [&]() -> uint64_t { return U(ExeC<void*, CEntity*>(0x49CCB0, Ent(e))); });
    MEND(1500)
    MROW("CTagManager::IsTag", 0x49CCE0)
        Setup(r);
        const uint16_t mi = (uint16_t)r.below(4);
        return VCase(r, d, [&]() -> uint64_t { return (uint8_t)CTM::IsTag(*Ent(0)); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, CEntity*>(0x49CCE0, Ent(0)); },
            [](Rng& rr) { BuildDefault(rr); SAt<uint16_t>(kEnt + 0x22) = (uint16_t)rr.below(4); });
    MEND(400)
    MROW("CTagManager::SetAlpha(RpAtomic)", 0x49CD30)
        Setup(r);
        const int a = (int)r.below(kNEnt); const uint32_t al = r.below(4) ? (uint32_t)RAlpha(r) | (r.u32() & 0xFFFFFF00u) : r.u32();
        return VCase(r, d, [&]() -> uint64_t { TSTL(SetAlphaAtomic)(*Atm(a), (uint8)al); return 0; }, [&]() -> uint64_t { ExeC<void, RpAtomic*, uint32_t>(0x49CD30, Atm(a), al); return 0; });
    MEND(300)
    MROW("CTagManager::GetAlpha(RpAtomic)", 0x49CD40)
        Setup(r);
        const int a = (int)r.below(kNEnt);
        return VCase(r, d, [&]() -> uint64_t { return TSTL(GetAlphaAtomic)(*Atm(a)); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, RpAtomic*>(0x49CD40, Atm(a)); });
    MEND(300)
    MROW("CTagManager::GetAlpha(CEntity)", 0x49CF90)
        Setup(r); if (g_n == 0) g_n = 1 + (int)r.below(12);
        const int e = (int)r.below(g_n);   // a tag of the array: the exe dereferences address 4 when an atomic-less entity is not in the array
        return VCase(r, d, [&]() -> uint64_t { return CTM::GetAlpha(*TagAt(e)->Entity); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t, CEntity*>(0x49CF90, TagAt(e)->Entity); });
    MEND(1500)
    MROW("CTagManager::SetAlpha(CEntity)", 0x49CEC0)
        Setup(r); if (g_n == 0) g_n = 1 + (int)r.below(12);
        const int e = (int)r.below(g_n); const uint32_t al = r.below(4) ? (uint32_t)RAlpha(r) | (r.u32() & 0xFFFFFF00u) : r.u32();
        g_tg = r.below(4) ? -1 : g_n;
        return VCase(r, d, [&]() -> uint64_t { CTM::SetAlpha(*TagAt(e)->Entity, (uint8)al); return 0; }, [&]() -> uint64_t { ExeC<void, CEntity*, uint32_t>(0x49CEC0, TagAt(e)->Entity, al); return 0; });
    MEND(2500)
    MROW("CTagManager::GetPercentageTagged", 0x49CDA0)
        Setup(r);
        const uint32_t k = r.below(4);
        g_n = k == 0 ? 0 : k == 1 ? (int)r.below(10) : (int)r.below(151); g_tg = (int)r.below(k == 0 ? 3 : g_n + 3);
        if (r.below(30) == 0) g_tg = (int)r.below(1000) + 150;
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)CTM::GetPercentageTagged(); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x49CDA0); });
    MEND(2000)
    MROW("CTagManager::UpdateNumTagged", 0x49CDE0)
        Setup(r);
        return VCase(r, d, [&]() -> uint64_t { CTM::UpdateNumTagged(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x49CDE0); return 0; });
    MEND(500)
    // area queries: the rect around the origin of the entity positions
    static const auto BuildRect = [](Rng& rr) {
        BuildDefault(rr);
        float c[4];
        const float x0 = Fe(rr, 2500.f, 20), y0 = Fe(rr, 2500.f, 20), w = std::fabs(Fe(rr, 2500.f, 20)), h = std::fabs(Fe(rr, 2500.f, 20));
        c[0] = x0; c[2] = x0 + w; c[3] = y0; c[1] = y0 + h;     // left, top(+4 = max y), right, bottom(+0xC = min y)
        if (rr.below(25) == 0) c[rr.below(4)] = Fe(rr, 100.f, 1);
        std::memcpy(S(kRect), c, 16);
        // some entities exactly on the rect borders
        for (int i = 0; i < kNEnt; ++i) if (rr.below(6) == 0) {
            float p[2] = { rr.below(2) ? c[0] : c[2], rr.below(2) ? c[3] : c[1] };
            if (rr.below(2)) p[1] = Fe(rr, 100.f, 1) * 0 + (c[3] + c[1]) * 0.5f;
            std::memcpy(S(kEnt + 0x40 * i) + 4, p, 8); std::memcpy(S(kMat + 0x50 * i) + 0x30, p, 8);
        }
    };
    MROW("CTagManager::GetPercentageTaggedInArea", 0x49D0B0)
        Setup(r); if (r.below(3)) g_n = 4 + (int)r.below(9);
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)CTM::GetPercentageTaggedInArea(SAt<CRect>(kRect)); }, [&]() -> uint64_t { return ExeC<uint32_t, const void*>(0x49D0B0, S(kRect)); }, +BuildRect);
    MEND(3000)
    MROW("CTagManager::SetAlphaInArea", 0x49CFE0)
        Setup(r); if (r.below(3)) g_n = 4 + (int)r.below(9);
        const uint32_t al = r.below(4) ? (uint32_t)RAlpha(r) | (r.u32() & 0xFFFFFF00u) : r.u32();
        return VCase(r, d, [&]() -> uint64_t { CTM::SetAlphaInArea(SAt<CRect>(kRect), (uint8)al); return 0; }, [&]() -> uint64_t { ExeC<void, const void*, uint32_t>(0x49CFE0, S(kRect), al); return 0; }, +BuildRect);
    MEND(3000)
}

#include "wave3_oracle_misc2.inc"

static void TestW3MiscAll() {
    using namespace w3m;
    (void)spdlog::default_logger_raw();
    spdlog::set_level(spdlog::level::off);   // FindTagDesc logs a warning for an unknown entity (a formatted message allocates from the arena on the port side only)
    InstallStubs();
    TestW3MiscLocalisation();
    TestW3MiscTags();
    TestW3MiscPlatform();
    TestW3MiscCustomBuilding();
    for (int m = 0; m < 4; ++m) StaticRef<uint32_t>(0xA9B0C8 + 4 * m) = 0;   // the fake model infos must not outlive the rows (static destructors at exit)
}
