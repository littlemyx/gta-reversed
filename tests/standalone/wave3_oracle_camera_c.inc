// ORACLE-W3-CAMERA rows C: CIdleCam (IdleCam.cpp), the global idle camera at 0xB6FDA0. ROW / CC / Tw are defined in wave3_oracle_camera.inc.
static void TestW3CameraC() {
    using namespace w2; using namespace w3c;
    CIdleCam* const I = &gIdleCam;
    ROW("CIdleCam::Init", 0x50E6D0, 150) { CC(I->Init(); return 0;, ExeT<void>(0x50E6D0, I); return 0;); } ENDROW;
    ROW("CIdleCam::Reset", 0x50A160, 300) {
        const int rc = r.below(2);
        CC(I->Reset(rc != 0); return 0;, ExeT<void>(0x50A160, I, rc); return 0;);
    } ENDROW;
    ROW("CIdleCam::SetTarget", 0x50A280, 800) {
        const int k = r.below(7);
        CEntity* e = k == 6 ? nullptr : Ent(k);
        CC(I->SetTarget(e); return 0;, ExeT<void>(0x50A280, I, e); return 0;);
    } ENDROW;
    ROW("CIdleCam::SetTargetPlayer", 0x50EB50, 500) {
        Tw tw([](Rng& rr) { if (rr.below(6) == 0) CWorld::Players[CWorld::PlayerInFocus].m_pPed = nullptr; });
        CC(I->SetTargetPlayer(); return 0;, ExeT<void>(0x50EB50, I); return 0;);
    } ENDROW;
    ROW("CIdleCam::IdleCamGeneralProcess", 0x50E690, 600) {
        Tw tw([](Rng& rr) {
            TheCamera.m_nActiveCam = (uint8)(rr.below(10) == 0 ? 2 : rr.below(2));
            if (rr.below(2)) TheCamera.m_aCams[TheCamera.m_nActiveCam].m_nMode = MODE_FOLLOWPED;
            gIdleCam.m_IdleTickerFrames = rr.below(3) ? (int32)rr.below(200000) : (int32)(90000 + (int)rr.below(3) - 1);
            gIdleCam.m_TimeControlsIdleForIdleToKickIn = rr.below(2) ? 90000.f : Fl(rr, 100000.f, 4);
        });
        CC(I->IdleCamGeneralProcess(); return 0;, ExeT<void>(0x50E690, I); return 0;);
    } ENDROW;
}
