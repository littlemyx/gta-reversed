#include "StdInc.h"

#include "3dMarker.h"
#include "3dMarkers.h"

#include <reversiblebugfixes/Bugs.hpp>

void C3dMarker::InjectHooks() {
    RH_ScopedClass(C3dMarker);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(AddMarker, 0x722230);
    RH_ScopedInstall(DeleteMarkerObject, 0x722390, { .Locked = true }); // The original leaves the matrix attached to the destroyed frame (#1514)
    RH_ScopedInstall(IsZCoordinateUpToDate, 0x7226A0);
    RH_ScopedInstall(Render, 0x7223D0);
    RH_ScopedInstall(SetZCoordinateIfNotUpToDate, 0x724E10);
    RH_ScopedInstall(UpdateZCoordinate, 0x724D40);
}

RpAtomic* GetAtomicForMarkerType(e3dMarkerType type) {
    return GetFirstAtomic(
        [type]{
            switch (type) {
            case MARKER3D_TUBE:
                return C3dMarkers::m_pRpClumpArray[1]; // black cylinder
            case MARKER3D_ARROW2:
                return C3dMarkers::m_pRpClumpArray[0]; // diamond_3
            default:
                return C3dMarkers::m_pRpClumpArray[type];
            }
        }()
    );
}

// uint8 r, uint8 g, uint8 b, uint8 a -> CRGBA
// 0x722230
bool C3dMarker::AddMarker(uint32 id, e3dMarkerType type, float size, uint8 red, uint8 green, uint8 blue, uint8 alpha, uint16 pulsePeriod, float pulseFraction, int16 rotateRate) {
    const CRGBA color = { red, green, blue, alpha };

    m_Atomic = RpAtomicClone(GetAtomicForMarkerType(type));
    assert(m_Atomic);

    const auto frame = RwFrameCreate();
    const auto geo   = RpAtomicGetGeometry(m_Atomic);

    RpAtomicSetFrame(m_Atomic, frame);
    CVisibilityPlugins::SetAtomicRenderCallback(m_Atomic, nullptr);

    RpGeometrySetFlags(geo, RpGeometryGetFlags(geo) | rpGEOMETRYMODULATEMATERIALCOLOR);

    m_Mat.SetUnity();
    m_Mat.Attach(RwFrameGetMatrix(frame), false);

    m_ID               = id;
    m_Material         = RpGeometryGetMaterial(geo, 0);
    m_Size             = size;
    m_StdSize          = size;
    m_Color            = color;
    m_PulsePeriod      = pulsePeriod;
    m_PulseFraction    = pulseFraction;
    m_RotateRate       = rotateRate;
    m_StartTime        = CTimer::GetTimeInMS();
    m_OnScreenTestTime = CTimer::GetTimeInMS();
    m_LastPosition     = CVector{};
    m_Type             = type;
    m_RoofHeight       = 65535.0f;

    return m_Atomic != nullptr;
}

// 0x722390
void C3dMarker::DeleteMarkerObject() {
    m_ID        = 0;
    m_StartTime = 0;
    m_IsInUse   = false;
    m_IsActive  = false;
    m_Type      = MARKER3D_NA;

    // Destroy RW object
    const auto frame = RpAtomicGetFrame(m_Atomic);
    RpAtomicDestroy(m_Atomic);
#ifdef NOTSA_STANDALONE_RUN
    // NOTSA-AUDIT: no `m_Mat.Detach()` in the run build, like the exe (0x722390): the matrix stays attached to the destroyed frame. The exe's `C3dMarkers::Init` does not
    // touch the matrix (it resets the other fields one by one), and `AddMarker` re-attaches before the next use; the port's Init is ported from the asm now
    // (it used to `rng::fill(.., C3dMarker{})` = CMatrix::operator= through the stale pointer into the freed frame, which this Detach worked around).
#else
#line 85
    if (notsa::bugfixes::GenericCrashing) {
        // (the exe has no such call)
        m_Mat.Detach(); // FIX(#1514): Otherwise `C3dMarkers::Init` writes into the destroyed frame's matrix
    }
#endif
#line 85
    RwFrameDestroy(frame);
    m_Atomic = nullptr;
}

// 0x7226A0
bool C3dMarker::IsZCoordinateUpToDate() {
    const auto& pos = m_Mat.GetPosition();
    return m_LastMapReadX == NOTSA_FTOL(uint16, pos.x)
        && m_LastMapReadY == NOTSA_FTOL(uint16, pos.y);
}

// 0x7223D0
void C3dMarker::Render() {
    if (!m_Atomic) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(m_Type == MARKER3D_CYLINDER ? rwCULLMODECULLNONE : rwCULLMODECULLBACK));

    m_Mat.UpdateRW();

    auto transform = CMatrix{ m_Mat.m_pAttachMatrix };
    if (m_Type == MARKER3D_TORUS || m_Type == MARKER3D_ARROW2) {
        if (m_Normal != CVector{ 0.0f, 0.0f, 1.0f }) {
            // 0x72247E..0x7224F6: right = normalise(Z x normal), forward = normalise(normal x right), up = normal
            transform *= CMatrix::WithUp(m_Normal);
        }
    }

    // 0x722570..0x7225AA: Scale(size) for every type, then 0x5A2E60 (1, 1, 20) on top for the tube (not size * 20 in one step)
    transform.Scale(m_Size);
    if (m_Type == MARKER3D_TUBE) {
        // 0x5A2E60 scales the x/y/z COMPONENT of each of the three rows (x*1, y*1, z*20), not the axis vectors
        transform.GetRight().z   *= 20.0f;
        transform.GetForward().z *= 20.0f;
        transform.GetUp().z      *= 20.0f;
    }

    transform.UpdateRW();

    RwFrameUpdateObjects(RpAtomicGetFrame(m_Atomic));

    switch (m_Type) {
    case MARKER3D_ARROW:
    case MARKER3D_CONE:
    case MARKER3D_CONE_NO_COLLISION:
        m_Color.a = 255;
        break;
    }

    // TODO: What the fuck? I dont' think this does what they intended it to.
    NOTSA_GLOBAL_LOCAL(s_SavedAlpha, 0x8D5E44, (uint8), { 255 });
    if (m_Color.a == s_SavedAlpha) {
        m_Color.a += m_Color.a < 128u ? 1 : -1; // But then again, this doesn't make a lot more sense either
    }
    s_SavedAlpha = m_Color.a;

    const auto color = m_Color.ToRwRGBA();
    RpMaterialSetColor(m_Material, &color);

    SetBrightMarkerColours(m_Brightness);

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(FALSE));

    if (m_Type == MARKER3D_ARROW2)
        RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(rwCULLMODECULLNONE));

    // And now, render the atomic
    RpAtomicRender(m_Atomic);

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(TRUE));

    ReSetAmbientAndDirectionalColours();
}

// 0x724E10
void C3dMarker::SetZCoordinateIfNotUpToDate(float coordinate) {
    if (!IsZCoordinateUpToDate()) {
        m_Mat.GetPosition().z = coordinate;
    }
}

// 0x724D40
void C3dMarker::UpdateZCoordinate(CVector point, float zDistance) {
    if (IsZCoordinateUpToDate()) {
        return;
    }

    auto& pos = m_Mat.GetPosition();

    // 0x724D75: `fcomp 10000; test ah, 5; jp` - proceeds only for dist^2 < 10000 (a NaN distance returns)
    if (!(DistanceBetweenPointsSquared2D(pos, point) < sq(100.0f))) {
        return;
    }

    if (!CColStore::HasCollisionLoaded(pos, AREA_CODE_NORMAL_WORLD)) {
        return;
    }

    bool found{};
    auto height = CWorld::FindGroundZFor3DCoord({ pos.x, pos.y, pos.z + 1.0f }, &found, nullptr);
    if (found) {
        pos.z = height - zDistance * ExeRecip(20.0f);
    }

    m_LastMapReadX = NOTSA_FTOL(uint16, pos.x);
    m_LastMapReadY = NOTSA_FTOL(uint16, pos.y);
}

void C3dMarker::DeleteIfHasAtomic() {
    if (m_Atomic) {
        DeleteMarkerObject();
    }
}
