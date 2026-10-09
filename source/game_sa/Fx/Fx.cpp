/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"

#include "Fx.h"
#include "Localisation.h"
#include "Shadows.h"
#include "Camera.h"
#include "SurfaceInfos_c.h"
#include "Timer.h"
#include "Vehicle.h"

static auto& TempVertexBuffer = StaticRef<std::array<RxObjSpace3DVertex, 4>>(0xC4D958);

auto& g_fx = StaticRef<Fx_c>(0xA9AE00);

// Debris prim rotation counter, lives right before `g_fx`
static auto& s_DebrisPrimIdx = StaticRef<int32>(0xA9ADE4);

namespace {
// The original's camera distance checks accumulate the squares in extended precision, the term order differs per function.
// (z^2 + x^2) + y^2   (a + b == b + a, so this is also (x^2 + z^2) + y^2)
double CamDistSq_ZXY(const CVector& pos) {
    const auto& cam = TheCamera.GetPosition();
    const double dx = (double)cam.x - pos.x;
    const double dy = (double)cam.y - pos.y;
    const double dz = (double)cam.z - pos.z;
    return (dz * dz + dx * dx) + dy * dy;
}
double CamDistSq_XZY(const CVector& pos) { return CamDistSq_ZXY(pos); }

// (z^2 + y^2) + x^2
double CamDistSq_ZYX(const CVector& pos) {
    const auto& cam = TheCamera.GetPosition();
    const double dx = (double)cam.x - pos.x;
    const double dy = (double)cam.y - pos.y;
    const double dz = (double)cam.z - pos.z;
    return (dz * dz + dy * dy) + dx * dx;
}

// `rand() % 10000 * 1e-4f` (0x821B1E, 0x858FC4)
double RandFrac10000() {
    return (double)(rand() % 10000) * (double)1e-4f;
}

// 0x59C810 - `v` transformed by the transposed rotation of `m` (x87: extended precision, this term order)
CVector InverseTransformVectorOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return CVector{
        (float)(((double)r.y * v.y + (double)r.z * v.z) + (double)v.x * r.x),
        (float)(((double)f.y * v.y + (double)f.x * v.x) + (double)f.z * v.z),
        (float)(((double)u.y * v.y + (double)u.x * v.x) + (double)u.z * v.z)
    };
}

struct WheelFxColor {
    float red, green, blue;
};

// Shared body of `Fx_c::AddWheel{Grass,Gravel,Mud}`: identical code in the original, only the color differs
void AddWheelDirt(Fx_c& fx, CVehicle* vehicle, const CVector& pos, float lightMult, WheelFxColor color) {
    if (vehicle->m_pDriver != FindPlayerPed(0) && vehicle->m_pDriver != FindPlayerPed(1)) { // 0x56E210
        return;
    }

    auto* const playerVeh = FindPlayerVehicle(-1, false); // 0x56E0D0

    const double d2 = CamDistSq_ZYX(pos);
    if (!(d2 <= 625.0)) { // 0x85A6E8 (FCOM + JP: NaN returns)
        return;
    }
    const auto frameAndModel = [&] { return (uint8)vehicle->m_nModelIndex + CTimer::GetFrameCounter(); }; // byte [vehicle + 0x22]
    if (d2 > 400.0) { // 0x85A700
        if (frameAndModel() & 3) {
            return;
        }
    } else if (d2 > 64.0 || !playerVeh) { // 0x859A44
        if (frameAndModel() & 1) {
            return;
        }
    }

    FxPrtMult_c fxMults{ color.red, color.green, color.blue, 1.0f, 0.0f, 0.0f, 0.05f }; // 0x4AB290
    for (auto i = 0; i < 3; i++) {
        fxMults.m_fSize = (float)((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL * (double)0.03f + (double)0.03f); // 0x821B1E, 0x858C7C, 0x858B10

        float tmp = vehicle->m_vecMoveSpeed.x * -1.5f; // 0x85A704
        CVector vel;
        vel.x = (float)((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL * (double)tmp);
        tmp   = vehicle->m_vecMoveSpeed.y * -1.5f;
        vel.y = (float)((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL * (double)tmp);

        CVector partPos = pos;
        vel.z = (float)((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL * (double)1.5f + 2.0); // 0x858CE8, 0x858CA0

        partPos.x = (float)((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL * (double)0.4f + (double)partPos.x - (double)0.2f); // 0x858EE8, 0x858CC4
        partPos.y = (float)((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL * (double)0.4f + (double)partPos.y - (double)0.2f);

        fx.m_WheelDirt->AddParticle(partPos, vel, 0.0f, fxMults, -1.0f, lightMult, 0.6f, false); // 0x4AA440
    }
}

// Shared body of `Fx_c::AddWheel{Sand,Dust}`: nearly identical code in the original (color + particle life differ)
void AddWheelSandOrDust(Fx_c& fx, CVehicle* vehicle, const CVector& pos, bool bWheelsSpinning, float lightMult, WheelFxColor color, bool isDust) {
    auto* const playerVeh = FindPlayerVehicle(-1, false); // 0x56E0D0

    const float d2 = (float)CamDistSq_ZYX(pos); // stored to a float temp in the original
    if (!(d2 <= 625.0f)) { // 0x85A6E8 (FCOMP + JP: NaN returns)
        return;
    }

    const auto fxQuality = fx.m_FxQuality;
    const auto frameAndModel = CTimer::GetFrameCounter() + (uint32)(int32)(int16)vehicle->m_nModelIndex; // MOVSX word [vehicle + 0x22]
    if (fxQuality >= FX_QUALITY_MEDIUM) {
        if (frameAndModel & 1) {
            return;
        }
        if (d2 > 64.0f || !playerVeh) { // 0x859A44
            if (frameAndModel & 3) {
                return;
            }
        }
    } else if (fxQuality == FX_QUALITY_LOW) {
        if (frameAndModel & 3) {
            return;
        }
        if (d2 > 64.0f || !playerVeh) {
            if (frameAndModel & 7) {
                return;
            }
        }
    }

    FxPrtMult_c fxMults{ color.red, color.green, color.blue, 0.5f, 1.0f, 0.0f, 0.0f }; // 0x4AB290

    const float absGasPedal = std::fabs(vehicle->m_GasPedal); // vehicle + 0x49C

    const auto& moveSpeed = vehicle->m_vecMoveSpeed;
    const auto Speed = [&] { return std::sqrt(((double)moveSpeed.x * moveSpeed.x + (double)moveSpeed.y * moveSpeed.y) + (double)moveSpeed.z * moveSpeed.z); };
    double speedMult; // stays on the x87 stack (no float rounding)
    if (bWheelsSpinning) {
        speedMult = 1.0;
    } else if (Speed() + Speed() > 1.0) { // 0x858624 (NaN takes the else branch)
        speedMult = 1.0;
    } else {
        speedMult = Speed() + Speed();
    }

    // 0x858B1C = 0.1f, 0x858C20 = 0.9f, 0x858C28 = 0.05f
    fxMults.m_fLife = isDust
        ? (float)((double)0.05f * speedMult + (double)0.1f)
        : (float)((1.0 + speedMult) * (double)0.1f);
    fxMults.m_fSize = (float)((double)0.9f * speedMult + (double)0.1f);

    // Smaller particles for bikes
    double stepMult;
    float  sizeMult;
    switch (vehicle->m_nVehicleSubType) {
    case VEHICLE_TYPE_BMX:
        stepMult = 2.0;
        sizeMult = 0.25f; // 0x858C84
        break;
    case VEHICLE_TYPE_BIKE:
    case VEHICLE_TYPE_QUAD:
        stepMult = 2.0;
        sizeMult = 0.5f; // 0x858B8C
        break;
    default:
        stepMult = 1.5f; // 0x858CE8
        sizeMult = 0.7f; // 0x858CB0
        break;
    }
    fxMults.m_fSize = (float)((double)fxMults.m_fSize * (double)sizeMult);

    // Distance moved this frame -> number of particles
    const float  moveX  = CTimer::GetTimeStep() * moveSpeed.x;
    const float  moveY  = CTimer::GetTimeStep() * moveSpeed.y;
    const double moveZd = (double)CTimer::GetTimeStep() * moveSpeed.z;
    const float  moveZ  = (float)moveZd;
    const double length = std::sqrt((moveZd * (double)moveZ + (double)moveY * moveY) + (double)moveX * moveX);
    const int32  numParticles = std::max(1, (int32)(length * stepMult)); // 0x821B40 (ftol)

    const float velMult = (float)((speedMult + (double)0.8f) - (double)0.2f); // 0x858C98, 0x858CC4
    const float numParticlesF = (float)numParticles;
    for (auto i = 0; i < numParticles; i++) {
        CVector vel;
        float   tmp = (float)((double)absGasPedal * moveSpeed.x * (double)-40.0f); // 0x85A708
        vel.x = (float)((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL * (double)tmp); // 0x821B1E, 0x858C7C
        tmp   = (float)((double)absGasPedal * moveSpeed.y * (double)-40.0f);
        vel.y = (float)((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL * (double)tmp);
        vel.z = (float)((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL * (double)velMult + (double)0.2f); // 0x858CC4

        const double f = 1.0 - (double)i / (double)numParticlesF;
        const double offX = (double)moveX * f;
        const float  offY = (float)((double)moveY * f);
        const float  offZ = (float)((double)moveZ * f);
        const CVector partPos{
            (float)((double)pos.x - offX),
            (float)((double)pos.y - (double)offY),
            (float)((double)pos.z - (double)offZ)
        };

        fx.m_Sand->AddParticle(partPos, vel, 0.0f, fxMults, -1.0f, lightMult, 0.7f, false); // 0x4AA440
    }
}
} // namespace

void Fx_c::InjectHooks() {
    RH_ScopedClass(Fx_c);
    RH_ScopedCategory("Fx");

    // + RH_ScopedInstall(Constructor, 0x49E620);
    // + RH_ScopedInstall(Destructor, 0x49E630);
    // + RH_ScopedInstall(InitStaticSystems, 0x49E660);
    // + RH_ScopedInstall(ExitStaticSystems, 0x49E850);
    // + RH_ScopedInstall(InitEntitySystems, 0x49EA60);
    // + RH_ScopedInstall(ExitEntitySystems, 0x4A12D0);
    // + RH_ScopedInstall(Init, 0x49EA90);
    // + RH_ScopedInstall(Exit, 0x4A1320);
    // + RH_ScopedInstall(Reset, 0x49EAE0);
    // + RH_ScopedInstall(CreateEntityFx, 0x4A11E0);
    // + RH_ScopedInstall(DestroyEntityFx, 0x4A1280);
    // + RH_ScopedInstall(Update, 0x49E640);
    // + RH_ScopedInstall(Render, 0x49E650);
    RH_ScopedInstall(CreateMatFromVec, 0x49E950);
    // + RH_ScopedInstall(SetFxQuality, 0x49EA40);
    // + RH_ScopedInstall(GetFxQuality, 0x49EA50);
    RH_ScopedInstall(AddBlood, 0x49EB00);
    RH_ScopedInstall(AddWood, 0x49EE10);
    RH_ScopedInstall(AddSparks, 0x49F040);
    RH_ScopedInstall(AddTyreBurst, 0x49F300);
    RH_ScopedInstall(AddBulletImpact, 0x49F3D0);
    RH_ScopedInstall(AddPunchImpact, 0x49F670);
    // RH_ScopedInstall(AddDebris, 0x49F750);
    // RH_ScopedInstall(AddGlass, 0x49F970);
    // RH_ScopedInstall(AddWheelSpray, 0x49FB30);
    // RH_ScopedInstall(AddWheelGrass, 0x49FF20);
    // RH_ScopedInstall(AddWheelGravel, 0x4A0170);
    // RH_ScopedInstall(AddWheelMud, 0x4A03C0);
    // RH_ScopedInstall(AddWheelSand, 0x4A0610);
    // RH_ScopedInstall(AddWheelDust, 0x4A09C0);
    // RH_ScopedInstall(TriggerWaterHydrant, 0x4A0D70);
    // RH_ScopedInstall(TriggerGunshot, 0x4A0DE0);
    // RH_ScopedInstall(TriggerTankFire, 0x4A0FA0);
    RH_ScopedInstall(TriggerWaterSplash, 0x4A1070);
    RH_ScopedInstall(TriggerBulletSplash, 0x4A10E0);
    RH_ScopedInstall(TriggerFootSplash, 0x4A1150);

    RH_ScopedGlobalInstall(RenderAddTri_, 0x4A1410);
    RH_ScopedGlobalInstall(RenderEnd, 0x4A1600);
    RH_ScopedGlobalInstall(RenderBegin, 0x4A13B0);
    RH_ScopedGlobalInstall(RotateVecIntoVec, 0x4A1660);
    RH_ScopedGlobalInstall(RotateVecAboutVec, 0x4A1780);
}

Fx_c* Fx_c::Constructor() { this->Fx_c::Fx_c(); return this; }
Fx_c* Fx_c::Destructor() { this->Fx_c::~Fx_c(); return this; }

// 0x49E660
void Fx_c::InitStaticSystems() {
    m_Blood          = g_fxMan.CreateFxSystem("prt_blood",            CVector{}, nullptr, true);
    m_BoatSplash     = g_fxMan.CreateFxSystem("prt_boatsplash",       CVector{}, nullptr, true);
    m_Bubble         = g_fxMan.CreateFxSystem("prt_bubble",           CVector{}, nullptr, true);
    m_Cardebris      = g_fxMan.CreateFxSystem("prt_cardebris",        CVector{}, nullptr, true);
    m_CollisionSmoke = g_fxMan.CreateFxSystem("prt_collisionsmoke",   CVector{}, nullptr, true);
    m_GunShell       = g_fxMan.CreateFxSystem("prt_gunshell",         CVector{}, nullptr, true);
    m_Sand           = g_fxMan.CreateFxSystem("prt_sand",             CVector{}, nullptr, true);
    m_Sand2          = g_fxMan.CreateFxSystem("prt_sand2",            CVector{}, nullptr, true);
    m_SmokeHuge      = g_fxMan.CreateFxSystem("prt_smoke_huge",       CVector{}, nullptr, true);
    m_SmokeII3expand = g_fxMan.CreateFxSystem("prt_smokeII_3_expand", CVector{}, nullptr, true);
    m_Spark          = g_fxMan.CreateFxSystem("prt_spark",            CVector{}, nullptr, true);
    m_Spark2         = g_fxMan.CreateFxSystem("prt_spark_2",          CVector{}, nullptr, true);
    m_Splash         = g_fxMan.CreateFxSystem("prt_splash",           CVector{}, nullptr, true);
    m_Wake           = g_fxMan.CreateFxSystem("prt_wake",             CVector{}, nullptr, true);
    m_WaterSplash    = g_fxMan.CreateFxSystem("prt_watersplash",      CVector{}, nullptr, true);
    m_WheelDirt      = g_fxMan.CreateFxSystem("prt_wheeldirt",        CVector{}, nullptr, true);
    m_Glass          = g_fxMan.CreateFxSystem("prt_glass",            CVector{}, nullptr, true);
}

// 0x49E850
void Fx_c::ExitStaticSystems() {
    g_fxMan.DestroyFxSystem(m_Blood);
    g_fxMan.DestroyFxSystem(m_BoatSplash);
    g_fxMan.DestroyFxSystem(m_Bubble);
    g_fxMan.DestroyFxSystem(m_Cardebris);
    g_fxMan.DestroyFxSystem(m_CollisionSmoke);
    g_fxMan.DestroyFxSystem(m_GunShell);
    g_fxMan.DestroyFxSystem(m_Sand);
    g_fxMan.DestroyFxSystem(m_Sand2);
    g_fxMan.DestroyFxSystem(m_SmokeHuge);
    g_fxMan.DestroyFxSystem(m_SmokeII3expand);
    g_fxMan.DestroyFxSystem(m_Spark);
    g_fxMan.DestroyFxSystem(m_Spark2);
    g_fxMan.DestroyFxSystem(m_Splash);
    g_fxMan.DestroyFxSystem(m_Wake);
    g_fxMan.DestroyFxSystem(m_WaterSplash);
    g_fxMan.DestroyFxSystem(m_WheelDirt);
    g_fxMan.DestroyFxSystem(m_Glass);
}

// 0x49EA60
void Fx_c::InitEntitySystems() {
    // NOP
}

// NOTSA
static void CreateFxWithinCameraRange(const char* name, const CVector& pos, float range) {
    if (DistanceBetweenPointsSquared(TheCamera.GetPosition(), pos) <= range) {
        if (auto* fxSystem = g_fxMan.CreateFxSystem(name, pos, nullptr, false)) {
            fxSystem->PlayAndKill();
        }
    }
}

// 0x4A12D0
void Fx_c::ExitEntitySystems() {
    for (auto it = m_FxEntities.GetHead(); it; it = m_FxEntities.GetNext(it)) {
        m_FxEntities.RemoveItem(it);
        g_fxMan.DestroyFxSystem(it->m_System);
        delete it;
    }
}

// 0x49EA90
void Fx_c::Init() {
    ZoneScoped;

    g_fxMan.Init();
    g_fxMan.LoadFxProject("models\\effects.fxp");
    g_fxMan.SetWindData(&CWeather::WindDir, &CWeather::Wind);
    InitStaticSystems();
    InitEntitySystems();
    m_Randomizer = 0;
}

// 0x4A1320
void Fx_c::Exit() {
    ExitEntitySystems();
    ExitStaticSystems();
    g_fxMan.Exit();
}

// 0x49EAE0
void Fx_c::Reset() {
    g_fxMan.DestroyAllFxSystems();
    InitStaticSystems();
    InitEntitySystems(); // NOTSA
}

// 0x4A11E0
void Fx_c::CreateEntityFx(CEntity* entity, const char* fxName, const CVector& pos, RwMatrix* transform) {
    // ((void(__thiscall*)(Fx_c*, CEntity*, char*, RwV3d*, RwMatrix*))0x4A11E0)(this, entity, fxName, pos transform);

    auto* particle = g_fxMan.CreateFxSystem(fxName, pos, transform, true);
    if (particle) {
        auto it = new FxEntitySystem();
        it->m_System = particle;
        it->m_Entity = entity;
        m_FxEntities.AddItem(it);
        it->m_System->Play();
    }
}

// 0x4A1280
void Fx_c::DestroyEntityFx(CEntity* entity) {
    // ((void(__thiscall*)(Fx_c*, CEntity*))0x4A1280)(this, entity);

    for (auto it = m_FxEntities.GetHead(); it; it = m_FxEntities.GetNext(it)) {
        if (it->m_Entity == entity) {
            m_FxEntities.RemoveItem(it);
            it->m_System->Kill();
            operator delete(it);
        }
    }
}

// 0x49E640
void Fx_c::Update(RwCamera* camera, float timeDelta) {
    ZoneScoped;

    g_fxMan.Update(camera, timeDelta);
}

// 0x49E650
void Fx_c::Render(RwCamera* camera, bool heatHaze) {
    ZoneScoped;

    g_fxMan.Render(camera, heatHaze);
}

// 0x49E950
void Fx_c::CreateMatFromVec(RwMatrix* out, const CVector* origin, const CVector* direction) {
    // Identity (written field by field)
    out->right = { 1.0f, 0.0f, 0.0f };
    out->up    = { 0.0f, 1.0f, 0.0f };
    out->at    = { 0.0f, 0.0f, 1.0f };
    out->pos   = { 0.0f, 0.0f, 0.0f };
    out->flags |= 0x20003; // rwMATRIXTYPEORTHONORMAL | rwMATRIXINTERNALIDENTITY

    out->pos = *origin;
    out->up  = *direction;
    RwV3dNormalize(&out->up, &out->up); // 0x7ED9B0

    // Cross products with (0, 0, 1)-like constants, the intermediates stay on the x87 stack (no float rounding)
    const double ux = out->up.x, uy = out->up.y, uz = out->up.z;
    const double t = uz * 0.0;               // 0x858B50
    const double a = t - uy * -1.0;          // 0x858C1C
    const double b = ux * -1.0 - t;
    const double c = uy * 0.0 - ux * 0.0;
    out->right.x = (float)a;
    out->right.y = (float)b;
    out->right.z = (float)c;
    out->at.x = (float)(b * uz - c * uy);
    out->at.y = (float)(c * ux - a * uz);
    out->at.z = (float)(a * uy - b * ux);

    RwMatrixUpdate(out); // 0x7F18A0
}

// 0x49EA40
void Fx_c::SetFxQuality(FxQuality_e quality) {
    m_FxQuality = quality;
}

// 0x49EA50
FxQuality_e Fx_c::GetFxQuality() const {
    return m_FxQuality;
}

// 0x49EB00
void Fx_c::AddBlood(const CVector& pos, const CVector& direction, int32 amount, float lightMult) {
    if (!CLocalisation::Blood()) { // 0x56D230
        return;
    }

    // Distance check, accumulated in extended precision
    {
        const auto& cam = TheCamera.GetPosition();
        const double dx = (double)cam.x - pos.x;
        const double dy = (double)cam.y - pos.y;
        const double dz = (double)cam.z - pos.z;
        if (dx * dx + dy * dy + dz * dz > 625.0) { // 0x85A6E8 = 25^2 (FCOMP + JE: NaN passes)
            return;
        }
    }

    // Wrap the 0..9999 random into a [0..1) float multiplier
    const auto Rand10000 = [] { return (double)(rand() % 10000) * (double)1e-4f; }; // 0x821B1E, 0x858FC4

    FxPrtMult_c fxMults{ 0.5f, 0.0f, 0.0f, 1.0f, 0.8f, 0.0f, 0.8f }; // 0x4AB290
    for (auto i = 0; i < amount; i++) {
        fxMults.m_fSize = (float)(Rand10000() * (double)0.3f + (double)0.7f); // 0x858C24, 0x858CB0

        CVector vel{ direction.x * 1.5f, direction.y * 1.5f, direction.z * 1.5f }; // 0x858CE8
        vel.x = (float)(Rand10000() * 2.0 - 1.0 + vel.x);
        vel.y = (float)(Rand10000() * 2.0 - 1.0 + vel.y);
        vel.z = (float)(Rand10000() * 2.0 - 1.0 + vel.z);

        m_Blood->AddParticle(pos, vel, 0.0f, fxMults, -1.0f, lightMult, 0.6f, false); // 0x4AA440
    }

    CVector dropPos{ direction.x * 0.5f + pos.x, direction.y * 0.5f + pos.y, direction.z * 0.5f + pos.z }; // 0x858B8C
    dropPos.x = (float)(Rand10000() * (double)0.2f - (double)0.1f + dropPos.x); // 0x858CC4, 0x858B1C
    const auto yRand = Rand10000();
    m_Randomizer++;
    dropPos.y = (float)(yRand * (double)0.2f - (double)0.1f + dropPos.y);
    dropPos.z += 1.0f;

    switch (m_Randomizer & 7) {
    case 5:
        CShadows::AddPermanentShadow(SHADOW_DEFAULT, gpBloodPoolTex, &dropPos, 0.1f, 0.0f, 0.0f, -0.1f, 255, 200, 0, 0, 4.0f, (rand() & 0xFFF) + 2000, 1.0f); // 0x706F60
        break;
    case 2:
        CShadows::AddPermanentShadow(SHADOW_DEFAULT, gpBloodPoolTex, &dropPos, 0.2f, 0.0f, 0.0f, -0.2f, 255, 200, 0, 0, 4.0f, (rand() & 0xFFF) + 8000, 1.0f); // 0x706F60
        break;
    }
}

// 0x49EE10
void Fx_c::AddWood(const CVector& pos, const CVector& direction, int32 amount, float lightMult) {
    if (CamDistSq_XZY(pos) > 625.0) { // 0x85A6E8 (FCOMP + JZ: NaN passes)
        return;
    }

    FxPrtMult_c fxMults{ 0.5f, 0.25f, 0.0f, 1.0f, 0.3f, 0.0f, 1.0f }; // 0x4AB290
    for (auto i = 0; i < amount; i++) {
        fxMults.m_Color.red   = (float)(RandFrac10000() * (double)0.12f + (double)0.13f);  // 0x85A6F8, 0x859020
        fxMults.m_Color.green = (float)(RandFrac10000() * (double)0.03f + (double)0.12f);  // 0x85A6F4, 0x85A6F0
        fxMults.m_Color.blue  = (float)(RandFrac10000() * (double)0.03f + (double)0.04f);  // 0x85A6EC, 0x858CEC
        fxMults.m_fSize       = (float)(RandFrac10000() * (double)0.3f  + (double)0.7f);   // 0x858C24, 0x858CB0

        CVector vel{ direction.x * 4.0f, direction.y * 4.0f, direction.z * 4.0f }; // 0x858B90
        vel.x = (float)(RandFrac10000() * 4.0 - 2.0 + vel.x); // 0x858B90, 0x858CA0
        vel.y = (float)(RandFrac10000() * 4.0 - 2.0 + vel.y);
        vel.z = (float)(RandFrac10000() * 4.0 - 2.0 + vel.z);

        g_fx.m_Blood->AddParticle(pos, vel, 0.0f, fxMults, -1.0f, lightMult, 0.6f, false); // 0x4AA440
    }
}

// 0x49F040
void Fx_c::AddSparks(const CVector& origin, const CVector& direction, float force, int32 amount, CVector across, eSparkType sparksType, float spread, float life) {
    const double d2 = CamDistSq_ZYX(origin);
    if (d2 > 22500.0) { // 0x85A6FC (FCOM + JZ: NaN passes)
        return;
    }
    if (d2 > 225.0 && (CTimer::GetFrameCounter() & 1)) { // 0x8599D4 (NaN does not skip)
        return;
    }

    FxPrtMult_c fxMults{ 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, (float)((double)life * (double)0.8f) }; // 0x4AB290, 0x858C98

    const CVector acrossStep{ across.x * CTimer::GetTimeStep(), across.y * CTimer::GetTimeStep(), across.z * CTimer::GetTimeStep() };
    if (amount <= 0) {
        return;
    }

    const float amountF = (float)amount;
    const float nSpread = -spread;
    const float range   = (float)((double)spread - (double)nSpread);
    for (auto i = 0; i < amount; i++) {
        CVector dir = direction;
        const float f = (float)(1.0 - (double)i / (double)amountF); // 0x858624

        // 0x858C7C = 1/32767
        dir.x = (float)((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL * ((double)spread - (double)nSpread) + (double)nSpread + (double)dir.x); // 0x821B1E
        dir.y = (float)((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL * ((double)spread - (double)nSpread) + (double)nSpread + (double)dir.y);
        const double dirZ = (double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL * (double)range + (double)nSpread + (double)dir.z; // stays unrounded on the x87 stack

        const CVector vel{
            (float)((double)dir.x * (double)force),
            (float)((double)dir.y * (double)force),
            (float)(dirZ * (double)force)
        };

        const double stepX = (double)acrossStep.x * (double)f;
        const double stepY = (double)acrossStep.y * (double)f;
        const float  stepZ = (float)((double)acrossStep.z * (double)f);
        const CVector pos{
            (float)((double)origin.x - stepX),
            (float)((double)origin.y - stepY),
            (float)((double)origin.z - (double)stepZ)
        };

        auto* const sys = sparksType != SPARK_PARTICLE_SPARK2 ? m_Spark : m_Spark2;
        sys->AddParticle(pos, vel, (float)((double)f * (double)0.05f), fxMults, -1.0f, 1.2f, 0.6f, false); // 0x858C28, 0x4AA440
    }
}

// 0x49F300
void Fx_c::AddTyreBurst(const CVector& posn, const CVector& velocity) {
    if (CamDistSq_XZY(posn) > 625.0) { // 0x85A6E8 (FCOMP + JZ: NaN passes)
        return;
    }

    const FxPrtMult_c fxMults{ 1.0f, 1.0f, 1.0f, 0.4f, 0.12f, 0.0f, 0.1f }; // 0x4AB290
    for (auto i = 0; i < 4; i++) {
        m_SmokeII3expand->AddParticle(posn, velocity, (float)((double)i * (double)0.05f), fxMults, -1.0f, 1.2f, 0.6f, false); // 0x858C28, 0x4AA440
    }
}

// 0x49F3D0
// NOTE: `bulletFxType` is the surface ID (-> `SurfaceInfos_c::GetBulletFx`), `arg4` is the light multiplier
void Fx_c::AddBulletImpact(const CVector& posn, const CVector& direction, int32 bulletFxType, int32 amount, float arg4) {
    const auto bulletFx = g_surfaceInfos.GetBulletFx(bulletFxType); // 0x55E670
    if (!bulletFx) {
        return;
    }
    if (CamDistSq_ZXY(posn) > 22500.0) { // 0x85A6FC (FCOMP + JZ: NaN passes)
        return;
    }

    switch (bulletFx) {
    case 1: { // Sparks + smoke
        g_fx.AddSparks(posn, direction, 3.0f, amount, CVector{ 0.0f, 0.0f, 0.0f }, SPARK_PARTICLE_SPARK, 0.4f, 1.0f); // 0x49F040

        FxPrtMult_c fxMults{ 1.0f, 1.0f, 1.0f, 0.15f, 0.4f, 0.0f, 0.075f }; // 0x4AB290
        int32 numParticles = 2;
        if (amount >= 8) {
            numParticles = 1;
            fxMults.m_Color.alpha += fxMults.m_Color.alpha;
        }
        for (auto i = 0; i < numParticles; i++) {
            g_fx.m_SmokeII3expand->AddParticle(posn, direction, (float)((double)i * (double)0.05f), fxMults, -1.0f, arg4, 0.6f, false); // 0x858C28, 0x4AA440
        }
        break;
    }
    case 2:
    case 4: { // Sand-like dust
        FxPrtMult_c fxMults{ 0.81f, 0.67f, 0.57f, 0.15f, 0.4f, 0.0f, 0.3f }; // 0x4AB290
        if (bulletFx == 4) {
            fxMults.m_Color.red = fxMults.m_Color.green = fxMults.m_Color.blue = 0.6f;
        }
        int32 numParticles = 4;
        if (amount >= 8) {
            numParticles = 2;
            fxMults.m_Color.alpha += fxMults.m_Color.alpha;
        }
        for (auto i = 0; i < numParticles; i++) {
            const CVector vel{ direction.x * 0.3f, direction.y * 0.3f, direction.z * 0.3f }; // 0x858C24
            g_fx.m_Sand->AddParticle(posn, vel, (float)((double)i * (double)0.05f), fxMults, -1.0f, arg4, 0.6f, false); // 0x858C28, 0x4AA440
        }
        break;
    }
    case 3: // Wood
        g_fx.AddWood(posn, direction, (int32)((double)amount * 0.5), 1.0f); // 0x858B8C, 0x49EE10
        break;
    default:
        break;
    }
}

// 0x49F670
// NOTE: the 3rd argument is unused by the original (RET 0xC, never read)
void Fx_c::AddPunchImpact(const CVector& pos, const CVector& velocity, int32 num) {
    const auto& cam = TheCamera.GetPosition();
    const double dx = (double)cam.x - pos.x;
    const double dy = (double)cam.y - pos.y;
    const double dz = (double)cam.z - pos.z;
    if (dx * dx + dy * dy + dz * dz > 625.0) { // 0x85A6E8 (FCOMP + JE: NaN passes)
        return;
    }

    const FxPrtMult_c fxMults{ 1.0f, 1.0f, 1.0f, 0.4f, 0.1f, 0.0f, 0.1f }; // 0x4AB290
    m_SmokeII3expand->AddParticle(pos, velocity, 0.0f,  fxMults, -1.0f, 1.2f, 0.6f, false); // 0x4AA440
    m_SmokeII3expand->AddParticle(pos, velocity, 0.05f, fxMults, -1.0f, 1.2f, 0.6f, false); // 0x4AA440
}

// 0x49F750
void Fx_c::AddDebris(const CVector& pos, const RwRGBA& color, float scale, int32 amount) {
    ((void(__thiscall*)(Fx_c*, const CVector&, const RwRGBA&, float, int32))0x49F750)(this, pos, color, scale, amount);
}

// 0x49F970
void Fx_c::AddGlass(const CVector& pos, const RwRGBA& color, float scale, int32 amount) {
    ((void(__thiscall*)(Fx_c*, const CVector&, const RwRGBA&, float, int32))0x49F970)(this, pos, color, scale, amount);
}

// 0x49FB30
void Fx_c::AddWheelSpray(CVehicle* vehicle, CVector pos, bool bWheelsSpinning, bool bInWater, float lightMult) {
    ((void(__thiscall*)(Fx_c*, CVehicle*, CVector, uint8, uint8, float))0x49FB30)(this, vehicle, pos, bWheelsSpinning, bInWater, lightMult);
}

// 0x49FF20
void Fx_c::AddWheelGrass(CVehicle* vehicle, CVector pos, bool bWheelsSpinning, float lightMult) {
    ((void(__thiscall*)(Fx_c*, CVehicle*, CVector, uint8, float))0x49FF20)(this, vehicle, pos, bWheelsSpinning, lightMult);
}

// 0x4A0170
void Fx_c::AddWheelGravel(CVehicle* vehicle, CVector pos, bool bWheelsSpinning, float lightMult) {
    ((void(__thiscall*)(Fx_c*, CVehicle*, CVector, uint8, float))0x4A0170)(this, vehicle, pos, bWheelsSpinning, lightMult);
}

// 0x4A03C0
void Fx_c::AddWheelMud(CVehicle* vehicle, CVector pos, bool bWheelsSpinning, float lightMult) {
    ((void(__thiscall*)(Fx_c*, CVehicle*, CVector, uint8, float))0x4A03C0)(this, vehicle, pos, bWheelsSpinning, lightMult);
}

// 0x4A0610
void Fx_c::AddWheelSand(CVehicle* vehicle, CVector pos, bool bWheelsSpinning, float lightMult) {
    ((void(__thiscall*)(Fx_c*, CVehicle*, CVector, uint8, float))0x4A0610)(this, vehicle, pos, bWheelsSpinning, lightMult);
}

// 0x4A09C0
void Fx_c::AddWheelDust(CVehicle* vehicle, CVector pos, bool bWheelsSpinning, float lightMult) {
    ((void(__thiscall*)(Fx_c*, CVehicle*, CVector, uint8, float))0x4A09C0)(this, vehicle, pos, bWheelsSpinning, lightMult);
}

// 0x4A0D70
void Fx_c::TriggerWaterHydrant(const CVector& pos) {
    ((void(__thiscall*)(Fx_c*, const CVector&))0x4A0D70)(this, pos);
}

// 0x4A0DE0
void Fx_c::TriggerGunshot(CEntity* entity, const CVector& origin, const CVector& target, bool doGunflash) {
    ((void(__thiscall*)(Fx_c*, CEntity*, const CVector&, const CVector&, bool))0x4A0DE0)(this, entity, origin, target, doGunflash);
}

// 0x4A0FA0
void Fx_c::TriggerTankFire(const CVector& pos, const CVector& dir) {
    ((void(__thiscall*)(Fx_c*, const CVector&, const CVector&))0x4A0FA0)(this, pos, dir);
}

// 0x4A1070
void Fx_c::TriggerWaterSplash(const CVector& pos) {
    CreateFxWithinCameraRange("water_splash_big", pos, 625.0f);
}

// 0x4A10E0
void Fx_c::TriggerBulletSplash(const CVector& pos) {
    CreateFxWithinCameraRange("water_splash", pos, 625.0f);
}

// 0x4A1150
void Fx_c::TriggerFootSplash(const CVector& pos) {
    CreateFxWithinCameraRange("water_splsh_sml", pos, 625.0f);
}

// see RwIm3DTransformFlags
// 0x4A13B0
void RenderBegin(RwRaster* newRaster, RwMatrix* transform, uint32 transformRenderFlags) {
    g_fx.m_pTransformLTM = transform;
    g_fx.m_nVerticesCount = 0;
    g_fx.m_nVerticesCount2 = 0;
    g_fx.m_pRasterToRender = newRaster;
    g_fx.m_nTransformRenderFlags = transformRenderFlags;
    g_fx.m_pVerts = TempBufferVertices.m_3d;

    // And maybe update raster on RW if the same isnt already set...
    RwRaster* currRaster{};
    RwRenderStateGet(rwRENDERSTATETEXTURERASTER, &currRaster);
    if (currRaster != newRaster)
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(newRaster));
}

// Wrapper for original func
// 0x4A1410
void RenderAddTri_(
    float x1, float y1, float z1,
    float x2, float y2, float z2,
    float x3, float y3, float z3,
    float u1, float v1,
    float u2, float v2,
    float u3, float v3,
    int32 r1, int32 g1, int32 b1, int32 a1,
    int32 r2, int32 g2, int32 b2, int32 a2,
    int32 r3, int32 g3, int32 b3, int32 a3
) {
    RenderAddTri(
        { x1, y1, z1 },
        { x2, y2, z2 },
        { x3, y3, z3 },
        { u1, v1 },
        { u2, v2 },
        { u3, v3 },
        CRGBA().FromInt32(r1, g1, b1, a1),
        CRGBA().FromInt32(r2, g2, b2, a2),
        CRGBA().FromInt32(r3, g3, b3, a3)
    );
}

// TODO: I honestly think this should be a class method...
// Although originally it wasnt.
// NOTE: Method signature changed to use CVector + RwTexCoords instead of raw values for convenience.
void RenderAddTri(CVector pos1, CVector pos2, CVector pos3, RwTexCoords coord1, RwTexCoords coord2, RwTexCoords coord3, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3) {
    const auto GetVertex = [](unsigned i) {
        return &g_fx.m_pVerts[i];
    };

    const CVector pos[] = { pos1, pos2, pos3 };
    const RwRGBA color[] = {
        { color1.ToRwRGBA() },
        { color2.ToRwRGBA() },
        { color3.ToRwRGBA() },
    };
    for (unsigned i = 0; i < 3; i++) {
        RxObjSpace3DVertexSetPos(GetVertex(i), &pos[i]);
        RxObjSpace3DVertexSetPreLitColor(GetVertex(i), &color[i]);
    }

    if (g_fx.m_pRasterToRender) {
        const RwTexCoords uvs[] = { coord1, coord2, coord3 };
        for (unsigned i = 0; i < 3; i++) {
            RxObjSpace3DVertexSetU(GetVertex(i), uvs[i].u);
            RxObjSpace3DVertexSetV(GetVertex(i), uvs[i].v);
        }
    }

    g_fx.m_pVerts += 3;
    g_fx.m_nVerticesCount2 += 3;
    g_fx.m_nVerticesCount++;

    if (g_fx.m_nVerticesCount2 >= TOTAL_TEMP_BUFFER_3DVERTICES - 3 || g_fx.m_nVerticesCount >= TOTAL_TEMP_BUFFER_INDICES - 1) {
        RenderEnd(); // Render vertices to free up vertex buffer
    }
}

// 0x4A1600
void RenderEnd() {
    if (!g_fx.m_nVerticesCount)
        return;

    if (RwIm3DTransform(TempBufferVertices.m_3d, 3 * g_fx.m_nVerticesCount, g_fx.m_pTransformLTM, g_fx.m_nTransformRenderFlags)) {
        RwIm3DRenderPrimitive(rwPRIMTYPETRILIST);
        RwIm3DEnd();
    }

    g_fx.m_pVerts = TempBufferVertices.m_3d;
    g_fx.m_nVerticesCount2 = 0;
    g_fx.m_nVerticesCount = 0;
}

// 0x4A1660
void RotateVecIntoVec(RwV3d& vecRes, const RwV3d& vec, const RwV3d& vecAlign) {
    const CVector up = vecAlign;
    const auto ref = CVector{ 3.f, 4.f, 5.f }.Normalized();

    RwV3d right;
    RwV3dCrossProduct(&right, &up, &ref);
    RwV3dNormalize(&right, &right);

    RwV3d at;
    RwV3dCrossProduct(&at, &up, &right);

    auto* m = g_fxMan.FxRwMatrixCreate();
    m->right = right;
    m->up    = up;
    m->at    = at;
    m->pos   = { 0.0f, 0.0f, 0.0f };

    RwMatrixUpdate(m);
    RwV3dTransformVectors(&vecRes, &vec, 1, m);
    g_fxMan.FxRwMatrixDestroy(m);
}

// 0x4A1780
void RotateVecAboutVec(RwV3d& vecRes, const RwV3d& vec, const RwV3d& axis, float angle) {
    const float x = axis.x, y = axis.y, z = axis.z;

    const float s = CMaths::GetSinFast(angle);
    const float c = CMaths::GetCosFast(angle);
    const float t = 1.0f - c;

    vecRes.x = (t*x*x + c)   * vec.x + (t*x*y - s*z) * vec.y + (t*x*z + s*y) * vec.z;
    vecRes.y = (t*x*y + s*z) * vec.x + (t*y*y + c)   * vec.y + (t*y*z - s*x) * vec.z;
    vecRes.z = (t*x*z - s*y) * vec.x + (t*y*z + s*x) * vec.y + (t*z*z + c)   * vec.z;
}
