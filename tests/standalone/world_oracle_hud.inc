// (b) radar / HUD / sprites / fonts / lights, see world_oracle_test.cpp
#include "Radar.h"
#include "MenuManager.h"
#include "Camera.h"
#include "Cam.h"
#include "HudColours.h"
#include "Entity/Entity.h"
#include "Sprite.h"
#include "Sprite2d.h"
#include "Draw.h"
#include "Font.h"
#include "Messages.h"
#include "PointLights.h"
#include "Skidmarks.h"
#include "Coronas.h"
#include "Clouds.h"
#include "Weather.h"
#include "Replay.h"
#include "Timer.h"
#include "Scene.h"
#include "Scripts/TheScripts.h"

// run `port` and `exe` from the same pristine state of the given global ranges, compare the ranges afterwards (bit-exact); leaves the exe's result in the globals
struct Rg { uintptr_t va; size_t n; };
template<class P, class E>
static bool Both(std::string& d, std::initializer_list<Rg> rgs, P&& port, E&& exe) {
    std::vector<Snap> pre, a, b;
    for (auto& g : rgs) { pre.emplace_back(g.va, g.n); a.emplace_back(g.va, g.n); b.emplace_back(g.va, g.n); }
    for (auto& s : pre) s.Save();
    port();
    for (auto& s : a) s.Save();
    for (auto& s : pre) s.Restore();
    exe();
    for (auto& s : b) s.Save();
    bool ok = true;
    for (size_t i = 0; i < a.size(); ++i) if (!a[i].SameAs(b[i])) {
        ok = false; int shown = 0;
        for (size_t o = 0; o + 4 <= a[i].b.size() && shown < 6; o += 4) {   // only the first differing dwords
            uint32_t x, y; std::memcpy(&x, &a[i].b[o], 4); std::memcpy(&y, &b[i].b[o], 4);
            if (x != y) { d += "@" + HexVA((unsigned)(a[i].va + o)) + " got " + F(BitsF(x)) + " exe " + F(BitsF(y)) + "; "; ++shown; }
        }
    }
    return ok;
}

// the camera state CRadar::CalculateCachedSinCos reads, restored by the destructor
struct CamFix {
    Snap cam{ 0xB6F028, sizeof(CCamera) };
    Raw<CEntity> ent; Raw<CMatrix> entMat, camMat;
    CamFix(Rng& r) {
        cam.Save();
        const auto gm = [&](Raw<CMatrix>& m) { for (int i = 0; i < 16; ++i) reinterpret_cast<float*>(m.b)[i] = GenF(r, r.below(2) ? 1.f : 1000.f); };
        gm(entMat); gm(camMat);
        ent.Fill(r); ent->m_matrix = reinterpret_cast<CMatrixLink*>(entMat.get());
        TheCamera.m_nActiveCam = (uint8)r.below(2);
        auto& c = TheCamera.m_aCams[TheCamera.m_nActiveCam];
        static const int modes[] = { 4, 16, 18, 22, 4, 16, 1, 3, 7, 16, 22 };
        c.m_nMode = (eCamMode)modes[r.below(11)];
        c.m_nDirectionWasLooking = r.below(4);
        c.m_pCamTargetEntity = ent.get();
        c.m_vecSourceBeforeLookBehind = GenV(r, 1000.f);
        TheCamera.m_matrix = r.below(4) ? reinterpret_cast<CMatrixLink*>(camMat.get()) : nullptr;
        TheCamera.m_placement.m_fHeading = GenF(r, 7.f);
    }
    ~CamFix() { cam.Restore(); }
};

static void SetRes(Rng& r) { static const int w[] = { 640, 800, 1024, 1280, 1920 }, h[] = { 448, 600, 768, 720, 1080 }; RsGlobal.maximumWidth = r.below(3) ? w[r.below(5)] : 1 + (int)r.below(5000); RsGlobal.maximumHeight = r.below(3) ? h[r.below(5)] : 1 + (int)r.below(3000); }
static CVector2D Pt(Rng& r, float sc) { return CVector2D{ GenF(r, sc), GenF(r, sc) }; }
static std::string P2S(const CVector2D& p) { return "{" + F(p.x) + "," + F(p.y) + "}"; }
static void TestHudSprites();
static void TestHudFont();
static void TestHudLights();
static void TestHudSkid();
static void TestHudCorona();
static void TestHudClouds();
static void TestHud() {
    TestHudSprites();
    TestHudFont();
    TestHudLights();
    TestHudSkid();
    TestHudCorona();
    TestHudClouds();
    const auto RadGlobals = [](Rng& r) {   // radar statics: range, origin, cached sin / cos
        CRadar::m_radarRange = r.below(8) ? 350.f + r.f01() * 2640.f : GenF(r, 4000.f);
        CRadar::vec2DRadarOrigin = { GenF(r, 3000.f), GenF(r, 3000.f) };
        const float a = r.f01() * 6.2831853f;
        CRadar::cachedSin = r.below(8) ? std::sin(a) : GenF(r, 2.f); CRadar::cachedCos = r.below(8) ? std::cos(a) : GenF(r, 2.f);
    };

    Run("CRadar::CalculateCachedSinCos 0x583670", [&](Rng& r, std::string& d) {
        CamFix cf(r);
        FrontEndMenuManager.m_bDrawingMap = r.below(5) == 0;
        return Both(d, { {0xBA8308, 12} }, [] { CRadar::CalculateCachedSinCos(); }, [] { oracle::Fn<void __cdecl()>(0x583670)(); });
    });
    Run("CRadar::LimitRadarPoint 0x5832F0", [&](Rng& r, std::string& d) {
        FrontEndMenuManager.m_bDrawingMap = r.below(4) == 0;
        CVector2D p0 = Pt(r, PickScale(r)), p1 = p0, p2 = p0;
        float a = 0, e = 0;
        const bool ok = Both(d, {}, [&] { a = CRadar::LimitRadarPoint(p1); }, [&] { e = oracle::Fn<float __cdecl(CVector2D*)>(0x5832F0)(&p2); });
        if (ok && SameF(a, e) & SameF(p1.x, p2.x) & SameF(p1.y, p2.y)) return true;
        d += "in " + P2S(p0) + " got " + F(a) + P2S(p1) + " exe " + F(e) + P2S(p2); return false;
    });
    Run("CRadar::LimitToMap 0x583350", [&](Rng& r, std::string& d) {
        SetRes(r);
        FrontEndMenuManager.m_bMapLoaded = r.below(2) != 0;
        FrontEndMenuManager.m_fMapZoom   = std::fabs(GenF(r, r.below(2) ? 400.f : 32000.f, S_NAN | S_ZERO | S_TINY));
        FrontEndMenuManager.m_vMapOrigin = { GenF(r, 400.f, S_NAN | S_ZERO | S_TINY), GenF(r, 400.f, S_NAN | S_ZERO | S_TINY) };
        float x1 = GenF(r, 1500.f), y1 = GenF(r, 1500.f), x2 = x1, y2 = y1;
        CRadar::LimitToMap(x1, y1);
        oracle::Fn<void __cdecl(float*, float*)>(0x583350)(&x2, &y2);
        if (SameF(x1, x2) & SameF(y1, y2)) return true;
        d = "in " + F(x2) + "," + F(y2) + " got " + F(x1) + "," + F(y1); return false;
    });
    Run("CRadar::CalculateBlipAlpha 0x583420", [&](Rng& r, std::string& d) {
        FrontEndMenuManager.m_bDrawingMap = r.below(8) == 0;
        const float dist = r.below(3) ? r.f01() * (r.below(2) ? 60.f : 3000.f) : GenF(r, PickScale(r));
        const uint8 a = CRadar::CalculateBlipAlpha(dist), b = oracle::Fn<uint8 __cdecl(float)>(0x583420)(dist);
        if (a == b) return true;
        d = "dist " + F(dist) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CRadar::TransformRadarPointToScreenSpace 0x583480", [&](Rng& r, std::string& d) {
        SetRes(r);
        FrontEndMenuManager.m_bDrawingMap = r.below(4) == 0;
        FrontEndMenuManager.m_fMapZoom    = GenF(r, 400.f);
        FrontEndMenuManager.m_vMapOrigin  = { GenF(r, 400.f), GenF(r, 400.f) };
        const CVector2D in = Pt(r, r.below(2) ? 1.5f : PickScale(r));
        const CVector2D a = CRadar::TransformRadarPointToScreenSpace(in);
        CVector2D b{ BitsF(0xDEADBEEF), BitsF(0xDEADBEEF) };
        oracle::Fn<void __cdecl(CVector2D*, const CVector2D*)>(0x583480)(&b, &in);
        if (SameF(a.x, b.x) & SameF(a.y, b.y)) return true;
        d = "in " + P2S(in) + " got " + P2S(a) + " exe " + P2S(b); return false;
    });
    Run("CRadar::TransformRealWorldPointToRadarSpace 0x583530", [&](Rng& r, std::string& d) {
        RadGlobals(r);
        const CVector2D in = Pt(r, r.below(2) ? 3000.f : PickScale(r));
        const CVector2D a = CRadar::TransformRealWorldPointToRadarSpace(in);
        CVector2D b{ BitsF(0xDEADBEEF), BitsF(0xDEADBEEF) };
        oracle::Fn<void __cdecl(CVector2D*, const CVector2D*)>(0x583530)(&b, &in);
        if (SameF(a.x, b.x) & SameF(a.y, b.y)) return true;
        d = "in " + P2S(in) + " range " + F(CRadar::m_radarRange) + " org " + P2S(CRadar::vec2DRadarOrigin) + " sc " + F(CRadar::cachedSin) + "," + F(CRadar::cachedCos) + " got " + P2S(a) + " exe " + P2S(b); return false;
    });
    Run("CRadar::TransformRadarPointToRealWorldSpace 0x5835A0", [&](Rng& r, std::string& d) {
        RadGlobals(r);
        const CVector2D in = Pt(r, r.below(2) ? 1.5f : PickScale(r));
        const CVector2D a = CRadar::TransformRadarPointToRealWorldSpace(in);
        CVector2D b{ BitsF(0xDEADBEEF), BitsF(0xDEADBEEF) };
        oracle::Fn<void __cdecl(CVector2D*, const CVector2D*)>(0x5835A0)(&b, &in);
        if (SameF(a.x, b.x) & SameF(a.y, b.y)) return true;
        d = "in " + P2S(in) + " range " + F(CRadar::m_radarRange) + " org " + P2S(CRadar::vec2DRadarOrigin) + " sc " + F(CRadar::cachedSin) + "," + F(CRadar::cachedCos) + " got " + P2S(a) + " exe " + P2S(b); return false;
    });
    Run("CRadar::SetupRadarRect 0x584A80", [&](Rng& r, std::string& d) {
        const int x = r.below(3) ? (int)r.below(14) - 1 : (int)r.u32(), y = r.below(3) ? (int)r.below(14) - 1 : (int)r.u32();
        return Both(d, { {0x8D0920, 16} }, [&] { CRadar::SetupRadarRect(x, y); }, [&] { oracle::Fn<void __cdecl(int, int)>(0x584A80)(x, y); });
    });
    Run("ClipRadarTileCoords 0x584B00", [&](Rng& r, std::string& d) {
        int x = r.below(3) ? (int)r.below(18) - 3 : (int)r.u32(), y = r.below(3) ? (int)r.below(18) - 3 : (int)r.u32(), x2 = x, y2 = y;
        const bool a = ClipRadarTileCoords(x, y), b = oracle::Fn<bool __cdecl(int*, int*)>(0x584B00)(&x2, &y2);
        if (a == b && x == x2 && y == y2) return true;
        d = "got " + std::to_string(a) + " " + std::to_string(x) + "," + std::to_string(y) + " exe " + std::to_string(b) + " " + std::to_string(x2) + "," + std::to_string(y2); return false;
    });
    Run("IsPointInsideRadar 0x584D40", [&](Rng& r, std::string& d) {
        const CVector2D p = Pt(r, r.below(2) ? 1.2f : PickScale(r));
        const bool a = IsPointInsideRadar(p), b = oracle::Fn<bool __cdecl(const CVector2D*)>(0x584D40)(&p);
        Hit(b);
        if (a == b) return true;
        d = "p " + P2S(p) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("GetTextureCorners 0x584D90", [&](Rng& r, std::string& d) {
        const int x = r.below(3) ? (int)r.below(14) - 1 : (int)r.u32(), y = r.below(3) ? (int)r.below(14) - 1 : (int)r.u32();
        CVector2D a[4], b[4]; std::memset(a, 0xCD, sizeof a); std::memset(b, 0xCD, sizeof b);
        GetTextureCorners(x, y, a); oracle::Fn<void __cdecl(int, int, CVector2D*)>(0x584D90)(x, y, b);
        if (SameBlob(a, b, sizeof a)) return true;
        d = "x " + std::to_string(x) + " y " + std::to_string(y) + " got " + Hex(a, 32) + " exe " + Hex(b, 32); return false;
    });
    const auto GenSeg = [&](Rng& r, CVector2D& s, CVector2D& e) {
        const float sc = r.below(3) ? 2.f : PickScale(r);
        s = Pt(r, sc); e = Pt(r, sc);
        if (r.below(8) == 0) e.x = s.x; if (r.below(8) == 0) e.y = s.y;
        if (r.below(10) == 0) { s.x = r.below(2) ? 1.f : -1.f; } if (r.below(10) == 0) { e.y = r.below(2) ? 1.f : -1.f; }
    };
    Run("LineRadarBoxCollision 0x584E00", [&](Rng& r, std::string& d) {
        CVector2D s, e; GenSeg(r, s, e);
        CVector2D ra{ BitsF(0xDEADBEEF), BitsF(0xDEADBEEF) }, rb = ra;
        const int a = LineRadarBoxCollision(ra, s, e), b = oracle::Fn<int __cdecl(CVector2D*, const CVector2D*, const CVector2D*)>(0x584E00)(&rb, &s, &e);
        Hit(b != -1);
        if (a == b && SameF(ra.x, rb.x) & SameF(ra.y, rb.y)) return true;
        d = "s " + P2S(s) + " e " + P2S(e) + " got " + std::to_string(a) + P2S(ra) + " exe " + std::to_string(b) + P2S(rb); return false;
    });
    Run("CRadar::ClipRadarPoly 0x585040", [&](Rng& r, std::string& d) {
        CVector2D in[4];
        if (r.below(2)) {   // a rotated rectangle / quad around a centre (like the radar tile corners)
            const float cx = GenF(r, 3.f), cy = GenF(r, 3.f), w = r.f01() * 4.f + 0.01f, h = r.f01() * 4.f + 0.01f, an = r.f01() * 6.2831853f, sn = std::sin(an), cs = std::cos(an);
            const float sx[4] = { -1, 1, 1, -1 }, sy[4] = { -1, -1, 1, 1 };
            for (int i = 0; i < 4; ++i) in[i] = { cx + cs * sx[i] * w - sn * sy[i] * h, cy + sn * sx[i] * w + cs * sy[i] * h };
        } else for (auto& p : in) p = Pt(r, r.below(2) ? 3.f : PickScale(r));
        CVector2D oa[12], ob[12]; std::memset(oa, 0xCD, sizeof oa); std::memset(ob, 0xCD, sizeof ob);
        const int a = CRadar::ClipRadarPoly(oa, in), b = oracle::Fn<int __cdecl(CVector2D*, const CVector2D*)>(0x585040)(ob, in);
        Hit(b > 0);
        bool ok = a == b;
        if (ok && a >= 0 && a <= 12) {   // the exe reads stack slots as floats for its corners[-1] (UB, denormal garbage): port writes (0,0) there
            for (int i = 0; i < a * 2; ++i) { const uint32_t pa = FBits(((float*)oa)[i]), pb = FBits(((float*)ob)[i]); if (pa != pb && !(pa == 0 && pb < 0x00800000u)) ok &= SameBits(pa, pb); }
        }
        if (ok) return true;
        d = "in " + P2S(in[0]) + P2S(in[1]) + P2S(in[2]) + P2S(in[3]) + " got " + std::to_string(a) + " exe " + std::to_string(b);
        for (int i = 0; i < std::min(std::max(a, b), 8); ++i) d += " [" + P2S(oa[i]) + "|" + P2S(ob[i]) + "]";
        return false;
    });
    Run("CRadar::GetRadarTraceColour 0x584770", [&](Rng& r, std::string& d) {
        Snap hc(0xBAB22C, 0x3C); hc.Save(); hc.FillRandom(r); hc.Restore();
        const uint32 col = r.below(4) ? r.below(10) : r.u32();
        const bool br = r.below(2), fr = r.below(2);
        const uint32 a = CRadar::GetRadarTraceColour((eBlipColour)col, br, fr), b = oracle::Fn<uint32 __cdecl(uint32, bool, bool)>(0x584770)(col, br, fr);
        if (a == b) return true;
        d = "col " + std::to_string(col) + " got " + HexVA(a) + " exe " + HexVA(b); return false;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
// CSprite / CSprite2d vertex building
static CRGBA GenCol(Rng& r) { return CRGBA{ (uint8)r.u32(), (uint8)r.u32(), (uint8)r.u32(), (uint8)r.u32() }; }
static CRect GenRect(Rng& r) { const float sc = r.below(2) ? 700.f : PickScale(r); return CRect{ GenF(r, sc), GenF(r, sc), GenF(r, sc), GenF(r, sc) }; }
static std::string RectS(const CRect& c) { return "{" + F(c.left) + "," + F(c.bottom) + "," + F(c.right) + "," + F(c.top) + "}"; }

static void TestHudSprites() {
    // Sprite2d statics: 0xC80458 nextBufferIndex, +4 nextBufferVertex, +8 NearScreenZ, +C RecipNearClip, +10 maVertices[8] (28 bytes each)
    const Rg k2d{ 0xC80458, 0x10 + 8 * 28 };
    const auto Seed2d = [&](Rng& r) {
        Snap s(k2d.va, k2d.n); s.Save(); s.FillRandom(r);
        s.Restore();
        CSprite2d::NearScreenZ = GenF(r, r.below(2) ? 1.f : PickScale(r)); CSprite2d::RecipNearClip = GenF(r, r.below(2) ? 1.f : PickScale(r));
    };
    const auto Fill2d = [&](Rng& r) { Snap s(0xC80468, 8 * 28); s.Save(); s.FillRandom(r); s.Restore(); };
    const auto Vtx = [](unsigned i) { return std::to_string(i); };
    (void)Vtx;

    Run("CSprite2d::IsVertexBufferEmpty 0x727390", [&](Rng& r, std::string& d) {
        CSprite2d::nextBufferVertex = r.below(3) ? 0 : (int)r.u32();
        const bool a = CSprite2d::IsVertexBufferEmpty(), b = oracle::Fn<bool __cdecl()>(0x727390)();
        Hit(b); if (a == b) return true; d = "v " + std::to_string(CSprite2d::nextBufferVertex); return false;
    });
    Run("CSprite2d::IsVertexBufferFull 0x7273A0", [&](Rng& r, std::string& d) {
        CSprite2d::nextBufferVertex = r.below(2) ? 0x5FC + (int)r.below(5) - 2 : (int)r.u32() >> r.below(32);
        CSprite2d::nextBufferIndex  = r.below(2) ? 0xFFA + (int)r.below(5) - 2 : (int)r.u32() >> r.below(32);
        const bool a = CSprite2d::IsVertexBufferFull(), b = oracle::Fn<bool __cdecl()>(0x7273A0)();
        Hit(b); if (a == b) return true; d = "v " + std::to_string(CSprite2d::nextBufferVertex) + " i " + std::to_string(CSprite2d::nextBufferIndex); return false;
    });
    Run("CSprite2d::SetVertices(CRect,4 colors) 0x727420", [&](Rng& r, std::string& d) {
        Seed2d(r); const CRect rc = GenRect(r); const CRGBA c1 = GenCol(r), c2 = GenCol(r), c3 = GenCol(r), c4 = GenCol(r);
        if (Both(d, { k2d }, [&] { CSprite2d::SetVertices(rc, c1, c2, c3, c4); }, [&] { oracle::Fn<void __cdecl(const CRect*, const CRGBA*, const CRGBA*, const CRGBA*, const CRGBA*)>(0x727420)(&rc, &c1, &c2, &c3, &c4); })) return true;
        d += "rect " + RectS(rc); return false;
    });
    Run("CSprite2d::SetVertices(8 floats) 0x727590", [&](Rng& r, std::string& d) {
        Seed2d(r); float f[8]; for (auto& x : f) x = GenF(r, r.below(2) ? 700.f : PickScale(r)); const CRGBA c1 = GenCol(r), c2 = GenCol(r), c3 = GenCol(r), c4 = GenCol(r);
        return Both(d, { k2d }, [&] { CSprite2d::SetVertices(f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], c1, c2, c3, c4); },
                    [&] { oracle::Fn<void __cdecl(float, float, float, float, float, float, float, float, const CRGBA*, const CRGBA*, const CRGBA*, const CRGBA*)>(0x727590)(f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], &c1, &c2, &c3, &c4); });
    });
    Run("CSprite2d::SetVertices(CRect,4 colors,8 uv) 0x727710", [&](Rng& r, std::string& d) {
        Seed2d(r); float f[8]; for (auto& x : f) x = GenF(r, 2.f); const CRect rc = GenRect(r); const CRGBA c1 = GenCol(r), c2 = GenCol(r), c3 = GenCol(r), c4 = GenCol(r);
        return Both(d, { k2d }, [&] { CSprite2d::SetVertices(rc, c1, c2, c3, c4, f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7]); },
                    [&] { oracle::Fn<void __cdecl(const CRect*, const CRGBA*, const CRGBA*, const CRGBA*, const CRGBA*, float, float, float, float, float, float, float, float)>(0x727710)(&rc, &c1, &c2, &c3, &c4, f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7]); });
    });
    Run("CSprite2d::SetVertices(n,pos,uv,color) 0x727890", [&](Rng& r, std::string& d) {
        Seed2d(r); const int n = 1 + r.below(8); CVector2D pos[8], uv[8]; for (int i = 0; i < 8; ++i) { pos[i] = Pt(r, 700.f); uv[i] = Pt(r, 2.f); } const CRGBA c = GenCol(r);
        return Both(d, { k2d }, [&] { CSprite2d::SetVertices(n, pos, uv, c); }, [&] { oracle::Fn<void __cdecl(int, const CVector2D*, const CVector2D*, const CRGBA*)>(0x727890)(n, pos, uv, &c); });
    });
    Run("CSprite2d::SetVertices(n,pos,color) 0x727920", [&](Rng& r, std::string& d) {
        Seed2d(r); const int n = 1 + r.below(8); CVector2D pos[8]; for (auto& p : pos) p = Pt(r, 700.f); const CRGBA c = GenCol(r);
        return Both(d, { k2d }, [&] { CSprite2d::SetVertices(n, pos, c); }, [&] { oracle::Fn<void __cdecl(int, const CVector2D*, const CRGBA*)>(0x727920)(n, pos, &c); });
    });
    Run("CSprite2d::SetMaskVertices 0x7279B0", [&](Rng& r, std::string& d) {
        Seed2d(r); const int n = 1 + r.below(8); CVector2D pos[8]; for (auto& p : pos) p = Pt(r, 700.f); const float depth = GenF(r, 2.f);
        return Both(d, { k2d }, [&] { CSprite2d::SetMaskVertices(n, pos, depth); }, [&] { oracle::Fn<void __cdecl(int, const CVector2D*, float)>(0x7279B0)(n, pos, depth); });
    });
    Run("CSprite2d::SetVertices(RwIm2DVertex*,...) 0x727A00", [&](Rng& r, std::string& d) {
        Seed2d(r); float f[8]; for (auto& x : f) x = GenF(r, 2.f); const CRect rc = GenRect(r); const CRGBA c1 = GenCol(r), c2 = GenCol(r), c3 = GenCol(r), c4 = GenCol(r);
        RwIm2DVertex va[4], vb[4]; std::memset(va, 0xCD, sizeof va); std::memset(vb, 0xCD, sizeof vb);
        CSprite2d::SetVertices(va, rc, c1, c2, c3, c4, f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7]);
        oracle::Fn<void __cdecl(RwIm2DVertex*, const CRect*, const CRGBA*, const CRGBA*, const CRGBA*, const CRGBA*, float, float, float, float, float, float, float, float)>(0x727A00)(vb, &rc, &c1, &c2, &c3, &c4, f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7]);
        if (SameBlob(va, vb, sizeof va)) return true;
        d = "got " + Hex(va, sizeof va) + " exe " + Hex(vb, sizeof vb); return false;
    });
    Run("CSprite2d::SetVerticesForSniper 0x727FD0", [&](Rng& r, std::string& d) {
        Seed2d(r); const CRect rc = GenRect(r); const CRGBA c1 = GenCol(r), c2 = GenCol(r), c3 = GenCol(r), c4 = GenCol(r);
        return Both(d, { k2d }, [&] { CSprite2d::SetVerticesForSniper(rc, c1, c2, c3, c4); }, [&] { oracle::Fn<void __cdecl(const CRect*, const CRGBA*, const CRGBA*, const CRGBA*, const CRGBA*)>(0x727FD0)(&rc, &c1, &c2, &c3, &c4); });
    });
    Run("CSprite2d::OffsetTexCoordForBilinearFiltering 0x728150", [&](Rng& r, std::string& d) {
        Seed2d(r); Fill2d(r);
        const float w = GenF(r, r.below(2) ? 1024.f : PickScale(r)), h = GenF(r, r.below(2) ? 1024.f : PickScale(r));
        return Both(d, { k2d }, [&] { CSprite2d::OffsetTexCoordForBilinearFiltering(w, h); }, [&] { oracle::Fn<void __cdecl(float, float)>(0x728150)(w, h); });
    });

    // ---- CSprite: Set4Vertices2D / CalcScreenCoors / CalcHorizonCoors / buffered sprites
    Run("CSprite::Set4Vertices2D(CRect) 0x70E1C0", [&](Rng& r, std::string& d) {
        CSprite::m_f2DNearScreenZ = GenF(r, 2.f); CSprite::m_fRecipNearClipPlane = GenF(r, 2.f);
        const CRect rc = GenRect(r); const CRGBA tl = GenCol(r), tr = GenCol(r), bl = GenCol(r), br = GenCol(r);
        RwIm2DVertex va[4], vb[4]; std::memset(va, 0xCD, sizeof va); std::memset(vb, 0xCD, sizeof vb);
        CSprite::Set4Vertices2D(va, rc, tl, tr, bl, br);
        oracle::Fn<void __cdecl(RwIm2DVertex*, const CRect*, const CRGBA*, const CRGBA*, const CRGBA*, const CRGBA*)>(0x70E1C0)(vb, &rc, &tl, &tr, &bl, &br);
        if (SameBlob(va, vb, sizeof va)) return true;
        d = "got " + Hex(va, sizeof va) + " exe " + Hex(vb, sizeof vb); return false;
    });
    Run("CSprite::CalcScreenCoors 0x70CE30", [&](Rng& r, std::string& d) {
        CamFix cf(r); SetRes(r);
        CMatrix& vm = TheCamera.m_mViewMatrix; (void)vm;
        float* m = reinterpret_cast<float*>(0xB6FA2C); for (int i = 0; i < 16; ++i) m[i] = GenF(r, r.below(2) ? 1.f : 500.f);
        CDraw::ms_fNearClipZ = GenF(r, r.below(2) ? 1.f : 20.f); CDraw::ms_fFarClipZ = GenF(r, r.below(2) ? 1000.f : 20000.f); CDraw::ms_fFOV = r.below(4) ? 45.f + r.f01() * 40.f : GenF(r, 100.f);
        const RwV3d in{ GenF(r, 100.f), GenF(r, 100.f), r.below(3) ? GenF(r, 200.f) : GenF(r, PickScale(r)) }; const bool cmax = r.below(2), cmin = r.below(2);
        RwV3d oa{ 9, 9, 9 }, ob{ 9, 9, 9 }; float wa = 7, ha = 7, wb = 7, hb = 7;
        const bool a = CSprite::CalcScreenCoors(in, &oa, &wa, &ha, cmax, cmin);
        const bool b = oracle::Fn<bool __cdecl(const RwV3d*, RwV3d*, float*, float*, bool, bool)>(0x70CE30)(&in, &ob, &wb, &hb, cmax, cmin);
        Hit(b);
        if (a == b && SameBlob(&oa, &ob, 12) & SameF(wa, wb) & SameF(ha, hb)) return true;
        d = "in " + V({in.x, in.y, in.z}) + " got " + std::to_string(a) + V({oa.x, oa.y, oa.z}) + F(wa) + "," + F(ha) + " exe " + std::to_string(b) + V({ob.x, ob.y, ob.z}) + F(wb) + "," + F(hb); return false;
    });
    Run("CSprite::CalcHorizonCoors 0x70E3E0", [&](Rng& r, std::string& d) {
        CamFix cf(r);
        float* m = reinterpret_cast<float*>(0xB6FA2C); for (int i = 0; i < 16; ++i) m[i] = GenF(r, r.below(2) ? 1.f : 500.f);
        TheCamera.m_placement.m_vPosn = GenV(r, 3000.f);
        TheCamera.m_fCamFrontXNorm = GenF(r, 1.f); TheCamera.m_fCamFrontYNorm = GenF(r, 1.f);
        SetRes(r);
        const float a = CSprite::CalcHorizonCoors(), b = oracle::Fn<float __cdecl()>(0x70E3E0)();
        if (SameF(a, b)) return true;
        d = "got " + F(a) + " exe " + F(b); return false;
    });

    // ---- buffered XLU sprites: the vertex / index buffer, the buffer index and the z-test flag
    const auto SeedSpr = [&](Rng& r) {
        SetRes(r);
        CSprite::m_f2DNearScreenZ = r.below(4) ? r.f01() : GenF(r, 2.f); CSprite::m_f2DFarScreenZ = r.below(4) ? 1.f - r.f01() * 0.1f : GenF(r, 2.f);
        CDraw::ms_fNearClipZ = r.below(4) ? 0.2f + r.f01() * 2.f : GenF(r, 5.f); CDraw::ms_fFarClipZ = r.below(4) ? 500.f + r.f01() * 3000.f : GenF(r, 5000.f);
        StaticRef<int32>(0xC6A158) = r.below(8);
        Snap v((uintptr_t)&TempBufferVertices.m_2d[0], 9 * 4 * 28); v.Save(); v.FillRandom(r); v.Restore();
        Snap ix((uintptr_t)&aTempBufferIndices[0], 112); ix.Save(); ix.FillRandom(r); ix.Restore();
    };
    const Rg kSprRegs[3] = { { 0xC6A158, 8 }, { (uintptr_t)&TempBufferVertices.m_2d[0], 9 * 4 * 28 }, { (uintptr_t)&aTempBufferIndices[0], 112 } };
    const auto SprPos = [&](Rng& r) { return CVector{ r.below(10) ? r.f01() * 700.f - 30.f : GenF(r, PickScale(r)), r.below(10) ? r.f01() * 500.f - 30.f : GenF(r, PickScale(r)), r.below(8) ? 0.5f + r.f01() * 100.f : GenF(r, PickScale(r)) }; };
    const auto SprSize = [&](Rng& r) { return r.below(8) ? r.f01() * 60.f : GenF(r, PickScale(r)); };
    const auto Intens = [&](Rng& r) { return (int16)(r.below(4) ? r.below(300) : r.u32()); };
    const auto SprDesc = [&](std::string& d, const char* what, const std::string& in) { d += std::string(what) + " " + in; };

    Run("CSprite::RenderBufferedOneXLUSprite 0x70E4A0", [&](Rng& r, std::string& d) {
        SeedSpr(r); const CVector pos = SprPos(r); const CVector2D sz{ SprSize(r), SprSize(r) }; const uint8 cr = r.u32(), cg = r.u32(), cb = r.u32(), al = r.u32(); const int16 in = Intens(r); const float rz = GenF(r, 2.f);
        if (Both(d, { kSprRegs[0], kSprRegs[1], kSprRegs[2] }, [&] { CSprite::RenderBufferedOneXLUSprite(pos, sz, cr, cg, cb, in, rz, al); },
                 [&] { oracle::Fn<void __cdecl(CVector, CVector2D, uint8, uint8, uint8, int16, float, uint8)>(0x70E4A0)(pos, sz, cr, cg, cb, in, rz, al); })) return true;
        SprDesc(d, "in", V(pos) + P2S(sz)); return false;
    });
    Run("CSprite::RenderBufferedOneXLUSprite_Rotate_Aspect 0x70E780", [&](Rng& r, std::string& d) {
        SeedSpr(r); const CVector pos = SprPos(r); const float w = SprSize(r), h = SprSize(r); const uint8 cr = r.u32(), cg = r.u32(), cb = r.u32(), al = r.u32(); const int16 in = Intens(r); const float rz = GenF(r, 2.f), ang = r.below(8) ? r.f01() * 12.f - 6.f : GenF(r, PickScale(r));
        if (Both(d, { kSprRegs[0], kSprRegs[1], kSprRegs[2] }, [&] { CSprite::RenderBufferedOneXLUSprite_Rotate_Aspect(pos.x, pos.y, pos.z, w, h, cr, cg, cb, in, rz, ang, al); },
                 [&] { oracle::Fn<void __cdecl(float, float, float, float, float, uint8, uint8, uint8, int16, float, float, uint8)>(0x70E780)(pos.x, pos.y, pos.z, w, h, cr, cg, cb, in, rz, ang, al); })) return true;
        SprDesc(d, "in", V(pos) + " w " + F(w) + " h " + F(h) + " ang " + F(ang)); return false;
    });
    Run("CSprite::RenderBufferedOneXLUSprite_Rotate_Dimension 0x70EAB0", [&](Rng& r, std::string& d) {
        SeedSpr(r); const CVector pos = SprPos(r); const CVector2D sz{ SprSize(r), SprSize(r) }; const uint8 cr = r.u32(), cg = r.u32(), cb = r.u32(), al = r.u32(); const int16 in = Intens(r); const float rz = GenF(r, 2.f), ang = r.below(8) ? r.f01() * 12.f - 6.f : GenF(r, PickScale(r));
        if (Both(d, { kSprRegs[0], kSprRegs[1], kSprRegs[2] }, [&] { CSprite::RenderBufferedOneXLUSprite_Rotate_Dimension(pos, sz, cr, cg, cb, in, rz, ang, al); },
                 [&] { oracle::Fn<void __cdecl(CVector, CVector2D, uint8, uint8, uint8, int16, float, float, uint8)>(0x70EAB0)(pos, sz, cr, cg, cb, in, rz, ang, al); })) return true;
        SprDesc(d, "in", V(pos) + P2S(sz) + " ang " + F(ang) + " rgb " + std::to_string(cr) + "," + std::to_string(cg) + "," + std::to_string(cb) + " int " + std::to_string(in) + " a " + std::to_string(al)); return false;
    });
    Run("CSprite::RenderBufferedOneXLUSprite_Rotate_2Colours 0x70EDE0", [&](Rng& r, std::string& d) {
        SeedSpr(r); const CVector pos = SprPos(r); const float w = SprSize(r), h = SprSize(r); const uint8 c[6] = { (uint8)r.u32(), (uint8)r.u32(), (uint8)r.u32(), (uint8)r.u32(), (uint8)r.u32(), (uint8)r.u32() }, al = r.u32();
        const float fx = GenF(r, 2.f), fy = GenF(r, 2.f), rz = GenF(r, 2.f), ang = r.below(8) ? r.f01() * 12.f - 6.f : GenF(r, PickScale(r));
        if (Both(d, { kSprRegs[0], kSprRegs[1], kSprRegs[2] }, [&] { CSprite::RenderBufferedOneXLUSprite_Rotate_2Colours(pos.x, pos.y, pos.z, w, h, c[0], c[1], c[2], c[3], c[4], c[5], fx, fy, rz, ang, al); },
                 [&] { oracle::Fn<void __cdecl(float, float, float, float, float, uint8, uint8, uint8, uint8, uint8, uint8, float, float, float, float, uint8)>(0x70EDE0)(pos.x, pos.y, pos.z, w, h, c[0], c[1], c[2], c[3], c[4], c[5], fx, fy, rz, ang, al); })) return true;
        SprDesc(d, "in", V(pos) + " w " + F(w) + " h " + F(h) + " ang " + F(ang)); return false;
    });
    Run("CSprite::RenderBufferedOneXLUSprite2D 0x70F440", [&](Rng& r, std::string& d) {
        SeedSpr(r); CSprite::m_fRecipNearClipPlane = GenF(r, 2.f); const CVector2D pos{ r.f01() * 700.f, r.f01() * 500.f }, sz{ SprSize(r), 0.f }; const RwRGBA col{ (uint8)r.u32(), (uint8)r.u32(), (uint8)r.u32(), (uint8)r.u32() }; const int16 in = Intens(r); const uint8 al = r.u32();
        if (Both(d, { kSprRegs[0], kSprRegs[1], kSprRegs[2] }, [&] { CSprite::RenderBufferedOneXLUSprite2D(pos, sz, col, in, al); },
                 [&] { oracle::Fn<void __cdecl(CVector2D, CVector2D, const RwRGBA*, int16, uint8)>(0x70F440)(pos, sz, &col, in, al); })) return true;
        SprDesc(d, "in", P2S(pos) + P2S(sz)); return false;
    });
}


// ---------------------------------------------------------------------------------------------------------------------------------
// CFont (the exe's CMessages string helpers are replaced on the port side by plain strlen / no-op: the test strings contain no ~k~ control tokens)
uint32 CMessages::GetStringLength(const GxtChar* str) { return (uint32)std::strlen((const char*)str); }
void CMessages::InsertPlayerControlKeysInString(GxtChar*) {}

static void SeedFont(Rng& r) {
    Snap fd(0xC718B0, 2 * 210 + 4); fd.Save();
    for (size_t i = 0; i < fd.b.size(); ++i) fd.b[i] = r.below(6) ? (uint8)r.below(40) : (uint8)r.u32();
    fd.Restore();
    CFont::m_Scale = { r.below(5) ? 0.2f + r.f01() * 2.f : GenF(r, 4.f), r.below(5) ? 0.2f + r.f01() * 2.f : GenF(r, 4.f) };
    CFont::m_FontStyle = r.below(3) ? (uint8)r.below(4) : (uint8)r.u32();
    CFont::m_FontTextureId = (uint8)r.below(2);
    CFont::m_bFontPropOn = r.below(2);
    CFont::m_nFontOutlineSize = (uint8)(r.below(3) ? r.below(4) : r.u32());
    CFont::m_bFontCentreAlign = r.below(4) == 0; CFont::m_bFontRightAlign = r.below(4) == 0; CFont::m_bFontJustify = r.below(3) == 0;
    CFont::m_bNewLine = r.below(8) == 0; CFont::PS2Symbol = (eExtraFontSymbol)0;
    CFont::m_fWrapx = r.below(6) ? 30.f + r.f01() * 600.f : GenF(r, 700.f); CFont::m_fFontCentreSize = r.below(6) ? 30.f + r.f01() * 600.f : GenF(r, 700.f); CFont::m_fRightJustifyWrap = r.below(6) ? r.f01() * 300.f : GenF(r, 700.f);
    CFont::m_Color = GenCol(r);
    Snap hc(0xBAB22C, 0x3C); hc.Save(); hc.FillRandom(r); hc.Restore();
    Snap gs(0xB71670, 352); gs.Save(); gs.FillRandom(r); gs.Restore();
}
static void GenText(Rng& r, uint8* out, size_t cap) {
    static const char* toks[] = { "~r~", "~g~", "~b~", "~h~", "~n~", "~w~", "~y~", "~p~", "~s~", "~l~", "~<~", "~>~", "~x~", "~o~", "~u~", "~~", "~z~" };
    size_t n = 0; const int len = r.below(5) ? r.below(40) : r.below(150);
    for (int i = 0; i < len && n + 8 < cap; ++i) {
        if (r.below(9) == 0) { const char* t = toks[r.below(17)]; while (*t) out[n++] = (uint8)*t++; }
        else if (r.below(5) == 0) out[n++] = ' ';
        else { uint8 c = (uint8)(0x21 + r.below(0x5E)); if (c == '~' || c == 'k' || c == 'K') c = 'a'; if (r.below(40) == 0) c = (uint8)r.u32() | 1; if (c == 0) c = 'x'; out[n++] = c; }
    }
    out[n] = 0;
}

static void TestHudFont() {
    Run("CFont::FindSubFontCharacter 0x7192C0", [&](Rng& r, std::string& d) {
        const uint8 l = (uint8)(r.below(3) ? r.below(150) : r.u32()), st = (uint8)(r.below(2) ? r.below(4) : r.u32());
        const uint8 a = CFont::FindSubFontCharacter(l, st), b = oracle::Fn<uint8 __cdecl(uint8, uint8)>(0x7192C0)(l, st);
        if (a == b) return true; d = "l " + std::to_string(l) + " st " + std::to_string(st) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CFont::GetCharacterSize 0x719750", [&](Rng& r, std::string& d) {
        SeedFont(r); const uint8 l = (uint8)(r.below(3) ? r.below(170) : r.u32());
        const float a = CFont::GetCharacterSize(l), b = oracle::Fn<float __cdecl(uint8)>(0x719750)(l);
        if (SameF(a, b)) return true; d = "l " + std::to_string(l) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CFont::GetStringWidth 0x71A0E0", [&](Rng& r, std::string& d) {
        SeedFont(r); uint8 txt[400]; GenText(r, txt, sizeof txt); const bool full = r.below(2), script = r.below(4) == 0;
        CTheScripts::NumberOfIntroTextLinesThisFrame = 0; *reinterpret_cast<int32*>(&CTheScripts::IntroTextLines[0].FontStyle) = (int32)r.below(4); CTheScripts::IntroTextLines[0].Scale = { 0.5f, 1.f };
        const float a = CFont::GetStringWidth(txt, full, script), b = oracle::Fn<float __cdecl(const uint8*, bool, bool)>(0x71A0E0)(txt, full, script);
        if (SameF(a, b)) return true; d = std::string("txt '") + (const char*)txt + "' full " + std::to_string(full) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CFont::ParseToken 0x718F00", [&](Rng& r, std::string& d) {
        SeedFont(r); uint8 txt[400]; GenText(r, txt, sizeof txt); size_t off = 0; while (txt[off] && txt[off] != '~') ++off;   // start at a tag when there is one
        if (!txt[off]) { txt[0] = '~'; txt[1] = "rgbhwsypnl<>xouaBDJKMQTVYCP"[r.below(27)]; txt[2] = '~'; txt[3] = 'A'; txt[4] = 0; off = 0; }
        const bool blip = r.below(2); char tg0 = r.below(2) ? 0 : 'Z'; char tga = tg0, tgb = tg0; const bool useTag = r.below(3) != 0;
        CRGBA ca = GenCol(r), cb = ca; char *ra = nullptr, *rb = nullptr;
        if (!Both(d, { {0xC71A54, 2} }, [&] { ra = CFont::ParseToken((char*)txt + off, ca, blip, useTag ? &tga : nullptr); }, [&] { rb = oracle::Fn<char* __cdecl(char*, CRGBA*, bool, char*)>(0x718F00)((char*)txt + off, &cb, blip, useTag ? &tgb : nullptr); })) return false;
        if (ra == rb && tga == tgb && SameBlob(&ca, &cb, 4)) return true;
        d += std::string("txt '") + (const char*)txt + "' off " + std::to_string(off) + " got +" + std::to_string(ra - (char*)txt) + " exe +" + std::to_string(rb - (char*)txt) + " tag " + tga + "/" + tgb + " col " + HexVA(*(uint32*)&ca) + "/" + HexVA(*(uint32*)&cb); return false;
    });
    Run("CFont::GetNumberLines 0x71A5E0", [&](Rng& r, std::string& d) {
        SeedFont(r); uint8 txt[400]; GenText(r, txt, sizeof txt); const float x = r.below(4) ? r.f01() * 600.f : GenF(r, 700.f), y = r.f01() * 400.f;
        int16 a = 0, b = 0;
        if (Both(d, { {0xC71A54, 0x4C}, {0xB71670, 4} }, [&] { a = CFont::GetNumberLines(x, y, txt); }, [&] { b = oracle::Fn<int16 __cdecl(float, float, const uint8*)>(0x71A5E0)(x, y, txt); }) && a == b) return true;
        d += std::string("txt '") + (const char*)txt + "' x " + F(x) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
// CPointLights (the RW call AddAnExtraDirectionalLight is recorded on both sides)
static std::vector<std::array<float, 7>> g_extraLights;
void AddAnExtraDirectionalLight(RpWorld* w, float x, float y, float z, float red, float green, float blue) { g_extraLights.push_back({ (float)(uintptr_t)w, x, y, z, red, green, blue }); }
static void __cdecl HostAddExtraLight(RpWorld* w, float x, float y, float z, float red, float green, float blue) { AddAnExtraDirectionalLight(w, x, y, z, red, green, blue); }

static void SeedLights(Rng& r, const CVector& nr) {
    CPointLights::NumLights = r.below(6);
    for (auto& l : CPointLights::aLights) {
        l.m_vecPosn = r.below(4) ? CVector{ nr.x + GenF(r, 20.f, S_NAN | S_ZERO), nr.y + GenF(r, 20.f, S_NAN | S_ZERO), nr.z + GenF(r, 20.f, S_NAN | S_ZERO) } : GenV(r, 100.f);
        l.m_vecDirection = GenDir(r);
        l.m_fRadius = r.below(5) ? 1.f + r.f01() * 30.f : GenF(r, 50.f);
        l.m_fColorRed = r.below(4) ? r.f01() * 2.f : GenF(r, 3.f); l.m_fColorGreen = r.below(4) ? r.f01() * 2.f : GenF(r, 3.f); l.m_fColorBlue = r.below(4) ? r.f01() * 2.f : GenF(r, 3.f);
        l.m_pEntityToLight = r.below(3) ? nullptr : reinterpret_cast<CEntity*>(0x1000 + r.below(3) * 16);
        l.m_nType = (ePointLightType)r.below(6); l.m_nFogType = (uint8)r.u32(); l.m_bGenerateShadows = r.below(2);
    }
}
static std::string Lights2S(const std::vector<std::array<float, 7>>& v) { std::string s; for (auto& a : v) { s += "["; for (int i = 1; i < 7; ++i) s += F(a[i]) + (i < 6 ? "," : ""); s += "]"; } return s; }

static void TestHudLights() {
    oracle::Patch(0x735840, (void*)&HostAddExtraLight);
    Run("CPointLights::GetLightMultiplier 0x6FFE70", [&](Rng& r, std::string& d) {
        const CVector pt = GenV(r, 30.f); SeedLights(r, pt);
        const float a = CPointLights::GetLightMultiplier(&pt), b = oracle::Fn<float __cdecl(const CVector*)>(0x6FFE70)(&pt);
        if (SameF(a, b)) return true; d = "pt " + V(pt) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CPointLights::GenerateLightsAffectingObject 0x6FFBB0", [&](Rng& r, std::string& d) {
        const CVector pt = GenV(r, 30.f); SeedLights(r, pt);
        CEntity* ent = r.below(3) ? nullptr : reinterpret_cast<CEntity*>(0x1000 + r.below(3) * 16);
        float ta = GenF(r, 2.f), tb = ta; float* pa = r.below(4) ? &ta : nullptr; float* pb = pa ? &tb : nullptr;
        g_extraLights.clear(); const float a = CPointLights::GenerateLightsAffectingObject(&pt, pa, ent); const auto la = g_extraLights;
        g_extraLights.clear(); const float b = oracle::Fn<float __cdecl(const CVector*, float*, CEntity*)>(0x6FFBB0)(&pt, pb, ent); const auto lb = g_extraLights;
        bool ok = SameF(a, b) & (la.size() == lb.size());
        if (ok) { for (size_t i = 0; i < la.size(); ++i) ok &= SameBlob(la[i].data(), lb[i].data(), 28); if (pa) ok &= SameF(ta, tb); }
        if (ok) return true;
        d = "pt " + V(pt) + " got " + F(a) + " " + Lights2S(la) + " tot " + F(ta) + " exe " + F(b) + " " + Lights2S(lb) + " tot " + F(tb); return false;
    });
    Run("CPointLights::AddLight 0x7000E0", [&](Rng& r, std::string& d) {
        CamFix cf(r); TheCamera.m_matrix = nullptr; TheCamera.m_placement.m_vPosn = GenV(r, 40.f);
        SeedLights(r, TheCamera.m_placement.m_vPosn);
        CPointLights::NumLights = r.below(4) ? r.below(32) : 30 + r.below(5);
        const uint8 type = (uint8)r.below(6), fog = (uint8)r.u32(); const bool gen = r.below(2);
        const CVector pt = r.below(4) ? CVector{ TheCamera.m_placement.m_vPosn.x + GenF(r, 40.f), TheCamera.m_placement.m_vPosn.y + GenF(r, 40.f), TheCamera.m_placement.m_vPosn.z + GenF(r, 40.f) } : GenV(r, 100.f);
        const CVector dir = GenDir(r); const float rad = r.below(5) ? 1.f + r.f01() * 40.f : GenF(r, 80.f), cr = r.f01() * 2.f, cg = GenF(r, 2.f), cb = r.f01() * 2.f;
        CEntity* ent = r.below(2) ? nullptr : reinterpret_cast<CEntity*>(0x2000);
        if (Both(d, { {0xC3F0D0, 4}, {0xC3F0E0, 32 * 0x30} }, [&] { CPointLights::AddLight(type, pt, dir, rad, cr, cg, cb, fog, gen, ent); },
                 [&] { oracle::Fn<void __cdecl(uint8, CVector, CVector, float, float, float, float, uint8, bool, CEntity*)>(0x7000E0)(type, pt, dir, rad, cr, cg, cb, fog, gen, ent); })) return true;
        d += "pt " + V(pt) + " cam " + V(TheCamera.m_placement.m_vPosn); return false;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
// CSkidmarks (static array of 32 marks)
static void SeedSkid(Rng& r, bool freeSlot) {
    CTimer::m_snTimeInMilliseconds = r.below(8) ? 100000 + r.below(100000) : r.u32();
    CReplay::Mode = MODE_RECORD;
    for (auto& m : CSkidmarks::m_aSkidmarks) {
        for (auto& p : m.m_vPosn) p = GenV(r, 100.f, S_NAN | S_ZERO | S_TINY);
        for (auto& f : m.m_partDirX) f = GenF(r, 3.f, S_NAN | S_ZERO | S_TINY);
        for (auto& f : m.m_partDirY) f = GenF(r, 3.f, S_NAN | S_ZERO | S_TINY);
        m.m_nId = r.below(3) ? 1 + r.below(6) : r.u32();
        const uint32 now = CTimer::m_snTimeInMilliseconds;
        m.m_lastDisappearTimeUpdateMs = r.below(3) ? now - r.below(400) : r.u32();
        m.m_fadeBeginMs = r.below(3) ? now + r.below(20000) - 5000 : r.u32(); m.m_disappearAtMs = r.below(3) ? now + r.below(30000) - 5000 : r.u32();
        m.m_nType = (eSkidmarkType)r.below(4); m.m_nNumParts = (uint16)(r.below(4) ? r.below(16) : r.u32() & 0xF);
        m.m_nState = (eSkidmarkState)(freeSlot ? r.below(3) : 1 + r.below(2)); m.m_bActive = r.below(2);
    }
    if (freeSlot && r.below(2)) CSkidmarks::m_aSkidmarks[r.below(32)].m_nState = eSkidmarkState::INACTIVE;
}
static void TestHudSkid() {
    constexpr Rg kSk{ 0xC79AA8, 32 * 0x158 };
    Run("CSkidmarks::Update 0x7205C0", [&](Rng& r, std::string& d) {
        SeedSkid(r, true);
        return Both(d, { kSk }, [] { CSkidmarks::Update(); }, [] { oracle::Fn<void __cdecl()>(0x7205C0)(); });
    });
    Run("CSkidmarks::RegisterOne 0x720930", [&](Rng& r, std::string& d) {
        SeedSkid(r, true); CSkidmarks::m_aSkidmarks[r.below(32)].m_nState = eSkidmarkState::INACTIVE;   // (the no-free-slot path calls CCamera::IsSphereVisible -> RwV3dTransformPoints (device): not covered)
        const uint32 id = r.below(2) ? 1 + r.below(6) : r.u32(); const CVector pos = GenV(r, 100.f, S_NAN | S_ZERO | S_TINY); const float dx = GenF(r, 3.f), dy = GenF(r, 3.f), len = GenF(r, 5.f);
        const eSkidmarkType ty = (eSkidmarkType)r.below(4); bool ba = r.below(3) == 0, bb = ba;
        if (Both(d, { kSk }, [&] { CSkidmarks::RegisterOne(id, pos, dx, dy, ty, &ba, len); }, [&] { oracle::Fn<void __cdecl(uint32, const CVector*, float, float, eSkidmarkType, bool*, float)>(0x720930)(id, &pos, dx, dy, ty, &bb, len); }) && ba == bb) return true;
        d += "id " + std::to_string(id) + " pos " + V(pos) + " blood " + std::to_string(ba) + "/" + std::to_string(bb); return false;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
// CCoronas / CRegisteredCorona (the line-of-sight path of CRegisteredCorona::Update and attached coronas are not covered: CWorld / CEntity references)
static void SeedCoronas(Rng& r) {
    CCoronas::NumCoronas = r.below(8); CCoronas::LightsMult = r.below(3) ? r.f01() : GenF(r, 2.f); CCoronas::bChangeBrightnessImmediately = r.below(3) == 0 ? (int)r.below(4) : 0; CCoronas::SunBlockedByClouds = r.below(2);
    CTimer::ms_fTimeStep = r.below(4) ? r.f01() * 4.f : GenF(r, 8.f);
    Snap cs((uintptr_t)&CCoronas::aCoronas[0], sizeof(CCoronas::aCoronas)); cs.Save(); cs.FillRandom(r); cs.Restore();
    for (auto& c : CCoronas::aCoronas) {
        c.m_dwId = r.below(3) == 0 ? 0 : 1 + r.below(8); c.m_pTexture = nullptr; c.m_pAttachedTo = nullptr; c.m_bAttached = false; c.m_bCheckObstacles = (eCoronaLOSCheck)0;
        c.m_vPosn = GenV(r, 200.f, S_NAN | S_ZERO | S_TINY); c.m_fFadeSpeed = r.below(4) ? r.f01() * 20.f : GenF(r, 40.f); c.m_fSize = r.f01() * 3.f; c.m_fFarClip = r.f01() * 200.f;
        c.m_FadedIntensity = (uint8)r.u32(); c.m_Color = GenCol(r); c.m_bRegisteredThisFrame = r.below(2); c.m_bJustCreated = r.below(2); c.m_bOffScreen = r.below(2); c.m_bOnlyFromBelow = r.below(2);
    }
}
static void TestHudCorona() {
    const Rg kCor{ 0xC3E058, 64 * 0x3C }, kCnt{ 0xC3E038, 4 }, kGl{ 0x8D4B5C, 4 };
    Run("CRegisteredCorona::Update 0x6FABF0", [&](Rng& r, std::string& d) {
        CamFix cf(r); TheCamera.m_matrix = nullptr; TheCamera.m_placement.m_vPosn = GenV(r, 200.f, S_NAN | S_ZERO | S_TINY);
        SeedCoronas(r); const size_t i = r.below(64);
        return Both(d, { kCor, kCnt, kGl }, [&] { CCoronas::aCoronas[i].Update(); }, [&] { oracle::Fn<void __fastcall(CRegisteredCorona*, int)>(0x6FABF0)(&CCoronas::aCoronas[i], 0); });
    });
    Run("CCoronas::UpdateCoronaCoors 0x6FC4D0", [&](Rng& r, std::string& d) {
        CamFix cf(r); TheCamera.m_matrix = nullptr; TheCamera.m_placement.m_vPosn = GenV(r, 200.f, S_NAN | S_ZERO | S_TINY);
        SeedCoronas(r); const uint32 id = r.below(10); const CVector pos = r.below(2) ? CVector{ TheCamera.m_placement.m_vPosn.x + GenF(r, 150.f), TheCamera.m_placement.m_vPosn.y + GenF(r, 150.f), GenF(r, 100.f) } : GenV(r, 400.f);
        const float range = r.below(4) ? r.f01() * 200.f : GenF(r, 400.f), ang = GenF(r, 7.f);
        return Both(d, { kCor, kCnt }, [&] { CCoronas::UpdateCoronaCoors(id, pos, range, ang); }, [&] { oracle::Fn<void __cdecl(uint32, const CVector*, float, float)>(0x6FC4D0)(id, &pos, range, ang); });
    });
    Run("CCoronas::RegisterCorona 0x6FC180", [&](Rng& r, std::string& d) {
        CamFix cf(r); TheCamera.m_matrix = nullptr; TheCamera.m_placement.m_vPosn = GenV(r, 200.f, S_NAN | S_ZERO | S_TINY);
        SeedCoronas(r); const uint32 id = r.below(12); const uint8 cr = r.u32(), cg = r.u32(), cb = r.u32(), in = r.below(4) ? (uint8)r.u32() : (uint8)0;
        const CVector pos = r.below(2) ? CVector{ TheCamera.m_placement.m_vPosn.x + GenF(r, 80.f), TheCamera.m_placement.m_vPosn.y + GenF(r, 80.f), TheCamera.m_placement.m_vPosn.z + GenF(r, 80.f) } : GenV(r, 400.f);
        const float size = GenF(r, 5.f), range = r.below(4) ? r.f01() * 200.f : GenF(r, 400.f), ang = GenF(r, 7.f), pull = GenF(r, 3.f), fade = r.below(4) ? r.f01() * 20.f : GenF(r, 40.f);
        RwTexture* tex = reinterpret_cast<RwTexture*>(0x1234 + r.below(3)); const auto fl = (eCoronaFlareType)r.below(4); const auto rf = (eCoronaReflType)r.below(3); const auto los = (eCoronaLOSCheck)r.below(2); const auto tr = (eCoronaTrail)r.below(2);
        const bool neon = r.below(3) == 0, fullBright = r.below(2), below = r.below(2), white = r.below(2);
        return Both(d, { kCor, kCnt }, [&] { CCoronas::RegisterCorona(id, nullptr, cr, cg, cb, in, pos, size, range, tex, fl, rf, los, tr, ang, neon, pull, fullBright, fade, below, white); },
                    [&] { oracle::Fn<void __cdecl(uint32, CEntity*, uint8, uint8, uint8, uint8, const CVector*, float, float, RwTexture*, eCoronaFlareType, eCoronaReflType, eCoronaLOSCheck, eCoronaTrail, float, bool, float, bool, float, bool, bool)>(0x6FC180)(id, nullptr, cr, cg, cb, in, &pos, size, range, tex, fl, rf, los, tr, ang, neon, pull, fullBright, fade, below, white); });
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void TestHudClouds() {
    Run("CClouds::Update 0x712FF0", [&](Rng& r, std::string& d) {
        CamFix cf(r); TheCamera.m_fOrientation = GenF(r, r.below(2) ? 7.f : PickScale(r));
        CWeather::Wind = r.below(4) ? r.f01() * 2.f : GenF(r, 5.f); CTimer::ms_fTimeStep = r.below(4) ? r.f01() * 4.f : GenF(r, 50.f);
        CClouds::CloudRotation = r.below(4) ? GenF(r, 20.f) : GenF(r, 1e6f); CClouds::IndividualRotation = r.below(3) ? (int32)r.below(100000) : (int32)r.u32();
        return Both(d, { {0xC6AA6C, 8} }, [] { CClouds::Update(); }, [] { oracle::Fn<void __cdecl()>(0x712FF0)(); });
    });
}
