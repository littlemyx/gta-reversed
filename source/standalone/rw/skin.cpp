// P2B-05a: RpSkin* on top of librw (rw::Skin, skin pipeline plugin).
//   RpSkinPluginAttach, RpSkinGeometry{Get,Set}Skin, RpSkinAtomic{Get,Set}HAnimHierarchy, RpSkinAtomic{Set,Get}Type, RpSkinCreate, RpSkinGetNumBones,
//   RpSkinGetSkinToBoneMatrices, RpSkinGetVertexBoneIndices, RpSkinGetVertexBoneWeights. (RpAnisotPluginAttach lives in platform.cpp, batch 09.)
// Verified against the exe: PluginAttach 0x7C6820, AtomicSetHAnimHierarchy 0x7C7520, AtomicGetHAnimHierarchy 0x7C7540, GeometryGetSkin 0x7C7550, GeometrySetSkin
// 0x7C7560 (+ _rpSkinInitialize 0x7C8740), Create 0x7C75B0, GetNumBones 0x7C77E0, GetVertexBoneWeights 0x7C77F0, GetVertexBoneIndices 0x7C7800,
// GetSkinToBoneMatrices 0x7C7810, AtomicSetType 0x7C7830 (-> 0x7C89B0), AtomicGetType 0x7C7880.
// Layout: librw's Skin keeps weights as float[4] per vertex (= RwMatrixWeights), bone indices as uint8[4] per vertex (= the exe's RwUInt32 per vertex, byte 0 =
// weight 0) and the inverse bone matrices as numBones x 64 bytes (= RwMatrix, flags / pads included), so the three getters are casts. Skin pipelines: the atomic's
// pipeline is librw's skin pipeline (skinGlobals.pipelines[platform], created when the D3D9 driver plugin opens); the skin type is ignored like in the exe
// (0x7C89B0 stores one global pipeline for every type).
// RpSkinGeometrySetSkin sorts every vertex's weights in descending order (bubble sort with the bone indices following) and the used-bone list ascending, like
// _rpSkinInitialize. It does not destroy the previous skin (the exe's _rpSkinDeinitialize is a no-op).
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

#include <algorithm>
#include <cassert>
#include <cstring>

// W: registers librw's skin plugin (geometry skin data, atomic hierarchy slot, stream rights / always callbacks that attach the skin pipeline, D3D9 driver
// plugin) once per Engine::init cycle -- before RwEngineOpen, as the game does.
RwBool RpSkinPluginAttach() {
    if (rw::Geometry::s_plglist.getPluginOffset(rw::ID_SKIN) < 0) {
        rw::registerSkinPlugin();
    }
    return rw::Geometry::s_plglist.getPluginOffset(rw::ID_SKIN) >= 0 ? TRUE : FALSE;
}

// D
RpAtomic* RpSkinAtomicSetHAnimHierarchy(RpAtomic* atomic, RpHAnimHierarchy* hierarchy) {
    rw::Skin::setHierarchy(atomic, hierarchy);
    return atomic;
}

// A: the exe maps the types whose plugin is missing (MatFX 0x120 -> generic, Toon 0x12E -> generic) and then stores the one skin pipeline.
RpAtomic* RpSkinAtomicSetType(RpAtomic* atomic, RpSkinType type) {
    if (type == rpSKINTYPEMATFX && rw::Material::s_plglist.getPluginOffset(rw::ID_MATFX) < 0) {
        type = rpSKINTYPEGENERIC;
    }
    if (type == rpSKINTYPETOON) { // no toon module in librw
        type = rpSKINTYPEGENERIC;
    }
    rw::Skin::setPipeline(atomic, type);
    return atomic;
}

// A: 0x7C7880 -- the pipeline's plugin data when it belongs to the skin plugin, else rpNASKINTYPE.
RpSkinType RpSkinAtomicGetType(RpAtomic* atomic) {
    if (atomic->pipeline && atomic->pipeline->pluginID == rw::ID_SKIN) {
        return static_cast<RpSkinType>(atomic->pipeline->pluginData);
    }
    return rpNASKINTYPE;
}

// W: _rpSkinInitialize 0x7C8740: weights of each vertex whose first weight is < 1.0 are bubble-sorted descending (strict `<` swaps) with the bone indices
// following; then the used-bone list is sorted ascending.
RpGeometry* RpSkinGeometrySetSkin(RpGeometry* geometry, RpSkin* skin) {
    if (rw::Skin::get(geometry) == skin) {
        return geometry;
    }
    rw::Skin::set(geometry, skin);
    if (!skin) {
        return geometry;
    }
    float* w = skin->weights;
    uint8_t* idx = skin->indices;
    for (RwInt32 v = 0; w && idx && v < geometry->numVertices; v++, w += 4, idx += 4) {
        if (w[0] >= 1.0f) {
            continue;
        }
        bool swapped;
        do {
            swapped = false;
            for (int k = 0; k < 3; k++) {
                if (w[k] < w[k + 1]) {
                    std::swap(w[k], w[k + 1]);
                    std::swap(idx[k], idx[k + 1]);
                    swapped = true;
                }
            }
        } while (swapped);
    }
    if (skin->usedBones) {
        std::sort(skin->usedBones, skin->usedBones + skin->numUsedBones);
    }
    return geometry;
}

// W: 0x7C75B0. Copies the three arrays; the used-bone list is built in first-seen order from the vertices' non-zero weights below `numWeights` (the maximum
// number of leading non-zero weights over all vertices). The skin is NOT sorted until RpSkinGeometrySetSkin.
RpSkin* RpSkinCreate(RwUInt32 numVertices, RwUInt32 numBones, RwMatrixWeights* vertexWeights, RwUInt32* vertexIndices, RwMatrix* inverseMatrices) {
    RpSkin* skin = rwNewT(rw::Skin, 1, rw::MEMDUR_EVENT | rw::ID_SKIN);
    if (!skin) {
        return nullptr;
    }
    std::memset(skin, 0, sizeof(*skin));
    skin->init(static_cast<rw::int32>(numBones), static_cast<rw::int32>(numBones), static_cast<rw::int32>(numVertices));
    skin->numWeights = 1;
    if (vertexWeights) {
        const float* w = reinterpret_cast<const float*>(vertexWeights);
        for (RwUInt32 v = 0; v < numVertices; v++, w += 4) {
            while (skin->numWeights < 4 && w[skin->numWeights] != 0.0f) {
                skin->numWeights++;
            }
        }
    }
    skin->numUsedBones = 0;
    if (vertexWeights && vertexIndices) {
        const float* w = reinterpret_cast<const float*>(vertexWeights);
        const uint8_t* ix = reinterpret_cast<const uint8_t*>(vertexIndices);
        for (RwUInt32 v = 0; v < numVertices; v++, w += 4, ix += 4) {
            for (rw::int32 k = 0; k < skin->numWeights; k++) {
                if (w[k] == 0.0f) {
                    continue;
                }
                bool seen = false;
                for (rw::int32 u = 0; u < skin->numUsedBones && !seen; u++) {
                    seen = skin->usedBones[u] == ix[k];
                }
                if (!seen && skin->numUsedBones < static_cast<rw::int32>(numBones)) {
                    skin->usedBones[skin->numUsedBones++] = ix[k];
                }
            }
        }
    }
    if (inverseMatrices && skin->inverseMatrices) {
        std::memcpy(skin->inverseMatrices, inverseMatrices, numBones * 64);
    }
    if (vertexIndices && skin->indices) {
        std::memcpy(skin->indices, vertexIndices, numVertices * 4);
    }
    if (vertexWeights && skin->weights) {
        std::memcpy(skin->weights, vertexWeights, numVertices * 16);
    }
    return skin;
}

RwUInt32 RpSkinGetNumBones(RpSkin* skin) {
    return static_cast<RwUInt32>(skin->numBones);
}

const RwMatrix* RpSkinGetSkinToBoneMatrices(RpSkin* skin) {
    return reinterpret_cast<const RwMatrix*>(skin->inverseMatrices);
}

const RwUInt32* RpSkinGetVertexBoneIndices(RpSkin* skin) {
    return reinterpret_cast<const RwUInt32*>(skin->indices);
}

RwMatrixWeights* RpSkinGetVertexBoneWeights(RpSkin* skin) {
    return reinterpret_cast<RwMatrixWeights*>(skin->weights);
}
#endif // NOTSA_RW_LIBRW
