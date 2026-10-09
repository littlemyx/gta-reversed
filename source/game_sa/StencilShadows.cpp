#include "StdInc.h"
#include "StencilShadowObject.h"
#include "Bike.h"
#include "Object.h"

// TODO: Statically allocate after reversing RenderForVehicle&RenderForObject.
static inline auto& s_ShadowTrianglePointsUnk         = StaticRef<RxVertexIndex*>(0xC6A170);
static inline auto& s_ShadowTrianglePoints            = StaticRef<CVector*>(0xC6A174);
static inline auto& s_TransformedShadowTrianglePoints = StaticRef<CVector*>(0xC6A178);
static inline auto& s_SunPosNrm                       = StaticRef<CVector>(0x8D5244); // CVector(1.0, 1.0, -2.0)

// Squared distance (in the original: "signed", negative if the camera is inside) from `camPos` to the bounding sphere's surface of `entity`
// 0x711160 and 0x7111F0 (identical copies, usercall: ebx = entity, esi = camPos)
static float GetSignedSquaredDistanceToBoundSphere(CEntity* entity, const CVector& camPos) {
    auto* const colModel = entity->GetColModel();
    CVector     centre;
    entity->TransformFromObjectSpace(centre, colModel->m_boundSphere.m_vecCenter);

    const float dx = camPos.x - centre.x;
    const float dy = camPos.y - centre.y;
    const float dz = camPos.z - centre.z;

    const float dist   = std::sqrt((dz * dz + dy * dy) + dx * dx);
    const float radius = colModel->m_boundSphere.m_fRadius;
    const float sign   = (dist < radius) ? -1.0f : 1.0f;
    return sign * (dist - radius) * (dist - radius);
}

// 0x710BA0 - Find the active shadow object that belongs to the given entity
static CStencilShadowObject* FindShadowObjectOfEntity(CEntity* entity) {
    for (auto* obj = CStencilShadows::pFirstActiveStencilShadowObject; obj; obj = obj->m_pNext) {
        if (obj->m_pOwner == entity) {
            return obj;
        }
    }
    return nullptr;
}

// 0x711280 (thiscall, `this` = `pFirstAvailableStencilShadowObject`) - Takes the first available shadow object and makes it active for `entity`
static CStencilShadowObject* CreateShadowObjectFor(CEntity* entity, eStencilShadowObjType type) {
    if (type == eStencilShadowObjType::NONE || type > eStencilShadowObjType::VEHICLE) {
        return nullptr;
    }

    auto* const colModel = entity->GetColModel();
    auto* const colData  = colModel->m_pColData;
    if (!colData || !colData->bHasShadowInfo) {
        return nullptr;
    }

    const auto numShadowTriangles = (int32)colData->m_nNumShadowTriangles;
    if (numShadowTriangles <= 0) {
        return nullptr;
    }

    auto* const obj = CStencilShadows::pFirstAvailableStencilShadowObject;
    obj->m_NumShadowFaces        = (int16)numShadowTriangles;
    obj->m_FaceID                = numShadowTriangles * 15; // NOTE: This is actually the capacity (in vertices) of `m_ShadowFacesData`
    obj->m_pOwner                = entity;
    obj->m_Type                  = type;
    obj->m_SizeOfShadowFacesData = 0;                       // NOTE: This is actually the number of vertices in `m_ShadowFacesData`
    obj->m_ShadowFacesData       = (CVector*)CMemoryMgr::Malloc((uint32)(numShadowTriangles * 15 * sizeof(CVector)));
    entity->RegisterReference(reinterpret_cast<CEntity**>(&obj->m_pOwner));

    CStencilShadows::UpdateHierarchy(
        CStencilShadows::pFirstAvailableStencilShadowObject,
        CStencilShadows::pFirstActiveStencilShadowObject,
        obj
    );
    return obj;
}

// NOTSA - Object that doesn't want to have a stencil shadow (invisible, exploded, or broken)
static bool IsObjectNotSuitableForShadow(CEntity* entity) {
    if (!entity->GetIsTypeObject()) {
        return false;
    }
    auto* const obj = entity->AsObject();
    return !entity->m_bIsVisible
        || obj->objectFlags.bIsExploded
        || obj->objectFlags.bIsBroken;
}

// 0x70FA70 (usercall: ecx = edges, edx = &count, edi = a, stack = b)
// Adds the edge (a, b) to the list, or - if the opposite edge (shared by 2 triangles) is already in there - removes it
static void AddOrRemoveShadowEdge(RxVertexIndex* edges, uint16& count, uint16 a, uint16 b) {
    for (auto i = 0u; i < count; i++) {
        if ((edges[i * 2] == a && edges[i * 2 + 1] == b) || (edges[i * 2] == b && edges[i * 2 + 1] == a)) {
            if (count > 1) {
                edges[i * 2]     = edges[(count - 1) * 2];
                edges[i * 2 + 1] = edges[(count - 1) * 2 + 1];
            }
            count--;
            return;
        }
    }
    edges[count * 2]     = a;
    edges[count * 2 + 1] = b;
    count++;
}

// Common part of `RenderForVehicle` and `RenderForObject` (Both functions are identical, except for the matrix and the extrusion distance used)
static void BuildShadowVolume(CStencilShadowObject* obj, CCollisionData* colData, CMatrix& matrix, float extrudeDist) {
    CMatrix invMatrix;
    Invert(matrix, invMatrix);

    // Light direction in the object's space
    const CVector origin = invMatrix.TransformPoint(CVector{ 0.0f, 0.0f, 0.0f });
    const CVector light  = invMatrix.TransformPoint(s_SunPosNrm);
    const float   dirX   = light.x - origin.x;
    const float   dirY   = light.y - origin.y;
    const float   dirZ   = light.z - origin.z;

    obj->m_SizeOfShadowFacesData = 0;

    auto* const edges  = s_ShadowTrianglePointsUnk;
    auto* const points = s_ShadowTrianglePoints;
    auto* const worldPoints = s_TransformedShadowTrianglePoints;

    uint16      numEdges = 0;
    const auto  numVerts = (int32)colData->m_nNumShadowVertices;

    for (auto i = 0; i < numVerts; i++) {
        colData->GetShadTrianglePoint(points[i], i);
    }
    TransformPoints(worldPoints, numVerts, matrix, points);

    // Shadow volume's caps
    for (auto t = 0; t < (int16)obj->m_NumShadowFaces; t++) {
        const auto& tri = colData->m_pShadowTriangles[t];
        const auto  a = tri.vA, b = tri.vB, c = tri.vC;

        const CVector& p0 = points[a];
        const CVector& p1 = points[b];
        const CVector& p2 = points[c];
        const CVector& w0 = worldPoints[a];
        const CVector& w1 = worldPoints[b];
        const CVector& w2 = worldPoints[c];

        const float e1x = p1.x - p0.x, e1y = p1.y - p0.y, e1z = p1.z - p0.z;
        const float e2x = p2.x - p0.x, e2y = p2.y - p0.y, e2z = p2.z - p0.z;

        const float nx = e2z * e1y - e2y * e1z;
        const float ny = e2x * e1z - e2z * e1x;
        const float nz = e2y * e1x - e2x * e1y;

        if (dirZ * nz + ny * dirY + nx * dirX < 0.0f) {
            // Facing the light => Add the extruded copy of the triangle
            const float ex = s_SunPosNrm.x * extrudeDist;
            const float ey = s_SunPosNrm.y * extrudeDist;
            const float ez = s_SunPosNrm.z * extrudeDist;

            const CVector ext0{ ex + w0.x, w0.y + ey, w0.z + ez };
            const CVector ext1{ w1.x + ex, w1.y + ey, ez + w1.z };
            const CVector ext2{ w2.x + ex, w2.y + ey, w2.z + ez };

            if ((int32)obj->m_FaceID <= (int32)obj->m_SizeOfShadowFacesData + 3) {
                break;
            }
            auto* const out = &obj->m_ShadowFacesData[obj->m_SizeOfShadowFacesData];
            out[0] = ext0;
            out[1] = ext2;
            out[2] = ext1;
        } else {
            // Facing away from the light => Add the (flipped) triangle itself, and register its edges
            AddOrRemoveShadowEdge(edges, numEdges, a, b);
            AddOrRemoveShadowEdge(edges, numEdges, b, c);
            AddOrRemoveShadowEdge(edges, numEdges, c, a);

            if ((int32)obj->m_FaceID <= (int32)obj->m_SizeOfShadowFacesData + 3) {
                break;
            }
            auto* const out = &obj->m_ShadowFacesData[obj->m_SizeOfShadowFacesData];
            out[0] = w0;
            out[1] = w2;
            out[2] = w1;
        }
        obj->m_SizeOfShadowFacesData += 3;
    }

    // Shadow volume's sides (Silhouette edges extruded)
    if (numEdges != 0) {
        const float ex = s_SunPosNrm.x * extrudeDist;
        const float ey = s_SunPosNrm.y * extrudeDist;
        const float ez = s_SunPosNrm.z * extrudeDist;

        for (auto e = 0; e < numEdges; e++) {
            const CVector& p = worldPoints[edges[e * 2]];
            const CVector& q = worldPoints[edges[e * 2 + 1]];

            const CVector extP{ ex + p.x, p.y + ey, p.z + ez };
            const CVector extQ{ q.x + ex, q.y + ey, q.z + ez };

            if ((int32)obj->m_FaceID <= (int32)obj->m_SizeOfShadowFacesData + 6) {
                break;
            }
            auto* const out = &obj->m_ShadowFacesData[obj->m_SizeOfShadowFacesData];
            out[0] = p;
            out[1] = q;
            out[2] = extP;
            out[3] = q;
            out[4] = extQ;
            out[5] = extP;
            obj->m_SizeOfShadowFacesData += 6;
        }
    }
}

void CStencilShadows::InjectHooks() {
    RH_ScopedClass(CStencilShadows);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x70F9E0);
    RH_ScopedInstall(Shutdown, 0x711390);
    RH_ScopedInstall(Process, 0x711D90);
    RH_ScopedInstall(GraphicsHighQuality, 0x70F9B0);
    RH_ScopedInstall(UpdateHierarchy, 0x710BC0);
    RH_ScopedInstall(RegisterStencilShadows, 0x711760);
    RH_ScopedInstall(RenderStencilShadows, 0x7113B0);
    RH_ScopedInstall(RenderForVehicle, 0x70FAE0);
    RH_ScopedInstall(RenderForObject, 0x710310);
    RH_ScopedInstall(Render, 0x710D50);
    RH_ScopedInstall(RenderBuffer, 0x710B50);
    RH_ScopedInstall(SunSetPositionFromEntity, 0x710AF0);
    RH_ScopedInstall(sub_710CC0, 0x710CC0);
}

// 0x70F9E0
void CStencilShadows::Init() {
    ZoneScoped;

    RwD3D9SetStencilClear(0);
    pFirstAvailableStencilShadowObject = m_StencilShadowObjects.data();
    pFirstActiveStencilShadowObject = nullptr;

    for (auto&& [i, obj] : rngv::enumerate(m_StencilShadowObjects)) {
        obj.m_pOwner                = nullptr;
        obj.m_NumShadowFaces        = 0;
        obj.m_Type                  = eStencilShadowObjType::NONE;
        obj.m_SizeOfShadowFacesData = 0;
        obj.m_FaceID                = 0;
        obj.m_ShadowFacesData       = 0;

        obj.m_pPrev = i ? &m_StencilShadowObjects[i - 1] : nullptr;
        obj.m_pNext = (i != m_StencilShadowObjects.size() - 1) ? &m_StencilShadowObjects[i + 1] : nullptr;
    }
}

// 0x711390
void CStencilShadows::Shutdown() {
    for (auto* obj = pFirstActiveStencilShadowObject; obj;) {
        auto* next = obj->m_pNext;
        obj->Destroy();
        obj = next;
    }
}

// 0x710D50
void CStencilShadows::Render(const CRGBA& color) {
    uiTempBufferIndicesStored  = 0;
    uiTempBufferVerticesStored = 0;

    const auto intColor = color.ToIntARGB();

    for (auto* obj = pFirstActiveStencilShadowObject; obj; obj = obj->m_pNext) {
        auto* const data = obj->m_ShadowFacesData;
        const auto  size = (int32)obj->m_SizeOfShadowFacesData; // Number of vertices

        // 2 triangles at a time
        const auto numQuads = size / 6;
        for (auto q = 0; q < numQuads; q++) {
            const auto* const verts = &data[q * 6];

            sub_710CC0(6, 6);

            const auto firstVert = (size_t)uiTempBufferVerticesStored;
            const auto firstIdx  = (size_t)uiTempBufferIndicesStored;
            for (auto i = 0u; i < 6; i++) {
                aTempBufferIndices[firstIdx + i]            = (RxVertexIndex)(firstIdx + i);
                RwCompatVertexPos(TempBufferVertices.m_3d[firstVert + i]) = verts[i];
                TempBufferVertices.m_3d[firstVert + i].color     = intColor;
            }
            uiTempBufferIndicesStored  += 6;
            uiTempBufferVerticesStored += 6;
        }

        // Remaining triangle (if any)
        if ((size / 3) & 1) {
            const auto* const verts = &data[numQuads * 6];

            sub_710CC0(3, 3);

            const auto firstVert = (size_t)uiTempBufferVerticesStored;
            const auto firstIdx  = (size_t)uiTempBufferIndicesStored;
            for (auto i = 0u; i < 3; i++) {
                aTempBufferIndices[firstIdx + i]            = (RxVertexIndex)(firstIdx + i);
                RwCompatVertexPos(TempBufferVertices.m_3d[firstVert + i]) = verts[i];
                TempBufferVertices.m_3d[firstVert + i].color     = intColor;
            }
            uiTempBufferIndicesStored  += 3;
            uiTempBufferVerticesStored += 3;
        }
    }

    if (uiTempBufferIndicesStored && uiTempBufferVerticesStored) {
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
        LittleTest();
        if (RwIm3DTransform(TempBufferVertices.m_3d, uiTempBufferVerticesStored, nullptr, rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA)) {
            RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, aTempBufferIndices, uiTempBufferIndicesStored);
            RwIm3DEnd();
        }
        uiTempBufferVerticesStored = uiTempBufferIndicesStored = 0;
    }
}

// unused
// 0x710AF0
void CStencilShadows::SunSetPositionFromEntity(const CEntity* entity) {
    if (!entity) {
        return;
    }
    s_SunPosNrm = entity->GetPosition().Normalized();
}

// 0x710B50
void CStencilShadows::RenderBuffer(const CVector& pos) {
    s_SunPosNrm = pos.Normalized(); 
}

// 0x710CC0
void CStencilShadows::sub_710CC0(int32 indices, int32 vertices) {
    if (uiTempBufferIndicesStored + indices < TOTAL_TEMP_BUFFER_INDICES
        && uiTempBufferVerticesStored + vertices < TOTAL_TEMP_BUFFER_3DVERTICES) {
        return;
    }

    if (!uiTempBufferIndicesStored || !uiTempBufferVerticesStored) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
    LittleTest();
    if (RwIm3DTransform(TempBufferVertices.m_3d, uiTempBufferVerticesStored, nullptr, rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA)) {
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, aTempBufferIndices, uiTempBufferIndicesStored);
        RwIm3DEnd();
    }
    uiTempBufferVerticesStored = uiTempBufferIndicesStored = 0;
}

// 0x7113B0
void CStencilShadows::RenderStencilShadows() {
    ZoneScoped;

    if (!GraphicsHighQuality()) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,             RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESTENCILENABLE,            RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,                RWRSTATE(rwSHADEMODEFLAT));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,                RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,            RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,        RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,                 RWRSTATE(rwBLENDZERO));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,                RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONMASK,      RWRSTATE(uint32(-1)));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONWRITEMASK, RWRSTATE(uint32(-1)));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTION,          RWRSTATE(rwSTENCILFUNCTIONALWAYS));
    RwRenderStateSet(rwRENDERSTATESTENCILFAIL,              RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILZFAIL,             RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILPASS,              RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONREF,       RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,              RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESTENCILZFAIL,             RWRSTATE(rwSTENCILOPERATIONINCR));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLFRONT));

    Render(CRGBA{ 0, 0, 0, 255 });

    RwRenderStateSet(rwRENDERSTATESTENCILZFAIL,             RWRSTATE(rwSTENCILOPERATIONDECR));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLBACK));

    Render(CRGBA{ 0, 0, 0, 255 });

    // WTF is up with these states?
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,        RWRSTATE(FALSE)); // same state
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,             RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,              RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESTENCILENABLE,            RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,                RWRSTATE(rwSHADEMODEGOURAUD));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLBACK));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,             RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,              RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESTENCILENABLE,            RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,                RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,        RWRSTATE(TRUE)); // same state
    RwRenderStateSet(rwRENDERSTATESRCBLEND,                 RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,                RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,                RWRSTATE(rwSHADEMODEFLAT));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,            RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLNONE));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONREF,       RWRSTATE(1u));
    RwRenderStateSet(rwRENDERSTATESTENCILFUNCTION,          RWRSTATE(rwSTENCILFUNCTIONLESSEQUAL));
    RwRenderStateSet(rwRENDERSTATESTENCILFAIL,              RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILZFAIL,             RWRSTATE(rwSTENCILOPERATIONKEEP));
    RwRenderStateSet(rwRENDERSTATESTENCILPASS,              RWRSTATE(rwSTENCILOPERATIONKEEP));

    CSprite2d::InitPerFrame();
    CSprite2d::DrawRect(
        CRect{ 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT },
        CRGBA{ 0, 0, 0, (uint8)(50u * CTimeCycle::m_CurrentColours.m_nShadowStrength / 256) }
    );

    RwRenderStateSet(rwRENDERSTATESTENCILENABLE,            RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,             RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,              RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,        RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,                RWRSTATE(rwSHADEMODEGOURAUD));
    RwRenderStateSet(rwRENDERSTATECULLMODE,                 RWRSTATE(rwCULLMODECULLBACK));
}

// 0x70FAE0
void CStencilShadows::RenderForVehicle(CStencilShadowObject* object) {
    auto* const entity = object->m_pOwner;

    float extrudeDist = 5.0f;
    if (entity->GetIsTypeVehicle()) {
        const auto subType = entity->AsVehicle()->m_nVehicleSubType;
        if (subType == VEHICLE_TYPE_PLANE || subType == VEHICLE_TYPE_HELI) {
            extrudeDist = 40.0f;
        }
    }

    auto* const colData = entity->GetColModel()->m_pColData;
    if (!colData || (int32)colData->m_nNumShadowTriangles != (int32)object->m_NumShadowFaces) {
        return;
    }

    CMatrix* matrix = &entity->GetMatrix();
    if (entity->GetIsTypeVehicle() && entity->AsVehicle()->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        auto* const bike = entity->AsBike();
        bike->CalculateLeanMatrix();
        matrix = &bike->m_mLeanMatrix;
    }

    BuildShadowVolume(object, colData, *matrix, extrudeDist);
}

// 0x710310
void CStencilShadows::RenderForObject(CStencilShadowObject* object) {
    auto* const entity = object->m_pOwner;

    auto* const colData = entity->GetColModel()->m_pColData;
    if (!colData || (int32)colData->m_nNumShadowTriangles != (int32)object->m_NumShadowFaces) {
        return;
    }

    BuildShadowVolume(object, colData, entity->GetMatrix(), 60.0f);
}

// 0x711D90
void CStencilShadows::Process(CVector& cameraPos) {
    ZoneScoped;

    if (!GraphicsHighQuality()) {
        return;
    }

    static uint8 s_RegisterShadowCounter{}, s_RenderForObjCounter{};

    RegisterStencilShadows(cameraPos, ++s_RegisterShadowCounter % 8);

    // why do we even do this?
    s_ShadowTrianglePointsUnk         = (RxVertexIndex*)CMemoryMgr::Malloc(12'288 * sizeof(RxVertexIndex));
    s_ShadowTrianglePoints            = (CVector*)CMemoryMgr::Malloc(2'048 * sizeof(CVector));
    s_TransformedShadowTrianglePoints = (CVector*)CMemoryMgr::Malloc(2'048 * sizeof(CVector));

    auto i{ 0 };
    for (auto* obj = pFirstActiveStencilShadowObject; obj; obj = obj->m_pNext) {
        switch (obj->m_Type) {
        case eStencilShadowObjType::OBJECT:
            if ((i++ % 4) == s_RenderForObjCounter) {
                RenderForObject(obj);
            }
            break;
        case eStencilShadowObjType::VEHICLE:
            RenderForVehicle(obj);
            break;
        default:
            break;
        }
    }
    s_RenderForObjCounter = (s_RenderForObjCounter + 1) % 4;

    CMemoryMgr::Free(std::exchange(s_ShadowTrianglePointsUnk, nullptr));
    CMemoryMgr::Free(std::exchange(s_ShadowTrianglePoints, nullptr));
    CMemoryMgr::Free(std::exchange(s_TransformedShadowTrianglePoints, nullptr));
}

// 0x70F9B0
bool CStencilShadows::GraphicsHighQuality() {
    return ::GraphicsHighQuality();
}

// 0x710BC0
void CStencilShadows::UpdateHierarchy(CStencilShadowObject*& firstAvailable, CStencilShadowObject*& firstActive, CStencilShadowObject* newOne) {
    if (auto* prev = newOne->m_pPrev) {
        auto* next = newOne->m_pNext;
        if (next) {
            next->m_pPrev = prev;
            newOne->m_pPrev->m_pNext = newOne->m_pNext;
        } else {
            prev->m_pNext = nullptr;
        }
    } else {
        auto* next     = newOne->m_pNext;
        firstAvailable = next;
        if (next) {
            next->m_pPrev = nullptr;
        }
    }
    newOne->m_pNext = firstActive;
    newOne->m_pPrev = nullptr;
    firstActive     = newOne;

    if (newOne->m_pNext) {
        newOne->m_pNext->m_pPrev = newOne;
    }
}

// 0x711760
bool CStencilShadows::RegisterStencilShadows(CVector& cameraPos, bool doNotCreateNew) {
    if (doNotCreateNew) {
        // Only get rid of the unsuitable ones
        for (auto* obj = pFirstActiveStencilShadowObject; obj;) {
            auto* const next = obj->m_pNext;
            if (!obj->m_pOwner || IsObjectNotSuitableForShadow(obj->m_pOwner)) {
                obj->Destroy();
            }
            obj = next;
        }
        return true;
    }

    // Remove shadows that are no longer needed
    for (auto* obj = pFirstActiveStencilShadowObject; obj;) {
        auto* const next   = obj->m_pNext;
        auto* const entity = obj->m_pOwner;
        if (!entity || IsObjectNotSuitableForShadow(entity)) {
            obj->Destroy();
        } else {
            switch (obj->m_Type) {
            case eStencilShadowObjType::OBJECT:
                if (GetSignedSquaredDistanceToBoundSphere(entity, cameraPos) > 2500.0f || !entity->IsInCurrentArea()) {
                    obj->Destroy();
                }
                break;
            case eStencilShadowObjType::VEHICLE:
                if (GetSignedSquaredDistanceToBoundSphere(entity, cameraPos) > 2500.0f) {
                    obj->Destroy();
                }
                break;
            default:
                break;
            }
        }
        obj = next;
    }

    if (!pFirstAvailableStencilShadowObject) {
        return true;
    }

    // Vehicles
    for (auto& veh : GetVehiclePool()->GetAllValid()) {
        auto* const colModel = veh.GetColModel();
        if (!colModel || !colModel->m_pColData || !colModel->m_pColData->bHasShadowInfo) {
            continue;
        }
        if (!veh.m_pCollisionList.m_node) { // Has collided with something?
            continue;
        }
        if (FindShadowObjectOfEntity(&veh)) {
            continue;
        }
        if (!(GetSignedSquaredDistanceToBoundSphere(&veh, cameraPos) <= 2500.0f)) {
            continue;
        }
        if (!pFirstAvailableStencilShadowObject) {
            return false;
        }
        CreateShadowObjectFor(&veh, eStencilShadowObjType::VEHICLE);
        if (!pFirstAvailableStencilShadowObject) {
            return false;
        }
    }

    if (!pFirstAvailableStencilShadowObject) {
        return true;
    }

    // Objects (Buildings and dynamic objects) around the camera
    const auto GetSectorCoord = [](float v) {
        return (int32)std::floor((double)(float)((double)(v * 0.02f) + 60.0));
    };
    const auto camX = cameraPos.x, camY = cameraPos.y;
    const auto minX = std::max(0, GetSectorCoord(camX - 50.0f));
    const auto minY = std::max(0, GetSectorCoord(camY - 50.0f));
    const auto maxX = std::min(MAX_SECTORS_X - 1, GetSectorCoord(camX + 50.0f));
    const auto maxY = std::min(MAX_SECTORS_Y - 1, GetSectorCoord(camY + 50.0f));

    CWorld::AdvanceCurrentScanCode();

    // Returns false if we ran out of shadow objects
    const auto TryRegister = [&](CEntity* entity) -> bool {
        if (entity->m_bIsProcObject) {
            return true;
        }
        if (IsObjectNotSuitableForShadow(entity)) {
            return true;
        }
        auto* const colModel = entity->GetColModel();
        if (!colModel || !colModel->m_pColData || !colModel->m_pColData->bHasShadowInfo) {
            return true;
        }
        if (entity->GetScanCode() == CWorld::ms_nCurrentScanCode) {
            return true;
        }
        entity->SetScanCode(CWorld::ms_nCurrentScanCode);
        if (!entity->IsInCurrentArea()) {
            return true;
        }
        if (entity->GetModelInfo()->GetModelType() != MODEL_INFO_ATOMIC) {
            return true;
        }
        if (FindShadowObjectOfEntity(entity)) {
            return true;
        }
        if (!(GetSignedSquaredDistanceToBoundSphere(entity, cameraPos) <= 2500.0f)) {
            return true;
        }
        if (!pFirstAvailableStencilShadowObject) {
            return false;
        }
        CreateShadowObjectFor(entity, eStencilShadowObjType::OBJECT);
        return pFirstAvailableStencilShadowObject != nullptr;
    };

    for (auto y = minY; y <= maxY; y++) {
        for (auto x = minX; x <= maxX; x++) {
            for (auto* const entity : CWorld::GetSector(x, y).Buildings) {
                if (!TryRegister(entity)) {
                    return false;
                }
            }
            for (auto* const entity : CWorld::GetRepeatSector(x, y).Objects) {
                if (!TryRegister(entity)) {
                    return false;
                }
            }
        }
    }

    return true;
}
