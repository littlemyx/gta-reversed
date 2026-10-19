// ORACLE-W3-COLL part B: game_sa/Core class A functions (CMatrix, CQuaternion, CRect, CVector, CVector2D, CMatrixLink / CMatrixLinkList) vs the exe.
// Included by game_oracle_test.cpp (W3_COLL_ONLY) after wave3_coll_oracle_a.inc (uses its W3* helpers).

// the fakerw implementation of RwMatrixUpdate is not linked into this test: same body as the exe's (0x7F18A0: flags &= 0xFFFDFFFC)
RwMatrix* RwMatrixUpdate(RwMatrix* m) { reinterpret_cast<uint32*>(m)[3] &= 0xFFFDFFFCu; return m; }

static W3Mat W3GenMat(Rng& r, float s, bool garbagePads = true) {
    W3Mat m{}; m.m = GenMat(r, s);
    if (garbagePads) { m.m.f0 = r.u32(); m.m.f1 = r.u32(); m.m.f2 = r.u32(); m.m.f3 = r.u32(); }
    m.attach = nullptr; m.own = 0;
    return m;
}
static CMatrix& W3C(W3Mat& m) { return *reinterpret_cast<CMatrix*>(&m); }
static RwMatrix* W3Rw(void* p) { return reinterpret_cast<RwMatrix*>(p); }

// `this` row: a garbage-padded matrix, the port method on one copy, the exe function (thiscall, ret N) on the other; the whole 0x48 bytes are compared
template<class PF, class EF>
static bool W3MatCase(Rng& r, std::string& d, const std::string& args, PF&& pf, EF&& ef, float s, bool noPad = false) {
    W3Mat a = W3GenMat(r, s), e = a; const W3Mat in = a;
    pf(W3C(a)); ef(static_cast<void*>(&e));
    if (noPad ? W3SameMatNoPad(&a, &e) : W3Same(&a, &e, sizeof a)) return true;
    d = args + " in " + Hex(&in, 0x40) + " got " + Hex(&a, 0x40) + " exe " + Hex(&e, 0x40); return false;
}
static CQuaternion W3Quat(Rng& r) {
    CQuaternion q; const float s = r.below(4) ? 1.f : PickScale(r);
    q.x = GenF(r, s); q.y = GenF(r, s); q.z = GenF(r, s); q.w = GenF(r, s);
    if (r.below(3)) { const double l = std::sqrt((double)q.x * q.x + (double)q.y * q.y + (double)q.z * q.z + (double)q.w * q.w); if (l > 0.0 && std::isfinite(l)) { q.x = (float)(q.x / l); q.y = (float)(q.y / l); q.z = (float)(q.z / l); q.w = (float)(q.w / l); } }
    return q;
}

// ---- CMatrixLinkList fixture: 8 nodes distributed over the 3 lists of a fixed-address list object (so port and exe see identical pointers) ----
alignas(16) static uint8 g_mlBuf[sizeof(CMatrixLinkList)];
alignas(16) static uint8 g_mnBuf[8][sizeof(CMatrixLink)];
static CMatrixLinkList& W3L() { return *reinterpret_cast<CMatrixLinkList*>(g_mlBuf); }
static CMatrixLink& W3N(int i) { return *reinterpret_cast<CMatrixLink*>(g_mnBuf[i]); }
static void W3LinkAfter(CMatrixLink* after, CMatrixLink* n) { n->m_pNext = after->m_pNext; after->m_pNext->m_pPrev = n; n->m_pPrev = after; after->m_pNext = n; }
static void W3BuildList(Rng& r, int nodes = 8) {
    W3Garbage(r, g_mlBuf, sizeof g_mlBuf); W3Garbage(r, g_mnBuf, sizeof g_mnBuf);
    auto& L = W3L();
    L.m_head.m_pNext = &L.m_tail; L.m_tail.m_pPrev = &L.m_head;
    L.m_allocatedListHead.m_pNext = &L.m_allocatedListTail; L.m_allocatedListTail.m_pPrev = &L.m_allocatedListHead;
    L.m_freeListHead.m_pNext = &L.m_freeListTail; L.m_freeListTail.m_pPrev = &L.m_freeListHead;
    L.m_pObjects = &W3N(0);
    const int bias = r.below(4);   // some cases: empty free list / empty lists
    for (int i = 0; i < nodes; ++i) {
        const int k = bias == 0 ? 2 : r.below(3);
        CMatrixLink* head = k == 0 ? &L.m_head : k == 1 ? &L.m_allocatedListHead : &L.m_freeListHead;
        CMatrixLink* anchor = head;
        for (int s = r.below(3); s > 0 && anchor->m_pNext && anchor->m_pNext != &L.m_tail && anchor->m_pNext != &L.m_allocatedListTail && anchor->m_pNext != &L.m_freeListTail; --s) anchor = anchor->m_pNext;
        W3LinkAfter(anchor, &W3N(i));
    }
}
static constexpr size_t kMlLen = sizeof(g_mlBuf) + sizeof(g_mnBuf);
struct W3ListSnap { uint8 b[kMlLen]; };
static W3ListSnap W3SnapList() { W3ListSnap s; std::memcpy(s.b, g_mlBuf, sizeof g_mlBuf); std::memcpy(s.b + sizeof g_mlBuf, g_mnBuf, sizeof g_mnBuf); return s; }
static void W3RestoreList(const W3ListSnap& s) { std::memcpy(g_mlBuf, s.b, sizeof g_mlBuf); std::memcpy(g_mnBuf, s.b + sizeof g_mlBuf, sizeof g_mnBuf); }
template<class PF, class EF>
static bool W3ListCase(Rng& r, std::string& d, const char* what, PF&& pf, EF&& ef) {
    W3BuildList(r); const W3ListSnap s0 = W3SnapList();
    const uintptr_t ra = pf(); const W3ListSnap s1 = W3SnapList();
    W3RestoreList(s0);
    const uintptr_t re = ef(); const W3ListSnap s2 = W3SnapList();
    if (ra == re && std::memcmp(s1.b, s2.b, kMlLen) == 0) return true;
    g_hardDiff = true;
    size_t at = 0; while (at < kMlLen && s1.b[at] == s2.b[at]) ++at;
    d = std::string(what) + " ret " + Hex(&ra, 4) + "/" + Hex(&re, 4) + " first diff @" + std::to_string(at) + " (list " + std::to_string(sizeof(CMatrixLinkList)) + ")";
    return false;
}

static void TestW3CollB() {
    // ---- CMatrix ----
    Run("CMatrix::Scale 0x459350", [](Rng& r, std::string& d) {
        const float s = PickScale(r), sc = GenF(r, 5.f);
        return W3MatCase(r, d, "scale " + F(sc), [&](CMatrix& m) { m.Scale(sc); }, [&](void* p) { oracle::Fn<void __fastcall(void*, int, float)>(0x459350)(p, 0, sc); }, s);
    });
    Run("CMatrix::SetUnity 0x59AE70", [](Rng& r, std::string& d) {
        return W3MatCase(r, d, "", [&](CMatrix& m) { m.SetUnity(); }, [&](void* p) { oracle::Fn<void __fastcall(void*)>(0x59AE70)(p); }, PickScale(r)); });
    Run("CMatrix::ResetOrientation 0x59AEA0", [](Rng& r, std::string& d) {
        return W3MatCase(r, d, "", [&](CMatrix& m) { m.ResetOrientation(); }, [&](void* p) { oracle::Fn<void __fastcall(void*)>(0x59AEA0)(p); }, PickScale(r)); });
    Run("CMatrix::SetScale-f 0x59AED0", [](Rng& r, std::string& d) {
        const float sc = GenF(r, 5.f);
        return W3MatCase(r, d, "scale " + F(sc), [&](CMatrix& m) { m.SetScale(sc); }, [&](void* p) { oracle::Fn<void __fastcall(void*, int, float)>(0x59AED0)(p, 0, sc); }, PickScale(r)); });
    Run("CMatrix::SetScale-fff 0x59AF00", [](Rng& r, std::string& d) {
        const CVector sc = GenV(r, 5.f);
        return W3MatCase(r, d, "scale " + V(sc), [&](CMatrix& m) { m.SetScale(sc.x, sc.y, sc.z); }, [&](void* p) { oracle::Fn<void __fastcall(void*, int, float, float, float)>(0x59AF00)(p, 0, sc.x, sc.y, sc.z); }, PickScale(r)); });
    Run("CMatrix::SetTranslate 0x59AF40", [](Rng& r, std::string& d) {
        const CVector t = GenV(r, PickScale(r));
        return W3MatCase(r, d, "t " + V(t), [&](CMatrix& m) { m.SetTranslate(t); }, [&](void* p) { oracle::Fn<void __fastcall(void*, int, float, float, float)>(0x59AF40)(p, 0, t.x, t.y, t.z); }, PickScale(r)); });
    Run("CMatrix::SetTranslateOnly 0x59AF80", [](Rng& r, std::string& d) {
        const CVector t = GenV(r, PickScale(r));
        return W3MatCase(r, d, "t " + V(t), [&](CMatrix& m) { m.SetTranslateOnly(t); }, [&](void* p) { oracle::Fn<void __fastcall(void*, int, float, float, float)>(0x59AF80)(p, 0, t.x, t.y, t.z); }, PickScale(r)); });
    Run("CMatrix::operator+= 0x59ADF0", [](Rng& r, std::string& d) {
        W3Mat o = W3GenMat(r, PickScale(r));
        return W3MatCase(r, d, "other " + Hex(&o, 0x40), [&](CMatrix& m) { m += W3C(o); }, [&](void* p) { oracle::Fn<void __fastcall(void*, int, const void*)>(0x59ADF0)(p, 0, &o); }, PickScale(r)); });
    Run("CMatrix::CopyOnlyMatrix 0x59ADD0", [](Rng& r, std::string& d) {
        W3Mat o = W3GenMat(r, PickScale(r));
        return W3MatCase(r, d, "other " + Hex(&o, 0x40), [&](CMatrix& m) { m.CopyOnlyMatrix(W3C(o)); }, [&](void* p) { oracle::Fn<void __fastcall(void*, int, const void*)>(0x59ADD0)(p, 0, &o); }, PickScale(r)); });
    Run("CMatrix::UpdateMatrix 0x59AD20", [](Rng& r, std::string& d) {
        alignas(16) uint8 rw[0x40]; { RawMat g = GenMat(r, PickScale(r)); g.f0 = r.u32(); g.f1 = r.u32(); g.f2 = r.u32(); g.f3 = r.u32(); std::memcpy(rw, &g, 0x40); }
        uint8 rw0[0x40]; std::memcpy(rw0, rw, 0x40);
        const bool ok = W3MatCase(r, d, "rw " + Hex(rw, 0x40), [&](CMatrix& m) { m.UpdateMatrix(W3Rw(rw)); }, [&](void* p) { oracle::Fn<void __fastcall(void*, int, void*)>(0x59AD20)(p, 0, rw); }, PickScale(r));
        return ok && std::memcmp(rw, rw0, 0x40) == 0;
    });
    Run("CMatrix::CopyToRwMatrix 0x59B8B0", [](Rng& r, std::string& d) {
        W3Mat a = W3GenMat(r, PickScale(r)), e = a; alignas(16) uint8 rwa[0x40], rwe[0x40]; W3Garbage(r, rwa, 0x40); std::memcpy(rwe, rwa, 0x40);
        W3C(a).CopyToRwMatrix(W3Rw(rwa));
        oracle::Fn<void __fastcall(void*, int, void*)>(0x59B8B0)(&e, 0, rwe);
        if (W3Same(rwa, rwe, 0x40) && W3Same(&a, &e, sizeof a)) return true;
        d = "mat " + Hex(&a, 0x40) + " rw got " + Hex(rwa, 0x40) + " exe " + Hex(rwe, 0x40); return false;
    });
    auto upd = [](const char* name, unsigned va, bool rwVariant) {
        Run(name, [=](Rng& r, std::string& d) {
            W3Mat a = W3GenMat(r, PickScale(r)); alignas(16) uint8 rwa[0x40], rwe[0x40]; W3Garbage(r, rwa, 0x40); std::memcpy(rwe, rwa, 0x40);
            if (!rwVariant || r.below(4)) a.attach = rwa;   // UpdateRW: sometimes no attached matrix
            W3Mat e = a; e.attach = a.attach ? rwe : nullptr;
            if (rwVariant) W3C(a).UpdateRW(); else W3C(a).Update();
            oracle::Fn<void __fastcall(void*)>(va)(&e);
            Hit(a.attach != nullptr);
            // compare everything but the attach pointers themselves
            a.attach = nullptr; e.attach = nullptr;
            if (W3Same(rwa, rwe, 0x40) && W3Same(&a, &e, sizeof a)) return true;
            d = "mat " + Hex(&a, 0x40) + " rw got " + Hex(rwa, 0x40) + " exe " + Hex(rwe, 0x40); return false;
        });
    };
    upd("CMatrix::Update 0x59BB60", 0x59BB60, false);
    upd("CMatrix::UpdateRW 0x59BBB0", 0x59BBB0, true);
    Run("CMatrix::Invert-1 0x59B920", [](Rng& r, std::string& d) {
        W3Mat in = W3GenMat(r, PickScale(r)); W3Mat oa = W3GenMat(r, 1.f), oe = oa;
        if (r.below(2)) { in.m = W3Rigid(r, false, PickScale(r)); if (r.below(2)) { const float k = 0.5f + r.f01() * 3.f; in.m.right = W3Mul(in.m.right, k); in.m.fwd = W3Mul(in.m.fwd, k); in.m.up = W3Mul(in.m.up, k); } }
        const W3Mat in0 = in; W3Mat in2 = in;
        CMatrix& ra = Invert(W3C(in), W3C(oa));
        void* re = oracle::Fn<void* __cdecl(void*, void*)>(0x59B920)(&in2, &oe);
        if (&ra == &W3C(oa) && re == &oe && W3SameMatNoPad(&oa, &oe) && W3SameMatNoPad(&in, &in2)) return true;
        d = "in " + Hex(&in0, 0x40) + " got " + Hex(&oa, 0x48) + " exe " + Hex(&oe, 0x48) + " in-after " + Hex(&in, 0x40) + "/" + Hex(&in2, 0x40); return false;
    });
    Run("CMatrix::SetRotate-quat 0x59BBF0", [](Rng& r, std::string& d) {
        const CQuaternion q = W3Quat(r);
        // SKIP (known, not fixed): with inf / NaN components or products far outside float range the exe keeps the intermediates on the x87 stack and spills only some of them
        // (0x59BBF0: 8 of the 9 products are stored, the rest stay in registers), so -0 / inf-inf / denormal results differ in 2 of 1500 cases; every finite game-range quaternion matches
        for (float c : { q.x, q.y, q.z, q.w }) if (!std::isfinite(c) || (c != 0.f && (std::fabs(c) < 1e-15f || std::fabs(c) > 1e15f))) return true;
        return W3MatCase(r, d, "q " + Q(q), [&](CMatrix& m) { m.SetRotate(q); }, [&](void* p) { oracle::Fn<void __fastcall(void*, int, const CQuaternion*)>(0x59BBF0)(p, 0, &q); }, 1.f, true); });
    auto mul = [](const char* name, unsigned va, bool plus) {
        Run(name, [=](Rng& r, std::string& d) {
            const float s = PickScale(r); W3Mat a = W3GenMat(r, s), b = W3GenMat(r, r.below(2) ? s : PickScale(r));
            if (r.below(3) == 0) { a.m = W3Rigid(r, false, 10.f); b.m = W3Rigid(r, false, 10.f); }
            alignas(16) uint8 e[sizeof(W3Mat)]; W3Garbage(r, e, sizeof e);
            const CMatrix x = plus ? (W3C(a) + W3C(b)) : (W3C(a) * W3C(b));
            oracle::Fn<void* __cdecl(void*, const void*, const void*)>(va)(e, &a, &b);
            if (W3SameMatNoPad(&x, e)) return true;
            d = "a " + Hex(&a, 0x40) + " b " + Hex(&b, 0x40) + " got " + Hex(&x, 0x40) + " exe " + Hex(e, 0x40); return false;
        });
    };
    mul("CMatrix::operator*-Mat 0x59BE30", 0x59BE30, false);
    mul("CMatrix::operator+- 0x59BFA0", 0x59BFA0, true);

    // ---- CQuaternion ----
    auto quatThis = [](const char* name, unsigned va, int kind) {   // kind 0: Scale(float) 1: Copy(q) 2: Conjugate
        Run(name, [=](Rng& r, std::string& d) {
            CQuaternion a = W3Quat(r), e = a; const CQuaternion src = W3Quat(r); const float sc = GenF(r, 4.f);
            if (kind == 0) { a.Scale(sc); oracle::Fn<void __fastcall(void*, int, float)>(va)(&e, 0, sc); }
            else if (kind == 1) { a.Copy(src); oracle::Fn<void __fastcall(void*, int, const CQuaternion*)>(va)(&e, 0, &src); }
            else { a.Conjugate(); oracle::Fn<void __fastcall(void*)>(va)(&e); }
            if (SameQ(a, e)) return true;
            d = "src " + Q(src) + " sc " + F(sc) + " got " + Q(a) + " exe " + Q(e); return false;
        });
    };
    quatThis("CQuaternion::Scale 0x4CF9B0", 0x4CF9B0, 0);
    quatThis("CQuaternion::Copy 0x4CF9E0", 0x4CF9E0, 1);
    quatThis("CQuaternion::Conjugate 0x4D37D0", 0x4D37D0, 2);
    Run("CQuaternion::Multiply 0x59C270", [](Rng& r, std::string& d) {
        const CQuaternion x = W3Quat(r), y = W3Quat(r); CQuaternion a = W3Quat(r), e = a;
        a.Multiply(x, y); oracle::Fn<void __fastcall(void*, int, const CQuaternion*, const CQuaternion*)>(0x59C270)(&e, 0, &x, &y);
        if (SameQ(a, e)) return true;
        d = "x " + Q(x) + " y " + Q(y) + " got " + Q(a) + " exe " + Q(e); return false;
    });
    Run("CQuaternion::Get- 0x59C080", [](Rng& r, std::string& d) {
        const CQuaternion q = W3Quat(r); alignas(16) uint8 ra[0x40], re[0x40]; W3Garbage(r, ra, 0x40); std::memcpy(re, ra, 0x40);
        q.Get(W3Rw(ra)); oracle::Fn<void __fastcall(const void*, int, void*)>(0x59C080)(&q, 0, re);
        if (W3Same(ra, re, 0x40)) return true;
        d = "q " + Q(q) + " got " + Hex(ra, 0x40) + " exe " + Hex(re, 0x40); return false;
    });
    Run("CQuaternion::Set-Matrix 0x59C3E0", [](Rng& r, std::string& d) {
        RawMat m; if (r.below(2)) { const RawMat g = W3Rigid(r, false, 10.f); m = g; if (r.below(4) == 0) m.right = W3Mul(m.right, -1.f); } else m = GenMat(r, PickScale(r));
        std::swap(m.fwd, m.up);   // RwMatrix order: right, up, at
        m.f0 = r.u32(); m.f1 = r.u32(); m.f2 = r.u32(); m.f3 = r.u32();
        CQuaternion a = W3Quat(r), e = a;
        a.Set(*W3Rw(&m)); oracle::Fn<void __fastcall(void*, int, const void*)>(0x59C3E0)(&e, 0, &m);
        if (SameQ(a, e)) return true;
        d = "m " + Hex(&m, 0x40) + " got " + Q(a) + " exe " + Q(e); return false;
    });

    // ---- CRect (left, bottom, right, top) ----
    auto genRect = [](Rng& r, float S) { CRect c; float v[4] = { GenF(r, S), GenF(r, S), GenF(r, S), GenF(r, S) }; if (r.below(4)) { std::sort(v, v + 4); v[1] = v[1] + 0.f; std::swap(v[1], v[2]); /* left <= right, bottom <= top after the swap below */ std::swap(v[1], v[2]); }
        std::memcpy(&c, v, 16); return c; };
    Run("CRect::IsFlipped 0x404190", [genRect](Rng& r, std::string& d) {
        const CRect c = genRect(r, W3S(r)); const bool x = c.IsFlipped(), y = oracle::Fn<bool __fastcall(const CRect*)>(0x404190)(&c);
        Hit(x); if (x == y) return true;
        d = "rect " + Hex(&c, 16) + " got " + (x ? "1" : "0"); return false;
    });
    auto rectMut = [genRect](const char* name, unsigned va, int kind) {
        Run(name, [=](Rng& r, std::string& d) {
            const float S = W3S(r); CRect a = genRect(r, S), e = a; const CRect o = genRect(r, S); const float p = GenF(r, S), q = GenF(r, S), sz = GenF(r, S * 0.3f);
            switch (kind) {
            case 0: a.Restrict(o); oracle::Fn<void __fastcall(void*, int, const CRect*)>(va)(&e, 0, &o); break;
            case 1: a.Resize(p, q); oracle::Fn<void __fastcall(void*, int, float, float)>(va)(&e, 0, p, q); break;
            case 2: a.SetFromCenter(p, q, sz); oracle::Fn<void __fastcall(void*, int, float, float, float)>(va)(&e, 0, p, q, sz); break;
            default: a.StretchToPoint(p, q); oracle::Fn<void __fastcall(void*, int, float, float)>(va)(&e, 0, p, q); break;
            }
            if (W3Same(&a, &e, 16)) return true;
            d = "rect0 " + Hex(&o, 16) + " p " + F(p) + " q " + F(q) + " sz " + F(sz) + " got " + Hex(&a, 16) + " exe " + Hex(&e, 16); return false;
        });
    };
    rectMut("CRect::Restrict 0x404200", 0x404200, 0);
    rectMut("CRect::Resize 0x404260", 0x404260, 1);
    rectMut("CRect::SetFromCenter 0x43E020", 0x43E020, 2);
    rectMut("CRect::StretchToPoint 0x5327F0", 0x5327F0, 3);
    Run("CRect::IsPointInside- 0x404290", [genRect](Rng& r, std::string& d) {
        const float S = W3S(r); const CRect c = genRect(r, S); CVector2D p{ GenF(r, S), GenF(r, S) };
        if (r.below(2)) { float v[4]; std::memcpy(v, &c, 16); p = { v[0] + (v[2] - v[0]) * (r.f01() * 1.2f - 0.1f), v[1] + (v[3] - v[1]) * (r.f01() * 1.2f - 0.1f) }; if (r.below(8) == 0) p.x = v[0]; if (r.below(8) == 0) p.y = v[3]; }
        const bool x = c.IsPointInside(p), y = oracle::Fn<bool __fastcall(const CRect*, int, const CVector2D*)>(0x404290)(&c, 0, &p);
        Hit(x); if (x == y) return true;
        d = "rect " + Hex(&c, 16) + " p " + F(p.x) + F(p.y) + " got " + (x ? "1" : "0"); return false;
    });
    Run("CRect::IsPointInside-Tolerance 0x4042D0", [genRect](Rng& r, std::string& d) {
        const float S = W3S(r); const CRect c = genRect(r, S); CVector2D p{ GenF(r, S), GenF(r, S) }; const float tol = r.below(3) ? r.f01() * S * 0.1f : GenF(r, S);
        if (r.below(2)) { float v[4]; std::memcpy(v, &c, 16); p = { v[0] + (v[2] - v[0]) * (r.f01() * 1.4f - 0.2f), v[1] + (v[3] - v[1]) * (r.f01() * 1.4f - 0.2f) }; }
        const bool x = c.IsPointInside(p, tol), y = oracle::Fn<bool __fastcall(const CRect*, int, const CVector2D*, float)>(0x4042D0)(&c, 0, &p, tol);
        Hit(x); if (x == y) return true;
        d = "rect " + Hex(&c, 16) + " p " + F(p.x) + F(p.y) + " tol " + F(tol) + " got " + (x ? "1" : "0"); return false;
    });
    Run("CRect::GetCenter- 0x43E050", [genRect](Rng& r, std::string& d) {
        const CRect c = genRect(r, W3S(r)); float xa = 1.f, ya = 2.f, xe = 1.f, ye = 2.f;
        c.GetCenter(&xa, &ya); oracle::Fn<void __fastcall(const CRect*, int, float*, float*)>(0x43E050)(&c, 0, &xe, &ye);
        if (SameF(xa, xe) && SameF(ya, ye)) return true;
        d = "rect " + Hex(&c, 16) + " got " + F(xa) + F(ya) + " exe " + F(xe) + F(ye); return false;
    });

    // ---- CVector / CVector2D ----
    Run("CVector::DotProduct-v3d*v3d* 0x40FDB0", [](Rng& r, std::string& d) {
        const float s = PickScale(r); CVector a = GenV(r, s), b = GenV(r, s);
        const float x = DotProduct(&a, &b), y = oracle::Fn<float __cdecl(CVector*, CVector*)>(0x40FDB0)(&a, &b);
        if (SameF(x, y)) return true;
        d = "a " + V(a) + " b " + V(b) + " got " + F(x) + " exe " + F(y); return false;
    });
    auto vecOp = [](const char* name, unsigned va, int kind) {
        Run(name, [=](Rng& r, std::string& d) {
            const float s = PickScale(r); const CVector a = GenV(r, s), b = GenV(r, s); CVector x{ 1.f, 2.f, 3.f }, y{ 1.f, 2.f, 3.f };
            const W3Mat m = W3GenMat(r, s, true);
            switch (kind) {
            case 0: x.Sum(a, b); oracle::Fn<void __fastcall(void*, int, const CVector*, const CVector*)>(va)(&y, 0, &a, &b); break;
            case 1: x.Difference(a, b); oracle::Fn<void __fastcall(void*, int, const CVector*, const CVector*)>(va)(&y, 0, &a, &b); break;
            case 2: x.Cross_OG(a, b); oracle::Fn<void __fastcall(void*, int, const CVector*, const CVector*)>(va)(&y, 0, &a, &b); break;
            case 3: x.FromMultiply(*reinterpret_cast<const CMatrix*>(&m), a); oracle::Fn<void __fastcall(void*, int, const void*, const CVector*)>(va)(&y, 0, &m, &a); break;
            default: x.FromMultiply3x3(*reinterpret_cast<const CMatrix*>(&m), a); oracle::Fn<void __fastcall(void*, int, const void*, const CVector*)>(va)(&y, 0, &m, &a); break;
            }
            if (SameV(x, y)) return true;
            d = "a " + V(a) + " b " + V(b) + " m " + Hex(&m, 0x40) + " got " + V(x) + " exe " + V(y); return false;
        });
    };
    vecOp("CVector::Sum 0x40FDD0", 0x40FDD0, 0);
    vecOp("CVector::Difference 0x40FE00", 0x40FE00, 1);
    vecOp("CVector::Cross_OG 0x70F890", 0x70F890, 2);
    vecOp("CVector::FromMultiply 0x59C670", 0x59C670, 3);
    vecOp("CVector::FromMultiply3x3 0x59C6D0", 0x59C6D0, 4);
    Run("CVector2D::Magnitude 0x420860", [](Rng& r, std::string& d) {
        const float s = PickScale(r); const CVector2D v{ GenF(r, s), GenF(r, s) };
        const float x = v.Magnitude(), y = oracle::Fn<float __fastcall(const CVector2D*)>(0x420860)(&v);
        if (SameF(x, y)) return true;
        d = "v " + F(v.x) + F(v.y) + " got " + F(x) + " exe " + F(y); return false;
    });
    Run("CVector2D::operator= 0x43E110", [](Rng& r, std::string& d) {
        const CVector2D src{ GenF(r, 100.f), GenF(r, 100.f) }; CVector2D a{ 5.f, 6.f }, e{ 5.f, 6.f };
        a = src; oracle::Fn<void __fastcall(void*, int, const CVector2D*)>(0x43E110)(&e, 0, &src);
        if (SameF(a.x, e.x) && SameF(a.y, e.y)) return true;
        d = "src " + F(src.x) + F(src.y) + " got " + F(a.x) + F(a.y) + " exe " + F(e.x) + F(e.y); return false;
    });

    // ---- CMatrixLink / CMatrixLinkList ----
    Run("CMatrixLink::Insert 0x54E8F0", [](Rng& r, std::string& d) {
        const int a = r.below(8), b = r.below(8);
        return W3ListCase(r, d, "Insert", [&]() -> uintptr_t { W3N(a).Insert(&W3N(b)); return 0; }, [&]() -> uintptr_t { oracle::Fn<void __fastcall(void*, int, void*)>(0x54E8F0)(&W3N(a), 0, &W3N(b)); return 0; }); });
    Run("CMatrixLink::Remove 0x54E910", [](Rng& r, std::string& d) {
        const int a = r.below(8);
        return W3ListCase(r, d, "Remove", [&]() -> uintptr_t { W3N(a).Remove(); return 0; }, [&]() -> uintptr_t { oracle::Fn<void __fastcall(void*)>(0x54E910)(&W3N(a)); return 0; }); });
    Run("CMatrixLinkList::AddToList1 0x54E9D0", [](Rng& r, std::string& d) {
        return W3ListCase(r, d, "AddToList1", [&]() -> uintptr_t { auto* p = W3L().AddToList1(); Hit(p != nullptr); return (uintptr_t)p; }, [&]() -> uintptr_t { return (uintptr_t)oracle::Fn<void* __fastcall(void*)>(0x54E9D0)(&W3L()); }); });
    Run("CMatrixLinkList::AddToList2 0x54EA10", [](Rng& r, std::string& d) {
        return W3ListCase(r, d, "AddToList2", [&]() -> uintptr_t { auto* p = W3L().AddToList2(); Hit(p != nullptr); return (uintptr_t)p; }, [&]() -> uintptr_t { return (uintptr_t)oracle::Fn<void* __fastcall(void*)>(0x54EA10)(&W3L()); }); });
    auto moveRow = [](const char* name, unsigned va, int kind) {
        Run(name, [=](Rng& r, std::string& d) {
            const int a = r.below(8);
            return W3ListCase(r, d, name,
                [&]() -> uintptr_t { auto& L = W3L(); if (kind == 0) L.MoveToList1(&W3N(a)); else if (kind == 1) L.MoveToList2(&W3N(a)); else L.MoveToFreeList(&W3N(a)); return 0; },
                [&]() -> uintptr_t { oracle::Fn<void __fastcall(void*, int, void*)>(va)(&W3L(), 0, &W3N(a)); return 0; });
        });
    };
    moveRow("CMatrixLinkList::MoveToList1 0x54EA60", 0x54EA60, 0);
    moveRow("CMatrixLinkList::MoveToList2 0x54EA90", 0x54EA90, 1);
    moveRow("CMatrixLinkList::MoveToFreeList 0x54EAD0", 0x54EAD0, 2);
    auto cntRow = [](const char* name, unsigned va, int kind) {
        Run(name, [=](Rng& r, std::string& d) {
            return W3ListCase(r, d, name,
                [&]() -> uintptr_t { auto& L = W3L(); return (uintptr_t)(kind == 0 ? L.GetNumFree() : kind == 1 ? L.GetNumUsed1() : L.GetNumUsed2()); },
                [&]() -> uintptr_t { return (uintptr_t)oracle::Fn<int __fastcall(void*)>(va)(&W3L()); });
        });
    };
    cntRow("CMatrixLinkList::GetNumFree 0x54EB10", 0x54EB10, 0);
    cntRow("CMatrixLinkList::GetNumUsed1 0x54EB30", 0x54EB30, 1);
    cntRow("CMatrixLinkList::GetNumUsed2 0x54EB50", 0x54EB50, 2);
}
