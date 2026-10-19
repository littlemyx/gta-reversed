// ORACLE-W3-ENTITY rows D: CEntity::IsEntityOccluded (active occluders + camera view matrix in the exe image) and the early-out branches of SetupLighting / RemoveLighting.
// The occluder fixture is built around the entity: lines that contain the entity's screen centre, faces whose planes separate camera and entity (so the hit rate is not ~0).
#include "Occlusion.h"
#include "ActiveOccluder.h"
#include "Camera.h"
#include "Draw.h"
#include "common.h"

namespace w3e {
static void OccluderState(Rng& r) {
    // camera: position in the placement (no matrix), view matrix = rotation + translation chosen so that the entity's bound centre lands at depth z
    auto& cam = static_cast<CPlaceable&>(TheCamera);
    *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(&cam) + offsetof(CPlaceable, m_matrix)) = nullptr;
    const CVector camPos(Fl(r, 3000.f, 30), Fl(r, 3000.f, 30), Fl(r, 100.f, 30));
    cam.m_placement.m_vPosn = camPos;
    RsGlobal.maximumWidth = r.below(2) ? 640 : (int32)(320 + r.below(1600)); RsGlobal.maximumHeight = r.below(2) ? 448 : (int32)(240 + r.below(1000));
    CDraw::ms_fFOV = r.below(6) ? 45.f + r.f01() * 40.f : Fl(r, 100.f, 2);
    uint8_t m[0x50]; WireMatrixBody(r, m);
    auto& V = *reinterpret_cast<CMatrix*>(m);
    const CVector centre = EntA()->CEntity::GetBoundCentre();
    const float depth = r.below(8) == 0 ? (r.f01() - 0.2f) * 4.f : 2.f + r.f01() * 250.f;
    const CVector target(Fl(r, 40.f, 30), Fl(r, 40.f, 30), depth);
    const CVector rc = V.TransformVector(centre);
    V.GetPosition() = target - rc;
    TheCamera.GetViewMatrix() = V;
    // occluders
    COcclusion::NumActiveOccluders = r.below(6) == 0 ? 0 : 1 + r.below(4);
    CVector scr; const bool onScreen = CalcScreenCoors(centre, scr);
    if (!onScreen) { scr = CVector(Fl(r, 300.f, 30), Fl(r, 300.f, 30), depth); }
    for (size_t i = 0; i < (size_t)COcclusion::NumActiveOccluders; ++i) {
        auto& o = COcclusion::ActiveOccluders[i];
        FillGarbage(r, &o, sizeof o);
        o.m_DistToCam = (uint16)(r.below(4) ? r.below((uint32)std::clamp(std::isfinite(scr.z) ? scr.z : 1.f, 1.f, 1000.f)) : r.below(400));
        o.m_LinesUsed = (uint8)r.below(7);
        o.m_NumFaces = (uint8)(r.below(10) == 0 ? 0 : 1 + r.below(3));
        for (auto& l : o.m_Lines) {   // a line whose "inside" half-plane contains the screen centre: origin behind the centre on the perpendicular
            const float a = r.f01() * 6.2831853f; const CVector2D dir(std::cos(a), std::sin(a));
            const float off = r.below(5) ? 30.f + r.f01() * 300.f : -20.f * r.f01();
            l.Dir = dir; l.Origin = CVector2D(scr.x, scr.y) + CVector2D(dir.y, -dir.x) * -off * (r.below(6) ? 1.f : -1.f); l.Length = r.f01() * 400.f;
        }
        for (size_t f = 0; f < 3; ++f) {   // a plane that separates the camera from the entity centre
            const CVector n = CVector(Fl(r, 1.f, 40), Fl(r, 1.f, 40), Fl(r, 1.f, 40));
            const CVector mid = (camPos + centre) * 0.5f;
            o.m_FaceNormals[f] = n; o.m_FaceOffsets[f] = n.Dot(mid) + (r.below(5) ? 0.f : Fl(r, 5.f, 10));
        }
    }
}
} // namespace w3e

static void TestW3EntityD() {
    using namespace w2; using namespace w3e;
    ROW("CEntity::IsEntityOccluded", 0x71FAE0, 3000) {
        Tw tw([](Rng& rr) { OccluderState(rr); });
        const bool ok = ECase(r, d, [&]() -> uint64_t { return B8(EntA()->CEntity::IsEntityOccluded()); }, [&]() -> uint64_t { return ExeT<uint8_t>(0x71FAE0, EntA()); });
        if (!ok) { char t[200]; std::snprintf(t, sizeof t, " nocc %d hasMatrix %d type %d bbox %g %g %g / %g %g %g", (int)COcclusion::NumActiveOccluders, EntA()->m_matrix != nullptr, (int)EntA()->GetType(), 0.0, 0.0, 0.0, 0.0, 0.0, 0.0); d += t; }
        return ok;
    } ENDROW;
    // the occluder helpers IsEntityOccluded calls (game_sa/ActiveOccluder.cpp, common.cpp), same fixture
    ROW("CActiveOccluder::IsPointWithinOcclusionArea", 0x71E580, 2000) {
        const CVector2D pt(Fl(r, 400.f, 12), Fl(r, 300.f, 12)); const float rad = r.below(2) ? 0.f : (r.below(8) ? r.f01() * 100.f : Fl(r, 100.f, 1));
        Tw tw([](Rng& rr) { OccluderState(rr); if (COcclusion::NumActiveOccluders == 0) COcclusion::NumActiveOccluders = 1; });
        EC(return B8(COcclusion::ActiveOccluders[0].IsPointWithinOcclusionArea(pt, rad));, return ExeT<uint8_t>(0x71E580, &COcclusion::ActiveOccluders[0], pt, rad););
    } ENDROW;
    ROW("CActiveOccluder::IsPointBehindOccluder", 0x71FA40, 2000) {
        const CVector pt(Fl(r, 3000.f, 12), Fl(r, 3000.f, 12), Fl(r, 100.f, 12)); const float rad = r.below(2) ? 0.f : (r.below(8) ? r.f01() * 40.f : Fl(r, 40.f, 1));
        Tw tw([](Rng& rr) { OccluderState(rr); if (COcclusion::NumActiveOccluders == 0) COcclusion::NumActiveOccluders = 1; });
        EC(return B8(COcclusion::ActiveOccluders[0].IsPointBehindOccluder(pt, rad));, return ExeT<uint8_t>(0x71FA40, &COcclusion::ActiveOccluders[0], pt, rad););
    } ENDROW;
    ROW("::IsPointInsideLine", 0x71E050, 2000) {
        const CVector2D org(Fl(r, 400.f, 12), Fl(r, 300.f, 12)); const float a = r.f01() * 6.2831853f; const CVector2D dir = r.below(8) ? CVector2D(std::cos(a), std::sin(a)) : CVector2D(Fl(r, 2.f, 3), Fl(r, 2.f, 3));
        const CVector2D pt(org.x + Fl(r, 100.f, 12), org.y + Fl(r, 100.f, 12)); const float rad = r.below(2) ? 0.f : (r.below(8) ? r.f01() * 50.f : Fl(r, 50.f, 1));
        EC(return B8(IsPointInsideLine(org, dir, pt, rad));, return ExeC<uint8_t>(0x71E050, org, dir, pt, rad););
    } ENDROW;
    // SetupLighting / RemoveLighting call the RenderWare light API (exe code vs librw): only the branch that does not reach it is compared (m_bLightObject clear / reset == false)
    ROW("CEntity::SetupLighting (not a light object)", 0x553DC0, 100) {
        Tw tw([](Rng&) { EntA()->m_bLightObject = false; });
        EC(return B8(EntA()->CEntity::SetupLighting());, return ExeT<uint8_t>(0x553DC0, EntA()););
    } ENDROW;
    ROW("CEntity::RemoveLighting (reset false)", 0x553370, 100) {
        EC(EntA()->CEntity::RemoveLighting(false); return 0;, ExeT<void>(0x553370, EntA(), 0); return 0;);
    } ENDROW;
}
