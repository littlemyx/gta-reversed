RpMaterial* W3E_SetCompAlphaCB(RpMaterial* material, void* data);   // wave3_entity_tu.cpp
// ORACLE-W3-ENTITY rows A: CEntity / CPhysical / CAnimatedBuilding functions on the entity fixture (see wave3_oracle_entity.inc). ROW / EC / Tw are defined there.
static void TestW3EntityA() {
    using namespace w2; using namespace w3e;
    // ---- the empty / constant virtuals of CEntity (called qualified), CAnimatedBuilding stubs
    ROW("CEntity::SetIsStatic", 0x403E20, 200) { const int v = (int)r.below(2); EC(EntA()->CEntity::SetIsStatic(v != 0); return 0;, ExeT<void>(0x403E20, EntA(), v); return 0;); } ENDROW;
    ROW("CEntity::ProcessControl", 0x403E40, 40) { EC(EntA()->CEntity::ProcessControl(); return 0;, ExeT<void>(0x403E40, EntA()); return 0;); } ENDROW;
    ROW("CEntity::ProcessCollision", 0x403E50, 40) { EC(EntA()->CEntity::ProcessCollision(); return 0;, ExeT<void>(0x403E50, EntA()); return 0;); } ENDROW;
    ROW("CEntity::ProcessShift", 0x403E60, 40) { EC(EntA()->CEntity::ProcessShift(); return 0;, ExeT<void>(0x403E60, EntA()); return 0;); } ENDROW;
    ROW("CEntity::TestCollision", 0x403E70, 100) { const int v = (int)r.below(2); EC(return B8(EntA()->CEntity::TestCollision(v != 0));, return ExeT<uint8_t>(0x403E70, EntA(), v);); } ENDROW;
    ROW("CEntity::Teleport", 0x403E80, 100) { const CVector p(Fl(r, 3000.f), Fl(r, 3000.f), Fl(r, 100.f)); const int v = (int)r.below(2); EC(EntA()->CEntity::Teleport(p, v != 0); return 0;, ExeT<void>(0x403E80, EntA(), p, v); return 0;); } ENDROW;
    ROW("CEntity::SpecialEntityPreCollisionStuff", 0x403E90, 100) {
        const int v = (int)r.below(2);
        EC(EntA()->CEntity::SpecialEntityPreCollisionStuff(PhyB(), v != 0, At<bool>(kOut), At<bool>(kOut + 1), At<bool>(kOut + 2), At<bool>(kOut + 3)); return 0;,
              ExeT<void>(0x403E90, EntA(), PhyB(), v, P(kOut), P(kOut + 1), P(kOut + 2), P(kOut + 3)); return 0;);
    } ENDROW;
    ROW("CEntity::SpecialEntityCalcCollisionSteps", 0x403EA0, 100) {
        EC(return EntA()->CEntity::SpecialEntityCalcCollisionSteps(At<bool>(kOut), At<bool>(kOut + 1));, return ExeT<uint8_t>(0x403EA0, EntA(), P(kOut), P(kOut + 1)););
    } ENDROW;
    ROW("CEntity::FlagToDestroyWhenNextProcessed", 0x403EB0, 40) { EC(EntA()->CEntity::FlagToDestroyWhenNextProcessed(); return 0;, ExeT<void>(0x403EB0, EntA()); return 0;); } ENDROW;
    ROW("CAnimatedBuilding::ProcessControl", 0x537A30, 40) { EC(reinterpret_cast<CAnimatedBuilding*>(EntA())->CAnimatedBuilding::ProcessControl(); return 0;, ExeT<void>(0x537A30, EntA()); return 0;); } ENDROW;
    ROW("CAnimatedBuilding::ProcessCollision", 0x537A40, 100) { EC(reinterpret_cast<CAnimatedBuilding*>(EntA())->CAnimatedBuilding::ProcessCollision(); return 0;, ExeT<void>(0x537A40, EntA()); return 0;); } ENDROW;
    // ---- flag / type accessors
    ROW("CEntity::GetIsStatic", 0x4633E0, 300) { EC(return B8(EntA()->GetIsStatic());, return ExeT<uint8_t>(0x4633E0, EntA());); } ENDROW;
    ROW("CEntity::GetIsTypePhysical", 0x4DA030, 300) { EC(return B8(EntA()->GetIsTypePhysical());, return ExeT<uint8_t>(0x4DA030, EntA());); } ENDROW;
    ROW("CEntity::TreatAsPlayerForCollisions", 0x541F70, 300) {
        Tw tw([](Rng& rr) { if (rr.below(3) == 0) EntA()->SetStatus((eEntityStatus)0); });
        EC(return B8(EntA()->TreatAsPlayerForCollisions());, return ExeT<uint8_t>(0x541F70, EntA()););
    } ENDROW;
    ROW("::SetCompAlphaCB (entity)", 0x533280, 200) {
        const uintptr_t data = r.below(3) ? r.below(256) : r.u32();
        EC(return U(W3E_SetCompAlphaCB(reinterpret_cast<RpMaterial*>(P(kMat)), reinterpret_cast<void*>(data)));, return (uint64_t)ExeC<uint32_t>(0x533280, P(kMat), (void*)data););
    } ENDROW;
    // ---- transforms and bounds
    ROW("CEntity::TransformFromObjectSpace (out)", 0x533560, 600) {
        Tw tw([](Rng& rr) { At<CVector>(kIn) = CVector(Fl(rr, 20.f, 14), Fl(rr, 20.f, 14), Fl(rr, 10.f, 14)); });
        EC(return U(EntA()->CEntity::TransformFromObjectSpace(At<CVector>(kOut), At<CVector>(kIn)));, return (uint64_t)ExeT<uint32_t>(0x533560, EntA(), P(kOut), P(kIn)););
    } ENDROW;
    ROW("CEntity::GetBoundRect", 0x534120, 600) {
        EC(At<CRect>(kOut) = EntA()->CEntity::GetBoundRect(); return U(P(kOut));, return (uint64_t)ExeT<uint32_t>(0x534120, EntA(), P(kOut)););
    } ENDROW;
    ROW("CEntity::GetBoundCentre (vec)", 0x534250, 600) {
        EC(At<CVector>(kOut) = EntA()->CEntity::GetBoundCentre(); return U(P(kOut));, return (uint64_t)ExeT<uint32_t>(0x534250, EntA(), P(kOut)););
    } ENDROW;
    ROW("CEntity::GetBoundCentre (ref)", 0x534290, 600) {
        EC(EntA()->CEntity::GetBoundCentre(At<CVector>(kOut)); return 0;, ExeT<void>(0x534290, EntA(), P(kOut)); return 0;);
    } ENDROW;
    ROW("CEntity::GetIsTouching (entity)", 0x5343F0, 600) {
        const bool self = r.below(8) == 0;
        Tw tw([](Rng& rr) { if (rr.below(2)) EntB()->GetPosition() = EntA()->GetPosition() + CVector(Fl(rr, 12.f, 14), Fl(rr, 12.f, 14), Fl(rr, 12.f, 14)); });   // B near A half of the time
        EC(return B8(EntA()->CEntity::GetIsTouching(self ? EntA() : EntB()));, return ExeT<uint8_t>(0x5343F0, EntA(), self ? EntA() : EntB()););
    } ENDROW;
    ROW("CEntity::GetIsTouching (centre, radius)", 0x5344B0, 600) {
        const float rad = r.below(10) == 0 ? Fl(r, 50.f, 1) : r.f01() * 60.f;
        Tw tw([](Rng& rr) { At<CVector>(kIn) = rr.below(2) ? EntA()->GetPosition() + CVector(Fl(rr, 30.f, 14), Fl(rr, 30.f, 14), Fl(rr, 30.f, 14)) : CVector(Fl(rr, 3000.f, 14), Fl(rr, 3000.f, 14), Fl(rr, 100.f, 14)); });
        EC(return B8(EntA()->CEntity::GetIsTouching(At<CVector>(kIn), rad));, return ExeT<uint8_t>(0x5344B0, EntA(), P(kIn), rad););
    } ENDROW;
    ROW("CEntity::GetDistanceFromCentreOfMassToBaseOfModel", 0x536BE0, 400) {
        Tw tw([](Rng& rr) { FillGarbage(rr, &CVehicle::m_aSpecialColModel, sizeof CVehicle::m_aSpecialColModel); });
        EC(return RF(EntA()->CEntity::GetDistanceFromCentreOfMassToBaseOfModel());, return RF(ExeT<float>(0x536BE0, EntA())););
    } ENDROW;
    ROW("CEntity::CalculateBBProjection", 0x535340, 800) {
        struct Mk { Mk() { g_flMask = S_NOHUGE | S_INF; } ~Mk() { g_flMask = S_ALL; } } mk;   // 1e30-range products overflow float but not the exe's extended registers (not a game value)
        Tw tw([](Rng&) { ClampHuge(P(kNodes), 0x40); ClampHuge(P(kColA), 0x28); });
        EC(EntA()->CEntity::CalculateBBProjection(&At<CVector>(kOut), &At<CVector>(kOut + 12), &At<CVector>(kOut + 24), &At<CVector>(kOut + 36)); return 0;,
              ExeT<void>(0x535340, EntA(), P(kOut), P(kOut + 12), P(kOut + 24), P(kOut + 36)); return 0;);
    } ENDROW;
    // ---- wind / building set-ups
    ROW("CEntity::DoesNotCollideWithFlyers", 0x532D40, 300) { EC(return B8(EntA()->CEntity::DoesNotCollideWithFlyers());, return ExeT<uint8_t>(0x532D40, EntA());); } ENDROW;
    ROW("CEntity::BuildWindSockMatrix", 0x532DB0, 500) { EC(EntA()->CEntity::BuildWindSockMatrix(); return 0;, ExeT<void>(0x532DB0, EntA()); return 0;); } ENDROW;
    ROW("CEntity::ModifyMatrixForBannerInWind", 0x535040, 500) { EC(EntA()->CEntity::ModifyMatrixForBannerInWind(); return 0;, ExeT<void>(0x535040, EntA()); return 0;); } ENDROW;
    ROW("CEntity::SetupBigBuilding", 0x533150, 200) { EC(EntA()->CEntity::SetupBigBuilding(); return 0;, ExeT<void>(0x533150, EntA()); return 0;); } ENDROW;
    ROW("CEntity::PreRenderForGlassWindow", 0x533240, 300) {
        Tw tw([](Rng& rr) { CGlass::NumGlassEntities = rr.below(8) == 0 ? rr.below(48) : rr.below(33); });
        EC(EntA()->CEntity::PreRenderForGlassWindow(); return 0;, ExeT<void>(0x533240, EntA()); return 0;);
    } ENDROW;
    // ---- CPhysical
    ROW("CPhysical::GetHasCollidedWithAnyObject", 0x543580, 600) {
        Tw tw([](Rng& rr) { EntB()->SetType((eEntityType)(rr.below(6))); EntC()->SetType((eEntityType)(rr.below(6))); if (rr.below(8) == 0) At<void*>(kA + offsetof(CPhysical, m_apCollidedEntities)) = nullptr; });
        EC(return B8(PhyA()->CPhysical::GetHasCollidedWithAnyObject());, return ExeT<uint8_t>(0x543580, PhyA()););
    } ENDROW;
    ROW("CPhysical::RemoveRefsToEntity", 0x544280, 600) {
        const int which = (int)r.below(3);
        EC(PhyA()->CPhysical::RemoveRefsToEntity(which == 0 ? EntB() : which == 1 ? EntC() : nullptr); return 0;, ExeT<void>(0x544280, PhyA(), which == 0 ? EntB() : which == 1 ? EntC() : nullptr); return 0;);
    } ENDROW;
    ROW("CPhysical::CanPhysicalBeDamaged", 0x5448B0, 1500) {
        const int w = r.below(10) == 0 ? (int)r.below(256) : (int)r.below(0x40); const bool wantOut = r.below(2) != 0;
        EC(return B8(PhyA()->CPhysical::CanPhysicalBeDamaged((eWeaponType)w, wantOut ? &At<bool>(kOut) : nullptr));, return ExeT<uint8_t>(0x5448B0, PhyA(), w, wantOut ? P(kOut) : nullptr););
    } ENDROW;
    ROW("CPhysical::GetBoundRect", 0x5449B0, 600) {
        EC(At<CRect>(kOut) = PhyA()->CPhysical::GetBoundRect(); return U(P(kOut));, return (uint64_t)ExeT<uint32_t>(0x5449B0, PhyA(), P(kOut)););
    } ENDROW;
    ROW("CPhysical::AddCollisionRecord", 0x543490, 800) {
        const int which = (int)r.below(3);
        Tw tw([](Rng& rr) { if (rr.below(2)) At<void*>(kA + offsetof(CPhysical, m_apCollidedEntities) + 4 * (size_t)rr.below(6)) = EntB(); At<uint16_t>(kA + offsetof(CVehicle, m_nAlarmState)) = rr.below(3) == 0 ? 0xFFFF : (uint16_t)rr.u32(); At<uint16_t>(kB + offsetof(CVehicle, m_nAlarmState)) = rr.below(3) == 0 ? 0xFFFF : (uint16_t)rr.u32(); });
        EC(PhyA()->CPhysical::AddCollisionRecord(which == 0 ? EntB() : which == 1 ? EntC() : EntA()); return 0;, ExeT<void>(0x543490, PhyA(), which == 0 ? EntB() : which == 1 ? EntC() : EntA()); return 0;);
    } ENDROW;
}
