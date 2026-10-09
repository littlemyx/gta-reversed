// P2B-06 unit test: RwIm2DRender* / RwIm3D* over librw's D3D9 device (needs a D3D9 HAL adapter; works under Wine/wined3d, no game data).
// Renders into the camera frame buffer and reads the pixels back. Exit code 0 = all checks passed.
//  - device-less checks: return values without a transform / device, RwIm3DTransform limits, accessor macros
//  - Im2D: triangle, quad (strip, fan, indexed list), line, vertex colour, game-side vertex layout (rhw / emissiveColor)
//  - Im3D: triangle with and without an LTM (translation and scale), indexed quad, lines, vertex alpha vs rwIM3D_ALLOPAQUE, End pairing
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

static RwGlobals s_RwGlobals{};
RwGlobals* RwEngineInstance = &s_RwGlobals;
bool RwInitialized = false;
RwRGBAReal AmbientSaturated{}; // normally defined in rwglobals.cpp (kept out of this test's link line)

static int g_fail = 0;
#define CHECK(c) do { const bool ok_ = !!(c); std::printf("%-4s %s\n", ok_ ? "ok" : "FAIL", #c); if (!ok_) ++g_fail; } while (0)

static RwBool Set(RwRenderState s, RwUInt32 v) { return RwRenderStateSet(s, reinterpret_cast<void*>(static_cast<std::uintptr_t>(v))); }

// ---- read the whole render target back (X8R8G8B8 / A8R8G8B8), alpha forced to 0xFF
struct Shot {
    int w = 0, h = 0;
    std::vector<unsigned> px;
    unsigned at(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h) ? 0xDEADBEEFu : px[y * w + x]; }
    int count(unsigned c) const { int n = 0; for (unsigned p : px) n += p == c; return n; }
    int countNot(unsigned c) const { return w * h - count(c); }
    // bounding box and centroid of the pixels that differ from `bg`
    bool bbox(unsigned bg, int& x0, int& y0, int& x1, int& y1) const {
        x0 = y0 = 1 << 30; x1 = y1 = -1;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (at(x, y) != bg) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
        return x1 >= 0;
    }
};
static Shot Capture(IDirect3DDevice9* dev) {
    Shot s;
    IDirect3DSurface9 *rt = nullptr, *sys = nullptr;
    if (SUCCEEDED(dev->GetRenderTarget(0, &rt))) {
        D3DSURFACE_DESC d{};
        rt->GetDesc(&d);
        if (SUCCEEDED(dev->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr)) && SUCCEEDED(dev->GetRenderTargetData(rt, sys))) {
            D3DLOCKED_RECT lr{};
            if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
                s.w = d.Width; s.h = d.Height; s.px.resize(s.w * s.h);
                for (int y = 0; y < s.h; y++) for (int x = 0; x < s.w; x++) s.px[y * s.w + x] = *(unsigned*)((char*)lr.pBits + y * lr.Pitch + x * 4) | 0xFF000000u;
                sys->UnlockRect();
            }
        }
        if (sys) sys->Release();
        rt->Release();
    }
    return s;
}

static constexpr unsigned kBlack = 0xFF000000u;
static RwCamera* g_cam;
static RwRGBA    g_black{0, 0, 0, 255};
static void Clear() { RwCameraClear(g_cam, &g_black, rwCAMERACLEARIMAGE | rwCAMERACLEARZ); }

// the states a game would have set for UI / plain geometry
static void BaseStates() {
    Set(rwRENDERSTATEZTESTENABLE, 0);
    Set(rwRENDERSTATEZWRITEENABLE, 0);
    Set(rwRENDERSTATECULLMODE, rwCULLMODECULLNONE);
    Set(rwRENDERSTATEVERTEXALPHAENABLE, 0);
    Set(rwRENDERSTATEFOGENABLE, 0);
    Set(rwRENDERSTATESRCBLEND, rwBLENDSRCALPHA);
    Set(rwRENDERSTATEDESTBLEND, rwBLENDINVSRCALPHA);
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
}

static RwIm2DVertex V2(float x, float y, unsigned r, unsigned g, unsigned b, unsigned a = 255) {
    RwIm2DVertex v{};
    RwIm2DVertexSetScreenX(&v, x); RwIm2DVertexSetScreenY(&v, y); RwIm2DVertexSetScreenZ(&v, RwIm2DGetNearScreenZ() + 0.5f * (RwIm2DGetFarScreenZ() - RwIm2DGetNearScreenZ()));
    RwIm2DVertexSetRecipCameraZ(&v, 1.0f);
    RwIm2DVertexSetU(&v, 0.0f, 1.0f); RwIm2DVertexSetV(&v, 0.0f, 1.0f);
    RwIm2DVertexSetIntRGBA(&v, r, g, b, a);
    return v;
}
static RwIm3DVertex V3(float x, float y, float z, unsigned r, unsigned g, unsigned b, unsigned a = 255) {
    RwIm3DVertex v{};
    RwIm3DVertexSetPos(&v, x, y, z);
    RwIm3DVertexSetU(&v, 0.0f); RwIm3DVertexSetV(&v, 0.0f);
    RwIm3DVertexSetRGBA(&v, r, g, b, a);
    return v;
}
static unsigned Argb(unsigned r, unsigned g, unsigned b) { return 0xFF000000u | (r << 16) | (g << 8) | b; }
static bool Near(unsigned a, unsigned b, int tol) {
    for (int s = 0; s < 24; s += 8) if (std::abs((int)((a >> s) & 0xFF) - (int)((b >> s) & 0xFF)) > tol) return false;
    return true;
}

static void DeviceLess(bool device) {
    std::printf("--- return values without a device / transform\n");
    CHECK(RwIm3DEnd() == FALSE);
    CHECK(RwIm3DRenderPrimitive(rwPRIMTYPETRILIST) == FALSE);
    RwImVertexIndex idx[3] = {0, 1, 2};
    CHECK(RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, idx, 3) == FALSE);
    CHECK(RwIm3DRenderLine(0, 1) == FALSE && RwIm3DRenderTriangle(0, 1, 2) == FALSE);
    RwIm3DVertex v[3] = {V3(0, 0, 1, 1, 2, 3), V3(1, 0, 1, 4, 5, 6), V3(0, 1, 1, 7, 8, 9)};
    CHECK(RwIm3DTransform(v, 0x10001, nullptr, 0) == nullptr);       // exe: > 0x10000 vertices is an error
    CHECK(RwIm3DEnd() == FALSE);                                      // ...and does not start a transform
    CHECK(RwIm3DTransform(v, 3, nullptr, rwIM3D_VERTEXUV) == v);     // returns the vertex pointer
    CHECK(RwIm3DRenderPrimitive(rwPRIMTYPENAPRIMTYPE) == FALSE && RwIm3DRenderPrimitive(rwPRIMTYPEPOINTLIST) == FALSE); // RW has no Im3D points
    CHECK(RwIm3DEnd() == TRUE && RwIm3DEnd() == FALSE);
    // accessor macros
    CHECK(v[1].color == 0xFF040506u || v[1].color == Argb(4, 5, 6));
    CHECK(RwIm3DVertexGetPos(&v[2])->y == 1.0f && RwIm3DVertexGetPos(&v[2])->z == 1.0f);
    RwIm2DVertex a = V2(3, 4, 10, 20, 30, 40);
    CHECK(RwIm2DVertexGetScreenX(&a) == 3 && RwIm2DVertexGetScreenY(&a) == 4 && RwIm2DVertexGetRecipCameraZ(&a) == 1.0f);
    CHECK(a.emissiveColor == 0x280A141Eu && RwIm2DVertexGetRed(&a) == 10 && RwIm2DVertexGetGreen(&a) == 20 && RwIm2DVertexGetBlue(&a) == 30 && RwIm2DVertexGetAlpha(&a) == 40);
    RwIm2DVertex tri[3] = {a, a, a};
    if (!device) CHECK(RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, tri, 3) == FALSE);   // no D3D9 device
}

static void Im2DTests(IDirect3DDevice9* dev) {
    std::printf("--- Im2D\n");
    CHECK(Near(Argb(1, 2, 3), Argb(1, 2, 4), 1));
    // triangle (pixel colour = vertex colour, flat)
    Clear(); BaseStates();
    const unsigned red = Argb(255, 0, 128);
    RwIm2DVertex t[3] = {V2(10, 10, 255, 0, 128), V2(110, 10, 255, 0, 128), V2(10, 110, 255, 0, 128)};
    CHECK(RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, t, 3) == TRUE);
    Shot s = Capture(dev);
    std::printf("tri: (20,20)=%08X (200,200)=%08X (100,100)=%08X\n", s.at(20, 20), s.at(200, 200), s.at(100, 100));
    CHECK(s.at(20, 20) == red && s.at(50, 50) == red && s.at(200, 200) == kBlack && s.at(100, 100) == kBlack);
    int x0, y0, x1, y1;
    CHECK(s.bbox(kBlack, x0, y0, x1, y1) && std::abs(x0 - 10) <= 1 && std::abs(y0 - 10) <= 1 && std::abs(x1 - 110) <= 1 && std::abs(y1 - 110) <= 1);
    std::printf("tri bbox %d,%d..%d,%d, %d pixels (ideal 5000)\n", x0, y0, x1, y1, s.countNot(kBlack));
    CHECK(std::abs(s.countNot(kBlack) - 5000) < 250);

    // quad: strip, fan, indexed list/strip; same rectangle (150,20)-(230,100), green
    const unsigned green = Argb(0, 200, 0);
    RwIm2DVertex q[4] = {V2(150, 20, 0, 200, 0), V2(230, 20, 0, 200, 0), V2(150, 100, 0, 200, 0), V2(230, 100, 0, 200, 0)};
    Clear();
    CHECK(RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, q, 4) == TRUE);
    s = Capture(dev);
    CHECK(s.at(190, 60) == green && s.at(149, 60) == kBlack && s.at(231, 60) == kBlack && s.at(190, 101) == kBlack);
    CHECK(std::abs(s.count(green) - 80 * 80) < 200);
    RwIm2DVertex f[4] = {q[0], q[1], q[3], q[2]};
    Clear();
    CHECK(RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, f, 4) == TRUE);
    s = Capture(dev);
    CHECK(s.at(190, 60) == green && std::abs(s.count(green) - 6400) < 200);
    RwImVertexIndex il[6] = {0, 1, 2, 2, 1, 3};
    Clear();
    CHECK(RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, q, 4, il, 6) == TRUE);
    s = Capture(dev);
    CHECK(s.at(190, 60) == green && std::abs(s.count(green) - 6400) < 200);
    RwImVertexIndex is[4] = {2, 0, 3, 1};
    Clear();
    CHECK(RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRISTRIP, q, 4, is, 4) == TRUE);
    s = Capture(dev);
    CHECK(s.at(190, 60) == green && std::abs(s.count(green) - 6400) < 200);
    // triangle out of an array
    Clear();
    CHECK(RwIm2DRenderTriangle(q, 4, 0, 1, 2) == TRUE);
    s = Capture(dev);
    CHECK(s.at(160, 30) == green && s.at(220, 90) == kBlack);   // only the upper-left half
    CHECK(RwIm2DRenderTriangle(q, 4, 0, 1, 4) == FALSE);        // index out of the array

    // vertex colour interpolation: black -> white across x
    RwIm2DVertex g[3] = {V2(20, 150, 0, 0, 0), V2(220, 150, 255, 255, 255), V2(20, 200, 0, 0, 0)};
    Clear();
    CHECK(RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, g, 3) == TRUE);
    s = Capture(dev);
    const unsigned mid = s.at(110, 155);
    std::printf("gradient at x=110: %08X\n", mid);
    CHECK(((mid >> 16) & 0xFF) > 90 && ((mid >> 16) & 0xFF) < 170 && ((mid >> 16) & 0xFF) == ((mid >> 8) & 0xFF));

    // lines (RwIm2DRenderLine): a horizontal 1px line at y = 60
    RwIm2DVertex l[2] = {V2(30, 60.5f, 255, 255, 0), V2(130, 60.5f, 255, 255, 0)};
    Clear();
    CHECK(RwIm2DRenderLine(l, 2, 0, 1) == TRUE);
    s = Capture(dev);
    int onLine = 0;
    for (int x = 20; x < 140; x++) for (int y = 58; y <= 62; y++) onLine += s.at(x, y) == Argb(255, 255, 0);
    std::printf("line pixels: %d (ideal 100)\n", onLine);
    CHECK(onLine >= 90 && onLine <= 110 && s.countNot(kBlack) == onLine);
    // polyline
    RwIm2DVertex pl[3] = {V2(30, 30, 255, 255, 0), V2(80, 30, 255, 255, 0), V2(80, 80, 255, 255, 0)};
    Clear();
    CHECK(RwIm2DRenderPrimitive(rwPRIMTYPEPOLYLINE, pl, 3) == TRUE);
    s = Capture(dev);
    CHECK(s.count(Argb(255, 255, 0)) >= 90 && s.at(55, 30) == Argb(255, 255, 0) && s.at(80, 55) == Argb(255, 255, 0));

    // alpha: vertex alpha only counts when the caller enabled vertex alpha (Im2D leaves blending to the caller)
    RwIm2DVertex h[3] = {V2(10, 10, 255, 0, 0, 128), V2(110, 10, 255, 0, 0, 128), V2(10, 110, 255, 0, 0, 128)};
    Clear();
    CHECK(RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, h, 3) == TRUE);
    CHECK(Capture(dev).at(30, 30) == Argb(255, 0, 0));        // blending off: opaque
    Clear();
    Set(rwRENDERSTATEVERTEXALPHAENABLE, 1);
    CHECK(RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, h, 3) == TRUE);
    const unsigned blended = Capture(dev).at(30, 30);
    std::printf("vertex alpha 128 over black: %08X\n", blended);
    CHECK(Near(blended, Argb(128, 0, 0), 2));
    Set(rwRENDERSTATEVERTEXALPHAENABLE, 0);

    // invalid calls
    CHECK(RwIm2DRenderPrimitive(rwPRIMTYPENAPRIMTYPE, t, 3) == FALSE);
    CHECK(RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, nullptr, 3) == FALSE);
    // exe 0x7FBB00/0x7FBD30: a count of 0 primitives is still "drawn" (state set, D3D9 returns S_OK), a negative count is rejected by D3D
    {
        RwD3D9SetFVF(0x42); RwD3D9SetRenderState(D3DRS_CLIPPING, TRUE); RwD3D9SetRenderState(D3DRS_LIGHTING, TRUE); rw::d3d::flushCache();
        CHECK(RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, t, 2) == TRUE);           // not even one triangle: empty draw
        DWORD fvf = 0, clip = 1, light = 1;
        dev->GetFVF(&fvf); dev->GetRenderState(D3DRS_CLIPPING, &clip); dev->GetRenderState(D3DRS_LIGHTING, &light);
        CHECK(fvf == 0x144 && clip == 0 && light == 0);                          // ...but the Im2D state was applied
        CHECK(RwIm2DRenderPrimitive(rwPRIMTYPELINELIST, t, 1) == TRUE);
        CHECK(RwIm2DRenderPrimitive(rwPRIMTYPEPOLYLINE, t, 1) == TRUE);          // n-1 = 0
        CHECK(RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, t, 2) == TRUE);          // n-2 = 0
        CHECK(RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, t, 1) == FALSE);         // n-2 < 0
        CHECK(RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, t, 0) == FALSE);
        CHECK(RwIm2DRenderPrimitive(rwPRIMTYPEPOINTLIST, t, 3) == TRUE);         // count stays 0 in the exe
        CHECK(RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, q, 4, il, 0) == TRUE);
        CHECK(RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, q, 4, il, 2) == TRUE);
        CHECK(RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, q, 4, il, -1) == FALSE);
        CHECK(RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRISTRIP, q, 4, il, 1) == FALSE);
        CHECK(RwIm2DRenderIndexedPrimitive(rwPRIMTYPENAPRIMTYPE, q, 4, il, 3) == FALSE);
        // more than 10000 indices for fewer vertices: the exe has no index buffer big enough -> FALSE (before touching the state)
        RwD3D9SetFVF(0x42); rw::d3d::flushCache();
        std::vector<RwImVertexIndex> many(10002, 0);
        CHECK(RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, q, 4, many.data(), 10002) == FALSE);
        dev->GetFVF(&fvf);
        CHECK(fvf == 0x42);
    }
    // sub-raster offset (exe: raster offsets of the camera's frame buffer are added to x / y of every vertex; 0x7FB650..0x7FBD30)
    {
        RwRaster* fbr = g_cam->frameBuffer;
        const auto ox = fbr->offsetX, oy = fbr->offsetY;
        fbr->offsetX = 7; fbr->offsetY = 3;
        RwIm2DVertex ot[3] = {V2(10, 10, 255, 0, 128), V2(110, 10, 255, 0, 128), V2(10, 110, 255, 0, 128)};
        Clear(); BaseStates();
        CHECK(RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, ot, 3) == TRUE);
        Shot os = Capture(dev);
        int bx0, by0, bx1, by1;
        CHECK(os.bbox(kBlack, bx0, by0, bx1, by1) && std::abs(bx0 - 17) <= 1 && std::abs(by0 - 13) <= 1 && std::abs(bx1 - 117) <= 1 && std::abs(by1 - 113) <= 1);
        CHECK(ot[0].x == 10.0f && ot[0].y == 10.0f);                             // the caller's vertices are not modified
        Clear();
        CHECK(RwIm2DRenderLine(ot, 3, 0, 1) == TRUE);
        os = Capture(dev);
        CHECK(os.bbox(kBlack, bx0, by0, bx1, by1) && std::abs(bx0 - 17) <= 1 && by0 >= 12 && by1 <= 14);
        Clear();
        RwImVertexIndex oi[3] = {0, 1, 2};
        CHECK(RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, ot, 3, oi, 3) == TRUE);
        os = Capture(dev);
        CHECK(os.bbox(kBlack, bx0, by0, bx1, by1) && std::abs(bx0 - 17) <= 1 && std::abs(by0 - 13) <= 1);
        fbr->offsetX = ox; fbr->offsetY = oy;
    }
    // RwIm2DGetNearScreenZ / GetFarScreenZ come from the device z range
    CHECK(RwIm2DGetNearScreenZ() == rw::engine->device.zNear && RwIm2DGetFarScreenZ() == rw::engine->device.zFar);
    std::printf("2D z range %g..%g\n", RwIm2DGetNearScreenZ(), RwIm2DGetFarScreenZ());
}

// camera: frame at the origin looking down +Z, view window 1x1 (90 degrees), near 1, far 100. A vertex (x, y, z) lands at screen
// x = W/2 - x/z * W/2 (RW's view matrix mirrors x), y = H/2 - y/z * H/2.
static void Im3DTests(IDirect3DDevice9* dev, int W, int H) {
    std::printf("--- Im3D (%dx%d)\n", W, H);
    BaseStates();
    const unsigned red = Argb(255, 0, 0);
    const float z = 10.0f, e = 1.0f;     // half-size 1 at depth 10 -> W/20 pixels
    RwIm3DVertex tri[3] = {V3(-e, -e, z, 255, 0, 0), V3(e, -e, z, 255, 0, 0), V3(0, e, z, 255, 0, 0)};

    Clear();
    CHECK(RwIm3DTransform(tri, 3, nullptr, 0) == tri);
    CHECK(RwIm3DRenderPrimitive(rwPRIMTYPETRILIST) == TRUE);
    CHECK(RwIm3DEnd() == TRUE);
    Shot s = Capture(dev);
    int x0, y0, x1, y1;
    const bool any = s.bbox(kBlack, x0, y0, x1, y1);
    std::printf("tri no LTM: bbox %d,%d..%d,%d, %d px (centre %d,%d)\n", x0, y0, x1, y1, s.countNot(kBlack), W / 2, H / 2);
    CHECK(any && s.at(W / 2, H / 2) == red);
    const int baseW = x1 - x0, baseH = y1 - y0, basePx = s.countNot(kBlack);
    CHECK(std::abs(((x0 + x1) / 2) - W / 2) <= 1);
    CHECK(std::abs(baseW - W / 10) <= 2 && std::abs(baseH - H / 10) <= 2);   // 2*e/z * W/2 = W/10

    // LTM: scale 5 about the origin (matrix rows scaled) -> 5x bigger, still centred
    RwMatrix* m = RwMatrixCreate();
    const RwV3d sc{5, 5, 1};
    RwMatrixScale(m, &sc, rwCOMBINEREPLACE);
    RwMatrixUpdate(m);
    Clear();
    CHECK(RwIm3DTransform(tri, 3, m, 0) == tri);
    CHECK(RwIm3DRenderPrimitive(rwPRIMTYPETRILIST) == TRUE);
    RwIm3DEnd();
    s = Capture(dev);
    s.bbox(kBlack, x0, y0, x1, y1);
    std::printf("tri LTM scale 5: bbox %d,%d..%d,%d, %d px\n", x0, y0, x1, y1, s.countNot(kBlack));
    CHECK(std::abs((x1 - x0) - 5 * baseW) <= 4 && std::abs((y1 - y0) - 5 * baseH) <= 4);
    CHECK(std::abs(s.countNot(kBlack) - 25 * basePx) < 25 * basePx / 8);
    // LTM: translate by (+4, 0, 0): moves along the (mirrored) screen x axis; the centre pixel is no longer covered
    RwMatrix* tm = RwMatrixCreate();
    const RwV3d off{4, 0, 0};
    RwMatrixTranslate(tm, &off, rwCOMBINEREPLACE);
    RwMatrixUpdate(tm);
    Clear();
    RwIm3DTransform(tri, 3, tm, 0);
    CHECK(RwIm3DRenderPrimitive(rwPRIMTYPETRILIST) == TRUE);
    RwIm3DEnd();
    s = Capture(dev);
    s.bbox(kBlack, x0, y0, x1, y1);
    const int cx = (x0 + x1) / 2;
    std::printf("tri LTM translate +4 x: bbox centre x=%d (screen centre %d), expected %d\n", cx, W / 2, W / 2 - (int)(4.0f / z * W / 2));
    CHECK(s.at(W / 2, H / 2) == kBlack);
    CHECK(std::abs(cx - (W / 2 - (int)(4.0f / z * W / 2))) <= 2);
    CHECK(std::abs(s.countNot(kBlack) - basePx) <= 8);   // same triangle, different sub-pixel phase
    // after End the transform is gone: LTM pointer is not retained
    CHECK(RwIm3DRenderPrimitive(rwPRIMTYPETRILIST) == FALSE);

    // indexed: a quad from 4 vertices (two triangles), with UV flag and a vertex colour
    RwIm3DVertex q[4] = {V3(-e, -e, z, 0, 255, 0), V3(e, -e, z, 0, 255, 0), V3(-e, e, z, 0, 255, 0), V3(e, e, z, 0, 255, 0)};
    RwImVertexIndex il[6] = {0, 1, 2, 2, 1, 3};
    Clear();
    CHECK(RwIm3DTransform(q, 4, nullptr, rwIM3D_VERTEXUV) == q);
    CHECK(RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, il, 6) == TRUE);
    RwIm3DEnd();
    s = Capture(dev);
    s.bbox(kBlack, x0, y0, x1, y1);
    std::printf("indexed quad: bbox %d,%d..%d,%d, %d px, centre %08X\n", x0, y0, x1, y1, s.countNot(kBlack), s.at(W / 2, H / 2));
    CHECK(s.at(W / 2, H / 2) == Argb(0, 255, 0) && std::abs((x1 - x0) - W / 10) <= 2 && std::abs((y1 - y0) - H / 10) <= 2);
    const int quadPx = s.countNot(kBlack);
    // index count not a multiple of 3 is rounded down: 5 indices = one triangle only
    Clear();
    RwIm3DTransform(q, 4, nullptr, 0);
    CHECK(RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, il, 5) == TRUE);
    RwIm3DEnd();
    s = Capture(dev);
    CHECK(s.countNot(kBlack) < quadPx * 6 / 10 && s.countNot(kBlack) > quadPx * 4 / 10);
    // strip / fan, non-indexed
    Clear();
    RwIm3DVertex strip[4] = {q[0], q[1], q[2], q[3]};
    RwIm3DTransform(strip, 4, nullptr, 0);
    CHECK(RwIm3DRenderPrimitive(rwPRIMTYPETRISTRIP) == TRUE);
    RwIm3DEnd();
    s = Capture(dev);
    CHECK(std::abs(s.countNot(kBlack) - quadPx) <= 4);

    // lines: RenderLine and a linelist; 1px wide
    RwIm3DVertex ln[2] = {V3(-5, 0, z, 255, 255, 0), V3(5, 0, z, 255, 255, 0)};
    Clear();
    RwIm3DTransform(ln, 2, nullptr, 0);
    CHECK(RwIm3DRenderLine(0, 1) == TRUE);
    RwIm3DEnd();
    s = Capture(dev);
    const int linePx = s.countNot(kBlack);
    s.bbox(kBlack, x0, y0, x1, y1);
    std::printf("Im3D line: %d px, bbox %d,%d..%d,%d (ideal width %d)\n", linePx, x0, y0, x1, y1, W / 2);
    CHECK(linePx >= W / 2 - 4 && linePx <= W / 2 + 4 && y1 - y0 <= 1 && std::abs((x1 - x0) - W / 2) <= 3);
    Clear();
    RwIm3DTransform(ln, 2, nullptr, 0);
    CHECK(RwIm3DRenderPrimitive(rwPRIMTYPELINELIST) == TRUE);
    RwIm3DEnd();
    CHECK(Capture(dev).countNot(kBlack) == linePx);
    Clear();
    RwIm3DTransform(tri, 3, nullptr, 0);
    CHECK(RwIm3DRenderTriangle(0, 1, 2) == TRUE);
    RwIm3DEnd();
    CHECK(Capture(dev).countNot(kBlack) == basePx);

    // vertex alpha: without ALLOPAQUE the call enables vertex alpha (blend src alpha) -> half intensity; with ALLOPAQUE it is opaque
    RwIm3DVertex ha[3] = {V3(-e, -e, z, 255, 0, 0, 128), V3(e, -e, z, 255, 0, 0, 128), V3(0, e, z, 255, 0, 0, 128)};
    Clear();
    RwIm3DTransform(ha, 3, nullptr, 0);
    RwIm3DRenderPrimitive(rwPRIMTYPETRILIST);
    RwIm3DEnd();
    unsigned c = Capture(dev).at(W / 2, H / 2);
    std::printf("Im3D alpha 128, flags 0: %08X\n", c);
    CHECK(Near(c, Argb(128, 0, 0), 2));
    Clear();
    RwIm3DTransform(ha, 3, nullptr, rwIM3D_ALLOPAQUE);
    RwIm3DRenderPrimitive(rwPRIMTYPETRILIST);
    RwIm3DEnd();
    c = Capture(dev).at(W / 2, H / 2);
    std::printf("Im3D alpha 128, ALLOPAQUE: %08X\n", c);
    CHECK(c == red);

    // the camera view matrix applies: moving the world with an LTM that rotates 180 degrees about Y mirrors the triangle to behind the camera -> nothing
    RwMatrix* rm = RwMatrixCreate();
    const RwV3d yAxis{0, 1, 0};
    RwMatrixRotate(rm, &yAxis, 180.0f, rwCOMBINEREPLACE);
    RwMatrixUpdate(rm);
    Clear();
    RwIm3DTransform(tri, 3, rm, 0);
    CHECK(RwIm3DRenderPrimitive(rwPRIMTYPETRILIST) == TRUE);
    RwIm3DEnd();
    CHECK(Capture(dev).countNot(kBlack) == 0);
    RwMatrixDestroy(rm); RwMatrixDestroy(tm); RwMatrixDestroy(m);

    // exe 0x7EF450: the vertex count lives in a 16-bit field, 0x10000 vertices become 0 -> nothing is drawn (but the call succeeds)
    {
        std::vector<RwIm3DVertex> big(0x10000, V3(0, 0, z, 255, 0, 0));
        big[0] = tri[0]; big[1] = tri[1]; big[2] = tri[2];
        Clear();
        CHECK(RwIm3DTransform(big.data(), 0x10000, nullptr, 0) == big.data());
        CHECK(RwIm3DRenderPrimitive(rwPRIMTYPETRILIST) == TRUE);
        CHECK(RwIm3DEnd() == TRUE);
        CHECK(Capture(dev).countNot(kBlack) == 0);
        Clear();
        RwIm3DTransform(big.data(), 0xFFFF, nullptr, 0);
        CHECK(RwIm3DRenderPrimitive(rwPRIMTYPETRILIST) == TRUE);
        RwIm3DEnd();
        CHECK(Capture(dev).countNot(kBlack) == basePx);
    }
    // a NULL vertex pointer is no transform in progress (Render / End test the pointer): everything fails
    CHECK(RwIm3DTransform(nullptr, 3, nullptr, 0) == nullptr);
    CHECK(RwIm3DRenderPrimitive(rwPRIMTYPETRILIST) == FALSE && RwIm3DRenderTriangle(0, 1, 2) == FALSE && RwIm3DEnd() == FALSE);
    // an indexed call whose index count rounds down to 0 draws the whole vertex array non-indexed (node 0x80E40C: numIndices == 0 -> 0x80E7C5)
    {
        RwImVertexIndex bad[3] = {2, 2, 2};
        Clear();
        RwIm3DTransform(tri, 3, nullptr, 0);
        CHECK(RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, bad, 2) == TRUE);   // 2 -> 0 indices after rounding
        RwIm3DEnd();
        CHECK(Capture(dev).countNot(kBlack) == basePx);
        Clear();
        RwIm3DTransform(tri, 3, nullptr, 0);
        CHECK(RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, bad, 3) == TRUE);   // degenerate triangle (2,2,2): nothing
        RwIm3DEnd();
        CHECK(Capture(dev).countNot(kBlack) == 0);
        // state set by the node: NOCLIP -> CLIPPING off, otherwise on; lighting / normalize off
        DWORD clip = 7, light = 7, norm = 7;
        RwD3D9SetRenderState(D3DRS_LIGHTING, TRUE); RwD3D9SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
        RwIm3DTransform(tri, 3, nullptr, rwIM3D_NOCLIP);
        RwIm3DRenderPrimitive(rwPRIMTYPETRILIST);
        RwIm3DEnd();
        dev->GetRenderState(D3DRS_CLIPPING, &clip); dev->GetRenderState(D3DRS_LIGHTING, &light); dev->GetRenderState(D3DRS_NORMALIZENORMALS, &norm);
        CHECK(clip == 0 && light == 0 && norm == 0);
        RwIm3DTransform(tri, 3, nullptr, 0);
        RwIm3DRenderPrimitive(rwPRIMTYPETRILIST);
        RwIm3DEnd();
        dev->GetRenderState(D3DRS_CLIPPING, &clip);
        CHECK(clip == 1);
    }

    // Im2D after Im3D still works (FVF / shader / stage state are re-set per call)
    RwIm2DVertex t2[3] = {V2(10, 10, 255, 0, 128), V2(110, 10, 255, 0, 128), V2(10, 110, 255, 0, 128)};
    Clear();
    CHECK(RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, t2, 3) == TRUE);
    CHECK(Capture(dev).at(20, 20) == Argb(255, 0, 128));
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    HWND wnd = CreateWindowA("STATIC", "rw_im2d_im3d_test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(wnd != nullptr);
    rw::MemoryFunctions mf{};   // plain CRT heap, same as the engine layer (engine.cpp)
    mf.rwmalloc  = [](size_t sz, unsigned) -> void* { return sz ? std::malloc(sz) : nullptr; };
    mf.rwrealloc = [](void* p, size_t sz, unsigned) -> void* { return std::realloc(p, sz); };
    mf.rwfree    = [](void* p) { std::free(p); };
    CHECK(rw::Engine::init(&mf));
    rw::EngineOpenParams params{};
    params.window = wnd;
    CHECK(rw::Engine::open(&params));
    rw::Engine::start();
    const bool device = rw::d3d::d3ddevice != nullptr;
    std::printf("D3D9 device: %s\n", device ? "yes" : "NO (device-less run)");
    RwEngineInstance->dOpenDevice.zBufferNear = rw::engine->device.zNear;
    RwEngineInstance->dOpenDevice.zBufferFar  = rw::engine->device.zFar;

    DeviceLess(device);
    if (device) {
        auto* dev = rw::d3d::d3ddevice;
        const int W = rw::d3d::d3d9Globals.present.BackBufferWidth, H = rw::d3d::d3d9Globals.present.BackBufferHeight;
        g_cam = RwCameraCreate();
        RwFrame* f = RwFrameCreate();
        RwCameraSetFrame(g_cam, f);
        RwCameraSetNearClipPlane(g_cam, 1.0f); RwCameraSetFarClipPlane(g_cam, 100.0f);
        RwV2d vw{1.0f, 1.0f};
        RwCameraSetViewWindow(g_cam, &vw);
        RwRaster* fb = rw::Raster::create(W, H, 0, rw::Raster::CAMERA);
        RwRaster* zb = rw::Raster::create(W, H, 0, rw::Raster::ZBUFFER);
        RwCameraSetRaster(g_cam, fb); RwCameraSetZRaster(g_cam, zb);
        f->matrix.setIdentity();
        RwFrameUpdateObjects(f);
        CHECK(RwCameraBeginUpdate(g_cam) == g_cam);
        Im2DTests(dev);
        Im3DTests(dev, W, H);
        RwCameraEndUpdate(g_cam);
        RwCameraSetRaster(g_cam, nullptr); RwCameraSetZRaster(g_cam, nullptr);
        fb->destroy(); zb->destroy();
        RwCameraSetFrame(g_cam, nullptr);
        RwCameraDestroy(g_cam);
        RwFrameDestroy(f);
        rw::Engine::stop();
    }
    rw::Engine::close();
    rw::Engine::term();
    DestroyWindow(wnd);
    std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "PASSED", g_fail);
    return g_fail;
}
