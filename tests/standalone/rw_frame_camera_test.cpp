// P2B-02b unit test: RwFrame* / RwCamera* shim over librw/D3D9.
//  1. frame hierarchy, LTM propagation and dirty flags against hand-computed matrices (row-vector convention: p' = p * M)
//  2. camera: defaults, view window / recip, zScale/zShift, frustum sphere tests
//  3. (needs a D3D9 HAL adapter; works under Wine/wined3d) BeginUpdate / Clear (colour + stencil value) / EndUpdate / ShowRaster
// Exit code 0 = all checks passed.
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

// engine globals (rwglobals.cpp needs the game PCH): only the members frame.cpp / camera.cpp touch
static RwGlobals s_RwGlobals{};
RwGlobals* RwEngineInstance = &s_RwGlobals;
bool RwInitialized = false;

static int g_fail = 0;
#define CHECK(c) do { const bool ok_ = !!(c); std::printf("%-4s %s\n", ok_ ? "ok" : "FAIL", #c); if (!ok_) ++g_fail; } while (0)

static bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }
static bool V3Near(const RwV3d& v, float x, float y, float z, float eps = 1e-4f) { return Near(v.x, x, eps) && Near(v.y, y, eps) && Near(v.z, z, eps); }


// ---- D3D9 helpers (inside a BeginUpdate .. EndUpdate pair)
static unsigned ReadPixel(IDirect3DDevice9* dev, int x, int y) {
    IDirect3DSurface9 *rt = nullptr, *sys = nullptr;
    unsigned px = 0xDEADBEEF;
    if (SUCCEEDED(dev->GetRenderTarget(0, &rt))) {
        D3DSURFACE_DESC d{};
        rt->GetDesc(&d);
        if (SUCCEEDED(dev->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr)) && SUCCEEDED(dev->GetRenderTargetData(rt, sys))) {
            D3DLOCKED_RECT lr{};
            if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
                px = *(unsigned*)((char*)lr.pBits + y * lr.Pitch + x * 4) | 0xFF000000u; // ignore alpha of X8R8G8B8
                sys->UnlockRect();
            }
        }
        if (sys) sys->Release();
        rt->Release();
    }
    return px;
}

// full-viewport quad, fixed function, depth off, stencil test EQUAL to `ref`, no stencil writes
static bool DrawQuadStencilEqual(IDirect3DDevice9* dev, unsigned ref, unsigned argb) {
    struct V { float x, y, z, rhw; DWORD c; } q[4] = {{0, 0, 0.5f, 1, argb}, {4096, 0, 0.5f, 1, argb}, {0, 4096, 0.5f, 1, argb}, {4096, 4096, 0.5f, 1, argb}};
    dev->SetPixelShader(nullptr);
    dev->SetVertexShader(nullptr);
    dev->SetTexture(0, nullptr);
    dev->SetRenderState(D3DRS_ZENABLE, FALSE);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_STENCILENABLE, TRUE);
    dev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
    dev->SetRenderState(D3DRS_STENCILREF, ref);
    dev->SetRenderState(D3DRS_STENCILMASK, 0xFF);
    dev->SetRenderState(D3DRS_STENCILWRITEMASK, 0);
    dev->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
    dev->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
    dev->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
    dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    const bool ok = SUCCEEDED(dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(V)));
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    return ok;
}

static int g_cbCount = 0;
static RwFrame* CountChildCB(RwFrame*, void*) { ++g_cbCount; return nullptr; }          // stops after the first child
static RwFrame* CollectChildCB(RwFrame* f, void* data) { *(*(RwFrame***)data)++ = f; return f; }
static RwObject* CountObjCB(RwObject*, void* data) { ++*(int*)data; return nullptr; }
static RwObject* DetachObjCB(RwObject* o, void*) { rw::ObjectWithFrame* of = (rw::ObjectWithFrame*)o; of->setFrame(nullptr); return o; }

static void FrameTests() {
    std::printf("--- frames\n");
    RwFrame* R = RwFrameCreate();
    RwFrame* A = RwFrameCreate();
    RwFrame* B = RwFrameCreate();
    CHECK(R && A && B && R->root == R);
    CHECK(Near(RwFrameGetLTM(R)->pos.x, 0) && (RwFrameGetLTM(R)->flags & rw::Matrix::IDENTITY));

    // R: translate (10,0,0). A: rotate 90 deg about Z, then translate (0,5,0) (post-concat: p' = p*Rz + (0,5,0)). B: translate (2,0,0).
    const RwV3d tR{10, 0, 0}, tA{0, 5, 0}, tB{2, 0, 0}, zAxis{0, 0, 1};
    CHECK(RwFrameTranslate(R, &tR, rwCOMBINEPOSTCONCAT) == R);
    CHECK(RwFrameRotate(A, &zAxis, 90.0f, rwCOMBINEPOSTCONCAT) == A);
    CHECK(RwFrameTranslate(A, &tA, rwCOMBINEPOSTCONCAT) == A);
    CHECK(RwFrameTranslate(B, &tB, rwCOMBINEPOSTCONCAT) == B);
    CHECK((R->object.privateFlags & rw::Frame::HIERARCHYSYNC) == rw::Frame::HIERARCHYSYNC); // updateObjects marked the (own) root

    CHECK(RwFrameAddChild(R, A) == R);
    CHECK(RwFrameAddChild(A, B) == A);
    CHECK(B->root == R && A->root == R && RwFrameGetParent(B) == A && RwFrameGetParent(A) == R);
    CHECK(RwFrameCount(R) == 3 && RwFrameCount(A) == 2 && RwFrameCount(B) == 1);

    // hand computed: B origin in A = (2,0,0) -> *Rz90 = (0,2,0) -> +(0,5,0) = (0,7,0) -> +(10,0,0) = (10,7,0); axes rotated by 90 deg
    RwMatrix* ltmB = RwFrameGetLTM(B);
    CHECK(ltmB == &B->ltm);
    CHECK(V3Near(ltmB->pos, 10, 7, 0));
    CHECK(V3Near(ltmB->right, 0, 1, 0) && V3Near(ltmB->up, -1, 0, 0) && V3Near(ltmB->at, 0, 0, 1));
    CHECK(V3Near(RwFrameGetLTM(A)->pos, 10, 5, 0));
    CHECK(V3Near(RwFrameGetLTM(R)->pos, 10, 0, 0));
    CHECK((R->object.privateFlags & rw::Frame::HIERARCHYSYNCLTM) == 0); // LTM sync cleared the root's flag

    // dirty propagation: moving the root dirties the whole subtree, LTMs are stale until asked for
    const RwV3d d1{1, 0, 0};
    RwFrameTranslate(R, &d1, rwCOMBINEPOSTCONCAT);
    CHECK(R->object.privateFlags & rw::Frame::HIERARCHYSYNCLTM);
    CHECK(Near(B->ltm.pos.x, 10));                       // not yet synchronised
    CHECK(V3Near(RwFrameGetLTM(B)->pos, 11, 7, 0));      // pulled by GetLTM
    // moving only A: root flagged, only A's subtree recomputed
    RwFrameTranslate(A, &d1, rwCOMBINEPRECONCAT);        // pre-concat: shift in A's own rotated axes -> world (0,1,0)
    CHECK(A->root == R && (R->object.privateFlags & rw::Frame::HIERARCHYSYNC));
    CHECK(V3Near(RwFrameGetLTM(B)->pos, 11, 8, 0));

    // child order (AddChild prepends, as RW) and early stop
    RwFrame* C = RwFrameCreate();
    RwFrameAddChild(R, C);
    RwFrame* order[4] = {}; RwFrame** cursor = order;
    CHECK(RwFrameForAllChildren(R, CollectChildCB, &cursor) == R && order[0] == C && order[1] == A && order[2] == nullptr);
    g_cbCount = 0;
    CHECK(RwFrameForAllChildren(R, CountChildCB, nullptr) == R && g_cbCount == 1);

    // RemoveChild returns the child and makes it its own root again
    CHECK(RwFrameRemoveChild(C) == C && C->root == C && RwFrameGetParent(C) == nullptr && RwFrameCount(R) == 3);
    CHECK(RwFrameRemoveChild(C) == C);                   // no parent: untouched
    RwFrameDestroy(C);

    // moving a subtree: B under R directly
    CHECK(RwFrameAddChild(R, B) == R && RwFrameGetParent(B) == R && RwFrameCount(A) == 1 && B->root == R);
    CHECK(V3Near(RwFrameGetLTM(B)->pos, 13, 0, 0));      // (2,0,0) + R(11,0,0)

    // SetIdentity / Transform / OrthoNormalize
    CHECK(RwFrameSetIdentity(B) == B && V3Near(B->matrix.pos, 0, 0, 0) && (B->matrix.flags & rw::Matrix::IDENTITY));
    CHECK(V3Near(RwFrameGetLTM(B)->pos, 11, 0, 0));
    RwMatrix skew{}; skew.right = {2, 0, 0}; skew.up = {1, 3, 0}; skew.at = {0, 0.5f, 4}; skew.pos = {1, 2, 3}; skew.flags = rw::Matrix::TYPENORMAL;
    CHECK(RwFrameTransform(B, &skew, rwCOMBINEREPLACE) == B);
    CHECK(RwFrameOrthoNormalize(B) == B);
    const RwMatrix& m = B->matrix;
    CHECK(Near(rw::length(m.right), 1) && Near(rw::length(m.up), 1) && Near(rw::length(m.at), 1));
    CHECK(Near(rw::dot(m.right, m.up), 0) && Near(rw::dot(m.right, m.at), 0) && Near(rw::dot(m.up, m.at), 0));
    CHECK(V3Near(m.at, 0, 0.5f / std::sqrt(16.25f), 4.0f / std::sqrt(16.25f)) && V3Near(m.pos, 1, 2, 3));
    CHECK((m.flags & rw::Matrix::TYPEMASK) == rw::Matrix::TYPEORTHONORMAL && !(m.flags & rw::Matrix::IDENTITY));
    CHECK(Near(rw::dot(rw::cross(m.up, m.at), m.right), 1)); // right-handed

    // attached objects: ForAllObjects visits them, stops on null, tolerates detaching the visited object
    RwCamera* cam1 = RwCameraCreate(); RwCamera* cam2 = RwCameraCreate();
    rw::ObjectWithFrame* ow1 = &cam1->object; rw::ObjectWithFrame* ow2 = &cam2->object;
    ow1->setFrame(A); ow2->setFrame(A);
    int n = 0;
    CHECK(RwFrameForAllObjects(A, CountObjCB, &n) == A && n == 1);
    CHECK(RwFrameForAllObjects(A, DetachObjCB, nullptr) == A && RwCameraGetFrame(cam1) == nullptr && RwCameraGetFrame(cam2) == nullptr);
    CHECK(RwCameraDestroy(cam1) && RwCameraDestroy(cam2));

    // clone: every ORIGINAL frame's root field points at its clone; the clone has the same local matrices
    RwFrameAddChild(A, B);
    RwFrame* cloneR = _rwFrameCloneAndLinkClones(R);
    CHECK(cloneR && cloneR != R && R->root == cloneR && RwFrameCount(cloneR) == 3);
    CHECK(V3Near(RwFrameGetLTM(cloneR)->pos, 11, 0, 0) && Near(cloneR->child->matrix.pos.y, A->matrix.pos.y));
    R->purgeClone();   // RW's _rwFramePurgeClone (the game never calls it: it destroys the originals): restores the originals' root fields
    CHECK(R->root == R && A->root == R && B->root == R);
    for (RwFrame* c = cloneR->child; c;) { RwFrame* n = c->child; RwFrameDestroy(c); c = n; }   // clone of A (child: clone of B)
    RwFrameDestroy(cloneR);

    // destroying a middle frame orphans (does not destroy) its children and gives them a valid root
    RwFrame* AA = A->child;
    RwFrame* Aorig = A;
    CHECK(RwFrameDestroy(Aorig) == TRUE);
    CHECK(AA == B && RwFrameGetParent(B) == nullptr && B->root == B && RwFrameCount(R) == 1);
    RwFrameUpdateObjects(B);                              // must not touch the freed frame
    CHECK(V3Near(RwFrameGetLTM(B)->pos, 1, 2, 3));
    RwFrameDestroy(B); RwFrameDestroy(R);
    CHECK(rw::Frame::numAllocated == 0);
}

static void CameraTests(bool device) {
    std::printf("--- camera (device %s)\n", device ? "yes" : "no");
    RwCamera* cam = RwCameraCreate();
    CHECK(cam != nullptr);
    CHECK(cam->nearPlane == 0.05f && cam->farPlane == 10.0f && cam->projection == rwPERSPECTIVE && cam->viewWindow.x == 1.0f);
    RwFrame* f = RwFrameCreate();
    CHECK(RwCameraSetFrame(cam, f) == cam && RwCameraGetFrame(cam) == f);

    CHECK(RwCameraSetNearClipPlane(cam, 1.0f) == cam && RwCameraSetFarClipPlane(cam, 100.0f) == cam);
    CHECK(RwCameraGetNearClipPlane(cam) == 1.0f && RwCameraGetFarClipPlane(cam) == 100.0f);
    const float N = rw::engine->device.zNear, F = rw::engine->device.zFar;
    std::printf("device z range %g..%g, zScale %g zShift %g\n", N, F, cam->zScale, cam->zShift);
    CHECK(Near(cam->zShift, 100.0f / 99.0f, 2e-3f) && Near(cam->zScale, -100.0f / 99.0f, 2e-3f)); // N=0,F=1 -> f/(f-n), -n*f/(f-n)
    RwV2d vw{0.5f, 0.25f};
    CHECK(RwCameraSetViewWindow(cam, &vw) == cam && cam->viewWindow.x == 0.5f && cam->viewWindow.y == 0.25f);
    const RwV2d recip = RwCameraGetRecipViewWindow(cam);
    CHECK(Near(recip.x, 2.0f) && Near(recip.y, 4.0f));
    RwV2d zero{0, 0};
    CHECK(RwCameraSetViewWindow(cam, &zero) == nullptr && cam->viewWindow.x == 0.5f);
    vw = {1.0f, 1.0f};
    RwCameraSetViewWindow(cam, &vw);                       // 90 degrees both ways
    CHECK(RwCameraSetProjection(cam, rwPARALLEL) == cam && cam->projection == rwPARALLEL);
    CHECK(RwCameraSetProjection(cam, rwPERSPECTIVE) == cam && cam->projection == rwPERSPECTIVE);
    CHECK(RwCameraSetProjection(cam, (RwCameraProjection)7) == nullptr);

    // frame at the origin looks along +Z (identity LTM). Planes are built when the camera syncs (BeginUpdate / syncDirty).
    rw::Frame::syncDirty();
    CHECK(cam->frustumPlanes[0].plane.normal.z > 0.99f && Near(cam->frustumPlanes[0].plane.distance, 100.0f)); // far, pointing +Z
    CHECK(cam->frustumPlanes[1].plane.normal.z < -0.99f && Near(cam->frustumPlanes[1].plane.distance, -1.0f));  // near
    std::printf("planes: right (%.3f %.3f %.3f) top (%.3f %.3f %.3f) left (%.3f %.3f %.3f) bottom (%.3f %.3f %.3f)\n",
                cam->frustumPlanes[2].plane.normal.x, cam->frustumPlanes[2].plane.normal.y, cam->frustumPlanes[2].plane.normal.z,
                cam->frustumPlanes[3].plane.normal.x, cam->frustumPlanes[3].plane.normal.y, cam->frustumPlanes[3].plane.normal.z,
                cam->frustumPlanes[4].plane.normal.x, cam->frustumPlanes[4].plane.normal.y, cam->frustumPlanes[4].plane.normal.z,
                cam->frustumPlanes[5].plane.normal.x, cam->frustumPlanes[5].plane.normal.y, cam->frustumPlanes[5].plane.normal.z);
    const float s2 = 0.70710678f;
    // RW's camera X axis is mirrored (the view matrix negates x): plane [2] ("right") bounds the world -X side, [4] the +X side
    CHECK(Near(cam->frustumPlanes[2].plane.normal.x, -s2) && Near(cam->frustumPlanes[2].plane.normal.z, -s2)); // normals point out of the frustum
    CHECK(Near(cam->frustumPlanes[4].plane.normal.x, s2) && Near(cam->frustumPlanes[4].plane.normal.z, -s2));
    CHECK(Near(cam->frustumPlanes[3].plane.normal.y, s2) || Near(cam->frustumPlanes[3].plane.normal.y, -s2)); // top / bottom are +-45 degrees
    struct { RwSphere s; RwFrustumTestResult expect; const char* what; } cases[] = {
        {{{0, 0, 50}, 1},      rwSPHEREINSIDE,   "centre of the view, mid range"},
        {{{0, 0, 200}, 1},     rwSPHEREOUTSIDE,  "behind the far plane"},
        {{{0, 0, 100}, 5},     rwSPHEREBOUNDARY, "straddling the far plane"},
        {{{0, 0, -10}, 1},     rwSPHEREOUTSIDE,  "behind the camera"},
        {{{0, 0, 0.5f}, 2},    rwSPHEREBOUNDARY, "straddling the near plane"},
        {{{60, 0, 50}, 1},     rwSPHEREOUTSIDE,  "right of the frustum (x > z)"},
        {{{50, 0, 50}, 1},     rwSPHEREBOUNDARY, "on the right plane"},
        {{{-60, 0, 50}, 1},    rwSPHEREOUTSIDE,  "left of the frustum"},
        {{{0, 60, 50}, 1},     rwSPHEREOUTSIDE,  "above"},
        {{{0, -60, 50}, 1},    rwSPHEREOUTSIDE,  "below"},
        {{{20, -20, 50}, 1},   rwSPHEREINSIDE,   "off axis, inside"},
        {{{0, 0, 50}, 500},    rwSPHEREBOUNDARY, "huge sphere around the frustum"},
    };
    for (auto& c : cases) {
        const RwFrustumTestResult r = RwCameraFrustumTestSphere(cam, &c.s);
        std::printf("  sphere %-34s -> %d (expect %d)\n", c.what, (int)r, (int)c.expect);
        CHECK(r == c.expect);
    }
    // moving the camera: world sphere at (0,0,50) is inside for a camera at the origin, outside for one moved 200 units to +X
    const RwV3d mv{200, 0, 0};
    RwFrameTranslate(f, &mv, rwCOMBINEPOSTCONCAT);
    rw::Frame::syncDirty();
    const RwSphere s0{{0, 0, 50}, 1}, s1{{200, 0, 50}, 1};
    CHECK(RwCameraFrustumTestSphere(cam, &s0) == rwSPHEREOUTSIDE && RwCameraFrustumTestSphere(cam, &s1) == rwSPHEREINSIDE);

    if (device) {
        std::printf("--- camera on the D3D9 device\n");
        RwRaster* fb = rw::Raster::create(rw::d3d::d3d9Globals.present.BackBufferWidth, rw::d3d::d3d9Globals.present.BackBufferHeight, 0, rw::Raster::CAMERA);
        RwRaster* zb = rw::Raster::create(rw::d3d::d3d9Globals.present.BackBufferWidth, rw::d3d::d3d9Globals.present.BackBufferHeight, 0, rw::Raster::ZBUFFER);
        CHECK(fb && zb);
        RwCameraSetRaster(cam, fb); RwCameraSetZRaster(cam, zb);
        f->matrix.setIdentity();                 // camera back at the origin
        RwFrameUpdateObjects(f);
        auto* dev = rw::d3d::d3ddevice;
        const bool hasStencil = rw::d3d::d3d9Globals.present.AutoDepthStencilFormat == D3DFMT_D24S8;
        std::printf("depth format %d, stencil %s\n", (int)rw::d3d::d3d9Globals.present.AutoDepthStencilFormat, hasStencil ? "yes" : "no");

        CHECK(RwEngineInstance->curCamera == nullptr);
        const RwUInt16 rf = RwEngineInstance->renderFrame;
        CHECK(RwCameraBeginUpdate(cam) == cam && RwEngineInstance->curCamera == cam && RwEngineInstance->renderFrame == rf);   // exe: only the world render callback bumps it
        D3DMATRIX v{}, p{};
        dev->GetTransform(D3DTS_VIEW, &v); dev->GetTransform(D3DTS_PROJECTION, &p);
        // camera at the origin, identity LTM: view = identity with x negated; projection = diag(1/vw), m33 = f/(f-n), m34 = 1, m43 = -n*f/(f-n)
        std::printf("view diag %g %g %g, proj m11 %g m22 %g m33 %g m34 %g m43 %g\n", v._11, v._22, v._33, p._11, p._22, p._33, p._34, p._43);
        CHECK(Near(v._11, -1) && Near(v._22, 1) && Near(v._33, 1) && Near(v._41, 0) && Near(v._44, 1));
        CHECK(Near(p._11, 1) && Near(p._22, 1) && Near(p._33, 100.0f / 99.0f) && Near(p._34, 1) && Near(p._43, -100.0f / 99.0f) && Near(p._44, 0));

        std::printf("fb %dx%d off %d,%d type %d\n", fb->width, fb->height, fb->offsetX, fb->offsetY, fb->type);
        { HRESULT hr = dev->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF00FF00, 1.0f, 0); std::printf("direct Clear hr=%08lX pixel %08X\n", (unsigned long)hr, ReadPixel(dev, 8, 8)); }
        // colour clear: read the render target back
        RwRGBA red{255, 0, 0, 255};
        CHECK(RwCameraClear(cam, &red, rwCAMERACLEARIMAGE | rwCAMERACLEARZ) == cam);
        std::printf("pixel after red clear: %08X\n", ReadPixel(dev, 8, 8));
        CHECK(ReadPixel(dev, 8, 8) == 0xFFFF0000u);

        // stencil clear value: clear stencil to 0x5A (colour black), then draw a quad that passes only where stencil == 0x5A
        if (hasStencil) {
            RwD3D9SetStencilClear(0x5A);
            RwRGBA black{0, 0, 0, 255};
            CHECK(RwCameraClear(cam, &black, rwCAMERACLEARIMAGE | rwCAMERACLEARZ | rwCAMERACLEARSTENCIL) == cam);
            CHECK(DrawQuadStencilEqual(dev, 0x5A, 0xFF00FF00u));   // green where stencil == 0x5A: must appear
            CHECK(ReadPixel(dev, 8, 8) == 0xFF00FF00u);
            RwD3D9SetStencilClear(0);
            CHECK(RwCameraClear(cam, &black, rwCAMERACLEARIMAGE | rwCAMERACLEARZ | rwCAMERACLEARSTENCIL) == cam);
            CHECK(DrawQuadStencilEqual(dev, 0x5A, 0xFF0000FFu));   // stencil is 0 now: blue must NOT appear
            CHECK(ReadPixel(dev, 8, 8) == 0xFF000000u);
            CHECK(DrawQuadStencilEqual(dev, 0x00, 0xFF0000FFu));   // ...but a test against 0 passes
            CHECK(ReadPixel(dev, 8, 8) == 0xFF0000FFu);
        }
        RwCameraEndUpdate(cam);
        CHECK(RwEngineInstance->curCamera == nullptr);
        CHECK(RwCameraShowRaster(cam, nullptr, 0) == cam);
        CHECK(RwCameraClear(cam, &red, rwCAMERACLEARIMAGE) == cam);   // outside Begin/EndUpdate is allowed too
        CHECK(RwD3D9CameraAttachWindow(cam, nullptr) == TRUE);
        RwCameraSetRaster(cam, nullptr); RwCameraSetZRaster(cam, nullptr);
        fb->destroy(); zb->destroy();
    }
    RwCameraSetFrame(cam, nullptr);
    CHECK(RwCameraDestroy(cam) == TRUE);
    RwFrameDestroy(f);
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    HWND wnd = CreateWindowA("STATIC", "rw_frame_camera_test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(wnd != nullptr);
    rw::MemoryFunctions mf{};   // plain CRT heap, same as the engine layer (engine.cpp)
    mf.rwmalloc  = [](size_t sz, unsigned) -> void* { return sz ? std::malloc(sz) : nullptr; };
    mf.rwrealloc = [](void* p, size_t sz, unsigned) -> void* { return std::realloc(p, sz); };
    mf.rwfree    = [](void* p) { std::free(p); };
    CHECK(rw::Engine::init(&mf));
    rw::EngineOpenParams params{};
    params.window = wnd;
    CHECK(rw::Engine::open(&params));
    const bool opened = rw::d3d::d3d9Globals.d3d9 != nullptr;
    std::printf("D3D9 object: %s\n", opened ? "yes" : "NO");
    rw::Engine::start();
    const bool device = rw::d3d::d3ddevice != nullptr;
    std::printf("D3D9 device: %s\n", device ? "yes" : "NO (device-less run)");
    RwEngineInstance->dOpenDevice.zBufferNear = 0; RwEngineInstance->dOpenDevice.zBufferFar = 1;

    FrameTests();
    CameraTests(device);

    if (device) rw::Engine::stop();
    rw::Engine::close();
    rw::Engine::term();
    DestroyWindow(wnd);
    std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "PASSED", g_fail);
    return g_fail;
}
