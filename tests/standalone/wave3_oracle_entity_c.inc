// ORACLE-W3-ENTITY rows C: CPhysical state rows (damage record, lighting, friction) and the pool-pointer validity functions (IsBuildingPointerValid / IsEntityPointerValid).
struct W3ePdTag { using type = CPlayerPedData* CPed::*; };
template struct W2Rob<W3ePdTag, &CPed::m_pPlayerData>;
CPlayerPedData* CPed::* W2Get(W3ePdTag);
namespace w3e {
constexpr size_t kPool = 0x3100 /*2 x 0x40*/, kPoolSt = 0x3180 /*2 x 0x10*/, kPoolStore = 0x3200 /*2 x 16 x 0x38*/;
static CEntity* PoolEnt(int pool, int i) { return reinterpret_cast<CEntity*>(P(kPoolStore + (size_t)pool * 0x380 + (size_t)i * 0x38)); }
// fake building (pool 0) and dummy (pool 1) pools in the fixture, installed at the exe's pool globals (0xB74498 / 0xB744A0)
static void BuildPools(Rng& r) {
    for (int k = 0; k < 2; ++k) {
        uint8_t* st = P(kPoolSt + (size_t)k * 0x10);
        for (int i = 0; i < 16; ++i) st[i] = r.below(4) ? (uint8_t)r.below(0x7F) : 0x80;   // bit 7 = free slot
        if (k == 0) { new (P(kPool)) CBuildingPool(16, P(kPoolStore), st); StaticRef<void*>(0xB74498) = P(kPool); }
        else        { new (P(kPool + 0x40)) CDummyPool(16, P(kPoolStore + 0x380), st); StaticRef<void*>(0xB744A0) = P(kPool + 0x40); }
        for (int i = 0; i < 16; ++i) { uint8_t* e = reinterpret_cast<uint8_t*>(PoolEnt(k, i)); e[0x36] = (uint8_t)((e[0x36] & 0xF8) | (r.below(6) ? (k == 0 ? 1 : 5) : r.below(8))); }
    }
}
static void LightingState(Rng& r) {
    FillGarbage(r, &CTimeCycle::m_CurrentColours, sizeof CTimeCycle::m_CurrentColours);
    ClampHuge(reinterpret_cast<uint8_t*>(&CTimeCycle::m_CurrentColours), sizeof CTimeCycle::m_CurrentColours);   // products of 1e30-range floats overflow float but not the exe's extended registers
    ClampHuge(P(kA + offsetof(CPhysical, m_fContactSurfaceBrightness)), 8);
    gfLaRiotsLightMult = r.below(4) ? r.f01() * 1.5f : GenF(r, 2.f, S_NOHUGE | S_INF);
    StaticRef<int32_t>(0xB72914) = (int32_t)r.below(6);   // CGame::currArea
    EntA()->SetAreaCode((eAreaCodes)(r.below(2) ? 0 : r.below(256)));
    // a ped's player data (interior-lighting flag) lives in the scratch block
    FillGarbage(r, P(kScratch), 0x100);
    reinterpret_cast<CPed*>(P(kA))->*W2Get(W3ePdTag{}) = r.below(3) ? reinterpret_cast<CPlayerPedData*>(P(kScratch)) : nullptr;
    P(kScratch)[offsetof(CPlayerPedData, m_bForceInteriorLighting)] = (uint8_t)r.below(2);
}
} // namespace w3e

static void TestW3EntityC() {
    using namespace w2; using namespace w3e;
    ROW("CPhysical::SetDamagedPieceRecord", 0x5428C0, 1000) {
        const float dmg = r.below(8) ? r.f01() * 40.f : Fl(r, 40.f, 2); const float mult = Fl(r, 5.f, 14); const bool useC = r.below(3) == 0;
        Tw tw([&](Rng& rr) {
            auto* cp = reinterpret_cast<CColPoint*>(P(kCp));
            cp->m_vecPoint = CVector(Fl(rr, 3000.f, 14), Fl(rr, 3000.f, 14), Fl(rr, 100.f, 14)); cp->m_vecNormal = CVector(Fl(rr, 1.f, 14), Fl(rr, 1.f, 14), Fl(rr, 1.f, 14));
            cp->m_nSurfaceTypeA = (eSurfaceType)(rr.below(3) ? 0 : rr.below(180)); cp->m_nSurfaceTypeB = (eSurfaceType)(rr.below(3) ? 0 : rr.below(180));
            if (rr.below(3) == 0) cp->m_nSurfaceTypeB = SURFACE_CAR_MOVINGCOMPONENT; if (rr.below(3) == 0) cp->m_nSurfaceTypeA = SURFACE_CAR_MOVINGCOMPONENT;
            if (rr.below(3) == 0) PhyA()->m_fDamageIntensity = dmg - 1.f; else if (rr.below(3) == 0) PhyA()->m_fDamageIntensity = dmg + 1.f;
            if (rr.below(2)) { EntC()->RegisterReference(reinterpret_cast<CEntity**>(&PhyA()->m_pDamageEntity)); PhyA()->m_pDamageEntity = EntC(); }
            if (rr.below(3) == 0) StaticRef<int32_t>(0x8CD778) = useC ? g_idxB : (int32_t)rr.below(20000);   // ModelIndices::MI_POOL_CUE_BALL
            if (rr.below(3) == 0) EntB()->SetTypeObject();
            At<uint8_t>(kA + offsetof(CObject, m_nLastWeaponDamage)) = (uint8_t)(rr.below(2) ? 255 : rr.below(256));
        });
        CEntity* e = useC ? EntC() : EntB();
        EC(PhyA()->CPhysical::SetDamagedPieceRecord(dmg, e, *reinterpret_cast<CColPoint*>(P(kCp)), mult); return 0;, ExeT<void>(0x5428C0, PhyA(), dmg, e, P(kCp), mult); return 0;);
    } ENDROW;
    ROW("CPhysical::GetLightingFromCol", 0x5447B0, 800) {
        const int interior = (int)r.below(2);
        Tw tw([](Rng& rr) { LightingState(rr); });
        EC(return RF(PhyA()->CPhysical::GetLightingFromCol(interior != 0));, return RF(ExeT<float>(0x5447B0, PhyA(), interior)););
    } ENDROW;
    ROW("CPhysical::GetLightingTotal", 0x544850, 1000) {
        Tw tw([](Rng& rr) { LightingState(rr); });
        EC(return RF(PhyA()->CPhysical::GetLightingTotal());, return RF(ExeT<float>(0x544850, PhyA())););
    } ENDROW;
    ROW("CPhysical::ApplyFriction (other)", 0x545980, 1500) {
        struct Mk { Mk() { g_flMask = S_NAN | S_ZERO; } ~Mk() { g_flMask = S_ALL; } } mk;   // specials: NaN / 0 only (inf inputs give inf - inf chains whose compare outcome differs between float and extended evaluation; not game values)
        const float fr = r.below(8) ? r.f01() * 0.5f : Fl(r, 2.f, 2);
        Tw tw([](Rng& rr) {
            EnsureMatrix(rr, EntA(), 0); EnsureMatrix(rr, EntB(), 1);   // the exe reads the other entity's matrix without a null check
            for (size_t off : { kA, kB }) {
                At<float>(off + offsetof(CPhysical, m_fMass)) = rr.below(12) ? 1.f + rr.f01() * 3000.f : Fl(rr, 100.f, 2);
                At<float>(off + offsetof(CPhysical, m_fTurnMass)) = rr.below(12) ? 1.f + rr.f01() * 8000.f : Fl(rr, 100.f, 2);
                At<CVector>(off + offsetof(CPhysical, m_vecMoveSpeed)) = CVector(Fl(rr, 1.f, 14), Fl(rr, 1.f, 14), Fl(rr, 1.f, 14));
                At<CVector>(off + offsetof(CPhysical, m_vecTurnSpeed)) = CVector(Fl(rr, 0.2f, 14), Fl(rr, 0.2f, 14), Fl(rr, 0.2f, 14));
                At<CVector>(off + offsetof(CPhysical, m_vecCentreOfMass)) = CVector(Fl(rr, 0.5f, 14), Fl(rr, 0.5f, 14), Fl(rr, 0.5f, 14));
                { auto& fl = At<uint32_t>(off + offsetof(CPhysical, m_nPhysicalFlags)); fl = (fl & ~0x34u) | (rr.below(2) ? 0x10u : 0u) | (rr.below(2) ? 0x4u : 0u) | (rr.below(8) == 0 ? 0x20u : 0u); }   // bDisableTurnForce (0x10), bDisableCollisionForce (0x4), bDisableMoveForce (0x20, rare)
            }
            ClampHuge(P(kNodes), 0x40, 1e6f, 1e-3f); ClampHuge(P(kNodes + 0x60), 0x40, 1e6f, 1e-3f);   // matrices of A / B (garbage ones can hold 1e30-range elements)
            for (size_t off : { kA, kB }) ClampHuge(P(off + offsetof(CPhysical, m_vecMoveSpeed)), 0x6C, 1e6f, 1e-3f);   // speeds / forces / masses: tiny values make the downstream float helpers underflow where the exe's extended registers do not
            auto* cp = reinterpret_cast<CColPoint*>(P(kCp));
            const CVector n(Fl(rr, 1.f, 14), Fl(rr, 1.f, 14), Fl(rr, 1.f, 14)); cp->m_vecNormal = n * (1.f / std::max(n.Magnitude(), 1e-3f));
            cp->m_vecPoint = EntA()->GetPosition() + CVector(Fl(rr, 3.f, 14), Fl(rr, 3.f, 14), Fl(rr, 3.f, 14));
        });
        const bool ok = ECase(r, d, [&]() -> uint64_t { return B8(PhyA()->CPhysical::ApplyFriction(PhyB(), fr, *reinterpret_cast<CColPoint*>(P(kCp)))); }, [&]() -> uint64_t { return ExeT<uint8_t>(0x545980, PhyA(), PhyB(), fr, P(kCp)); });
        if (!ok) { char t[400]; std::snprintf(t, sizeof t, " br=%d fr=%g A: flags %08X type %d mass %g turn %g ms %g %g %g | B: flags %08X type %d mass %g turn %g ms %g %g %g", ((PhyA()->m_nPhysicalFlags >> 4) & 1) | (((PhyB()->m_nPhysicalFlags >> 4) & 1) << 1), fr, PhyA()->m_nPhysicalFlags, (int)PhyA()->GetType(), PhyA()->m_fMass, PhyA()->m_fTurnMass, PhyA()->m_vecMoveSpeed.x, PhyA()->m_vecMoveSpeed.y, PhyA()->m_vecMoveSpeed.z, PhyB()->m_nPhysicalFlags, (int)PhyB()->GetType(), PhyB()->m_fMass, PhyB()->m_fTurnMass, PhyB()->m_vecMoveSpeed.x, PhyB()->m_vecMoveSpeed.y, PhyB()->m_vecMoveSpeed.z); d += t; }
        return ok;
    } ENDROW;
    // ---- pool pointer validity
    ROW("::IsBuildingPointerValid", 0x4040E0, 600) {
        const int kind = (int)r.below(6); const int slot = (int)r.below(16); const int off = r.below(5) == 0 ? (int)r.below(0x38) : 0;
        Tw tw([](Rng& rr) { BuildPools(rr); });
        CBuilding* arg = kind == 0 ? nullptr : kind == 1 ? reinterpret_cast<CBuilding*>(P(kPoolStore + 0x380)) /*dummy storage*/ : kind == 2 ? reinterpret_cast<CBuilding*>(P(kPoolStore - 0x38)) : reinterpret_cast<CBuilding*>(P(kPoolStore + (size_t)slot * 0x38 + (size_t)off));
        EC(return B8(IsBuildingPointerValid(arg));, return ExeC<uint8_t>(0x4040E0, arg););
    } ENDROW;
    ROW("::IsDummyPointerValid", 0x532730, 800) {
        const int kind = (int)r.below(7); const int slot = (int)r.below(18); const int off = r.below(4) == 0 ? (int)r.below(0x38) : 0;
        Tw tw([](Rng& rr) { BuildPools(rr); });
        CDummy* arg = kind == 0 ? nullptr : kind == 1 ? reinterpret_cast<CDummy*>(P(kPoolStore + 0x380 - 0x38 + (size_t)r.below(0x38))) : kind == 2 ? reinterpret_cast<CDummy*>(P(kPoolStore + 0x380 + 16 * 0x38 + (size_t)(r.below(3) * 0x38))) : reinterpret_cast<CDummy*>(P(kPoolStore + 0x380 + (size_t)slot * 0x38 + (size_t)off));
        EC(return B8(IsDummyPointerValid(arg));, return ExeC<uint8_t>(0x532730, arg););
    } ENDROW;
    ROW("::IsEntityPointerValid", 0x533310, 2000) {
        rf::Init();
        const int kind = (int)r.below(9); const int slot = (int)r.below(16);
        CEntity* arg = nullptr;
        Tw tw([&](Rng& rr) {
            BuildPools(rr); rf::Enter(); rf::Reset(rr, 0);
            switch (kind) {
            case 1: arg = rf::PedE(slot % rf::kP); break;
            case 2: arg = rf::VehE(slot % rf::kV); break;
            case 3: arg = rf::ObjE(slot % rf::kO); break;
            case 4: arg = PoolEnt(0, slot); break;
            case 5: arg = PoolEnt(1, slot); break;
            case 6: arg = rf::LooseE(slot % rf::kX); break;
            case 7: arg = EntA(); break;
            default: arg = nullptr; break;
            }
            if (!arg) return;
            if (kind == 7) arg->SetType((eEntityType)rr.below(8));
            if (kind == 2 || kind == 7) { *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(arg) + offsetof(CVehicle, m_nVehicleType)) = rr.below(4) ? (int32_t)rr.below(12) : (int32_t)VEHICLE_TYPE_FPLANE; }
            if (rr.below(8) == 0) arg->SetType((eEntityType)rr.below(8));
            if (rr.below(2)) *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(arg) + offsetof(CPhysical, m_pCollisionList)) = reinterpret_cast<void*>(0x1000);
            if (rr.below(2)) *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(arg) + 0x1C) = rr.u32();   // flags (IsBIGBuilding ...)
            if (kind == 1) { auto* ped = arg->AsPed(); ped->bInVehicle = rr.below(2); ped->m_pVehicle = rr.below(3) == 0 ? nullptr : static_cast<CVehicle*>(rf::VehE(rr.below(rf::kV))); }
        });
        const bool ok = ECase(r, d, [&]() -> uint64_t { return B8(IsEntityPointerValid(arg)); }, [&]() -> uint64_t { return ExeC<uint8_t>(0x533310, arg); });
        if (!ok) { char t[120]; std::snprintf(t, sizeof t, " kind %d slot %d arg %p type %d", kind, slot, (void*)arg, arg ? (int)arg->GetType() : -1); d += t; }
        rf::Leave();
        return ok;
    } ENDROW;
}
