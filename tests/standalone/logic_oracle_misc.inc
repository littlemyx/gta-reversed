// (e) misc game logic: CPedType / CAcquaintance, CGangs (small), CTheZones (pure parts), CCheat (cheat string -> hash -> ApplyCheat), CKeyGen.
// Needs lw:: (logic_oracle_wanted.inc) for the fake player / pools. Recorders: CHud::SetHelpMessage, the exe's cheat function table (replaced by host recorders in the SHARED memory 0x8A5B58).
#include "TheZones.h"
#include "Cheat.h"
#include "Core/KeyGen.h"
#include "Gangs.h"
#include "GangInfo.h"
#include "Acquaintance.h"
#include "Hud.h"
#include "CutsceneMgr.h"

void CHud::SetHelpMessage(const GxtChar* text, bool quick, bool permanent, bool brief) { g_audioEv.push_back({ 0xBE17, (uint32)(uintptr_t)text, (uint32)quick | (uint32)permanent << 1 | (uint32)brief << 2, 0, 0 }); }
static void __cdecl HostSetHelpMessage(const GxtChar* text, bool quick, bool permanent, bool brief) { g_audioEv.push_back({ 0xBE17, (uint32)(uintptr_t)text, (uint32)quick | (uint32)permanent << 1 | (uint32)brief << 2, 0, 0 }); }

namespace lm {
template<int I> static void __cdecl CheatRec() { g_audioEv.push_back({ 0xC4EA, (uint32)I, 0, 0, 0 }); }
template<int... Is> static void FillCheatTable(std::integer_sequence<int, Is...>, void (**tab)()) { ((tab[Is] = &CheatRec<Is>), ...); }

static const char* kCheatCodes[] = {
    "HESOYAM", "LXGIWYL", "KJKSZPJ", "UZUMYMW", "BAGOWPG", "FULLCLIP", "TURNUPTHEHEAT", "TURNDOWNTHEHEAT", "AIWPRTON", "OHDUDE", "ASNAEB", "JUMPJET", "CVWKXAM", "AFZLLQLL", "YLTEICZ",
    "SPEEDFREAK", "CJPHONEHOME", "JCNRUAD", "BIFBUZZ", "ZEIIVG", "LIFESABEACH", "RZHSUEW", "ICIKPYH", "LEAVEMEALONE", "AIYPWZQP", "MIDNIGHTRIDER", "STATEOFEMERGENCY", "FOOOXFT",
    "ONLYHOMIESALLOWED", "BRINGITON", "NATURALTALENT", "OLDSPEEDDEMON", "ROCKETMAN", "GOODBYECRUELWORLD", "FATTEN", "STINGLIKEABEE", "BTCDBCB", "KVGYZQK", "JYSDSOD", "THELASTSHOWOFTHEYEAR",
    "WANRLTW", "ALNSFMZO", "PGGOMOY", "PPGWJHT", "EEZXMXG", "CPKTNWT", "SJMAHPE", "BUFFMEUP", "KANGAROO", "IWPRTON", "AGBDLCID", "YOUWONTTAKEMEALIVE", "BLUESUEDESHOES", "UZUMYMW",
};
// the cheat string holds the last typed characters, newest first (hash tables contain the hashes of the REVERSED cheat codes)
static void FillString(Rng& r, const char* code, bool junkAfter) {
    std::memset(CCheat::m_CheatString, 0, CCheat::CHEAT_STRING_SIZE);
    const int L = (int)std::strlen(code);
    int p = 0;
    for (int i = L - 2; i >= 0 && p < CCheat::CHEAT_STRING_SIZE - 2; --i) CCheat::m_CheatString[p++] = code[i];   // everything typed before the last char
    if (junkAfter) while (p < CCheat::CHEAT_STRING_SIZE - 2 && r.below(4)) CCheat::m_CheatString[p++] = (char)('A' + r.below(26));
}

struct CheatMem {
    uint8 s[0x80];
    void Save() { std::memcpy(s, (void*)0x969110, sizeof s); }
    bool Same(const CheatMem& o) const { return !std::memcmp(s, o.s, sizeof s); }
    void Restore() const { std::memcpy((void*)0x969110, s, sizeof s); }
};

// zones
static void GenZoneLabel(char (&dst)[8], Rng& r) {
    static const char* names[] = { "SAN_AND", "LA", "SF", "LV", "CHC", "VERO", "BLUEB", "DOWN", "CITYS", "GAN", "EBE", "PLS", "PAL", "SANDY", "BAY" };
    std::memset(dst, 0, 8);
    const char* n = names[r.below(15)];
    std::strncpy(dst, n, 7);
    if (r.below(20) == 0) for (int i = 0; i < 8; ++i) dst[i] = (char)('A' + r.below(26));   // unterminated
}
static void GenZone(CZone& z, Rng& r, bool large = false) {
    std::memset(&z, 0, sizeof z);
    GenZoneLabel(z.m_InfoLabel, r); GenZoneLabel(z.m_TextLabel, r);
    auto c = [&](int16 lo, int16 hi) -> int16 { return (int16)(lo + (int)r.below((uint32)(hi - lo + 1))); };
    const int16 x1 = c(-3000, 2900), y1 = c(-3000, 2900), z1 = c(-100, 600);
    const int16 w = large ? c(500, 5000) : c(5, 800), h = large ? c(500, 5000) : c(5, 800);
    z.m_fX1 = x1; z.m_fY1 = y1; z.m_fZ1 = z1; z.m_fX2 = (int16)std::min<int>(x1 + w, 32000); z.m_fY2 = (int16)std::min<int>(y1 + h, 32000); z.m_fZ2 = (int16)(z1 + c(10, 1400));
    if (r.below(30) == 0) { z.m_fX2 = (int16)r.u32(); z.m_fY2 = (int16)r.u32(); z.m_fZ1 = (int16)r.u32(); }   // raw extremes / inverted
    z.m_ZoneInfoIndex = (int16)r.below(r.below(8) ? 60 : 380);
    z.m_nType = (eZoneType)r.below(4);
    z.m_nLevel = (eLevelName)r.below(4);
}
static void GenZones(Rng& r) {
    CTheZones::TotalNumberOfNavigationZones = (int16)(r.below(4) ? 1 + r.below(60) : r.below(381));
    CTheZones::TotalNumberOfMapZones = (int16)(r.below(4) ? 1 + r.below(12) : 1 + r.below(39));   // >= 1: the port's bounds-checked view asserts on an empty list
    CTheZones::TotalNumberOfZoneInfos = (int16)(r.below(4) ? 1 + r.below(60) : r.below(381));
    for (int i = 0; i < 380; ++i) GenZone(CTheZones::NavigationZoneArray[i], r, i == 0 || r.below(20) == 0);
    for (int i = 0; i < 39; ++i)  GenZone(CTheZones::MapZoneArray[i], r, i == 0 || r.below(4) == 0);
    for (auto& z : CTheZones::NavigationZoneArray) z.m_ZoneInfoIndex = (int16)(z.m_ZoneInfoIndex % std::max<int>(1, CTheZones::TotalNumberOfZoneInfos));   // the port indexes a bounds-checked view
    for (auto& zi : CTheZones::ZoneInfoArray) {
        std::memset(&zi, 0, sizeof zi);
        for (auto& g : zi.GangStrength) g = r.below(3) ? 0 : (uint8)r.u32();
        zi.DealerStrength = (uint8)r.below(16);
        zi.ZoneColor = { (uint8)r.u32(), (uint8)r.u32(), (uint8)r.u32(), (uint8)r.u32() };
        zi.PopType = r.below(20); zi.RadarMode = r.below(2); zi.IsNoCops = r.below(2); zi.PopRaces = r.below(16);
    }
    for (auto& v : *reinterpret_cast<std::array<bool, 100>*>(&CTheZones::ZonesVisited)) v = r.below(2);
    CTheZones::ZonesRevealed = (int32)r.below(101);
    CTheZones::m_CurrLevel = (eLevelName)r.below(4);
}
static CVector ZonePoint(Rng& r) {
    if (r.below(3) == 0 && CTheZones::TotalNumberOfNavigationZones > 0) {   // inside / on the border of a random zone
        const auto& z = CTheZones::NavigationZoneArray[r.below(CTheZones::TotalNumberOfNavigationZones)];
        const auto pick = [&](int a, int b) { return (float)(a + (int)r.below(3) - 1 + (int)(r.f01() * (float)(b - a))); };
        return { pick(z.m_fX1, z.m_fX2), pick(z.m_fY1, z.m_fY2), pick(z.m_fZ1, z.m_fZ2) };
    }
    return r.below(8) ? CVector{ (r.f01() * 2.f - 1.f) * 3400.f, (r.f01() * 2.f - 1.f) * 3400.f, (r.f01() * 2.f - 1.f) * 800.f } : GenV(r, 4000.f);
}
struct ZoneMem {
    std::vector<uint8> b;
    void Save() { b.resize(0xBA6720 - 0xBA1900); std::memcpy(b.data(), (void*)0xBA1900, b.size()); }
    bool Same(const ZoneMem& o) const { return !std::memcmp(b.data(), o.b.data(), b.size()); }
    void Restore() const { std::memcpy((void*)0xBA1900, b.data(), b.size()); }
};
template<class PortFn, class ExeFn>
static bool ZCase(Rng& r, std::string& d, PortFn&& port, ExeFn&& exe, const char* what) {
    lw::Init();
    GenZones(r);
    static ZoneMem m0, m1, m2;
    m0.Save();
    const long long a = port();
    m1.Save(); m0.Restore();
    const long long e = exe();
    m2.Save();
    const bool ok = m1.Same(m2) && a == e;
    if (!ok) d = std::string(what) + " ret got " + std::to_string(a) + " exe " + std::to_string(e) + DiffDesc(m1.b.data(), m2.b.data(), m1.b.size(), "z");
    return ok;
}
} // namespace lm

static void TestMisc() {
    using namespace lm;
    auto sub = [](Rng& r) { return Rng(r.u32() * 2654435761ull + 4242); };
    oracle::Patch(0x588BE0, (void*)&HostSetHelpMessage);
    oracle::Patch(0x823A0E, (void*)+[](int c) -> int { return (c >= 'a' && c <= 'z') ? c - 32 : c; });   // the exe's CRT toupper (locale lock not available here): "C" locale

    // ================================================================ CKeyGen
    Run("CKeyGen::GetKey(str, len) 0x53CED0", [&](Rng& r, std::string& d) {
        char s[40]; const int n = r.below(36); for (int i = 0; i < n; ++i) s[i] = (char)(r.below(4) ? 'A' + r.below(26) : r.u32()); s[n] = 0;
        const int len = (int)r.below(n + 1);
        const uint32 a = CKeyGen::GetKey(s, len), e = oracle::Fn<uint32 __cdecl(const char*, int)>(0x53CED0)(s, len);
        if (a == e) return true; d = "got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    }, 1500);
    Run("CKeyGen::GetKey(str) 0x53CF00", [&](Rng& r, std::string& d) {
        char s[40]; const int n = r.below(36); for (int i = 0; i < n; ++i) s[i] = (char)(r.below(4) ? 'A' + r.below(26) : 1 + r.below(255)); s[n] = 0;
        const uint32 a = CKeyGen::GetKey(s), e = oracle::Fn<uint32 __cdecl(const char*)>(0x53CF00)(s);
        if (a == e) return true; d = "got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    }, 1500);
    Run("CKeyGen::GetUppercaseKey 0x53CF30", [&](Rng& r, std::string& d) {
        char s[40]; const int n = r.below(36); for (int i = 0; i < n; ++i) s[i] = (char)(r.below(4) ? (r.below(2) ? 'a' : 'A') + r.below(26) : 1 + r.below(127)); s[n] = 0;
        const uint32 a = CKeyGen::GetUppercaseKey(s), e = oracle::Fn<uint32 __cdecl(const char*)>(0x53CF30)(s);
        if (a == e) return true; d = "got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    }, 1500);

    // ================================================================ CPedType / CAcquaintance
    static CAcquaintance* acq = (CAcquaintance*)lw::Z(sizeof(CAcquaintance) * (PED_TYPE_COUNT + 1));
    StaticRef<CAcquaintance*>(0xC0BBE8) = acq;
    const auto GenAcq = [&](Rng& r) { for (int i = 0; i < PED_TYPE_COUNT; ++i) for (auto& v : acq[i].m_acquaintances) v = r.below(3) ? r.u32() : (r.below(2) ? 0 : 1u << r.below(32)); };
    static std::vector<uint8> a0(sizeof(CAcquaintance) * PED_TYPE_COUNT), a1(a0.size()), a2(a0.size());
    const auto AcqCase = [&](Rng& r, std::string& d, auto&& port, auto&& exe) {
        GenAcq(r); std::memcpy(a0.data(), acq, a0.size());
        const long long x = port(); std::memcpy(a1.data(), acq, a0.size()); std::memcpy(acq, a0.data(), a0.size());
        const long long y = exe(); std::memcpy(a2.data(), acq, a0.size());
        if (x == y && SameBlob(a1.data(), a2.data(), a0.size())) return true;
        d = "ret got " + std::to_string(x) + " exe " + std::to_string(y) + DiffDesc(a1.data(), a2.data(), a0.size(), "a"); return false;
    };
    Run("CPedType::GetPedFlag 0x608830", [&](Rng& r, std::string& d) {
        const int t = r.below(4) ? (int)r.below(34) : (int)(r.u32() % 200) - 100;
        const uint32 a = CPedType::GetPedFlag((ePedType)t), e = oracle::Fn<uint32 __cdecl(int)>(0x608830)(t);
        if (a == e) return true; d = "type " + std::to_string(t) + " got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    }, 600);
    Run("CPedType::PoliceDontCareAboutCrimesAgainstPedType 0x608A40", [&](Rng& r, std::string& d) {
        const int t = r.below(4) ? (int)r.below(34) : (int)r.u32();
        const bool a = CPedType::PoliceDontCareAboutCrimesAgainstPedType((ePedType)t), e = oracle::Fn<bool __cdecl(int)>(0x608A40)(t) & 1;
        if (a == e) return true; d = "type " + std::to_string(t) + " got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    }, 600);
    Run("CPedType::FindPedType 0x608790", [&](Rng& r, std::string& d) {
        static const char* names[] = { "PLAYER1","PLAYER2","PLAYER_NETWORK","PLAYER_UNUSED","CIVMALE","CIVFEMALE","COP","GANG1","GANG2","GANG3","GANG4","GANG5","GANG6","GANG7","GANG8","GANG9","GANG10","DEALER","MEDIC","FIREMAN","CRIMINAL","BUM","PROSTITUTE","SPECIAL","MISSION1","MISSION2","MISSION3","MISSION4","MISSION5","MISSION6","MISSION7","MISSION8" };
        const char* n = names[r.below(32)];
        const int a = CPedType::FindPedType(n), e = oracle::Fn<int __cdecl(const char*)>(0x608790)(n);
        if (a == e) return true; d = std::string(n) + " got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    }, 300);
    Run("CPedType::GetPedTypeAcquaintances(id,type) 0x6089D0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const AcquaintanceId id = (AcquaintanceId)a.below(5); const ePedType t = (ePedType)a.below(PED_TYPE_COUNT);
        return AcqCase(r, d, [&] { return (long long)CPedType::GetPedTypeAcquaintances(id, t); }, [&] { return (long long)oracle::Fn<uint32 __cdecl(int, int)>(0x6089D0)((int)id, (int)t); });
    }, 600);
    Run("CPedType::GetPedTypeAcquaintances(type) 0x6089B0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const ePedType t = (ePedType)a.below(PED_TYPE_COUNT);
        return AcqCase(r, d, [&] { return (long long)((uintptr_t)&CPedType::GetPedTypeAcquaintances(t) - (uintptr_t)acq); }, [&] { return (long long)((uintptr_t)oracle::Fn<CAcquaintance* __cdecl(int)>(0x6089B0)((int)t) - (uintptr_t)acq); });
    }, 300);
    Run("CPedType::SetPedTypeAsAcquaintance 0x608E20", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const AcquaintanceId id = (AcquaintanceId)a.below(5); const ePedType t = (ePedType)a.below(PED_TYPE_COUNT); const int32 bit = a.below(3) ? (int32)(1u << a.below(32)) : (int32)a.u32();
        return AcqCase(r, d, [&] { CPedType::SetPedTypeAsAcquaintance(id, t, bit); return 0LL; }, [&] { oracle::Fn<void __cdecl(int, int, int)>(0x608E20)((int)id, (int)t, bit); return 0LL; });
    }, 2000);
    Run("CPedType::ClearPedTypeAcquaintances 0x608A20", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const AcquaintanceId id = (AcquaintanceId)a.below(5); const ePedType t = (ePedType)a.below(PED_TYPE_COUNT);
        return AcqCase(r, d, [&] { CPedType::ClearPedTypeAcquaintances(id, t); return 0LL; }, [&] { oracle::Fn<void __cdecl(int, int)>(0x608A20)((int)id, (int)t); return 0LL; });
    }, 600);
    Run("CPedType::ClearPedTypeAsAcquaintance 0x6089F0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const AcquaintanceId id = (AcquaintanceId)a.below(5); const ePedType t = (ePedType)a.below(PED_TYPE_COUNT); const int32 bit = a.below(3) ? (int32)(1u << a.below(32)) : (int32)a.u32();
        return AcqCase(r, d, [&] { CPedType::ClearPedTypeAsAcquaintance(id, t, bit); return 0LL; }, [&] { oracle::Fn<void __cdecl(int, int, int)>(0x6089F0)((int)id, (int)t, bit); return 0LL; });
    }, 2000);
    {
        struct G { const char* n; unsigned va; int kind; };
        static const G gs[] = { { "GetAcquaintances", 0x608970, 0 }, { "SetAcquaintances", 0x608960, 1 }, { "ClearAcquaintances", 0x6089A0, 2 }, { "SetAsAcquaintance", 0x608DA0, 3 }, { "ClearAsAcquaintance", 0x608980, 4 } };
        for (auto& g : gs)
            Run(std::string("CAcquaintance::") + g.n + " 0x" + HexVA(g.va), [&](Rng& r, std::string& d) {
                Rng a = sub(r); const AcquaintanceId id = (AcquaintanceId)a.below(5); const uint32 v = a.below(3) ? (1u << a.below(32)) : a.u32(); const int k = a.below(PED_TYPE_COUNT);
                CAcquaintance* ac = &acq[k];
                return AcqCase(r, d, [&] {
                    switch (g.kind) { case 0: return (long long)ac->GetAcquaintances(id); case 1: ac->SetAcquaintances(id, v); break; case 2: ac->ClearAcquaintances(id); break; case 3: ac->SetAsAcquaintance(id, v); break; default: ac->ClearAsAcquaintance(id, v); }
                    return 0LL; }, [&] {
                    switch (g.kind) { case 0: return (long long)oracle::Fn<uint32 __fastcall(CAcquaintance*, int, int)>(g.va)(ac, 0, (int)id); case 1: oracle::Fn<void __fastcall(CAcquaintance*, int, int, uint32)>(g.va)(ac, 0, (int)id, v); break;
                                      case 2: oracle::Fn<void __fastcall(CAcquaintance*, int, int)>(g.va)(ac, 0, (int)id); break; default: oracle::Fn<void __fastcall(CAcquaintance*, int, int, uint32)>(g.va)(ac, 0, (int)id, v); }
                    return 0LL; });
            }, 1000);
    }

    // ================================================================ CGangs
    {
        static std::vector<uint8> g0(0x100), g1(0x100), g2(0x100);
        const auto GangCase = [&](Rng& r, std::string& d, auto&& port, auto&& exe) {
            for (auto& g : CGangs::Gang) { g.m_nPedModelOverride = (int8)(r.below(3) ? -1 : (int)r.u32()); for (auto& w : g.m_nGangWeapons) w = (eWeaponType)r.below(47); }
            for (auto& b : CGangs::GangAttackWithCops) b = r.below(2);
            std::memcpy(g0.data(), (void*)0xC091E0, 0x100);
            const long long x = port(); std::memcpy(g1.data(), (void*)0xC091E0, 0x100); std::memcpy((void*)0xC091E0, g0.data(), 0x100);
            const long long y = exe(); std::memcpy(g2.data(), (void*)0xC091E0, 0x100);
            if (x == y && SameBlob(g1.data(), g2.data(), 0x100)) return true;
            d = "ret got " + std::to_string(x) + " exe " + std::to_string(y) + DiffDesc(g1.data(), g2.data(), 0x100, "g"); return false;
        };
        Run("CGangs::Initialise 0x5DE680", [&](Rng& r, std::string& d) { return GangCase(r, d, [&] { CGangs::Initialise(); return 0LL; }, [&] { oracle::Fn<void __cdecl()>(0x5DE680)(); return 0LL; }); }, 100);
        Run("CGangs::SetGangWeapons 0x5DE550", [&](Rng& r, std::string& d) {
            Rng a = sub(r); const int16 id = (int16)a.below(10); const eWeaponType w[3] = { (eWeaponType)a.below(47), (eWeaponType)a.below(47), (eWeaponType)a.below(47) };
            return GangCase(r, d, [&] { CGangs::SetGangWeapons(id, w[0], w[1], w[2]); return 0LL; }, [&] { oracle::Fn<void __cdecl(int, int, int, int)>(0x5DE550)(id, (int)w[0], (int)w[1], (int)w[2]); return 0LL; });
        }, 600);
        Run("CGangs::SetGangPedModelOverride 0x5DE580", [&](Rng& r, std::string& d) {
            Rng a = sub(r); const int16 id = (int16)a.below(10); const int8 v = (int8)a.u32();
            return GangCase(r, d, [&] { CGangs::SetGangPedModelOverride(id, v); return 0LL; }, [&] { oracle::Fn<void __cdecl(int, int)>(0x5DE580)(id, v); return 0LL; });
        }, 600);
        Run("CGangs::GetWillAttackPlayerWithCops 0x5DE500", [&](Rng& r, std::string& d) {
            Rng a = sub(r); const ePedType t = (ePedType)(PED_TYPE_GANG1 + a.below(10));
            return GangCase(r, d, [&] { return (long long)CGangs::GetWillAttackPlayerWithCops(t); }, [&] { return (long long)(oracle::Fn<bool __cdecl(int)>(0x5DE500)((int)t) & 1); });
        }, 300);
        Run("CGangs::SetWillAttackPlayerWithCops 0x5DE4E0", [&](Rng& r, std::string& d) {
            Rng a = sub(r); const ePedType t = (ePedType)(PED_TYPE_GANG1 + a.below(10)); const bool b = a.below(2);
            return GangCase(r, d, [&] { CGangs::SetWillAttackPlayerWithCops(t, b); return 0LL; }, [&] { oracle::Fn<void __cdecl(int, bool)>(0x5DE4E0)((int)t, b); return 0LL; });
        }, 300);
    }

    // ================================================================ CTheZones
    Run("CTheZones::PointLiesWithinZone 0x572270", [&](Rng& r, std::string& d) {
        Rng a = sub(r); CZone z; GenZone(z, a); const CVector p = a.below(3) ? CVector{ (float)z.m_fX1 + (float)(int)a.below(3) - 1, (float)z.m_fY2 + (float)(int)a.below(3) - 1, (float)z.m_fZ1 } : GenV(a, 3500.f);
        const bool x = CTheZones::PointLiesWithinZone(&p, &z), y = oracle::Fn<bool __cdecl(const CVector*, CZone*)>(0x572270)(&p, &z) & 1;
        Hit(x); if (x == y) return true; d = "got " + std::to_string(x) + " exe " + std::to_string(y); return false;
    }, 3000);
    Run("CTheZones::ZoneIsEntirelyContainedWithinOtherZone 0x572220", [&](Rng& r, std::string& d) {
        Rng a = sub(r); CZone z1, z2; GenZone(z1, a, a.below(2)); if (a.below(3) == 0) z2 = z1; else GenZone(z2, a, a.below(2)); if (a.below(3) == 0) { z1.m_fX1 = z2.m_fX1; z1.m_fY2 = z2.m_fY2; }
        const bool x = CTheZones::ZoneIsEntirelyContainedWithinOtherZone(&z1, &z2), y = oracle::Fn<bool __cdecl(CZone*, CZone*)>(0x572220)(&z1, &z2) & 1;
        Hit(x); if (x == y) return true; d = "got " + std::to_string(x) + " exe " + std::to_string(y); return false;
    }, 3000);
    Run("CTheZones::Calc2DDistanceBetween2Zones 0x5725B0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); CZone z1, z2; GenZone(z1, a); GenZone(z2, a, a.below(2));
        const float x = CTheZones::Calc2DDistanceBetween2Zones(&z1, &z2), y = oracle::Fn<float __cdecl(CZone*, CZone*)>(0x5725B0)(&z1, &z2);
        if (SameF(x, y)) return true; d = "got " + F(x) + " exe " + F(y); return false;
    }, 3000);
    Run("CTheZones::GetLevelFromPosition 0x572300", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const CVector p = ZonePoint(a);
        return ZCase(r, d, [&] { return (long long)CTheZones::GetLevelFromPosition(p); }, [&] { return (long long)(oracle::Fn<int __cdecl(const CVector*)>(0x572300)(&p) & 0xFF); }, "GetLevelFromPosition");
    }, 3000);
    Run("CTheZones::FindSmallestZoneForPosition 0x572360", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const CVector p = ZonePoint(a); const bool navi = a.below(2);
        return ZCase(r, d, [&] { return (long long)(uintptr_t)CTheZones::FindSmallestZoneForPosition(p, navi); }, [&] { return (long long)(uintptr_t)oracle::Fn<CZone* __cdecl(const CVector*, bool)>(0x572360)(&p, navi); }, "FindSmallestZone");
    }, 4000);
    Run("CTheZones::GetZoneInfo 0x572400", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const CVector p = ZonePoint(a); const bool wantZone = a.below(2);
        CZone *z1 = nullptr, *z2 = nullptr;
        const bool ok = ZCase(r, d, [&] { z1 = nullptr; return (long long)(uintptr_t)CTheZones::GetZoneInfo(p, wantZone ? &z1 : nullptr); },
                              [&] { z2 = nullptr; return (long long)(uintptr_t)oracle::Fn<CZoneInfo* __cdecl(const CVector*, CZone**)>(0x572400)(&p, wantZone ? &z2 : nullptr); }, "GetZoneInfo");
        return ok && z1 == z2;
    }, 3000);
    Run("CTheZones::GetNavigationZone 0x572590", [&](Rng& r, std::string& d) {
        const uint16 i = (uint16)r.below(380);
        return ZCase(r, d, [&] { CTheZones::TotalNumberOfNavigationZones = 380; return (long long)(uintptr_t)CTheZones::GetNavigationZone(i); }, [&] { CTheZones::TotalNumberOfNavigationZones = 380; return (long long)(uintptr_t)oracle::Fn<CZone* __cdecl(uint16)>(0x572590)(i); }, "GetNavigationZone");
    }, 300);
    Run("CTheZones::GetMapZone 0x5725A0", [&](Rng& r, std::string& d) {
        const uint16 i = (uint16)r.below(39);
        return ZCase(r, d, [&] { CTheZones::TotalNumberOfMapZones = 39; return (long long)(uintptr_t)CTheZones::GetMapZone(i); }, [&] { CTheZones::TotalNumberOfMapZones = 39; return (long long)(uintptr_t)oracle::Fn<CZone* __cdecl(uint16)>(0x5725A0)(i); }, "GetMapZone");
    }, 300);
    Run("CTheZones::FindZoneByLabel 0x572C40", [&](Rng& r, std::string& d) {
        Rng a = sub(r); static const char* names[] = { "SAN_AND", "LA", "SF", "LV", "CHC", "VERO", "BLUEB", "DOWN", "CITYS", "GAN", "EBE", "PLS", "PAL", "SANDY", "BAY", "NOPE", "ZZZ" };
        const char* n = names[a.below(17)];
        return ZCase(r, d, [&] { return (long long)CTheZones::FindZoneByLabel(n, ZONE_TYPE_INFO); }, [&] { return (long long)(int16)oracle::Fn<int __cdecl(const char*, int)>(0x572C40)(n, ZONE_TYPE_INFO); }, "FindZoneByLabel");
    }, 1500);
    Run("CTheZones::FindZone 0x572B80", [&](Rng& r, std::string& d) {
        Rng a = sub(r); static const char* names[] = { "SAN_AND", "LA", "SF", "LV", "CHC", "VERO", "BLUEB", "DOWN", "CITYS", "GAN", "EBE", "PLS", "PAL", "SANDY", "BAY" };
        uint64_t packed = 0; std::strncpy((char*)&packed, names[a.below(15)], 7);
        const eZoneType ty = (eZoneType)(a.below(3) == 0 ? a.below(4) : (a.below(2) ? ZONE_TYPE_NAVI : ZONE_TYPE_INFO)); CVector p = ZonePoint(a);
        return ZCase(r, d, [&] { return (long long)CTheZones::FindZone(&p, packed, ty); }, [&] { return (long long)(oracle::Fn<bool __cdecl(CVector*, uint32, uint32, int)>(0x572B80)(&p, (uint32)packed, (uint32)(packed >> 32), ty) & 1); }, "FindZone");
    }, 3000);
    Run("CTheZones::SetZoneRadarColours 0x572CC0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const int16 idx = (int16)a.below(380); const char mode = (char)a.below(3); const uint8 cr = (uint8)a.u32(), cg = (uint8)a.u32(), cb = (uint8)a.u32();
        return ZCase(r, d, [&] { CTheZones::SetZoneRadarColours(idx, mode, cr, cg, cb); return 0LL; }, [&] { oracle::Fn<void __cdecl(int, int, int, int, int)>(0x572CC0)(idx, mode, cr, cg, cb); return 0LL; }, "SetZoneRadarColours");
    }, 600);
    Run("CTheZones::AssignZoneInfoForThisZone 0x572180", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const int16 cnt = (int16)a.below(300); const int16 idx = (int16)(cnt + a.below(3));   // like CreateZone: the new zone is not yet counted (the exe's loop never ends when idx < count)
        return ZCase(r, d, [&] { CTheZones::TotalNumberOfNavigationZones = cnt; CTheZones::AssignZoneInfoForThisZone(idx); return 0LL; }, [&] { CTheZones::TotalNumberOfNavigationZones = cnt; oracle::Fn<void __cdecl(int)>(0x572180)(idx); return 0LL; }, "AssignZoneInfoForThisZone");
    }, 600);
    Run("CTheZones::ResetZonesRevealed 0x572110", [&](Rng& r, std::string& d) {
        return ZCase(r, d, [&] { CTheZones::ResetZonesRevealed(); return 0LL; }, [&] { oracle::Fn<void __cdecl()>(0x572110)(); return 0LL; }, "ResetZonesRevealed");
    }, 100);
    Run("CTheZones::InitZonesPopulationSettings 0x5720D0", [&](Rng& r, std::string& d) {
        return ZCase(r, d, [&] { CTheZones::InitZonesPopulationSettings(); return 0LL; }, [&] { oracle::Fn<void __cdecl()>(0x5720D0)(); return 0LL; }, "InitZonesPopulationSettings");
    }, 100);
    Run("CTheZones::CreateZone 0x5728A0", [&](Rng& r, std::string& d) {
        Rng a = sub(r); char info[12], text[12]; const int n1 = 1 + a.below(7), n2 = 1 + a.below(7);
        for (int i = 0; i < n1; ++i) info[i] = (char)(a.below(2) ? 'A' + a.below(26) : 'a' + a.below(26)); info[n1] = 0;
        for (int i = 0; i < n2; ++i) text[i] = (char)(a.below(2) ? 'A' + a.below(26) : 'a' + a.below(26)); text[n2] = 0;
        const eZoneType ty = (eZoneType)(a.below(2) ? (a.below(2) ? ZONE_TYPE_NAVI : ZONE_TYPE_MAP) : ZONE_TYPE_LOCAL_NAVI); const CVector p1 = GenV(a, 4000.f, S_NOHUGE), p2 = GenV(a, 4000.f, S_NOHUGE);
        const eLevelName lv = (eLevelName)a.below(4);
        const int nNavi = a.below(300), nMap = a.below(30);
        char info2[12], text2[12]; std::strcpy(info2, info); std::strcpy(text2, text);
        return ZCase(r, d, [&] { CTheZones::TotalNumberOfNavigationZones = (int16)nNavi; CTheZones::TotalNumberOfMapZones = (int16)nMap; CTheZones::CreateZone(info, ty, p1, p2, lv, text); return 0LL; },
                     [&] { CTheZones::TotalNumberOfNavigationZones = (int16)nNavi; CTheZones::TotalNumberOfMapZones = (int16)nMap; oracle::Fn<void __cdecl(const char*, int, float, float, float, float, float, float, int, const char*)>(0x5728A0)(info2, ty, p1.x, p1.y, p1.z, p2.x, p2.y, p2.z, lv, text2); return 0LL; }, "CreateZone");
    }, 600);
    Run("CTheZones::FillZonesWithGangColours 0x572440", [&](Rng& r, std::string& d) {
        Rng a = sub(r); const bool dis = a.below(2);
        return ZCase(r, d, [&] { CTheZones::FillZonesWithGangColours(dis); return 0LL; }, [&] { oracle::Fn<void __cdecl(bool)>(0x572440)(dis); return 0LL; }, "FillZonesWithGangColours");
    }, 600);
    Run("CTheZones::SetCurrentZoneAsUnlocked 0x572800", [&](Rng& r, std::string& d) {
        Rng a = sub(r); lw::Init();
        const CVector p = a.below(8) ? CVector{ (a.f01() * 2.f - 1.f) * 3100.f, (a.f01() * 2.f - 1.f) * 3100.f, 10.f } : GenV(a, 3300.f, S_NOHUGE);
        lw::Pl->m_placement.m_vPosn = p; lw::Pl->bInVehicle = false;
        const bool off = a.below(8) == 0; const int area = a.below(6) == 0 ? (int)a.below(5) : 0;
        return ZCase(r, d, [&] { CTheScripts::bPlayerIsOffTheMap = off; CGame::currArea = (eAreaCodes)area; lw::Pl->m_placement.m_vPosn = p; CTheZones::SetCurrentZoneAsUnlocked(); return 0LL; },
                     [&] { CTheScripts::bPlayerIsOffTheMap = off; CGame::currArea = (eAreaCodes)area; lw::Pl->m_placement.m_vPosn = p; oracle::Fn<void __cdecl()>(0x572800)(); return 0LL; }, "SetCurrentZoneAsUnlocked");
    }, 3000);

    // ================================================================ CCheat
    static CheatMem cm0, cm1, cm2;
    {
        static void (*saved[TOTAL_CHEATS])();
        FillCheatTable(std::make_integer_sequence<int, TOTAL_CHEATS>{}, saved);
        // the exe's table (shared memory): recorders, some entries null (=> Toggle)
        const auto RandomTable = [&](Rng& r) { for (int i = 0; i < TOTAL_CHEATS; ++i) CCheat::m_aCheatFunctions[i] = r.below(5) == 0 ? nullptr : saved[i]; };
        const auto CheatCase = [&](Rng& r, std::string& d, auto&& setup, auto&& port, auto&& exe, const char* what) {
            lw::Init();
            RandomTable(r);
            for (auto& b : CCheat::m_aCheatsActive) b = r.below(4) == 0;
            CCheat::m_bHasPlayerCheated = r.below(2);
            CCutsceneMgr::ms_running = r.below(10) == 0;
            CStats::bShowUpdateStats = false; CStats::TotalNumStatMessages = 0;
            for (auto& v : CStats::StatTypesInt) v = (int32)r.below(100);
            setup();
            static uint8 s0[0x180], s1[0x180], s2[0x180];
            cm0.Save(); std::memcpy(s0, (void*)0xB79000, sizeof s0);
            g_audioEv.clear(); const long long x = port(); const auto e1 = g_audioEv; cm1.Save(); std::memcpy(s1, (void*)0xB79000, sizeof s1);
            cm0.Restore(); std::memcpy((void*)0xB79000, s0, sizeof s0);
            g_audioEv.clear(); const long long y = exe(); const auto e2 = g_audioEv; cm2.Save(); std::memcpy(s2, (void*)0xB79000, sizeof s2);
            const bool ok = x == y && cm1.Same(cm2) && e1 == e2 && SameBlob(s1, s2, sizeof s1);
            Hit(!e1.empty());
            if (!ok) d = std::string(what) + " ret got " + std::to_string(x) + " exe " + std::to_string(y) + " events " + std::to_string(e1.size()) + "/" + std::to_string(e2.size()) + DiffDesc(cm1.s, cm2.s, sizeof cm1.s, "c") + DiffDesc(s1, s2, sizeof s1, "st");
            return ok;
        };
        Run("CCheat::AddToCheatString 0x438480", [&](Rng& r, std::string& d) {
            Rng a = sub(r); char key;
            const bool real = a.below(2);
            const char* code = kCheatCodes[a.below(sizeof kCheatCodes / sizeof *kCheatCodes)];
            if (real) key = code[std::strlen(code) - 1]; else key = (char)(a.below(4) ? 'A' + a.below(26) : a.u32());
            return CheatCase(r, d, [&] { if (real) FillString(a, code, true); else { std::memset(CCheat::m_CheatString, 0, 30); const int n = a.below(29); for (int i = 0; i < n; ++i) CCheat::m_CheatString[i] = (char)('A' + a.below(26)); } },
                [&] { CCheat::AddToCheatString(key); return (long long)CCheat::m_bHasPlayerCheated; }, [&] { oracle::Fn<void __cdecl(char)>(0x438480)(key); return (long long)CCheat::m_bHasPlayerCheated; }, "AddToCheatString");
        }, 6000);
        Run("CCheat::ApplyCheat 0x438370", [&](Rng& r, std::string& d) {
            Rng a = sub(r); const eCheats c = (eCheats)a.below(TOTAL_CHEATS);
            return CheatCase(r, d, [] {}, [&] { CCheat::ApplyCheat(c); return 0LL; }, [&] { oracle::Fn<void __cdecl(int)>(0x438370)((int)c); return 0LL; }, "ApplyCheat");
        }, 1500);
        Run("CCheat::IsZoneStreamingAllowed 0x407410", [&](Rng& r, std::string& d) {
            return CheatCase(r, d, [] {}, [&] { return (long long)CCheat::IsZoneStreamingAllowed(); }, [&] { return (long long)(oracle::Fn<bool __cdecl()>(0x407410)() & 1); }, "IsZoneStreamingAllowed");
        }, 1500);
        Run("CCheat::HandleSpecialCheats 0x439A10", [&](Rng& r, std::string& d) {
            Rng a = sub(r); const eCheats c = (eCheats)a.below(TOTAL_CHEATS);
            return CheatCase(r, d, [&] { CCheat::m_aCheatsActive[CHEAT_BEACH_PARTY] = false; CCheat::m_aCheatsActive[CHEAT_FUNHOUSE_THEME] = false; CCheat::m_aCheatsActive[CHEAT_COUNTRY_TRAFFIC] = false; },
                             [&] { CCheat::HandleSpecialCheats(c); return 0LL; }, [&] { oracle::Fn<void __cdecl(int)>(0x439A10)((int)c); return 0LL; }, "HandleSpecialCheats");
        }, 1500);
        Run("CCheat::ResetCheats 0x438450", [&](Rng& r, std::string& d) {
            return CheatCase(r, d, [&] { std::memset(CCheat::m_CheatString, 'A', 29); CTimer::ms_fTimeScale = r.f01() * 3.f; }, [&] { CCheat::ResetCheats(); return (long long)(CTimer::ms_fTimeScale != 1.0f); }, [&] { oracle::Fn<void __cdecl()>(0x438450)(); return (long long)(CTimer::ms_fTimeScale != 1.0f); }, "ResetCheats");
        }, 300);
        Run("CCheat::DoCheats 0x439AF0", [&](Rng& r, std::string& d) {
            Rng a = sub(r); const char* code = kCheatCodes[a.below(sizeof kCheatCodes / sizeof *kCheatCodes)]; const char last = code[std::strlen(code) - 1];
            return CheatCase(r, d, [&] {
                    auto* pad = CPad::GetPad(0); std::memset(&pad->NewKeyState, 0, sizeof pad->NewKeyState); std::memset(&pad->OldKeyState, 0, sizeof pad->OldKeyState);
                    FillString(a, code, true);
                    if (a.below(2)) { pad->NewKeyState.standardKeys[(uint8)last] = 1; } else { for (int i = 0; i < 4; ++i) pad->NewKeyState.standardKeys[a.below(256)] = 1; }
                    for (int i = 0; i < 3; ++i) pad->OldKeyState.standardKeys[a.below(256)] = 1;
                }, [&] { CCheat::DoCheats(); return 0LL; }, [&] { oracle::Fn<void __cdecl()>(0x439AF0)(); return 0LL; }, "DoCheats");
        }, 3000);
    }
}
