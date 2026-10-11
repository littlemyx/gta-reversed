// included by game_oracle_test.cpp: CCollision primitives vs the exe (random CColSphere / CColBox / CColLine / triangle inputs, degenerate ones included)

static float PickGeoScale(Rng& r) { static const float sc[] = { 2.f, 4.f, 10.f, 10.f, 100.f, 3000.f, 1e-2f }; return sc[r.below(7)]; }

static void FillSurface(Rng& r, CColSurface& s) {
    s.m_nMaterial = (eSurfaceType)r.below(179);
    s.m_nPiece = (uint8)r.below(256);
    s.m_nLighting.value = (uint8)r.below(256);
    s.m_nLight = (uint8)r.below(256);
}
static CColSphere GenSphere(Rng& r, float S, const CVector* near_ = nullptr) {
    CColSphere s;
    s.m_vecCenter = near_ ? CVector{ near_->x + GenF(r, S * 0.5f), near_->y + GenF(r, S * 0.5f), near_->z + GenF(r, S * 0.5f) } : GenV(r, S);
    s.m_fRadius = r.below(10) ? (0.05f + r.f01()) * S * 0.6f : GenF(r, S);
    FillSurface(r, s.m_Surface);
    return s;
}
static CBox GenBox(Rng& r, float S) {
    const CVector c = GenV(r, S), e = r.below(12) ? CVector{ r.f01() * S * 0.5f, r.f01() * S * 0.5f, r.f01() * S * 0.5f } : GenV(r, S);
    CBox b;
    b.m_vecMin = CVector(c.x - e.x, c.y - e.y, c.z - e.z);
    b.m_vecMax = CVector(c.x + e.x, c.y + e.y, c.z + e.z);
    if (r.below(30) == 0) std::swap(b.m_vecMin.x, b.m_vecMax.x);
    if (r.sp && r.below(100) < r.sp) b.m_vecMin.y = GenF(r, S);
    return b;
}
static CColBox GenColBox(Rng& r, float S) {
    CColBox cb(GenBox(r, S));
    FillSurface(r, cb.m_Surface);
    return cb;
}
// initial `maxTouchDistance` of the line tests is the t parameter bound (callers pass <= 1; a value > 1 makes the exe return garbage from uninitialised stack variables when no plane is hit)
static float GenLineT0(Rng& r) { return r.below(2) ? 1.0f : r.below(6) ? r.f01() : GenF(r, 1.0f, S_NAN | S_ZERO | S_DEN | S_TINY); }
static CColLine GenLine(Rng& r, float S, bool vertical = false) {
    CColLine l;
    l.m_vecStart = GenV(r, S);
    l.m_vecEnd = r.below(14) ? GenV(r, S) : l.m_vecStart; // some zero-length lines
    if (vertical) { l.m_vecEnd.x = l.m_vecStart.x; l.m_vecEnd.y = l.m_vecStart.y; }
    l.m_fStartSize = GenF(r, 1.f); l.m_fEndSize = GenF(r, 1.f);
    return l;
}

struct COut { CColPoint cp; float t; uint8 poly[sizeof(CStoredCollPoly) + 16]; };
static void FillOut(COut& o, uint32_t seed) {
    Rng q(seed);
    uint8* p = reinterpret_cast<uint8*>(&o);
    for (size_t i = 0; i < sizeof o; ++i) p[i] = (uint8)q.u32();
}
// CColPoint bytes without the 2 padding bytes at 0x26
static bool SameCP(const CColPoint& a, const CColPoint& b) {
    const bool x = SameBlob(&a, &b, 0x20), y = (std::memcmp((const uint8*)&a + 0x20, (const uint8*)&b + 0x20, 6) == 0 ? true : (g_hardDiff = true, false)), z = SameBlob((const uint8*)&a + 0x28, (const uint8*)&b + 0x28, sizeof(CColPoint) - 0x28);
    return x & y & z;
}
static std::string CPs(const CColPoint& c) {
    char b[64]; std::snprintf(b, sizeof b, " surf %u/%u/%02X %u/%u/%02X", (unsigned)c.m_nSurfaceTypeA, c.m_nPieceTypeA, c.m_nLightingA.value, (unsigned)c.m_nSurfaceTypeB, c.m_nPieceTypeB, c.m_nLightingB.value);
    return "pt " + V(c.m_vecPoint) + " f0C " + F(c.field_C) + " n " + V(c.m_vecNormal) + " f1C " + F(c.field_1C) + " depth " + F(c.m_fDepth) + b;
}

// result of a Process* call: ret + colpoint + in/out float (+ poly buffer)
static bool CmpProc(bool ra, const COut& a, bool rb, const COut& b, bool poly, std::string& d, const char* extra = "") {
    Hit(ra);
    const bool same = (ra == rb ? true : (g_hardDiff = true, false)) & SameCP(a.cp, b.cp) & SameF(a.t, b.t) & (!poly || SameBlob(a.poly, b.poly, sizeof(CStoredCollPoly)));
    if (same) return true;
    d = std::string(extra) + " got " + (ra ? "true" : "false") + " t " + F(a.t) + " " + CPs(a.cp) + "  |  exe " + (rb ? "true" : "false") + " t " + F(b.t) + " " + CPs(b.cp);
    if (poly && std::memcmp(a.poly, b.poly, sizeof(CStoredCollPoly)) != 0) d += " POLY differs: " + Hex(a.poly, sizeof(CStoredCollPoly)) + " vs " + Hex(b.poly, sizeof(CStoredCollPoly));
    return false;
}

// ---- triangle mesh input -----------------------------------------------------------------------------------------------------------------
struct TriIn {
    int16_t         v[8][3];
    CColTriangle    tri;
    uint8           plane[sizeof(CColTrianglePlane)];
    const CompressedVector* verts() const { return reinterpret_cast<const CompressedVector*>(v); }
    const CColTrianglePlane& pl() const { return *reinterpret_cast<const CColTrianglePlane*>(plane); }
    CVector Vert(int i) const { return { v[i][0] / 128.f, v[i][1] / 128.f, v[i][2] / 128.f }; }
};
static TriIn GenTri(Rng& r, float S, bool nonDegenerate = false) {
  for (int attempt = 0;; ++attempt) {
    TriIn t{};
    const float lim = std::fmin(S * 128.f, 32000.f);
    for (auto& vv : t.v) for (auto& c : vv) c = (int16_t)((r.f01() * 2.f - 1.f) * lim);
    int ia = r.below(8), ib = r.below(8), ic = r.below(8);
    if (r.below(10)) { // mostly distinct
        while (ib == ia) ib = r.below(8);
        while (ic == ia || ic == ib) ic = r.below(8);
    }
    t.tri = CColTriangle((uint16)ia, (uint16)ib, (uint16)ic, (eSurfaceType)r.below(179), tColLighting((uint8)r.below(256)));
    // plane like CColTrianglePlane::Set: unit normal (int16 / 4096), offset = dot(a, n) (int16 / 128), orientation = dominant axis
    const CVector a = t.Vert(ia), b = t.Vert(ib), c = t.Vert(ic);
    // CColTrianglePlane(vA, vB, vC): normal = (C - A) x (B - A)
    double n[3] = { (double)(c.y - a.y) * (b.z - a.z) - (double)(c.z - a.z) * (b.y - a.y), (double)(c.z - a.z) * (b.x - a.x) - (double)(c.x - a.x) * (b.z - a.z), (double)(c.x - a.x) * (b.y - a.y) - (double)(c.y - a.y) * (b.x - a.x) };
    double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    const double rawLen = len;
    if (len == 0.0) { n[0] = 0; n[1] = 0; n[2] = 1; len = 1; }
    int16_t pn[3] = { (int16_t)std::lround(n[0] / len * 4096.0), (int16_t)std::lround(n[1] / len * 4096.0), (int16_t)std::lround(n[2] / len * 4096.0) };
    if (!nonDegenerate && r.below(12) == 0) for (auto& x : pn) x = (int16_t)(r.u32() & 0x1FFF) - 0x1000;     // inconsistent plane
    const double off = a.x * n[0] / len + a.y * n[1] / len + a.z * n[2] / len;
    int16_t po = (int16_t)std::lround(std::fmax(-32000.0, std::fmin(32000.0, off * 128.0)));
    if (!nonDegenerate && r.below(12) == 0) po = (int16_t)(r.u32() & 0xFFF) - 0x800;
    uint8 ori = 4;
    const double ax = std::fabs(n[0]), ay = std::fabs(n[1]), az = std::fabs(n[2]);
    if (ax > ay && ax > az) ori = n[0] <= 0 ? 1 : 0; else if (ay > az) ori = n[1] <= 0 ? 3 : 2; else ori = n[2] <= 0 ? 5 : 4;
    if (!nonDegenerate && r.below(16) == 0) ori = (uint8)r.below(6);
    std::memset(t.plane, 0, sizeof t.plane);
    std::memcpy(t.plane, pn, 6); std::memcpy(t.plane + 6, &po, 2); t.plane[8] = ori;
    if (nonDegenerate && attempt < 50 && !(rawLen > 1e-7 && ia != ib && ib != ic && ia != ic)) continue;   // collapsed triangles drive the exe into uninitialised locals (see ProcessSphereTriangle)
    return t;
  }
}
static std::string TriS(const TriIn& t) {
    int16_t pn[4]; std::memcpy(pn, t.plane, 8);
    char b[200]; std::snprintf(b, sizeof b, " plane n=(%g,%g,%g) off=%g ori=%u", pn[0] / 4096.0, pn[1] / 4096.0, pn[2] / 4096.0, pn[3] / 128.0, t.plane[8]);
    return "verts " + Hex(t.v, sizeof t.v) + " tri " + Hex(&t.tri, sizeof t.tri) + " A=" + V(t.Vert(t.tri.vA)) + " B=" + V(t.Vert(t.tri.vB)) + " C=" + V(t.Vert(t.tri.vC)) + b + " plane " + Hex(t.plane, sizeof t.plane);
}

// triangles are int16 / 128 meshes: a scale below ~1 collapses them to the origin (the exe then runs on uninitialised locals), above 250 they saturate
static float TriScale(float S) { return std::fmax(std::fmin(S, 250.f), 2.f); }
static void TestCollisionImpl(const std::string& sfx, float (*pick)(Rng&), float cap) {
    // ---- tests without output ----
    Run(std::string("CCollision::TestSphereSphere 0x411E70") + sfx, [pick, cap](Rng& r, std::string& d) {
        const float S = pick(r); const CColSphere a = GenSphere(r, S), b = r.below(2) ? GenSphere(r, S, &a.m_vecCenter) : GenSphere(r, S);
        const bool x = CCollision::TestSphereSphere(a, b), y = oracle::Fn<bool __cdecl(const CColSphere*, const CColSphere*)>(0x411E70)(&a, &b);
        Hit(x); if (x == y) return true;
        d = "a " + Hex(&a, sizeof a) + " b " + Hex(&b, sizeof b) + " got " + (x ? "1" : "0");
        return false;
    });
    Run(std::string("CCollision::TestSphereBox 0x4120C0") + sfx, [pick, cap](Rng& r, std::string& d) {
        const float S = pick(r); const CColSphere a = GenSphere(r, S); const CBox b = GenBox(r, S);
        const CSphere& sp = a;
        const bool x = CCollision::TestSphereBox(sp, b), y = oracle::Fn<bool __cdecl(const CSphere*, const CBox*)>(0x4120C0)(&sp, &b);
        Hit(x); if (x == y) return true;
        d = "sph " + Hex(&sp, sizeof sp) + " box " + Hex(&b, sizeof b) + " got " + (x ? "1" : "0");
        return false;
    });
    Run(std::string("CCollision::PointInTriangle 0x412700") + sfx, [pick, cap](Rng& r, std::string& d) {
        const float S = pick(r); CVector tri[3] = { GenV(r, S), GenV(r, S), GenV(r, S) };
        if (r.below(8) == 0) tri[2] = tri[0];
        CVector p = r.below(3) ? CVector{ (tri[0].x + tri[1].x + tri[2].x) / 3 + GenF(r, S * 0.3f), (tri[0].y + tri[1].y + tri[2].y) / 3 + GenF(r, S * 0.3f), (tri[0].z + tri[1].z + tri[2].z) / 3 + GenF(r, S * 0.3f) } : GenV(r, S);
        if (r.below(5) == 0) p.z = tri[0].z;
        const bool x = CCollision::PointInTriangle(p, tri), y = oracle::Fn<bool __stdcall(const CVector*, const CVector*)>(0x412700)(&p, tri);
        Hit(x); if (x == y) return true;
        d = "p " + V(p) + " tri " + V(tri[0]) + V(tri[1]) + V(tri[2]) + " got " + (x ? "1" : "0");
        return false;
    });
    auto distTest = [&](const char* name, float (*port)(const CVector&, const CVector&, const CVector&), unsigned va) {
        Run(std::string(name) + sfx, [=](Rng& r, std::string& d) {
            const float S = pick(r); const CVector a = GenV(r, S), b = r.below(10) ? GenV(r, S) : a, p = GenV(r, S);
            const float x = port(a, b, p), y = oracle::Fn<float __cdecl(const CVector*, const CVector*, const CVector*)>(va)(&a, &b, &p);
            if (SameF(x, y)) return true;
            d = "a " + V(a) + " b " + V(b) + " p " + V(p) + " got " + F(x) + " exe " + F(y);
            return false;
        });
    };
    distTest("CCollision::DistToLineSqr 0x412850", &CCollision::DistToLineSqr, 0x412850);
    distTest("CCollision::DistToLine 0x417610", &CCollision::DistToLine, 0x417610);
    Run(std::string("CCollision::DistToMathematicalLine 0x412970") + sfx, [pick, cap](Rng& r, std::string& d) {
        const float S = pick(r); const CVector a = GenV(r, S), b = r.below(10) ? GenV(r, S) : a, p = GenV(r, S);
        const float x = CCollision::DistToMathematicalLine(&a, &b, &p), y = oracle::Fn<float __cdecl(const CVector*, const CVector*, const CVector*)>(0x412970)(&a, &b, &p);
        if (SameF(x, y)) return true;
        d = "a " + V(a) + " b " + V(b) + " p " + V(p) + " got " + F(x) + " exe " + F(y);
        return false;
    });
    Run(std::string("CCollision::ClosestPointOnLine 0x417FD0") + sfx, [pick, cap](Rng& r, std::string& d) {
        const float S = pick(r); const CVector a = GenV(r, S), b = r.below(10) ? GenV(r, S) : a, p = GenV(r, S);
        CVector x{ 7, 8, 9 }, y{ 7, 8, 9 };
        CCollision::ClosestPointOnLine(a, b, p, x);
        oracle::Fn<void __cdecl(const CVector*, const CVector*, const CVector*, CVector*)>(0x417FD0)(&a, &b, &p, &y);
        if (SameV(x, y)) return true;
        d = "a " + V(a) + " b " + V(b) + " p " + V(p) + " got " + V(x) + " exe " + V(y);
        return false;
    });
    Run(std::string("CCollision::TestLineSphere 0x417470") + sfx, [pick, cap](Rng& r, std::string& d) {
        const float S = pick(r); const CColLine l = GenLine(r, S); const CColSphere s = GenSphere(r, S, r.below(2) ? &l.m_vecStart : nullptr);
        const bool x = CCollision::TestLineSphere(l, s), y = oracle::Fn<bool __cdecl(const CColLine*, const CColSphere*)>(0x417470)(&l, &s);
        Hit(x); if (x == y) return true;
        d = "line " + Hex(&l, sizeof l) + " sph " + Hex(&s, sizeof s) + " got " + (x ? "1" : "0");
        return false;
    });
    Run(std::string("CCollision::TestLineBox_DW 0x412C70") + sfx, [pick, cap](Rng& r, std::string& d) {
        const float S = pick(r); const CColLine l = GenLine(r, S); const CBox b = GenBox(r, S);
        const bool x = CCollision::TestLineBox_DW(l, b), y = oracle::Fn<bool __cdecl(const CColLine*, const CBox*)>(0x412C70)(&l, &b);
        Hit(x); if (x == y) return true;
        d = "line " + Hex(&l, sizeof l) + " box " + Hex(&b, sizeof b) + " got " + (x ? "1" : "0");
        return false;
    });
    Run(std::string("CCollision::TestLineTriangle 0x413AC0") + sfx, [pick, cap](Rng& r, std::string& d) {
        const float S = pick(r); const CColLine l = GenLine(r, std::fmin(S, cap)); const TriIn t = GenTri(r, TriScale(S));
        const bool x = CCollision::TestLineTriangle(l, t.verts(), t.tri, t.pl());
        const bool y = oracle::Fn<bool __cdecl(const CColLine*, const CompressedVector*, const CColTriangle*, const CColTrianglePlane*)>(0x413AC0)(&l, t.verts(), &t.tri, &t.pl());
        Hit(x); if (x == y) return true;
        d = "line " + Hex(&l, sizeof l) + " " + TriS(t) + " got " + (x ? "1" : "0");
        return false;
    });
    Run(std::string("CCollision::TestSphereTriangle 0x4165B0") + sfx, [pick, cap](Rng& r, std::string& d) {
        const float S = std::fmin(pick(r), cap); const TriIn t = GenTri(r, TriScale(S)); const CVector c = t.Vert(t.tri.vA); const CColSphere s = GenSphere(r, S, r.below(3) ? &c : nullptr);
        const bool x = CCollision::TestSphereTriangle(s, t.verts(), t.tri, t.pl());
        const bool y = oracle::Fn<bool __cdecl(const CColSphere*, const CompressedVector*, const CColTriangle*, const CColTrianglePlane*)>(0x4165B0)(&s, t.verts(), &t.tri, &t.pl());
        Hit(x); if (x == y) return true;
        d = "sph " + Hex(&s, sizeof s) + " " + TriS(t) + " got " + (x ? "1" : "0");
        return false;
    });

    // ---- Process*: ret + colpoint + float in/out (+ poly) ----
    Run(std::string("CCollision::ProcessSphereBox 0x412130") + sfx, [pick, cap](Rng& r, std::string& d) {
        const float S = pick(r); const CColSphere s = GenSphere(r, S); const CColBox b = GenColBox(r, S);
        const uint32_t seed = r.u32(); COut a, e; FillOut(a, seed); e = a;
        a.t = r.below(3) ? r.f01() * S * S : GenF(r, S); e.t = a.t;
        const bool ra = CCollision::ProcessSphereBox(s, b, a.cp, a.t);
        const bool rb = oracle::Fn<bool __cdecl(const CColSphere*, const CColBox*, CColPoint*, float*)>(0x412130)(&s, &b, &e.cp, &e.t);
        return CmpProc(ra, a, rb, e, false, d, ("sph " + Hex(&s, sizeof s) + " box " + Hex(&b, sizeof b) + " t0 " + F(e.t)).c_str());
    });
    Run(std::string("CCollision::ProcessSphereSphere 0x416450") + sfx, [pick, cap](Rng& r, std::string& d) {
        const float S = pick(r); const CColSphere s1 = GenSphere(r, S), s2 = GenSphere(r, S, r.below(2) ? &s1.m_vecCenter : nullptr);
        COut a, e; FillOut(a, r.u32()); e = a; a.t = e.t = r.below(3) ? r.f01() * S : GenF(r, S);
        const float t0 = a.t;
        const bool ra = CCollision::ProcessSphereSphere(s1, s2, a.cp, a.t);
        const bool rb = oracle::Fn<bool __cdecl(const CColSphere*, const CColSphere*, CColPoint*, float*)>(0x416450)(&s1, &s2, &e.cp, &e.t);
        return CmpProc(ra, a, rb, e, false, d, ("s1 " + Hex(&s1, sizeof s1) + " s2 " + Hex(&s2, sizeof s2) + " t0 " + F(t0)).c_str());
    });
    Run(std::string("CCollision::ProcessLineSphere 0x412AA0") + sfx, [pick, cap](Rng& r, std::string& d) {
        const float S = pick(r); const CColLine l = GenLine(r, S); const CColSphere s = GenSphere(r, S, r.below(2) ? &l.m_vecStart : nullptr);
        COut a, e; FillOut(a, r.u32()); e = a; a.t = e.t = GenLineT0(r);
        const float t0 = a.t;
        const bool ra = CCollision::ProcessLineSphere(l, s, a.cp, a.t);
        const bool rb = oracle::Fn<bool __cdecl(const CColLine*, const CColSphere*, CColPoint*, float*)>(0x412AA0)(&l, &s, &e.cp, &e.t);
        return CmpProc(ra, a, rb, e, false, d, ("line " + Hex(&l, sizeof l) + " sph " + Hex(&s, sizeof s) + " t0 " + F(t0)).c_str());
    });
    Run(std::string("CCollision::ProcessLineBox 0x413100") + sfx, [pick, cap](Rng& r, std::string& d) {
        const float S = pick(r); const CColLine l = GenLine(r, S); const CColBox b = GenColBox(r, S);
        COut a, e; FillOut(a, r.u32()); e = a; a.t = e.t = GenLineT0(r);
        const float t0 = a.t;
        const bool ra = CCollision::ProcessLineBox(l, b, a.cp, a.t);
        const bool rb = oracle::Fn<bool __cdecl(const CColLine*, const CColBox*, CColPoint*, float*)>(0x413100)(&l, &b, &e.cp, &e.t);
        return CmpProc(ra, a, rb, e, false, d, ("line " + Hex(&l, sizeof l) + " box " + Hex(&b, sizeof b) + " t0 " + F(t0)).c_str());
    });
    auto lineTri = [&](const char* name, unsigned va, bool vertical, bool (*port)(const CColLine&, const CompressedVector*, const CColTriangle&, const CColTrianglePlane&, CColPoint&, float&, CStoredCollPoly*)) {
        Run(std::string(name) + sfx, [=](Rng& r, std::string& d) {
            const float S = std::fmin(pick(r), cap); const CColLine l = GenLine(r, S, vertical && r.below(10)); const TriIn t = GenTri(r, TriScale(S));
            const bool withPoly = r.below(2);
            COut a, e; FillOut(a, r.u32()); e = a; a.t = e.t = GenLineT0(r);
            const float t0 = a.t;
            auto* pa = withPoly ? reinterpret_cast<CStoredCollPoly*>(a.poly) : nullptr;
            auto* pe = withPoly ? reinterpret_cast<CStoredCollPoly*>(e.poly) : nullptr;
            const bool ra = port(l, t.verts(), t.tri, t.pl(), a.cp, a.t, pa);
            const bool rb = oracle::Fn<bool __cdecl(const CColLine*, const CompressedVector*, const CColTriangle*, const CColTrianglePlane*, CColPoint*, float*, CStoredCollPoly*)>(va)(&l, t.verts(), &t.tri, &t.pl(), &e.cp, &e.t, pe);
            return CmpProc(ra, a, rb, e, true, d, ("line " + Hex(&l, sizeof l) + " " + TriS(t) + " t0 " + F(t0) + (withPoly ? " poly" : " nopoly")).c_str());
        });
    };
    lineTri("CCollision::ProcessLineTriangle 0x4140F0", 0x4140F0, false, &CCollision::ProcessLineTriangle);
    lineTri("CCollision::ProcessVerticalLineTriangle 0x4147E0", 0x4147E0, true, &CCollision::ProcessVerticalLineTriangle);
    Run(std::string("CCollision::ProcessSphereTriangle 0x416BA0") + sfx, [pick, cap](Rng& r, std::string& d) {
        const float S = std::fmin(pick(r), cap); const TriIn t = GenTri(r, TriScale(S), true); const CVector c = t.Vert(t.tri.vB); CColSphere s = GenSphere(r, S, r.below(3) ? &c : nullptr);
        // a NaN sphere makes all three edge tests fail (numPassed == 0): the exe then continues with uninitialised locals (UB, not reproducible). An INFINITE centre does the same
        // (inf - inf = NaN in the edge tests: found by the stream F pilot, the old x87 port has the same 4 'mismatches' in 100,000 cases); an infinite RADIUS is a valid input
        if (!std::isfinite(s.m_vecCenter.x) || !std::isfinite(s.m_vecCenter.y) || !std::isfinite(s.m_vecCenter.z) || std::isnan(s.m_fRadius)) return true;
        COut a, e; FillOut(a, r.u32()); e = a; a.t = e.t = r.below(3) ? 1.0f + r.f01() * S : GenF(r, 2.f);
        const float t0 = a.t;
        const bool ra = CCollision::ProcessSphereTriangle(s, t.verts(), t.tri, t.pl(), a.cp, a.t);
        const bool rb = oracle::Fn<bool __cdecl(const CColSphere*, const CompressedVector*, const CColTriangle*, const CColTrianglePlane*, CColPoint*, float*)>(0x416BA0)(&s, t.verts(), &t.tri, &t.pl(), &e.cp, &e.t);
        return CmpProc(ra, a, rb, e, false, d, ("sph " + Hex(&s, sizeof s) + " " + TriS(t) + " t0 " + F(t0)).c_str());
    });
}

// scales for the stream F wide-range rows: squares of 1e19+ overflow a float but not the x87's extended exponent, squares of 1e-20 underflow into denormals (triangles stay in their int16/128 range)
static float PickGeoScaleWide(Rng& r) { static const float sc[] = { 1e-18f, 1e-10f, 1e-3f, 1.f, 100.f, 3000.f, 1e10f, 1e18f, 1e19f, 3e19f, 1e25f, 1e30f }; return sc[r.below(12)]; }
static void TestCollision() {
    TestCollisionImpl("", PickGeoScale, 250.f);
    TestCollisionImpl(" [wide range]", PickGeoScaleWide, 1e30f);
}
