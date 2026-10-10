// (c)+(d) CCarCtrl / CPathFind helpers, see physics_oracle_test.cpp
// The pure parts of CCarCtrl (speed multiplier / intersection / steering helpers) on fake vehicles, and CPathFind on a fake node set written into the
// real `ThePaths` (0x96F050): port and exe code read the same global, so every case rebuilds the node set from a seed before each of the two calls.
#include "CarCtrl.h"
#include "PathFind.h"
#include "Entity/Vehicle/Vehicle.h"
#include "MatrixLink.h"
#include "TrafficLights.h"
#include "GameLogic.h"
#include "CarAI.h"

// the sector-list scanners of the traffic AI ask for the player's vehicle: there is no player in this test
CVehicle* FindPlayerVehicle(int, bool) { return nullptr; }
namespace fm16 { alignas(16) static uint8 g_player[0x800]; static bool g_playerActive = false; }   // the FireM16 case (physics_oracle_weapon.inc) switches the fake player on
CPlayerPed* FindPlayerPed(int) { return fm16::g_playerActive ? reinterpret_cast<CPlayerPed*>(fm16::g_player) : nullptr; }
// not under test here: the exe's own (the traffic-light checks ask for the appearance of the fake vehicles)
bool CGameLogic::LaRiotsActiveHere() { return oracle::Fn<bool __cdecl()>(0x441C10)(); }
void CCarAI::CarHasReasonToStop(CVehicle* veh) { oracle::Fn<void __cdecl(CVehicle*)>(0x41C050)(veh); }

namespace cd {

// ---------------------------------------------------------------------------------------------------------------------------------
// fake vehicle: zeroed raw buffer with the real layout, position in the placement or in a (fake) matrix
struct VehFx {
    alignas(16) uint8 buf[0x1000] {};
    alignas(16) uint8 mat[sizeof(CMatrixLink) + 64] {};
    CVehicle* veh() { return reinterpret_cast<CVehicle*>(buf); }
};
static_assert(sizeof(CVehicle) < 0x1000);
static VehFx g_veh;
alignas(16) static uint8 mi[2][0x80], cm[2][0x80];   // fake model infos / col models of the model indices 600 and 601
static CVehicle* MakeVeh(Rng& r, const CVector& pos, const CVector& moveSpeed, bool forceMat = false) {
    std::memset(g_veh.buf, 0, sizeof g_veh.buf);
    std::memset(g_veh.mat, 0, sizeof g_veh.mat);
    CVehicle* v = g_veh.veh();
    if (forceMat || r.below(2)) {
        v->m_matrix = reinterpret_cast<CMatrixLink*>(g_veh.mat);
        v->m_matrix->GetPosition() = pos;
        v->m_placement.m_vPosn = CVector{ GenF(r, 50.f), GenF(r, 50.f), GenF(r, 50.f) };   // must not be read
        if (forceMat) {
            const auto set = [&](CVector& dst, const CVector& src) { dst.x = src.x; dst.y = src.y; dst.z = src.z; };
            const int sp0 = r.sp; r.sp = 0;   // (finite axes: NaN angles never end the steering loops, in the exe as well)
            set(v->m_matrix->GetRight(), GenDir(r)); set(v->m_matrix->GetForward(), GenDir(r)); set(v->m_matrix->GetUp(), CVector{ 0.f, 0.f, 1.f });
            r.sp = sp0;
        }
    } else {
        v->m_placement.m_vPosn = pos;
    }
    v->m_vecMoveSpeed = moveSpeed;
    return v;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// fake path node set, written into ThePaths
constexpr int NAREA = 72, NNODE = 10, NLINK = 40;
static CPathNode        g_nodes[NAREA][NNODE];
static CNodeAddress     g_links[NAREA][NLINK];
static CCarPathLink     g_navi[NAREA][NLINK];
static CCarPathLinkAddress g_naviLinks[NAREA][NLINK];
static uint8            g_linkLen[NAREA][NLINK];
static CPathIntersectionInfo g_inter[NAREA][NLINK];

static bool g_forceLinks = false;   // every node has at least one link (the route logic divides by the number of links)
static void PutI16(void* base, size_t off, int v) { int16 x = (int16)v; std::memcpy((uint8*)base + off, &x, 2); }

// deterministic from `seed`
static void BuildPaths(uint64_t seed, int loadedPct = 85, float posSpread = 1.f) {
    Rng r(seed);
    auto& P = ThePaths;
    std::memset(&P, 0, sizeof(CPathFind));
    for (int a = 0; a < NAREA; ++a) {
        const bool loaded = (int)r.below(100) < loadedPct;
        P.m_pPathNodes[a] = loaded ? g_nodes[a] : nullptr;
        P.m_pNaviNodes[a] = g_navi[a];
        P.m_pNodeLinks[a] = g_links[a];
        P.m_pLinkLengths[a] = g_linkLen[a];
        P.m_pPathIntersections[a] = g_inter[a];
        if (a < NUM_PATH_MAP_AREAS) P.m_pNaviLinks[a] = g_naviLinks[a];
        P.m_anNumNodes[a] = NNODE;
        const uint32 nv = r.below(NNODE + 1);
        P.m_anNumVehicleNodes[a] = nv;
        P.m_anNumPedNodes[a] = NNODE - nv;
        P.m_anNumCarPathLinks[a] = NLINK;
        P.m_anNumAddresses[a] = NLINK;
        const int cx = a % 8, cy = (a / 8) % 8;
        for (int n = 0; n < NNODE; ++n) {
            auto& nd = g_nodes[a][n];
            std::memset(&nd, 0, sizeof nd);
            auto* raw = reinterpret_cast<uint8_t*>(&nd);
            const bool inCell = r.below(6) != 0;
            const float x0 = -3000.f + 750.f * cx, y0 = -3000.f + 750.f * cy;
            const float x = inCell ? x0 + r.f01() * 750.f * posSpread : (r.f01() * 2 - 1) * 3600.f;
            const float y = inCell ? y0 + r.f01() * 750.f * posSpread : (r.f01() * 2 - 1) * 3600.f;
            PutI16(raw, 8, (int)(x * 8.f)); PutI16(raw, 10, (int)(y * 8.f)); PutI16(raw, 12, (int)((r.f01() * 2 - 1) * 80.f * 8.f));
            PutI16(raw, 0x10, (int)r.below(NLINK - 8));          // m_wBaseLinkId
            PutI16(raw, 0x12, a); PutI16(raw, 0x14, n);          // own address
            raw[0x16] = (uint8)r.u32();                          // m_nPathWidth
            raw[0x18] = (uint8)((r.below(7)) | (r.below(5) == 0 ? 0x20 : 0) | (r.below(7) == 0 ? 0x80 : 0) | (r.u32() & 0x50));
            if (g_forceLinks && !(raw[0x18] & 0xF)) raw[0x18] |= (uint8)(1 + r.below(5));
            raw[0x19] = (uint8)(r.u32() & (r.below(3) ? 0x3C : 0xFF));
            raw[0x1A] = (uint8)r.u32();
            if (g_forceLinks) raw[0x1A] &= 0x0F;   // no roadblock / parking nodes (they ask the world / the model infos)
        }
        for (int l = 0; l < NLINK; ++l) {
            g_links[a][l] = CNodeAddress{ (uint16)r.below(g_forceLinks ? NUM_PATH_MAP_AREAS : NAREA), (uint16)r.below(NNODE) };
            auto& nv2 = g_navi[a][l];
            auto* raw = reinterpret_cast<uint8_t*>(&nv2);
            std::memset(raw, 0, sizeof nv2);
            PutI16(raw, 0, (int)((r.f01() * 2 - 1) * 3600.f * 8.f)); PutI16(raw, 2, (int)((r.f01() * 2 - 1) * 3600.f * 8.f));
            const CNodeAddress att{ (uint16)r.below(NAREA), (uint16)r.below(NNODE) };
            std::memcpy(raw + 4, &att, 4);
            raw[8] = (uint8)r.u32(); raw[9] = (uint8)r.u32(); raw[10] = (uint8)r.u32();
            raw[11] = (uint8)r.u32(); raw[12] = (uint8)r.u32();
            g_naviLinks[a][l] = CCarPathLinkAddress{ (size_t)r.below(NUM_PATH_MAP_AREAS), (size_t)r.below(NLINK) };
            g_linkLen[a][l] = (uint8)r.u32();
            *reinterpret_cast<uint8*>(&g_inter[a][l]) = (uint8)(r.u32() & 3);
        }
    }
}
static uint32_t PathHash() {   // everything the functions under test may modify (node flags)
    uint32_t h = 2166136261u;
    for (int a = 0; a < NAREA; ++a) for (int n = 0; n < NNODE; ++n)
        for (size_t i = 0; i < sizeof(CPathNode); ++i) h = (h ^ ((uint8*)&g_nodes[a][n])[i]) * 16777619u;
    return h;
}
static uint32 Addr32(CNodeAddress a) { uint32 v; std::memcpy(&v, &a, 4); return v; }
static CNodeAddress RandAddr(Rng& r, int pctInvalid = 8, int numAreas = NAREA) {
    if ((int)r.below(100) < pctInvalid) return CNodeAddress{ (uint16)0xFFFF, (uint16)r.below(NNODE) };
    return CNodeAddress{ (uint16)r.below(numAreas), (uint16)r.below(NNODE) };
}
static std::string A(CNodeAddress a) { return std::to_string(a.m_wAreaId) + ":" + std::to_string(a.m_wNodeId); }

// a position that is near the fake nodes most of the time
static CVector GenWorldPos(Rng& r) {
    const float s = r.below(3) ? 3200.f : 400.f;
    CVector p{ GenF(r, s, S_NAN | S_ZERO | S_TINY), GenF(r, s, S_NAN | S_ZERO | S_TINY), GenF(r, 100.f, S_NAN | S_ZERO) };
    return p;
}


// the whole path fixture (nodes, links, ThePaths) as a byte blob: the AI functions that pick nodes change the car's route (and maybe node flags), so the second run starts from the same state
static std::vector<uint8> g_pathSnap;
static void SavePaths() {
    g_pathSnap.resize(sizeof g_nodes + sizeof g_links + sizeof g_navi + sizeof g_naviLinks + sizeof g_linkLen + sizeof g_inter + sizeof(CPathFind));
    uint8* p = g_pathSnap.data();
    const auto put = [&](const void* src, size_t n) { std::memcpy(p, src, n); p += n; };
    put(g_nodes, sizeof g_nodes); put(g_links, sizeof g_links); put(g_navi, sizeof g_navi); put(g_naviLinks, sizeof g_naviLinks); put(g_linkLen, sizeof g_linkLen); put(g_inter, sizeof g_inter); put(&ThePaths, sizeof(CPathFind));
}
static void RestorePaths() {
    const uint8* p = g_pathSnap.data();
    const auto get = [&](void* dst, size_t n) { std::memcpy(dst, p, n); p += n; };
    get(g_nodes, sizeof g_nodes); get(g_links, sizeof g_links); get(g_navi, sizeof g_navi); get(g_naviLinks, sizeof g_naviLinks); get(g_linkLen, sizeof g_linkLen); get(g_inter, sizeof g_inter); get(&ThePaths, sizeof(CPathFind));
}

// outputs of the Steer* functions (steer, gas, brake, handbrake) + the whole vehicle afterwards (+ the path fixture)
struct SteerOut { float steer = BitsF(0xDEADBEEF), gas = BitsF(0xDEADBEEF), brake = BitsF(0xDEADBEEF); bool hand = false; std::vector<uint8> veh, paths; };
template<class P, class E>
static bool CompareSteer(P&& port, E&& exe, std::string& d, const std::string& in) {
    std::vector<uint8> snap(g_veh.buf, g_veh.buf + sizeof g_veh.buf);
    SavePaths();
    const std::vector<uint8> pathsInit = g_pathSnap;
    SteerOut a, b;
    port(a);
    a.veh.assign(g_veh.buf, g_veh.buf + sizeof g_veh.buf);
    SavePaths(); a.paths = g_pathSnap;
    std::memcpy(g_veh.buf, snap.data(), snap.size());
    g_pathSnap = pathsInit; RestorePaths();
    exe(b);
    b.veh.assign(g_veh.buf, g_veh.buf + sizeof g_veh.buf);
    SavePaths(); b.paths = g_pathSnap;
    Hit(a.gas != BitsF(0xDEADBEEF));
    if (SameF(a.steer, b.steer) & SameF(a.gas, b.gas) & SameF(a.brake, b.brake) & (a.hand == b.hand) & SameBlob(a.veh.data(), b.veh.data(), a.veh.size()) & (a.paths == b.paths)) return true;
    d = in + " got " + F(a.steer) + "," + F(a.gas) + "," + F(a.brake) + "," + std::to_string(a.hand) + " exe " + F(b.steer) + "," + F(b.gas) + "," + F(b.brake) + "," + std::to_string(b.hand) + (a.veh == b.veh ? "" : " (vehicle differs)") + (a.paths == b.paths ? "" : " (paths differ)");
    return false;
}
} // namespace cd

static void TestCD() {
    using namespace cd;

    // =============================================================================================================================
    // CCarCtrl, free of world state
    Run("CCarCtrl::FindSpeedMultiplier 0x4224E0", [](Rng& r, std::string& d) {
        const float s = r.below(4) ? 4.f : (r.below(2) ? 300.f : 1e5f);   // (the wrapping loops never end for huge angles, in the exe as well)
        const float ang = GenF(r, s, S_NAN | S_ZERO | S_DEN | S_TINY), mn = GenF(r, r.below(2) ? 1.5f : 4.f), mx = GenF(r, 4.f), mm = GenF(r, 1.5f);
        const float a = CCarCtrl::FindSpeedMultiplier(ang, mn, mx, mm), b = oracle::Fn<float __cdecl(float, float, float, float)>(0x4224E0)(ang, mn, mx, mm);
        Hit(a != mm);
        if (SameF(a, b)) return true;
        d = "ang " + F(ang) + " min " + F(mn) + " max " + F(mx) + " mm " + F(mm) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CCarCtrl::FindIntersection2Lines 0x4226F0", [](Rng& r, std::string& d) {
        const float s = PickScale(r);
        float v[8]; for (auto& x : v) x = GenF(r, s);
        if (r.below(8) == 0) { v[4] = v[0] * 2.f; v[5] = v[1]; v[6] = v[2]; v[7] = v[3]; }   // parallel / identical lines
        float ax = BitsF(0xDEADBEEF), ay = ax, bx = ax, by = ax;
        CCarCtrl::FindIntersection2Lines(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], &ax, &ay);
        oracle::Fn<void __cdecl(float, float, float, float, float, float, float, float, float*, float*)>(0x4226F0)(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], &bx, &by);
        Hit(ax != BitsF(0xDEADBEEF));
        if (SameF(ax, bx) & SameF(ay, by)) return true;
        d = "in " + F(v[0]) + "," + F(v[1]) + "," + F(v[2]) + "," + F(v[3]) + "," + F(v[4]) + "," + F(v[5]) + "," + F(v[6]) + "," + F(v[7]) + " got " + F(ax) + "," + F(ay) + " exe " + F(bx) + "," + F(by); return false;
    });
    Run("CCarCtrl::FindSpeedMultiplierWithSpeedFromNodes 0x424130", [](Rng& r, std::string& d) {
        static const int8 vals[] = { -1, 0, 1, 2, 3, 4, 5, 127, -128, -2 };
        const int8 v = vals[r.below(10)];
        const float a = CCarCtrl::FindSpeedMultiplierWithSpeedFromNodes(v), b = oracle::Fn<float __cdecl(int)>(0x424130)((int)v);
        if (SameF(a, b)) return true;
        d = "arg " + std::to_string(v) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CCarCtrl::FindMaxSteerAngle 0x427FE0", [](Rng& r, std::string& d) {
        const auto sp = GenV(r, r.below(2) ? 1.2f : PickScale(r));
        auto* v = MakeVeh(r, {}, sp);
        const float a = CCarCtrl::FindMaxSteerAngle(v), b = oracle::Fn<float __cdecl(CVehicle*)>(0x427FE0)(v);
        if (SameF(a, b)) return true;
        d = "speed " + V(sp) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CCarCtrl::ThisVehicleShouldTryNotToTurn 0x421FE0", [](Rng& r, std::string& d) {
        auto* v = MakeVeh(r, {}, {});
        v->m_nModelIndex = r.below(3) ? (int16)(400 + r.below(220)) : (int16)(int)(r.u32());
        const bool a = CCarCtrl::ThisVehicleShouldTryNotToTurn(v), b = oracle::Fn<bool __cdecl(CVehicle*)>(0x421FE0)(v);
        Hit(a);
        if (a == b) return true;
        d = "model " + std::to_string(v->m_nModelIndex) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CCarCtrl::FindPercDependingOnDistToLink 0x422620", [](Rng& r, std::string& d) {
        BuildPaths(r.u32());
        const CNodeAddress ln = CNodeAddress{ (uint16)r.below(NUM_PATH_MAP_AREAS), (uint16)r.below(NLINK) };
        const CCarPathLinkAddress la{ (size_t)ln.m_wAreaId, (size_t)ln.m_wNodeId };
        const auto lp = g_navi[la.m_wAreaId][la.m_wCarPathLinkId].m_posn;
        const float dist = r.below(4) ? r.f01() * 25.f : PickScale(r);
        const float ang = r.f01() * 6.2831853f;
        CVector pos{ (float)lp.x + std::cos(ang) * dist, (float)lp.y + std::sin(ang) * dist, GenF(r, 50.f) };
        if (r.sp && r.below(100) < r.sp) pos = GenV(r, 3000.f);
        auto* v = MakeVeh(r, pos, {});
        uint16 laRaw; std::memcpy(&laRaw, &la, 2);
        const float a = CCarCtrl::FindPercDependingOnDistToLink(v, la), b = oracle::Fn<float __cdecl(CVehicle*, uint32)>(0x422620)(v, laRaw);
        Hit(a != 0.5f && a != 1.f);
        if (SameF(a, b)) return true;
        d = "pos " + V(pos) + " link " + std::to_string(la.m_wAreaId) + ":" + std::to_string(la.m_wCarPathLinkId) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CCarCtrl::FindGhostRoadHeight 0x422370", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 80);
        auto* v = MakeVeh(r, GenWorldPos(r), {});
        auto& ap = v->m_autoPilot;
        ap.m_currentAddress = RandAddr(r, 6);
        ap.m_startingRouteNode = RandAddr(r, 6);
        if (r.below(2) && ap.m_currentAddress.m_wAreaId != 0xFFFF && ap.m_startingRouteNode.m_wAreaId != 0xFFFF) {   // vehicle close to the segment
            const auto p1 = g_nodes[ap.m_currentAddress.m_wAreaId][ap.m_currentAddress.m_wNodeId].GetPosition();
            v->GetPosition() = CVector{ p1.x + GenF(r, 40.f), p1.y + GenF(r, 40.f), p1.z + GenF(r, 5.f) };
        }
        const float a = CCarCtrl::FindGhostRoadHeight(v), b = oracle::Fn<float __cdecl(CVehicle*)>(0x422370)(v);
        Hit(a != 0.f);
        if (SameF(a, b)) return true;
        d = "pos " + V(v->GetPosition()) + " cur " + A(ap.m_currentAddress) + " start " + A(ap.m_startingRouteNode) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CCarCtrl::FindPathDirection 0x422090", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 90);
        CNodeAddress n[3];
        for (auto& x : n) x = RandAddr(r, 5);
        if (r.below(8) == 0) n[2] = n[0];                 // U-turn exactly
        if (r.below(10) == 0) n[1] = n[0];                // zero length
        bool u1 = false, u2 = false;
        const int8 a = CCarCtrl::FindPathDirection(n[0], n[1], n[2], &u1);
        const int8 b = (int8)oracle::Fn<int __cdecl(uint32, uint32, uint32, bool*)>(0x422090)(Addr32(n[0]), Addr32(n[1]), Addr32(n[2]), &u2);
        Hit(a != 0);
        if (a == b && u1 == u2) return true;
        d = "nodes " + A(n[0]) + " " + A(n[1]) + " " + A(n[2]) + " got " + std::to_string(a) + "/" + std::to_string(u1) + " exe " + std::to_string(b) + "/" + std::to_string(u2); return false;
    });

    // two fake vehicle models (model info table entries 600 / 601 -> raw model info with the colmodel pointer at +0x14, bounding box first in the colmodel)
    {
        for (int i = 0; i < 2; ++i) { void* p = cm[i]; std::memcpy(mi[i] + 0x14, &p, 4); void* q = mi[i]; std::memcpy(reinterpret_cast<uint8*>(0xA9B0C8) + (600 + i) * 4, &q, 4); }
        const auto Rects = [](const char* name, unsigned va, float (*port)(CVehicle*, CVehicle*, float, float, CVector*, CVector*)) {
            Run(name, [va, port](Rng& r, std::string& d) {
                for (int i = 0; i < 2; ++i) {   // bounding boxes: min = -max most of the time, like a car
                    float* bb = reinterpret_cast<float*>(cm[i]);
                    const float hx = 0.8f + r.f01() * 1.5f, hy = 1.5f + r.f01() * 3.f;
                    bb[0] = -hx; bb[1] = r.below(4) ? -hy : -hy * r.f01(); bb[2] = -1.f; bb[3] = hx; bb[4] = hy; bb[5] = 1.f;
                    if (r.sp && r.below(100) < r.sp) bb[r.below(6)] = GenF(r, 5.f);
                }
                static VehFx fx2;
                CVehicle* a1 = MakeVeh(r, GenV(r, r.below(3) ? 8.f : 30.f, S_NAN | S_ZERO | S_TINY), {});
                a1->m_nModelIndex = 600;
                std::memset(fx2.buf, 0, sizeof fx2.buf);
                CVehicle* a2 = fx2.veh();
                a2->m_placement.m_vPosn = CVector{ GenF(r, r.below(3) ? 8.f : 30.f, S_NAN | S_ZERO | S_TINY), GenF(r, 8.f, S_NAN | S_ZERO | S_TINY), 0.f };
                a2->m_nModelIndex = 601;
                const float mvx = GenF(r, r.below(2) ? 1.f : 10.f), mvy = GenF(r, r.below(2) ? 1.f : 10.f);
                CVector d1 = GenDir(r), d2 = GenDir(r);
                const float x = port(a1, a2, mvx, mvy, &d1, &d2);
                const float y = oracle::Fn<float __cdecl(CVehicle*, CVehicle*, float, float, CVector*, CVector*)>(va)(a1, a2, mvx, mvy, &d1, &d2);
                Hit(x < 1.f);
                if (SameF(x, y)) return true;
                d = "p1 " + V(a1->GetPosition()) + " p2 " + V(a2->GetPosition()) + " mv " + F(mvx) + "," + F(mvy) + " d1 " + V(d1) + " d2 " + V(d2) + " bb0 " + Hex(cm[0], 24) + " bb1 " + Hex(cm[1], 24) + " got " + F(x) + " exe " + F(y); return false;
            });
        };
        Rects("CCarCtrl::TestCollisionBetween2MovingRects 0x425B30", 0x425B30, &CCarCtrl::TestCollisionBetween2MovingRects);
        Rects("CCarCtrl::TestCollisionBetween2MovingRects_OnlyFrontBumper 0x425F70", 0x425F70, &CCarCtrl::TestCollisionBetween2MovingRects_OnlyFrontBumper);
    }
    Run("CCarCtrl::ClitargetOrientationToLink 0x422760", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 90);
        const CCarPathLinkAddress la{ (size_t)r.below(NUM_PATH_MAP_AREAS), (size_t)r.below(NLINK) };
        auto* v = MakeVeh(r, GenV(r, r.below(2) ? 50.f : 3000.f, S_NAN | S_ZERO | S_TINY), {});
        const int8 sign = r.below(3) ? (r.below(2) ? 1 : -1) : (int8)r.below(3);
        const float tx = GenF(r, r.below(2) ? 50.f : 3000.f, S_NAN | S_ZERO | S_TINY), ty = GenF(r, r.below(2) ? 50.f : 3000.f, S_NAN | S_ZERO | S_TINY);
        const float o0 = r.below(4) ? GenF(r, 4.f, S_NAN | S_ZERO | S_TINY) : GenF(r, 100.f, S_NAN | S_ZERO | S_TINY);
        float o1 = o0, o2 = o0;
        uint16 raw; std::memcpy(&raw, &la, 2);
        CCarCtrl::ClitargetOrientationToLink(v, la, sign, &o1, tx, ty);
        oracle::Fn<void __cdecl(CVehicle*, uint32, int, float*, float, float)>(0x422760)(v, raw, (int)sign, &o2, tx, ty);
        Hit(o1 != o0);
        if (SameF(o1, o2)) return true;
        d = "pos " + V(v->GetPosition()) + " link " + std::to_string(la.m_wAreaId) + ":" + std::to_string(la.m_wCarPathLinkId) + " sign " + std::to_string(sign) + " t " + F(tx) + "," + F(ty) + " o " + F(o0) + " got " + F(o1) + " exe " + F(o2); return false;
    });
    // steering: gas / brake / handbrake / steer of the AI on a fake vehicle (matrix, speed, autopilot), empty world
    const auto MakeSteerVeh = [](Rng& r, int driveStyle = -1) {
        auto* v = MakeVeh(r, GenV(r, r.below(2) ? 100.f : 2900.f, S_ZERO | S_TINY), GenV(r, r.below(2) ? 0.5f : 2.f, S_NAN | S_ZERO | S_TINY), true)   /* (a NaN position never ends the angle loops, in the exe as well) */;
        v->m_nModelIndex = 600;
        v->m_autoPilot.m_nCarDrivingStyle = (eCarDrivingStyle)(driveStyle >= 0 ? driveStyle : r.below(8));
        v->m_autoPilot.m_nCruiseSpeed = (uint8)r.u32();
        v->m_autoPilot.m_nCarMission = (eCarMission)r.below(60);
        v->m_nVehicleSubType = (eVehicleType)r.below(10);
        v->m_fSteerAngle = GenF(r, 0.7f);
        return v;
    };
    Run("CCarCtrl::SteerAICarWithPhysicsHeadingForTarget 0x433280", [MakeSteerVeh](Rng& r, std::string& d) {
        auto* v = MakeSteerVeh(r);
        const CVector tg = GenV(r, r.below(2) ? 100.f : 3000.f, S_ZERO | S_TINY);
        return CompareSteer(
            [&](SteerOut& o) { CCarCtrl::SteerAICarWithPhysicsHeadingForTarget(v, nullptr, tg.x, tg.y, &o.steer, &o.gas, &o.brake, &o.hand); },
            [&](SteerOut& o) { oracle::Fn<void __cdecl(CVehicle*, CPhysical*, float, float, float*, float*, float*, bool*)>(0x433280)(v, nullptr, tg.x, tg.y, &o.steer, &o.gas, &o.brake, &o.hand); },
            d, "pos " + V(v->GetPosition()) + " ms " + V(v->m_vecMoveSpeed) + " target " + V(tg) + " style " + std::to_string((int)v->m_autoPilot.m_nCarDrivingStyle) + " cruise " + std::to_string(v->m_autoPilot.m_nCruiseSpeed));
    });
    Run("CCarCtrl::SteerAICarWithPhysicsTryingToBlockTarget 0x4335E0", [MakeSteerVeh](Rng& r, std::string& d) {
        auto* v = MakeSteerVeh(r);
        const CVector tg = GenV(r, r.below(2) ? 100.f : 3000.f, S_ZERO | S_TINY), dir = GenV(r, r.below(2) ? 0.2f : 1.f, S_ZERO | S_TINY);
        return CompareSteer(
            [&](SteerOut& o) { CCarCtrl::SteerAICarWithPhysicsTryingToBlockTarget(v, nullptr, tg.x, tg.y, dir.x, dir.y, &o.steer, &o.gas, &o.brake, &o.hand); },
            [&](SteerOut& o) { oracle::Fn<void __cdecl(CVehicle*, CEntity*, float, float, float, float, float*, float*, float*, bool*)>(0x4335E0)(v, nullptr, tg.x, tg.y, dir.x, dir.y, &o.steer, &o.gas, &o.brake, &o.hand); },
            d, "pos " + V(v->GetPosition()) + " ms " + V(v->m_vecMoveSpeed) + " target " + V(tg) + " dir " + V(dir));
    });
    Run("CCarCtrl::SteerAICarWithPhysicsTryingToBlockTarget_Stop 0x428990", [MakeSteerVeh](Rng& r, std::string& d) {
        auto* v = MakeSteerVeh(r);
        const CVector tg = GenV(r, r.below(2) ? 100.f : 3000.f, S_ZERO | S_TINY), dir = GenV(r, r.below(2) ? 0.2f : 1.f, S_ZERO | S_TINY);
        return CompareSteer(
            [&](SteerOut& o) { CCarCtrl::SteerAICarWithPhysicsTryingToBlockTarget_Stop(v, tg.x, tg.y, dir.x, dir.y, &o.steer, &o.gas, &o.brake, &o.hand); },
            [&](SteerOut& o) { oracle::Fn<void __cdecl(CVehicle*, float, float, float, float, float*, float*, float*, bool*)>(0x428990)(v, tg.x, tg.y, dir.x, dir.y, &o.steer, &o.gas, &o.brake, &o.hand); },
            d, "pos " + V(v->GetPosition()) + " ms " + V(v->m_vecMoveSpeed) + " target " + V(tg) + " dir " + V(dir));
    });
    Run("CCarCtrl::SteerAIBoatWithPhysicsHeadingForTarget 0x428BE0", [MakeSteerVeh](Rng& r, std::string& d) {
        auto* v = MakeSteerVeh(r);
        const CVector tg = GenV(r, r.below(2) ? 100.f : 3000.f, S_ZERO | S_TINY);
        return CompareSteer(
            [&](SteerOut& o) { CCarCtrl::SteerAIBoatWithPhysicsHeadingForTarget(v, tg.x, tg.y, &o.steer, &o.gas, &o.brake); },
            [&](SteerOut& o) { oracle::Fn<void __cdecl(CVehicle*, float, float, float*, float*, float*)>(0x428BE0)(v, tg.x, tg.y, &o.steer, &o.gas, &o.brake); },
            d, "pos " + V(v->GetPosition()) + " ms " + V(v->m_vecMoveSpeed) + " target " + V(tg) + " cruise " + std::to_string(v->m_autoPilot.m_nCruiseSpeed));
    });
    Run("CCarCtrl::SteerAICarTowardsPointInEscort 0x4336D0", [MakeSteerVeh](Rng& r, std::string& d) {
        auto* v = MakeSteerVeh(r);
        static VehFx fx2;
        std::memset(fx2.buf, 0, sizeof fx2.buf); std::memset(fx2.mat, 0, sizeof fx2.mat);
        CVehicle* e = fx2.veh();
        e->m_matrix = reinterpret_cast<CMatrixLink*>(fx2.mat);
        e->m_matrix->GetPosition() = GenV(r, 100.f, S_ZERO | S_TINY);
        e->m_vecMoveSpeed = GenV(r, 1.f, S_ZERO | S_TINY);   // (finite: a NaN target point never ends the angle loops)
        const auto set = [&](CVector& dst, const CVector& src) { dst.x = src.x; dst.y = src.y; dst.z = src.z; };
        { const int sp0 = r.sp; r.sp = 0; set(e->m_matrix->GetRight(), GenDir(r)); set(e->m_matrix->GetForward(), GenDir(r)); set(e->m_matrix->GetUp(), CVector{ 0.f, 0.f, 1.f }); r.sp = sp0; }
        const float ox = GenF(r, 20.f, S_ZERO | S_TINY), oy = GenF(r, 20.f, S_ZERO | S_TINY);
        return CompareSteer(
            [&](SteerOut& o) { CCarCtrl::SteerAICarTowardsPointInEscort(v, e, ox, oy, &o.steer, &o.gas, &o.brake, &o.hand); },
            [&](SteerOut& o) { oracle::Fn<void __cdecl(CVehicle*, CVehicle*, float, float, float*, float*, float*, bool*)>(0x4336D0)(v, e, ox, oy, &o.steer, &o.gas, &o.brake, &o.hand); },
            d, "pos " + V(v->GetPosition()) + " ms " + V(v->m_vecMoveSpeed) + " epos " + V(e->GetPosition()) + " ems " + V(e->m_vecMoveSpeed) + " off " + F(ox) + "," + F(oy));
    });
    Run("CCarCtrl::FindNodesThisCarIsNearestTo 0x42BD20", [MakeSteerVeh](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 90);
        auto* v = MakeSteerVeh(r);
        v->GetPosition() = GenWorldPos(r);
        CNodeAddress a1{ 0x7777, 0x8888 }, a2 = a1, b1 = a1, b2 = a1;
        CCarCtrl::FindNodesThisCarIsNearestTo(v, a1, a2);
        oracle::Fn<void __cdecl(CVehicle*, CNodeAddress*, CNodeAddress*)>(0x42BD20)(v, &b1, &b2);
        Hit(a1.m_wAreaId != 0xFFFF);
        // the node ids are garbage in the original when nothing was found
        const auto same = [](CNodeAddress x, CNodeAddress y) { return x.m_wAreaId == y.m_wAreaId && (x.m_wAreaId == 0xFFFF || x.m_wNodeId == y.m_wNodeId); };
        if (same(a1, b1) && same(a2, b2)) return true;
        d = "pos " + V(v->GetPosition()) + " got " + A(a1) + "," + A(a2) + " exe " + A(b1) + "," + A(b2); return false;
    });

    // WeaveFor*: the angle range [lower, upper] that is blocked by a ped / by another car
    Run("CCarCtrl::WeaveForPed 0x426970", [MakeSteerVeh](Rng& r, std::string& d) {
        auto* v = MakeSteerVeh(r);
        alignas(16) static uint8 pedBuf[0x800];
        std::memset(pedBuf, 0, sizeof pedBuf);
        auto* ped = reinterpret_cast<CPed*>(pedBuf);
        const CVector pp = r.below(2) ? CVector{ v->GetPosition().x + GenF(r, 20.f), v->GetPosition().y + GenF(r, 20.f), 0.f } : GenV(r, 100.f, S_ZERO | S_TINY);
        ped->m_placement.m_vPosn = pp;
        const float lo = GenF(r, r.below(3) ? 3.5f : 10.f, S_NAN | S_ZERO | S_DEN | S_TINY), hi = r.below(3) ? lo + r.f01() * 0.3f : GenF(r, 3.5f, S_NAN | S_ZERO | S_DEN | S_TINY);
        float lo2 = lo, hi2 = hi;
        if (r.below(3)) { const float h = CGeneral::GetATanOfXY(pp.x - v->GetPosition().x, pp.y - v->GetPosition().y); lo2 = h + (r.f01() - 0.5f) * 0.4f; hi2 = lo2 + r.f01() * 0.4f; }   // ranges around the bearing of the ped
        float l1 = lo2, u1 = hi2, l2 = lo2, u2 = hi2;
        CCarCtrl::WeaveForPed(ped, v, &l1, &u1);
        oracle::Fn<void __cdecl(CPed*, CVehicle*, float*, float*)>(0x426970)(ped, v, &l2, &u2);
        Hit(l1 != lo2 || u1 != hi2);
        if (SameF(l1, l2) & SameF(u1, u2)) return true;
        d = "veh " + V(v->GetPosition()) + " ped " + V(pp) + " range " + F(lo) + "," + F(hi) + " got " + F(l1) + "," + F(u1) + " exe " + F(l2) + "," + F(u2); return false;
    });
    Run("CCarCtrl::WeaveForOtherCar 0x426350", [MakeSteerVeh](Rng& r, std::string& d) {
        auto* v = MakeSteerVeh(r);
        v->m_autoPilot.m_nCarMission = MISSION_NONE;
        static VehFx fx2;
        std::memset(fx2.buf, 0, sizeof fx2.buf); std::memset(fx2.mat, 0, sizeof fx2.mat);
        CVehicle* e = fx2.veh();
        e->m_nModelIndex = 601;
        e->m_matrix = reinterpret_cast<CMatrixLink*>(fx2.mat);
        const CVector vp = v->GetPosition();
        const auto set = [&](CVector& dst, const CVector& src) { dst.x = src.x; dst.y = src.y; dst.z = src.z; };
        { const int sp0 = r.sp; r.sp = 0; 
          const float ang = r.f01() * 6.2831853f, ca = std::cos(ang), sa = std::sin(ang);
          set(e->m_matrix->GetRight(), CVector{ ca, sa, 0.f }); set(e->m_matrix->GetForward(), CVector{ -sa, ca, 0.f }); set(e->m_matrix->GetUp(), CVector{ 0.f, 0.f, 1.f });
          const float dist = r.below(4) ? 2.f + r.f01() * 14.f : r.f01() * 60.f, a2 = r.f01() * 6.2831853f;
          set(e->m_matrix->GetPosition(), CVector{ vp.x + std::cos(a2) * dist, vp.y + std::sin(a2) * dist, vp.z });
          r.sp = sp0; }
        e->m_vecMoveSpeed = GenV(r, 1.f, S_ZERO | S_TINY);
        e->m_autoPilot.m_nCarMission = (eCarMission)(r.below(4) ? 0 : r.below(60));
        v->m_vecMoveSpeed = GenV(r, 1.f, S_ZERO | S_TINY);
        for (int i = 0; i < 2; ++i) {   // vehicle model 600, entity model 601 (random boxes)
            float* bb = reinterpret_cast<float*>(cm[i]);
            const float hx = 0.8f + r.f01() * 1.5f, hy = 1.5f + r.f01() * 3.f;
            bb[0] = -hx; bb[1] = -hy * (0.7f + 0.3f * r.f01()); bb[2] = -1.f; bb[3] = hx; bb[4] = hy; bb[5] = 1.f;
        }
        const float lo = r.f01() * 6.28f, hi = r.below(2) ? lo : r.f01() * 6.28f;
        float l1 = lo, u1 = hi, l2 = lo, u2 = hi;
        CCarCtrl::WeaveForOtherCar(e, v, &l1, &u1);
        oracle::Fn<void __cdecl(CEntity*, CVehicle*, float*, float*)>(0x426350)(e, v, &l2, &u2);
        Hit(l1 != lo || u1 != hi);
        if (SameF(l1, l2) & SameF(u1, u2)) return true;
        d = "veh " + V(vp) + " ms " + V(v->m_vecMoveSpeed) + " ent " + V(e->GetPosition()) + " ems " + V(e->m_vecMoveSpeed) + " fwd " + V(e->m_matrix->GetForward()) + " range " + F(lo) + "," + F(hi) + " got " + F(l1) + "," + F(u1) + " exe " + F(l2) + "," + F(u2); return false;
    });

    // parking: the vehicle follows the nodes of its route
    const auto ParkTest = [MakeSteerVeh](const char* name, unsigned va, void (*port)(CVehicle*, float*, float*, float*, bool*), std::initializer_list<eCarMission> missions) {
        Run(name, [MakeSteerVeh, va, port, missions](Rng& r, std::string& d) {
            BuildPaths(r.u32(), 90);
            auto* v = MakeSteerVeh(r);
            auto& ap = v->m_autoPilot;
            ap.m_startingRouteNode = RandAddr(r, 0);
            ap.m_currentAddress    = RandAddr(r, 0);
            ap.m_nCarMission = *(missions.begin() + r.below((uint32)missions.size()));
            if (ThePaths.m_pPathNodes[ap.m_startingRouteNode.m_wAreaId] && r.below(2)) {   // close to the node that is the target
                const auto sp = g_nodes[ap.m_startingRouteNode.m_wAreaId][ap.m_startingRouteNode.m_wNodeId].GetPosition();
                v->GetPosition() = CVector{ sp.x + GenF(r, r.below(2) ? 3.f : 20.f, S_ZERO | S_TINY), sp.y + GenF(r, r.below(2) ? 3.f : 20.f, S_ZERO | S_TINY), sp.z };
            }
            return CompareSteer(
                [&](SteerOut& o) { port(v, &o.steer, &o.gas, &o.brake, &o.hand); },
                [&](SteerOut& o) { oracle::Fn<void __cdecl(CVehicle*, float*, float*, float*, bool*)>(va)(v, &o.steer, &o.gas, &o.brake, &o.hand); },
                d, "pos " + V(v->GetPosition()) + " ms " + V(v->m_vecMoveSpeed) + " start " + A(ap.m_startingRouteNode) + " cur " + A(ap.m_currentAddress) + " mission " + std::to_string((int)ap.m_nCarMission));
        });
    };
    ParkTest("CCarCtrl::SteerAICarParkParallel 0x433BA0", 0x433BA0, &CCarCtrl::SteerAICarParkParallel, { MISSION_PARK_PARALLEL, MISSION_PARK_PARALLEL_2 });
    ParkTest("CCarCtrl::SteerAICarParkPerpendicular 0x433EA0", 0x433EA0, &CCarCtrl::SteerAICarParkPerpendicular, { MISSION_PARK_PERPENDICULAR, MISSION_PARK_PERPENDICULAR_2 });

    // traffic lights: the three path links of the vehicle decide
    const auto MakeLightVeh = [MakeSteerVeh](Rng& r) {
        BuildPaths(r.u32(), 90);
        auto* v = MakeSteerVeh(r);
        alignas(16) static uint8 handling[0x100];
        std::memset(handling, 0, sizeof handling);
        static const uint32 flags[] = { 0x0, 0x1000000, 0x2000000, 0x4000000, 0x8000000, 0x3000000, 0x5000000 };
        const uint32 fl = flags[r.below(r.below(4) ? 4 : 7)];
        std::memcpy(handling + 0xCC, &fl, 4);
        void* hp = handling; std::memcpy(g_veh.buf + 0x384, &hp, 4);
        auto& ap = v->m_autoPilot;
        const auto PickLink = [&](CCarPathLinkAddress& dst, int8& sign) {
            dst = r.below(10) == 0 ? CCarPathLinkAddress{ 63, 1023 } : CCarPathLinkAddress{ (size_t)r.below(NUM_PATH_MAP_AREAS), (size_t)r.below(NLINK) };
            sign = r.below(2) ? 1 : -1;
        };
        PickLink(ap.m_nCurrentPathNodeInfo, ap._smthCurr);
        PickLink(ap.m_nNextPathNodeInfo, ap._smthNext);
        PickLink(ap.m_nPreviousPathNodeInfo, ap._smthPrev);
        ap.m_currentAddress = RandAddr(r, 5, NUM_PATH_MAP_AREAS); ap.m_startingRouteNode = RandAddr(r, 5, NUM_PATH_MAP_AREAS); ap.m_endingRouteNode = RandAddr(r, 5, NUM_PATH_MAP_AREAS);
        // addresses that are the "attached to" of the links make the direction checks pass
        for (auto* pi : { &ap.m_nCurrentPathNodeInfo, &ap.m_nNextPathNodeInfo, &ap.m_nPreviousPathNodeInfo }) {
            if (pi->m_wCarPathLinkId == 1023 && pi->m_wAreaId == 63) continue;
            auto& nv = g_navi[pi->m_wAreaId][pi->m_wCarPathLinkId];
            if (r.below(2)) { CNodeAddress* targets[] = { &ap.m_currentAddress, &ap.m_startingRouteNode, &ap.m_endingRouteNode }; std::memcpy(reinterpret_cast<uint8*>(&nv) + 4, targets[pi == &ap.m_nCurrentPathNodeInfo ? 0 : pi == &ap.m_nNextPathNodeInfo ? 1 : 2], 4); }
            if (r.below(2)) {   // the vehicle stands close to the link
                const auto lp = nv.m_posn; const auto dir = nv.m_dir; const float k = (r.f01() * 2 - 1) * 14.f;
                v->GetPosition() = CVector{ (float)lp.x + (float)dir.x * k, (float)lp.y + (float)dir.y * k, 0.f };
            }
        }
        v->SetStatus(r.below(2) ? STATUS_PHYSICS : STATUS_SIMPLE);
        CTimer::m_snTimeInMillisecondsNonClipped = r.u32() & 0xFFFFF;
        return v;
    };
    Run("CTrafficLights::ShouldCarStopForBridge 0x49D420", [MakeLightVeh](Rng& r, std::string& d) {
        auto* v = MakeLightVeh(r);
        auto& ap = v->m_autoPilot;
        const bool a = CTrafficLights::ShouldCarStopForBridge(v), b = oracle::Fn<bool __cdecl(CVehicle*)>(0x49D420)(v);
        Hit(a);
        if (a == b) return true;
        d = "cur " + std::to_string(*(uint16*)&ap.m_nCurrentPathNodeInfo) + " next " + std::to_string(*(uint16*)&ap.m_nNextPathNodeInfo) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CTrafficLights::ShouldCarStopForLight 0x49D610", [MakeLightVeh](Rng& r, std::string& d) {
        auto* v = MakeLightVeh(r);
        const bool unkn = r.below(2);
        const bool a = CTrafficLights::ShouldCarStopForLight(v, unkn), b = oracle::Fn<bool __cdecl(CVehicle*, bool)>(0x49D610)(v, unkn);
        Hit(a);
        if (a == b) return true;
        d = "pos " + V(v->GetPosition()) + " unkn " + std::to_string(unkn) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });

    // the main traffic AI: follows the links of the route (needs the next node -> PickNextNodeRandomly: rand() on both sides, seeded alike)
    const auto FollowTest = [MakeLightVeh](const char* name, unsigned va, void (*port)(CVehicle*, float*, float*, float*, bool*)) { Run(name, [MakeLightVeh, va, port](Rng& r, std::string& d) {
        g_forceLinks = true;
        auto* v = MakeLightVeh(r);
        g_forceLinks = false;
        auto& ap = v->m_autoPilot;
        ap.m_nCurrentLane = (uint8)r.below(4); ap.m_nNextLane = (uint8)r.below(4);
        ap.m_nCarMission = r.below(4) ? MISSION_CRUISE : MISSION_NONE;
        ap.m_nCarDrivingStyle = (eCarDrivingStyle)r.below(8);
        ap.m_nCruiseSpeed = (uint8)(r.below(3) ? 10 + r.below(40) : r.u32());
        ap.m_vecDestinationCoors = GenV(r, 100.f, S_ZERO | S_TINY);
        ap.m_nTempAction = TEMPACT_NONE;
        // the vehicle near the current link so that the node logic is exercised
        const auto& cur = g_navi[ap.m_nCurrentPathNodeInfo.m_wAreaId][ap.m_nCurrentPathNodeInfo.m_wCarPathLinkId];
        if (r.below(4)) v->GetPosition() = CVector{ (float)cur.m_posn.x + GenF(r, r.below(2) ? 12.f : 80.f, S_ZERO | S_TINY), (float)cur.m_posn.y + GenF(r, r.below(2) ? 12.f : 80.f, S_ZERO | S_TINY), 0.f };
        const uint32 seed = r.u32();
        return CompareSteer(
            [&](SteerOut& o) { SeedBoth(seed); port(v, &o.steer, &o.gas, &o.brake, &o.hand); },
            [&](SteerOut& o) { SeedBoth(seed); oracle::Fn<void __cdecl(CVehicle*, float*, float*, float*, bool*)>(va)(v, &o.steer, &o.gas, &o.brake, &o.hand); },
            d, "pos " + V(v->GetPosition()) + " ms " + V(v->m_vecMoveSpeed) + " style " + std::to_string((int)ap.m_nCarDrivingStyle) + " cruise " + std::to_string(ap.m_nCruiseSpeed) + " cur " + std::to_string(*(uint16*)&ap.m_nCurrentPathNodeInfo) + " next " + std::to_string(*(uint16*)&ap.m_nNextPathNodeInfo) + " seed " + std::to_string(seed));
    }); };
    FollowTest("CCarCtrl::SteerAICarWithPhysicsFollowPath 0x434900", 0x434900, &CCarCtrl::SteerAICarWithPhysicsFollowPath);
    FollowTest("CCarCtrl::SteerAICarWithPhysicsFollowPath_Racing 0x435830", 0x435830, &CCarCtrl::SteerAICarWithPhysicsFollowPath_Racing);

    // the world is empty here (zeroed sector lists), so the traffic scans only exercise the geometry / speed arithmetic around them
    Run("CCarCtrl::FindMaximumSpeedForThisCarInTraffic 0x434400", [](Rng& r, std::string& d) {
        auto* v = MakeVeh(r, GenV(r, r.below(2) ? 3000.f : 20.f, S_NAN | S_ZERO | S_TINY | S_HUGE), GenV(r, 0.5f), true);
        v->m_nModelIndex = 600;
        auto& ap = v->m_autoPilot;
        ap.m_nCruiseSpeed = (uint8)r.u32();
        ap.m_SpeedMult = r.below(3) ? 0.5f + r.f01() * 2.f : GenF(r, 4.f);
        ap.m_nCarDrivingStyle = (eCarDrivingStyle)r.below(8);
        std::vector<uint8> snap(g_veh.buf, g_veh.buf + sizeof g_veh.buf);
        const float a = CCarCtrl::FindMaximumSpeedForThisCarInTraffic(v);
        std::vector<uint8> after(g_veh.buf, g_veh.buf + sizeof g_veh.buf);
        std::memcpy(g_veh.buf, snap.data(), snap.size());
        const float b = oracle::Fn<float __cdecl(CVehicle*)>(0x434400)(v);
        Hit(a != 0.f);
        if (SameF(a, b) && !std::memcmp(after.data(), g_veh.buf, after.size())) return true;
        d = "pos " + V(v->GetPosition()) + " style " + std::to_string((int)ap.m_nCarDrivingStyle) + " cruise " + std::to_string(ap.m_nCruiseSpeed) + " mult " + F(ap.m_SpeedMult) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CCarCtrl::FindAngleToWeaveThroughTraffic 0x4325C0", [](Rng& r, std::string& d) {
        auto* v = MakeVeh(r, GenV(r, r.below(2) ? 2900.f : 20.f, S_NAN | S_ZERO | S_TINY), GenV(r, r.below(2) ? 1.f : 0.1f, S_NAN | S_ZERO | S_TINY), true);
        v->m_nModelIndex = 600;
        v->m_autoPilot.m_nCarDrivingStyle = (eCarDrivingStyle)r.below(8);
        const float s = r.below(4) ? 4.f : (r.below(2) ? 300.f : 1e5f);
        const float target = GenF(r, s, S_ZERO | S_DEN | S_TINY), heading = GenF(r, s, S_NAN | S_ZERO | S_DEN | S_TINY), mult = r.below(3) ? 0.5f + r.f01() : GenF(r, 3.f);
        const float a = CCarCtrl::FindAngleToWeaveThroughTraffic(v, v, target, heading, mult);
        const float b = oracle::Fn<float __cdecl(CVehicle*, CPhysical*, float, float, float)>(0x4325C0)(v, v, target, heading, mult);
        Hit(a != target);
        if (SameF(a, b)) return true;
        d = "pos " + V(v->GetPosition()) + " target " + F(target) + " heading " + F(heading) + " mult " + F(mult) + " got " + F(a) + " exe " + F(b); return false;
    });

    // =============================================================================================================================
    // CPathFind: grid helpers
    Run("CPathFind::FindXRegionForCoors 0x44D890", [](Rng& r, std::string& d) {
        const float x = r.below(3) ? GenF(r, 3300.f) : GenF(r, PickScale(r));
        const size_t a = ThePaths.FindXRegionForCoors(x), b = oracle::Fn<size_t __fastcall(CPathFind*, int, float)>(0x44D890)(&ThePaths, 0, x);
        if (a == b) return true;
        d = "x " + F(x) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CPathFind::FindYRegionForCoors 0x44D8C0", [](Rng& r, std::string& d) {
        const float y = r.below(3) ? GenF(r, 3300.f) : GenF(r, PickScale(r));
        const size_t a = ThePaths.FindYRegionForCoors(y), b = oracle::Fn<size_t __fastcall(CPathFind*, int, float)>(0x44D8C0)(&ThePaths, 0, y);
        if (a == b) return true;
        d = "y " + F(y) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CPathFind::FindRegionForCoors 0x44D830", [](Rng& r, std::string& d) {
        const float x = GenF(r, r.below(3) ? 3300.f : PickScale(r)), y = GenF(r, r.below(3) ? 3300.f : PickScale(r));
        const size_t a = ThePaths.FindRegionForCoors({ x, y });
        const size_t b = oracle::Fn<size_t __fastcall(CPathFind*, int, float, float)>(0x44D830)(&ThePaths, 0, x, y);
        if (a == b) return true;
        d = "xy " + F(x) + "," + F(y) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CPathFind::FindXCoorsForRegion 0x44D8F0", [](Rng& r, std::string& d) {
        const size_t x = r.below(10);
        const float a = ThePaths.FindXCoorsForRegion(x), b = oracle::Fn<float __fastcall(CPathFind*, int, size_t)>(0x44D8F0)(&ThePaths, 0, x);
        if (SameF(a, b)) return true;
        d = "x " + std::to_string(x) + " got " + F(a) + " exe " + F(b); return false;
    }, 40);
    Run("CPathFind::FindYCoorsForRegion 0x44D910", [](Rng& r, std::string& d) {
        const size_t y = r.below(10);
        const float a = ThePaths.FindYCoorsForRegion(y), b = oracle::Fn<float __fastcall(CPathFind*, int, size_t)>(0x44D910)(&ThePaths, 0, y);
        if (SameF(a, b)) return true;
        d = "y " + std::to_string(y) + " got " + F(a) + " exe " + F(b); return false;
    }, 40);
    Run("CPathFind::FindStartPointOfRegion 0x44D930", [](Rng& r, std::string& d) {
        const size_t x = r.below(10), y = r.below(10);
        float ax = 7, ay = 7, bx = 8, by = 8;
        ThePaths.FindStartPointOfRegion(x, y, ax, ay);
        oracle::Fn<void __fastcall(CPathFind*, int, size_t, size_t, float*, float*)>(0x44D930)(&ThePaths, 0, x, y, &bx, &by);
        if (SameF(ax, bx) & SameF(ay, by)) return true;
        d = "xy " + std::to_string(x) + "," + std::to_string(y) + " got " + F(ax) + "," + F(ay) + " exe " + F(bx) + "," + F(by); return false;
    }, 100);

    // =============================================================================================================================
    // CPathFind on the fake node set
    Run("CPathFind::TakeWidthIntoAccountForWandering 0x4509A0", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 80);
        const CNodeAddress ad = RandAddr(r, 10);
        const int16 seed = r.below(4) ? (int16)r.below(256) : (int16)r.u32();
        const CVector a = ThePaths.TakeWidthIntoAccountForWandering(ad, seed);
        CVector b{ 9, 9, 9 };
        oracle::Fn<CVector* __fastcall(CPathFind*, int, CVector*, uint32, int)>(0x4509A0)(&ThePaths, 0, &b, Addr32(ad), (int)seed);
        Hit(a.x != 0.f);
        if (SameV(a, b)) return true;
        d = "node " + A(ad) + " seed " + std::to_string(seed) + " got " + V(a) + " exe " + V(b); return false;
    });
    Run("CPathFind::CalcDistToAnyConnectingLinks 0x44F190", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 80);
        const int ar = r.below(NAREA), nd = r.below(NNODE);
        CPathNode* node = &g_nodes[ar][nd];
        const CVector pos = r.below(3) ? GenWorldPos(r) : CVector{ node->GetPosition().x + GenF(r, 60.f), node->GetPosition().y + GenF(r, 60.f), node->GetPosition().z + GenF(r, 10.f) };
        const float a = ThePaths.CalcDistToAnyConnectingLinks(node, pos);
        const float b = oracle::Fn<float __fastcall(CPathFind*, int, CPathNode*, float, float, float)>(0x44F190)(&ThePaths, 0, node, pos.x, pos.y, pos.z);
        Hit(a < 1000.f);
        if (SameF(a, b)) return true;
        d = "node " + A({ (uint16)ar, (uint16)nd }) + " pos " + V(pos) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CPathFind::FindNodeClosestInRegion 0x44F2C0", [](Rng& r, std::string& d) {
        const uint64_t sd = r.u32();
        BuildPaths(sd, 90);
        const uint16 area = (uint16)r.below(NAREA);
        const CVector pos = GenWorldPos(r);
        const uint8 type = r.below(12) ? r.below(2) : 2;
        const bool low = r.below(2), unkn = r.below(2), boats = r.below(4) == 0;
        for (int i = 0; i < NNODE; ++i) if (r.below(3) == 0) reinterpret_cast<uint8*>(&g_nodes[area][i])[0x19] |= 2;   // unk1
        const float init = r.below(4) ? 20000.f * r.f01() : GenF(r, 500.f);
        CNodeAddress o1{ 0x1111, 0x2222 }, o2 = o1;
        float d1 = init, d2 = init;
        const uint32_t h0 = PathHash();
        ThePaths.FindNodeClosestInRegion(&o1, area, pos, type, &d1, low, unkn, boats, false);
        const uint32_t h1 = PathHash();
        oracle::Fn<void __fastcall(CPathFind*, int, CNodeAddress*, uint32, float, float, float, uint32, float*, uint32, uint32, uint32, uint32)>(0x44F2C0)(&ThePaths, 0, &o2, area, pos.x, pos.y, pos.z, type, &d2, low, unkn, boats, 0);
        Hit(o1.m_wAreaId != 0x1111);
        if (!std::memcmp(&o1, &o2, 4) && SameF(d1, d2) && h0 == h1 && h1 == PathHash()) return true;
        d = "area " + std::to_string(area) + " pos " + V(pos) + " type " + std::to_string(type) + " init " + F(init) + " got " + A(o1) + " " + F(d1) + " exe " + A(o2) + " " + F(d2); return false;
    });
    Run("CPathFind::FindNodeClosestToCoors 0x44F460", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 85);
        const CVector pos = GenWorldPos(r);
        const ePathType type = (ePathType)(r.below(10) ? r.below(2) : 2);
        const float maxD = r.below(5) ? (float)(10 + r.below(r.below(2) ? 800 : 9990)) : GenF(r, 2000.f);
        const uint16 low = r.below(2), unkn = r.below(2), boats = r.below(4) == 0;
        const int32 u6 = r.below(2);
        const CNodeAddress a = ThePaths.FindNodeClosestToCoors(pos, type, maxD, low, 0, unkn, boats, u6);
        CNodeAddress b{ 0x5555, 0x6666 };
        oracle::Fn<CNodeAddress* __fastcall(CPathFind*, int, CNodeAddress*, float, float, float, uint32, float, uint32, uint32, uint32, uint32, uint32)>(0x44F460)(&ThePaths, 0, &b, pos.x, pos.y, pos.z, type, maxD, low, 0, unkn, boats, u6);
        Hit(a.m_wAreaId != 0xFFFF);
        // the node id of "nothing found" is garbage in the original (only the area id is set)
        if (a.m_wAreaId == b.m_wAreaId && (a.m_wAreaId == 0xFFFF || a.m_wNodeId == b.m_wNodeId)) return true;
        d = "pos " + V(pos) + " type " + std::to_string(type) + " max " + F(maxD) + " low " + std::to_string(low) + " got " + A(a) + " exe " + A(b); return false;
    });
    Run("CPathFind::FindNodeClosestToCoorsFavourDirection 0x44FCE0", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 85);
        const CVector pos = GenWorldPos(r);
        const ePathType type = (ePathType)r.below(2);
        CVector2D dir{ GenF(r, 1.f), GenF(r, 1.f) };
        const CNodeAddress a = ThePaths.FindNodeClosestToCoorsFavourDirection(pos, type, dir);
        CNodeAddress b{ 0x5555, 0x6666 };
        oracle::Fn<CNodeAddress* __fastcall(CPathFind*, int, CNodeAddress*, float, float, float, uint32, float, float)>(0x44FCE0)(&ThePaths, 0, &b, pos.x, pos.y, pos.z, type, dir.x, dir.y);
        Hit(a.m_wAreaId != 0xFFFF);
        if (a.m_wAreaId == b.m_wAreaId && (a.m_wAreaId == 0xFFFF || a.m_wNodeId == b.m_wNodeId)) return true;   // (the node id of "nothing found" is garbage in the original)
        d = "pos " + V(pos) + " type " + std::to_string(type) + " dir " + F(dir.x) + "," + F(dir.y) + " got " + A(a) + " exe " + A(b); return false;
    });
    Run("CPathFind::FindNthNodeClosestToCoors 0x44F8C0", [](Rng& r, std::string& d) {
        const uint64_t sd = r.u32();
        const CVector pos = GenWorldPos(r);
        const uint8 type = r.below(10) ? r.below(2) : 2;
        const float maxD = r.below(5) ? (float)(10 + r.below(r.below(2) ? 800 : 9990)) : GenF(r, 2000.f);
        const bool low = r.below(2), unkn = r.below(2), boats = r.below(4) == 0, ign = r.below(2);
        const int nth = r.below(4);
        if (type > 1 && nth) return true;   // the original writes out of bounds there (see the port's BUG comment)
        BuildPaths(sd, 90);
        CNodeAddress o1{ 0x7777, 0x8888 }, o2 = o1;
        const CNodeAddress a = ThePaths.FindNthNodeClosestToCoors(pos, type, maxD, low, unkn, nth, boats, ign, &o1);
        const uint32_t h1 = PathHash();
        BuildPaths(sd, 90);
        CNodeAddress b{ 0x5555, 0x6666 };
        oracle::Fn<CNodeAddress* __fastcall(CPathFind*, int, CNodeAddress*, float, float, float, uint32, float, uint32, uint32, int, uint32, uint32, CNodeAddress*)>(0x44F8C0)(&ThePaths, 0, &b, pos.x, pos.y, pos.z, type, maxD, low, unkn, nth, boats, ign, &o2);
        const uint32_t h2 = PathHash();
        Hit(a.m_wAreaId != 0xFFFF);
        if (a.m_wAreaId == b.m_wAreaId && (a.m_wAreaId == 0xFFFF || a.m_wNodeId == b.m_wNodeId) && o1.m_wAreaId == o2.m_wAreaId && (o1.m_wAreaId == 0xFFFF || o1.m_wNodeId == o2.m_wNodeId) && h1 == h2) return true;
        d = "pos " + V(pos) + " type " + std::to_string(type) + " nth " + std::to_string(nth) + " got " + A(a) + " out " + A(o1) + " exe " + A(b) + " out " + A(o2) + " hash " + std::to_string(h1 == h2); return false;
    });
    Run("CPathFind::CalcRoadDensity 0x44EFC0", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 85, 0.5f);
        const float x = r.below(4) ? (float)(-3000 + (int)r.below(6000)) : GenF(r, 3500.f, S_NAN | S_ZERO), y = r.below(4) ? (float)(-3000 + (int)r.below(6000)) : GenF(r, 3500.f, S_NAN | S_ZERO);
        const float a = ThePaths.CalcRoadDensity(x, y), b = oracle::Fn<float __fastcall(CPathFind*, int, float, float)>(0x44EFC0)(&ThePaths, 0, x, y);
        Hit(a != 0.f);
        if (SameF(a, b)) return true;
        d = "xy " + F(x) + "," + F(y) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CPathFind::FindNodeCoorsForScript(LinkedNode) 0x4505E0", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 85, 0.3f);
        const CNodeAddress ad = RandAddr(r, 10);
        // the original does not look at the area of the linked node (index 0xFFFF reads far outside of the path table)
        if (ad.m_wAreaId != 0xFFFF && ThePaths.m_pPathNodes[ad.m_wAreaId]) {
            const auto& nd = g_nodes[ad.m_wAreaId][ad.m_wNodeId];
            if (nd.m_nNumLinks && g_links[ad.m_wAreaId][nd.m_wBaseLinkId].m_wAreaId >= NAREA) return true;
        }
        bool f1 = false, f2 = false; const CVector a = ThePaths.FindNodeCoorsForScript(ad, &f1);
        CVector b{ 9, 9, 9 };
        oracle::Fn<CVector* __fastcall(CPathFind*, int, CVector*, uint32, bool*)>(0x4505E0)(&ThePaths, 0, &b, Addr32(ad), &f2);
        Hit(f1 && a.x != 0.f);
        if (SameV(a, b) && f1 == f2) return true;
        d = "node " + A(ad) + " got " + V(a) + " " + std::to_string(f1) + " exe " + V(b) + " " + std::to_string(f2); return false;
    });
    Run("CPathFind::FindNodeCoorsForScript(TwoNodes) 0x450780", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 85, 0.3f);
        CNodeAddress n1 = RandAddr(r, 8), n2 = RandAddr(r, 8);
        if (r.below(8) == 0) n2 = n1;
        float h1 = BitsF(0xDEADBEEF), h2 = h1; bool f1 = false, f2 = false;
        const CVector a = ThePaths.FindNodeCoorsForScript(n1, n2, h1, &f1);
        CVector b{ 9, 9, 9 };
        oracle::Fn<CVector* __fastcall(CPathFind*, int, CVector*, uint32, uint32, float*, bool*)>(0x450780)(&ThePaths, 0, &b, Addr32(n1), Addr32(n2), &h2, &f2);
        Hit(f1 && a.x != 0.f);
        if (SameV(a, b) && SameF(h1, h2) && f1 == f2) return true;
        d = "nodes " + A(n1) + " " + A(n2) + " got " + V(a) + " " + F(h1) + " " + std::to_string(f1) + " exe " + V(b) + " " + F(h2) + " " + std::to_string(f2); return false;
    });
    Run("CPathFind::IsWaterNodeNearby 0x450DE0", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 85, 0.3f);
        const CVector pos = GenWorldPos(r);
        const float radius = r.below(4) ? (float)r.below(900) : GenF(r, 3000.f, S_NAN | S_ZERO | S_TINY);
        const bool a = ThePaths.IsWaterNodeNearby(pos, radius), b = oracle::Fn<bool __fastcall(CPathFind*, int, float, float, float, float)>(0x450DE0)(&ThePaths, 0, pos.x, pos.y, pos.z, radius);
        Hit(a);
        if (a == b) return true;
        d = "pos " + V(pos) + " r " + F(radius) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CPathFind::TestForPedTrafficLight 0x44D480", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 100);
        const CNodeAddress n1{ (uint16)r.below(NAREA), (uint16)r.below(NNODE) };
        CNodeAddress n2{ (uint16)r.below(NAREA), (uint16)r.below(NNODE) };
        if (r.below(2)) { const auto& nd = g_nodes[n1.m_wAreaId][n1.m_wNodeId]; if (nd.m_nNumLinks) n2 = g_links[n1.m_wAreaId][nd.m_wBaseLinkId + r.below(nd.m_nNumLinks)]; }
        const bool a = ThePaths.TestForPedTrafficLight(n1, n2), b = oracle::Fn<bool __fastcall(CPathFind*, int, uint32, uint32)>(0x44D480)(&ThePaths, 0, Addr32(n1), Addr32(n2));
        Hit(a);
        if (a == b) return true;
        d = "nodes " + A(n1) + " " + A(n2) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CPathFind::GetPathNode 0x420AC0", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 100);
        const CNodeAddress ad{ (uint16)r.below(NAREA), (uint16)r.below(NNODE) };
        const CPathNode* a = ThePaths.GetPathNode(ad);
        const CPathNode* b = oracle::Fn<CPathNode* __fastcall(CPathFind*, int, uint32)>(0x420AC0)(&ThePaths, 0, Addr32(ad));
        if (a == b) return true;
        d = "node " + A(ad) + " got " + std::to_string((uintptr_t)a) + " exe " + std::to_string((uintptr_t)b); return false;
    }, 200);
    Run("CPathFind::RecordNodesClosestToCoors 0x44FA30", [](Rng& r, std::string& d) {
        const uint64_t sd = r.u32();
        const CVector pos = GenWorldPos(r);
        const uint8 type = r.below(10) ? r.below(2) : 2;
        const int count = r.below(6);
        const float maxD = r.below(5) ? (float)(10 + r.below(r.below(2) ? 800 : 9990)) : GenF(r, 2000.f);
        const bool u2 = r.below(2), u3 = r.below(2), boats = r.below(4) == 0, u6 = r.below(2);
        CNodeAddress o1[6], o2[6];
        for (int i = 0; i < 6; ++i) o1[i] = o2[i] = CNodeAddress{ 0x7777, 0x8888 };
        BuildPaths(sd, 90);
        ThePaths.RecordNodesClosestToCoors(pos, type, count, o1, maxD, u2, u3, boats, u6);
        const uint32_t h1 = PathHash();
        BuildPaths(sd, 90);
        oracle::Fn<void __fastcall(CPathFind*, int, float, float, float, uint32, int, CNodeAddress*, float, uint32, uint32, uint32, uint32)>(0x44FA30)(&ThePaths, 0, pos.x, pos.y, pos.z, type, count, o2, maxD, u2, u3, boats, u6);
        const uint32_t h2 = PathHash();
        Hit(o1[0].m_wAreaId != 0x7777);
        if (!std::memcmp(o1, o2, sizeof o1) && h1 == h2) return true;
        d = "pos " + V(pos) + " type " + std::to_string(type) + " count " + std::to_string(count) + " got " + A(o1[0]) + "," + A(o1[1]) + "," + A(o1[2]) + " exe " + A(o2[0]) + "," + A(o2[1]) + "," + A(o2[2]) + " hash " + std::to_string(h1 == h2); return false;
    });
    Run("CPathFind::Find2NodesForCarCreation 0x452090", [](Rng& r, std::string& d) {
        const uint64_t sd = r.u32();
        const CVector pos = GenWorldPos(r);
        const bool low = r.below(2);
        CNodeAddress a1{ 0x7777, 0x8888 }, a2 = a1, b1 = a1, b2 = a1;
        BuildPaths(sd, 90);
        static std::string where; where = "Find2Nodes seed " + std::to_string(sd) + " pos " + V(pos) + " low " + std::to_string(low) + " PORT"; oracle::g_where = where.c_str();
        ThePaths.Find2NodesForCarCreation(pos, &a1, &a2, low);
        where = "Find2Nodes seed " + std::to_string(sd) + " pos " + V(pos) + " low " + std::to_string(low) + " EXE";
        const uint32_t h1 = PathHash();
        BuildPaths(sd, 90);
        oracle::Fn<void __fastcall(CPathFind*, int, float, float, float, CNodeAddress*, CNodeAddress*, uint32)>(0x452090)(&ThePaths, 0, pos.x, pos.y, pos.z, &b1, &b2, low);
        const uint32_t h2 = PathHash();
        Hit(a2.m_wAreaId != 0x7777);
        if (!std::memcmp(&a1, &b1, 4) && !std::memcmp(&a2, &b2, 4) && h1 == h2) return true;
        d = "pos " + V(pos) + " low " + std::to_string(low) + " got " + A(a1) + "," + A(a2) + " exe " + A(b1) + "," + A(b2) + " hash " + std::to_string(h1 == h2); return false;
    });
    Run("CPathFind::FindParkingNodeInArea 0x4513F0", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 90);
        // parking nodes (behaviour type 2) in some of the nodes
        for (int a = 0; a < NAREA; ++a) for (int n = 0; n < NNODE; ++n) if (r.below(3) == 0) reinterpret_cast<uint8*>(&g_nodes[a][n])[0x1A] = (uint8)((reinterpret_cast<uint8*>(&g_nodes[a][n])[0x1A] & 0x0F) | 0x20);
        const float cx = (float)(-3000 + (int)r.below(6000)), cy = (float)(-3000 + (int)r.below(6000)), hs = r.below(3) ? (float)(200 + r.below(3000)) : GenF(r, 3000.f, S_NAN | S_ZERO);
        const float lim[6] = { cx - hs, cx + hs, cy - hs, cy + hs, r.below(4) ? -100.f : GenF(r, 100.f), r.below(4) ? 100.f : GenF(r, 100.f) };
        int32& idx = *reinterpret_cast<int32*>(0x977B9C);
        const int32 i0 = r.below(5); idx = i0;
        const CVector a = ThePaths.FindParkingNodeInArea(lim[0], lim[1], lim[2], lim[3], lim[4], lim[5]);
        const int32 i1 = idx; idx = i0;
        CVector b{ 9, 9, 9 };
        oracle::Fn<CVector* __fastcall(CPathFind*, int, CVector*, float, float, float, float, float, float)>(0x4513F0)(&ThePaths, 0, &b, lim[0], lim[1], lim[2], lim[3], lim[4], lim[5]);
        Hit(a.x != 0.f || a.y != 0.f);
        if (SameV(a, b) && i1 == idx) return true;
        d = "lim " + F(lim[0]) + "," + F(lim[1]) + "," + F(lim[2]) + "," + F(lim[3]) + "," + F(lim[4]) + "," + F(lim[5]) + " idx0 " + std::to_string(i0) + " got " + V(a) + " " + std::to_string(i1) + " exe " + V(b) + " " + std::to_string(idx); return false;
    });
    Run("CPathFind::MarkRoadNodeAsDontWander 0x450560", [](Rng& r, std::string& d) {
        const uint64_t sd = r.u32();
        const CVector pos = GenWorldPos(r);
        BuildPaths(sd, 90);
        ThePaths.MarkRoadNodeAsDontWander(pos.x, pos.y, pos.z);
        const uint32_t h1 = PathHash();
        BuildPaths(sd, 90);
        oracle::Fn<void __fastcall(CPathFind*, int, float, float, float)>(0x450560)(&ThePaths, 0, pos.x, pos.y, pos.z);
        const uint32_t h2 = PathHash();
        Hit(h1 != 0);
        if (h1 == h2) return true;
        d = "pos " + V(pos) + " hashes differ"; return false;
    });
    Run("CPathFind::FindNodeOrientationForCarPlacement 0x450320", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 90);
        const CNodeAddress ad{ (uint16)r.below(NUM_PATH_MAP_AREAS), (uint16)r.below(NNODE) };   // (the navi link table only exists for the 64 map areas)
        const float a = ThePaths.FindNodeOrientationForCarPlacement(ad), b = oracle::Fn<float __fastcall(CPathFind*, int, uint32)>(0x450320)(&ThePaths, 0, Addr32(ad));
        Hit(a != 0.f);
        if (SameF(a, b)) return true;
        d = "node " + A(ad) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CPathFind::FindPedCreationPosBetweenNodes 0x44DA30", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 85);
        const CNodeAddress n1 = RandAddr(r, 6), n2 = RandAddr(r, 6);
        const int32 seed = r.below(3) ? (int32)r.below(65536) : (int32)r.u32();
        float x1 = GenF(r, 100.f), y1 = GenF(r, 100.f), x2 = x1, y2 = y1;
        ThePaths.FindPedCreationPosBetweenNodes(n1, n2, seed, &x1, &y1);
        oracle::Fn<void __fastcall(CPathFind*, int, uint32, uint32, int, float*, float*)>(0x44DA30)(&ThePaths, 0, Addr32(n1), Addr32(n2), seed, &x2, &y2);
        if (SameF(x1, x2) & SameF(y1, y2)) return true;
        d = "nodes " + A(n1) + " " + A(n2) + " seed " + std::to_string(seed) + " got " + F(x1) + "," + F(y1) + " exe " + F(x2) + "," + F(y2); return false;
    });
    Run("CPathFind::These2NodesAreAdjacent 0x44D230", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 100);
        const CNodeAddress n1{ (uint16)r.below(NAREA), (uint16)r.below(NNODE) };
        CNodeAddress n2{ (uint16)r.below(NAREA), (uint16)r.below(NNODE) };
        if (r.below(2)) { const auto& nd = g_nodes[n1.m_wAreaId][n1.m_wNodeId]; if (nd.m_nNumLinks) n2 = g_links[n1.m_wAreaId][nd.m_wBaseLinkId + r.below(nd.m_nNumLinks)]; }
        const bool a = ThePaths.These2NodesAreAdjacent(n1, n2), b = oracle::Fn<bool __fastcall(CPathFind*, int, uint32, uint32)>(0x44D230)(&ThePaths, 0, Addr32(n1), Addr32(n2));
        Hit(a);
        if (a == b) return true;
        d = "nodes " + A(n1) + " " + A(n2) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CPathFind::FindLinkBetweenNodes 0x451350", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 90);
        const CNodeAddress n1{ (uint16)r.below(NUM_PATH_MAP_AREAS), (uint16)r.below(NNODE) };
        // NOTE: the original searches the link table of the first node without a bound (UB / a crash when the nodes are not adjacent): only adjacent pairs, or unloaded areas
        CNodeAddress n2{ (uint16)r.below(NAREA), (uint16)r.below(NNODE) };
        { const auto& nd = g_nodes[n1.m_wAreaId][n1.m_wNodeId]; if (nd.m_nNumLinks && r.below(8)) n2 = g_links[n1.m_wAreaId][nd.m_wBaseLinkId + r.below(nd.m_nNumLinks)]; else n2.m_wAreaId = (uint16)(r.below(2) ? 0xFFFF : n2.m_wAreaId); }
        if (n2.m_wAreaId != 0xFFFF && ThePaths.m_pPathNodes[n2.m_wAreaId] && ThePaths.m_pPathNodes[n1.m_wAreaId]) {   // both loaded: must be adjacent
            const auto& nd = g_nodes[n1.m_wAreaId][n1.m_wNodeId]; bool adj = false;
            for (uint32 i = 0; i < nd.m_nNumLinks; ++i) adj |= g_links[n1.m_wAreaId][nd.m_wBaseLinkId + i] == n2;
            if (!adj) return true;
        }
        const CCarPathLinkAddress a = ThePaths.FindLinkBetweenNodes(n1, n2);
        CCarPathLinkAddress b;
        oracle::Fn<void* __fastcall(CPathFind*, int, CCarPathLinkAddress*, uint32, uint32)>(0x451350)(&ThePaths, 0, &b, Addr32(n1), Addr32(n2));
        Hit(*(uint16*)&a != 0xFFFF);
        if (!std::memcmp(&a, &b, 2)) return true;
        d = "nodes " + A(n1) + " " + A(n2) + " got " + std::to_string(*(uint16*)&a) + " exe " + std::to_string(*(uint16*)&b); return false;
    });
    Run("CPathFind::TestCrossesRoad 0x44D790", [](Rng& r, std::string& d) {
        BuildPaths(r.u32(), 100);
        const CNodeAddress n1{ (uint16)r.below(NAREA), (uint16)r.below(NNODE) };
        CNodeAddress n2{ (uint16)r.below(NAREA), (uint16)r.below(NNODE) };
        if (r.below(2)) { const auto& nd = g_nodes[n1.m_wAreaId][n1.m_wNodeId]; if (nd.m_nNumLinks) n2 = g_links[n1.m_wAreaId][nd.m_wBaseLinkId + r.below(nd.m_nNumLinks)]; }
        const bool a = ThePaths.TestCrossesRoad(n1, n2), b = oracle::Fn<bool __fastcall(CPathFind*, int, uint32, uint32)>(0x44D790)(&ThePaths, 0, Addr32(n1), Addr32(n2));
        Hit(a);
        if (a == b) return true;
        d = "nodes " + A(n1) + " " + A(n2) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
}
