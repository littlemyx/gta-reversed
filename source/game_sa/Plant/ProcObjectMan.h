#pragma once

#include "List_c.h"
#include "ProcSurfaceInfo.h"

class ProcSurfaceInfo_c;
class CObject;
class CEntity;
class CPlantLocTri;

class ProcObjectListItem : public ListItem_c<ProcObjectListItem> {
public:
    CEntity*      m_Obj;
    CPlantLocTri* m_LocTri;
    bool          m_bAllocatedMatrix;
};
VALIDATE_SIZE(ProcObjectListItem, 0x14);

class ProcObjectMan_c {
public:
    int32 m_numAllocatedMatrices;
    int32 m_numProcSurfaceInfos;
    ProcSurfaceInfo_c           m_ProcObjSurfaceInfos[128];
    ProcObjectListItem          m_Objects[512];
    TList_c<ProcObjectListItem> m_ObjectsList;

public:
    static void InjectHooks();

    ProcObjectMan_c() = default;  // 0x5A3E20
    ~ProcObjectMan_c() = default; // 0x5A3090

    void Init();
    void Update();
    void Exit();
    void LoadDataFile();

    ProcObjectListItem* GetEntityFromPool();
    void ReturnEntityToPool(ProcObjectListItem* item);

    int32 ProcessTriangleAdded(CPlantLocTri* plant);
    void ProcessTriangleRemoved(CPlantLocTri* plant);
};
VALIDATE_SIZE(ProcObjectMan_c, 0x4C14);

NOTSA_GLOBAL_HDR(g_procObjMan, 0xBB7CB0, (ProcObjectMan_c), {}); VALIDATE_SIZE(g_procObjMan, 19476);
