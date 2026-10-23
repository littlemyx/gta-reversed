// ORACLE-W3-VEH: exe-oracle rows for game_sa/Entity/Vehicle (classes A and B of .notes/ORACLE_COVERAGE.md). Included by wave2_oracle_all.inc when W3_VEH_ONLY is defined
// (executable wave3_vehicle_oracle_test = the W2 harness: deterministic arena, fake pools, entity reference world, whole exe data image compared after every case).
//
// Difference to the task / event rows of W2: the `this` object is a VEHICLE. A garbage vehicle faults in nearly every function (m_matrix, m_pHandlingData, the model-info table), so W3FuncCase
// wires the pointers a real vehicle always has (matrix, handling data, vehicle model info with a collision model and dummy table, passenger list, the winch rope table) and keeps the
// counters in their game range; everything else stays garbage. The exe data image is restored after every case (several functions bump a global counter / array index, which would drift
// out of range over the cases - W2 has no such function).
#include "Models/VehicleModelInfo.h"
#include "Models/ModelInfo.h"
#include "Collision/ColModel.h"
#include "Ropes.h"
#include "game_sa/Entity/Vehicle/Automobile.h"
#include "game_sa/Entity/Vehicle/Bike.h"
#include "game_sa/Entity/Vehicle/Bmx.h"
#include "game_sa/Entity/Vehicle/Boat.h"
#include "game_sa/Entity/Vehicle/Heli.h"
#include "game_sa/Entity/Vehicle/MonsterTruck.h"
#include "game_sa/Entity/Vehicle/Plane.h"
#include "game_sa/Entity/Vehicle/QuadBike.h"
#include "game_sa/Entity/Vehicle/Trailer.h"
#include "game_sa/Entity/Vehicle/Train.h"
#include "game_sa/Entity/Vehicle/Vehicle.h"

// ---- ORACLE-C-VEH: vehicle fixture with REAL vtable pointers (class C = the exe function's closure contains virtual calls) --------------------------------------------------------------
// A garbage vptr makes every virtual call fault (both sides: the case carries no information). W3FuncCase(..., vtCls) writes the EXE's vftable address (RH_ScopedVirtualClass VA) into the vehicle
// (and its embedded CAEVehicleAudioEntity and the model info) before the exe call and the PORT's vftable before the port call, and clears the words again afterwards, so the compare of the object ignores
// them. The port's vftable is the compiler's `??_7Cls@@6B@`, reached through /alternatename (no object has to be constructed). Virtual callees then run their real code on each side.
#define W3_PORT_VT(C) extern "C" const uint32_t w3vt_##C[]; __pragma(comment(linker, "/alternatename:_w3vt_" #C "=??_7" #C "@@6B@"))
W3_PORT_VT(CAutomobile) W3_PORT_VT(CMonsterTruck) W3_PORT_VT(CQuadBike) W3_PORT_VT(CHeli) W3_PORT_VT(CPlane) W3_PORT_VT(CBoat) W3_PORT_VT(CTrain) W3_PORT_VT(CBike) W3_PORT_VT(CBmx) W3_PORT_VT(CTrailer)
W3_PORT_VT(CAEVehicleAudioEntity) W3_PORT_VT(CVehicleModelInfo)

namespace w3 {
using namespace w2;
alignas(16) static uint8_t g_mat[0x60];     // the vehicle's CMatrixLink
alignas(16) static uint8_t g_hand[0x100];   // tHandlingData
alignas(16) static uint8_t g_mi[0x400];     // CVehicleModelInfo
alignas(16) static uint8_t g_vs[0x400];     // CVehicleModelInfo::CVehicleStructure
alignas(16) static uint8_t g_col[0x80];     // CColModel
static int g_model = 0;
alignas(16) static uint8_t g_mi2[0x400];   // the upgrade model's CAtomicModelInfo (IsValidModForVehicle)
alignas(16) static uint8_t g_out[0x40];     // hand rows: out parameters (CVector results, uint32& flags, bool&) - compared like the rest
alignas(16) static uint8_t g_frame[0x100];  // a fake RwFrame (car nodes)
alignas(16) static uint8_t g_cp[0x60];      // a fake CColPoint
static std::function<void(Rng&, uint8_t*)> g_post;   // hand rows: extra fixture work after the vehicle is wired
template<class T> static void Put32(uint8_t* b, size_t off, T v) { std::memcpy(b + off, &v, sizeof v); }

// models the vehicle code switches on (plus random ones)
static int PickModel(Rng& r) {
    static const int t[] = { MODEL_HUNTER, MODEL_SEASPAR, MODEL_RCBARON, MODEL_RUSTLER, MODEL_MAVERICK, MODEL_POLMAV, MODEL_HYDRA, MODEL_CARGOBOB, MODEL_TORNADO, MODEL_KART, MODEL_RCBANDIT,
        MODEL_RHINO, MODEL_DUMPER, MODEL_PACKER, MODEL_DOZER, MODEL_FORKLIFT, MODEL_BAGBOXA, MODEL_BAGBOXB, MODEL_TOWTRUCK, MODEL_INFERNUS, MODEL_BMX, MODEL_ANDROM, MODEL_BUS };
    return r.below(3) ? t[r.below((uint32)std::size(t))] : 400 + r.below(212);
}
static void WireMatrix(Rng& r, uint8_t* m) {
    FillGarbage(r, m, 0x50);
    auto& M = *reinterpret_cast<CMatrix*>(m);
    if (r.below(4)) {   // mostly a proper rotation, like the game's vehicles
        const float h = r.f01() * 6.2831853f, p = (r.f01() - 0.5f) * 1.2f;
        M.GetRight() = CVector(std::cos(h), std::sin(h), 0.f);
        M.GetForward() = CVector(-std::sin(h) * std::cos(p), std::cos(h) * std::cos(p), std::sin(p));
        M.GetUp() = CVector(std::sin(h) * std::sin(p), -std::cos(h) * std::sin(p), std::cos(p));
    }
    M.GetPosition() = GV(r);
    std::memset(m + 0x40, 0, 0x10);     // m_pAttachMatrix, m_bOwnsAttachedMatrix, (CMatrixLink) list links
}
// wires the pointers of a vehicle in g_src (size sz)
static void WireVehicle(Rng& r, uint8_t* v, size_t sz) {
    WireMatrix(r, g_mat);
    Put32(v, offsetof(CPlaceable, m_matrix), (uint32_t)(uintptr_t)g_mat);
    FillGarbage(r, g_hand, sizeof g_hand);
    FillGarbage(r, g_out, sizeof g_out); FillGarbage(r, g_frame, sizeof g_frame); FillGarbage(r, g_cp, sizeof g_cp);
    Put32(v, offsetof(CVehicle, m_pHandlingData), (uint32_t)(uintptr_t)g_hand);
    // model info + collision model + dummy table
    g_model = PickModel(r);
    Put32(v, offsetof(CEntity, m_nModelIndex), (int16_t)g_model);
    FillGarbage(r, g_mi, sizeof g_mi); FillGarbage(r, g_vs, sizeof g_vs); FillGarbage(r, g_col, sizeof g_col);
    Put32(g_mi, offsetof(CBaseModelInfo, m_pColModel), (uint32_t)(uintptr_t)g_col);
    Put32(g_mi, offsetof(CVehicleModelInfo, m_pVehicleStruct), (uint32_t)(uintptr_t)g_vs);
    Put32(g_mi, offsetof(CVehicleModelInfo, m_nVehicleClass), (int32_t)(r.below(8) ? r.below(12) : -1));
    // x87 extended exponent range: a square of a tiny garbage float is 0 in the port's float arithmetic but not in the exe's registers (0x6D5400 CanBeDriven) - the game's dummies are never that small
    for (size_t i = 0; i + 4 <= sizeof g_vs; i += 4) { float f; std::memcpy(&f, g_vs + i, 4); if (std::fabs(f) < 1.0e-15f) { f = 0.f; std::memcpy(g_vs + i, &f, 4); } }
    if (r.below(2)) std::memset(g_vs, 0, 0x100);                       // all dummies at the origin (IsZero branches)
    else if (r.below(2)) std::memset(g_vs + r.below(40) * 12, 0, 12);
    { auto* bb = reinterpret_cast<float*>(g_col); for (int i = 0; i < 6; ++i) bb[i] = GF(r, 5.f); if (r.below(2)) { bb[0] = -std::fabs(bb[0]); bb[3] = std::fabs(bb[3]); bb[1] = -std::fabs(bb[1]); bb[4] = std::fabs(bb[4]); bb[2] = -std::fabs(bb[2]); bb[5] = std::fabs(bb[5]); } }
    CModelInfo::ms_modelInfoPtrs[g_model] = reinterpret_cast<CBaseModelInfo*>(g_mi);
    // counters and lists in their game ranges
    { const int32_t vt = (int32_t)(r.below(12) - (r.below(10) == 0 ? 1 : 0)); Put32(v, offsetof(CVehicle, m_nVehicleType), vt); Put32(g_mi, offsetof(CVehicleModelInfo, m_nVehicleType), vt); }   // the game copies the model's type into the vehicle (0x6D5400 reads the model's, the port the vehicle's)
    Put32(v, offsetof(CVehicle, m_nVehicleSubType), (int32_t)r.below(12));
    Put32(v, offsetof(CVehicle, m_nDoorLock), (int32_t)r.below(10));
    v[offsetof(CVehicle, m_vehicleSpecialColIndex)] = (uint8_t)(r.below(4) ? 0xFF : r.below(4));   // -1 = the model info collision model
    const int maxP = (int)r.below(9); v[offsetof(CVehicle, m_nMaxPassengers)] = (uint8_t)maxP;
    int n = 0;
    for (int i = 0; i < 8; ++i) { CEntity* p = r.below(3) ? nullptr : GPedN(r); Put32(v, offsetof(CVehicle, m_apPassengers) + 4 * i, (uint32_t)(uintptr_t)p); n += p != nullptr && i < maxP; }
    v[offsetof(CVehicle, m_nNumPassengers)] = (uint8_t)(r.below(5) ? n : r.below(9));
    { CEntity* dr = r.below(3) == 0 ? nullptr : GPedN(r); Put32(v, offsetof(CVehicle, m_pDriver), (uint32_t)(uintptr_t)dr); }
    // the winch rope table: some ropes in use, one of them (usually) owned by this vehicle
    FillGarbage(r, &CRopes::aRopes[0], sizeof CRopes::aRopes);
    for (auto& rp : CRopes::aRopes) rp.m_nType = (eRopeType)r.below(4);
    { auto& rp = CRopes::aRopes[r.below((uint32)CRopes::aRopes.size())]; rp.m_nId = (uintptr_t)v + 0x1D; if (!(int)rp.m_nType) rp.m_nType = (eRopeType)(1 + r.below(3)); }
    (void)sz;
}
static void UnwireModel() { CModelInfo::ms_modelInfoPtrs[g_model] = nullptr; }

// ORACLE-C-VEH: class table. exeVt = RH_ScopedVirtualClass VA, portVt = the compiler's vftable (alternatename above), vtype = the eVehicleType the game stores for the class
struct VtInfo { const char* name; uint32_t exeVt; const uint32_t* portVt; size_t size; int vtype; };
static const VtInfo kVt[] = {
    { "CAutomobile", 0x871120, w3vt_CAutomobile, sizeof(CAutomobile), VEHICLE_TYPE_AUTOMOBILE }, { "CMonsterTruck", 0x8717D8, w3vt_CMonsterTruck, sizeof(CMonsterTruck), VEHICLE_TYPE_MTRUCK },
    { "CQuadBike", 0x871AE8, w3vt_CQuadBike, sizeof(CQuadBike), VEHICLE_TYPE_QUAD }, { "CHeli", 0x871680, w3vt_CHeli, sizeof(CHeli), VEHICLE_TYPE_HELI },
    { "CPlane", 0x871948, w3vt_CPlane, sizeof(CPlane), VEHICLE_TYPE_PLANE }, { "CBoat", 0x8721A0, w3vt_CBoat, sizeof(CBoat), VEHICLE_TYPE_BOAT },
    { "CTrain", 0x872370, w3vt_CTrain, sizeof(CTrain), VEHICLE_TYPE_TRAIN }, { "CBike", 0x871360, w3vt_CBike, sizeof(CBike), VEHICLE_TYPE_BIKE },
    { "CBmx", 0x871528, w3vt_CBmx, sizeof(CBmx), VEHICLE_TYPE_BMX }, { "CTrailer", 0x871C28, w3vt_CTrailer, sizeof(CTrailer), VEHICLE_TYPE_TRAILER },
};
constexpr uint32_t kExeAudioVt = 0x862CEC, kExeVmiVt = 0x85C5C8;
static int VtIdx(const char* cls) { for (int i = 0; i < (int)std::size(kVt); ++i) if (!std::strcmp(kVt[i].name, cls)) return i; return -1; }   // -1: a base class (CVehicle / CPhysical ...): the case picks a random vehicle class
// writes the exe's (`exe`) or the port's vptrs into the wired vehicle / model info; `clear` zeroes them (after the call, so the compare ignores them)
static void SetVptrs(uint8_t* v, int vi, int side /* 0 clear, 1 port, 2 exe */) {
    const uint32_t vt = side == 1 ? (uint32_t)(uintptr_t)kVt[vi].portVt : side == 2 ? kVt[vi].exeVt : 0;
    const uint32_t au = side == 1 ? (uint32_t)(uintptr_t)w3vt_CAEVehicleAudioEntity : side == 2 ? kExeAudioVt : 0;
    const uint32_t mi = side == 1 ? (uint32_t)(uintptr_t)w3vt_CVehicleModelInfo : side == 2 ? kExeVmiVt : 0;
    Put32(v, 0, vt); Put32(v, offsetof(CVehicle, m_vehicleAudio), au); Put32(g_mi, 0, mi);
}
template<class PF, class EF>
static bool W3FuncCase(Rng& r, std::string& d, size_t sz, std::initializer_list<EntOff> ents, PF&& portFn, EF&& exeFn, bool sub = false, bool dtor = false, const char* vtCls = nullptr) {
    const int vi0 = vtCls ? VtIdx(vtCls) : -2;                                  // -2: no vptr injection (classes A / B); -1: base class
    const int vi = vi0 == -1 ? (int)r.below((uint32)std::size(kVt)) : vi0;      // base-class rows get a random vehicle class (and its size)
    if (vi >= 0 && sz) sz = kVt[vi].size;
    static std::vector<uint8_t> img0(kImgHi - kImgLo);
    const auto post = std::move(g_post); g_post = nullptr;   // a faulting case must not leave a hook with dangling captures behind
    std::memcpy(img0.data(), (void*)kImgLo, img0.size());
    const uint32_t seed = Prep(r, 0);
    FillGarbage(r, g_anim, sizeof g_anim);
    g_subTask = nullptr;
    if (sub) { g_subTask = g_tp.pool->New(); if (g_subTask) ::new (static_cast<void*>(g_subTask)) CTaskSimpleStandStill(50, false, false, 8.0f); }
    if (sz) { FillGarbage(r, g_src, (sz + 3) & ~size_t(3)); for (const auto& e : ents) PutEnt(r, e.off, e.kind, e.size); WireVehicle(r, g_src, sz); if (vi >= 0) { Put32(g_src, offsetof(CVehicle, m_nVehicleType), (int32_t)kVt[vi].vtype); Put32(g_mi, offsetof(CVehicleModelInfo, m_nVehicleType), (int32_t)kVt[vi].vtype); } }
    if (post) post(r, g_src);
    Reg thisReg{ (uintptr_t)g_src, sz ? sz : 4 };
    if (dtor) { thisReg.mask.assign(sz ? sz : 4, 0); for (int k = 0; k < 4; ++k) thisReg.mask[k] = 1; }
    const bool ok = BothA(d, seed, { thisReg, Reg{ (uintptr_t)g_anim, sizeof g_anim }, Reg{ (uintptr_t)g_mat, sizeof g_mat }, Reg{ (uintptr_t)g_hand, sizeof g_hand }, Reg{ (uintptr_t)g_mi, sizeof g_mi }, Reg{ (uintptr_t)g_vs, sizeof g_vs },
            Reg{ (uintptr_t)g_col, sizeof g_col }, Reg{ (uintptr_t)g_mi2, sizeof g_mi2 }, Reg{ (uintptr_t)g_out, sizeof g_out }, Reg{ (uintptr_t)g_frame, sizeof g_frame }, Reg{ (uintptr_t)g_cp, sizeof g_cp }, g_tp.RegStruct(), g_tp.RegFlags(), g_tp.RegStore(), g_ep.RegStruct(), g_ep.RegFlags(), g_ep.RegStore(), Reg{ kImgLo, kImgHi - kImgLo } },
        [&]() -> uint64_t { if (vi >= 0 && sz) SetVptrs(g_src, vi, 1); const uint64_t rv = portFn(static_cast<void*>(g_src)); if (vi >= 0 && sz) SetVptrs(g_src, vi, 0); return rv; },
        [&]() -> uint64_t { if (vi >= 0 && sz) SetVptrs(g_src, vi, 2); const uint64_t rv = exeFn(static_cast<void*>(g_src)); if (vi >= 0 && sz) SetVptrs(g_src, vi, 0); return rv; }, kRf);
    if (!ok) { char t[96]; uint32_t hf = 0; std::memcpy(&hf, g_src + offsetof(CVehicle, m_nHandlingFlagsIntValue), 4); std::snprintf(t, sizeof t, " [model %d hflags %08X]", g_model, hf); d += t; }
    Done();
    UnwireModel();
    std::memcpy((void*)kImgLo, img0.data(), img0.size());
    return ok;
}
} // namespace w3
using w3::W3FuncCase;

#if __has_include("wave3_oracle_gen_veh.inc")
#include "wave3_oracle_gen_veh.inc"      // GENERATED (tools/gen_w3_veh.py): vehicle member / static functions on a wired vehicle
#define W3_HAVE_GEN 1
#endif


// ---- hand rows: free functions and functions that need extra fixture state ------------------------------------------------------------------
static void TestW3VehHand() {
    using namespace w2; using namespace w3;
    // 0x6E38F0 / 0x49B010 are plain functions taking a vehicle: the wired vehicle in g_src (or a pool vehicle) is the argument
    Run("::IsVehiclePointerValid 0x6E38F0", [&](Rng& r, std::string& d) {
        CVehicle* arg;
        switch (r.below(5)) { case 0: arg = nullptr; break; case 1: arg = reinterpret_cast<CVehicle*>(reinterpret_cast<uint8_t*>(rf::VehE((int)r.below(rf::kV))) + 1 + r.below(4)); break; case 2: arg = reinterpret_cast<CVehicle*>(g_src); break; default: arg = reinterpret_cast<CVehicle*>(rf::VehE((int)r.below(rf::kV))); }
        g_post = [&](Rng& rr, uint8_t*) {
            for (int i = 0; i < rf::kV; ++i) {
                auto* vb = reinterpret_cast<uint8_t*>(rf::VehE(i));
                Put32(vb, offsetof(CVehicle, m_nVehicleType), (int32_t)(rr.below(3) == 0 ? VEHICLE_TYPE_FPLANE : rr.below(12)));
                Put32(vb, offsetof(CPhysical, m_pCollisionList), (uint32_t)(rr.below(2) ? 0u : 0x12340u));
            }
        };
        const bool ok = W3FuncCase(r, d, sizeof(CAutomobile), {}, [&](void*) -> uint64_t { return (uint64_t)(uint8_t)(::IsVehiclePointerValid(arg) ? 1 : 0); }, [&](void*) -> uint64_t { return (uint64_t)(uint8_t)(oracle::Fn<int __cdecl(void*)>(0x6E38F0)(arg) ? 1 : 0); });
        g_post = nullptr; return ok;
    }, 120);
    Run("::IsValidModForVehicle 0x49B010", [&](Rng& r, std::string& d) {
        uint32_t modelId = 1000 + r.below(194);
        const uint32_t pick = r.below(4);
        g_post = [&](Rng& rr, uint8_t* v) {
            auto* veh = reinterpret_cast<CVehicle*>(v);
            auto* vmi = reinterpret_cast<CVehicleModelInfo*>(g_mi);
            vmi->m_nWheelUpgradeClass = (uint8)rr.below(4);
            for (auto& u : vmi->m_anUpgrades) u = (int16)(rr.below(3) ? 1000 + rr.below(194) : -1);
            if (pick == 0) vmi->m_anUpgrades[rr.below(18)] = (int16)modelId;
            if (pick == 1) { const int32 ws = (int8)vmi->m_nWheelUpgradeClass; const int n = CVehicleModelInfo::GetNumWheelUpgrades(ws); if (n > 0) modelId = (uint32)CVehicleModelInfo::GetWheelUpgrade(ws, (int)rr.below(n)); }
            FillGarbage(rr, g_mi2, sizeof g_mi2);
            auto* mi2 = reinterpret_cast<CBaseModelInfo*>(g_mi2);
            mi2->bUsesVehDummy = rr.below(2);
            mi2->CarMod = rr.below(3) == 0 ? 2 : rr.below(2) ? 0x11 : rr.below(32);
            CModelInfo::ms_modelInfoPtrs[modelId] = mi2;
            veh->m_vehicleAudio.m_AuSettings.RadioType = (eAERadioType)(rr.below(3) ? 0 : rr.below(5));
            veh->m_vehicleAudio.m_AuSettings.BassSetting = (eBassSetting)rr.below(3);
        };
        const bool ok = W3FuncCase(r, d, sizeof(CAutomobile), {}, [&](void* p) -> uint64_t { return (uint64_t)(uint8_t)(::IsValidModForVehicle(modelId, static_cast<CVehicle*>(p)) ? 1 : 0); }, [&](void* p) -> uint64_t { return (uint64_t)(uint8_t)(oracle::Fn<int __cdecl(uint32_t, void*)>(0x49B010)(modelId, p) ? 1 : 0); });
        g_post = nullptr; return ok;
    }, 200);
    Run("::ProcessTrainAnnouncements 0x6F5910", [&](Rng& r, std::string& d) {
        return W3FuncCase(r, d, 0, {}, [&](void*) -> uint64_t { ::ProcessTrainAnnouncements(); return 0; }, [&](void*) -> uint64_t { oracle::Fn<void __cdecl()>(0x6F5910)(); return 0; });
    }, 10);
    Run("::PlayAnnouncement 0x6F5920", [&](Rng& r, std::string& d) {
        const uint8 a = (uint8)r.u32(), b = (uint8)r.u32();
        return W3FuncCase(r, d, 0, {}, [&](void*) -> uint64_t { ::PlayAnnouncement(a, b); return 0; }, [&](void*) -> uint64_t { oracle::Fn<void __cdecl(int, int)>(0x6F5920)(a, b); return 0; });
    }, 10);
}

#include "wave3_oracle_veh_hand2.inc"
#include "wave3_oracle_veh_hand3.inc"

static void TestW3VtInfra();
static void TestW3Veh() {
    TestW3VtInfra();
    TestW3VehHand();
    TestW3VehHand2();
    TestW3VehHand3();
#ifdef W3_HAVE_GEN
    TestW3VehFuncs();
#endif
}
static_assert(offsetof(CVehicleModelInfo, m_fWheelSizeFront) == 0x40 && offsetof(CVehicleModelInfo, m_fWheelSizeRear) == 0x44, "exe 0x6A6090 reads +0x40 / +0x44");
static_assert(offsetof(CVehicle, m_nHandlingFlagsIntValue) == 0x38C, "exe 0x6A6090 reads +0x38C");

// ORACLE-C-VEH self-test: every slot of the port's vftable must be a real function (an unresolved symbol is 0x400000 under /FORCE:UNRESOLVED: a virtual call through it faults in the port only).
// Prints the slot numbers that are unresolved (their TU is not linked into this test); the exe's vftable is checked to point into .text.
static void TestW3VtInfra() {
    using namespace w3;
    int bad = 0;
    for (const auto& c : kVt) {
        int n = 0; std::string un;
        for (int i = 0; i < 71; ++i) { const uint32_t pv = c.portVt[i]; const uint32_t ev = *reinterpret_cast<const uint32_t*>(c.exeVt + 4 * i); if (pv == 0x400000) { un += " " + std::to_string(i); ++n; } if (i < 66 && (ev < 0x401000 || ev > 0x850000)) ++bad; }
        std::printf("w3 vt: %-14s port vftable %p, unresolved slots:%s\n", c.name, (const void*)c.portVt, un.empty() ? " none" : un.c_str());
    }
    std::printf("w3 vt infra: exe vftable slots outside .text: %d\n", bad);
}
