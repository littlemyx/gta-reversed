// ORACLE-W3-CAMERA: exe-oracle rows for the uncovered class A/B functions of the camera root files (Camera.cpp, Cam.cpp, IdleCam.cpp) on a real CCamera / CCam fixture.
// Included by wave2_oracle_all.inc when W3_CAMERA_ONLY is defined (executable wave3_camera_oracle_test = the W2 harness: deterministic arena, whole exe data image + the scratch block compared after every case).
//
// Fixture: TheCamera at its exe address (0xB6F028, same address on both sides: NOTSA_ORACLE_ADDRESS_GLOBALS) filled with garbage and then given plausible selectors (active cam, modes, flags) and pointers; gIdleCam likewise.
// A scratch block g_sc holds two player peds (CWorld::Players[0/1] + PlayerInFocus, state / vehicle / flags), two vehicles (handling data behind m_pHandlingData, model info + collision model behind the exe's
// ms_modelInfoPtrs 0xA9B0C8), two objects, matrices behind every entity, argument vectors / out parameters and a spline data block. All calls are qualified or non-virtual.
#include "Camera.h"
#include "Cam.h"
#include "IdleCam.h"
#include "World.h"
#include "PlayerInfo.h"
#include "References.h"
#include "Timer.h"
#include "Draw.h"
#include "General.h"
#include "Entity/Ped/Ped.h"
#include "Entity/Ped/PlayerPed.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Models/ModelInfo.h"
#include "Models/BaseModelInfo.h"
#include "Collision/ColModel.h"
#include "wave3_oracle_camera_bools.inc"   // GENERATED: the bool members of CCamera / CCam / CIdleCam

namespace w3c {
using namespace w2;
constexpr uintptr_t CAM = 0xB6F028, IDLE = 0;   // IDLE is filled in at run time from &gIdleCam
constexpr size_t kCamSize = 0xD78, kCamOne = 0x238, kCamFirst = 0x174;
constexpr size_t kScSize = 0x6000;
alignas(16) static uint8_t g_sc[kScSize];
// scratch layout
constexpr size_t oPed = 0x0000 /*2 * 0x800*/, oVeh = 0x1000 /*2 * 0xA80*/, oObj = 0x2500 /*2 * 0x400*/, oMat = 0x2D00 /*8 * 0x60*/, oMi = 0x3000 /*6 * 0x310*/, oCol = 0x4260 /*6 * 0x40*/, oHnd = 0x43E0 /*2 * 0xE0*/,
                 oOut = 0x4600 /*0x600*/, oSpl = 0x4C00 /*0x1000*/, oRw = 0x5C00 /*0x300*/;
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& At(uintptr_t va) { return *reinterpret_cast<T*>(va); }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
static uint32_t P32(const void* p) { return (uint32_t)(uintptr_t)p; }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static inline uint64_t B8(bool b) { return b ? 1 : 0; }
static CPed* PedF(int k) { return reinterpret_cast<CPed*>(S(oPed + (size_t)k * 0x800)); }
static CVehicle* Veh(int k) { return reinterpret_cast<CVehicle*>(S(oVeh + (size_t)k * 0xA80)); }
static CEntity* Obj(int k) { return reinterpret_cast<CEntity*>(S(oObj + (size_t)k * 0x400)); }
static CEntity* Ent(int k) { return k < 2 ? static_cast<CEntity*>(PedF(k)) : k < 4 ? static_cast<CEntity*>(Veh(k - 2)) : Obj(k - 4); }   // 0..5
static CCam* Cam(int k) { return reinterpret_cast<CCam*>(CAM + kCamFirst + (size_t)k * kCamOne); }
template<class R, class... A> static R ExeT(uint32_t addr, void* self, A... a) { return oracle::Fn<R __fastcall(void*, int, A...)>(addr)(self, 0, a...); }
template<class R, class... A> static R ExeC(uint32_t addr, A... a) { return oracle::Fn<R __cdecl(A...)>(addr)(a...); }
template<class R, class... A> static R ExeS(uint32_t addr, A... a) { return oracle::Fn<R __stdcall(A...)>(addr)(a...); }

static std::function<void(Rng&)> g_tweak;     // row-specific fixture work after the standard build
static int g_wired[6];                        // model indices wired behind ms_modelInfoPtrs

// a game-range float with a few special values mixed in
static float Fl(Rng& r, float scale, int special = 14) {
    if (special && r.below((uint32)special) == 0) return GenF(r, scale, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF);
    return GF(r, scale);
}
static float FlP(Rng& r, float scale) { return GF(r, scale); }   // finite
static CVector V3(Rng& r, float scale, int special = 14) { return CVector(Fl(r, scale, special), Fl(r, scale, special), Fl(r, scale, special)); }
static CVector V3P(Rng& r, float scale) { return CVector(FlP(r, scale), FlP(r, scale), FlP(r, scale)); }

static void WireMatrixBody(Rng& r, uint8_t* m) {   // a plain 4x4 CMatrix (no RW matrix attached)
    FillGarbage(r, m, 0x60);
    auto& M = *reinterpret_cast<CMatrix*>(m);
    if (r.below(4)) {
        const float h = r.f01() * 6.2831853f, p = (r.f01() - 0.5f) * (r.below(3) ? 1.2f : 3.1f);
        M.GetRight() = CVector(std::cos(h), std::sin(h), 0.f);
        M.GetForward() = CVector(-std::sin(h) * std::cos(p), std::cos(h) * std::cos(p), std::sin(p));
        M.GetUp() = CVector(std::sin(h) * std::sin(p), -std::cos(h) * std::sin(p), std::cos(p));
        if (r.below(6) == 0) { const float s = 0.2f + r.f01() * 3.f; M.GetRight() *= s; M.GetForward() *= s; M.GetUp() *= (r.below(2) ? s : 1.f); }
    }
    M.GetPosition() = CVector(Fl(r, 3000.f, 20), Fl(r, 3000.f, 20), Fl(r, 100.f, 20));
    std::memset(m + 0x40, 0, 8);   // m_pRwMatrix, m_bOwnsRwMatrix / m_bHasRwMatrix
}
static void WireModel(Rng& r, int k) {
    uint8_t* mi = S(oMi + (size_t)k * 0x310); uint8_t* col = S(oCol + (size_t)k * 0x40);
    int idx;
    for (;;) { idx = (int)r.below(20000); bool dup = false; for (int j = 0; j < k; ++j) dup |= g_wired[j] == idx; if (!dup) break; }
    g_wired[k] = idx;
    FillGarbage(r, mi, 0x310); FillGarbage(r, col, 0x40);
    const uint32_t cp = P32(col); std::memcpy(mi + offsetof(CBaseModelInfo, m_pColModel), &cp, 4);
    auto* cm = reinterpret_cast<CColModel*>(col);
    const float ex = 0.3f + r.f01() * (r.below(8) ? 5.f : 40.f);
    cm->m_boundBox.m_vecMin = CVector(-ex * (0.2f + r.f01()), -ex * (0.2f + r.f01()), -ex * (0.2f + r.f01()));
    cm->m_boundBox.m_vecMax = CVector(ex * (0.2f + r.f01()), ex * (0.2f + r.f01()), ex * (0.2f + r.f01()));
    cm->m_boundSphere.m_vecCenter = CVector(Fl(r, 4.f, 14), Fl(r, 4.f, 14), Fl(r, 3.f, 14));
    cm->m_boundSphere.m_fRadius = r.below(12) == 0 ? Fl(r, 60.f, 2) : r.f01() * 40.f;
    reinterpret_cast<void**>(0xA9B0C8)[idx] = mi;
}
// entity k (0..5): type, model, matrix or placement, status
static void WireEnt(Rng& r, int k, size_t sz) {
    uint8_t* e = reinterpret_cast<uint8_t*>(Ent(k));
    FillGarbage(r, e, sz);
    const uint32_t ty = k < 2 ? 3 : k < 4 ? 2 : 4;     // ped / vehicle / object
    const uint32_t st = r.below(3) == 0 ? r.below(32) : 2 + r.below(2);
    e[0x36] = (uint8_t)(ty | (st << 3));
    *reinterpret_cast<uint16_t*>(e + offsetof(CEntity, m_nModelIndex)) = (uint16_t)g_wired[k];
    if (r.below(5) == 0) {   // placement only
        *reinterpret_cast<void**>(e + offsetof(CPlaceable, m_matrix)) = nullptr;
        *reinterpret_cast<CVector*>(e + 4) = V3P(r, 3000.f); *reinterpret_cast<float*>(e + 16) = (r.f01() - 0.5f) * 7.f;
    } else {
        uint8_t* m = S(oMat + (size_t)k * 0x60);
        WireMatrixBody(r, m);
        *reinterpret_cast<void**>(e + offsetof(CPlaceable, m_matrix)) = m;
    }
    *reinterpret_cast<void**>(e + offsetof(CEntity, m_pReferences)) = nullptr;
    *reinterpret_cast<void**>(e + 0x18) = nullptr;   // m_pRwObject
    // a physical: move / turn speed in game range
    if (k < 4) {
        *reinterpret_cast<CVector*>(e + offsetof(CPhysical, m_vecMoveSpeed)) = r.below(10) == 0 ? V3(r, 1.f) : V3P(r, 1.f);
        *reinterpret_cast<CVector*>(e + offsetof(CPhysical, m_vecTurnSpeed)) = r.below(10) == 0 ? V3(r, 0.5f) : V3P(r, 0.5f);
    }
}
// positions / speeds of the four physical fixture entities as plain finite game values (no NaN / inf / denormal): for the rows whose normalising maths differs only in the x87 invalid-operation / denormal results
static void SaneEntities(Rng& r) {
    for (int k = 0; k < 4; ++k) {
        CEntity* e = Ent(k);
        const CVector pos((r.f01() - 0.5f) * 3000.f, (r.f01() - 0.5f) * 3000.f, (r.f01() - 0.5f) * 100.f);
        if (e->m_matrix) e->m_matrix->GetPosition() = pos; else e->m_placement.m_vPosn = pos;
        static_cast<CPhysical*>(e)->m_vecMoveSpeed = CVector((r.f01() - 0.5f) * 2.f, (r.f01() - 0.5f) * 2.f, (r.f01() - 0.5f) * 2.f);
        static_cast<CPhysical*>(e)->m_vecTurnSpeed = CVector((r.f01() - 0.5f) * 0.5f, (r.f01() - 0.5f) * 0.5f, (r.f01() - 0.5f) * 0.5f);
    }
}
static const int kPedStates[] = { PEDSTATE_ENTER_CAR, PEDSTATE_CARJACK, PEDSTATE_OPEN_DOOR, PEDSTATE_EXIT_CAR, PEDSTATE_IDLE, PEDSTATE_DRIVING, PEDSTATE_ARRESTED, PEDSTATE_DEAD };

static void BuildCam(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    for (int k = 0; k < 6; ++k) WireModel(r, k);
    WireEnt(r, 0, 0x800); WireEnt(r, 1, 0x800); WireEnt(r, 2, 0xA80); WireEnt(r, 3, 0xA80); WireEnt(r, 4, 0x400); WireEnt(r, 5, 0x400);
    // peds: player peds in or out of vehicles
    for (int k = 0; k < 2; ++k) {
        CPed* p = PedF(k);
        p->m_nPedState = (ePedState)(r.below(3) ? kPedStates[r.below(8)] : (int)r.below(70));
        p->bInVehicle = r.below(2);
        p->m_pVehicle = r.below(8) == 0 ? nullptr : Veh(r.below(2));
        p->m_fCurrentRotation = r.below(12) ? (r.f01() - 0.5f) * 8.f : Fl(r, 8.f, 2);
        p->m_fAimingRotation = r.below(12) ? (r.f01() - 0.5f) * 8.f : Fl(r, 8.f, 2);
    }
    // vehicles: handling data
    for (int k = 0; k < 2; ++k) {
        uint8_t* h = S(oHnd + (size_t)k * 0xE0);
        FillGarbage(r, h, 0xE0);
        static const uint32_t kFl[] = { 0, VEHICLE_HANDLING_MODEL_IS_BOAT, VEHICLE_HANDLING_MODEL_IS_PLANE, VEHICLE_HANDLING_MODEL_IS_HELI, VEHICLE_HANDLING_MODEL_IS_BIKE };
        const uint32_t fl = r.below(6) == 0 ? r.u32() : kFl[r.below(5)];
        std::memcpy(h + offsetof(tHandlingData, m_nModelFlags), &fl, 4);
        Veh(k)->m_pHandlingData = reinterpret_cast<tHandlingData*>(h);
    }
    // the world
    CWorld::PlayerInFocus = (uint8)r.below(2);
    CWorld::Players[0].m_pPed = reinterpret_cast<CPlayerPed*>(PedF(0));
    CWorld::Players[1].m_pPed = r.below(4) == 0 ? nullptr : reinterpret_cast<CPlayerPed*>(PedF(1));
    CWorld::Players[0].m_pRemoteVehicle = nullptr; CWorld::Players[1].m_pRemoteVehicle = nullptr;
    // time
    CTimer::ms_fTimeStep = r.below(8) ? 0.2f + r.f01() * 3.f : Fl(r, 5.f, 2);
    CTimer::ms_fTimeStepNonClipped = CTimer::ms_fTimeStep;
    CTimer::m_snTimeInMilliseconds = r.u32();
    CTimer::m_FrameCounter = r.u32();
    CTimer::m_UserPause = false; CTimer::m_CodePause = false;
    CReferences::Init();
    // the camera: garbage, plausible selectors, pointers
    FillGarbage(r, CAM, kCamSize);
    auto& C = TheCamera;
    C.m_nActiveCam = (uint8)(r.below(10) == 0 ? 2 : r.below(2));
    C.m_pRwCamera = nullptr;
    C.m_pTargetEntity = r.below(4) == 0 ? nullptr : Ent((int)r.below(6));
    C.m_pAttachedEntity = r.below(4) == 0 ? nullptr : Ent((int)r.below(6));
    C.m_pToGarageWeAreIn = nullptr; C.m_pToGarageWeAreInForHackAvoidFirstPerson = nullptr;
    C.m_pExtraEntity[0] = C.m_pExtraEntity[1] = nullptr;
    for (auto& ps : C.m_aPathArray) ps.m_pArrPathData = nullptr;
    for (int k = 0; k < 3; ++k) {
        CCam& c = *Cam(k);
        c.m_pCamTargetEntity = r.below(4) == 0 ? nullptr : Ent((int)r.below(6));
        c.m_pLastCarEntered = r.below(2) ? nullptr : Veh(r.below(2));
        c.m_pLastPedLookedAt = r.below(2) ? nullptr : PedF(r.below(2));
        c.m_pCarWeAreFocussingOn = r.below(2) ? nullptr : Veh(r.below(2));
        c.m_pCarWeAreFocussingOnI = r.below(2) ? nullptr : Veh(r.below(2));
        c.m_nMode = (eCamMode)(r.below(6) ? r.below(64) : r.below(256));
        if (r.below(3)) { c.m_vecFront = V3P(r, 1.f); c.m_vecUp = V3P(r, 1.f); c.m_vecSource = V3P(r, 3000.f); }
    }
    C.m_bCooperativeCamMode = r.below(2);
    C.m_bAllowShootingWith2PlayersInCar = r.below(2);
    Scene.m_pRwCamera = nullptr;
    // bool members hold 0 / 1 in a running game (every exe store writes 0 / 1); `cmp byte, 1` in the exe and `if (b)` in the port differ on other bytes
#define W3C_NB(f) reinterpret_cast<uint8_t&>(C.f) &= 1;
    W3C_BOOLS_CAMERA(W3C_NB)
#undef W3C_NB
    for (int k = 0; k < 3; ++k) {
        CCam& c = *Cam(k);
#define W3C_NB(f) reinterpret_cast<uint8_t&>(c.f) &= 1;
        W3C_BOOLS_CAM(W3C_NB)
#undef W3C_NB
    }
    // the idle camera (global at 0xB6FDA0)
    FillGarbage(r, &gIdleCam, sizeof(CIdleCam));
#define W3C_NB(f) reinterpret_cast<uint8_t&>(gIdleCam.f) &= 1;
    W3C_BOOLS_IDLE(W3C_NB)
#undef W3C_NB
    gIdleCam.m_Target = r.below(3) == 0 ? nullptr : Ent((int)r.below(6));
    gIdleCam.m_Cam = r.below(5) == 0 ? nullptr : Cam((int)r.below(3));
    gIdleCam.m_ZoomState = (eIdleCamZoomState)r.below(4);
    if (g_tweak) g_tweak(r);
}

template<class PF, class EF>
static bool CCase(Rng& r, std::string& d, PF&& pf, EF&& ef) {
    const uint32_t seed = r.u32() | 1;
    auto* tab = reinterpret_cast<void**>(0xA9B0C8);
    std::vector<uint8_t> tabSave(sizeof(void*) * 20000); std::memcpy(tabSave.data(), tab, tabSave.size());
    BuildCam(r);
    Reg sc{ (uintptr_t)g_sc, oRw };   // the fake RwCamera behind Scene.m_pRwCamera (exe layout vs librw layout) is not compared
    Reg img{ kImgLo, kImgHi - kImgLo };
    const bool ok = BothA(d, seed, { sc, img }, std::forward<PF>(pf), std::forward<EF>(ef));
    std::memcpy(tab, tabSave.data(), tabSave.size());
    return ok;
}
} // namespace w3c

struct Tw { explicit Tw(std::function<void(Rng&)> f) { w3c::g_tweak = std::move(f); } ~Tw() { w3c::g_tweak = nullptr; } };
template<class Fn> static void RunScaledC(const std::string& name, Fn&& fn, int cases) {
    static const int scale = std::getenv("W3C_SCALE") ? std::max(1, std::atoi(std::getenv("W3C_SCALE"))) : 1;
    Run(name, std::forward<Fn>(fn), cases * scale);
}
#define ROW(NAME, ADDR, CASES) { const int _cs = (CASES); const std::string _nm = std::string(NAME " " #ADDR); RunScaledC(_nm, [&](Rng& r, std::string& d) -> bool
#define ENDROW , _cs); }
#define CC(PORT, EXE) return CCase(r, d, [&]() -> uint64_t { PORT }, [&]() -> uint64_t { EXE })
#include "wave3_oracle_camera_a.inc"      // CCamera
#include "wave3_oracle_camera_b.inc"      // CCam / free functions
#include "wave3_oracle_camera_c.inc"      // CIdleCam

static void TestW3CameraAll() {
    TestW3CameraA();
    TestW3CameraB();
    TestW3CameraC();
}
