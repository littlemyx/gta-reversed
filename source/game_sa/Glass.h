#pragma once

#include <rwcore.h>
#include <FallingGlassPane.h>
#include "Vector2D.h"
#include "Vector.h"
#include "game_sa/common.h"
#line 7

class CVehicle;
class CEntity;
class CObject;

enum class ePaneType : uint32 {
    DEFAULT = 0,
    DELAYED = 1,
    CAR     = 2
};

class CGlass {
public:
    static NOTSA_GLOBAL_DECL(CGlass, PanePolyPositions, 0x8D5CD8, (CVector2D[5][3]));
    static inline NOTSA_GLOBAL(ReflectionPolyVertexBaseIdx, 0xC71B18, (int32), {});
    static inline NOTSA_GLOBAL(ReflectionPolyIndexBaseIdx, 0xC71B1C, (int32), {});
    static inline NOTSA_GLOBAL_ALIAS(ReflectionPolyVertexBuffer, 0xC5B158, (std::array<RxObjSpace3DLitVertex, 1706>), *reinterpret_cast<std::array<RxObjSpace3DLitVertex, 1706>*>(reinterpret_cast<uint8*>(&TempBufferVertices) + 55296));
    static inline NOTSA_GLOBAL_ALIAS(ShatteredPolyVertexBuffer, 0xC56958, (std::array<RxObjSpace3DLitVertex, 512>), *reinterpret_cast<std::array<RxObjSpace3DLitVertex, 512>*>(reinterpret_cast<uint8*>(&TempBufferVertices) + 36864));
    static inline NOTSA_GLOBAL(ShatteredVerticesBaseIdx, 0xC71B20, (int32), {});
    static inline NOTSA_GLOBAL(ShatteredIndicesBaseIdx, 0xC71B24, (int32), {});
    static inline NOTSA_GLOBAL(H1iLightPolyVerticesIdx, 0xC71B28, (uint32), {});
    static inline NOTSA_GLOBAL(HiLightPolyIndicesIdx, 0xC71B2C, (int32), {});
    static inline NOTSA_GLOBAL(PanePolyCenterPositions, 0xC71B30, (std::array<CVector2D, 5>), {});
    static inline NOTSA_GLOBAL(apEntitiesToBeRendered, 0xC71B58, (CEntity*[32]), {});
    static inline NOTSA_GLOBAL(NumGlassEntities, 0xC71BD8, (uint32), {});
    static inline NOTSA_GLOBAL(aGlassPanes, 0xC71BF8, (std::array<CFallingGlassPane, 45>), {}); static_assert(sizeof(aGlassPanes) == 5040); // 45 panes: the exe loops end at 0xC73015 (+0x6D of pane 44) and the array ends at LastColCheckMS 0xC72FA8
    static inline NOTSA_GLOBAL(LastColCheckMS, 0xC72FA8, (uint32), {});

public:
    static void InjectHooks();

    static void Init();
    static bool HasGlassBeenShatteredAtCoors(CVector point);
    static void CarWindscreenShatters(CVehicle* vehicle);
    static void WasGlassHitByBullet(CEntity* entity, CVector hitPos);
    static void WindowRespondsToCollision(CEntity* entity, float fDamageIntensity, CVector vecMoveSpeed, CVector vecPoint, bool max1PaneSection);
    static void GeneratePanesForWindow(ePaneType type, CVector pos, CVector fwd, CVector right, CVector velocity, CVector center, float velocityCenterDragCoeff,
                                       bool bShatter, bool numSectionsMax1, int32 numSections, bool unk);
    static void Update();
    static void Render();
    template<typename PtrListType>
    static void FindWindowSectorList(PtrListType& objList, float& outDist, CEntity*& outEntity, CVector point);
    static void RenderReflectionPolys();
    static void RenderShatteredPolys();
    static void RenderHiLightPolys();
    static bool IsObjectGlass(CEntity* entity);
    static uint8 CalcAlphaWithNormal(const CVector& normal);
    static void AskForObjectToBeRenderedInGlass(CEntity* entity);
    static CFallingGlassPane* FindFreePane();
    static void WindowRespondsToSoftCollision(CEntity* entity, float fDamageIntensity);
    static void BreakGlassPhysically(CVector pos, float radius);
    static void WindowRespondsToExplosion(CEntity* entity, Const CVector point);
};
