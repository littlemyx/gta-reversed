// ORACLE-W3-COLL part A: game_sa/Collision class A functions (pure geometry maths) vs the exe. Included by game_oracle_test.cpp when W3_COLL_ONLY is defined
// (executable wave3_coll_oracle_test). Fixtures are built so that the interesting branch is reached: spheres/boxes/triangles/lines that really intersect (game-range
// scale), plus the special-value mix of Rng::sp (NaN / +-0 / denormal / huge / tiny / inf) that the harness already applies to every GenF.

static float W3S(Rng& r) { static const float sc[] = { 1.f, 2.f, 5.f, 10.f, 10.f, 50.f, 100.f, 500.f, 3000.f, 1e-2f }; return sc[r.below(10)]; }
static void W3Garbage(Rng& r, void* p, size_t n) { auto* b = static_cast<uint8*>(p); for (size_t i = 0; i < n; ++i) b[i] = (uint8)r.u32(); }
// dword-wise float-aware compare; a trailing partial dword is compared byte-wise
static bool W3Same(const void* a, const void* b, size_t n) {
    bool ok = SameBlob(a, b, n & ~size_t(3));
    if (n & 3) { const size_t o = n & ~size_t(3); if (std::memcmp((const uint8*)a + o, (const uint8*)b + o, n & 3)) { g_hardDiff = true; ok = false; } }
    return ok;
}
static CVector W3Unit(Rng& r) {
    double v[3] = { r.f01() * 2.0 - 1.0, r.f01() * 2.0 - 1.0, r.f01() * 2.0 - 1.0 };
    double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l < 1e-6) { v[0] = 0; v[1] = 0; v[2] = 1; l = 1; }
    return { (float)(v[0] / l), (float)(v[1] / l), (float)(v[2] / l) };
}
static CVector W3Lerp(const CVector& a, const CVector& b, float t) { return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t }; }
static CVector W3Add(const CVector& a, const CVector& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
static CVector W3Mul(const CVector& a, float s) { return { a.x * s, a.y * s, a.z * s }; }
// a point inside or near the box
static CVector W3NearBox(Rng& r, const CBox& b, float slack) {
    return { b.m_vecMin.x + (b.m_vecMax.x - b.m_vecMin.x) * (r.f01() * (1.f + slack) - slack * 0.5f), b.m_vecMin.y + (b.m_vecMax.y - b.m_vecMin.y) * (r.f01() * (1.f + slack) - slack * 0.5f),
             b.m_vecMin.z + (b.m_vecMax.z - b.m_vecMin.z) * (r.f01() * (1.f + slack) - slack * 0.5f) };
}
static CBox W3SaneBox(Rng& r, float S) {
    const CVector c = GenV(r, S, S_ZERO); const CVector e{ 0.05f + r.f01() * S * 0.4f, 0.05f + r.f01() * S * 0.4f, 0.05f + r.f01() * S * 0.4f };
    return CBox{ CVector(c.x - e.x, c.y - e.y, c.z - e.z), CVector(c.x + e.x, c.y + e.y, c.z + e.z) };
}
static CColSphere W3Sphere(Rng& r, const CVector& c, float rad) { CColSphere s; s.m_vecCenter = c; s.m_fRadius = rad; FillSurface(r, s.m_Surface); return s; }
static float W3Rad(Rng& r, float S) { const float base = std::fmax(0.1f, std::fmin(S * 0.08f, 5.f)); return r.below(15) ? base * (0.3f + r.f01() * 1.7f) : GenF(r, base); }

// colpoint compare without the padding floats at 0x0C / 0x1C and the 2 pad bytes at 0x26 (the exe copies whole stack locals whose padding is indeterminate)
static bool W3SameCPNoPad(const CColPoint& a, const CColPoint& b) {
    const auto* p = reinterpret_cast<const uint8*>(&a); const auto* q = reinterpret_cast<const uint8*>(&b);
    bool ok = SameBlob(p, q, 12) & SameBlob(p + 0x10, q + 0x10, 12) & SameBlob(p + 0x28, q + 0x28, 4);
    if (std::memcmp(p + 0x20, q + 0x20, 6) != 0) { g_hardDiff = true; ok = false; }
    return ok;
}
// matrix compare without the 4 padding dwords (+0x0C / 0x1C / 0x2C / 0x3C): the exe leaves them as its stack temporaries, the port as its own
static bool W3SameMatNoPad(const void* a, const void* b) {
    bool ok = true; for (int r = 0; r < 4; ++r) ok &= SameBlob((const uint8*)a + 16 * r, (const uint8*)b + 16 * r, 12); return ok;
}

struct W3Mat { RawMat m; void* attach; uint32 own; };   // CMatrix layout (0x48)
static_assert(sizeof(W3Mat) == 0x48);
static RawMat W3Rigid(Rng& r, bool ident, float S) {
    RawMat m{};
    if (ident) { m.right = CVector{ 1, 0, 0 }; m.fwd = CVector{ 0, 1, 0 }; m.up = CVector{ 0, 0, 1 }; m.pos = r.below(2) ? CVector{ 0, 0, 0 } : GenV(r, S, S_ZERO); return m; }
    const CVector ax = W3Unit(r); const double ang = r.f01() * 6.2831853, c = std::cos(ang), s = std::sin(ang), t = 1 - c, x = ax.x, y = ax.y, z = ax.z;
    const double R[3][3] = { { t * x * x + c, t * x * y - s * z, t * x * z + s * y }, { t * x * y + s * z, t * y * y + c, t * y * z - s * x }, { t * x * z - s * y, t * y * z + s * x, t * z * z + c } };
    m.right = CVector{ (float)R[0][0], (float)R[1][0], (float)R[2][0] }; m.fwd = CVector{ (float)R[0][1], (float)R[1][1], (float)R[2][1] }; m.up = CVector{ (float)R[0][2], (float)R[1][2], (float)R[2][2] };
    m.pos = GenV(r, S, S_ZERO);
    return m;
}
static CVector W3ToLocal(const RawMat& m, const CVector& p) {
    const double dx = (double)p.x - m.pos.x, dy = (double)p.y - m.pos.y, dz = (double)p.z - m.pos.z;
    return { (float)(dx * m.right.x + dy * m.right.y + dz * m.right.z), (float)(dx * m.fwd.x + dy * m.fwd.y + dz * m.fwd.z), (float)(dx * m.up.x + dy * m.up.y + dz * m.up.z) };
}
static CVector W3ToLocalDir(const RawMat& m, const CVector& p) {
    return { (float)((double)p.x * m.right.x + (double)p.y * m.right.y + (double)p.z * m.right.z), (float)((double)p.x * m.fwd.x + (double)p.y * m.fwd.y + (double)p.z * m.fwd.z),
             (float)((double)p.x * m.up.x + (double)p.y * m.up.y + (double)p.z * m.up.z) };
}

static void TestW3CollA() {
    // ---- plain data setters ----
    Run("CBox::Set 0x40EDE0", [](Rng& r, std::string& d) {
        const float S = W3S(r); const CVector mn = GenV(r, S), mx = GenV(r, S);
        alignas(4) uint8 a[sizeof(CBox)], e[sizeof(CBox)]; W3Garbage(r, a, sizeof a); std::memcpy(e, a, sizeof a);
        reinterpret_cast<CBox*>(a)->Set(mn, mx);
        oracle::Fn<void __fastcall(void*, int, const CVector*, const CVector*)>(0x40EDE0)(e, 0, &mn, &mx);
        if (W3Same(a, e, sizeof a)) return true;
        d = "min " + V(mn) + " max " + V(mx) + " got " + Hex(a, sizeof a) + " exe " + Hex(e, sizeof e); return false;
    });
    Run("CBox::Recalc 0x40EE20", [](Rng& r, std::string& d) {
        const float S = W3S(r); CBox b = GenBox(r, S);
        if (r.below(3) == 0) std::swap(b.m_vecMin.y, b.m_vecMax.y);
        if (r.below(3) == 0) std::swap(b.m_vecMin.z, b.m_vecMax.z);
        if (r.below(8) == 0) b.m_vecMax = b.m_vecMin;
        alignas(4) uint8 a[sizeof(CBox)], e[sizeof(CBox)]; std::memcpy(a, &b, sizeof a); std::memcpy(e, &b, sizeof a);
        reinterpret_cast<CBox*>(a)->Recalc();
        oracle::Fn<void __fastcall(void*)>(0x40EE20)(e);
        if (W3Same(a, e, sizeof a)) return true;
        d = "box " + Hex(&b, sizeof b) + " got " + Hex(a, sizeof a) + " exe " + Hex(e, sizeof e); return false;
    });
    Run("CColLine::Set 0x40EF10", [](Rng& r, std::string& d) {
        const float S = W3S(r); const CVector s = GenV(r, S), en = GenV(r, S);
        alignas(4) uint8 a[sizeof(CColLine)], e[sizeof(CColLine)]; W3Garbage(r, a, sizeof a); std::memcpy(e, a, sizeof a);
        reinterpret_cast<CColLine*>(a)->Set(s, en);
        oracle::Fn<void __fastcall(void*, int, const CVector*, const CVector*)>(0x40EF10)(e, 0, &s, &en);
        if (W3Same(a, e, sizeof a)) return true;
        d = "start " + V(s) + " end " + V(en) + " got " + Hex(a, sizeof a) + " exe " + Hex(e, sizeof e); return false;
    });
    Run("CColSphere::Set 0x40FD10", [](Rng& r, std::string& d) {
        const float S = W3S(r); const CVector c = GenV(r, S); const float rad = GenF(r, S);
        const int mat = r.below(179), piece = r.below(256), light = r.below(256);
        alignas(4) uint8 a[sizeof(CColSphere)], e[sizeof(CColSphere)]; W3Garbage(r, a, sizeof a); std::memcpy(e, a, sizeof a);
        reinterpret_cast<CColSphere*>(a)->Set(rad, c, (eSurfaceType)mat, (uint8)piece, tColLighting((uint8)light));
        oracle::Fn<void __fastcall(void*, int, float, const CVector*, int, int, int)>(0x40FD10)(e, 0, rad, &c, mat, piece, light);
        if (W3Same(a, e, sizeof a)) return true;
        d = "r " + F(rad) + " c " + V(c) + " mat/piece/light " + std::to_string(mat) + "/" + std::to_string(piece) + "/" + std::to_string(light) + " got " + Hex(a, sizeof a) + " exe " + Hex(e, sizeof e); return false;
    });
    Run("CColTrianglePlane::GetNormal-Out 0x411610", [](Rng& r, std::string& d) {
        alignas(4) uint8 pl[sizeof(CColTrianglePlane)]; W3Garbage(r, pl, sizeof pl);
        if (r.below(4) == 0) { const int16_t ext[] = { -32768, 32767, 0, 4096, -4096 }; for (int i = 0; i < 3; ++i) std::memcpy(pl + 2 * i, &ext[r.below(5)], 2); }
        CVector a{ 111.f, 222.f, 333.f }, e = a;
        reinterpret_cast<CColTrianglePlane*>(pl)->GetNormal(a);
        oracle::Fn<void __fastcall(void*, int, CVector*)>(0x411610)(pl, 0, &e);
        if (SameV(a, e)) return true;
        d = "plane " + Hex(pl, sizeof pl) + " got " + V(a) + " exe " + V(e); return false;
    });
    Run("CColTrianglePlane::Set 0x411660", [](Rng& r, std::string& d) {
        const float S = std::fmin(W3S(r), 250.f); const TriIn t = GenTri(r, TriScale(S), r.below(4) != 0);
        alignas(4) uint8 a[sizeof(CColTrianglePlane) + 2], e[sizeof(CColTrianglePlane) + 2]; W3Garbage(r, a, sizeof a); std::memcpy(e, a, sizeof a);
        reinterpret_cast<CColTrianglePlane*>(a)->Set(t.verts(), t.tri);
        oracle::Fn<void __fastcall(void*, int, const CompressedVector*, const CColTriangle*)>(0x411660)(e, 0, t.verts(), &t.tri);
        a[9] = e[9] = 0;   // padding byte: the exe leaves it alone, the port copies a stack temporary (harmless)
    Hit(t.plane[8] != 0);
        if (std::memcmp(a, e, sizeof a) == 0) return true;
        g_hardDiff = true; d = TriS(t) + " got " + Hex(a, sizeof a) + " exe " + Hex(e, sizeof e); return false;
    });
    Run("CCollision::Update 0x411E20 (empty)", [](Rng&, std::string&) {
        CCollision::Update();
        oracle::Fn<void __cdecl()>(0x411E20)();
        return true;
    });

    // ---- 2D / 1D line maths ----
    Run("CCollision::DistToMathematicalLine2D 0x412A30", [](Rng& r, std::string& d) {
        const float S = W3S(r); float v[6]; for (auto& x : v) x = GenF(r, S);
        if (r.below(10) == 0) { v[2] = v[0]; v[3] = v[1]; }
        const float x = CCollision::DistToMathematicalLine2D(v[0], v[1], v[2], v[3], v[4], v[5]);
        const float y = oracle::Fn<float __cdecl(float, float, float, float, float, float)>(0x412A30)(v[0], v[1], v[2], v[3], v[4], v[5]);
        if (SameF(x, y)) return true;
        d = "args " + F(v[0]) + F(v[1]) + F(v[2]) + F(v[3]) + F(v[4]) + F(v[5]) + " got " + F(x) + " exe " + F(y); return false;
    });
    Run("CCollision::DistAlongLine2D 0x412A80", [](Rng& r, std::string& d) {
        const float S = W3S(r); float v[6]; for (auto& x : v) x = GenF(r, S);
        const float x = CCollision::DistAlongLine2D(v[0], v[1], v[2], v[3], v[4], v[5]);
        const float y = oracle::Fn<float __cdecl(float, float, float, float, float, float)>(0x412A80)(v[0], v[1], v[2], v[3], v[4], v[5]);
        if (SameF(x, y)) return true;
        d = "args " + F(v[0]) + F(v[1]) + F(v[2]) + F(v[3]) + F(v[4]) + F(v[5]) + " got " + F(x) + " exe " + F(y); return false;
    });
    Run("CCollision::Test2DLineAgainst2DLine 0x4138D0", [](Rng& r, std::string& d) {
        const float S = W3S(r); float v[8];
        if (r.below(4)) {   // two segments through a common point X (crossing or just missing)
            const float X = GenF(r, S), Y = GenF(r, S); const CVector d1 = W3Unit(r), d2 = W3Unit(r);
            const float u1 = r.f01() * S * 0.5f, u2 = r.f01() * S * 0.5f, w1 = r.f01() * S * 0.5f, w2 = r.f01() * S * 0.5f;
            const float miss = r.below(5) == 0 ? S * 0.3f : 0.f;
            v[0] = X - d1.x * u1; v[1] = Y - d1.y * u1; v[2] = X + d1.x * u2; v[3] = Y + d1.y * u2;
            v[4] = X - d2.x * w1 + miss; v[5] = Y - d2.y * w1; v[6] = X + d2.x * w2 + miss; v[7] = Y + d2.y * w2;
        } else for (auto& x : v) x = GenF(r, S);
        if (r.below(12) == 0) { v[6] = v[4]; v[7] = v[5]; }
        const bool x = CCollision::Test2DLineAgainst2DLine(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
        const bool y = oracle::Fn<bool __cdecl(float, float, float, float, float, float, float, float)>(0x4138D0)(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
        Hit(x); if (x == y) return true;
        d = "args"; for (float f : v) d += " " + F(f); d += x ? " got 1" : " got 0"; return false;
    });

    // ---- line vs box ----
    Run("CCollision::TestLineBox 0x413070", [](Rng& r, std::string& d) {
        const float S = W3S(r); const CBox b = r.below(8) ? W3SaneBox(r, S) : GenBox(r, S); CColLine l = GenLine(r, S);
        if (r.below(4)) { l.m_vecStart = W3NearBox(r, b, 2.f); l.m_vecEnd = r.below(2) ? W3NearBox(r, b, 2.f) : W3Add(l.m_vecStart, W3Mul(W3Unit(r), S * 0.5f)); }
        const bool x = CCollision::TestLineBox(l, b), y = oracle::Fn<bool __cdecl(const CColLine*, const CBox*)>(0x413070)(&l, &b);
        Hit(x); if (x == y) return true;
        d = "line " + Hex(&l, sizeof l) + " box " + Hex(&b, sizeof b) + " got " + (x ? "1" : "0"); return false;
    });
    Run("CCollision::TestVerticalLineBox 0x413080", [](Rng& r, std::string& d) {
        const float S = W3S(r); const CBox b = r.below(8) ? W3SaneBox(r, S) : GenBox(r, S); CColLine l = GenLine(r, S, true);
        if (r.below(4)) {
            const CVector p = W3NearBox(r, b, 0.6f);
            l.m_vecStart = CVector{ p.x, p.y, b.m_vecMin.z + (b.m_vecMax.z - b.m_vecMin.z) * (r.f01() * 3.f - 1.f) };
            l.m_vecEnd = CVector{ p.x, p.y, b.m_vecMin.z + (b.m_vecMax.z - b.m_vecMin.z) * (r.f01() * 3.f - 1.f) };
        }
        const bool x = CCollision::TestVerticalLineBox(l, b), y = oracle::Fn<bool __cdecl(const CColLine*, const CBox*)>(0x413080)(&l, &b);
        Hit(x); if (x == y) return true;
        d = "line " + Hex(&l, sizeof l) + " box " + Hex(&b, sizeof b) + " got " + (x ? "1" : "0"); return false;
    });
    Run("CalculateColPointInsideBox 0x411EC0", [](Rng& r, std::string& d) {
        const float S = W3S(r); const CBox b = r.below(8) ? W3SaneBox(r, S) : GenBox(r, S);
        CVector p = r.below(5) ? W3NearBox(r, b, 0.f) : GenV(r, S);
        if (r.below(8) == 0) p.x = b.m_vecMin.x;
        COut a, e; FillOut(a, r.u32()); e = a;
        CalculateColPointInsideBox(b, p, a.cp);
        oracle::Fn<void __cdecl(const CBox*, const CVector*, CColPoint*)>(0x411EC0)(&b, &p, &e.cp);
        Hit(true);
        if (W3SameCPNoPad(a.cp, e.cp)) return true;
        d = "box " + Hex(&b, sizeof b) + " p " + V(p) + " got " + CPs(a.cp) + " exe " + CPs(e.cp); return false;
    });

    // ---- disc ----
    Run("CCollision::ProcessDiscCollision 0x413960", [](Rng& r, std::string& d) {
        const float S = std::fmin(W3S(r), 100.f);
        const RawMat m = W3Rigid(r, r.below(3) == 0, S);
        CColDisk disk{}; W3Garbage(r, &disk, sizeof disk);
        disk.m_vecCenter = GenV(r, S, S_ZERO); disk.m_fRadius = 0.2f + r.f01() * S * 0.3f; disk.m_vThickness = W3Unit(r); disk.m_fThickness = 0.05f + r.f01() * S * 0.1f;
        disk.m_Surface.m_nPiece = (uint8)r.below(32);
        const CVector u1 = [&] { CVector t = disk.m_vThickness, q = std::fabs(t.x) < 0.8f ? CVector{ 1, 0, 0 } : CVector{ 0, 1, 0 }; CVector c{ t.y * q.z - t.z * q.y, t.z * q.x - t.x * q.z, t.x * q.y - t.y * q.x }; const float l = std::sqrt(c.x * c.x + c.y * c.y + c.z * c.z); return CVector{ c.x / l, c.y / l, c.z / l }; }();
        const CVector u2{ disk.m_vThickness.y * u1.z - disk.m_vThickness.z * u1.y, disk.m_vThickness.z * u1.x - disk.m_vThickness.x * u1.z, disk.m_vThickness.x * u1.y - disk.m_vThickness.y * u1.x };
        const float ra = (r.f01() * 2.f - 1.f) * disk.m_fRadius * 0.9f, rb = (r.f01() * 2.f - 1.f) * disk.m_fRadius * 0.9f, h = (r.f01() * 2.f - 1.f) * disk.m_fThickness * 1.3f;
        CVector cpWorld{ disk.m_vecCenter.x + u1.x * ra + u2.x * rb + disk.m_vThickness.x * h, disk.m_vecCenter.y + u1.y * ra + u2.y * rb + disk.m_vThickness.y * h, disk.m_vecCenter.z + u1.z * ra + u2.z * rb + disk.m_vThickness.z * h };
        CVector nW; switch (r.below(4)) { case 0: nW = r.below(2) ? disk.m_vThickness : W3Mul(disk.m_vThickness, -1.f); break; case 1: nW = u1; break; default: nW = W3Unit(r); }
        COut tri, dcp, lcp; FillOut(tri, r.u32()); FillOut(dcp, r.u32()); FillOut(lcp, r.u32());
        tri.cp.m_vecPoint = W3ToLocal(m, cpWorld);   // matBA maps A -> B (B = R*A + pos), so A = R^T (B - pos)
        tri.cp.m_vecNormal = W3ToLocalDir(m, nW);
        tri.cp.m_fDepth = r.below(6) ? r.f01() * 2.f : GenF(r, 2.f);
        dcp.cp.m_fDepth = r.below(3) ? r.f01() * 2.f : GenF(r, 2.f);
        if (r.below(8) == 0) { tri.cp.m_vecPoint = GenV(r, S); tri.cp.m_vecNormal = GenV(r, 1.f); }
        float lr0 = r.below(5) ? (r.f01() * 4.f - 1.f) * (S * 0.3f) : GenF(r, 2.f);
        bool lc0 = r.below(2);
        COut tri2 = tri, dcp2 = dcp, lcp2 = lcp; float lr1 = lr0; bool lc1 = lc0;
        CMatrix cm; std::memcpy(static_cast<void*>(&cm), &m, sizeof m);
        const bool ra_ = CCollision::ProcessDiscCollision(tri.cp, cm, disk, dcp.cp, lc0, lr0, lcp.cp);
        const bool re_ = oracle::Fn<bool __cdecl(CColPoint*, const CMatrix*, const CColDisk*, CColPoint*, bool*, float*, CColPoint*)>(0x413960)(&tri2.cp, &cm, &disk, &dcp2.cp, &lc1, &lr1, &lcp2.cp);
        Hit(ra_);
        const bool ok = ra_ == re_ && SameCP(tri.cp, tri2.cp) && SameCP(dcp.cp, dcp2.cp) && SameCP(lcp.cp, lcp2.cp) && lc0 == lc1 && SameF(lr0, lr1);
        if (ok) return true;
        g_hardDiff = true;
        d = "mat " + Hex(&m, sizeof m) + " disk " + Hex(&disk, sizeof disk) + " tri " + CPs(tri2.cp) + " dcp0 " + CPs(dcp2.cp) + " lr0 " + F(lr1) + " | ret " + (ra_ ? "1" : "0") + "/" + (re_ ? "1" : "0") + " lc " + (lc0 ? "1" : "0") + "/" + (lc1 ? "1" : "0") + " lr " + F(lr0) + "/" + F(lr1) + " dcp " + CPs(dcp.cp) + " | " + CPs(dcp2.cp);
        return false;
    });

    // ---- stored poly / bounding box / vehicle ----
    Run("CCollision::IsStoredPolyStillValidVerticalLine 0x414D70", [](Rng& r, std::string& d) {
        const float S = W3S(r);
        CStoredCollPoly pa{}; W3Garbage(r, &pa, sizeof pa);
        for (auto& v : pa.verts) v = GenV(r, S);
        pa.valid = r.below(8) != 0;
        const float w1 = r.f01(), w2 = r.f01() * (1.f - w1); const CVector q{ pa.verts[0].x + (pa.verts[1].x - pa.verts[0].x) * w1 + (pa.verts[2].x - pa.verts[0].x) * w2, pa.verts[0].y + (pa.verts[1].y - pa.verts[0].y) * w1 + (pa.verts[2].y - pa.verts[0].y) * w2, pa.verts[0].z + (pa.verts[1].z - pa.verts[0].z) * w1 + (pa.verts[2].z - pa.verts[0].z) * w2 };
        CVector origin = r.below(8) ? CVector{ q.x, q.y, q.z + (0.1f + r.f01() * 3.f) * S * 0.2f } : GenV(r, S);
        float lnMag = r.below(8) ? q.z - (0.1f + r.f01() * 3.f) * S * 0.2f : GenF(r, S);
        if (r.below(6) == 0) { origin.x += S * 0.1f; origin.y -= S * 0.1f; }
        COut a, e; FillOut(a, r.u32()); e = a;
        CStoredCollPoly pe = pa;
        const bool x = CCollision::IsStoredPolyStillValidVerticalLine(origin, lnMag, a.cp, &pa);
        const bool y = oracle::Fn<bool __cdecl(const CVector*, float, CColPoint*, CStoredCollPoly*)>(0x414D70)(&origin, lnMag, &e.cp, &pe);
        Hit(x);
        if (x == y && W3SameCPNoPad(a.cp, e.cp) && W3Same(&pa, &pe, sizeof pa)) return true;
        d = "origin " + V(origin) + " lnMag " + F(lnMag) + " got " + (x ? "1 " : "0 ") + CPs(a.cp) + " valid " + std::to_string(pa.valid) + " | exe " + (y ? "1 " : "0 ") + CPs(e.cp) + " valid " + std::to_string(pe.valid) + " cp got " + Hex(&a.cp, 0x2C) + " exe " + Hex(&e.cp, 0x2C) + " poly got " + Hex(&pa, sizeof pa) + " exe " + Hex(&pe, sizeof pe); return false;
    });
    Run("CCollision::GetBoundingBoxFromTwoSpheres 0x415230", [](Rng& r, std::string& d) {
        const float S = W3S(r); const CColSphere a = GenSphere(r, S), b = r.below(2) ? GenSphere(r, S, &a.m_vecCenter) : GenSphere(r, S);
        alignas(4) uint8 e[sizeof(CColBox)]; W3Garbage(r, e, sizeof e);
        const CColBox x = CCollision::GetBoundingBoxFromTwoSpheres(a, b);   // the exe leaves the surface of the returned box alone (the port zero-initialises it): only the box is compared
        oracle::Fn<void* __cdecl(void*, const CColSphere*, const CColSphere*)>(0x415230)(e, &a, &b);
        if (W3Same(&x, e, sizeof(CBox))) return true;
        d = "a " + Hex(&a, sizeof a) + " b " + Hex(&b, sizeof b) + " got " + Hex(&x, sizeof x) + " exe " + Hex(e, sizeof e); return false;
    });
    Run("CCollision::IsThisVehicleSittingOnMe 0x4152C0", [](Rng& r, std::string& d) {
        alignas(16) static uint8 vb[2][0x1000];
        W3Garbage(r, vb[0], sizeof vb[0]); W3Garbage(r, vb[1], sizeof vb[1]);
        void* const pv = vb[0];
        for (size_t o = 0; o < 0xA00; o += 4) { const int k = r.below(6); if (k == 0) { const uintptr_t v = (uintptr_t)pv; std::memcpy(vb[1] + o, &v, 4); } else if (k <= 2) { const uint32 z = 0; std::memcpy(vb[1] + o, &z, 4); } }
        const int types[] = { VEHICLE_TYPE_AUTOMOBILE, VEHICLE_TYPE_BIKE, VEHICLE_TYPE_AUTOMOBILE, VEHICLE_TYPE_BIKE, VEHICLE_TYPE_BOAT, VEHICLE_TYPE_QUAD, VEHICLE_TYPE_BMX, VEHICLE_TYPE_TRAILER };
        reinterpret_cast<CVehicle*>(vb[1])->m_nVehicleType = (eVehicleType)types[r.below(8)];
        // the arrays of the wheel collision entities / ground physicals: some slots really hold `pv`
        CVehicle* veh = r.below(10) ? reinterpret_cast<CVehicle*>(pv) : nullptr; CVehicle* on = r.below(12) ? reinterpret_cast<CVehicle*>(vb[1]) : nullptr;
        if (r.below(2)) { for (auto*& p : reinterpret_cast<CAutomobile*>(vb[1])->m_apWheelCollisionEntity) if (r.below(3) == 0) p = reinterpret_cast<CPhysical*>(pv); }
        else            { for (auto*& p : reinterpret_cast<CBike*>(vb[1])->m_aGroundPhysicalPtrs) if (r.below(3) == 0) p = reinterpret_cast<CPhysical*>(pv); }
        const bool x = CCollision::IsThisVehicleSittingOnMe(veh, on), y = oracle::Fn<bool __cdecl(CVehicle*, CVehicle*)>(0x4152C0)(veh, on);
        Hit(x); if (x == y) return true;
        d = std::string("veh ") + (veh ? "set" : "null") + " on " + (on ? "set" : "null") + " type " + std::to_string((int)reinterpret_cast<CVehicle*>(vb[1])->m_nVehicleType) + " got " + (x ? "1" : "0"); return false;
    });

    // ---- sphere casts ----
    Run("CCollision::SphereCastVsSphere 0x417F20", [](Rng& r, std::string& d) {
        const float S = W3S(r); const float ra = W3Rad(r, S), rs = W3Rad(r, S);
        const CVector a = GenV(r, S); const CVector b = r.below(10) ? W3Add(a, W3Mul(W3Unit(r), r.f01() * S * 0.6f)) : GenV(r, S);
        CVector sc = W3Lerp(a, b, r.f01()); sc = W3Add(sc, W3Mul(W3Unit(r), r.f01() * (std::fabs(ra) + std::fabs(rs)) * 1.5f));
        if (r.below(8) == 0) sc = GenV(r, S);
        const CColSphere sa = W3Sphere(r, a, ra), sb = W3Sphere(r, b, r.below(4) ? ra : W3Rad(r, S)), ss = W3Sphere(r, sc, rs);
        const bool x = CCollision::SphereCastVsSphere(sa, sb, ss), y = oracle::Fn<bool __cdecl(const CColSphere*, const CColSphere*, const CColSphere*)>(0x417F20)(&sa, &sb, &ss);
        Hit(x); if (x == y) return true;
        d = "A " + Hex(&sa, sizeof sa) + " B " + Hex(&sb, sizeof sb) + " S " + Hex(&ss, sizeof ss) + " got " + (x ? "1" : "0"); return false;
    });
    Run("CCollision::SphereCastVsBBox 0x415590", [](Rng& r, std::string& d) {
        const float S = W3S(r); const float ra = W3Rad(r, S);
        const CVector a = GenV(r, S); const CVector b = r.below(10) ? W3Add(a, W3Mul(W3Unit(r), r.f01() * S * 0.6f)) : GenV(r, S);
        CColBox box = r.below(8) ? CColBox(W3SaneBox(r, S)) : GenColBox(r, S);
        if (r.below(5)) { const CVector c = W3Add(W3Lerp(a, b, r.f01()), W3Mul(W3Unit(r), r.f01() * S * 0.2f)); const CVector e{ 0.05f + r.f01() * S * 0.2f, 0.05f + r.f01() * S * 0.2f, 0.05f + r.f01() * S * 0.2f };
            box.m_vecMin = CVector{ c.x - e.x, c.y - e.y, c.z - e.z }; box.m_vecMax = CVector{ c.x + e.x, c.y + e.y, c.z + e.z }; }
        const CColSphere sa = W3Sphere(r, a, ra), sb = W3Sphere(r, b, r.below(4) ? ra : W3Rad(r, S));
        const bool x = CCollision::SphereCastVsBBox(sa, sb, box), y = oracle::Fn<bool __cdecl(const CColSphere*, const CColSphere*, const CColBox*)>(0x415590)(&sa, &sb, &box);
        Hit(x); if (x == y) return true;
        d = "A " + Hex(&sa, sizeof sa) + " B " + Hex(&sb, sizeof sb) + " box " + Hex(&box, sizeof box) + " got " + (x ? "1" : "0"); return false;
    });
    Run("CCollision::SphereCastVersusVsPoly 0x415CF0", [](Rng& r, std::string& d) {
        const float S = std::fmin(W3S(r), 250.f); const TriIn t = GenTri(r, TriScale(S), r.below(6) != 0);
        int16_t pn[4]; std::memcpy(pn, t.plane, 8); const CVector n{ pn[0] / 4096.f, pn[1] / 4096.f, pn[2] / 4096.f };
        const CVector va = t.Vert(t.tri.vA), vb = t.Vert(t.tri.vB), vc = t.Vert(t.tri.vC);
        const float w1 = r.f01(), w2 = r.f01() * (1.f - w1);
        const CVector q{ va.x + (vb.x - va.x) * w1 + (vc.x - va.x) * w2, va.y + (vb.y - va.y) * w1 + (vc.y - va.y) * w2, va.z + (vb.z - va.z) * w1 + (vc.z - va.z) * w2 };
        const float rad = W3Rad(r, TriScale(S)), h1 = (0.05f + r.f01() * 2.f) * rad, h2 = (r.f01() * 3.f - 0.5f) * rad;
        CVector a = W3Add(q, W3Mul(n, h1)), b = W3Add(q, W3Mul(n, -h2));
        if (r.below(3) == 0) { const CVector s = W3Mul(W3Unit(r), rad); a = W3Add(a, s); b = W3Add(b, W3Mul(s, 0.5f)); }
        if (r.below(10) == 0) { a = GenV(r, S); b = GenV(r, S); }
        const CColSphere sa = W3Sphere(r, a, rad), sb = W3Sphere(r, b, r.below(5) ? rad : W3Rad(r, S));
        const bool x = CCollision::SphereCastVersusVsPoly(sa, sb, t.tri, t.pl(), const_cast<CompressedVector*>(t.verts()));
        const bool y = oracle::Fn<bool __cdecl(const CColSphere*, const CColSphere*, const CColTriangle*, const CColTrianglePlane*, const CompressedVector*)>(0x415CF0)(&sa, &sb, &t.tri, &t.pl(), t.verts());
        Hit(x); if (x == y) return true;
        d = "A " + Hex(&sa, sizeof sa) + " B " + Hex(&sb, sizeof sb) + " " + TriS(t) + " got " + (x ? "1" : "0"); return false;
    });

    // ---- ray / point-in-polygon / closest points ----
    Run("CCollision::RayPolyPOP 0x415620", [](Rng& r, std::string& d) {
        const float S = W3S(r); const CVector N = r.below(8) ? W3Unit(r) : GenV(r, 1.f), P = GenV(r, S);
        CVector D = GenV(r, S * 0.5f); CVector A = GenV(r, S);
        if (r.below(4)) { const float a = r.f01() * S * 0.5f; A = W3Add(P, W3Mul(N, a)); D = W3Add(W3Mul(N, -(a + r.f01() * S)), W3Mul(W3Unit(r), r.f01() * S * 0.3f)); }
        CColTriangle tri{}; CVector Aa = A, Ae = A; CVector Da = D, De = D, Na = N, Ne = N, Pa = P, Pe = P;
        const bool x = CCollision::RayPolyPOP(&Aa, &Da, &tri, &Na, &Pa), y = oracle::Fn<bool __cdecl(CVector*, CVector*, CColTriangle*, CVector*, CVector*)>(0x415620)(&Ae, &De, &tri, &Ne, &Pe);
        Hit(x); if (x == y && SameV(Aa, Ae) && SameV(Da, De) && SameV(Na, Ne) && SameV(Pa, Pe)) return true;
        d = "A " + V(A) + " D " + V(D) + " N " + V(N) + " P " + V(P) + " got " + (x ? "1 " : "0 ") + V(Aa) + " exe " + (y ? "1 " : "0 ") + V(Ae); return false;
    });
    Run("CCollision::GetPrincipleAxis 0x4156D0", [](Rng& r, std::string& d) {
        const float S = W3S(r); CVector n = GenV(r, S);
        switch (r.below(6)) { case 0: n.y = n.x; break; case 1: n.z = n.y; break; case 2: n.z = n.x = -n.y; break; case 3: n.x = -n.x; n.z = n.x; break; default: break; }
        const int x = CCollision::GetPrincipleAxis(n), y = oracle::Fn<int __cdecl(const CVector*)>(0x4156D0)(&n);
        Hit(x != 0); if (x == y) return true;
        d = "n " + V(n) + " got " + std::to_string(x) + " exe " + std::to_string(y); return false;
    });
    Run("CCollision::PointInPoly 0x415730", [](Rng& r, std::string& d) {
        const float S = W3S(r); CVector v[3] = { GenV(r, S), GenV(r, S), GenV(r, S) };
        if (r.below(12) == 0) v[2] = v[0];
        const double e1[3] = { (double)v[1].x - v[0].x, (double)v[1].y - v[0].y, (double)v[1].z - v[0].z }, e2[3] = { (double)v[2].x - v[0].x, (double)v[2].y - v[0].y, (double)v[2].z - v[0].z };
        CVector n{ (float)(e2[1] * e1[2] - e2[2] * e1[1]), (float)(e2[2] * e1[0] - e2[0] * e1[2]), (float)(e2[0] * e1[1] - e2[1] * e1[0]) };
        if (r.below(8) == 0) n = GenV(r, 1.f);
        if (r.below(10) == 0) { n.y = n.x; }
        CVector p; if (r.below(2)) { const float w1 = r.f01(), w2 = r.f01() * (1.f - w1); p = CVector{ v[0].x + (v[1].x - v[0].x) * w1 + (v[2].x - v[0].x) * w2, v[0].y + (v[1].y - v[0].y) * w1 + (v[2].y - v[0].y) * w2, v[0].z + (v[1].z - v[0].z) * w1 + (v[2].z - v[0].z) * w2 }; if (r.below(3) == 0) p = W3Add(p, W3Mul(W3Unit(r), S * 0.05f)); }
        else p = GenV(r, S);
        CColTriangle tri{};
        const bool x = CCollision::PointInPoly(p, tri, n, v), y = oracle::Fn<bool __cdecl(const CVector*, const CColTriangle*, const CVector*, const CVector*)>(0x415730)(&p, &tri, &n, v);
        Hit(x); if (x == y) return true;
        d = "p " + V(p) + " n " + V(n) + " v " + V(v[0]) + V(v[1]) + V(v[2]) + " got " + (x ? "1" : "0"); return false;
    });
    Run("CCollision::Closest3 0x415950", [](Rng& r, std::string& d) {
        const float S = W3S(r); CVector pts[3] = { GenV(r, S), GenV(r, S), GenV(r, S) }; if (r.below(8) == 0) pts[1] = pts[0];
        CVector pa = GenV(r, S), pe;
        if (r.below(3) == 0) pa = W3Add(pts[r.below(3)], W3Mul(W3Unit(r), S * 0.1f));
        pe = pa; CVector pts2[3] = { pts[0], pts[1], pts[2] };
        CCollision::Closest3(pts, &pa);
        oracle::Fn<void __cdecl(CVector*, CVector*)>(0x415950)(pts2, &pe);
        if (SameV(pa, pe) && SameV(pts[0], pts2[0]) && SameV(pts[1], pts2[1]) && SameV(pts[2], pts2[2])) return true;
        d = "pts " + V(pts2[0]) + V(pts2[1]) + V(pts2[2]) + " in " + V(pe) + " got " + V(pa); return false;
    });
    Run("CCollision::ClosestPointsOnPoly 0x418100", [](Rng& r, std::string& d) {
        const float S = W3S(r); CVector tri[3] = { GenV(r, S), GenV(r, S), GenV(r, S) }; const CVector p = GenV(r, S);
        CVector oa[3], oe[3]; for (int i = 0; i < 3; ++i) { oa[i] = oe[i] = CVector{ 7.f + i, 8.f, 9.f }; }
        CCollision::ClosestPointsOnPoly(nullptr, tri, const_cast<CVector*>(&p), oa);
        oracle::Fn<void __cdecl(void*, CVector*, const CVector*, CVector*)>(0x418100)(nullptr, tri, &p, oe);
        if (SameV(oa[0], oe[0]) && SameV(oa[1], oe[1]) && SameV(oa[2], oe[2])) return true;
        d = "tri " + V(tri[0]) + V(tri[1]) + V(tri[2]) + " p " + V(p) + " got " + V(oa[0]) + V(oa[1]) + V(oa[2]) + " exe " + V(oe[0]) + V(oe[1]) + V(oe[2]); return false;
    });
    Run("CCollision::ClosestPointOnPoly 0x418150", [](Rng& r, std::string& d) {
        const float S = W3S(r); CVector tri[3] = { GenV(r, S), GenV(r, S), GenV(r, S) }; CVector pa = GenV(r, S), pe = pa;
        CCollision::ClosestPointOnPoly(nullptr, tri, &pa);
        oracle::Fn<void __cdecl(void*, CVector*, CVector*)>(0x418150)(nullptr, tri, &pe);
        if (SameV(pa, pe)) return true;
        d = "tri " + V(tri[0]) + V(tri[1]) + V(tri[2]) + " p " + V(pe) + " got " + V(pa); return false;
    });
    Run("ClosestSquaredDistanceBetweenFiniteLines 0x415A40", [](Rng& r, std::string& d) {
        const float S = W3S(r); const CVector s1 = GenV(r, S), s1e = r.below(12) ? GenV(r, S) : s1;
        CVector s2a = GenV(r, S), s2b = r.below(12) ? GenV(r, S) : s2a;
        if (r.below(2)) { const CVector m = W3Lerp(s1, s1e, r.f01()); s2a = W3Add(m, W3Mul(W3Unit(r), S * 0.2f)); s2b = W3Add(m, W3Mul(W3Unit(r), S * 0.2f)); }
        if (r.below(8) == 0) { s2a = W3Add(s1, W3Mul(W3Unit(r), S * 0.1f)); s2b = W3Add(s1e, W3Mul(W3Unit(r), S * 0.1f)); }   // nearly parallel
        const CVector u{ s1e.x - s1.x, s1e.y - s1.y, s1e.z - s1.z };
        const float a = r.below(10) ? (float)((double)u.x * u.x + (double)u.y * u.y + (double)u.z * u.z) : GenF(r, S);
        const float x = ClosestSquaredDistanceBetweenFiniteLines(s1, s2a, s2b, u, a);
        const float y = oracle::Fn<float __cdecl(const CVector*, const CVector*, const CVector*, const CVector*, float)>(0x415A40)(&s1, &s2a, &s2b, &u, a);
        const double xd = ClosestSquaredDistanceBetweenFiniteLinesOG(s1, s2a, s2b, u, a), yd = oracle::Fn<double __cdecl(const CVector*, const CVector*, const CVector*, const CVector*, float)>(0x415A40)(&s1, &s2a, &s2b, &u, a);
        uint64_t xb, yb; std::memcpy(&xb, &xd, 8); std::memcpy(&yb, &yd, 8);
        const bool dok = xb == yb || (std::isnan(xd) && std::isnan(yd)) || (g_special && false);
        if (SameF(x, y) && dok) return true;
        if (!dok) g_hardDiff = true;
        d = "s1 " + V(s1) + " u " + V(u) + " a " + F(a) + " s2 " + V(s2a) + V(s2b) + " got " + F(x) + " exe " + F(y) + " dbl got " + std::to_string(xd) + " exe " + std::to_string(yd); return false;
    });

    // ---- CCollisionData ----
    auto triPoint = [](const char* name, unsigned va, bool shadow) {
        Run(name, [=](Rng& r, std::string& d) {
            alignas(16) uint8 cd[0x30]; W3Garbage(r, cd, sizeof cd);
            alignas(4) static int16_t verts[2][16][3]; for (auto& s : verts) for (auto& v : s) for (auto& c : v) c = (int16_t)r.u32();
            CompressedVector* pv = reinterpret_cast<CompressedVector*>(verts[0]); CompressedVector* ps = reinterpret_cast<CompressedVector*>(verts[1]);
            std::memcpy(cd + 0x14, &pv, 4); std::memcpy(cd + 0x28, &ps, 4);
            const int id = r.below(16); CVector a{ 5.f, 6.f, 7.f }, e = a;
            auto* c = reinterpret_cast<CCollisionData*>(cd);
            if (shadow) c->GetShadTrianglePoint(a, id); else c->GetTrianglePoint(a, id);
            oracle::Fn<void __fastcall(void*, int, CVector*, int)>(va)(cd, 0, &e, id);
            if (SameV(a, e)) return true;
            d = "id " + std::to_string(id) + " got " + V(a) + " exe " + V(e); return false;
        });
    };
    triPoint("CCollisionData::GetTrianglePoint 0x40F5E0", 0x40F5E0, false);
    triPoint("CCollisionData::GetShadTrianglePoint 0x40F640", 0x40F640, true);
    Run("CCollisionData::SetLinkPtr 0x40F6C0", [](Rng& r, std::string& d) {
        alignas(16) uint8 ca[0x30], ce[0x30]; W3Garbage(r, ca, sizeof ca);
        alignas(16) static uint8 planesA[0x80], planesE[0x80]; W3Garbage(r, planesA, sizeof planesA);
        uint8* base = planesA + r.below(4); uint16 n = (uint16)r.below(8);
        std::memcpy(ca + 4, &n, 2); std::memcpy(ca + 0x1C, &base, 4);
        std::memcpy(ce, ca, sizeof ca); std::memcpy(planesE, planesA, sizeof planesA);
        // the exe side must see the same addresses: run it on the same plane buffer (restored from a copy)
        uint8 save[0x80]; std::memcpy(save, planesA, sizeof save);
        void* link = reinterpret_cast<void*>(uintptr_t(0x0BADF00D) + r.below(0x1000) * 4);
        reinterpret_cast<CCollisionData*>(ca)->SetLinkPtr(reinterpret_cast<CLink<CCollisionData*>*>(link));
        uint8 after[0x80]; std::memcpy(after, planesA, sizeof after);
        std::memcpy(planesA, save, sizeof save);
        oracle::Fn<void __fastcall(void*, int, void*)>(0x40F6C0)(ca, 0, link);
        if (std::memcmp(after, planesA, sizeof after) == 0 && std::memcmp(ca, ce, 0x30) == 0) return true;
        g_hardDiff = true; d = "n " + std::to_string(n) + " base+" + std::to_string(base - planesA) + " got " + Hex(after, sizeof after) + " exe " + Hex(planesA, sizeof after); return false;
    });
    Run("CCollisionData::GetLinkPtr 0x40F6E0", [](Rng& r, std::string& d) {
        alignas(16) uint8 ca[0x30]; W3Garbage(r, ca, sizeof ca);
        alignas(16) static uint8 planes[0x80]; W3Garbage(r, planes, sizeof planes);
        uint8* base = planes + r.below(4); uint16 n = (uint16)r.below(8);
        std::memcpy(ca + 4, &n, 2); std::memcpy(ca + 0x1C, &base, 4);
        alignas(16) static uint8 lk[12]; const uint32 self = (uint32)(uintptr_t)ca; std::memcpy(lk, &self, 4);   // link->data == this (the port asserts it)
        uint8* slot = reinterpret_cast<uint8*>((reinterpret_cast<uintptr_t>(base + 10 * n) + 3) & ~uintptr_t(3));
        const uintptr_t lkp = reinterpret_cast<uintptr_t>(lk); std::memcpy(slot, &lkp, 4);
        void* x = reinterpret_cast<CCollisionData*>(ca)->GetLinkPtr();
        void* y = oracle::Fn<void* __fastcall(void*)>(0x40F6E0)(ca);
        Hit(x != nullptr); if (x == y) return true;
        d = "n " + std::to_string(n) + " got " + Hex(&x, 4) + " exe " + Hex(&y, 4); return false;
    });
}
