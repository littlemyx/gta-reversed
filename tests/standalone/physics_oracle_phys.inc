// (b) CPhysical helpers on a fake entity (see PhysFx in physics_oracle_test.cpp)
#include "Entity/Vehicle/Vehicle.h"
#include "tHandlingData.h"
#include "cHandlingDataMgr.h"
template<class Body>
static void PhysRun(const char* name, Body body, int cases = 0) {
    Run(name, [=](Rng& r, std::string& d) {
        PhysFx a; BuildPhys(r, a); PhysFx e(a);
        std::string extra;
        const bool ok = body(r, a.p(), e.p(), extra);
        if (ok && SamePhys(a, e)) return true;
        d = PhysDesc(e) + " " + extra + DiffDesc(a.e, e.e, sizeof a.e, "ent") + DiffDesc(a.m, e.m, sizeof a.m, "mat");
        return false;
    }, cases);
}
static void TestPhysical() {
    using P = CPhysical;
    const float vs = 5.f;
    PhysRun("CPhysical::ApplyMoveForce(vec) 0x5429F0", [&](Rng& r, P& a, P& e, std::string& x) {
        const CVector f = GenV(r, PickScale(r)); x = "f " + V(f);
        a.ApplyMoveForce(f);
        oracle::Fn<void __fastcall(P*, int, CVector)>(0x5429F0)(&e, 0, f);
        return true;
    });
    PhysRun("CPhysical::ApplyTurnForce 0x542A50", [&](Rng& r, P& a, P& e, std::string& x) {
        const CVector f = GenV(r, PickScale(r)), p = GenV(r, PickScale(r)); x = "f " + V(f) + " p " + V(p);
        a.ApplyTurnForce(f, p);
        oracle::Fn<void __fastcall(P*, int, CVector, CVector)>(0x542A50)(&e, 0, f, p);
        return true;
    });
    PhysRun("CPhysical::ApplyForce 0x542B50", [&](Rng& r, P& a, P& e, std::string& x) {
        const CVector f = GenV(r, PickScale(r)), p = GenV(r, PickScale(r)); const bool u = r.below(4) != 0; x = "f " + V(f) + " p " + V(p) + " upd " + std::to_string(u);
        a.ApplyForce(f, p, u);
        oracle::Fn<void __fastcall(P*, int, CVector, CVector, bool)>(0x542B50)(&e, 0, f, p, u);
        return true;
    });
    PhysRun("CPhysical::GetSpeed 0x542CE0", [&](Rng& r, P& a, P& e, std::string& x) {
        const CVector p = GenV(r, PickScale(r)); x = "p " + V(p);
        const CVector ra = a.GetSpeed(p);
        CVector re{ 7, 8, 9 };
        oracle::Fn<CVector * __fastcall(P*, int, CVector*, CVector)>(0x542CE0)(&e, 0, &re, p);
        x += " got " + V(ra) + " exe " + V(re);
        return SameV(ra, re);
    });
    PhysRun("CPhysical::ApplyMoveSpeed 0x542DD0", [&](Rng& r, P& a, P& e, std::string& x) {
        a.ApplyMoveSpeed();
        oracle::Fn<void __fastcall(P*, int)>(0x542DD0)(&e, 0);
        return true;
    });
    PhysRun("CPhysical::ApplyTurnSpeed 0x542E20", [&](Rng& r, P& a, P& e, std::string& x) {
        a.ApplyTurnSpeed();
        oracle::Fn<void __fastcall(P*, int)>(0x542E20)(&e, 0);
        return true;
    });
    PhysRun("CPhysical::ApplyGravity 0x542FE0", [&](Rng& r, P& a, P& e, std::string& x) {
        a.ApplyGravity();
        oracle::Fn<void __fastcall(P*, int)>(0x542FE0)(&e, 0);
        return true;
    });
    PhysRun("CPhysical::ApplyFrictionMoveForce 0x5430A0", [&](Rng& r, P& a, P& e, std::string& x) {
        const CVector f = GenV(r, PickScale(r)); x = "f " + V(f);
        a.ApplyFrictionMoveForce(f);
        oracle::Fn<void __fastcall(P*, int, CVector)>(0x5430A0)(&e, 0, f);
        return true;
    });
    PhysRun("CPhysical::ApplyFrictionTurnForce 0x543100", [&](Rng& r, P& a, P& e, std::string& x) {
        const CVector f = GenV(r, PickScale(r)), p = GenV(r, PickScale(r)); x = "f " + V(f) + " p " + V(p);
        a.ApplyFrictionTurnForce(f, p);
        oracle::Fn<void __fastcall(P*, int, CVector, CVector)>(0x543100)(&e, 0, f, p);
        return true;
    });
    PhysRun("CPhysical::ApplyFrictionForce 0x543220", [&](Rng& r, P& a, P& e, std::string& x) {
        const CVector f = GenV(r, PickScale(r)), p = GenV(r, PickScale(r)); x = "f " + V(f) + " p " + V(p);
        a.ApplyFrictionForce(f, p);
        oracle::Fn<void __fastcall(P*, int, CVector, CVector)>(0x543220)(&e, 0, f, p);
        return true;
    });
    PhysRun("CPhysical::ApplySpringCollision 0x543C90", [&](Rng& r, P& a, P& e, std::string& x) {
        CVector dir = GenDir(r), pt = GenV(r, PickScale(r)), dir2 = dir, pt2 = pt;
        const float lvl = GenF(r, 3.f), len = r.below(4) ? r.f01() * 1.5f : GenF(r, 2.f), bias = GenF(r, 2.f);
        float la = -5.f, le = -5.f;
        x = "dir " + V(dir) + " pt " + V(pt) + " lvl " + F(lvl) + " len " + F(len) + " bias " + F(bias);
        const bool ra = a.ApplySpringCollision(lvl, dir, pt, len, bias, la);
        const bool re = oracle::Fn<bool __fastcall(P*, int, float, CVector*, CVector*, float, float, float*)>(0x543C90)(&e, 0, lvl, &dir2, &pt2, len, bias, &le);
        return ra == re && SameF(la, le) && SameV(dir, dir2) && SameV(pt, pt2);
    });
    PhysRun("CPhysical::ApplySpringCollisionAlt 0x543D60", [&](Rng& r, P& a, P& e, std::string& x) {
        CVector dir = GenDir(r), pt = GenV(r, PickScale(r)), n = GenDir(r), dir2 = dir, pt2 = pt, n2 = n;
        const float lvl = GenF(r, 3.f), len = r.below(4) ? r.f01() * 1.5f : GenF(r, 2.f), bias = GenF(r, 2.f);
        float la = -5.f, le = -5.f;
        x = "dir " + V(dir) + " pt " + V(pt) + " n " + V(n) + " lvl " + F(lvl) + " len " + F(len) + " bias " + F(bias);
        const bool ra = a.ApplySpringCollisionAlt(lvl, dir, pt, len, bias, n, la);
        const bool re = oracle::Fn<bool __fastcall(P*, int, float, CVector*, CVector*, float, float, CVector*, float*)>(0x543D60)(&e, 0, lvl, &dir2, &pt2, len, bias, &n2, &le);
        return ra == re && SameF(la, le) && SameV(dir, dir2) && SameV(pt, pt2) && SameV(n, n2);
    });
    PhysRun("CPhysical::ApplySpringDampening 0x543E90", [&](Rng& r, P& a, P& e, std::string& x) {
        CVector dir = GenDir(r), pt = GenV(r, PickScale(r)), pos = GenV(r, 3.f), dir2 = dir, pt2 = pt, pos2 = pos;
        const float damp = GenF(r, 3.f), lim = GenF(r, 50.f);
        x = "dir " + V(dir) + " pt " + V(pt) + " pos " + V(pos) + " damp " + F(damp) + " lim " + F(lim);
        const bool ra = a.ApplySpringDampening(damp, lim, dir, pt, pos);
        const bool re = oracle::Fn<bool __fastcall(P*, int, float, float, CVector*, CVector*, CVector*)>(0x543E90)(&e, 0, damp, lim, &dir2, &pt2, &pos2);
        return ra == re;
    });
    PhysRun("CPhysical::ApplySpringDampeningOld 0x544100", [&](Rng& r, P& a, P& e, std::string& x) {
        CVector dir = GenDir(r), pt = GenV(r, PickScale(r)), pos = GenV(r, 3.f), dir2 = dir, pt2 = pt, pos2 = pos;
        const float damp = GenF(r, 3.f), unused = GenF(r, 5.f);
        x = "dir " + V(dir) + " pt " + V(pt) + " pos " + V(pos) + " damp " + F(damp);
        const bool ra = a.ApplySpringDampeningOld(damp, unused, dir, pt, pos);
        const bool re = oracle::Fn<bool __fastcall(P*, int, float, float, CVector*, CVector*, CVector*)>(0x544100)(&e, 0, damp, unused, &dir2, &pt2, &pos2);
        return ra == re;
    });
    PhysRun("CPhysical::ApplyAirResistance 0x544C40", [&](Rng& r, P& a, P& e, std::string& x) {
        if (std::isnan(CTimer::ms_fTimeStep)) return true;   // pow(1, NaN): the exe's SSE2 pow gives NaN, the host CRT (C99) gives 1; a NaN time step never occurs in game
        CVehicle::m_fAirResistanceMult = r.below(2) ? 2.5f : GenF(r, 5.f);
        a.ApplyAirResistance();
        oracle::Fn<void __fastcall(P*, int)>(0x544C40)(&e, 0);
        return true;
    });
    // CVehicle wheel processing: fake vehicle (CPhysical fixture + handling data + a few CVehicle fields), rand() seeded identically on both sides
    auto wheelFix = [](Rng& r, P& a, P& e, uint8* hd) {
        for (size_t i = 0; i + 4 <= sizeof(tHandlingData); i += 4) { const float v = r.below(2) ? r.f01() * 4.f : GenF(r, 2000.f); std::memcpy(hd + i, &v, 4); }
        auto* h = reinterpret_cast<tHandlingData*>(hd);
        h->m_fMass = r.below(8) ? 500.f + r.f01() * 4000.f : 1.f + r.f01() * 100.f;   // > 0: the port's std::clamp(-brake, brake) asserts on a negative bound
        h->m_fTractionLoss = r.f01() * 1.5f;   // >= 0: the port's std::clamp(-adhesion, adhesion) asserts on a negative adhesion
        h->m_fSuspensionAntiDiveMultiplier = r.below(3) ? r.f01() : GenF(r, 2.f);
        const auto vtype = (eVehicleType)(r.below(3) == 0 ? VEHICLE_TYPE_BIKE : r.below(10));
        const int16 model = r.below(6) == 0 ? MODEL_RCBANDIT : (int16)r.below(600);
        const float gas = r.below(3) ? r.f01() : GenF(r, 1.5f);
        const auto status = (eEntityStatus)r.below(8);
        for (P* p : { &a, &e }) {
            auto* v = reinterpret_cast<CVehicle*>(p);
            v->m_pHandlingData = h;
            v->m_nVehicleType = vtype;
            v->m_nModelIndex = model;
            v->m_GasPedal = gas;
            v->SetStatus(status);
        }
        gHandlingDataMgr.fWheelFriction = r.below(3) ? 0.9f + r.f01() * 0.2f : r.f01() * 5.f;
    };
    for (int bike = 0; bike < 2; ++bike) {
        PhysRun(bike ? "CVehicle::ProcessBikeWheel 0x6D73B0" : "CVehicle::ProcessWheel 0x6D6C00", [=](Rng& r, P& a, P& e, std::string& x) {
            alignas(16) uint8 hd[sizeof(tHandlingData)];
            wheelFix(r, a, e, hd);
            CTimer::ms_fTimeStep = std::fabs(CTimer::ms_fTimeStep);   // negative / NaN steps make the clamp bounds invalid in the port (assert)
            if (!(CTimer::ms_fTimeStep >= 0.f)) CTimer::ms_fTimeStep = 1.f;
            const float sc = r.below(3) ? 2.f : PickScale(r);
            CVector fwd = GenDir(r), right = GenDir(r), cs = GenV(r, sc), cp = GenV(r, 3.f), fwd2 = fwd, right2 = right, cs2 = cs, cp2 = cp;
            const int32 onGround = 1 + r.below(4);
            const float thrust = r.below(3) ? GenF(r, 1.f) : 0.f, brake = r.below(3) ? 0.f : r.f01() * 2.f, adh = r.below(8) ? r.f01() * 2.f : std::fabs(GenF(r, 3.f)), destab = r.f01();
            for (float c : { fwd.x, fwd.y, fwd.z, right.x, right.y, right.z, cs.x, cs.y, cs.z, cp.x, cp.y, cp.z, thrust, brake, adh }) if (!std::isfinite(c)) return true;   // NaN / inf wheel geometry: NaN compare chains differ (not reachable in game)
            const int8 wid = (int8)r.below(4);
            float ws = 0.f, ws2 = 0.f;
            tWheelState st = (tWheelState)r.below(4), st2 = st;
            const uint16 stat = (uint16)r.below(4);
            const int special = r.below(5);
            const unsigned seed = r.u32();
            for (uint32_t va = 0xC1CDA8; va < 0xC1CDB8; ++va) *(uint8*)va = (uint8)(r.u32() & 1);   // the function's static flags
            uint8 statics0[16]; std::memcpy(statics0, (void*)0xC1CDA8, 16);
            x = "fwd " + V(fwd) + " right " + V(right) + " cs " + V(cs) + " cp " + V(cp) + " og " + std::to_string(onGround) + " thr " + F(thrust) + " br " + F(brake) + " adh " + F(adh) + " st " + std::to_string((int)st) + " stat " + std::to_string(stat) + " sp " + std::to_string(special);
            SeedBoth(seed);
            auto* va_ = reinterpret_cast<CVehicle*>(&a);
            auto* ve_ = reinterpret_cast<CVehicle*>(&e);
            if (bike) va_->ProcessBikeWheel(fwd, right, cs, cp, onGround, thrust, brake, adh, destab, wid, &ws, &st, (eBikeWheelSpecial)special, stat);
            else      va_->ProcessWheel(fwd, right, cs, cp, onGround, thrust, brake, adh, wid, &ws, &st, stat);
            uint8 statics1[16]; std::memcpy(statics1, (void*)0xC1CDA8, 16);
            std::memcpy((void*)0xC1CDA8, statics0, 16);
            SeedBoth(seed);
            if (bike) oracle::Fn<void __fastcall(CVehicle*, int, CVector&, CVector&, CVector&, CVector&, int32, float, float, float, float, int8, float*, tWheelState*, int, uint16)>(0x6D73B0)(ve_, 0, fwd2, right2, cs2, cp2, onGround, thrust, brake, adh, destab, wid, &ws2, &st2, special, stat);
            else      oracle::Fn<void __fastcall(CVehicle*, int, CVector&, CVector&, CVector&, CVector&, int32, float, float, float, int8, float*, tWheelState*, uint16)>(0x6D6C00)(ve_, 0, fwd2, right2, cs2, cp2, onGround, thrust, brake, adh, wid, &ws2, &st2, stat);
            uint8 statics2[16]; std::memcpy(statics2, (void*)0xC1CDA8, 16);
            const bool ok = st == st2 && SameBlob(&ws, &ws2, 4) && !std::memcmp(statics1, statics2, 16) && SameV(fwd, fwd2) && SameV(cs, cs2);
            if (!ok) x += " state " + std::to_string((int)st) + "/" + std::to_string((int)st2) + " statics " + Hex(statics1, 16) + "/" + Hex(statics2, 16);
            return ok;
        }, 0);
    }
    (void)vs;
}
