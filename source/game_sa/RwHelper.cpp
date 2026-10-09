#include "StdInc.h"

#include "RwHelper.h"

#include <cmath>

void RwHelperInjectHooks() {
    RH_ScopedNamespaceName("RwHelper");
    RH_ScopedCategoryGlobal();

    RH_ScopedGlobalInstall(GetEventGlobalGroup, 0x4ABA50);
    RH_ScopedGlobalInstall(GetNameAndDamage, 0x5370A0);
    RH_ScopedGlobalInstall(GetFirstAtomicCallback, 0x734810);
    RH_ScopedGlobalInstall(GetFirstAtomic, 0x734820);
    RH_ScopedGlobalInstall(Get2DEffectAtomicCallback, 0x734850);
    RH_ScopedGlobalInstall(Get2DEffectAtomic, 0x734880);
    RH_ScopedGlobalInstall(GetFirstObjectCallback, 0x7348B0);
    RH_ScopedGlobalInstall(GetFirstObject, 0x7348C0);
    RH_ScopedGlobalInstall(GetFirstFrameCallback, 0x7348F0);
    RH_ScopedGlobalInstall(GetFirstChild, 0x734900);
    RH_ScopedGlobalInstall(SkinAtomicGetHAnimHierarchCB, 0x734A20);
    RH_ScopedGlobalInstall(GetAnimHierarchyFromSkinClump, 0x734A40);
    RH_ScopedGlobalInstall(GetAnimHierarchyFromFrame, 0x734AB0);
    RH_ScopedGlobalInstall(GetAnimHierarchyFromClump, 0x734B10);
    RH_ScopedGlobalInstall(AtomicRemoveAnimFromSkinCB, 0x734B90);
    RH_ScopedGlobalInstall(RpAtomicConvertGeometryToTL, 0x734BE0);
    RH_ScopedGlobalInstall(RpAtomicConvertGeometryToTS, 0x734C20);
    RH_ScopedGlobalInstall(atomicConvertGeometryToTL, 0x734C60);
    RH_ScopedGlobalInstall(RpClumpConvertGeometryToTL, 0x734CB0);
    RH_ScopedGlobalInstall(atomicConvertGeometryToTS, 0x734CE0);
    RH_ScopedGlobalInstall(RpClumpConvertGeometryToTS, 0x734D30);
    RH_ScopedGlobalInstall(forceLinearFilteringAtomicsCB, 0x734DA0);
    RH_ScopedGlobalInstall(SetFilterModeOnClumpsTextures, 0x734DC0);
    RH_ScopedGlobalInstall(forceLinearFilteringMatTexturesCB, 0x734D60);
    RH_ScopedGlobalInstall(SetFilterModeOnAtomicsTextures, 0x734D80);
    RH_ScopedGlobalInstall(RpGeometryReplaceOldMaterialWithNewMaterial, 0x734DE0);
    RH_ScopedGlobalInstall(RwTexDictionaryFindHashNamedTexture, 0x734E50);
    RH_ScopedGlobalInstall(RpClumpGetBoundingSphere, 0x734FC0);
    RH_ScopedGlobalInstall(SkinGetBonePositions, 0x735140);
    RH_ScopedGlobalInstall(SkinSetBonePositions, 0x7352D0);
    RH_ScopedGlobalInstall(SkinGetBonePositionsToTable, 0x735360);
    RH_ScopedGlobalInstall(RemoveRefsCB, 0x7226D0);
}

// 0x4ABA50
CEventGlobalGroup* GetEventGlobalGroup() {
    static auto& globalEvents = StaticRef<CEventGlobalGroup*>(0xA9AF6C);

    if (globalEvents)
        return globalEvents;

    globalEvents = new CEventGlobalGroup(nullptr);
    return globalEvents;
}

// TODO: Check `outName` size (to avoid buffer overflow)
// 0x5370A0
void GetNameAndDamage(const char* name, char* objName, bool& bIsDamageModel) {
    const size_t nodesz = strlen(name);

    const auto TerminatedCopy = [=](size_t offset) {
        strncpy_s(objName, nodesz - offset + 1, name, nodesz - offset);
        objName[nodesz - offset] = 0;
    };

    // EndsWith "_dam"
    if (name[nodesz - 4] == '_' && name[nodesz - 3] == 'd' && name[nodesz - 2] == 'a' && name[nodesz - 1] == 'm'
    ) {
        bIsDamageModel = true;
        TerminatedCopy(sizeof("_dam") - 1);
    }
    else {
        bIsDamageModel = false;
        // EndsWith "_l0" or "_L0"
        if (name[nodesz - 3] == '_' &&
            (name[nodesz - 2] == 'L' || name[nodesz - 2] == 'l') && name[nodesz - 1] == '0'
        ) {
            TerminatedCopy(sizeof("_l0") - 1);
        } else
            strcpy_s(objName, strlen(name) + 1, name);
    }
}

// 0x734810
RpAtomic* GetFirstAtomicCallback(RpAtomic* atomic, void* data) {
    *(RpAtomic**)(data) = atomic;
    return nullptr;
}

// 0x734820
RpAtomic* GetFirstAtomic(RpClump* clump) {
    RpAtomic* atomic{};
    RpClumpForAllAtomics(clump, GetFirstAtomicCallback, &atomic);
    return atomic;
}

// 0x734850
RpAtomic* Get2DEffectAtomicCallback(RpAtomic* atomic, void* data) {
    if (RpGeometryGet2dFxCount(RpAtomicGetGeometry(atomic)) > 0) { // The original reads the plugin data directly (`C2dEffect::g2dEffectPluginOffset`)
        *(RpAtomic**)(data) = atomic;
        return nullptr;
    }
    return atomic;
}

// 0x734880
RpAtomic* Get2DEffectAtomic(RpClump* clump) {
    RpAtomic* atomic{};
    RpClumpForAllAtomics(clump, Get2DEffectAtomicCallback, &atomic);
    return atomic;
}

// 0x7348B0
RwObject* GetFirstObjectCallback(RwObject* object, void* data) {
    *(RwObject**)(data) = object;
    return nullptr;
}

// 0x7348C0
RwObject* GetFirstObject(RwFrame* frame) {
    RwObject* obj{};
    RwFrameForAllObjects(frame, GetFirstObjectCallback, &obj);
    return obj;
}

// 0x7348F0
RwFrame* GetFirstFrameCallback(RwFrame* frame, void* data) {
    *(RwFrame**)(data) = frame;
    return nullptr;
}

// 0x734900
RwFrame* GetFirstChild(RwFrame* frame) {
    RwFrame* child{};
    RwFrameForAllChildren(frame, GetFirstFrameCallback, &child);
    return child;
}

// NOTSA name (the original is a lambda-like function with no symbol)
// 0x734A70
static RwFrame* GetAnimHierarchyFromFrameCB(RwFrame* frame, void* data) {
    if (const auto hier = RpHAnimFrameGetHierarchy(frame)) {
        *(RpHAnimHierarchy**)(data) = hier;
        return nullptr;
    }
    RwFrameForAllChildren(frame, GetAnimHierarchyFromFrameCB, data);
    return frame;
}

// 0x734AB0
RpHAnimHierarchy* GetAnimHierarchyFromFrame(RwFrame* frame) {
    assert(frame);

    RpHAnimHierarchy* hier{};
    if (const auto h = RpHAnimFrameGetHierarchy(frame)) {
        hier = h;
    } else {
        RwFrameForAllChildren(frame, GetAnimHierarchyFromFrameCB, &hier);
    }
    if (!hier) { // The original does the (identical) search a second time
        RwFrameForAllChildren(frame, GetAnimHierarchyFromFrameCB, &hier);
    }
    return hier;
}

// 0x734B10
RpHAnimHierarchy* GetAnimHierarchyFromClump(RpClump* clump) {
    assert(clump);
    return GetAnimHierarchyFromFrame(RpClumpGetFrame(clump));
}

// 0x734A40
RpHAnimHierarchy* GetAnimHierarchyFromSkinClump(RpClump* clump) {
    RpHAnimHierarchy* anim{};
    RpClumpForAllAtomics(clump, SkinAtomicGetHAnimHierarchCB, &anim);
    return anim;
}

// name not from Android
// 0x734A20
RpAtomic* SkinAtomicGetHAnimHierarchCB(RpAtomic* atomic, void* data) {
    *(RpHAnimHierarchy**)(data) = RpSkinAtomicGetHAnimHierarchy(atomic);
    return nullptr;
}

// 0x734B90
RpAtomic* AtomicRemoveAnimFromSkinCB(RpAtomic* atomic, void* data) {
    if (RpSkinGeometryGetSkin(RpAtomicGetGeometry(atomic))) {
        if (RpHAnimHierarchy* hier = RpSkinAtomicGetHAnimHierarchy(atomic)) {
            RtAnimAnimation*& currAnim = hier->currentAnim->pCurrentAnim;
            if (currAnim) {
                RtAnimAnimationDestroy(currAnim);
            }
            currAnim = nullptr;
        }
    }
    return atomic;
}

// 0x734BE0
bool RpAtomicConvertGeometryToTL(RpAtomic* atomic) {
    RpGeometry* geometry = RpAtomicGetGeometry(atomic);

    auto flags = RpGeometryGetFlags(geometry);
    if (flags & rpGEOMETRYNATIVE || !(flags & rpGEOMETRYTRISTRIP))
        return false;

    RpGeometryLock(geometry, rpGEOMETRYLOCKALL);
    RpGeometrySetFlags(geometry, flags & ~rpGEOMETRYTRISTRIP);
    RpGeometryUnlock(geometry);

    return true;
}

// 0x734C20
bool RpAtomicConvertGeometryToTS(RpAtomic* atomic) {
    RpGeometry* geometry = RpAtomicGetGeometry(atomic);

    auto flags = RpGeometryGetFlags(geometry);
    if (flags & rpGEOMETRYNATIVE || flags & rpGEOMETRYTRISTRIP)
        return false;

    RpGeometryLock(geometry, rpGEOMETRYLOCKALL);
    RpGeometrySetFlags(geometry, flags | rpGEOMETRYTRISTRIP);
    RpGeometryUnlock(geometry);

    return true;
}

// 0x734C60
RpAtomic* atomicConvertGeometryToTL(RpAtomic* atomic, void* data) {
    if (!RpAtomicConvertGeometryToTL(atomic)) {
        *(bool*)(data) = false;
    }
    return atomic;
}

// 0x734CB0
bool RpClumpConvertGeometryToTL(RpClump* clump) {
    bool success{ true };
    RpClumpForAllAtomics(clump, atomicConvertGeometryToTL, &success);
    return success;
}

// 0x734CE0
RpAtomic* atomicConvertGeometryToTS(RpAtomic* atomic, void* data) {
    if (!RpAtomicConvertGeometryToTS(atomic)) {
        *(bool*)(data) = false;
    }
    return atomic;
}

// 0x734D30
bool RpClumpConvertGeometryToTS(RpClump* clump) {
    bool success{ true };
    RpClumpForAllAtomics(clump, atomicConvertGeometryToTS, &success);
    return success;
}

// 0x734D60
RpMaterial* forceLinearFilteringMatTexturesCB(RpMaterial* material, void* data) {
    if (RwTexture* texture = RpMaterialGetTexture(material)) {
        RwTextureSetFilterMode(texture, (RwTextureFilterMode)((unsigned)data));
    }
    return material;
}

// 0x734D80
bool SetFilterModeOnAtomicsTextures(RpAtomic* atomic, RwTextureFilterMode filtering) {
    RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), forceLinearFilteringMatTexturesCB, (void*)(unsigned)filtering);
    return true;
}

// 0x734DA0
RpAtomic* forceLinearFilteringAtomicsCB(RpAtomic* atomic, void* data) {
    SetFilterModeOnAtomicsTextures(atomic, (RwTextureFilterMode)((unsigned)data));
    return atomic;
}

// 0x734DC0
bool SetFilterModeOnClumpsTextures(RpClump* clump, RwTextureFilterMode filtering) {
    RpClumpForAllAtomics(clump, forceLinearFilteringAtomicsCB, (void*)(unsigned)filtering);
    return true;
}

// 0x734DE0
bool RpGeometryReplaceOldMaterialWithNewMaterial(RpGeometry* geometry, RpMaterial* oldMaterial, RpMaterial* newMaterial) {
    bool replaced{};

    const auto header = geometry->mesh;
    auto       mesh   = (RpMesh*)(header + 1); // NOTE: `firstMeshOffset` is not used by the original
    for (auto i = (uint32)header->numMeshes; i > 0; i--, mesh++) {
        if (mesh->material != oldMaterial) {
            continue;
        }
        const auto idx = _rpMaterialListFindMaterialIndex(&geometry->matList, mesh->material);
        RpMaterialDestroy(mesh->material);
        geometry->matList.materials[idx] = newMaterial;
        mesh->material                   = newMaterial;
        newMaterial->refCount++;
        replaced = true;
    }
    return replaced;
}

// 0x734E50
RwTexture* RwTexDictionaryFindHashNamedTexture(RwTexDictionary* txd, uint32 hash) {
    const auto end = rwLinkListGetTerminator(&txd->texturesInDict);
    for (auto link = rwLinkListGetFirstLLLink(&txd->texturesInDict); link != end; link = rwLLLinkGetNext(link)) {
        const auto texture = rwLLLinkGetData(link, RwTexture, lInDictionary);
        // NOTE: The original also checked `&texture->name != nullptr` here (always true)
        if (CKeyGen::GetUppercaseKey(texture->name) == hash) { // 0x53CF30
            return texture;
        }
    }
    return nullptr;
}

static auto& s_bUseLTMForClumpBoundingSphere = StaticRef<bool>(0x8D60BC); // NOTSA name

//! Transforms the center of the atomic's bounding sphere to world space (or clump space if not using the LTM)
static RwV3d AtomicGetBoundingSphereCenterTransformed(RpAtomic* atomic) {
    const auto frame = RpClumpGetFrame(atomic->clump);
    RwV3d      center;
    if (atomic->interpolator.flags & rpINTERPOLATORDIRTYSPHERE) {
        _rpAtomicResyncInterpolatedSphere(atomic);
    }
    RwV3dTransformPoints(&center, &atomic->boundingSphere.center, 1, s_bUseLTMForClumpBoundingSphere ? RwFrameGetLTM(frame) : RwFrameGetMatrix(frame));
    return center;
}

// NOTSA name
// 0x734970
static RpAtomic* ClumpBoundingSphereSumCentersCB(RpAtomic* atomic, void* data) {
    const auto center = AtomicGetBoundingSphereCenterTransformed(atomic);
    const auto sum    = (RwV3d*)data;
    sum->x = (float)((double)center.x + (double)sum->x);
    sum->y = (float)((double)center.y + (double)sum->y);
    sum->z = (float)((double)center.z + (double)sum->z);
    return atomic;
}

// NOTSA name
// 0x734ED0
static RpAtomic* ClumpBoundingSphereCalcRadiusCB(RpAtomic* atomic, void* data) {
    const auto sphere = (RwSphere*)data;

    if (atomic->interpolator.flags & rpINTERPOLATORDIRTYSPHERE) { // The original does this check up front, too
        _rpAtomicResyncInterpolatedSphere(atomic);
    }
    const auto center = AtomicGetBoundingSphereCenterTransformed(atomic);

    const double dx = (double)center.x - (double)sphere->center.x;
    const double dy = (double)center.y - (double)sphere->center.y;
    const double dz = (double)center.z - (double)sphere->center.z;
    const double radius = std::sqrt(dz * dz + dy * dy + dx * dx) + (double)atomic->boundingSphere.radius;
    if (radius > (double)sphere->radius) {
        sphere->radius = (float)radius;
    }
    return atomic;
}

// 0x734FC0
RpClump* RpClumpGetBoundingSphere(RpClump* clump, RwSphere* sphere, bool bUseLTM) {
    s_bUseLTMForClumpBoundingSphere = bUseLTM;

    if (!clump || !sphere) {
        return nullptr;
    }

    sphere->center = {};
    sphere->radius = 0.f;

    const float numAtomics = (float)RpClumpGetNumAtomics(clump);
    if (numAtomics < 1.f) {
        return nullptr;
    }

    // Center of the sphere = average of the (transformed) centers of the atomics' spheres
    RwV3d center{};
    RpClumpForAllAtomics(clump, ClumpBoundingSphereSumCentersCB, &center);

    const double recipNumAtomics = 1.0 / (double)numAtomics; // Kept in an x87 register in the original
    center.x = (float)((double)center.x * recipNumAtomics);
    center.y = (float)((double)center.y * recipNumAtomics);
    center.z = (float)((double)center.z * recipNumAtomics);

    // Radius = max distance to any atomic's sphere center + its radius
    RwSphere worldSphere{ center, 0.f };
    RpClumpForAllAtomics(clump, ClumpBoundingSphereCalcRadiusCB, &worldSphere);

    // Transform to clump local space
    RwMatrix invMat;
    if (s_bUseLTMForClumpBoundingSphere) {
        RwMatrixInvert(&invMat, RwFrameGetLTM(RpClumpGetFrame(clump)));
    } else {
        RwMatrixInvert(&invMat, RwFrameGetMatrix(RpClumpGetFrame(clump)));
    }
    RwV3dTransformPoints(&worldSphere.center, &worldSphere.center, 1, &invMat);

    sphere->radius = worldSphere.radius;
    sphere->center = worldSphere.center;

    return clump;
}

//! Bone positions (relative to their parent bone) of the last skinned clump `SkinGetBonePositions` has been called with
struct tSkinBonePos {
    int32 parent; //!< Index of the parent bone
    RwV3d pos;
};
VALIDATE_SIZE(tSkinBonePos, 0x10);
static auto& s_SkinBonePositions            = StaticRef<std::array<tSkinBonePos, 64>>(0xC88258); // NOTSA name
static auto& s_bSkinBonePositionsInitialized = StaticRef<bool>(0xC88658);                          // NOTSA name

// 0x735140
void SkinGetBonePositions(RpClump* clump) {
    if (s_bSkinBonePositionsInitialized) {
        return;
    }
    s_bSkinBonePositionsInitialized = true;

    RpAtomic* atomic{};
    RpClumpForAllAtomics(clump, GetFirstAtomicCallback, &atomic);
    const auto skin = RpSkinGeometryGetSkin(RpAtomicGetGeometry(atomic));

    RpHAnimHierarchy* hier{};
    RpClumpForAllAtomics(clump, SkinAtomicGetHAnimHierarchCB, &hier);

    s_SkinBonePositions[0] = { -1, {} }; // Root bone

    const auto nBones = RpSkinGetNumBones(skin);
    if (nBones <= 1) {
        return;
    }

    int32  nodeStk[64]{};
    int32* nodeStkPtr = nodeStk;
    int32  currNodeIdx{};
    for (int32 i = 1; i < nBones; i++) {
        RwMatrix invBoneMat;
        RwMatrixInvert(&invBoneMat, &RpSkinGetSkinToBoneMatrices(skin)[i]);
        RwV3dTransformPoints(&s_SkinBonePositions[i].pos, RwMatrixGetPos(&invBoneMat), 1, &RpSkinGetSkinToBoneMatrices(skin)[currNodeIdx]);
        s_SkinBonePositions[i].parent = currNodeIdx;

        const auto nodeFlags = hier->pNodeInfo[i].flags;
        if (nodeFlags & rpHANIMPUSHPARENTMATRIX) {
            *++nodeStkPtr = currNodeIdx;
        }
        currNodeIdx = nodeFlags & rpHANIMPOPPARENTMATRIX
            ? *nodeStkPtr--
            : i;
    }
}

// 0x7352D0
void SkinSetBonePositions(RpClump* clump) {
    RpAtomic* atomic{};
    RpClumpForAllAtomics(clump, GetFirstAtomicCallback, &atomic);
    const auto skin = RpSkinGeometryGetSkin(RpAtomicGetGeometry(atomic));

    RpHAnimHierarchy* hier{};
    RpClumpForAllAtomics(clump, SkinAtomicGetHAnimHierarchCB, &hier);
    const auto mats = RpHAnimHierarchyGetMatrixArray(hier);

    const auto nBones = RpSkinGetNumBones(skin);
    for (int32 i = 1; i < nBones; i++) {
        RwV3dTransformPoints(RwMatrixGetPos(&mats[i]), &s_SkinBonePositions[i].pos, 1, &mats[s_SkinBonePositions[i].parent]);
    }
}

// 0x735360
void SkinGetBonePositionsToTable(RpClump* clump, RwV3d* table) {
    if (!table) {
        return;
    }

    RpAtomic* atomic{};
    RpClumpForAllAtomics(clump, GetFirstAtomicCallback, &atomic);
    const auto skin = RpSkinGeometryGetSkin(RpAtomicGetGeometry(atomic));

    RpHAnimHierarchy* hier{};
    RpClumpForAllAtomics(clump, SkinAtomicGetHAnimHierarchCB, &hier);

    table[0] = {}; // Root bone

    const auto nBones = RpSkinGetNumBones(skin);
    int32  nodeStk[64]{};
    int32* nodeStkPtr = nodeStk;
    int32  currNodeIdx{};
    for (int32 i = 1; i < nBones; i++) {
        RwMatrix invBoneMat;
        RwMatrixInvert(&invBoneMat, &RpSkinGetSkinToBoneMatrices(skin)[i]);
        RwV3dTransformPoints(&table[i], RwMatrixGetPos(&invBoneMat), 1, &RpSkinGetSkinToBoneMatrices(skin)[currNodeIdx]);

        const auto nodeFlags = hier->pNodeInfo[i].flags;
        if (nodeFlags & rpHANIMPUSHPARENTMATRIX) {
            *++nodeStkPtr = currNodeIdx;
        }
        currNodeIdx = nodeFlags & rpHANIMPOPPARENTMATRIX
            ? *nodeStkPtr--
            : i;
    }
}

// 0x7226D0
RpAtomic* RemoveRefsCB(RpAtomic* atomic, void* data) {
    UNUSED(data);
    auto* modelInfo = CVisibilityPlugins::GetModelInfo(atomic);
    modelInfo->RemoveRef();
    return atomic;
}

// 0x7226F0
void RemoveRefsForAtomic(RpClump* clump) {
    RpClumpForAllAtomics(clump, RemoveRefsCB, nullptr);
}
