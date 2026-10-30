#include "StdInc.h"

#include "Mirrors.h"

#include "MovingThings.h"
#include "BreakManager_c.h"
#include "PlantMgr.h"
#include "Clouds.h"
#include "PostEffects.h"
#include "Shadows.h"
#include "CarFXRenderer.h"

NOTSA_GLOBAL(bFudgeNow, 0xC7C72A, (bool), {});

/*!
 * Screen positions in Los Santos stadium. Odd job called as "8-Track"
 * 0x8D5DD8
 * */
constexpr CVector Screens8Track[2][4] = {
    {
        { -1333.453f, -221.898f, 1079.141f },
        { -1333.453f, -189.898f, 1079.141f },
        { -1333.453f, -189.898f, 1067.141f },
        { -1333.453f, -221.898f, 1067.141f },
    },
    {
        { -1477.845f, -189.969f, 1079.141f },
        { -1477.845f, -221.969f, 1079.141f },
        { -1477.845f, -221.969f, 1067.141f },
        { -1477.845f, -189.969f, 1067.141f },
    }
};

void CMirrors::InjectHooks() {
    RH_ScopedClass(CMirrors);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x723000);
    RH_ScopedInstall(ShutDown, 0x723050);
    RH_ScopedInstall(CreateBuffer, 0x7230A0);
    RH_ScopedInstall(BuildCamMatrix, 0x723150);
    RH_ScopedInstall(RenderMirrorBuffer, 0x726090);
    RH_ScopedInstall(BuildCameraMatrixForScreens, 0x7266B0);
    RH_ScopedInstall(BeforeConstructRenderList, 0x726DF0);
    RH_ScopedInstall(BeforeMainRender, 0x727140);
}

// 0x723000
void CMirrors::Init() {
    ShutDown();
}

// 0x723050
void CMirrors::ShutDown() {
    if (pBuffer) {
        RwRasterDestroy(pBuffer);
        pBuffer = nullptr;
    }
    if (pZBuffer) {
        RwRasterDestroy(pZBuffer);
        pZBuffer = nullptr;
    }
    TypeOfMirror = MIRROR_TYPE_NONE;
    MirrorFlags = 0;
}

// 0x7230A0
void CMirrors::CreateBuffer() {
    if (pBuffer)
        return;

    const auto depth = RwRasterGetDepth(RwCameraGetRaster(Scene.m_pRwCamera));

    // 0x7230A0: quality >= 1 tries the 1024x512 pair; when either raster can't be created (the first one is destroyed again) the exe falls through to the 512x256 pair, which is not checked
    if (g_fx.GetFxQuality() >= FX_QUALITY_MEDIUM) {
        pBuffer = RwRasterCreate(1024, 512, depth, rwRASTERTYPECAMERATEXTURE);
        if (pBuffer) {
            pZBuffer = RwRasterCreate(1024, 512, depth, rwRASTERTYPEZBUFFER);
            if (pZBuffer) {
                return; // All needed buffers created
            }
            RwRasterDestroy(pBuffer);
            pBuffer = nullptr;
        }
    }
    pBuffer  = RwRasterCreate(512, 256, depth, rwRASTERTYPECAMERATEXTURE);
    pZBuffer = RwRasterCreate(512, 256, depth, rwRASTERTYPEZBUFFER);
}

// 0x723150
void CMirrors::BuildCamMatrix(CMatrix& mat, CVector pointA, CVector pointB) {
    mat.SetTranslateOnly(pointA);
    mat.GetForward() = Normalized(pointB - pointA);
    mat.GetRight() = Normalized(CrossProduct({ 0.0f, 0.0f, 1.0f }, mat.GetForward())); // 0x723150: the exe calls CVector::Normalise on the right vector as well
    mat.GetUp() = CrossProduct(mat.GetForward(), mat.GetRight());
}

// 0x726090
void CMirrors::RenderMirrorBuffer() {
    ZoneScoped;

    if (TypeOfMirror == MIRROR_TYPE_NONE)
        return;

    RwRaster* raster = RwCameraGetRaster(Scene.m_pRwCamera);
    const CVector2D rasterSize{ (float)RwRasterGetWidth(raster), (float)RwRasterGetHeight(raster) };

    DefinedState();

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(pBuffer));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDZERO));

    RwImVertexIndex indices[] = { 0, 1, 2, 0, 2, 3 };

    if (MirrorFlags & eZoneAttributes::STAIRS || bFudgeNow) {
        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
        RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
        RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(TRUE));

        constexpr CVector2D uvs[4]{
            { 0.0f, 0.0f },
            { 1.0f, 0.0f },
            { 1.0f, 1.0f },
            { 0.0f, 1.0f },
        };
        for (auto x = 0u; x < std::size(Screens8Track); x++) {
            RxObjSpace3DVertex vertices[4]{};
            for (auto i = 0u; i < std::size(vertices); i++) {
                RwIm3DVertexSetRGBA(&vertices[i], 255, 255, 255, 255);
                RwV3dAssign(RwIm3DVertexGetPos(&vertices[i]), &Screens8Track[x][i]);
                RwIm3DVertexSetU(&vertices[i], uvs[i].x);
                RwIm3DVertexSetV(&vertices[i], uvs[i].y);
            }

            if (RwIm3DTransform(vertices, std::size(vertices), nullptr, rwIM3D_VERTEXUV)) {
                RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, indices, std::size(indices));
                RwIm3DEnd();
            }
        }
    } else {
        const CVector2D pos[] = {
           { 0.0f,         0.0f,        },
           { 0.0f,         rasterSize.y },
           { rasterSize.x, rasterSize.y },
           { rasterSize.x, 0.0f,        }
        };

        constexpr CVector2D uvs[] = {
            { 1.0f, 0.0f },
            { 1.0f, 1.0f },
            { 0.0f, 1.0f },
            { 0.0f, 0.0f }
        };

        RwIm2DVertex vertices[4]{};
        for (auto i = 0u; i < std::size(vertices); i++) {
            RwIm2DVertexSetRecipCameraZ(&vertices[i], 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera));
            RwIm2DVertexSetIntRGBA(&vertices[i], 255, 255, 255, 255);

            RwIm2DVertexSetScreenX(&vertices[i], pos[i].x);
            RwIm2DVertexSetScreenY(&vertices[i], pos[i].y);
            RwIm2DVertexSetScreenZ(&vertices[i], RwIm2DGetNearScreenZ());

            RwIm3DVertexSetU(&vertices[i], uvs[i].x);
            RwIm3DVertexSetV(&vertices[i], uvs[i].y);
        }
        RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, vertices, std::size(vertices), indices, std::size(indices));
    }

    // 0x726090..: centre (216, -42, 1003) (the consts at 0x8729EC are stored z,y,x), squared sum added z,y,x, fsqrt, strict `< 50` (NaN skips)
    const auto exeDistLt50 = [] {
        const CVector& cam = TheCamera.GetPosition();
        const float dx = cam.x - 216.0f, dy = cam.y - (-42.0f), dz = cam.z - 1003.0f;
        return std::sqrt(dz * dz + dy * dy + dx * dx) < 50.0f;
    };
    if (exeDistLt50()) {
        // 0x726090: the exe sets these four states before the Z-only quad (write Z, no vertex alpha, src ZERO / dst ONE: colour untouched)
        RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
        RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDZERO));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDONE));

        const CVector pos[]{
            { 216.0f, -45.0f, 1000.0f },
            { 216.0f, -45.0f, 1006.0f },
            { 216.0f, -39.0f, 1006.0f },
            { 216.0f, -39.0f, 1000.0f }
        };

        constexpr CVector2D uvs[]{
            { 0.0f, 0.0f },
            { 1.0f, 0.0f },
            { 1.0f, 1.0f },
            { 0.0f, 1.0f },
        };

        // TODO: This is another very commonly used thing (constructing vertices from a set of pos, uvs)
        // Make a function out of it.

        RxObjSpace3DVertex vertices[4]{};
        for (auto i = 0u; i < std::size(vertices); i++) {
            RwIm3DVertexSetRGBA(&vertices[i], 255, 255, 255, 255);
            RwV3dAssign(RwIm3DVertexGetPos(&vertices[i]), &pos[i]);
            RwIm3DVertexSetU(&vertices[i], uvs[i].x);
            RwIm3DVertexSetV(&vertices[i], uvs[i].y);
        }

        if (RwIm3DTransform(vertices, std::size(vertices), nullptr, rwIM3D_VERTEXUV)) {
            RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, indices, std::size(indices));
            RwIm3DEnd();
        }
    }

    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
}

// ANDROID 1.0 0x5078B8
void CMirrors::RenderReflBuffer(bool) {

}

// ANDROID 1.0 0x507B2C
void CMirrors::RenderReflections() {

}

// 0x7266B0
void CMirrors::BuildCameraMatrixForScreens(CMatrix & mat) {
    const uint32 timeSeconds = (CTimer::GetTimeInMS() / 1000) % 32; // Wrapped to (0, 32]
    const uint32 timeMsLeftInSecond = CTimer::GetTimeInMS() % 1000;

    switch (timeSeconds) {
    case 0:
    case 1:
    case 2:
    case 3: {
        BuildCamMatrix(
            mat,
            { ((float)timeMsLeftInSecond * ExeRecip(1000.0f) + (float)timeSeconds) * 6.0f - 1249.3f, -224.5f, 1064.2f },
            { -1265.4f, -207.5f, 1053.2f }
        );
        break;
    }
    case 10:
    case 11:
    case 12:
    case 13: {
        BuildCamMatrix(mat,
            { -1406.4f, -135.3f, 1045.65f },
            { -1402.5f, -146.8f, 1043.10f }
        );
        break;
    }
    case 22:
    case 23:
    case 24:
    case 25: {
        BuildCamMatrix(mat,
            { -1479.0f, -290.5f, 1099.54f },
            { -1428.0f, -256.7f, ((float)timeMsLeftInSecond * ExeRecip(1000.0f) + (float)(timeSeconds - 22)) * 3.0f + 1057.3f }
        );
        break;
    }
    default: {
        mat.SetRotateZOnly(float(CTimer::GetTimeInMS() % 16384) * PI / 8192.0f);
        mat.SetTranslateOnly({ -1397.0f, -219.0f, 1054.0f });
        break;
    }
    }
}

// NOTSA
bool CMirrors::ShouldRenderPeds() {
    return bRenderingReflection && TypeOfMirror != MIRROR_TYPE_FLOOR;
}

// 0x726DF0
void CMirrors::BeforeConstructRenderList() {
    if (d3dRestored) {
        d3dRestored = false;
        Init();
    }

    CMirrorAttributeZone* mirrorAttrs = nullptr;

    const auto mirrorActive = [&](){
        // Check player is in heli/plane
        if (auto* vehicle = FindPlayerVehicle()) {
            if (vehicle->IsSubHeli() || vehicle->IsSubPlane()) {
                ShutDown();
                return false;
            }
        }

        mirrorAttrs = CCullZones::FindMirrorAttributesForCoors(TheCamera.GetPosition());
        if (!mirrorAttrs) {
            return false;
        }

        if ((mirrorAttrs->flags & eZoneAttributes::STAIRS) == 0) {
            return true;
        }

        return rng::any_of(Screens8Track, [](const auto& track) {
            TheCamera.m_bMirrorActive = false;
            return TheCamera.IsSphereVisible(CVector::Centroid(track), 8.0f);
        });
    }();

    if (mirrorActive) {
        // Actually update cam
        assert(mirrorAttrs);

        MirrorV = mirrorAttrs->mirrorV;
        MirrorNormal = CVector{ (float)mirrorAttrs->mirrorNormalX, (float)mirrorAttrs->mirrorNormalY, (float)mirrorAttrs->mirrorNormalZ } * ExeRecip(100.0f);
        MirrorFlags = mirrorAttrs->flags;

        TypeOfMirror = std::fabs(MirrorNormal.z) <= 0.7f ? MIRROR_TYPE_WALL : MIRROR_TYPE_FLOOR;
        CreateBuffer();
    } else {
        ShutDown();
    }

    if ((MirrorFlags & eZoneAttributes::STAIRS) != 0 || bFudgeNow) {
        CMatrix mat{};
        BuildCameraMatrixForScreens(mat);
        TheCamera.DealWithMirrorBeforeConstructRenderList(mirrorActive, MirrorNormal, MirrorV, &mat);
    } else {
        TheCamera.DealWithMirrorBeforeConstructRenderList(mirrorActive, MirrorNormal, MirrorV, nullptr);
    }
}

// 0x727140
void CMirrors::BeforeMainRender() {
    ZoneScoped;

    if (TypeOfMirror == MIRROR_TYPE_NONE)
        return;

    RwRaster* prevCamRaster  = RwCameraGetRaster(Scene.m_pRwCamera);
    RwRaster* prevCamZRaster = RwCameraGetZRaster(Scene.m_pRwCamera);

    RwCameraSetRaster(Scene.m_pRwCamera, pBuffer);
    RwCameraSetZRaster(Scene.m_pRwCamera, pZBuffer);

    TheCamera.SetCameraUpForMirror();

    RwRGBA color{ 0, 0, 0, 255 };
    RwCameraClear(Scene.m_pRwCamera, &color, rwCAMERACLEARZ | rwCAMERACLEARIMAGE | (GraphicsHighQuality() ? rwCAMERACLEARSTENCIL : 0));
    if (RsCameraBeginUpdate(Scene.m_pRwCamera)) {
        bRenderingReflection = true;
        DefinedState();
        RenderScene();
        CVisibilityPlugins::RenderWeaponPedsForPC();
        CVisibilityPlugins::ResetWeaponPedsForPC();
        bRenderingReflection = false;

        RwCameraEndUpdate(Scene.m_pRwCamera);

        RwCameraSetRaster(Scene.m_pRwCamera, prevCamRaster);
        RwCameraSetZRaster(Scene.m_pRwCamera, prevCamZRaster);

        TheCamera.RestoreCameraAfterMirror();
    }
}
