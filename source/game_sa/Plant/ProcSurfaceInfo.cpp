#include "StdInc.h"

#include "ProcSurfaceInfo.h"
#include "ProcObjectMan.h"
#include "PlantLocTri.h"

void ProcSurfaceInfo_c::InjectHooks() {
    RH_ScopedClass(ProcSurfaceInfo_c);
    RH_ScopedCategory("Plant");

    RH_ScopedInstall(Init, 0x5A2EB0);
    RH_ScopedInstall(Exit, 0x5A3270);
    RH_ScopedInstall(AddObject, 0x5A32D0);
    RH_ScopedInstall(AddObjects, 0x5A3850);
}

// 0x5A2EB0
void ProcSurfaceInfo_c::Init(
    const char* surfaceType,
    const char* modelName,
    float       spacing,
    float       minDist,
    int32       minRot,
    int32       maxRot,
    float       minScale,
    float       maxScale,
    float       minScaleZ,
    float       maxScaleZ,
    float       zOffsetMin,
    float       zOffsetMax,
    bool        align,
    uint8       useGrid
) {
    m_SurfaceId = g_surfaceInfos.GetSurfaceIdFromName(surfaceType);
    auto mi     = CModelInfo::GetModelInfo(modelName, &m_ModelIndex); // todo: int16 cast here, why?
    if (!mi) {
        return;
    }

    minDist = std::min(minDist, 80.0f); // 0x5A2EC9: fcomp 80.0, jne (<=, NaN) skips the store: minDist > 80 becomes 80 (procobj.dat 50/60/70/80 keep their value)

    m_fMinScale             = minScale;
    m_fMaxScale             = maxScale;
    m_fSpacing              = spacing;
    m_fMinScaleZ            = minScaleZ;
    m_fMaxScaleZ            = maxScaleZ;
    m_fOffsetMinZ           = zOffsetMin;
    m_fOffsetMaxZ           = zOffsetMax;
    m_Align                 = align;
    m_fSquaredSpacingRadius = 1.0f / (spacing * spacing);
    m_fSquaredMinDistance   = minDist * minDist;
    m_fMinRotInRadians      = DegreesToRadians(float(minRot));
    m_fMaxRotInRadians      = DegreesToRadians(float(maxRot));
    m_fUseGrid              = float(useGrid);
}

// 0x5A3270
void ProcSurfaceInfo_c::Exit() {
    for (auto it = m_Objects.GetHead(); it;) {
        auto* next = m_Objects.GetNext(it);

        m_Objects.RemoveItem(it);
        g_procObjMan.m_ObjectsList.AddItem(it);
        it->m_Obj->DeleteRwObject();
        CWorld::Remove(it->m_Obj);
        delete std::exchange(it->m_Obj, nullptr);

        it = next;
    }
}

// 0x5A32D0
ProcObjectListItem* ProcSurfaceInfo_c::AddObject(CVector pos, CVector normal, tColLighting lighting) {
    //Original function has some exception handling built in, this is ignored in reversed code
    auto head = g_procObjMan.m_ObjectsList.RemoveHead();
    if (!head || m_Align && normal.z < 0.95f && g_procObjMan.m_numAllocatedMatrices >= 200) {
        return nullptr;
    }

    auto modelInfo  = CModelInfo::GetModelInfo(m_ModelIndex);
    bool isBuilding = modelInfo->m_nObjectInfoIndex == -1;
    if (isBuilding) {
        GetBuildingPool()->SetDealWithNoMemory(true);
        head->m_Obj                           = new CBuilding();
        GetBuildingPool()->SetDealWithNoMemory(false);
        if (!head->m_Obj) {
            return nullptr;
        }

        head->m_Obj->SetModelIndexNoCreate(m_ModelIndex);
    } else {
        if (CObject::nNoTempObjects >= 150) {
            return nullptr;
        }

        GetObjectPool()->SetDealWithNoMemory(true);
        head->m_Obj                         = new CObject(m_ModelIndex, false);
        GetObjectPool()->SetDealWithNoMemory(false);
        if (!head->m_Obj) {
            return nullptr;
        }

        head->m_Obj->AsObject()->m_nObjectType = eObjectType::OBJECT_TYPE_DECORATION;
        head->m_Obj->AsObject()->SetVelocity({ 0.f, 0.f, 0.f });
        ++CObject::nNoTempObjects;
    }

    if (g_surfaceInfos.IsWater(m_SurfaceId)) {
        head->m_Obj->m_bUnderwater = true;
    }
    head->m_Obj->m_bIsProcObject = true;

    auto xyScale     = lerp(m_fMinScale, m_fMaxScale, CGeneral::GetRandomNumberInRange(0.f, 1.f));
    auto zScale      = lerp(m_fMinScaleZ, m_fMaxScaleZ, CGeneral::GetRandomNumberInRange(0.f, 1.f));
    auto rotation    = lerp(m_fMinRotInRadians, m_fMaxRotInRadians, CGeneral::GetRandomNumberInRange(0.f, 1.f));
    auto zOffset     = lerp(m_fOffsetMinZ, m_fOffsetMaxZ, CGeneral::GetRandomNumberInRange(0.f, 1.f));
    auto minBounding = modelInfo->GetColModel()->GetBoundingBox().m_vecMin;

    if (m_Align && normal.z < 0.95f) {
        CMatrix tempMatrix{};
        CVector rotVec = { (float)(x87::cos(rotation)), (float)(x87::sin(rotation)), 0.f };

        CVector forwardVec = normal.Cross(rotVec).Normalized();
        auto    rightVec   = forwardVec.Cross(normal).Normalized();
        auto    zMult      = zOffset - minBounding.z;

        tempMatrix.GetRight()    = rightVec;
        tempMatrix.GetForward()  = forwardVec;
        tempMatrix.GetUp()       = normal;
        tempMatrix.GetPosition() = pos + (normal * zMult);

        head->m_Obj->AllocateStaticMatrix();
        head->m_Obj->SetMatrix(tempMatrix);
        head->m_bAllocatedMatrix = true;
        ++g_procObjMan.m_numAllocatedMatrices;
    } else {
        pos.z = (pos.z - minBounding.z) + zOffset; // 0x5A36EA: fld pos.z; fsub minBounding.z; fadd zOffset
        head->m_Obj->SetPosn(pos);
        head->m_Obj->SetOrientation(0.f, 0.f, rotation);
        head->m_bAllocatedMatrix = false;
    }

    head->m_Obj->SetAreaCode(static_cast<eAreaCodes>(CGame::currArea));
    head->m_Obj->SetIsStatic(true);
    head->m_Obj->CreateRwObject();
    head->m_Obj->UpdateRwMatrix();
    head->m_Obj->UpdateRwFrame();
    CWorld::Add(head->m_Obj);
    m_Objects.AddItem(head);
    // 0x5A379E-0x5A37C9: cmp [esp+1F],0 / jne end: the scale block is for BUILDINGS only (the byte is `objectInfoIndex != -1`); xyScale != 1 jumps
    // into it, else zScale == 1 leaves (so `||`, the port had `!isBuilding && (xy != 1 && z != 1)`)
    if (isBuilding && (xyScale != 1.0f || zScale != 1.0f)) {
        auto* rwObject = head->m_Obj->GetRwObject();
        if (rwObject && rwObjectGetParent(rwObject) != (void*)-16) { // I don't understand this if, there must be some missing macro
            auto* modellingMatrix = head->m_Obj->GetModellingMatrix();

            auto tempMatrix = CMatrix(modellingMatrix, false);
            tempMatrix.ScaleXYZ(xyScale, xyScale, zScale);
            tempMatrix.GetPosition().z -= (minBounding.z * zScale - minBounding.z);
            tempMatrix.UpdateRW();

            head->m_Obj->UpdateRwFrame();
        }
    }
    return head;
}

// 0x5A3850
int32 ProcSurfaceInfo_c::AddObjects(CPlantLocTri* plant) {
    auto centerPos = (plant->m_V1 + plant->m_V2 + plant->m_V3) * 0.3333f; // 0x867028 (NOT 1/3: 0x3EAAA64C)
    auto camPos    = TheCamera.GetPosition();
    {
        // 0x5A38C9-0x5A38F6: squared distance summed z,y,x (DistanceBetweenPointsSquared sums x,y,z)
        const float dx = camPos.x - centerPos.x;
        const float dy = camPos.y - centerPos.y;
        const float dz = camPos.z - centerPos.z;
        if ((dz * dz + dy * dy) + dx * dx < m_fSquaredMinDistance) {
            return 0;
        }
    }

    auto vec1   = plant->m_V2 - plant->m_V1;
    auto vec2   = plant->m_V3 - plant->m_V2;
    auto vec3   = plant->m_V3 - plant->m_V1;
    auto normal = vec3.Cross(vec1);

    auto numAdded = 0;
    if (m_fUseGrid == 0.f) {
        // 0x5A3C23: |normal| summed z,y,x; then x 0.5 x spacing factor
        auto density = std::sqrt((normal.z * normal.z + normal.y * normal.y) + normal.x * normal.x) * 0.5f * m_fSquaredSpacingRadius;

        // 0x5A3C6B-0x5A3C9A: per vertex (z + y) + x, then (V3 + V2) + V1, + model index (the port summed the vector V1+V2+V3 first, then x,y,z)
        const float sumV3 = (plant->m_V3.z + plant->m_V3.y) + plant->m_V3.x;
        const float sumV2 = (plant->m_V2.z + plant->m_V2.y) + plant->m_V2.x;
        const float sumV1 = (plant->m_V1.z + plant->m_V1.y) + plant->m_V1.x;
        auto randSeed    = ((sumV3 + sumV2) + sumV1) + (float)m_ModelIndex;
        auto oldRandSeed = rand(); // R* usage of srand and rand seems rather weird.
        srand(NOTSA_FTOL(uint32, randSeed));

        if (!(density > 0.0f)) { // `fcomp 0; test ah,0x41; jne`: <= 0 and NaN leave
            srand(oldRandSeed);
            return 0;
        }

        for (density; density > 0.f; density -= 1.f) {
            auto cutOff = 1.f;
            if (density < 1.f) {
                cutOff = CGeneral::GetRandomNumberInRange(0.f, 1.f);
            }

            if (cutOff < density) {
                auto offset1 = CGeneral::GetRandomNumberInRange(0.f, 1.f);
                auto offset2 = offset1 * CGeneral::GetRandomNumberInRange(0.f, 1.f);

                //FIX_BUGS candidate probably, seems like the result of this calculation can fall outside of the triangle that it's based on
                auto objPos   = plant->m_V1 + (vec1 * offset1) + (vec2 * offset2);
                normal.Normalise(); // 0x5A3D4B: inside the loop, after the position: every accepted candidate renormalises the (already unit) normal
                auto addedObj = AddObject(objPos, normal, plant->m_nLighting);
                if (addedObj) {
                    numAdded++;
                    addedObj->m_LocTri = plant;
                }
            }
        }
        srand(oldRandSeed);
        return numAdded;
    }

    auto [minX, maxX] = std::minmax({ plant->m_V1.x, plant->m_V2.x, plant->m_V3.x });
    auto [minY, maxY] = std::minmax({ plant->m_V1.y, plant->m_V2.y, plant->m_V3.y });

    // A mess, but required one, could do that with some floorf and a single cast per line, but then the results would differ from original one for negative numbers
    auto step = 1.f / m_fSpacing;
    auto xCur = float(NOTSA_FTOL(int32, NOTSA_FTOL(int32, minX * step) * m_fSpacing));
    auto xEnd = float(NOTSA_FTOL(int32, (NOTSA_FTOL(int32, maxX * step) + 1) * m_fSpacing));
    auto yCur = float(NOTSA_FTOL(int32, NOTSA_FTOL(int32, minY * step) * m_fSpacing));
    auto yEnd = float(NOTSA_FTOL(int32, (NOTSA_FTOL(int32, maxY * step) + 1) * m_fSpacing));
    normal.Normalise();

    for (xCur; xCur < xEnd; xCur += m_fSpacing) {
        // 0x5A3AF0: the exe reloads the start y for every x column (the port kept the exhausted yCur: only the first column got objects)
        for (float y = yCur; y < yEnd; y += m_fSpacing) {
            float zPos;
            if (!IsPtInTriangle2D(xCur, y, plant->m_V1, plant->m_V2, plant->m_V3, normal, &zPos)) {
                continue;
            }

            auto addedObj = AddObject({ xCur, y, zPos }, normal, plant->m_nLighting);
            if (addedObj) {
                numAdded++;
                addedObj->m_LocTri = plant;
            }
        }
    }

    return numAdded;
}

// 0x5A2F80, Fucked up calling convention, can't hook
bool ProcSurfaceInfo_c::IsPtInTriangle2D(float x, float y, CVector v1, CVector v2, CVector v3, CVector normal, float* z) {
    if ((v1.x - v2.x) * (v1.y - y) + (v2.y - v1.y) * (v1.x - x) > 0.0f
        || (v2.x - v3.x) * (v2.y - y) + (v3.y - v2.y) * (v2.x - x) > 0.0f
        || (v1.y - v3.y) * (v3.x - x) + (v3.x - v1.x) * (v3.y - y) > 0.0f) {
        return false;
    }

    *z = (-(normal.x * x) - normal.y * y - -(v1.z * normal.z + normal.y * v1.y + normal.x * v1.x)) / normal.z;
    return true;
}
