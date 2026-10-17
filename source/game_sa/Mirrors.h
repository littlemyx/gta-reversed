/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "Matrix.h"
#include "Vector.h"

enum eMirrorType : uint8 {
    MIRROR_TYPE_NONE = 0,
    MIRROR_TYPE_WALL = 1,
    MIRROR_TYPE_FLOOR = 2,
    MIRROR_TYPE_SPHERE_MAP = 3
};

class CMirrors {
public:
    static inline NOTSA_GLOBAL(pBuffer, 0xC7C71C, (RwRaster*), {});
    static inline NOTSA_GLOBAL(pZBuffer, 0xC7C720, (RwRaster*), {});
    static inline NOTSA_GLOBAL(bRenderingReflection, 0xC7C728, (bool), {});
    static inline NOTSA_GLOBAL(d3dRestored, 0xC7C729, (bool), {});
    static inline NOTSA_GLOBAL(TypeOfMirror, 0xC7C724, (eMirrorType), {});
    static inline NOTSA_GLOBAL(MirrorFlags, 0xC7C618, (uint8), {});
    static inline NOTSA_GLOBAL(MirrorV, 0xC7C61C, (float), {});
    static inline NOTSA_GLOBAL(MirrorNormal, 0xC803D8, (CVector), {});

public:
    static void InjectHooks();

    static void Init();
    static void ShutDown();

    static void BeforeConstructRenderList();
    static void BeforeMainRender();

    static void BuildCamMatrix(CMatrix& camMatrix, CVector source, CVector dest);
    static void BuildCameraMatrixForScreens(CMatrix& mat);

    static void CreateBuffer();

    static void RenderMirrorBuffer();
    static void RenderReflBuffer(bool);
    static void RenderReflections();

    static bool ShouldRenderPeds();
};
