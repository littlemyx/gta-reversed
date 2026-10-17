/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "PtrListDoubleLink.h"
#include "PtrListSingleLink.h"

class CVehicle;
class CBaseModelInfo;

enum eRendererVisibility {
    RENDERER_INVISIBLE = 0,
    RENDERER_VISIBLE,
    RENDERER_CULLED,
    RENDERER_STREAMME
};

struct tScanLists {
    CPtrListSingleLink<CBuilding*>* buildingsList;
    CPtrListDoubleLink<CObject*>*   objectsList;
    CPtrListDoubleLink<CVehicle*>*  vehiclesList;
    CPtrListDoubleLink<CPed*>*      pedsList;
    CPtrListDoubleLink<CDummy*>*    dummiesList;

    template<typename Visitor>
    void VisitLists(Visitor&& visitor) {
        if (buildingsList) {
            std::invoke(visitor, *buildingsList);
        }
        if (objectsList) {
            std::invoke(visitor, *objectsList);
        }
        if (vehiclesList) {
            std::invoke(visitor, *vehiclesList);
        }
        if (pedsList) {
            std::invoke(visitor, *pedsList);
        }
        if (dummiesList) {
            std::invoke(visitor, *dummiesList);
        }
    }
};
VALIDATE_SIZE(tScanLists, 0x14);

struct tRenderListEntry {
    CEntity* entity;
    float distance;
};
VALIDATE_SIZE(tRenderListEntry, 8);

constexpr auto MAX_INVISIBLE_ENTITY_PTRS = 150u;
constexpr auto MAX_VISIBLE_ENTITY_PTRS   = 1000u;
constexpr auto MAX_VISIBLE_LOD_PTRS      = 1000u;
constexpr auto MAX_VISIBLE_SUPERLOD_PTRS = 50u;

class CWorldScan {
public:
    using tScanFunction = void(__cdecl*)(int32, int32);

    // NOTSA names. The original keeps 4 + 4 ints back to back (Y at 0xC81598, X at 0xC815A8) and a count at 0xC815B8
    static inline NOTSA_GLOBAL(ms_aExtraRectangleY, 0xC81598, (int32[4]), {});
    static inline NOTSA_GLOBAL(ms_aExtraRectangleX, 0xC815A8, (int32[4]), {});
    static inline NOTSA_GLOBAL(ms_nExtraRectangleCount, 0xC815B8, (int32), {});

    static void InjectHooks();
    static void ScanWorld(CVector2D* points, int32 pointsCount, tScanFunction scanFunction);
    static void SetExtraRectangleToScan(float minX, float maxX, float minY, float maxY);
};

class CRenderer {
public:
    static inline NOTSA_GLOBAL(ms_bRenderTunnels, 0xB745C0, (bool), {});
    static inline NOTSA_GLOBAL(ms_bRenderOutsideTunnels, 0xB745C1, (bool), {});
    static inline NOTSA_GLOBAL(ms_pLodDontRenderList, 0xB745CC, (tRenderListEntry*), {});
    static inline NOTSA_GLOBAL(ms_pLodRenderList, 0xB745D0, (tRenderListEntry*), {});
    static inline NOTSA_GLOBAL(m_pFirstPersonVehicle, 0xB745D4, (CVehicle*), {});
    static inline NOTSA_GLOBAL(ms_aInVisibleEntityPtrs, 0xB745D8, (CEntity*[MAX_INVISIBLE_ENTITY_PTRS]), {});
    static inline NOTSA_GLOBAL(ms_aVisibleSuperLodPtrs, 0xB74830, (CEntity*[MAX_VISIBLE_SUPERLOD_PTRS]), {});
    static inline NOTSA_GLOBAL(ms_aVisibleLodPtrs, 0xB748F8, (CEntity*[MAX_VISIBLE_LOD_PTRS]), {});
    static inline NOTSA_GLOBAL(ms_aVisibleEntityPtrs, 0xB75898, (CEntity*[MAX_VISIBLE_ENTITY_PTRS]), {});
    static inline NOTSA_GLOBAL(ms_nNoOfVisibleSuperLods, 0xB76838, (int32), {});
    static inline NOTSA_GLOBAL(ms_nNoOfInVisibleEntities, 0xB7683C, (int32), {});
    static inline NOTSA_GLOBAL(ms_nNoOfVisibleLods, 0xB76840, (int32), {});
    static inline NOTSA_GLOBAL(ms_nNoOfVisibleEntities, 0xB76844, (int32), {});
    static inline NOTSA_GLOBAL(ms_fFarClipPlane, 0xB76848, (float), {});
    static inline NOTSA_GLOBAL(ms_fCameraHeading, 0xB7684C, (float), {});
    static inline NOTSA_GLOBAL(m_loadingPriority, 0xB76850, (bool), {});
    static inline NOTSA_GLOBAL(ms_bInTheSky, 0xB76851, (bool), {});
    static inline NOTSA_GLOBAL(ms_vecCameraPosition, 0xB76870, (CVector), {});
    static inline NOTSA_GLOBAL(ms_lodDistScale, 0x8CD800, (float), { 1.2f }); // default 1.2
    static inline NOTSA_GLOBAL(ms_lowLodDistScale, 0x8CD804, (float), { 1.0f }); // default 1.0

public:
    static void InjectHooks();

    static void Init();
    static void Shutdown();
    static void RenderFadingInEntities();
    static void RenderFadingInUnderwaterEntities();
    static void RenderOneRoad(CEntity* entity);
    static void RenderOneNonRoad(CEntity* entity);
    static void RemoveVehiclePedLights(CPhysical* entity);
    static void AddEntityToRenderList(CEntity* entity, float distance);
    static tRenderListEntry* GetLodRenderListBase();
    static tRenderListEntry* GetLodDontRenderListBase();
    static void ResetLodRenderLists();
    static void AddToLodRenderList(CEntity* entity, float distance);
    static void AddToLodDontRenderList(CEntity* entity, float distance);
    static void ProcessLodRenderLists();
    static void PreRender();
    static void RenderRoads();
    static void RenderEverythingBarRoads();
    static void RenderFirstPersonVehicle();
    static bool SetupLightingForEntity(CPhysical* entity);
    static int32 SetupMapEntityVisibility(CEntity* entity, CBaseModelInfo* modelInfo, float distance, bool bIsTimeInRange);
    static int32 SetupEntityVisibility(CEntity* entity, float& outDistance);
    static int32 SetupBigBuildingVisibility(CEntity* entity, float& outDistance);
    static void ScanSectorList_ListModels(int32 sectorX, int32 sectorY);
    static void ScanSectorList_ListModelsVisible(int32 sectorX, int32 sectorY);
    static void ScanSectorList(int32 sectorX, int32 sectorY);
    static void ScanBigBuildingList(int32 sectorX, int32 sectorY);
    static bool ShouldModelBeStreamed(CEntity* entity, const CVector& origin, float farClip);
    template<typename PtrListType>
    static void ScanPtrList_RequestModels(PtrListType& list);
    static void ConstructRenderList();
    static void ScanSectorList_RequestModels(int32 sectorX, int32 sectorY);
    static void ScanWorld();
    static int32 GetObjectsInFrustum(CEntity** outEntities, float farPlane, RwMatrix* transformMatrix);
    static void RequestObjectsInFrustum(RwMatrix* transformMatrix, int32 modelRequestFlags);
    static void RequestObjectsInDirection(const CVector& posn, float angle, int32 modelRequestFlags);
    static void SetupScanLists(int32 sectorX, int32 sectorY);

    static void SetLoadingPriority(int8 priority) noexcept { m_loadingPriority = priority; } // 0x407370

    static auto GetVisibleLodPtrs()      { return std::span{ ms_aVisibleLodPtrs,      (size_t)ms_nNoOfVisibleLods }; }
    static auto GetVisibleEntityPtrs()   { return std::span{ ms_aVisibleEntityPtrs,   (size_t)ms_nNoOfVisibleEntities }; }
    static auto GetVisibleSuperLodPtrs() { return std::span{ ms_aVisibleSuperLodPtrs, (size_t)ms_nNoOfVisibleSuperLods }; }
    static auto GetInVisibleEntityPtrs() { return std::span{ ms_aInVisibleEntityPtrs, (size_t)ms_nNoOfInVisibleEntities }; }
};

NOTSA_GLOBAL_EXTERN(gnRendererModelRequestFlags, (uint32));
NOTSA_GLOBAL_EXTERN(gpOutEntitiesForGetObjectsInFrustum, (CEntity**));
