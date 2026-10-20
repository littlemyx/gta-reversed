// ORACLE-W3-EFFECTS: exe-oracle rows for the world-effect root files (Explosion, Fire, FireManager, CreepingFire, Glass, Rope, Ropes, Cranes, Bridge, Birds, Skidmarks, WaterCannon(s), Coronas, Clouds, PlantMgr, PlantSurfPropMgr, PPTriPlantBuffer, ProjectileInfo), classes A + B.
// Included by wave2_oracle_all.inc when W3_FX_ONLY is defined (executable wave3_fx_oracle_test = the W2 harness: deterministic arena, whole exe data image + the scratch block compared after every case).
//
// Fixture: the globals under test live at their exe addresses on both sides (NOTSA_ORACLE_ADDRESS_GLOBALS); a scratch block g_sc holds objects that are not exe globals (fire / pane / rope objects, entities) and out buffers.
// Every case rebuilds the fixture from the seed; both sides see identical memory; the image + scratch are compared after the call. Draw / audio / RenderWare callees are recorded by stubs on both sides.
#include "Explosion.h"
#include "Fire.h"
#include "FireManager.h"
#include "CreepingFire.h"
#include "Glass.h"
#include "Rope.h"
#include "Ropes.h"
#include "Cranes.h"
#include "Bridge.h"
#include "Birds.h"
#include "Skidmarks.h"
#include "WaterCannon.h"
#include "WaterCannons.h"
#include "Coronas.h"
#include "Clouds.h"
#include "PlantMgr.h"
#include "PlantSurfPropMgr.h"
#include "ProjectileInfo.h"
#include "Entity/Object/Projectile.h"
#include "Timer.h"
#include "Camera.h"
#include "World.h"
#include "Entity/Ped/Ped.h"
#include "Entity/Vehicle/Vehicle.h"

namespace w3f {
using namespace w2;
constexpr size_t kScSize = 0x20000;
alignas(16) static uint8_t g_sc[kScSize];
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& At(uintptr_t va) { return *reinterpret_cast<T*>(va); }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
static uint32_t P32(const void* p) { return (uint32_t)(uintptr_t)p; }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static inline uint64_t B8(bool b) { return b ? 1 : 0; }
static inline uint64_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
static std::function<void(Rng&)> g_tweak;     // row-specific fixture work after the standard build
static float Fl(Rng& r, float scale, int special = 14) {
    if (special && r.below((uint32)special) == 0) return GenF(r, scale, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF);
    return GF(r, scale);
}
// as Fl, without denormal / tiny operands: x87 keeps a wider exponent range than float in the intermediates (denormal results differ by an ulp; not game values)
static float FlN(Rng& r, float scale, int special = 14) {
    if (special && r.below((uint32)special) == 0) return GenF(r, scale, S_NAN | S_ZERO | S_INF);
    const float v = GenF(r, scale, S_ZERO);
    return (v != 0.f && std::fabs(v) < 1e-4f) ? 0.5f : v;
}
static float GN3(Rng& r, float scale) { const float v = GenF(r, scale, S_ZERO); return (v != 0.f && std::fabs(v) < 1e-4f) ? 0.5f : v; }   // game-range value (zero allowed, no denormal / tiny / NaN / inf)
static void BuildAll(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    if (g_tweak) g_tweak(r);
}
template<class PF, class EF>
static bool UCase(Rng& r, std::string& d, PF&& pf, EF&& ef) {
    const uint32_t seed = r.u32() | 1;
    BuildAll(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    Reg img{ kImgLo, kImgHi - kImgLo };
    return BothA(d, seed, { sc, img }, std::forward<PF>(pf), std::forward<EF>(ef));
}
} // namespace w3f

struct Tu { explicit Tu(std::function<void(Rng&)> f) { w3f::g_tweak = std::move(f); } ~Tu() { w3f::g_tweak = nullptr; } };
template<class Fn> static void RunScaledF(const std::string& name, Fn&& fn, int cases) {
    static const int scale = std::getenv("W3F_SCALE") ? std::max(1, std::atoi(std::getenv("W3F_SCALE"))) : 1;
    static const int div = std::getenv("W3F_DIV") ? std::max(1, std::atoi(std::getenv("W3F_DIV"))) : 1;   // quick runs while developing
    Run(name, std::forward<Fn>(fn), std::max(10, cases * scale / div));
}
#define ROW(NAME, ADDR, CASES) { const int _cs = (CASES); const std::string _nm = std::string(NAME " " #ADDR); RunScaledF(_nm, [&](Rng& r, std::string& d) -> bool
#define ENDROW , _cs); }
#include "wave3_oracle_fx_a.inc"      // Bridge
#include "wave3_oracle_fx_b.inc"      // Cranes / Explosion / Fire / FireManager / CreepingFire
#include "wave3_oracle_fx_c.inc"      // Glass / Birds / Skidmarks / ProjectileInfo / WaterCannon(s) / Ropes
#include "wave3_oracle_fx_d.inc"      // Clouds / PlantMgr / PlantSurfPropMgr / PPTriPlantBuffer
#include "wave3_oracle_fx_e.inc"      // Coronas / Clouds::VolumetricCloudsGetMaxDistance
#include "wave3_oracle_fx_f.inc"      // class C: CFire::Extinguish and the fire manager functions around it
#include "wave3_oracle_fx_g.inc"      // ProjectileInfo (IsProjectileInRange, RemoveIfThisIsAProjectile not-found path)
static void TestW3FxAll() {
    (void)spdlog::default_logger_raw();   // the default logger is created lazily by the first NOTSA_LOG_* of the port: do it outside the redirected allocator
    TestW3FxA();
    TestW3FxB();
    TestW3FxC();
    TestW3FxD();
    TestW3FxE();
    TestW3FxF();
    TestW3FxG();
}
