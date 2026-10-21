// per-class fixtures and hand rows of the gameplay wave. Included by wave3_oracle_gp.inc AFTER the generated rows (it defines w3g::GpFix, forward-declared there, and may use the W2Off_* helpers of the generated file).
#include "Collision/ColModel.h"
#include "Collision/CollisionData.h"
#include "PedGroup.h"
#include "PedGroupMembership.h"
#include "PedIntelligence.h"
#include "Models/ModelInfo.h"

struct GpMembers_T { using type = std::array<CPed*, TOTAL_PED_GROUP_MEMBERS> CPedGroupMembership::*; friend type W2Get(GpMembers_T); }; template struct W2Rob<GpMembers_T, &CPedGroupMembership::m_members>;
struct GpAdd_T { using type = void (CPedGroupMembership::*)(CPed*, int32); friend type W2Get(GpAdd_T); }; template struct W2Rob<GpAdd_T, &CPedGroupMembership::AddMember>;
struct GpIntel_T { using type = CPedIntelligence* CPed::*; friend type W2Get(GpIntel_T); }; template struct W2Rob<GpIntel_T, &CPed::m_pIntelligence>;

namespace w3g {
static float Fl(Rng& r, float lo, float hi) { return lo + (hi - lo) * r.f01(); }
static auto& Members(CPedGroupMembership& m) { return m.*W2Get(GpMembers_T{}); }
static size_t GpOffM(CPedIntelligence* CPed::* pm) { uint32_t o = 0; std::memcpy(&o, &pm, 4); return o; }

// every reference-world entity gets a matrix, and the same model (model info + collision model with a few spheres): garage / pickup code reads position and collision of its argument entities
static void WireWorld(Rng& r) {
    g_gwired = true;
    FillGarbage(r, g_gmi, sizeof g_gmi); FillGarbage(r, g_gcol, sizeof g_gcol); FillGarbage(r, g_gcd, sizeof g_gcd);
    auto* col = reinterpret_cast<CColModel*>(g_gcol); auto* cd = reinterpret_cast<CCollisionData*>(g_gcd);
    std::memset(g_gsph, 0, sizeof g_gsph);
    for (auto& s : g_gsph) { s.m_vecCenter = CVector(Fl(r, -2.f, 2.f), Fl(r, -3.f, 3.f), Fl(r, -1.f, 1.f)); s.m_fRadius = Fl(r, 0.2f, 2.f); }
    cd->m_nNumSpheres = (uint16)r.below(5); cd->m_nNumBoxes = 0; cd->m_nNumLines = 0; cd->m_pSpheres = g_gsph; cd->m_pBoxes = nullptr;
    col->m_pColData = r.below(8) ? cd : nullptr;
    col->m_boundSphere.m_fRadius = Fl(r, 0.5f, 6.f); col->m_boundSphere.m_vecCenter = CVector(0.f, 0.f, 0.f);
    col->m_boundBox.m_vecMin = CVector(-1.f, -2.f, -1.f); col->m_boundBox.m_vecMax = CVector(1.f, 2.f, 1.f);
    Put32(g_gmi, offsetof(CBaseModelInfo, m_pColModel), (uint32_t)(uintptr_t)g_gcol);
    g_gmi[0x4A] = (uint8_t)r.below(210);   // CVehicleModelInfo::m_nHandlingId: an index of the handling table (the port asserts the table bound, a real model always has one)
    for (auto& h : gHandlingDataMgr.m_aVehicleHandling) h.m_nHandlingFlags = (eVehicleHandlingFlags)r.u32();
    g_gmodel = (int)r.below(300);
    CModelInfo::ms_modelInfoPtrs[g_gmodel] = reinterpret_cast<CBaseModelInfo*>(g_gmi);
    int i = 0;
    for (CEntity* e : rf::All) {
        uint8_t* m = g_gmat[i++ % 40];
        FillGarbage(r, m, 0x50);
        auto& M = *reinterpret_cast<CMatrix*>(m);
        const float h = r.f01() * 6.2831853f;
        M.GetRight() = CVector(std::cos(h), std::sin(h), 0.f); M.GetForward() = CVector(-std::sin(h), std::cos(h), 0.f); M.GetUp() = CVector(0.f, 0.f, 1.f);
        M.GetPosition() = CVector(Fl(r, -40.f, 40.f), Fl(r, -40.f, 40.f), Fl(r, -4.f, 8.f));
        std::memset(m + 0x40, 0, 0x10);
        Put32(reinterpret_cast<uint8_t*>(e), offsetof(CPlaceable, m_matrix), (uint32_t)(uintptr_t)m);
        Put32(reinterpret_cast<uint8_t*>(e), offsetof(CEntity, m_nModelIndex), (int16_t)g_gmodel);
    }
}
static void UnwireWorld() { CModelInfo::ms_modelInfoPtrs[g_gmodel] = nullptr; }
// an empty CPedIntelligence for a ref-world ped (SetPedDecisionMakerType / RestorePedDecisionMakerType write into it)
static void WireIntel(Rng& r, CPed* ped) {
    (void)r; std::memset(g_gint, 0, sizeof g_gint);
    reinterpret_cast<CPedIntelligence*>(g_gint)->m_pPed = ped;
    Put32(reinterpret_cast<uint8_t*>(ped), GpOffM(W2Get(GpIntel_T{})), (uint32_t)(uintptr_t)g_gint);
}
static void SaneGarage(Rng& r, CGarage& g) {
    g.m_vPosn = CVector(Fl(r, -30.f, 30.f), Fl(r, -30.f, 30.f), Fl(r, -5.f, 5.f));
    g.m_vDirectionA = CVector2D(Fl(r, -1.f, 1.f), Fl(r, -1.f, 1.f)); g.m_vDirectionB = CVector2D(Fl(r, -1.f, 1.f), Fl(r, -1.f, 1.f));
    g.m_fTopZ = g.m_vPosn.z + Fl(r, 2.f, 8.f); g.m_fWidth = Fl(r, 4.f, 12.f); g.m_fHeight = Fl(r, 2.f, 8.f);
    g.m_fLeftCoord = g.m_vPosn.x - Fl(r, 2.f, 10.f); g.m_fRightCoord = g.m_vPosn.x + Fl(r, 2.f, 10.f);
    g.m_fFrontCoord = g.m_vPosn.y - Fl(r, 2.f, 10.f); g.m_fBackCoord = g.m_vPosn.y + Fl(r, 2.f, 10.f);
    g.m_fDoorPosition = Fl(r, 0.f, 6.f); g.m_nTimeToOpen = r.below(100000);
    g.m_nType = w2::GE<eGarageType>(r); g.m_nOriginalType = w2::GE<eGarageType>(r); g.m_nDoorState = (eGarageDoorState)r.below(6); g.m_nFlags = (uint8)r.u32();
}
static bool Is(const char* a, const char* b) { return std::strcmp(a, b) == 0; }
static void GpFix(const char* cls, Rng& r, uint8_t* v, size_t sz) {
    (void)sz;
    if (Is(cls, "CGarage") || Is(cls, "CGarages") || Is(cls, "CStoredCar") || Is(cls, "CPickups") || Is(cls, "CPickup")) WireWorld(r);
    if (Is(cls, "CGangWars")) { CGangWars::NumSpecificZones = (int32)r.below(6); for (auto& z : CGangWars::aSpecificZones) z = (int32)r.below(40); CTheZones::TotalNumberOfNavigationZones = (int16)(40 + r.below(300)); for (int k = 0; k < 40; ++k) CTheZones::NavigationZoneArray[k].m_ZoneInfoIndex = (decltype(CZone::m_ZoneInfoIndex))r.below(4); }   // LINK-CALLEES: the port indexes the navigation zones through a take-view that asserts past TotalNumberOfNavigationZones, the exe reads the array unchecked: keep the indices inside the view
    if (Is(cls, "CGarage")) SaneGarage(r, *reinterpret_cast<CGarage*>(v));
    if (Is(cls, "CGarages")) {
        CGarages::NumGarages = (int32)r.below(CGarages::MAX_NUM_GARAGES + 1);
        for (int i = 0; i < CGarages::MAX_NUM_GARAGES; ++i) { FillGarbage(r, &CGarages::aGarages[i], sizeof(CGarage)); SaneGarage(r, CGarages::aGarages[i]); CGarages::aGarages[i].m_pTargetCar = r.below(3) ? nullptr : static_cast<CVehicle*>(GVehN(r)); }
        CGarages::PlayerInGarage = (int8)r.below(2); CGarages::BombsAreFree = (int8)r.below(2); CGarages::RespraysAreFree = (int8)r.below(2); CGarages::NoResprays = r.below(2);
        CGarages::LastGaragePlayerWasIn = (int32)r.below(CGarages::MAX_NUM_GARAGES + 1) - 1; CGarages::pOldToGarageWeAreIn = nullptr;
    }
    if (Is(cls, "CPedGroupMembership") || Is(cls, "CPedGroup") || Is(cls, "CPedGroupIntelligence") || Is(cls, "CPedGroups")) {
        auto fill = [&](CPedGroupMembership& m) { for (auto& p : Members(m)) p = r.below(3) ? static_cast<CPed*>(GPedN(r)) : nullptr; };
        if (Is(cls, "CPedGroupMembership")) fill(*reinterpret_cast<CPedGroupMembership*>(v));
        if (Is(cls, "CPedGroup")) fill(reinterpret_cast<CPedGroup*>(v)->GetMembership());
        if (Is(cls, "CPedGroups")) { for (auto& g : CPedGroups::ms_groups) fill(g.GetMembership()); for (auto& a : CPedGroups::ms_activeGroups) a = (char)r.below(2); }
    }
}
} // namespace w3g

static void TestW3GpHand() {
    using namespace w2; using namespace w3g;
    // CGarages::TriggerMessage 0x447B80: tag of at most 7 chars (the exe strcpy's into the 8-byte MessageIDString, the port's strcpy_s rejects longer tags: no caller passes one); the stored message / start / end times sit on the
    // edges of the current time (exe `ja`: a message that ends exactly now still counts as running; start `jb`; 500 ms re-trigger limit `jbe`)
    Run("CGarages::TriggerMessage 0x447B80", [&](Rng& r, std::string& d) {
        static const char* tags[] = { "", "A", "GA_1", "GA_2", "GARAGE1" };
        const char* tag = tags[r.below(5)]; const int16 n1 = (int16)r.u32(), n2 = (int16)r.u32(); const uint16 tm = (uint16)(r.below(2) ? r.below(6000) : r.u32());
        const char* cur = r.below(2) ? tag : tags[r.below(5)];
        static const int32 edge[] = { -1000, -501, -500, -499, -2, -1, 0, 1, 2, 499, 500, 501, 1000, 5000 };
        const int32 ds = edge[r.below(14)], de = edge[r.below(14)];
        g_post = [=](Rng&, uint8_t*) { const uint32 now = CTimer::GetTimeInMS(); std::strcpy(CGarages::MessageIDString, cur); CGarages::MessageStartTime = now + ds; CGarages::MessageEndTime = now + de; };
        return W3GpCase(r, d, "CGarages", 0, {}, [&](void*) -> uint64_t { CGarages::TriggerMessage(tag, n1, tm, n2); return 0; },
            [&](void*) -> uint64_t { oracle::Fn<void __cdecl(const char*, int, int, int)>(0x447B80)(tag, X(n1), X(tm), X(n2)); return 0; });
    }, 400);
    // CPedGroupMembership::AddMember 0x5F6AE0 / RemoveMember(int) 0x5F80D0: slot in 0..7 (the port asserts the slot is empty / occupied, the exe indexes unchecked); the member ped gets an empty intelligence and a random ped type
    Run("CPedGroupMembership::AddMember 0x5F6AE0", [&](Rng& r, std::string& d) {
        CPed* member = static_cast<CPed*>(GPedN(r)); const int32 idx = (int32)r.below(8); const uint32 type = r.below(32);
        g_post = [=](Rng& rr, uint8_t* p) { Members(*reinterpret_cast<CPedGroupMembership*>(p))[idx] = nullptr; Put32(reinterpret_cast<uint8_t*>(member), offsetof(CPed, m_nPedType), type); WireIntel(rr, member); };
        return W3GpCase(r, d, "CPedGroupMembership", sizeof(CPedGroupMembership), {}, [&](void* p) -> uint64_t { (static_cast<CPedGroupMembership*>(p)->*W2Get(GpAdd_T{}))(member, idx); return 0; },
            [&](void* p) -> uint64_t { oracle::Fn<void __fastcall(void*, int, CPed*, int)>(0x5F6AE0)(p, 0, member, idx); return 0; });
    }, 200);
    Run("CPedGroupMembership::RemoveMember-ByMemIdx 0x5F80D0", [&](Rng& r, std::string& d) {
        const int32 idx = (int32)r.below(8); const uint32 type = r.below(32); const uint32 flags = r.below(4);
        g_post = [=](Rng& rr, uint8_t* p) {
            CPed* mem = static_cast<CPed*>(GPedN(rr)); Members(*reinterpret_cast<CPedGroupMembership*>(p))[idx] = mem;
            Put32(reinterpret_cast<uint8_t*>(mem), offsetof(CPed, m_nPedType), type); WireIntel(rr, mem); reinterpret_cast<CPed*>(mem)->bClearRadarBlipOnDeath = (flags & 1); };
        return W3GpCase(r, d, "CPedGroupMembership", sizeof(CPedGroupMembership), {}, [&](void* p) -> uint64_t { static_cast<CPedGroupMembership*>(p)->RemoveMember(idx); return 0; },
            [&](void* p) -> uint64_t { oracle::Fn<void __fastcall(void*, int, int)>(0x5F80D0)(p, 0, idx); return 0; });
    }, 200);
    // pointer-returning lookups: the returned table / string lives at a different address in the port, the CONTENT is what the game reads
    Run("CWeather::FindWeatherTypesList 0x72A520", [&](Rng& r, std::string& d) {
        const uint32 region = r.below(7);
        g_post = [=](Rng&, uint8_t*) { CWeather::WeatherRegion = (eWeatherRegion)region; };
        // the exe's lists are 64 BYTES (0x8D5EB0 + n * 0x40: movsx byte readers); the port declares them as 64 eWeatherType (int16) entries: compare the entry values
        auto hashP = [](const void* p) { uint64_t h = 1469598103934665603ull; for (int i = 0; i < 64; ++i) h = (h ^ (uint8_t)((const int16_t*)p)[i]) * 1099511628211ull; return h; };
        auto hashE = [](const void* p) { uint64_t h = 1469598103934665603ull; for (int i = 0; i < 64; ++i) h = (h ^ ((const uint8_t*)p)[i]) * 1099511628211ull; return h; };
        return W3GpCase(r, d, "CWeather", 0, {}, [&](void*) -> uint64_t { return hashP(CWeather::FindWeatherTypesList()); },
            [&](void*) -> uint64_t { return hashE(oracle::Fn<const void* __cdecl()>(0x72A520)()); });
    }, 40);
    Run("CPickup::FindStringForTextIndex 0x455540", [&](Rng& r, std::string& d) {
        const int32 idx = (int32)r.below(7) - 1;
        auto hash = [](const char* p) { uint64_t h = 1469598103934665603ull; for (int i = 0; p && p[i] && i < 64; ++i) h = (h ^ (uint8_t)p[i]) * 1099511628211ull; return h ^ (p ? 1 : 2); };
        return W3GpCase(r, d, "CPickup", 0, {}, [&](void*) -> uint64_t { return hash(CPickup::FindStringForTextIndex((ePickupPropertyText)idx)); },
            [&](void*) -> uint64_t { return hash(oracle::Fn<const char* __cdecl(int)>(0x455540)(X(idx))); });
    }, 40);
    // CPlayerInfo::CancelPlayerEnteringCars 0x56E860 is `ret 4` (callee pops the argument) but declared static (cdecl) in the port: call the exe as stdcall
    Run("CPlayerInfo::CancelPlayerEnteringCars 0x56E860", [&](Rng& r, std::string& d) {
        CVehicle* v = static_cast<CVehicle*>(GVehN(r));
        return W3GpCase(r, d, "CPlayerInfo", 0, {}, [&](void*) -> uint64_t { CPlayerInfo::CancelPlayerEnteringCars(v); return 0; },
            [&](void*) -> uint64_t { oracle::Fn<void __stdcall(CVehicle*)>(0x56E860)(v); return 0; });
    }, 20);
    Run("CPopulation::AddExistingPedInCar 0x611560", [&](Rng& r, std::string& d) {
        CPed* ped = static_cast<CPed*>(GPedN(r)); CVehicle* v = static_cast<CVehicle*>(GVehN(r));
        return W3GpCase(r, d, "CPopulation", 0, {}, [&](void*) -> uint64_t { return (uintptr_t)CPopulation::AddExistingPedInCar(ped, v); },
            [&](void*) -> uint64_t { return (uintptr_t)oracle::Fn<void* __cdecl(CPed*, CVehicle*)>(0x611560)(ped, v); });
    }, 20);
    // ---- rows with out parameters (the generator skips reference / pointer-to-pointer parameters): the return value and the out values are folded into one compared word
    const auto mix = [](uint64_t a, uint64_t b) { return (a * 0x9E3779B97F4A7C15ull) ^ (b + 0x1234567ull); };
    Run("CGangWars::CanPlayerStartAGangWarHere 0x443F80 (real zone infos)", [&](Rng& r, std::string& d) {   // LINK-CALLEES: navigation zones with zone-info index 0..3 and the argument = a real entry of the zone info array (the exe compares with &ZoneInfoArray[index], index 0 included)
        CZoneInfo* zi = &CTheZones::ZoneInfoArray[r.below(5)];
        return W3GpCase(r, d, "CGangWars", 0, {}, [&](void*) -> uint64_t { return (uint64_t)(uint8_t)(CGangWars::CanPlayerStartAGangWarHere(zi) ? 1 : 0); },
            [&](void*) -> uint64_t { return (uint64_t)((uint8_t)(oracle::Fn<int __cdecl(CZoneInfo*)>(0x443F80)(zi)) ? 1 : 0); });
    }, 200);
    // ZONEINFO: AddKillToProvocation 0x443950 reads GangStrength of &ZoneInfoArray[navZone.m_ZoneInfoIndex] with no index-0 special case (0x443982..0x443996); the fixture's zones carry zone-info indices 0..3 and the zone infos 0..3 random strengths
    Run("CGangWars::AddKillToProvocation 0x443950 (zone info index 0 included)", [&](Rng& r, std::string& d) {
        const int32 type = (int32)r.below(12);   // 7 / 9 (PED_TYPE_GANG1 / GANG3) reach the zone loop
        g_post = [=](Rng& rr, uint8_t*) {
            CGangWars::bCanTriggerGangWarWhenOnAMission = true;
            for (int i = 0; i < 4; ++i) for (auto& s : CTheZones::ZoneInfoArray[i].GangStrength) s = rr.below(3) ? 0 : (uint8)rr.below(256);
        };
        return W3GpCase(r, d, "CGangWars", 0, {}, [&](void*) -> uint64_t { CGangWars::AddKillToProvocation((ePedType)type); return 0; },
            [&](void*) -> uint64_t { oracle::Fn<void __cdecl(int)>(0x443950)(X(type)); return 0; });
    }, 400);
    Run("CGangWars::PickStreamedInPedForThisGang 0x443A20", [&](Rng& r, std::string& d) {
        const int gang = (int)r.below(3); int32 o1 = GI(r), o2 = o1;
        return W3GpCase(r, d, "CGangWars", 0, {}, [&](void*) -> uint64_t { const bool b = CGangWars::PickStreamedInPedForThisGang((eGangID)gang, o1); return mix(b ? 1 : 0, (uint32)o1); },
            [&](void*) -> uint64_t { const int b = oracle::Fn<int __cdecl(int, int*)>(0x443A20)(gang, &o2); return mix((uint8_t)b ? 1 : 0, (uint32)o2); });
    }, 100);
    Run("CGangWars::TellStreamingWhichGangsAreNeeded 0x443D50", [&](Rng& r, std::string& d) {
        uint32 o1 = GU(r), o2 = o1;
        return W3GpCase(r, d, "CGangWars", 0, {}, [&](void*) -> uint64_t { CGangWars::TellStreamingWhichGangsAreNeeded(o1); return o1; },
            [&](void*) -> uint64_t { oracle::Fn<void __cdecl(uint32*)>(0x443D50)(&o2); return o2; });
    }, 100);
    Run("CGarage::FindDoorsWithGarage 0x449FF0", [&](Rng& r, std::string& d) {
        CObject* a1 = r.below(3) ? nullptr : static_cast<CObject*>(GObjN(r)); CObject* a2 = r.below(3) ? nullptr : static_cast<CObject*>(GObjN(r)); CObject *b1 = a1, *b2 = a2;
        return W3GpCase(r, d, "CGarage", sizeof(CGarage), { { W2Off_CGarage_m_pTargetCar() + 0, "v"[0] } }, [&](void* p) -> uint64_t { static_cast<CGarage*>(p)->FindDoorsWithGarage(&a1, &a2); return mix((uintptr_t)a1, (uintptr_t)a2); },
            [&](void* p) -> uint64_t { oracle::Fn<void __fastcall(void*, int, CObject**, CObject**)>(0x449FF0)(p, 0, &b1, &b2); return mix((uintptr_t)b1, (uintptr_t)b2); });
    }, 100);
    Run("CPedGroup::FindDistanceToNearestMember 0x5FB0A0", [&](Rng& r, std::string& d) {
        CPed* a1 = r.below(2) ? nullptr : static_cast<CPed*>(GPedN(r)); CPed* b1 = a1;
        return W3GpCase(r, d, "CPedGroup", sizeof(CPedGroup), { { W2Off_CPedGroup_m_pPed() + 0, "p"[0] } }, [&](void* p) -> uint64_t { const float f = static_cast<CPedGroup*>(p)->FindDistanceToNearestMember(&a1); return mix(FBits(f), (uintptr_t)a1); },
            [&](void* p) -> uint64_t { const float f = oracle::Fn<float __fastcall(void*, int, CPed**)>(0x5FB0A0)(p, 0, &b1); return mix(FBits(f), (uintptr_t)b1); });
    }, 100);
}
