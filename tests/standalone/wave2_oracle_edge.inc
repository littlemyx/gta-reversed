// ORACLE-W2 (W2-OPEN): hand-maintained edge-input rows. The generated rows feed finite game-like floats only; these feed the values the x87 compare / fnstsw logic of the exe
// treats specially (NaN, +-inf, the exact thresholds 0.1 / 0.9, zero) for the direction argument and for CTimer::ms_fTimeStep, so a compare rewritten with the wrong
// ordered / unordered / equality semantics fails. Included by wave2_oracle_all.inc after wave2_oracle_gen_func.inc (uses its W2Off_* helpers).
static float EdgeF(Rng& r) {
    static const float t[] = { 0.f, -0.f, 0.1f, -0.1f, 0.9f, -0.9f, 1.f, -1.f, 0.5f, -0.5f, 0.05f, 0.95f, 2.f, -2.f, 200.f, -200.f, 3.0e38f, -3.0e38f,
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), -std::numeric_limits<float>::quiet_NaN(), 1.0e-40f, 0.07f };
    if (r.below(4) == 0) { return GF(r, 2.f); }
    return t[r.below((uint32)std::size(t))];
}
static CVector2D EdgeV2(Rng& r) { const float x = EdgeF(r); const float y = EdgeF(r); return { x, y }; }
// CTimer::ms_fTimeStep (0xB7CB5C, read by both sides) for the case, restored afterwards
struct EdgeTimeStep {
    float saved;
    explicit EdgeTimeStep(Rng& r) : saved(CTimer::ms_fTimeStep) {
        // only values whose product with 0.07f is exact in a float (powers of two, 0, inf, NaN, negative): the exe keeps `ts * 0.07f` unrounded in the x87 register (the port rounds it to a
        // float), which differs by one ulp in the CRT-default 53-bit mode (PC53) for any other step - a precision-mode artefact, the game runs with the 24-bit mode (PC24)
        static const float t[] = { 0.f, -0.f, 1.f, 0.5f, 2.f, 4.f, 0.25f, 1024.f, -1.f, -4.f, std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() };
        CTimer::ms_fTimeStep = t[r.below((uint32)std::size(t))];
    }
    ~EdgeTimeStep() { CTimer::ms_fTimeStep = saved; }
};
static void TestW2Edge() {
    using namespace w2;
    Run("CTaskSimpleDuck::ControlDuckMove edge inputs 0x6923F0", [&](Rng& r, std::string& d) {
        CVector2D a0 = EdgeV2(r); EdgeTimeStep ts(r);
        return FuncCase(r, d, sizeof(CTaskSimpleDuck), { { W2Off_CTaskSimpleDuck_m_DuckAnim() + 0, "n"[0] }, { W2Off_CTaskSimpleDuck_m_MoveAnim() + 0, "n"[0] }, { W2Off_CTaskSimpleDuck_m_bIsFinished() + 0, "b"[0] }, { W2Off_CTaskSimpleDuck_m_bIsAborting() + 0, "b"[0] }, { W2Off_CTaskSimpleDuck_m_bNeedToSetDuckFlag() + 0, "b"[0] }, { W2Off_CTaskSimpleDuck_m_bIsInControl() + 0, "b"[0] }, { W2Off_CTaskSimpleDuck_m_DuckControlType() + 0, "u"[0], W2Off_CTaskSimpleDuck_m_DuckControlType_sz() }, { W2Off_CTask_m_Parent() + 0, "n"[0] } }, [&](void* p) -> uint64_t { (void)p; return (static_cast<CTaskSimpleDuck*>(p)->CTaskSimpleDuck::ControlDuckMove(a0), uint64_t(0)); }, [&](void* p) -> uint64_t { (void)p; return (oracle::Fn<void __fastcall(void*, int, CVector2D)>(0x6923F0)(p, 0, a0), uint64_t(0)); }, false);
    }, 400);
    Run("CTaskSimpleUseGun::ControlGunMove edge inputs 0x61E0C0", [&](Rng& r, std::string& d) {
        CVector2D a0 = EdgeV2(r); EdgeTimeStep ts(r);
        return FuncCase(r, d, sizeof(CTaskSimpleUseGun), { { W2Off_CTaskSimpleUseGun_m_IsFinished() + 0, "b"[0] }, { W2Off_CTaskSimpleUseGun_m_IsInControl() + 0, "b"[0] }, { W2Off_CTaskSimpleUseGun_m_HasMoveControl() + 0, "b"[0] }, { W2Off_CTaskSimpleUseGun_m_HasFiredGun() + 0, "b"[0] }, { W2Off_CTaskSimpleUseGun_m_IsLOSBlocked() + 0, "b"[0] }, { W2Off_CTaskSimpleUseGun_m_SkipAim() + 0, "b"[0] }, { W2Off_CTaskSimpleUseGun_m_NextCmd() + 0, "u"[0], W2Off_CTaskSimpleUseGun_m_NextCmd_sz() }, { W2Off_CTaskSimpleUseGun_m_LastCmd() + 0, "u"[0], W2Off_CTaskSimpleUseGun_m_LastCmd_sz() }, { W2Off_CTaskSimpleUseGun_m_Anim() + 0, "n"[0] }, { W2Off_CTaskSimpleUseGun_m_WeaponInfo() + 0, "n"[0] }, { W2Off_CTaskSimpleUseGun_m_IsArmIKInUse() + 0, "b"[0] }, { W2Off_CTaskSimpleUseGun_m_IsLookIKInUse() + 0, "b"[0] }, { W2Off_CTaskSimpleUseGun_m_IsAimImmediate() + 0, "b"[0] }, { W2Off_CTask_m_Parent() + 0, "n"[0] } }, [&](void* p) -> uint64_t { (void)p; return (uint64_t)(uint8_t)(static_cast<CTaskSimpleUseGun*>(p)->CTaskSimpleUseGun::ControlGunMove(a0) ? 1 : 0); }, [&](void* p) -> uint64_t { (void)p; return (uint64_t)((uint8_t)(oracle::Fn<int __fastcall(void*, int, const CVector2D*)>(0x61E0C0)(p, 0, &a0)) ? 1 : 0); }, false);
    }, 400);
}
