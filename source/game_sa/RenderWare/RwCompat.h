#pragma once
/*
    Accessors for RenderWare struct fields whose names differ between the real RW 3.6 SDK headers (source/game_sa/RenderWare/rw, DLL / hook-dump
    builds) and the librw-backed fakerw headers (standalone builds, NOTSA_RW_LIBRW). Game code that used to touch such a field directly goes
    through these macros so that ONE spelling compiles in both configurations. Each macro yields an lvalue where the field was one.
*/

#ifdef NOTSA_RW_LIBRW

// RxObjSpace3DVertex::objVertex
#define RwCompatVertexPos(_v)               ((_v).position)
// RwFrame::modelling is available as RwFrameGetMatrix(frame) in both
// RpAtomic::interpolator.flags & rpINTERPOLATORDIRTYSPHERE: librw has no morph-target interpolation, so the sphere is never dirty
#define RwCompatAtomicSphereDirty(_a)       (false)
// RpGeometry::preLitLum
#define RwCompatGeometryPreLit(_g)          ((_g)->colors)
// RpGeometry::morphTarget (array of RpMorphTarget)
#define RwCompatGeometryMorphTargets(_g)    ((_g)->morphTargets)
// RpGeometry::mesh (RpMeshHeader*) and the RpMesh array that follows the header
#define RwCompatGeometryMeshHeader(_g)      ((_g)->meshHeader)
#define RwCompatMeshHeaderMeshes(_h)        ((_h)->getMeshes())
// RxPipeline::pluginId
#define RwCompatPipelinePluginId(_p)        ((_p)->pluginID)
// exe 0x7FE0A0 (_rwD3D9RenderStateVertexAlphaEnable), exported by the pipeline façade (pipeline.cpp)
#define RwCompatVertexAlphaEnable(_b)       _rwD3D9RenderStateVertexAlphaEnable((_b) ? TRUE : FALSE)
// RpTriangle::vertIndex[i]
#define RwCompatTriangleVert(_t, _i)        ((_t).v[_i])
// RpHAnimHierarchy::pNodeInfo (array; elements read with the RW field names nodeID / nodeIndex / flags / pFrame)
#define RwCompatHAnimNodeInfo(_h)           (reinterpret_cast<RpHAnimNodeInfo*>((_h)->nodeInfo))
// RpHAnimHierarchy::currentAnim
#define RwCompatHAnimInterpolator(_h)       ((_h)->interpolator)
// RtAnimInterpolator::pCurrentAnim / maxInterpKeyFrameSize
#define RwCompatInterpCurrentAnim(_i)       ((_i)->currentAnim)
#define RwCompatInterpMaxKeyFrameSize(_i)   ((_i)->maxInterpKeyFrameSize)
// RwTexDictionary::texturesInDict list and RwTexture::lInDictionary link
#define RwCompatTxdTextureList(_d)          (&(_d)->textures)
#define RwCompatTextureInDictLink           inDict

#else

#define RwCompatVertexPos(_v)               ((_v).objVertex)
#define RwCompatGeometryMeshHeader(_g)      ((_g)->mesh)
#define RwCompatMeshHeaderMeshes(_h)        (reinterpret_cast<RpMesh*>((_h) + 1)) // NOTE: `firstMeshOffset` is not used by the original
#define RwCompatPipelinePluginId(_p)        ((_p)->pluginId)
#define RwCompatVertexAlphaEnable(_b)       plugin::Call<0x7FE0A0, uint32>((_b) ? 1u : 0u)
#define RwCompatAtomicSphereDirty(_a)       (((_a)->interpolator.flags & rpINTERPOLATORDIRTYSPHERE) != 0)
#define RwCompatGeometryPreLit(_g)          ((_g)->preLitLum)
#define RwCompatGeometryMorphTargets(_g)    ((_g)->morphTarget)
#define RwCompatTriangleVert(_t, _i)        ((_t).vertIndex[_i])
#define RwCompatHAnimNodeInfo(_h)           ((_h)->pNodeInfo)
#define RwCompatHAnimInterpolator(_h)       ((_h)->currentAnim)
#define RwCompatInterpCurrentAnim(_i)       ((_i)->pCurrentAnim)
#define RwCompatInterpMaxKeyFrameSize(_i)   ((_i)->maxInterpKeyFrameSize)
#define RwCompatTxdTextureList(_d)          (&(_d)->texturesInDict)
#define RwCompatTextureInDictLink           lInDictionary

#endif
