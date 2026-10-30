#include "StdInc.h"

#include "3dMarkers.h"
#include "PostEffects.h"

// RwIm3DVertex StreakVertices[4];
#ifndef NOTSA_GLOBALS_DETACHED // unused pointer view of Boat.cpp's KeepWaterOutVertices (0xC278F8); gone in detached mode (kept in address mode: object code unchanged)
auto& aRenderVertices = StaticRef<RxObjSpace3DVertex*>(0xC278F8);
#endif
#line 8

void C3dMarkers::InjectHooks() {
    RH_ScopedClass(C3dMarkers);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x724E40);
    RH_ScopedInstall(Update, 0x7227B0);
    RH_ScopedInstall(Shutdown, 0x722710, { .Locked = true }); // FIX(#1514): Vanilla one inlines `C3dMarker::DeleteMarkerObject`
    RH_ScopedInstall(Render, 0x725040, { .Locked = true }); // FIX(#1514): Vanilla one inlines `C3dMarker::DeleteMarkerObject`
    RH_ScopedInstall(DirectionArrowFindFirstFreeSlot, 0x721120);
    RH_ScopedInstall(DirectionArrowSet, 0x721140);
    RH_ScopedInstall(DirectionArrowsDraw, 0x7211F0);
    RH_ScopedInstall(DirectionArrowsInit, 0x721100);
    RH_ScopedInstall(ForceRender, 0x722870);
    RH_ScopedInstall(PlaceMarker, 0x725120);
    RH_ScopedInstall(PlaceMarkerCone, 0x726D40);
    RH_ScopedInstall(PlaceMarkerSet, 0x725BA0);
    RH_ScopedInstall(User3dMarkerAtomicCB, 0x7210D0);
    RH_ScopedInstall(User3dMarkerDelete, 0x721090);
    RH_ScopedInstall(User3dMarkerDeleteAll, 0x7210B0);
    RH_ScopedInstall(User3dMarkerFindFirstFreeSlot, 0x720FB0);
    RH_ScopedInstall(User3dMarkerSet, 0x720FD0);
    RH_ScopedInstall(User3dMarkersDraw, 0x723240);
    RH_ScopedInstall(LoadUser3dMarkers, 0x5D42E0);
    RH_ScopedInstall(SaveUser3dMarkers, 0x5D4300);
}

#ifdef NOTSA_STANDALONE_RUN
namespace notsa::audit {
// NOTSA-AUDIT: the marker reset of `C3dMarkers::Init` (0x724E40: 32x the inlined C3dMarker::C3dMarker, stores at marker+0x48..+0x98), a free function so that the exe oracle
// (tests/standalone/logic_oracle_notsa.inc) can run it on a poisoned copy of the array and compare it byte for byte with the exe's own loop.
// The exe resets the markers FIELD BY FIELD and never touches
//   * `m_Mat` (+0x00..+0x47: a marker keeps its matrix, including `m_pAttachMatrix`, which is stale after `DeleteMarkerObject` destroyed the frame - nothing
//     reads it before `AddMarker` re-attaches),
//   * `m_LastMapReadY` (+0x86), `m_LastMapReadResultZ` (+0x88), `m_OnScreenTestTime` (+0x9C).
// `rng::fill(.., C3dMarker{})` (the DLL build) is a `CMatrix::operator=` per marker: it wrote through the stale attach pointer into the freed RwFrame (heap corruption at
// every game restart after a marker existed) - the reason the run build used to keep a `m_Mat.Detach()` in `DeleteMarkerObject` (removed: the exe has none).
// Note `m_Normal` is (0, 0, 1) here (+0x80 = 1.0f), not zero as in the DLL build.
void ResetMarkers3dLikeExe(C3dMarker* markers, size_t count) {
    VALIDATE_OFFSET(C3dMarker, m_Atomic, 0x48); VALIDATE_OFFSET(C3dMarker, m_Type, 0x50); VALIDATE_OFFSET(C3dMarker, m_IsInUse, 0x52); VALIDATE_OFFSET(C3dMarker, m_IsActive, 0x53); VALIDATE_OFFSET(C3dMarker, m_ID, 0x54); VALIDATE_OFFSET(C3dMarker, m_Color, 0x58); VALIDATE_OFFSET(C3dMarker, m_PulsePeriod, 0x5C); VALIDATE_OFFSET(C3dMarker, m_RotateRate, 0x5E); VALIDATE_OFFSET(C3dMarker, m_StartTime, 0x60); VALIDATE_OFFSET(C3dMarker, m_PulseFraction, 0x64); VALIDATE_OFFSET(C3dMarker, m_StdSize, 0x68); VALIDATE_OFFSET(C3dMarker, m_Size, 0x6C); VALIDATE_OFFSET(C3dMarker, m_Brightness, 0x70); VALIDATE_OFFSET(C3dMarker, m_DistToCam2D, 0x74); VALIDATE_OFFSET(C3dMarker, m_Normal, 0x78); VALIDATE_OFFSET(C3dMarker, m_LastMapReadX, 0x84); VALIDATE_OFFSET(C3dMarker, m_RoofHeight, 0x8C); VALIDATE_OFFSET(C3dMarker, m_LastPosition, 0x90); VALIDATE_SIZE(C3dMarker, 0xA0)



;
    for (auto& m : std::span{ markers, count }) {
        m.m_Atomic        = nullptr;                    // +0x48
        m.m_Type          = MARKER3D_NA;                // word 0x101 at +0x50
        m.m_IsInUse       = false;
        m.m_IsActive      = false;
        m.m_ID            = 0;
        m.m_Color         = CRGBA{ 255, 255, 255, 255 };
        m.m_PulsePeriod   = 1'024;
        m.m_RotateRate    = 5;
        m.m_StartTime     = 0;
        m.m_PulseFraction = 0.25f;
        m.m_StdSize       = 1.0f;
        m.m_Size          = 1.0f;
        m.m_Brightness    = 1.0f;
        m.m_DistToCam2D   = 0.0f;
        m.m_Normal        = CVector{ 0.0f, 0.0f, 1.0f };
        m.m_LastMapReadX  = 30'000;
        m.m_RoofHeight    = 65535.0f;                   // 0x477FFF00
        m.m_LastPosition  = CVector{ 0.0f, 0.0f, 0.0f };
    }
}
} // namespace notsa::audit
#endif

#line 35
//> 0x724E40
void C3dMarkers::Init() {
#ifdef NOTSA_STANDALONE_RUN
    notsa::audit::ResetMarkers3dLikeExe(m_aMarkerArray.data(), m_aMarkerArray.size());
#else
#line 37
    rng::fill(m_aMarkerArray, C3dMarker{});
#endif
#line 38

    NumActiveMarkers = 0;
    m_angleDiamondDeg = 0.0f;

    //
    // Load clumps + textures + models
    //

    rng::fill(m_pRpClumpArray, nullptr);
    {
        CTxdStore::ScopedTXDSlot slut{ "particle" };

        CFileMgr::ChangeDir("\\");
        m_pRpClumpArray[MARKER3D_CYLINDER] = LoadMarker("cylinder"); // black cylinder
        m_pRpClumpArray[MARKER3D_TORUS]    = LoadMarker("hoop");     // black hoop

        m_pRpClumpArray[MARKER3D_ARROW]
            = m_pRpClumpArray[MARKER3D_CONE_NO_COLLISION]
            = m_pRpClumpArray[MARKER3D_CONE]
            = LoadMarker("diamond_3");

    // MARKER3D_ARROW2 and MARKER3D_TUBE don't seem to have clumps.
    }
}

//> 0x7227B0
void C3dMarkers::Update() {
    m_angleDiamondDeg = (float)((x87d)CTimer::GetTimeStep() * 5.0f + m_angleDiamondDeg); // exe 0x7227B0: fmul/fadd unrounded, one fstp

    for (auto& marker : m_aMarkerArray) {
        if (marker.m_IsInUse) {
            marker.m_Mat.UpdateRW();
            RwFrameUpdateObjects(RpAtomicGetFrame(marker.m_Atomic));
            marker.m_IsActive = true;
        }
    }
}

//> 0x722710
void C3dMarkers::Shutdown() {
    for (auto& marker : m_aMarkerArray) {
        marker.DeleteIfHasAtomic();
    }

    // MARKER3D_ARROW / MARKER3D_CONE_NO_COLLISION / MARKER3D_CONE share one clump (see Init). The exe (0x722763..0x7227A4) destroys each distinct clump once and
    // clears EVERY slot that holds it; clearing only the current slot made the port destroy the shared clump up to three times (use-after-free in
    // RpClumpForAllAtomics, heap corruption fast fail 0xC0000374 at the end of a normal quit).
    for (auto& v : m_pRpClumpArray) {
        if (auto* const clump = v) {
            RpClumpForAllAtomics(clump, RemoveRefsCB, nullptr);
            RpClumpDestroy(clump);
            rng::replace(m_pRpClumpArray, clump, (RpClump*)nullptr);
        }
    }
}

//> 0x725040
void C3dMarkers::Render() {
    ZoneScoped;

    NOTSA_GLOBAL_LOCAL(ambient, 0xC80444, (RwRGBAReal), {});
    NOTSA_GLOBAL_LOCAL(directional, 0xC80434, (RwRGBAReal), {});

    // Setup render states
    ActivateDirectional();
    SetAmbientColours(&ambient);
    SetDirectionalColours(&directional);

    // Now draw all
    User3dMarkersDraw();
    Render3dMarkers();
    DirectionArrowsDraw();
}

// Code from `0x725103`
void C3dMarkers::Render3dMarkers() {
    NumActiveMarkers = 0;

    for (auto& marker : m_aMarkerArray) {
        if (marker.m_IsActive) {
            if (TheCamera.IsSphereVisible(marker.m_Mat.GetPosition(), 2.0f, reinterpret_cast<RwMatrix*>(&TheCamera.m_mMatInverse))) {
                if (marker.m_DistToCam2D < 150.0f || IgnoreRenderLimit || marker.m_Type == MARKER3D_TORUS) {
                    marker.Render();
                }
            }
            marker.m_IsInUse = false;
            marker.m_IsActive = false;

            NumActiveMarkers += 1;
        } else {
            marker.DeleteIfHasAtomic();
        }
    }
}

// notsa, code from 0x725198
C3dMarker* C3dMarkers::FindById(uint32 id) {
    for (auto& a : m_aMarkerArray) {
        if (!a.m_IsInUse && a.m_ID == id) {
            return &a;
        }
    }
    return nullptr;
}

// notsa, code from 0x7251EA
C3dMarker* C3dMarkers::FindFree() {
    for (auto& a : m_aMarkerArray) {
        if (a.m_Type == MARKER3D_NA) {
            return &a;
        }
    }
    return nullptr;
}

// Returns slot index; -1 if not found
//> 0x721120
int32 C3dMarkers::DirectionArrowFindFirstFreeSlot() {
    for (const auto& [index, arrow] : rngv::enumerate(ms_directionArrows)) {
        if (!arrow.m_bIsUsed) {
            return index;
        }
    }
    return -1;
}

//> 0x721140
void C3dMarkers::DirectionArrowSet(CVector posn, float size, int32 red, int32 green, int32 blue, int32 alpha, float dirX, float dirY, float dirZ) {
    auto arrowIndex = DirectionArrowFindFirstFreeSlot();
    if (arrowIndex == -1) {
        return;
    }

    auto& arrow = ms_directionArrows[arrowIndex];
    arrow.m_vecPosition.Set(posn.x, posn.y, posn.z + 3.0f);
    arrow.m_normal.Set(dirX, dirY, dirZ);
    arrow.m_fSize = size;
    arrow.m_nRed = red;
    arrow.m_nGreen = green;
    arrow.m_nBlue = blue;
    arrow.m_nAlpha = alpha;
    arrow.m_bIsUsed = true;
}

//> 0x7211F0
void C3dMarkers::DirectionArrowsDraw() {
    auto bRenderParamsSet = false;

    for (auto& arrow : ms_directionArrows) {
        if (!arrow.m_bIsUsed) {
            continue;
        }

        if (!bRenderParamsSet) {
            RwRenderStateGet(rwRENDERSTATECULLMODE,          &CPostEffects::ms_imf.cullMode);
            RwRenderStateGet(rwRENDERSTATEZTESTENABLE,       &CPostEffects::ms_imf.bZTest);
            RwRenderStateGet(rwRENDERSTATEZWRITEENABLE,      &CPostEffects::ms_imf.bZWrite);
            RwRenderStateGet(rwRENDERSTATEVERTEXALPHAENABLE, &CPostEffects::ms_imf.bVertexAlpha);

            RwRenderStateSet(rwRENDERSTATECULLMODE,          RWRSTATE(rwCULLMODECULLNONE));
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
            RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
            RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));

            bRenderParamsSet = true;
        }

        arrow.Render(m_pRpClumpArray[MARKER3D_CONE]);

        arrow.m_bIsUsed = false;
    }

    if (bRenderParamsSet) {
        RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(CPostEffects::ms_imf.cullMode));
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(CPostEffects::ms_imf.bZTest));
        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(CPostEffects::ms_imf.bZWrite));
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(CPostEffects::ms_imf.bVertexAlpha));
    }
}

//> 0x721100
void C3dMarkers::DirectionArrowsInit() {
    for (auto& arrow : ms_directionArrows) {
        arrow.m_bIsUsed = false;
    }
}

//> 0x722870
void C3dMarkers::ForceRender(bool ignore) {
    IgnoreRenderLimit = ignore;
}

//> 0x722810
RpClump* C3dMarkers::LoadMarker(const char* modelName) {
    // Find model from name
    int32 modelId{ MODEL_INVALID };
    const auto mi = CModelInfo::GetModelInfo(modelName, &modelId);

    // Make sure model is loaded
    CStreaming::RequestModel(modelId, STREAMING_GAME_REQUIRED);
    CStreaming::LoadAllRequestedModels(false);

    // Create an instance of the model, reset it's rotation and return.
    const auto atomic = reinterpret_cast<RpAtomic*>(mi->CreateInstance());
    const auto frame  = RpAtomicGetFrame(atomic);
    const auto clump  = RpClumpCreate();

    RwFrameSetIdentity(frame);

    RpClumpAddAtomic(clump, atomic);
    RpClumpSetFrame(clump, frame);

    mi->AddRef();

    return clump;
}

// 0x725660 / 0x725A8D (inlined twice): StdSize shrinks with the 2D distance to the player: >= 25 (or NaN) unchanged; > 5: size - ((25 - dist) * size) * 0.015f (0x8717A8 = 0.3f / 20);
// otherwise size - size * 0.3f. NOT size * (1 - 0.3 * (25 - clamp(dist)) / 20): the products and the `size -` round differently.
static float ExeMarkerStdSize(float size, float dist) {
    if (!(dist < 25.f)) {
        return size;
    }
    if (dist > 5.f) {
        return size - (25.f - dist) * size * (0.3f / 20.f);
    }
    return size - size * 0.3f;
}

//> 0x725120
C3dMarker* C3dMarkers::PlaceMarker(
    uint32 id,
    e3dMarkerType type,
    CVector& pos,
    float size,
    uint8 red, uint8 green, uint8 blue, uint8 alpha,
    uint16 pulsePeriod,
    float pulseFraction,
    int16 rotateRate,
    CVector normal,
    bool zCheck
) {
    assert(id != 0);

    auto markerToPlayerDist2D = (pos - FindPlayerCentreOfWorld(0)).Magnitude2D();

    switch (type) {
    case MARKER3D_TUBE: {
        markerToPlayerDist2D *= 0.25f;
        break;
    }
    case MARKER3D_ARROW:
    case MARKER3D_ARROW2:
    case MARKER3D_TORUS:
    case MARKER3D_CONE:
    case MARKER3D_CONE_NO_COLLISION:
    case MARKER3D_CYLINDER: {
        break;
    }
    default: {
        NOTSA_LOG_WARN("Invalid marker type ({})", (int)type);
        return nullptr; // OG behaviour
    }
    }


    //> 0x725198 - Try finding by ID
    C3dMarker* m{FindById(id)};

    //> 0x72521B - If not found by ID, try finding an unused one, or find one that can be replaced
    if (!m && !(m = FindFree())) {
        const auto IsReusableType = [](e3dMarkerType t) {
            switch (t) {
            case MARKER3D_ARROW:
            case MARKER3D_ARROW2:
            case MARKER3D_CONE:
            case MARKER3D_CONE_NO_COLLISION:
                return true;
            default:
                return false;
            }
        };

        if (!IsReusableType(type)) {
            return nullptr;
        }

        for (auto& v : m_aMarkerArray) {
            // 0x725240: the replaced marker must be FARTHER from the player than the new one (`dist < v.m_DistToCam2D`, ordered), then the farthest of those
            if (markerToPlayerDist2D < v.m_DistToCam2D && IsReusableType(v.m_Type) && (!m || v.m_DistToCam2D > m->m_DistToCam2D)) {
                m = &v;
            }
        }

        if (m) {
            m->m_Type = MARKER3D_NA;
        }
    }

    //> 0x725498 - Init the marker
    if (!m) {
        return nullptr;
    }

    m->m_DistToCam2D = markerToPlayerDist2D;

    if (m->m_ID == id && m->m_Type == type) {
        switch (type) {
        case MARKER3D_CONE:
        case MARKER3D_ARROW: {
            //> 0x7254EF - `|m_LastPosition|^2 < 1e-4f` (float constant 0x858FC4; `jnp` = taken on "less") OR the 2 s timer: the exe re-tests the roof when the marker has no
            // remembered position yet (zero) or every 2000 ms; the port had `<= sq(0.01) &&`, i.e. a 64-bit-constant compare AND-ed with the timer
            if (m->m_LastPosition.SquaredMagnitude() < 0.0001f || CTimer::GetTimeInMS() - m->m_OnScreenTestTime >= 2'000) {
                m->m_OnScreenTestTime = CTimer::GetTimeInMS();
                if (m->m_LastPosition != pos) {
                    m->m_LastPosition = pos; // 0x725556: remember the position the roof test was done for (the port never stored it)
                    CEntity* hitEntity{};
                    CColPoint hitCP{};
                    const auto hasHit = CWorld::ProcessLineOfSight(
                        pos - CVector{ 0.f, 0.f, 1.5f },
                        pos,
                        hitCP,
                        hitEntity,
                        true,
                        false,
                        false,
                        false,
                        false,
                        false,
                        false,
                        false
                    );
                    m->m_RoofHeight = hasHit
                        ? hitCP.m_vecPoint.z
                        : 65535.f;
                }
            }

            //> 0x7255F9
            if (m->m_RoofHeight < 65535.f) {
                size *= 0.5f;
                pos.z = m->m_RoofHeight - RpAtomicGetBoundingSphere(m->m_Atomic)->radius * 0.1f; // TODO: Is there a macro for `->radius`?
            }

            break;
        }
        }

        //> 0x725626
        switch (type) {
        case MARKER3D_CONE:
        case MARKER3D_CONE_NO_COLLISION: {
            pos.z += X87R(x87::sin(DegreesToRadians(m_angleDiamondDeg)) * (m->m_RoofHeight < 65535.f ? 0.15f : 0.3f));
        }
        }

        //> 0x725660
        switch (type) {
        case MARKER3D_ARROW:
        case MARKER3D_ARROW2:
        case MARKER3D_CONE:
        case MARKER3D_CONE_NO_COLLISION: {
            m->m_StdSize = ExeMarkerStdSize(size, markerToPlayerDist2D);
        }
        }

        //> 0x7256D1 -> Moved after the `if` branch

        //> 0x72577E
        m->m_Size = m->m_StdSize - X87R(x87::sin(0.f) * m->m_StdSize) * pulseFraction;
        if (m->m_RotateRate) {
            const auto lastPos = m->m_Mat.GetPosition();
            m->m_Mat.RotateZ(DegreesToRadians((float)(m->m_RotateRate) * CTimer::GetTimeStep()));
            m->m_Mat.GetPosition().x = lastPos.x;
            m->m_Mat.GetPosition().y = lastPos.y;
            if (m->m_LastMapReadX != NOTSA_FTOL(uint16, lastPos.x) || m->m_LastMapReadY != NOTSA_FTOL(uint16, lastPos.y)) {
                m->m_Mat.GetPosition().z = lastPos.z;
            }
        }

        //> 0x7257FB
        switch (type) {
        case MARKER3D_ARROW:
        case MARKER3D_CYLINDER:
        case MARKER3D_CONE:
        case MARKER3D_CONE_NO_COLLISION: {
            m->m_Mat.GetPosition().x = pos.x;
            m->m_Mat.GetPosition().y = pos.y;
            if (m->m_LastMapReadX != NOTSA_FTOL(uint16, pos.x) || m->m_LastMapReadY != NOTSA_FTOL(uint16, pos.y)) {
                m->m_Mat.GetPosition().z = pos.z;
            }
            break;
        }
        }

        switch (type) {
        case MARKER3D_TORUS:
        case MARKER3D_TUBE: { //> 0x725855
            m->m_Mat.GetPosition().x = pos.x;
            m->m_Mat.GetPosition().y = pos.y;
            if (type == MARKER3D_TUBE && zCheck) {
                if (m->m_LastMapReadX != NOTSA_FTOL(uint16, pos.x) || m->m_LastMapReadY != NOTSA_FTOL(uint16, pos.y)) {
                    m->m_Mat.GetPosition().z = pos.z;
                }
                m->UpdateZCoordinate(pos, 10.f);
            } else {
                m->m_Mat.GetPosition().z = pos.z; // 0x7258B8: TORUS and TUBE without zCheck always take the requested Z
            }
            break;
        }
        case MARKER3D_CYLINDER: { //> 0x7258C2
            auto& markerZ = m->m_Mat.GetPosition().z;
            // exe: `waterZ < markerZ` (C0, set for NaN too) selects the Z update, so the water branch needs an ORDERED `waterZ >= markerZ`
            if (float waterZ; CWaterLevel::GetWaterLevelNoWaves(m->m_Mat.GetPosition(), &waterZ, nullptr, nullptr) && waterZ >= markerZ) {
                markerZ = waterZ;
            } else {
                m->UpdateZCoordinate(pos, size);
            }
            break;
        }
        }
    } else { //> 0x725956
        if (m->m_ID) {
            m->m_ID        = 0;
            m->m_StartTime = 0;
            m->m_IsInUse   = 0;
            m->m_IsActive  = false;
            m->m_Type      = MARKER3D_NA;

            const auto f = RpAtomicGetFrame(m->m_Atomic);
            RpAtomicDestroy(m->m_Atomic);
            RwFrameDestroy(f);
            m->m_Atomic = nullptr;
        }

        m->m_LastMapReadX = 30'000;

        // 0x725991
        switch (type) {
        case MARKER3D_CONE:
        case MARKER3D_CONE_NO_COLLISION: {
            pos.z += X87R(x87::sin(DegreesToRadians(m_angleDiamondDeg)) * 0.3f); // 0x7259A7: fsin, THEN fmul 0.3 (0x858C24) - the port multiplied the angle
        }
        }

        m->m_Mat.SetTranslate(pos);

        m->AddMarker(id, type, size, red, green, blue, alpha, pulsePeriod, pulseFraction, rotateRate);

        //> 0x725A16 (Same as 0x7258C2)
        switch (type) {
        case MARKER3D_CYLINDER: {
            auto& markerZ = m->m_Mat.GetPosition().z;
            // exe: `waterZ < markerZ` (C0, set for NaN too) selects the Z update, so the water branch needs an ORDERED `waterZ >= markerZ`
            if (float waterZ; CWaterLevel::GetWaterLevelNoWaves(m->m_Mat.GetPosition(), &waterZ, nullptr, nullptr) && waterZ >= markerZ) {
                markerZ = waterZ;
            } else {
                m->UpdateZCoordinate(pos, size);
            }
            break;
        }
        }

        m->m_Mat.UpdateRW();

        //> 0x725A8D
        switch (type) {
        case MARKER3D_ARROW:
        case MARKER3D_ARROW2:
        case MARKER3D_CONE:
        case MARKER3D_CONE_NO_COLLISION: {
            m->m_StdSize = ExeMarkerStdSize(size, markerToPlayerDist2D);
        }
        }
    }


    //> 0x725AEC & 0x7256D1
    if (type != MARKER3D_ARROW2) {
        // 0x7256D7-0x725755: NaN distance keeps alpha; the fade is (1 - (size + 12 - dist) * K) * alpha with K = 0x8729E4 = (1 - 0.65f) / 11 (folded), not (x * 0.35f) / 11
        if (!(markerToPlayerDist2D < size + 12.f)) {
            m->m_Color.a = alpha;
        } else if (!(size + 1.f < markerToPlayerDist2D)) {
            m->m_Color.a = NOTSA_FTOL(uint8, (float)alpha * 0.65f);
        } else {
            m->m_Color.a = NOTSA_FTOL(uint8, (1.f - (size + 12.f - markerToPlayerDist2D) * ((1.f - 0.65f) / 11.f)) * (float)alpha);
        }
    }

    m->m_Normal = normal;
    m->m_IsInUse   = true;

    return m;
}

//> 0x726D40
void C3dMarkers::PlaceMarkerCone(uint32 id, CVector& point, float size, uint8 red, uint8 green, uint8 blue, uint8 alpha, uint16 pulsePeriod, float pulseFraction, int16 rotateRate, bool bEnableCollision) {
    // exe 0x726D4E-0x726D8A: dist = sqrt((dz^2 + dx^2) + dy^2); the call is skipped only if `dist < 1.6f` (NaN does NOT skip)
    const auto camPos = TheCamera.GetPosition();
    const x87d dx = (x87d)point.x - camPos.x;
    const x87d dy = (x87d)point.y - camPos.y;
    const x87d dz = (x87d)point.z - camPos.z;
    if (!((float)x87::sqrt((dz * dz + dx * dx) + dy * dy) < 1.6f)) {
        PlaceMarker(
            id,
            bEnableCollision
                ? MARKER3D_CONE
                : MARKER3D_CONE_NO_COLLISION,
            point,
            size,
            red, green, blue, m_colDiamond,
            pulsePeriod,
            pulseFraction,
            0,
            CVector{0.f, 0.f, 0.f},
            false
        );
    }
}

//> 0x725BA0
void C3dMarkers::PlaceMarkerSet(uint32 id, e3dMarkerType type, CVector& posn, float size, uint8 red, uint8 green, uint8 blue, uint8 alpha, uint16 pulsePeriod, float pulseFraction, int16 rotateRate) {
    PlaceMarker(
        id,
        type,
        posn,
        size,
        red, green, blue, NOTSA_FTOL(uint8, (float)alpha * ExeRecip(3.0f)),
        pulsePeriod,
        pulseFraction,
        1,
        CVector{0.0f, 0.0f, 0.0f},
        false
    );
}

//> 0x7210D0
// only set material color (m_user3dMarkerColor) for first material in first atomic; 'data' is unused
RpAtomic* C3dMarkers::User3dMarkerAtomicCB(RpAtomic* atomic, void*) {
    const auto color = m_user3dMarkerColor.ToRwRGBA();
    RpMaterialSetColor(RpGeometryGetMaterial(RpAtomicGetGeometry(atomic), 0), &color);
    return nullptr;
}

//> 0x721090
void C3dMarkers::User3dMarkerDelete(int32 slotIndex) {
    if (slotIndex >= 0 && slotIndex <= (int32)(ms_user3dMarkers.size() - 1)) {
        ms_user3dMarkers[slotIndex].m_bIsUsed = false;
    }
}

//> 0x7210B0
void C3dMarkers::User3dMarkerDeleteAll() {
    for (auto& marker : ms_user3dMarkers) {
        marker.m_bIsUsed = false;
    }
}

// Returns slot index; -1 if not found
//> 0x720FB0
int32 C3dMarkers::User3dMarkerFindFirstFreeSlot() {
    for (const auto& [index, marker] : rngv::enumerate(ms_user3dMarkers)) {
        if (!marker.m_bIsUsed) {
            return index;
        }
    }
    return -1;
}

// Returns slot index; -1 if not created; for 'color', see eHudColours
//> 0x720FD0
int32 C3dMarkers::User3dMarkerSet(float x, float y, float z, eHudColours color) {
    const auto markerIndex = User3dMarkerFindFirstFreeSlot();
    if (markerIndex != -1) {
        auto& marker = ms_user3dMarkers[markerIndex];

        marker.m_vecPosition.Set(x, y, z);
    
        const auto colour = HudColour.GetRGB(color);
        marker.m_nRed = colour.r;
        marker.m_nGreen = colour.g;
        marker.m_nBlue = colour.b;

        marker.m_bIsUsed = true;
    }

    return markerIndex;
}

//> 0x723240
void C3dMarkers::User3dMarkersDraw() {
    bool bRenderParamsSet = false;

    for (auto& marker : ms_user3dMarkers) {
        if (!marker.m_bIsUsed) {
            continue;
        }

        if (!bRenderParamsSet) {
            RwRenderStateGet(rwRENDERSTATECULLMODE, &CPostEffects::ms_imf.cullMode);
            RwRenderStateGet(rwRENDERSTATEZTESTENABLE, &CPostEffects::ms_imf.bZTest);
            RwRenderStateGet(rwRENDERSTATEZWRITEENABLE, &CPostEffects::ms_imf.bZWrite);

            RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(rwCULLMODECULLNONE));
            RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(TRUE));
            RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(FALSE));

            bRenderParamsSet = true;
        }

        marker.Render(m_pRpClumpArray[MARKER3D_CONE]);
    }

    if (bRenderParamsSet) {
        RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(CPostEffects::ms_imf.cullMode));
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(CPostEffects::ms_imf.bZTest));
        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(CPostEffects::ms_imf.bZWrite));
    }
}

//> 0x5D42E0
bool C3dMarkers::LoadUser3dMarkers() {
    for (auto& marker : ms_user3dMarkers) {
        CGenericGameStorage::LoadDataFromWorkBuffer(marker);
    }
    return true;
}

//> 0x5D4300
bool C3dMarkers::SaveUser3dMarkers() {
    for (auto& marker : ms_user3dMarkers) {
        CGenericGameStorage::SaveDataToWorkBuffer(marker);
    }
    return true;
}

// Exe 0x72133A-0x72147B (inlined in `DirectionArrowsDraw`): (0, 0, 1) keeps the unity matrix, otherwise right = Normalise(z x up) and forward = Normalise(up x right).
// `CMatrix::WithUp` does not normalise (that is what made the arrow narrower for a tilted normal).
static CMatrix ExeArrowUpMatrix(const CVector& up) {
    if (up.x == 0.f && up.y == 0.f && up.z == 1.f) {
        return CMatrix::Unity();
    }
    CVector right = CrossProduct(CVector::ZAxisVector(), up);
    right.Normalise();
    CVector fwd = CrossProduct(up, right);
    fwd.Normalise();
    return CMatrix{ CVector{}, right, fwd, up };
}

// Code from (beginning at): 0x7212A4
void tDirectionArrow::Render(RpClump* clump) {
    const auto frame = RpClumpGetFrame(clump);

    // Reset frame's matrix (As we re-calculate it below)
    RwFrameSetIdentity(frame);

    // Update position to our's
    RwFrameTranslate(frame, &m_vecPosition, rwCOMBINEREPLACE);

    auto DoRender = [
        &,
        transform = CMatrix{ RwFrameGetMatrix(frame) } * ExeArrowUpMatrix(m_normal) // Build transform matrix we'll use to render
    ](CRGBA color, float scale) mutable {
        transform.Scale(scale);
        transform.UpdateRwMatrix(RwFrameGetMatrix(frame)); // Push this matrix to the underlaying RW matrix

        // After matrix update, update frame too
        RwFrameUpdateObjects(frame);

        // Apply color to material of clump
        C3dMarkers::m_user3dMarkerColor = color;
        RpClumpForAllAtomics(clump, C3dMarkers::User3dMarkerAtomicCB, nullptr); // TODO: Can use a lambda here without `m_user3dMarkerColor` too!

        // Finally, render it
        RpClumpRender(clump);
    };

    DoRender({ 0, 0, 0, 255 }, m_fSize);
    DoRender(GetColor(),       0.8f   ); // Matrix overall scale comes out to be `m_fSize * 0.8f` (Because scaling is preserved from the last step)
}

// Code from (beginning at): 0x7232BF
void tUser3dMarker::Render(RpClump* clump) const {
    const auto frame = RpClumpGetFrame(clump);

    // Reset rotation
    RwFrameSetIdentity(frame);

    // Update position
    CVector pos = m_vecPosition;
    pos.z += X87R(x87::sin(DegreesToRadians(C3dMarkers::m_angleDiamondDeg)) * 0.25f);
    RwFrameTranslate(frame, &pos, rwCOMBINEREPLACE);

    // Apply color to material of clump
    C3dMarkers::m_user3dMarkerColor = GetColor();
    RpClumpForAllAtomics(clump, C3dMarkers::User3dMarkerAtomicCB, nullptr);

    // Finally, render it
    RpClumpRender(clump);
}
