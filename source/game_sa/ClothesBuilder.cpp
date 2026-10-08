#include "StdInc.h"

#include <extensions/ci_string.hpp>

#include "ClothesBuilder.h"
#include "PedClothesDesc.h"

auto& playerImg = StaticRef<CDirectory>(0xBC12C0);
auto& playerImgEntries = StaticRef<CDirectory::DirectoryInfo>(0xBBCDC8);

auto& gBoneIndices = StaticRef<notsa::mdarray<int16, 10, 64>>(0xBBC8C8);

auto& ms_ratiosHaveChanged  = StaticRef<bool>(0x8D0AA4);
auto& ms_geometryHasChanged = StaticRef<bool>(0x8D0AA5);
auto& ms_textureHasChanged  = StaticRef<bool>(0x8D0AA6);

void CClothesBuilder::InjectHooks() {
    RH_ScopedClass(CClothesBuilder);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(LoadCdDirectory, 0x5A4190);
    RH_ScopedInstall(RequestGeometry, 0x5A41C0);
    RH_ScopedInstall(RequestTexture, 0x5A4220);
    //RH_ScopedInstall(nullptr, 0x5A42B0, { .Reversed = false }); 
    //RH_ScopedInstall(nullptr, 0x5A4380, { .Reversed = false }); AtomicInstanceCB
    //RH_ScopedInstall(nullptr, 0x5A43A0, { .Reversed = false });
    //RH_ScopedInstall(nullptr, 0x5A44A0, { .Reversed = false }); DestroyTextureCB
    RH_ScopedInstall(PreprocessClothesDesc, 0x5A44C0);
    RH_ScopedInstall(ReleaseGeometry, 0x5A47B0);
    RH_ScopedGlobalInstall(GetAtomicWithName, 0x5A4810);
    RH_ScopedInstall(AddWeightToBoneVertex, 0x5A4840);
    RH_ScopedInstall(StoreBoneArray, 0x5A48B0);
    RH_ScopedOverloadedInstall(BlendGeometry, "3", 0x5A4940, RpGeometry * (*)(RpClump*, const char*, const char*, const char*, float, float, float));
    RH_ScopedOverloadedInstall(BlendGeometry, "2", 0x5A4F10, RpGeometry* (*)(RpClump*, const char*, const char*, float, float));
    RH_ScopedInstall(CopyGeometry, 0x5A5340);
    RH_ScopedInstall(ConstructGeometryArray, 0x5A55A0);
    RH_ScopedInstall(DestroySkinArrays, 0x5A56C0);
    RH_ScopedInstall(BuildBoneIndexConversionTable, 0x5A56E0);
    RH_ScopedInstall(CopyTexture, 0x5A5730);
    RH_ScopedInstall(PlaceTextureOnTopOfTexture, 0x5A57B0);
    RH_ScopedOverloadedInstall(BlendTextures, "Dst-Src", 0x5A5820, void (*)(RwTexture*, RwTexture*, float, float, int32));
    RH_ScopedOverloadedInstall(BlendTextures, "Dst-Src1-Src2", 0x5A59C0, void (*)(RwTexture*, RwTexture*, RwTexture*, float, float, float, int32));
    RH_ScopedOverloadedInstall(BlendTextures, "Dst-Src1-Src2-Tat", 0x5A5BC0, void (*)(RwTexture*, RwTexture*, RwTexture*, float, float, float, int32, RwTexture*));
    RH_ScopedGlobalInstall(GetTextureFromTxdAndLoadNextTxd, 0x5A5F70);
    RH_ScopedInstall(ConstructTextures, 0x5A6040);
    RH_ScopedInstall(ConstructGeometryAndSkinArrays, 0x5A6530);
    RH_ScopedInstall(CreateSkinnedClump, 0x5A69D0);
}

// inlined
// 0x5A4190
void CClothesBuilder::LoadCdDirectory() {
    playerImg.Init(550, &playerImgEntries);
    playerImg.ReadDirFile("MODELS\\PLAYER.IMG");
}

// 0x5A41C0
void CClothesBuilder::RequestGeometry(int32 modelId, uint32 modelNameKey) {
    CModelInfo::GetModelInfo(modelId)->bHasComplexHierarchy = true; // TODO/NOTE: Not sure

    uint32 size;
    CdStreamPos pos;
    VERIFY(playerImg.FindItem(CKeyGen::AppendStringToKey(modelNameKey, ".DFF"), pos, size));
    CStreaming::RequestFile(modelId, pos, size, CClothes::ms_clothesImageId, STREAMING_PRIORITY_REQUEST | STREAMING_GAME_REQUIRED);
}

// 0x5A4220
int32 CClothesBuilder::RequestTexture(uint32 txdNameKey) {
    if (txdNameKey == 0) {
        return -1;
    }

    auto& defaultTxdIdx = StaticRef<uint32>(0xBC12D0);
    const auto defaultTxd = CTxdStore::defaultTxds[defaultTxdIdx];
    defaultTxdIdx = (defaultTxdIdx + 1) % 4;

    uint32 size;
    CdStreamPos pos;
    VERIFY(playerImg.FindItem(CKeyGen::AppendStringToKey(txdNameKey, ".TXD"), pos, size));
    CStreaming::RequestFile(TXDToModelId(defaultTxd), pos, size, CClothes::ms_clothesImageId, STREAMING_PRIORITY_REQUEST | STREAMING_GAME_REQUIRED);

    return defaultTxd;
}

// 0x5A44C0
void CClothesBuilder::PreprocessClothesDesc(CPedClothesDesc& desc, bool a2) {
    // The rules are stored as a stream of `[opcode, args...]` in `CClothes::ms_clothesRules`
    enum eRuleOp : uint32 {
        RULE_SET_MODEL_IF_MODEL    = 0, // args: [modelKey, newModelKey]
        RULE_SET_TEX_IF_MODEL_PART = 1, // args: [modelKey, modelPart, newModelKey, newTexKey]
        RULE_SET_TEX_IF_MODEL      = 2, // args: [modelKey, newTexKey]
        RULE_CLEAR_MODEL_IF_MODEL  = 3, // args: [modelKey, modelPart]
        RULE_COND_REMOVE_MUST_HAVE = 4, // args: [modelKey]
        RULE_COND_ADD_MUST_HAVE    = 5, // args: [modelKey]
        RULE_COND_REMOVE_MUST_NOT  = 6, // args: [modelKey]
        RULE_COND_ADD_MUST_NOT     = 7, // args: [modelKey]
    };

    // Active conditions (a rule is skipped if any of these is fulfilled, see below)
    int32 condKeys[8];
    uint8 condMustMatch[8]{}; // NOTSA: OG didn't init this (but it's only read for used slots)
    rng::fill(condKeys, -1);

    auto&      rules   = CClothes::ms_clothesRules;
    const auto GetRule = [&](uint32 i) { return (int32)rules[i]; };

    for (uint32 p = 0; p < CClothes::ms_numRuleTags;) {
        const auto op = rules[p++];

        int32 key{}, part{-1}, modelKey{}, texKey{};
        switch (op) {
        case RULE_SET_MODEL_IF_MODEL:
            key      = GetRule(p);
            modelKey = GetRule(p + 1);
            p += 2;
            break;
        case RULE_SET_TEX_IF_MODEL_PART:
            key      = GetRule(p);
            part     = GetRule(p + 1);
            modelKey = GetRule(p + 2);
            texKey   = GetRule(p + 3);
            p += 4;
            break;
        case RULE_SET_TEX_IF_MODEL:
            key    = GetRule(p);
            texKey = GetRule(p + 1);
            p += 2;
            break;
        case RULE_CLEAR_MODEL_IF_MODEL:
            key  = GetRule(p);
            part = GetRule(p + 1);
            p += 2;
            break;
        case RULE_COND_REMOVE_MUST_HAVE:
        case RULE_COND_REMOVE_MUST_NOT:
            key = GetRule(p++);
            for (auto i = 0; i < 8; i++) {
                if (condKeys[i] == key) {
                    condKeys[i]      = -1;
                    condMustMatch[i] = op == RULE_COND_REMOVE_MUST_HAVE;
                    break;
                }
            }
            break;
        case RULE_COND_ADD_MUST_HAVE:
        case RULE_COND_ADD_MUST_NOT:
            for (auto i = 0; i < 8; i++) {
                if (condKeys[i] == -1) {
                    condKeys[i]      = GetRule(p);
                    condMustMatch[i] = op == RULE_COND_ADD_MUST_HAVE;
                    p++; // BUG: OG only skips the argument if a free slot was found
                    break;
                }
            }
            break;
        }

        for (auto partIdx = 0; partIdx < 10; partIdx++) {
            // Skip if any active condition is fulfilled
            const auto condModelIdx = part != -1 ? part : partIdx;
            auto       c            = 0;
            for (; c < 8; c++) {
                if (condKeys[c] != -1 && ((int32)desc.m_anModelKeys[condModelIdx] == condKeys[c]) == (bool)condMustMatch[c]) {
                    break;
                }
            }
            if (c != 8) {
                continue;
            }

            switch (op) {
            case RULE_SET_MODEL_IF_MODEL:
                if (a2 && (int32)desc.m_anModelKeys[partIdx] == key) {
                    desc.SetModel((uint32)modelKey, (eClothesModelPart)partIdx);
                }
                break;
            case RULE_SET_TEX_IF_MODEL_PART:
                if ((int32)desc.m_anModelKeys[partIdx] == key) {
                    if (part == CLOTHES_MODEL_HANDS) {
                        desc.SetModel((uint32)modelKey, CLOTHES_MODEL_HANDS);
                        break;
                    }
                    const auto texPart = CClothes::GetDependentTexture((eClothesModelPart)part);
                    if (texKey == 0) {
                        texKey = (int32)desc.m_anTextureKeys[texPart];
                    }
                    if (modelKey == 0) {
                        modelKey = (int32)desc.m_anModelKeys[part];
                    }
                    desc.SetTextureAndModel((uint32)texKey, (uint32)modelKey, texPart);
                }
                break;
            case RULE_SET_TEX_IF_MODEL:
                if ((int32)desc.m_anModelKeys[partIdx] == key) {
                    desc.SetTextureAndModel((uint32)texKey, (uint32)key, CClothes::GetDependentTexture((eClothesModelPart)partIdx));
                }
                break;
            case RULE_CLEAR_MODEL_IF_MODEL:
                if ((int32)desc.m_anModelKeys[partIdx] == key) {
                    desc.SetModel(0u, (eClothesModelPart)part);
                }
                break;
            }
        }
    }
}

// unused
// 0x5A47B0
void CClothesBuilder::ReleaseGeometry(int32 numToRelease) {
    for (auto i = numToRelease; i; CStreaming::SetModelIsDeletable(MODEL_CLOTHES01_ID384 + i))
        --i;
}

// 0x5A4810
RpAtomic* GetAtomicWithName(RpClump* clump, const char* name) {
    struct Context {
        notsa::ci_string_view name{};
        RpAtomic*             atomic{};
    } c{name};
    RpClumpForAllAtomics(clump, [](RpAtomic* a, void* data) { // 0x5A47E0
        auto& ctx = *static_cast<Context*>(data);
        if (ctx.name == GetFrameNodeName(RpAtomicGetFrame(a))) {
            ctx.atomic = a;
        }
        return a;
    }, &c);
    return c.atomic;
}

// 0x5A4840
void CClothesBuilder::AddWeightToBoneVertex(float (&weights)[8], uint8(&boneVertexIdxs)[8], float weightToAdd, RwUInt32 targetVertexIdx) { // Unknown OG name
    if (weightToAdd == 0.f) {
        return;
    }
    for (auto i = 0; i < 8; i++) {
        if (weights[i] == 0.f) { // Weight not yet used?
            boneVertexIdxs[i] = targetVertexIdx; // Add to list
            weights[i]        = weightToAdd;
            weights[i + 1]    = 0.f;             // Mark next as unused [Though this step in our case is not necessary as the whole weights array is already zero-inited]
            return;
        }
        if (boneVertexIdxs[i] == targetVertexIdx) { // Already in the list, use that
            weights[i] += weightToAdd;
            return;
        }
    }
    NOTSA_UNREACHABLE(); // OG code had UB in this case
}

// 0x5A48B0
void CClothesBuilder::StoreBoneArray(RpClump* clump, int32 idx) {
    const auto a = GetAtomicWithName(clump, "normal");
    assert(a);

    const auto h = RpSkinAtomicGetHAnimHierarchy(a);
    assert(h);
    
    rng::fill(gBoneIndices[idx], -1);
    for (auto i = h->numNodes; i-- > 0;) {
        gBoneIndices[idx][i] = static_cast<int16>(h->pNodeInfo[i].nodeID);
    }
}

/*
* @notsa
*
* Based on 0x5A4940 and 0x5A4F10
* Blend 2 or 3 geometries together. The result is stored in the 0th frame's geometry.
*
* @arg clump  The clump to which the frames belong to
* @arg names  Names of the frames [0th is the destination]
* @arg ratios Blend ratios of the corresponding frames
*/
template<size_t N>
RpGeometry* BlendGeometryImpl(RpClump* clump, const char* const (&names)[N], const float (&r)[N]) {
    static_assert(N == 2 || N == 3);

    struct GeoBlendData {
        RpGeometry*      g;
        uint8*           boneIdxs;     // 4 per vertex
        RwMatrixWeights* boneWeights;  // 1 per vertex
        RwTexCoords*     uvs;
        RwV3d*           verts;
        RwV3d*           nrmls;
    } d[N];
    for (size_t k = 0; k < N; k++) {
        const auto g    = RpAtomicGetGeometry(GetAtomicWithName(clump, names[k]));
        const auto skin = RpSkinGeometryGetSkin(g);
        const auto mt   = RpGeometryGetMorphTarget(g, 0);
        d[k] = {
            g,
            (uint8*)RpSkinGetVertexBoneIndices(skin),
            RpSkinGetVertexBoneWeights(skin),
            RpGeometryGetVertexTexCoords(g, 1),
            RpMorphTargetGetVertices(mt),
            RpMorphTargetGetVertexNormals(mt)
        };
    }
    const auto out = d[0].g;

    RpGeometryLock(out, rpGEOMETRYLOCKALL);

    for (auto i = 0; i < RpGeometryGetNumVertices(out); i++) {
        // NOTE: The order of operations is the same as in the original code
        {
            auto& v0 = d[0].verts[i];
            auto& n0 = d[0].nrmls[i];
            auto& t0 = d[0].uvs[i];
            const auto& v1 = d[1].verts[i];
            const auto& n1 = d[1].nrmls[i];
            const auto& t1 = d[1].uvs[i];
            if constexpr (N == 3) {
                const auto& v2 = d[2].verts[i];
                const auto& n2 = d[2].nrmls[i];
                const auto& t2 = d[2].uvs[i];

                v0.x = r[1] * v1.x + r[0] * v0.x + r[2] * v2.x;
                v0.y = r[1] * v1.y + r[2] * v2.y + r[0] * v0.y;
                v0.z = r[1] * v1.z + r[2] * v2.z + r[0] * v0.z;

                n0.x = r[2] * n2.x + r[1] * n1.x + r[0] * n0.x;
                n0.y = r[1] * n1.y + r[2] * n2.y + r[0] * n0.y;
                n0.z = r[1] * n1.z + r[2] * n2.z + r[0] * n0.z;
                RwV3dNormalize(&n0, &n0);

                t0.u = r[2] * t2.u + r[1] * t1.u + r[0] * t0.u;
                t0.v = r[1] * t1.v + r[2] * t2.v + r[0] * t0.v;
            } else {
                v0.x = r[1] * v1.x + r[0] * v0.x;
                v0.y = r[1] * v1.y + r[0] * v0.y;
                v0.z = r[1] * v1.z + r[0] * v0.z;

                n0.x = r[1] * n1.x + r[0] * n0.x;
                n0.y = r[1] * n1.y + r[0] * n0.y;
                n0.z = r[1] * n1.z + r[0] * n0.z;
                RwV3dNormalize(&n0, &n0);

                t0.u = r[1] * t1.u + r[0] * t0.u;
                t0.v = r[1] * t1.v + r[0] * t0.v;
            }
        }

        // Calculate bone weights
        float weights[8]{};
        uint8 boneVertexIdxs[8]{};
        for (size_t k = 0; k < N; k++) {
            const float* const w = &d[k].boneWeights[i].w0; // NOTE: w0, w1, w2, w3 are laid out sequentially
            for (auto wi = 0; wi < 4; wi++) {
                CClothesBuilder::AddWeightToBoneVertex(
                    weights,
                    boneVertexIdxs,
                    r[k] * w[wi],
                    d[k].boneIdxs[4 * i + wi]
                );
            }
        }
        for (auto b = 0; b < 4; b++) {
            d[0].boneIdxs[4 * i + b] = boneVertexIdxs[b];
        }
        float* const outW = &d[0].boneWeights[i].w0;
        for (auto wi = 0; wi < 4; wi++) {
            outW[wi] = weights[wi];
        }
        if (weights[4] != 0.f) { // More than 4 bones were used => re-normalize
            const auto t = 1.f / (weights[3] + weights[2] + weights[1] + weights[0]);
            for (auto wi = 0; wi < 4; wi++) {
                outW[wi] = t * outW[wi];
            }
        }
    }

    RpGeometryUnlock(out);
    out->refCount++; // TODO: RpGeometryAddRef [0x74CCB0] missing

    return out;
}

// 0x5A4940
RpGeometry* CClothesBuilder::BlendGeometry(RpClump* clump, const char* frameName0, const char* frameName1, const char* frameName2, float r0, float r1, float r2) {
    return BlendGeometryImpl<3>(clump, { frameName0, frameName1, frameName2 }, { r0, r1, r2 });
}

// 0x5A4F10
RpGeometry* CClothesBuilder::BlendGeometry(RpClump* clump, const char* frameName0, const char* frameName1, float r0, float r1) {
    return BlendGeometryImpl<2>(clump, { frameName0, frameName1 }, { r0, r1 });
}

// 0x5A5340
RpGeometry* CClothesBuilder::CopyGeometry(RpClump* clump, const char* dstFrameName, const char* srcFrameName) {
    const char* const names[]{ dstFrameName, srcFrameName };

    struct GeoCopyData {
        RpGeometry*      g;
        uint8*           boneIdxs;
        RwMatrixWeights* boneWeights;
        RwTexCoords*     uvs;
        RwV3d*           verts;
        RwV3d*           nrmls;
    } d[2];
    for (auto k = 0; k < 2; k++) {
        const auto g    = RpAtomicGetGeometry(GetAtomicWithName(clump, names[k]));
        const auto skin = RpSkinGeometryGetSkin(g);
        const auto mt   = RpGeometryGetMorphTarget(g, 0);
        d[k] = {
            g,
            (uint8*)RpSkinGetVertexBoneIndices(skin),
            RpSkinGetVertexBoneWeights(skin),
            RpGeometryGetVertexTexCoords(g, 1),
            RpMorphTargetGetVertices(mt),
            RpMorphTargetGetVertexNormals(mt)
        };
    }

    RpGeometryLock(d[0].g, rpGEOMETRYLOCKALL);

    for (auto i = 0; i < RpGeometryGetNumVertices(d[0].g); i++) {
        d[0].verts[i] = d[1].verts[i];
        d[0].nrmls[i] = d[1].nrmls[i];
        d[0].uvs[i]   = d[1].uvs[i];
        for (auto b = 0; b < 4; b++) {
            d[0].boneIdxs[4 * i + b] = d[1].boneIdxs[4 * i + b];
        }
        d[0].boneWeights[i] = d[1].boneWeights[i];
    }

    RpGeometryUnlock(d[0].g);

    // NOTE: OG then re-fetched all the pointers above, and ran a loop that did nothing [no side effects]

    d[0].g->refCount++; // TODO: RpGeometryAddRef [0x74CCB0] missing

    return d[0].g;
}

// 0x5A55A0
void CClothesBuilder::ConstructGeometryArray(RpGeometry** out, uint32* modelNameKeys, float normal, float fatness, float strength) {
    for (auto i = 0; i < 10; i++, out++) {
        if (modelNameKeys[i] == 0) {
            *out = nullptr;
            continue;
        }
        const auto modelIdx = (eModelID)((int)MODEL_CLOTHES01_ID384 + i);
        const auto mi       = CModelInfo::GetModelInfo(modelIdx);

        CModelInfo::GetModelInfo(modelIdx)->bHasComplexHierarchy = true;
        RequestGeometry(modelIdx, modelNameKeys[i]);
        CStreaming::LoadAllRequestedModels(true);

        if (i + 1 < 10 && modelNameKeys[i + 1]) { // Request next model to be loaded in advance
            RequestGeometry((eModelID)((int)MODEL_CLOTHES01_ID384 + i + 1), modelNameKeys[i + 1]);
            CStreaming::LoadRequestedModels();
        }

        *out = BlendGeometry(mi->GetRpClump(), "normal", "fat", "ripped", normal, fatness, strength);
        StoreBoneArray(mi->GetRpClump(), i);
        CStreaming::RemoveModel(modelIdx);
    }
}

// inlined, see 0x5A6CE1
// 0x5A56C0
void CClothesBuilder::DestroySkinArrays(RwMatrixWeights* weights, RwUInt32* bones) {
    // TODO: Should this be `delete[]` or `delete`?
    delete weights;
    delete bones;
}

// 0x5A56E0
void CClothesBuilder::BuildBoneIndexConversionTable(uint8* pTable, RpHAnimHierarchy* hier, int32 index) {
    for (const auto [tableIdx, boneId] : rngv::enumerate(gBoneIndices[index])) {
        if (boneId == -1) {
            break;
        }
        const auto idx = RpHAnimIDGetIndex(hier, boneId);
        pTable[tableIdx] = idx == 0xFF ? 0 : idx;
    }
}

void AssertTextureLayouts(std::initializer_list<RwTexture*> textures) {
    assert(textures.size() >= 2);
    for (auto i = 0u; i < textures.size() - 1; i++) {
        const auto r1 = RwTextureGetRaster(textures.begin()[i]), r2 = RwTextureGetRaster(textures.begin()[i + 1]);

        assert(RwRasterGetWidth(r1) == RwRasterGetWidth(r2));
        assert(RwRasterGetHeight(r1) == RwRasterGetHeight(r2));
        assert(RwRasterGetDepth(r1) == RwRasterGetDepth(r2));
        assert(RwRasterGetDepth(r1) == 32);
    }
}

// 0x5A5730
RwTexture* CClothesBuilder::CopyTexture(RwTexture* srcTex) {
    const auto srcRaster = RwTextureGetRaster(srcTex);
    
    // Create a new raster to which we're going to copy to
    const auto dstRaster = RwRasterCreate(
        RwRasterGetWidth(srcRaster),
        RwRasterGetHeight(srcRaster),
        RwRasterGetDepth(srcRaster),
        (RwRasterGetFormat(srcRaster) & rwRASTERFORMATPIXELFORMATMASK) | 4 // TODO
    );
    
    // Copy data from the src raster to this one
    const auto srcLck = RwRasterLock(srcRaster, 0, rwRASTERLOCKREAD);
    memcpy(
        RwRasterLock(dstRaster, 0, rwRASTERLOCKWRITE),
        srcLck,
        RwRasterGetHeight(srcRaster) * RwRasterGetStride(srcRaster)
    );
    RwRasterUnlock(srcRaster);
    RwRasterUnlock(dstRaster);

    // Create a texture from the copied raster
    const auto dstTex = RwTextureCreate(dstRaster);
    RwTextureSetFilterMode(dstTex, rwFILTERLINEAR);

    AssertTextureLayouts({ dstTex, srcTex });

    return dstTex;
}

// 0x5A57B0
void CClothesBuilder::PlaceTextureOnTopOfTexture(RwTexture* dstTex, RwTexture* srcTex) {
    ZoneScoped;

    AssertTextureLayouts({ dstTex, srcTex });

    const auto dstRaster = RwTextureGetRaster(dstTex);
    const auto srcRaster = RwTextureGetRaster(srcTex);

    auto dstIt = (RwUInt32*)RwRasterLock(dstRaster, 0, rwRASTERLOCKREADWRITE);
    auto srcIt = (RwUInt32*)RwRasterLock(srcRaster, 0, rwRASTERLOCKREADWRITE);

    // NOTE: They don't skip the stride, but it's fine [This way vectorization should be easier for the compiler]
    for (auto i = RwRasterGetHeight(dstRaster) * RwRasterGetWidth(dstRaster); i-- > 0; dstIt++, srcIt++) {
        if (*srcIt & 0xFF000000) { // Check alpha != 0
            *dstIt = *srcIt;
        }
    }

    RwRasterUnlock(dstRaster);
    RwRasterUnlock(srcRaster);
}

// 0x5A5820
void CClothesBuilder::BlendTextures(RwTexture* dst, RwTexture* src, float r1, float r2, int32 numColors) {
    ZoneScoped;

    AssertTextureLayouts({ dst, src });

    const auto dstRaster = RwTextureGetRaster(dst);
    const auto srcRaster = RwTextureGetRaster(src);

    CTimer::Suspend();

    auto srcIt = RwRasterLock(srcRaster, 0, rwRASTERLOCKREAD);
    auto dstIt = RwRasterLock(dstRaster, 0, rwRASTERLOCKREADWRITE);

    for (auto i = RwRasterGetHeight(dstRaster) * RwRasterGetWidth(dstRaster); i-- > 0; dstIt++, srcIt++) {
        for (auto c = 3; i-- > 0; dstIt++, srcIt++) { // Copy RGB, alpha stays the same
            *dstIt = multiply_weighted<RwUInt8>({ { *dstIt, r1 }, { *srcIt, r2 } });
        }
    }

    RwRasterUnlock(dstRaster);
    RwRasterUnlock(srcRaster);

    CTimer::Resume();
}

// 0x5A59C0
void CClothesBuilder::BlendTextures(RwTexture* dst, RwTexture* src1, RwTexture* src2, float r1, float r2, float r3, int32) {
    ZoneScoped;

    AssertTextureLayouts({ dst, src1, src2 });

    const auto dstRaster  = RwTextureGetRaster(dst);
    const auto src1Raster = RwTextureGetRaster(src1);
    const auto src2Raster = RwTextureGetRaster(src2);

    CTimer::Suspend();

    auto src1It = RwRasterLock(src1Raster, 0, rwRASTERLOCKREAD);
    auto src2It = RwRasterLock(src2Raster, 0, rwRASTERLOCKREAD);
    auto dstIt  = RwRasterLock(dstRaster, 0, rwRASTERLOCKREADWRITE);

    for (auto i = RwRasterGetHeight(dstRaster) * RwRasterGetWidth(dstRaster); i-- > 0; dstIt++, src1It++, src2It++) {
        for (auto c = 3; i-- > 0; dstIt++, src1It++, src2It++) { // Copy RGB, alpha doesn't change
            *dstIt = multiply_weighted<RwUInt8>({ { *dstIt, r1 }, { *src1It, r2 }, { *src2It, r3 } });
        }
    }

    RwRasterUnlock(dstRaster);
    RwRasterUnlock(src1Raster);
    RwRasterUnlock(src2Raster);

    CTimer::Resume();
}

// 0x5A5BC0
void CClothesBuilder::BlendTextures(RwTexture* dst, RwTexture* src1, RwTexture* src2, float r1, float r2, float r3, int32 numColors, RwTexture* tattoos) {
    ZoneScoped;

    AssertTextureLayouts({ dst, src1, src2, tattoos });

    const auto dstRaster  = RwTextureGetRaster(dst);
    const auto src1Raster = RwTextureGetRaster(src1);
    const auto src2Raster = RwTextureGetRaster(src2);
    const auto tatRaster  = RwTextureGetRaster(tattoos);

    CTimer::Suspend();

    auto src1It = RwRasterLock(src1Raster, 0, rwRASTERLOCKREAD);
    auto src2It = RwRasterLock(src2Raster, 0, rwRASTERLOCKREAD);
    auto tatIt  = RwRasterLock(tatRaster, 0, rwRASTERLOCKREAD);
    auto dstIt  = RwRasterLock(dstRaster, 0, rwRASTERLOCKREADWRITE);

    for (auto i = RwRasterGetHeight(dstRaster) * RwRasterGetWidth(dstRaster); i-- > 0; dstIt++, src1It++, src2It++, tatIt++) {
        const auto tatAlphaT = (float)tatIt[3] / 255.f;
        for (auto c = 3; i-- > 0; dstIt++, src1It++, src2It++, tatIt++) { // Copy RGB, alpha doesn't change
            *dstIt = (RwUInt8)lerp(multiply_weighted<RwUInt8>({ { *dstIt, r1 }, { *src1It, r2 }, { *src2It, r3 } }), *tatIt, tatAlphaT);
        }
    }

    RwRasterUnlock(dstRaster);
    RwRasterUnlock(src1Raster);
    RwRasterUnlock(src2Raster);
    RwRasterUnlock(tatRaster);

    CTimer::Resume();
}

// 0x5A5F70
RwTexture* GetTextureFromTxdAndLoadNextTxd(RwTexture* dstTex, int32 txdId_withTexture, int32 CRC_nextTxd, int32* nextTxdId) {
    if (txdId_withTexture == -1) {
        if (CRC_nextTxd) {
            *nextTxdId = CClothesBuilder::RequestTexture(CRC_nextTxd);
            CStreaming::LoadRequestedModels();
        } else {
            *nextTxdId = -1;
        }
        return dstTex;
    }

    CStreaming::LoadAllRequestedModels(true);
    if (CRC_nextTxd) {
        *nextTxdId = CClothesBuilder::RequestTexture(CRC_nextTxd);
        CStreaming::LoadRequestedModels();
    } else {
        *nextTxdId = -1;
    }
    const auto tex = GetFirstTexture(CTxdStore::GetTxd(txdId_withTexture));
    const auto res = dstTex
        ? CClothesBuilder::PlaceTextureOnTopOfTexture(dstTex, tex), dstTex
        : CClothesBuilder::CopyTexture(tex);
    CStreaming::RemoveModel(TXDToModelId(txdId_withTexture));
    return res;
}

// 0x5A6040
void CClothesBuilder::ConstructTextures(RwTexDictionary* dict, uint32* hashes, float factorA, float factorB, float factorC) {
    // Body tattoos (hashes[4] to hashes[12]): Each texture is placed on top of the previous one
    int32      txdId = RequestTexture(hashes[4]);
    int32      nextTxdIdA{}, torsoTxdId{};
    RwTexture* tattoos{};
    CStreaming::LoadRequestedModels();
    for (auto i = 4; i <= 12; i++) {
        uint32  nextKey;
        int32*  nextTxdIdOut;
        if (i < 12) {
            nextKey      = hashes[i + 1];
            nextTxdIdOut = &nextTxdIdA;
        } else {
            nextKey      = CKeyGen::GetUppercaseKey("player_torso");
            nextTxdIdOut = &torsoTxdId;
        }
        tattoos = GetTextureFromTxdAndLoadNextTxd(tattoos, txdId, nextKey, nextTxdIdOut);
        txdId   = nextTxdIdA;
    }

    if (factorB < 0.f) {
        factorA += factorB;
        factorB  = 0.f;
    }

    //> Torso
    CStreaming::LoadAllRequestedModels(true);
    int32 clothesTxdId = RequestTexture(hashes[0]);
    CStreaming::LoadRequestedModels();
    {
        const auto bodyDict = CTxdStore::GetTxd(torsoTxdId);
        const auto normal   = RwTexDictionaryFindNamedTexture(bodyDict, "torso");
        const auto fat      = RwTexDictionaryFindNamedTexture(bodyDict, "torso_fat");
        const auto ripped   = RwTexDictionaryFindNamedTexture(bodyDict, "torso_ripped");
        const auto tex      = CopyTexture(normal);
        if (tattoos) {
            BlendTextures(tex, fat, ripped, factorA, factorB, factorC, 0x6C, tattoos);
            RwTextureDestroy(tattoos);
        } else {
            BlendTextures(tex, fat, ripped, factorA, factorB, factorC, 0x6C);
        }
        CStreaming::RemoveModel(TXDToModelId(torsoTxdId));

        int32 legsTxdId;
        GetTextureFromTxdAndLoadNextTxd(tex, clothesTxdId, CKeyGen::GetUppercaseKey("player_legs"), &legsTxdId);
        RwTextureSetName(tex, "torso");
        RwTexDictionaryAddTexture(dict, tex);

        //> Legs
        CStreaming::LoadAllRequestedModels(true);
        clothesTxdId = RequestTexture(hashes[2]);
        CStreaming::LoadRequestedModels();
        const auto legsDict   = CTxdStore::GetTxd(legsTxdId);
        const auto legsNormal = RwTexDictionaryFindNamedTexture(legsDict, "legs");
        const auto legsFat    = RwTexDictionaryFindNamedTexture(legsDict, "legs_fat");
        const auto legsRipped = RwTexDictionaryFindNamedTexture(legsDict, "legs_ripped");
        const auto legsTex    = CopyTexture(legsNormal);
        BlendTextures(legsTex, legsFat, legsRipped, factorA, factorB, factorC, 0x6C);
        CStreaming::RemoveModel(TXDToModelId(legsTxdId));

        int32 faceTxdId;
        GetTextureFromTxdAndLoadNextTxd(
            legsTex,
            clothesTxdId,
            hashes[1] ? hashes[1] : CKeyGen::GetUppercaseKey("player_face"),
            &faceTxdId
        );
        RwTextureSetName(legsTex, "legs");
        RwTexDictionaryAddTexture(dict, legsTex);

        //> Face (head)
        CStreaming::LoadAllRequestedModels(true);
        int32 feetTxdId = RequestTexture(hashes[3] ? hashes[3] : CKeyGen::GetUppercaseKey("player_feet"));
        CStreaming::LoadRequestedModels();

        const auto faceDict   = CTxdStore::GetTxd(faceTxdId);
        const auto faceNormal = RwTexDictionaryFindNamedTexture(faceDict, "face");
        const auto faceFat    = RwTexDictionaryFindNamedTexture(faceDict, "face_fat");
        RwTexture* faceTex;
        if (faceNormal) {
            faceTex = CopyTexture(faceNormal);
            if (faceFat) {
                const auto sum = factorA + factorB + factorC;
                BlendTextures(faceTex, faceFat, (factorA + factorC) / sum, factorB / sum, 0x6C);
            }
        } else {
            faceTex = CopyTexture(GetFirstTexture(faceDict));
        }
        CStreaming::RemoveModel(TXDToModelId(faceTxdId));
        RwTextureSetName(faceTex, "head");
        RwTexDictionaryAddTexture(dict, faceTex);

        //> Feet + accessories
        int32 txdA{}, txdB{};
        const auto AddIf = [&](RwTexture* t, const char* name) {
            if (t) {
                RwTextureSetName(t, name);
                RwTexDictionaryAddTexture(dict, t);
            }
        };

        const auto feetTex = GetTextureFromTxdAndLoadNextTxd(nullptr, feetTxdId, hashes[13], &txdA);
        RwTextureSetName(feetTex, "feet"); // NOTE: No null check in OG
        RwTexDictionaryAddTexture(dict, feetTex);

        AddIf(GetTextureFromTxdAndLoadNextTxd(nullptr, txdA, hashes[14], &txdB), "necklace");
        AddIf(GetTextureFromTxdAndLoadNextTxd(nullptr, txdB, hashes[15], &txdA), "watch");
        AddIf(GetTextureFromTxdAndLoadNextTxd(nullptr, txdA, hashes[16], &txdB), "glasses");
        AddIf(GetTextureFromTxdAndLoadNextTxd(nullptr, txdB, hashes[17], &txdA), "hat");

        //> Extra
        if (txdA != -1) {
            CStreaming::LoadAllRequestedModels(true);
            const auto extraTex = CopyTexture(GetFirstTexture(CTxdStore::GetTxd(txdA)));
            CStreaming::RemoveModel(TXDToModelId(txdA));
            AddIf(extraTex, "extra1");
        }
    }
}

// 0x5A6530
void CClothesBuilder::ConstructGeometryAndSkinArrays(RpHAnimHierarchy* pBoneHier, RpGeometry** ppGeometry, RwMatrixWeights** ppWeights, uint32** ppIndices, uint32 numModels, RpGeometry** pGeometrys, RpMaterial** pMaterial) {
    // Calculate total number of vertices and triangles
    int32 totalVerts{}, totalTris{};
    for (uint32 i = 0; i < numModels; i++) {
        if (const auto g = pGeometrys[i]) {
            totalVerts += g->numVertices;
            totalTris  += g->numTriangles;
        }
    }

    //> Merge all geometries into a single one
    const auto out = RpGeometryCreate(totalVerts, totalTris, rpGEOMETRYTRISTRIP | rpGEOMETRYTEXTURED | rpGEOMETRYNORMALS | rpGEOMETRYLIGHT /*0x35*/);
    *ppGeometry = out;

    const auto outMT    = out->morphTarget;
    auto       outVerts = outMT->verts;
    auto       outNrmls = outMT->normals;
    auto       outUVs   = out->texCoords[0];
    auto       outTri   = out->triangles;

    int32 vertexBase{};
    for (uint32 i = 0; i < numModels; i++) {
        const auto g = pGeometrys[i];
        if (!g) {
            continue;
        }
        const auto mt = g->morphTarget;
        for (auto v = 0; v < g->numVertices; v++) {
            *outVerts++ = mt->verts[v];
            *outNrmls++ = mt->normals[v];
            *outUVs++   = g->texCoords[0][v];
        }
        const auto srcTris = g->triangles;
        for (auto t = 0; t < g->numTriangles; t++) {
            RpGeometryTriangleSetVertexIndices(
                out,
                outTri,
                (RwUInt16)(srcTris[t].vertIndex[0] + vertexBase),
                (RwUInt16)(srcTris[t].vertIndex[1] + vertexBase),
                (RwUInt16)(srcTris[t].vertIndex[2] + vertexBase)
            );
            RpGeometryTriangleSetMaterial(out, outTri, pMaterial[i]);
            outTri++;
        }
        vertexBase += g->numVertices;
    }

    RwSphere bounds;
    RpMorphTargetCalcBoundingSphere(outMT, &bounds);
    outMT->boundingSphere = bounds;

    RpGeometryUnlock(out);

    //> Merge skin data
    // NOTE: These are freed in `DestroySkinArrays`
    const auto outWeights = new RwMatrixWeights[totalVerts];
    const auto outBones   = new RwUInt32[totalVerts];
    *ppWeights = outWeights;
    *ppIndices = outBones;

    auto weightsIt = outWeights;
    auto bonesIt   = (uint8*)outBones;
    for (uint32 i = 0; i < numModels; i++) {
        const auto g = pGeometrys[i];
        if (!g) {
            continue;
        }
        const auto skin = RpSkinGeometryGetSkin(g);

        uint8 boneConvTable[64]{}; // NOTSA: OG didn't init this
        BuildBoneIndexConversionTable(boneConvTable, pBoneHier, i);

        for (auto v = 0; v < g->numVertices; v++) {
            const auto srcBones = (const uint8*)RpSkinGetVertexBoneIndices(skin) + v * 4;
            for (auto b = 0; b < 4; b++) {
                *bonesIt++ = boneConvTable[srcBones[b]];
            }
            *weightsIt++ = RpSkinGetVertexBoneWeights(skin)[v];
        }
    }
}

// 0x5A69D0
RpClump* CClothesBuilder::CreateSkinnedClump(RpClump* bones, RwTexDictionary* dict, CPedClothesDesc& ndscr, const CPedClothesDesc* odscr, bool bCutscenePlayer) {
    LoadCdDirectory();

    const struct {
        eClothesModelPart mp;
        const char*       name;
    } parts[]{
        {eClothesModelPart::CLOTHES_MODEL_TORSO, "torso"},
        {eClothesModelPart::CLOTHES_MODEL_HEAD, "head"},
        {eClothesModelPart::CLOTHES_MODEL_HANDS, "hands"},
        {eClothesModelPart::CLOTHES_MODEL_LEGS, "legs"},
        {eClothesModelPart::CLOTHES_MODEL_SHOES, "feet"}
    };
    for (const auto [mp, name] : parts) {
        if (!ndscr.m_anModelKeys[(int)mp]) {
            ndscr.SetModel(name, mp);
        }
    }

    if (odscr) {
        ms_geometryHasChanged = false;
        ms_ratiosHaveChanged  = false;
        if (odscr->m_fFatStat != ndscr.m_fFatStat || odscr->m_fMuscleStat != ndscr.m_fMuscleStat) {
            ms_textureHasChanged = true;
            ms_geometryHasChanged = true;
        } else {
            ms_textureHasChanged = true;
        }
        ms_geometryHasChanged = !rng::equal(ndscr.m_anModelKeys, odscr->m_anModelKeys);
        ms_ratiosHaveChanged  = !rng::equal(ndscr.m_anTextureKeys, odscr->m_anTextureKeys);
        if (!ms_ratiosHaveChanged && !ms_geometryHasChanged && !ms_textureHasChanged) {
            return nullptr;
        }
    } else {
        ms_ratiosHaveChanged = ms_geometryHasChanged = ms_textureHasChanged = true;
    }
    CPedClothesDesc dscr = ndscr;
    PreprocessClothesDesc(dscr, bCutscenePlayer);

    //> 0x5A42B0 - Calculate blend ratios
    float rNormal, rFatness, rMuscle;
    {
        rMuscle  = std::clamp(CStats::GetStatValue(STAT_MUSCLE) / 1000.f, 0.f, 1.f);
        rFatness = std::clamp((dscr.m_fFatStat - 200.f) / 800.f, 0.f, 1.f);
        rNormal  = 1.f - rMuscle - rFatness;
        if (rNormal <= 0.f) {
            const auto t = 1.f / (rFatness + rMuscle);
            rMuscle  *= t;
            rFatness *= t;
            rNormal   = 0.f;
        }
    }

    if ((ms_textureHasChanged || ms_ratiosHaveChanged) && !bCutscenePlayer) {
        RwTexDictionaryForAllTextures(dict, [](RwTexture* t, void* data) {
            RwTexDictionaryRemoveTexture(t);
            RwTextureDestroy(t);
            return t;
        }, nullptr);
        ConstructTextures(dict, dscr.m_anTextureKeys.data(), rNormal, rFatness, rMuscle);
    }

    constexpr auto NO_BODY_PARTS = 10;

    constexpr const char* BODY_PART_TEX_NAMES[NO_BODY_PARTS]{
        "torso",
        "head",
        "torso",
        "legs",
        "feet",
        "necklace",
        "watch",
        "glasses",
        "hat",
        "extra1"
    };

    //> 0x5A6B2C
    RpMaterial*       ms[NO_BODY_PARTS];
    for (uint32 i{}; const auto name : BODY_PART_TEX_NAMES) {
        ms[i++] = [&]() -> RpMaterial* {
            if (const auto tex = RwTexDictionaryFindNamedTexture(dict, name)) {
                const auto mat = RpMaterialCreate();
                RpMaterialSetTexture(mat, tex);
                const auto clr = RwRGBA(0xFF, 0xFF, 0xFF, 0xFF);
                RpMaterialSetColor(mat, &clr);
                return mat;
            }
            return nullptr;
        }();
    }

    //> 0x5A6C5D
    RpGeometry* gs[NO_BODY_PARTS]{};
    ConstructGeometryArray(gs, dscr.m_anModelKeys.data(), rNormal, rFatness, rMuscle);

    //> 0x5A6C6A
    const auto boneAtomic = GetFirstAtomic(bones);
    const auto boneSkin   = RpSkinGeometryGetSkin(RpAtomicGetGeometry(boneAtomic));
    const auto boneAnimHr = RpSkinAtomicGetHAnimHierarchy(boneAtomic);

    RwMatrixWeights* boneWeights;
    RwUInt32*        boneIdxs;
    RpGeometry*      tmpGeo;
    ConstructGeometryAndSkinArrays(
        boneAnimHr,
        &tmpGeo,
        &boneWeights,
        &boneIdxs,
        NO_BODY_PARTS,
        gs,
        ms
    );
    RpSkinGeometrySetSkin(
        tmpGeo,
        RpSkinCreate(
            RpGeometryGetNumVertices(tmpGeo),
            RpSkinGetNumBones(boneSkin),
            boneWeights,
            boneIdxs,
            const_cast<RwMatrix*>(RpSkinGetSkinToBoneMatrices(boneSkin)) // TODO
        )
    );
    DestroySkinArrays(boneWeights, boneIdxs);
    
    const auto hier = RpHAnimHierarchyCreateFromHierarchy(
        boneAnimHr,
        (RpHAnimHierarchyFlag)boneAnimHr->flags,       // TODO: Use function to access
        boneAnimHr->currentAnim->maxInterpKeyFrameSize // TODO: Use function to access
    );

    const auto childFrame = RwFrameCreate();
    RpHAnimFrameSetHierarchy(childFrame, hier);

    const auto atomic = RpAtomicCreate();
    RpAtomicSetGeometry(atomic, tmpGeo, NULL);
    RpSkinAtomicSetHAnimHierarchy(atomic, hier);
    RpAtomicSetFrame(atomic, childFrame);
    RpSkinAtomicSetType(atomic, rpSKINTYPEGENERIC);

    const auto rootFrame = RwFrameCreate();
    RwFrameAddChild(rootFrame, childFrame);

    const auto clump = RpClumpCreate();
    RpClumpSetFrame(clump, rootFrame);
    RpClumpAddAtomic(clump, atomic);

    // Free memory
    {
        RpGeometryDestroy(tmpGeo);

        RwTexDictionarySetCurrent(dict);
        for (auto i = 0; i < NO_BODY_PARTS; i++) {
            if (gs[i]) {
                RpGeometryDestroy(gs[i]);
            }
            if (ms[i]) {
                RpMaterialDestroy(ms[i]);
            }
        }
    }

    return clump;
}
