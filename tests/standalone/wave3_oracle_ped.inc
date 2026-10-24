// ORACLE-W3-PED: exe-oracle rows for game_sa/Entity/Ped (classes A and B of .notes/ORACLE_COVERAGE.md); the fixture (wired ped, W3PedCase) lives in wave3_oracle_pedfx.inc.
#include "wave3_oracle_pedfx.inc"

#if __has_include("wave3_oracle_gen_ped.inc")
#include "wave3_oracle_gen_ped.inc"      // GENERATED (tools/gen_w3_ped.py): ped member / static functions on a wired ped
#define W3P_HAVE_GEN 1
#endif

// ---- hand rows ------------------------------------------------------------------------------------------------------------------------------
static void TestW3PedHand() {
    using namespace w2; using namespace w3p;
    // CPed::operator new(size, poolRef) 0x5E4730: slot = poolRef >> 8, the low byte becomes the slot's flag byte; a ref outside the pool writes at an arbitrary address (the generated row used a random int)
    Run("CPed::operator new-poolIndexed 0x5E4730", [&](Rng& r, std::string& d) {
        const int32 ref = (int32)((r.below((uint32)rf::kP) << 8) | r.below(128));
        return W3PedCase(r, d, 0, {}, [&](void*) -> uint64_t { return (uint64_t)(uintptr_t)CPed::operator new(sizeof(CCopPed), ref); },
            [&](void*) -> uint64_t { return (uint64_t)(uintptr_t)oracle::Fn<void* __cdecl(unsigned, int32)>(0x5E4730)(sizeof(CCopPed), ref); });
    }, 60);
    // CPed::operator delete(void*) 0x5E4760: slot = (ptr - base) / sizeof(CPed pool object), flag byte |= 0x80, first-free index lowered; the argument is a ped of the fake pool (a random pointer is not a valid input)
    Run("CPed::operator delete 0x5E4760", [&](Rng& r, std::string& d) {
        void* ped = rf::PedE((int)r.below((uint32)rf::kP));
        return W3PedCase(r, d, 0, {}, [&](void*) -> uint64_t { CPed::operator delete(ped); return 0; },
            [&](void*) -> uint64_t { oracle::Fn<void __cdecl(void*)>(0x5E4760)(ped); return 0; });
    }, 60);
    // CPlayerPed::CanIKReachThisTarget 0x609F80: weapon = one of the ped's 13 weapon slots (the generated row passed null), target around the ped (and exactly level with it / at the 2D distance), NaN targets
    Run("CPlayerPed::CanIKReachThisTarget 0x609F80", [&](Rng& r, std::string& d) {
        const uint32 slot = r.below(13), mode = r.below(10);
        float dx = (r.f01() - 0.5f) * 40.f, dy = (r.f01() - 0.5f) * 40.f, dz = (r.f01() - 0.5f) * 30.f;
        if (mode == 0) { dx = std::numeric_limits<float>::quiet_NaN(); }
        else if (mode == 1) { dz = std::numeric_limits<float>::quiet_NaN(); }
        else if (mode == 2) { dx = 3.f; dy = 4.f; dz = -5.f; }   // |(dx,dy)| = 5 = pos.z - posn.z: the compare is exactly on the edge (when the ped height has no rounding)
        else if (mode == 3) { dx = dy = 0.f; dz = 0.f; }
        const bool arg2 = r.below(2);
        return W3PedCase(r, d, sizeof(CPlayerPed), {}, [&](void* p) -> uint64_t {
                auto* ped = static_cast<CPlayerPed*>(p); const CVector pos = ped->GetPosition();
                return (uint64_t)(uint8_t)(ped->CPlayerPed::CanIKReachThisTarget(CVector(pos.x + dx, pos.y + dy, pos.z - dz), &ped->m_aWeapons[slot], arg2) ? 1 : 0); },
            [&](void* p) -> uint64_t {
                auto* ped = static_cast<CPlayerPed*>(p); const CVector pos = ped->GetPosition();
                return (uint64_t)(uint8_t)(oracle::Fn<int __fastcall(void*, int, float, float, float, void*, int)>(0x609F80)(p, 0, pos.x + dx, pos.y + dy, pos.z - dz, &ped->m_aWeapons[slot], X(arg2)) & 0xFF); });
    }, 3000);
    // ORACLE-C-EVENTS-2: CPed::DisablePedSpeech / ...ForScriptSpeech trampoline to CAEPedSpeechAudioEntity (word parameter); the generated rows are skipped (w3_ped_skip.tsv)
    Run("CPed::DisablePedSpeech 0x5EFF60", [&](Rng& r, std::string& d) {
        static const int16 kA0[] = { 0, 1, 0x100, -1, 0x7FFF, 0x0100 | 1 };   // the exe compares the whole word (0x4E56DA cmp word ptr [esp+4], 0): the parameter is an int16, not a bool (the bool-typed port dropped the high byte)
        const int16 a0 = kA0[r.below(6)];
        return W3PedCase(r, d, sizeof(CPed), { { W2Off_CPed_m_roadRageWith() + 0, "p"[0] }, { W2Off_CPed_m_pIntelligence() + 0, "n"[0] }, { W2Off_CPed_m_pPlayerData() + 0, "n"[0] }, { W2Off_CPed_m_nCreatedBy() + 0, "u"[0], W2Off_CPed_m_nCreatedBy_sz() }, { W2Off_CPed_m_pWeaponObject() + 0, "n"[0] }, { W2Off_CPed_m_pGunflashObject() + 0, "n"[0] }, { W2Off_CPed_m_pGogglesObject() + 0, "n"[0] }, { W2Off_CPed_m_pGogglesState() + 0, "n"[0] }, { W2Off_CPed_m_nPedState() + 0, "u"[0], W2Off_CPed_m_nPedState_sz() }, { W2Off_CPed_m_nMoveState() + 0, "u"[0], W2Off_CPed_m_nMoveState_sz() }, { W2Off_CPed_m_standingOnEntity() + 0, "e"[0] }, { W2Off_CPed_m_pContactEntity() + 0, "e"[0] }, { W2Off_CPed_m_pVehicle() + 0, "v"[0] }, { W2Off_CPed_m_VehDeadInFrontOf() + 0, "v"[0] }, { W2Off_CPed_m_nPedType() + 0, "u"[0], W2Off_CPed_m_nPedType_sz() }, { W2Off_CPed_m_pStats() + 0, "n"[0] }, { W2Off_CPed_m_nSavedWeapon() + 0, "u"[0], W2Off_CPed_m_nSavedWeapon_sz() }, { W2Off_CPed_m_nDelayedWeapon() + 0, "u"[0], W2Off_CPed_m_nDelayedWeapon_sz() }, { W2Off_CPed_m_pTargetedObject() + 0, "e"[0] }, { W2Off_CPed_m_nWeaponSkill() + 0, "u"[0], W2Off_CPed_m_nWeaponSkill_sz() }, { W2Off_CPed_m_nFightingStyle() + 0, "u"[0], W2Off_CPed_m_nFightingStyle_sz() }, { W2Off_CPed_m_pFire() + 0, "n"[0] }, { W2Off_CPed_m_pLookTarget() + 0, "e"[0] }, { W2Off_CPed_m_pLastEntityDamage() + 0, "e"[0] }, { W2Off_CPed_m_pCoverPoint() + 0, "n"[0] }, { W2Off_CPed_m_pEnex() + 0, "n"[0] }, { W2Off_CPhysical_m_nContactSurface() + 0, "u"[0], W2Off_CPhysical_m_nContactSurface_sz() }, { W2Off_CPhysical_m_apCollidedEntities() + 0, "e"[0] }, { W2Off_CPhysical_m_apCollidedEntities() + 4, "e"[0] }, { W2Off_CPhysical_m_apCollidedEntities() + 8, "e"[0] }, { W2Off_CPhysical_m_apCollidedEntities() + 12, "e"[0] }, { W2Off_CPhysical_m_apCollidedEntities() + 16, "e"[0] }, { W2Off_CPhysical_m_apCollidedEntities() + 20, "e"[0] }, { W2Off_CPhysical_m_pDamageEntity() + 0, "e"[0] }, { W2Off_CPhysical_m_pAttachedTo() + 0, "e"[0] }, { W2Off_CPhysical_m_pEntityIgnoredCollision() + 0, "e"[0] }, { W2Off_CPhysical_m_pShadowData() + 0, "n"[0] }, { W2Off_CEntity_m_pRwObject() + 0, "n"[0] }, { W2Off_CEntity_m_pReferences() + 0, "n"[0] }, { W2Off_CEntity_m_AreaCode() + 0, "u"[0], W2Off_CEntity_m_AreaCode_sz() }, { W2Off_CPlaceable_m_matrix() + 0, "n"[0] } }, [&](void* p) -> uint64_t { (void)p; return (static_cast<CPed*>(p)->CPed::DisablePedSpeech(a0), uint64_t(0)); }, [&](void* p) -> uint64_t { (void)p; return (oracle::Fn<void __fastcall(void*, int, int)>(0x5EFF60)(p, 0, (int)a0), uint64_t(0)); }, false, false, &kPVt_CPlayerPed);
    }, 60);
    Run("CPed::DisablePedSpeechForScriptSpeech 0x5EFF80", [&](Rng& r, std::string& d) {
        static const int16 kA0[] = { 0, 1, 0x100, -1, 0x7FFF, 0x0100 | 1 };   // the exe compares the whole word (0x4E56DA cmp word ptr [esp+4], 0): the parameter is an int16, not a bool (the bool-typed port dropped the high byte)
        const int16 a0 = kA0[r.below(6)];
        return W3PedCase(r, d, sizeof(CPed), { { W2Off_CPed_m_roadRageWith() + 0, "p"[0] }, { W2Off_CPed_m_pIntelligence() + 0, "n"[0] }, { W2Off_CPed_m_pPlayerData() + 0, "n"[0] }, { W2Off_CPed_m_nCreatedBy() + 0, "u"[0], W2Off_CPed_m_nCreatedBy_sz() }, { W2Off_CPed_m_pWeaponObject() + 0, "n"[0] }, { W2Off_CPed_m_pGunflashObject() + 0, "n"[0] }, { W2Off_CPed_m_pGogglesObject() + 0, "n"[0] }, { W2Off_CPed_m_pGogglesState() + 0, "n"[0] }, { W2Off_CPed_m_nPedState() + 0, "u"[0], W2Off_CPed_m_nPedState_sz() }, { W2Off_CPed_m_nMoveState() + 0, "u"[0], W2Off_CPed_m_nMoveState_sz() }, { W2Off_CPed_m_standingOnEntity() + 0, "e"[0] }, { W2Off_CPed_m_pContactEntity() + 0, "e"[0] }, { W2Off_CPed_m_pVehicle() + 0, "v"[0] }, { W2Off_CPed_m_VehDeadInFrontOf() + 0, "v"[0] }, { W2Off_CPed_m_nPedType() + 0, "u"[0], W2Off_CPed_m_nPedType_sz() }, { W2Off_CPed_m_pStats() + 0, "n"[0] }, { W2Off_CPed_m_nSavedWeapon() + 0, "u"[0], W2Off_CPed_m_nSavedWeapon_sz() }, { W2Off_CPed_m_nDelayedWeapon() + 0, "u"[0], W2Off_CPed_m_nDelayedWeapon_sz() }, { W2Off_CPed_m_pTargetedObject() + 0, "e"[0] }, { W2Off_CPed_m_nWeaponSkill() + 0, "u"[0], W2Off_CPed_m_nWeaponSkill_sz() }, { W2Off_CPed_m_nFightingStyle() + 0, "u"[0], W2Off_CPed_m_nFightingStyle_sz() }, { W2Off_CPed_m_pFire() + 0, "n"[0] }, { W2Off_CPed_m_pLookTarget() + 0, "e"[0] }, { W2Off_CPed_m_pLastEntityDamage() + 0, "e"[0] }, { W2Off_CPed_m_pCoverPoint() + 0, "n"[0] }, { W2Off_CPed_m_pEnex() + 0, "n"[0] }, { W2Off_CPhysical_m_nContactSurface() + 0, "u"[0], W2Off_CPhysical_m_nContactSurface_sz() }, { W2Off_CPhysical_m_apCollidedEntities() + 0, "e"[0] }, { W2Off_CPhysical_m_apCollidedEntities() + 4, "e"[0] }, { W2Off_CPhysical_m_apCollidedEntities() + 8, "e"[0] }, { W2Off_CPhysical_m_apCollidedEntities() + 12, "e"[0] }, { W2Off_CPhysical_m_apCollidedEntities() + 16, "e"[0] }, { W2Off_CPhysical_m_apCollidedEntities() + 20, "e"[0] }, { W2Off_CPhysical_m_pDamageEntity() + 0, "e"[0] }, { W2Off_CPhysical_m_pAttachedTo() + 0, "e"[0] }, { W2Off_CPhysical_m_pEntityIgnoredCollision() + 0, "e"[0] }, { W2Off_CPhysical_m_pShadowData() + 0, "n"[0] }, { W2Off_CEntity_m_pRwObject() + 0, "n"[0] }, { W2Off_CEntity_m_pReferences() + 0, "n"[0] }, { W2Off_CEntity_m_AreaCode() + 0, "u"[0], W2Off_CEntity_m_AreaCode_sz() }, { W2Off_CPlaceable_m_matrix() + 0, "n"[0] } }, [&](void* p) -> uint64_t { (void)p; return (static_cast<CPed*>(p)->CPed::DisablePedSpeechForScriptSpeech(a0), uint64_t(0)); }, [&](void* p) -> uint64_t { (void)p; return (oracle::Fn<void __fastcall(void*, int, int)>(0x5EFF80)(p, 0, (int)a0), uint64_t(0)); }, false, false, &kPVt_CPlayerPed);
    }, 60);
}

static void TestW3Ped() {
    TestW3PedHand();
#ifdef W3P_HAVE_GEN
    TestW3PedFuncs();
#endif
}
