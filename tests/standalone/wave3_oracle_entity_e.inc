// ORACLE-C-ENTITY rows E: class C (virtual-call / camera / model-info closure) functions of game_sa/Entity. ROW / EC / Tw are defined in wave3_oracle_entity.inc.
// A row whose function reads its own vptr gets the exe's vftable (RH_ScopedVirtualClass VA) in word 0 of the entity before the exe call and the PORT's (the compiler's ??_7 through /alternatename) before
// the port call (VtGuard, cleared on scope exit so the whole-fixture compare ignores it).
#include "Plugins/TwoDEffectPlugin/2dEffect.h"
#include "Escalator.h"
#include "Escalators.h"
#include "Camera.h"
#include "Draw.h"
#include "Game.h"
#define W3E_PORT_VT(C) extern "C" const uint32_t w3evt_##C[]; __pragma(comment(linker, "/alternatename:_w3evt_" #C "=??_7" #C "@@6B@"))
// ModelInfo.cpp is not linked into this target; CBaseModelInfo::Get2dEffect (BaseModelInfo.cpp) reads the 2d-effect store through this accessor (same body as ModelInfo.cpp:337)
CStore<C2dEffect, CModelInfo::NUM_2DFX_INFOS>* CModelInfo::Get2dEffectStore() { return &ms_2dFXInfoStore; }
W3E_PORT_VT(CEntity)
W3E_PORT_VT(CPhysical)
namespace w3e {
static int __fastcall FakeMiStub(void*) { return 0; }
static const void* const g_fakeMiVt[40] = { FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub,
                                           FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub, FakeMiStub };
struct VtGuard { void* p; VtGuard(void* p_, const void* vt) : p(p_) { *reinterpret_cast<const void**>(p) = vt; } ~VtGuard() { *reinterpret_cast<const void**>(p) = nullptr; } };
// plausible camera state: a view matrix (rotation + position), the 4 view-space frustum normals, the 2 world-space ones + plane offsets (and the mirror set), the clip planes
static void CamState(Rng& r) {
    auto& C = TheCamera;
    uint8_t* vm = reinterpret_cast<uint8_t*>(&C.m_mViewMatrix);
    WireMatrixBody(r, vm);
    reinterpret_cast<CMatrix*>(vm)->m_pAttachMatrix = nullptr;
    auto unit2 = [&](float& a, float& b) { const float t = r.f01() * 6.2831853f; a = std::cos(t); b = std::sin(t); };
    for (int i = 0; i < 4; ++i) {
        C.m_avecFrustumNormals[i] = CVector(0.f, 0.f, 0.f);
        if (i < 2) unit2(C.m_avecFrustumNormals[i].x, C.m_avecFrustumNormals[i].y); else unit2(C.m_avecFrustumNormals[i].z, C.m_avecFrustumNormals[i].y);
        if (r.below(12) == 0) C.m_avecFrustumNormals[i].y = Fl(r, 2.f, 2);
        C.m_avecFrustumWorldNormals[i] = CVector(r.f01() * 2.f - 1.f, r.f01() * 2.f - 1.f, r.f01() * 2.f - 1.f);
        C.m_avecFrustumWorldNormals_Mirror[i] = CVector(r.f01() * 2.f - 1.f, r.f01() * 2.f - 1.f, r.f01() * 2.f - 1.f);
        C.m_fFrustumPlaneOffsets[i] = (r.f01() - 0.5f) * 6000.f;
        C.m_fFrustumPlaneOffsets_Mirror[i] = (r.f01() - 0.5f) * 6000.f;
    }
    C.m_bMirrorActive = r.below(3) == 0;
    CDraw::ms_fNearClipZ = r.below(10) ? 0.1f + r.f01() * 2.f : Fl(r, 5.f, 2);
    CDraw::ms_fFarClipZ = r.below(10) ? 100.f + r.f01() * 3000.f : Fl(r, 3000.f, 2);
}
// gives the model info of entity A / B (offset miOff) `n` 2d effects at store slots base..base+n-1, with a type picked from {wanted, light, other}
static void Wire2dFx(Rng& r, size_t miOff, int wantedType, int maxN = 4) {
    auto* mi = reinterpret_cast<CBaseModelInfo*>(P(miOff));
    At<void*>(miOff + 0x1C) = nullptr;                       // m_pRwObject (protected)
    At<const void*>(miOff) = g_fakeMiVt;                     // the exe's Get2dEffect / the port's call GetRwModelType() (slot 9) first
    mi->m_n2dfxCount = (uint8_t)r.below((uint32)maxN + 1);
    const int base = (int)r.below(100 - 4);
    mi->m_n2dEffectIndex = (int16_t)base;
    for (int i = 0; i < maxN; ++i) {
        auto& e = CModelInfo::ms_2dFXInfoStore.GetItemAtIndex(base + i);
        FillGarbage(r, &e, sizeof(C2dEffect));
        e.m_Pos = CVector(Fl(r, 20.f, 14), Fl(r, 20.f, 14), Fl(r, 10.f, 14));
        const uint32_t k = r.below(5);
        e.m_Type = (e2dEffectType)(k < 2 ? wantedType : k == 2 ? 0 : (int)r.below(20));
    }
}
} // namespace w3e

static void TestW3EntityE() {
    using namespace w2; using namespace w3e;
    // RenderWare is not run: the exe's RwV3dTransformPoints (0x7EDD90) / RwV3dTransformVector (0x7EDDC0) dispatch through the RW engine's function table (not initialised here), so they are Patch()ed to the very
    // function the port calls (the fakerw / librw one); both sides then transform with the same code and the entity code around it is what is compared
    oracle::Patch(0x7EDD90, (void*)&RwV3dTransformPoints); oracle::Patch(0x7EDDC0, (void*)&RwV3dTransformVector);
    // ---- HasPreRenderEffects: model flags, the special model-index globals, glass / pickup / traffic light, the 2d-effect light scan
    ROW("CEntity::HasPreRenderEffects", 0x532B70, 3000) {
        static const uint32_t kSpecial[] = { 0x8CD598, 0x8CD59C, 0x8CD5A0, 0x8CD5A4, 0x8CD5AC, 0x8CD638, 0x8CD6E0, 0x8CD71C, 0x8CD700, 0x8CD740, 0x8CD6FC, 0x8CD718, 0x8CD75C, 0x8CD518, 0x8CD51C, 0x8CD520, 0x8CD524 };
        const int pick = (int)r.below(40);                       // which special index (or a fixed one / random) the entity gets
        const uint32_t flagKind = r.below(5);
        const int fx = (int)r.below(3);
        Tw tw([=](Rng& rr) {
            for (uint32_t a : kSpecial) StaticRef<uint16_t>(a) = (uint16_t)rr.below(rr.below(4) ? 20000 : 600);
            int idx = g_idxA;
            if (pick < 17) { idx = (int)StaticRef<uint16_t>(kSpecial[pick]); }
            else if (pick == 17) idx = 0x159; else if (pick == 18) idx = 0x156; else if (pick == 19) idx = 0x158;
            else if (pick == 20) idx = 0x1AE; /* a few other small ids */ else if (pick == 21) idx = (int)rr.below(400);
            reinterpret_cast<void**>(0xA9B0C8)[idx] = P(kMiA);
            At<uint16_t>(kA + offsetof(CEntity, m_nModelIndex)) = (uint16_t)idx;
            auto* mi = reinterpret_cast<CBaseModelInfo*>(P(kMiA));
            uint16_t fl = mi->m_nFlags & ~0x7800;
            if (flagKind == 1) fl |= 0x800; else if (flagKind == 2) fl |= 0x1000; else if (flagKind == 3) fl |= 0x4800; else fl |= (uint16_t)(rr.below(16) << 11) & 0x7800;
            mi->m_nFlags = fl;
            Wire2dFx(rr, kMiA, 0, 3);
            if (fx == 0) mi->m_n2dfxCount = 0;
            At<uint16_t>(kA + offsetof(CEntity, m_nModelIndex)) = (uint16_t)idx;
            P(kA)[0x140] = (uint8_t)rr.u32();
        });
        EC(VtGuard g(EntA(), w3evt_CEntity); return B8(EntA()->CEntity::HasPreRenderEffects());, VtGuard g(EntA(), (const void*)0x863928); return ExeT<uint8_t>(0x532B70, EntA()););
    } ENDROW;
    // ---- camera visibility
    ROW("CEntity::GetIsOnScreen", 0x534540, 1500) {
        Tw tw([](Rng& rr) { CamState(rr); EnsureMatrix(rr, EntA(), 0); });
        EC(return B8(EntA()->CEntity::GetIsOnScreen());, return ExeT<uint8_t>(0x534540, EntA()););
    } ENDROW;
    ROW("CEntity::IsVisible", 0x536BC0, 1500) {
        Tw tw([](Rng& rr) { CamState(rr); EnsureMatrix(rr, EntA(), 0); At<void*>(kA + 0x18) = rr.below(4) ? (void*)P(kScratch) : nullptr; });
        EC(return B8(EntA()->CEntity::IsVisible());, return ExeT<uint8_t>(0x536BC0, EntA()););
    } ENDROW;
    ROW("CEntity::GetIsBoundingBoxOnScreen", 0x5345D0, 1500) {
        Tw tw([](Rng& rr) { CamState(rr); CGame::m_pWorkingMatrix1 = reinterpret_cast<RwMatrix*>(P(kIn)); CGame::m_pWorkingMatrix2 = reinterpret_cast<RwMatrix*>(P(kIn + 0x40)); FillGarbage(rr, P(kIn), 0x80); });
        EC(return B8(EntA()->CEntity::GetIsBoundingBoxOnScreen());, return ExeT<uint8_t>(0x5345D0, EntA()););
    } ENDROW;
    // ---- 2d effects
    ROW("CEntity::FindTriggerPointCoors", 0x533380, 1500) {
        const int index = (int)r.below(3);
        Tw tw([](Rng& rr) { EnsureMatrix(rr, EntA(), 0); Wire2dFx(rr, kMiA, 8); FillGarbage(rr, P(kOut), 16); });
        EC(return U(EntA()->CEntity::FindTriggerPointCoors(&At<CVector>(kOut), index));, return (uint64_t)ExeT<uint32_t>(0x533380, EntA(), P(kOut), index););
    } ENDROW;
    ROW("CEntity::RenderEffects (no road sign)", 0x5342B0, 600) {
        Tw tw([](Rng& rr) { Wire2dFx(rr, kMiA, 0, 3); EntA()->m_bHasRoadsignText = rr.below(3) != 0;
                            for (int i = 0; i < 4; ++i) { auto& e = CModelInfo::ms_2dFXInfoStore.GetItemAtIndex(i); (void)e; } });
        EC(return (EntA()->CEntity::RenderEffects(), uint64_t(0));, return (ExeT<void>(0x5342B0, EntA()), uint64_t(0)););
    } ENDROW;
    // ---- escalators
    ROW("CEntity::RemoveEscalatorsForEntity", 0x717900, 600) {
        Tw tw([](Rng& rr) {
            for (int i = 0; i < 32; ++i) {
                uint8_t* e = reinterpret_cast<uint8_t*>(0xC6E9A8 + 0x150 * (size_t)i);
                e[0x78] = (uint8_t)(rr.below(2));
                e[0x79] = 0;                                              // no step objects created
                *reinterpret_cast<void**>(e + 0x78 + 0x2C) = rr.below(3) == 0 ? (void*)EntA() : (rr.below(2) ? (void*)EntB() : nullptr);
            }
        });
        EC(VtGuard g(EntA(), w3evt_CEntity); EntA()->CEntity::RemoveEscalatorsForEntity(); return 0;, VtGuard g(EntA(), (const void*)0x863928); ExeT<void>(0x717900, EntA()); return 0;);
    } ENDROW;
}
