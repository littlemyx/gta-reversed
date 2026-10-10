// (b) CPhysical helpers on a fake entity (see PhysFx in physics_oracle_test.cpp)
#include "Entity/Vehicle/Vehicle.h"
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
    (void)vs;
}
