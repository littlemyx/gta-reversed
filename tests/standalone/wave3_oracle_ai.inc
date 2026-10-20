// ORACLE-W3-AI: exe-oracle rows for CarCtrl.cpp, PedIntelligence.cpp, Cover.cpp and Entity/Object/Object.cpp (classes A + B), on real fixtures: peds with a CPedIntelligence (acquaintances, scanner arrays,
// interesting entities, mental state), vehicles with autopilot / path-node addresses, objects with model infos + col models, cover points, the entity reference world of logic_oracle_refs.inc.
// Included by wave2_oracle_all.inc when W3_AI_ONLY is defined (executable wave3_ai_oracle_test = the W2 harness: deterministic arena, whole exe data image + a scratch block compared after every case).
#include "PedIntelligence.h"
#include "Entity/Ped/Ped.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Object/Object.h"
#include "CarCtrl.h"
#include "Cover.h"
#include "CoverPoint.h"
#include "Timer.h"
#include "World.h"
#include "PedType.h"
#include "TxdStore.h"
#include "Pools/PtrNodeDoubleLinkPool.h"
#include "Models/ModelInfo.h"
#include "Models/BaseModelInfo.h"
#include "Collision/ColModel.h"

namespace w3a {
using namespace w2;
constexpr size_t kScSize = 0x20000;
alignas(16) static uint8_t g_sc[kScSize];
// layout of the scratch block (every ped / intelligence / vehicle has 0x1000 bytes; objects, matrices, model infos below)
constexpr size_t oPedA = 0x0000, oPedB = 0x1000, oIntA = 0x2000, oIntB = 0x3000, oVehA = 0x4000, oVehB = 0x5000, oObjA = 0x6000, oObjB = 0x6400, oMat = 0x7000 /*16 x 0x60*/, oMi = 0x7800 /*4 x 0x400*/,
                 oCol = 0x8800 /*4 x 0x100*/, oOut = 0x9000 /*0x400*/, oList = 0x9800 /*0x1000: list nodes*/, oCpt = 0xA800;
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& At(uintptr_t va) { return *reinterpret_cast<T*>(va); }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
static inline uint64_t B8(bool b) { return b ? 1 : 0; }
static inline uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static std::function<void(Rng&)> g_tweak;     // row-specific fixture work after the standard build
static bool g_useRf = false;
template<class R, class... A> static R ExeThis(uint32_t addr, void* self, A... a) { return oracle::Fn<R __fastcall(void*, int, A...)>(addr)(self, 0, a...); }

static float Fl(Rng& r, float scale, int special = 12) { if (special && r.below((uint32)special) == 0) return GenF(r, scale, S_ALL); return GF(r, scale); }
static CVector Vec(Rng& r, float sc, int special = 14) { return CVector(Fl(r, sc, special), Fl(r, sc, special), Fl(r, sc * 0.1f, special)); }
// a matrix block (CMatrixLink-sized, 0x60) at slot i with a plausible rotation and position; returns the pointer
static uint8_t* Mat(Rng& r, int i, bool rotation = true) {
    uint8_t* m = S(oMat + (size_t)i * 0x60);
    FillGarbage(r, m, 0x40);
    auto& M = *reinterpret_cast<CMatrix*>(m);
    if (rotation) {
        const float h = r.f01() * 6.2831853f, p = (r.f01() - 0.5f) * (r.below(3) ? 0.6f : 3.1f);
        M.GetRight() = CVector(std::cos(h), std::sin(h), 0.f);
        M.GetForward() = CVector(-std::sin(h) * std::cos(p), std::cos(h) * std::cos(p), std::sin(p));
        M.GetUp() = CVector(std::sin(h) * std::sin(p), -std::cos(h) * std::sin(p), std::cos(p));
    }
    M.GetPosition() = CVector(Fl(r, 3000.f, 20), Fl(r, 3000.f, 20), Fl(r, 100.f, 20));
    std::memset(m + 0x40, 0, 8);
    return m;
}
// entity at `off`: garbage, the placeable's matrix pointer set to a matrix block (or null with the placement position), type bits, model index
static void Ent(Rng& r, size_t off, int matSlot, unsigned type, uint16_t model, size_t size, bool allowNullMat = true) {
    uint8_t* e = S(off);
    FillGarbage(r, e, size);
    e[0x36] = (uint8_t)((e[0x36] & 0xF8) | type);
    SAt<uint16_t>(off + 0x22) = model;
    if (allowNullMat && r.below(4) == 0) {
        SAt<void*>(off + 0x14) = nullptr;
        SAt<float>(off + 4) = Fl(r, 3000.f, 20); SAt<float>(off + 8) = Fl(r, 3000.f, 20); SAt<float>(off + 12) = Fl(r, 100.f, 20); SAt<float>(off + 16) = (r.f01() - 0.5f) * 7.f;
    } else {
        SAt<void*>(off + 0x14) = Mat(r, matSlot);
    }
    SAt<void*>(off + offsetof(CEntity, m_pReferences)) = nullptr;
}
static void ModelInfoFor(Rng& r, int slot, uint16_t idx, float radius) {
    const size_t mi = oMi + (size_t)slot * 0x400, col = oCol + (size_t)slot * 0x100;
    FillGarbage(r, S(mi), 0x400); FillGarbage(r, S(col), 0x100);
    SAt<void*>(mi + offsetof(CBaseModelInfo, m_pColModel)) = S(col);
    auto* cm = reinterpret_cast<CColModel*>(S(col));
    const float ex = 0.1f + r.f01() * 12.f;
    cm->m_boundBox.m_vecMin = CVector(-ex * (0.2f + r.f01()), -ex * (0.2f + r.f01()), -ex * (0.2f + r.f01()));
    cm->m_boundBox.m_vecMax = CVector(ex * (0.2f + r.f01()), ex * (0.2f + r.f01()), ex * (0.2f + r.f01()));
    cm->m_boundSphere.m_vecCenter = CVector(Fl(r, 4.f, 14), Fl(r, 4.f, 14), Fl(r, 3.f, 14));
    cm->m_boundSphere.m_fRadius = radius;
    reinterpret_cast<void**>(0xA9B0C8)[idx] = S(mi);
}
// peds: type 0..31 mostly, acquaintance masks with bits at game ped-type positions, an intelligence block behind m_pIntelligence
static void PedWorld(Rng& r, size_t pedOff, size_t intOff, int matSlot) {
    Ent(r, pedOff, matSlot, 3, 0, 0x7E0, false);
    FillGarbage(r, S(intOff), 0x600);
    auto* p = reinterpret_cast<CPed*>(S(pedOff));
    SAt<void*>(pedOff + 0x47C) = S(intOff);
    SAt<void*>(intOff) = p;
    p->m_nPedType = (ePedType)(r.below(10) == 0 ? (int)r.below(40) - 4 : (int)r.below(32));
    for (auto& a : p->m_acquaintance.m_acquaintances) a = r.below(4) == 0 ? r.u32() : (r.below(3) ? 0u : (r.u32() & r.u32() & r.u32()));
}
static void BuildAll(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    if (g_useRf) { rf::Enter(); rf::Reset(r, 0); }
    if (g_tweak) g_tweak(r);
}
template<class PF, class EF>
static bool UCase(Rng& r, std::string& d, PF&& pf, EF&& ef) {
    const uint32_t seed = r.u32() | 1;
    BuildAll(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    Reg img{ kImgLo, kImgHi - kImgLo };
    const bool ok = g_useRf ? BothA(d, seed, { sc, img }, std::forward<PF>(pf), std::forward<EF>(ef), kRf) : BothA(d, seed, { sc, img }, std::forward<PF>(pf), std::forward<EF>(ef));
    if (g_useRf) rf::Leave();
    return ok;
}
} // namespace w3a

struct Tu { explicit Tu(std::function<void(Rng&)> f, bool rf = false) { w3a::g_tweak = std::move(f); w3a::g_useRf = rf; } ~Tu() { w3a::g_tweak = nullptr; w3a::g_useRf = false; } };
template<class Fn> static void RunScaledA(const std::string& name, Fn&& fn, int cases) {
    static const int scale = std::getenv("W3A_SCALE") ? std::max(1, std::atoi(std::getenv("W3A_SCALE"))) : 1;
    static const int div = std::getenv("W3A_DIV") ? std::max(1, std::atoi(std::getenv("W3A_DIV"))) : 1;
    Run(name, std::forward<Fn>(fn), std::max(10, cases * scale / div));
}
#define ROW(NAME, ADDR, CASES) { const int _cs = (CASES); const std::string _nm = std::string(NAME " " #ADDR); RunScaledA(_nm, [&](Rng& r, std::string& d) -> bool
#define ENDROW , _cs); }
#include "wave3_oracle_ai_ped.inc"      // PedIntelligence.cpp
#include "wave3_oracle_ai_obj.inc"      // Object.cpp
#include "wave3_oracle_ai_car.inc"      // CarCtrl.cpp
#include "wave3_oracle_ai_car2.inc"     // CarCtrl.cpp (traffic geometry)
#include "wave3_oracle_ai_car3.inc"     // CarCtrl.cpp (objects / peds / cars in sector lists)
#include "wave3_oracle_ai_cover.inc"    // Cover.cpp
static void TestW3AiAll() {
    (void)spdlog::default_logger_raw();   // the default logger is created lazily by the first NOTSA_LOG_* of the port: do it outside the redirected allocator
    rf::Init();
    TestW3AiPed();
    TestW3AiObjA();
    TestW3AiObjB();
    TestW3AiCarA();
    TestW3AiCarB();
    TestW3AiCarC();
    TestW3AiCover();
}
