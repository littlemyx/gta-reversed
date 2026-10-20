// ORACLE-W3-IK rows B: IKChain.cpp and IKChainManager.cpp (see wave3_oracle_ik.inc).
namespace w3k {
constexpr uintptr_t kIkMgr = 0xC15448;   // g_ikChainMan: 32 IKChain_c (0x58 each), then the active list and the free list (head, tail, count each)
static int g_sel = 0;                    // chain bone selected by the row
static void BuildIkMgr0(Rng& r) {
    for (int i = 0; i < 32; ++i) { auto* n = reinterpret_cast<uint32_t*>(kIkMgr + i * 0x58); n[0] = r.u32(); n[1] = r.u32(); }
    std::memset(reinterpret_cast<void*>(kIkMgr + 0xB00), 0, 24);
}
static int32_t SelTag(int sel, int32_t other, int32_t hi = 0) { return (sel >= 8 ? other : Nf<int32_t>(sel % g_n, oTag)) | hi; }   // a chain bone's tag (read at call time, after the fixture is built) or an arbitrary one
} // namespace w3k

static void TestW3IkB() {
    using namespace w2; using namespace w3k;
    const auto FlagsRand = [&](Rng& r) { StaticRef<uint8_t>(0x8D2BD0) = r.below(3) == 0 ? 1 : 0; StaticRef<uint8_t>(0x8D2BD1) = r.below(3) == 0 ? 1 : 0; };
    // ---- IKChain_c: setters / getters
    KROW("IKChain_c::SetBlend", 0x617C10, 0)
        const float v = SpF(r, 2.f);
        return KCase(r, d, [&]() -> uint64_t { Ch()->SetBlend(v); return 0; }, [&]() -> uint64_t { ExeThis<void, float>(0x617C10, Ch(), v); return 0; });
    KEND(100)
    KROW("IKChain_c::SetOffsetBoneTag", 0x617C20, 0)
        const int32_t t = r.below(2) ? (int32_t)r.below(60) - 1 : (int32_t)r.u32();
        return KCase(r, d, [&]() -> uint64_t { Ch()->SetOffsetBoneTag(BT(t)); return 0; }, [&]() -> uint64_t { ExeThis<void, int32_t>(0x617C20, Ch(), t); return 0; });
    KEND(100)
    KROW("IKChain_c::SetOffsetPos", 0x617C30, 0)
        const CVector v{ SpF(r, 50.f), SpF(r, 50.f), SpF(r, 50.f) };
        return KCase(r, d, [&]() -> uint64_t { Ch()->SetOffsetPos(v); return 0; }, [&]() -> uint64_t { ExeThis<void, CVector>(0x617C30, Ch(), v); return 0; });
    KEND(100)
    KROW("IKChain_c::UpdateOffset", 0x617E20, 0)
        const int32_t t = r.below(2) ? (int32_t)r.below(60) - 1 : (int32_t)r.u32(); const CVector v{ SpF(r, 50.f), SpF(r, 50.f), SpF(r, 50.f) };
        return KCase(r, d, [&]() -> uint64_t { Ch()->UpdateOffset(BT(t), v); return 0; }, [&]() -> uint64_t { ExeThis<void, int32_t, CVector>(0x617E20, Ch(), t, v); return 0; });
    KEND(100)
    KROW("IKChain_c::UpdateTarget", 0x617E50, 0)
        const bool b = r.below(2);
        return KCase(r, d, [&]() -> uint64_t { Ch()->UpdateTarget(b); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t>(0x617E50, Ch(), b); return 0; });
    KEND(100)
    KROW("IKChain_c::GetPriority", 0x617C50, 0)
        return KCase(r, d, [&]() -> uint64_t { return (uint8_t)Ch()->GetPriority(); }, [&]() -> uint64_t { return (uint8_t)ExeThis<uint32_t>(0x617C50, Ch()); });
    KEND(100)
    KROW("IKChain_c::GetBoneNodeFromTag", 0x617C60, 0)
        const int sel = (int)r.below(10); const int32_t o = (int32_t)r.below(60) - 4; const int32_t hi = r.below(4) == 0 ? (int32_t)(0x10000u << r.below(8)) : 0;   // high bits: the exe compares all 32 bits of the tag
        return KCase(r, d, [&]() -> uint64_t { return U((Ch()->*W2Get(W3K_GetBoneNode{}))(BT(SelTag(sel, o, hi)))); }, [&]() -> uint64_t { return U(ExeThis<void*, int32_t>(0x617C60, Ch(), SelTag(sel, o, hi))); });
    KEND(400)
    // ---- IKChain_c: limits / speed through the bones
    KROW("IKChain_c::GetLimits", 0x618590, 0)
        const int ax = (int)r.below(3), sel = (int)r.below(8);
        return KCase(r, d, [&]() -> uint64_t { Ch()->GetLimits(BT(SelTag(sel, 0)), (eRotationAxis)ax, At<float>(kOut), At<float>(kOut + 4)); return 0; },
            [&]() -> uint64_t { ExeThis<void, int32_t, int, void*, void*>(0x618590, Ch(), SelTag(sel, 0), ax, P(kOut), P(kOut + 4)); return 0; });
    KEND(300)
    KROW("IKChain_c::SetLimits", 0x6185E0, 0)
        const int ax = (int)r.below(3), sel = (int)r.below(8); const float lo = SpF(r, 180.f), hi = SpF(r, 180.f);
        return KCase(r, d, [&]() -> uint64_t { Ch()->SetLimits(BT(SelTag(sel, 0)), (eRotationAxis)ax, lo, hi); return 0; },
            [&]() -> uint64_t { ExeThis<void, int32_t, int, float, float>(0x6185E0, Ch(), SelTag(sel, 0), ax, lo, hi); return 0; });
    KEND(300)
    KROW("IKChain_c::GetBoneSpeed", 0x618630, 0)
        const int sel = (int)r.below(8);
        return KCase(r, d, [&]() -> uint64_t { return FB(Ch()->GetBoneSpeed(BT(SelTag(sel, 0)))); }, [&]() -> uint64_t { return FB(ExeThis<float, int32_t>(0x618630, Ch(), SelTag(sel, 0))); });
    KEND(300)
    KROW("IKChain_c::SetBoneSpeed", 0x618680, 0)
        const int sel = (int)r.below(8); const float v = SpF(r, 3.f);
        return KCase(r, d, [&]() -> uint64_t { Ch()->SetBoneSpeed(BT(SelTag(sel, 0)), v); return 0; }, [&]() -> uint64_t { ExeThis<void, int32_t, float>(0x618680, Ch(), SelTag(sel, 0), v); return 0; });
    KEND(300)
    KROW("IKChain_c::ClampLimits", 0x618520, 0)
        const int sel = (int)r.below(10); const int32_t o = (int32_t)r.below(60) - 4; const bool lx = r.below(2), ly = r.below(2), lz = r.below(2), cur = r.below(2);
        FlagsRand(r);
        return KCase(r, d, [&]() -> uint64_t { Ch()->ClampLimits(BT(SelTag(sel, o)), lx, ly, lz, cur); return 0; },
            [&]() -> uint64_t { ExeThis<void, int32_t, uint32_t, uint32_t, uint32_t, uint32_t>(0x618520, Ch(), SelTag(sel, o), lx, ly, lz, cur); return 0; });
    KEND(500)
    KROW("IKChain_c::IsAtTarget", 0x617F30, 0)
        const float range = r.below(6) == 0 ? SpF(r, 10.f) : r.f01() * 260.f; const bool wantDist = r.below(4) != 0;
        return KCase(r, d, [&]() -> uint64_t { return Ch()->IsAtTarget(range, wantDist ? &At<float>(kOut) : nullptr) ? 1 : 0; },
            [&]() -> uint64_t { return (uint8_t)ExeThis<uint32_t, float, void*>(0x617F30, Ch(), range, wantDist ? P(kOut) : nullptr) ? 1 : 0; });
    KEND(1000)
    // ---- IKChainManager_c (the exe's global g_ikChainMan)
    KROW("IKChainManager_c::Init", 0x6180A0, 0)
        return KCase(r, d, [&]() -> uint64_t { return g_ikChainMan.Init() ? 1 : 0; }, [&]() -> uint64_t { return (uint8_t)ExeThis<uint32_t>(0x6180A0, &g_ikChainMan) ? 1 : 0; }, &BuildIkMgr0);
    KEND(60)
}
