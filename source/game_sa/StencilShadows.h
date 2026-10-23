#pragma once

class CStencilShadowObject;

class CStencilShadows {
public:
    static inline NOTSA_GLOBAL(m_StencilShadowObjects, 0xC6A198, (std::array<CStencilShadowObject, 64>), {}); VALIDATE_SIZE(m_StencilShadowObjects, 1792);

    static inline NOTSA_GLOBAL(pFirstAvailableStencilShadowObject, 0xC6A168, (CStencilShadowObject*), {});
    static inline NOTSA_GLOBAL(pFirstActiveStencilShadowObject, 0xC6A16C, (CStencilShadowObject*), {});

    static void InjectHooks();

    static void Init();
    static void Shutdown();
    static void Process(CVector& cameraPos);

    // NOTSA/FIX_BUGS: It was named GraphicsLowQuality for some reason
    static bool GraphicsHighQuality();

    static void UpdateHierarchy(CStencilShadowObject*& firstAvailable, CStencilShadowObject*& firstActive, CStencilShadowObject* newOne);
    static bool RegisterStencilShadows(CVector& cameraPos, bool doNotCreateNew);

    static void RenderStencilShadows();
    static void RenderForVehicle(CStencilShadowObject*);
    static void RenderForObject(CStencilShadowObject*);
    static void Render(const CRGBA& color);
    static void RenderBuffer(const CVector& pos);
    static void SunSetPositionFromEntity(const CEntity* entity);

    static void sub_710CC0(int32 indices, int32 vertices);
};
