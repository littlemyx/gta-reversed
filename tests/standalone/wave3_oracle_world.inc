// ORACLE-W3-WORLD: exe-oracle rows for the world / physics root files (World.cpp, World_Helper.cpp, PathFind.cpp, TheZones.cpp, CullZones.cpp, Timer.cpp, TimeCycle.cpp, common.cpp, GridRef.cpp, Draw.cpp ...)
// Included by wave2_oracle_all.inc when W3_WORLD_ONLY is defined (executable wave3_world_oracle_test = the W2 harness: deterministic arena, whole exe data image + the scratch block compared after every case).
//
// Fixture: the globals under test live at their exe addresses on both sides (NOTSA_ORACLE_ADDRESS_GLOBALS): CWorld::Players, TheCamera, ThePaths, the zone / cull-zone tables ...; a scratch block g_sc holds fake peds /
// vehicles (garbage + the fields the code reads), matrices and out parameters. Every case rebuilds the fixture from the seed; both sides see identical memory; the image + scratch are compared after the call.
#include "World.h"
#include "PlayerInfo.h"
#include "Camera.h"
#include "CarCtrl.h"
#include "Entity/Ped/Ped.h"
#include "Entity/Ped/PlayerPed.h"
#include "Entity/Vehicle/Vehicle.h"
#include "PathFind.h"
#include "TheZones.h"
#include "CullZones.h"
#include "TimeCycle.h"
#include "Timer.h"

namespace w3w {
using namespace w2;
constexpr size_t kScSize = 0x20000;
alignas(16) static uint8_t g_sc[kScSize];
constexpr size_t oPed = 0x0 /*4 * 0x800*/, oVeh = 0x2000 /*4 * 0x800*/, oMat = 0x4000 /*16 * 0x60*/, oOut = 0x4800 /*0x400*/, oBuf = 0x5000;
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& At(uintptr_t va) { return *reinterpret_cast<T*>(va); }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
static uint32_t P32(const void* p) { return (uint32_t)(uintptr_t)p; }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static inline uint64_t B8(bool b) { return b ? 1 : 0; }
static inline uint64_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
// a returned pointer as a comparable value: the offset inside the scratch block / TheCamera, 0 for null, else (a static zero vector of the port vs the exe's own) the 3 floats' bits folded
static uint64_t PtrId(const void* p) {
    const uintptr_t a = (uintptr_t)p;
    if (!a) return 0;
    if (a >= (uintptr_t)g_sc && a < (uintptr_t)g_sc + kScSize) return 0x10000000ull + (a - (uintptr_t)g_sc);
    if (a >= 0xB6F028 && a < 0xB6F028 + 0x1000) return 0x20000000ull + (a - 0xB6F028);
    uint32_t v[3]; std::memcpy(v, p, 12);
    return 0x30000000ull ^ ((uint64_t)v[0] * 31 + v[1]) * 31 + v[2];
}
static CPed* PedP(int k) { return reinterpret_cast<CPed*>(S(oPed + (size_t)k * 0x800)); }
static CVehicle* VehP(int k) { return reinterpret_cast<CVehicle*>(S(oVeh + (size_t)k * 0x800)); }
static uint8_t* MatP(int k) { return S(oMat + (size_t)k * 0x60); }

// path nodes: ThePaths (0x96F050) points into this block; 72 areas (64 map areas + 8 interior areas), 10 nodes and 40 link entries each (the last 16 link entries are free: area 0xFFFF)
constexpr int NAREA = 72, NNODE = 10, NLINK = 40, NLINK_USED = 24;
struct PfFx {
    uint8_t nodes[NAREA][NNODE][0x1C];
    uint32_t links[NAREA][NLINK];
    uint8_t navi[NAREA][NLINK][0xE];
    uint16_t naviLinks[NAREA][NLINK];
    uint8_t linkLen[NAREA][NLINK];
    uint8_t inter[NAREA][NLINK];
};
alignas(16) static PfFx g_pf;
static void PutI16(uint8_t* base, size_t off, int v) { int16_t x = (int16_t)v; std::memcpy(base + off, &x, 2); }
static uint8_t* Node(int a, int n) { return g_pf.nodes[a][n]; }
static uint32_t NA(int area, int node) { return (uint32_t)(uint16_t)area | ((uint32_t)(uint16_t)node << 16); }
static CPathNode* NodeP(int a, int n) { return reinterpret_cast<CPathNode*>(Node(a, n)); }
static void BuildPaths(Rng& r, int loadedPct = 85) {
    std::memset(&ThePaths, 0, sizeof(CPathFind));
    auto& P = ThePaths;
    for (int a = 0; a < NAREA; ++a) {
        const bool interior = a >= 64;
        P.m_pPathNodes[a] = ((int)r.below(100) < loadedPct) ? NodeP(a, 0) : nullptr;
        P.m_pNaviNodes[a] = reinterpret_cast<CCarPathLink*>(g_pf.navi[a]);
        P.m_pNodeLinks[a] = reinterpret_cast<CNodeAddress*>(g_pf.links[a]);
        P.m_pLinkLengths[a] = g_pf.linkLen[a];
        P.m_pPathIntersections[a] = reinterpret_cast<CPathIntersectionInfo*>(g_pf.inter[a]);
        if (a < 64) P.m_pNaviLinks[a] = reinterpret_cast<CCarPathLinkAddress*>(g_pf.naviLinks[a]);
        P.m_anNumNodes[a] = NNODE;
        const uint32_t nv = r.below(NNODE + 1);
        P.m_anNumVehicleNodes[a] = nv; P.m_anNumPedNodes[a] = NNODE - nv;
        P.m_anNumCarPathLinks[a] = NLINK_USED; P.m_anNumAddresses[a] = NLINK_USED;
        const int cx = a % 8, cy = (a / 8) % 8;
        for (int n = 0; n < NNODE; ++n) {
            uint8_t* raw = Node(a, n);
            std::memset(raw, 0, 0x1C);
            const bool inCell = r.below(6) != 0;
            const float x0 = -3000.f + 750.f * cx, y0 = -3000.f + 750.f * cy;
            const float x = inCell ? x0 + r.f01() * 750.f : (r.f01() * 2 - 1) * 3600.f, y = inCell ? y0 + r.f01() * 750.f : (r.f01() * 2 - 1) * 3600.f;
            PutI16(raw, 8, (int)(x * 8.f)); PutI16(raw, 10, (int)(y * 8.f)); PutI16(raw, 12, (int)((r.f01() * 2 - 1) * 80.f * 8.f));
            PutI16(raw, 0xE, (int)r.below(0x7FFE));                  // m_nTotal (path search distance)
            PutI16(raw, 0x10, (int)r.below(NLINK_USED - 15));       // base link id
            PutI16(raw, 0x12, a); PutI16(raw, 0x14, n);
            raw[0x16] = (uint8_t)r.u32();
            raw[0x17] = (uint8_t)r.below(4);                         // flood fill group
            raw[0x18] = (uint8_t)((r.below(7)) | (r.below(5) == 0 ? 0x20 : 0) | (r.below(7) == 0 ? 0x80 : 0) | (r.u32() & 0x50));
            raw[0x19] = (uint8_t)(r.u32() & (r.below(3) ? 0x3F : 0xFF));
            raw[0x1A] = (uint8_t)(r.below(3) ? (r.below(12) << 4) | (r.u32() & 0xF) : r.u32());   // behaviour in the high nibble
            if (interior) { PutI16(raw, 0x12, a); }
        }
        for (int l = 0; l < NLINK; ++l) {
            g_pf.links[a][l] = l < NLINK_USED ? NA((int)r.below(r.below(8) ? 64 : NAREA), (int)r.below(NNODE)) : 0x0000FFFFu;
            uint8_t* nv2 = g_pf.navi[a][l];
            std::memset(nv2, 0, 0xE);
            PutI16(nv2, 0, (int)((r.f01() * 2 - 1) * 3600.f * 8.f)); PutI16(nv2, 2, (int)((r.f01() * 2 - 1) * 3600.f * 8.f));
            const uint32_t att = NA((int)r.below(NAREA), (int)r.below(NNODE)); std::memcpy(nv2 + 4, &att, 4);
            for (int k = 8; k < 13; ++k) nv2[k] = (uint8_t)r.u32();
            g_pf.naviLinks[a][l] = (uint16_t)((r.below(64) & 0x3F) << 10 | r.below(NLINK));
            g_pf.linkLen[a][l] = (uint8_t)r.u32();
            g_pf.inter[a][l] = (uint8_t)(r.u32() & 3);
        }
    }
    for (int k = 0; k < NUM_PATH_INTERIOR_AREAS; ++k) P.m_interiorIDs[k] = r.below(3) ? 0xFFFFFFFFu : (uint32_t)r.below(4) | ((uint32_t)r.below(5) << 16);
}

static std::function<void(Rng&)> g_tweak;     // row-specific fixture work after the standard build
static float Fl(Rng& r, float scale, int special = 14) {
    if (special && r.below((uint32)special) == 0) return GenF(r, scale, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF);
    return GF(r, scale);
}
static float FlP(Rng& r, float scale) { return GF(r, scale); }

// players, their peds / vehicles / matrices and the camera
static void BuildPlayers(Rng& r) {
    FillGarbage(r, S(0), 0x4800);
    for (int k = 0; k < 16; ++k) {   // plausible matrices: finite axes and positions
        uint8_t* m = MatP(k);
        float* f = reinterpret_cast<float*>(m);
        for (int i = 0; i < 24; ++i) f[i] = FlP(r, i % 4 == 3 ? 1.f : 50.f);
        const CVector pos(FlP(r, 3000.f), FlP(r, 3000.f), FlP(r, 100.f)); std::memcpy(m + 0x30, &pos, 12);
    }
    for (int k = 0; k < 8; ++k) {   // 0..3 peds, 4..7 vehicles
        uint8_t* e = k < 4 ? reinterpret_cast<uint8_t*>(PedP(k)) : reinterpret_cast<uint8_t*>(VehP(k - 4));
        const uint32_t mp = r.below(2) ? P32(MatP(k)) : 0; std::memcpy(e + offsetof(CPlaceable, m_matrix), &mp, 4);
        const CVector pp(FlP(r, 3000.f), FlP(r, 3000.f), FlP(r, 100.f)); std::memcpy(e + 4, &pp, 12);
        const float hd = FlP(r, 7.f); std::memcpy(e + 0x10, &hd, 4);
        const CVector sp(FlP(r, 2.f), FlP(r, 2.f), FlP(r, 2.f)); std::memcpy(e + offsetof(CPhysical, m_vecMoveSpeed), &sp, 12);
    }
    for (int k = 0; k < 4; ++k) {
        CPed* p = PedP(k);
        p->bInVehicle = r.below(3) != 0;
        p->m_pVehicle = r.below(6) == 0 ? nullptr : VehP(r.below(4));
    }
    for (int k = 0; k < 4; ++k) {
        CVehicle* v = VehP(k);
        v->m_nVehicleType = (eVehicleType)(r.below(3) == 0 ? 6 : r.below(11));
    }
    for (int k = 0; k < 2; ++k) {
        auto& pl = CWorld::Players[k];
        pl.m_pPed = r.below(8) == 0 ? nullptr : reinterpret_cast<CPlayerPed*>(PedP(r.below(4)));
        pl.m_pRemoteVehicle = r.below(4) == 0 ? VehP(r.below(4)) : nullptr;
    }
    CWorld::PlayerInFocus = (uint8)r.below(2);
    CCarCtrl::bCarsGeneratedAroundCamera = r.below(3) == 0;
    TheCamera.m_matrix = r.below(2) ? reinterpret_cast<CMatrixLink*>(MatP(8 + r.below(8))) : nullptr;
    TheCamera.m_placement.m_vPosn = CVector(FlP(r, 3000.f), FlP(r, 3000.f), FlP(r, 100.f));
    std::memset(reinterpret_cast<void*>(0xB7CCB8), 0, 0x190);   // Players[-1] (the exe reads Players[playerId].m_pRemoteVehicle with playerId = -1 unnormalised)
}

static void BuildAll(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    BuildPlayers(r);
    BuildPaths(r);
    if (g_tweak) g_tweak(r);
}

template<class PF, class EF>
static bool WCase(Rng& r, std::string& d, PF&& pf, EF&& ef) {
    const uint32_t seed = r.u32() | 1;
    BuildAll(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    Reg img{ kImgLo, kImgHi - kImgLo };
    Reg pfr{ (uintptr_t)&g_pf, sizeof g_pf };
    return BothA(d, seed, { sc, img, pfr }, std::forward<PF>(pf), std::forward<EF>(ef));
}
} // namespace w3w

struct Tw { explicit Tw(std::function<void(Rng&)> f) { w3w::g_tweak = std::move(f); } ~Tw() { w3w::g_tweak = nullptr; } };
template<class Fn> static void RunScaledW(const std::string& name, Fn&& fn, int cases) {
    static const int scale = std::getenv("W3W_SCALE") ? std::max(1, std::atoi(std::getenv("W3W_SCALE"))) : 1;
    Run(name, std::forward<Fn>(fn), cases * scale);
}
#define ROW(NAME, ADDR, CASES) { const int _cs = (CASES); const std::string _nm = std::string(NAME " " #ADDR); RunScaledW(_nm, [&](Rng& r, std::string& d) -> bool
#define ENDROW , _cs); }
#define IC(PORT, EXE) return w3w::WCase(r, d, [&]() -> uint64_t { PORT }, [&]() -> uint64_t { EXE })
#include "wave3_oracle_world_a.inc"      // World.cpp / World_Helper.cpp
#include "wave3_oracle_world_b.inc"      // PathFind.cpp
#include "wave3_oracle_world_c.inc"      // zones / cull zones / timer / time cycle / common / grid ref / draw

static void TestW3WorldAll() {
    TestW3WorldA();
    TestW3WorldB();
    TestW3WorldC();
}
