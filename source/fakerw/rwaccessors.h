#pragma once
/*
    fakerw accessor layer (header-defined RW API: the macros of the original headers), first slice of P2B-00c.
    Everything here is a trivial inline over a librw field; anything that needs behaviour lives in source/fakerw/*.cpp (later batches).
    Included at the end of fakerw.h.
*/

//--------------------------------------------------------------------------------------------------
// Object type ids (rpworld.h / rwcore.h macros) -- pinned to librw's Foo::ID
//--------------------------------------------------------------------------------------------------
#define rwFRAME     0
#define rpGEOMETRY  8
#define rpLIGHT     3
#define rpATOMIC    1
#define rpCLUMP     2
#define rpWORLD     7
#define rwCAMERA    4
static_assert(rwFRAME == rw::Frame::ID && rpGEOMETRY == rw::Geometry::ID && rpLIGHT == rw::Light::ID && rpATOMIC == rw::Atomic::ID &&
              rpCLUMP == rw::Clump::ID && rpWORLD == rw::World::ID && rwCAMERA == rw::Camera::ID);
#define rwRASTERLOCKREADWRITE (rwRASTERLOCKREAD | rwRASTERLOCKWRITE)
#define rpHANIMPOPPARENTMATRIX  0x01
#define rpHANIMPUSHPARENTMATRIX 0x02

//--------------------------------------------------------------------------------------------------
// RwObject (rwplcore.h)
//--------------------------------------------------------------------------------------------------
#define RwObjectGetType(o)      (((const RwObject*)(o))->type)
#define rwObjectSetType(o, t)   (((RwObject*)(o))->type) = (RwUInt8)(t)
#define rwObjectGetSubType(o)   (((const RwObject*)(o))->subType)
#define rwObjectSetSubType(o, t)(((RwObject*)(o))->subType) = (RwUInt8)(t)
#define rwObjectGetFlags(o)     (((const RwObject*)(o))->flags)
#define rwObjectSetFlags(o, f)  (((RwObject*)(o))->flags) = (RwUInt8)(f)
#define rwObjectTestFlags(o, f) ((((const RwObject*)(o))->flags) & (RwUInt8)(f))
#define rwObjectGetPrivateFlags(o)    (((const RwObject*)(o))->privateFlags)
#define rwObjectSetPrivateFlags(o, f) (((RwObject*)(o))->privateFlags) = (RwUInt8)(f)
#define rwObjectGetParent(o)    (((const RwObject*)(o))->parent)
#define rwObjectSetParent(c, p) (((RwObject*)(c))->parent) = (void*)(p)

//--------------------------------------------------------------------------------------------------
// RwFrame
//--------------------------------------------------------------------------------------------------
#define RwFrameGetParent(_f)    ((RwFrame*)rwObjectGetParent(_f))
#define RwFrameGetMatrix(_f)    (&(_f)->matrix)
// RwFrame::modelling in the original is librw's `matrix`; kept for the few direct field users until P2B-00d rewrites them
inline RwMatrix* RwFrameGetModelling(RwFrame* frame) { return &frame->matrix; }
inline const RwMatrix* RwFrameGetModelling(const RwFrame* frame) { return &frame->matrix; }

//--------------------------------------------------------------------------------------------------
// RwCamera
//--------------------------------------------------------------------------------------------------
#define RwCameraGetFrame(_camera)       ((RwFrame*)rwObjectGetParent(_camera))
#define RwCameraGetRaster(_camera)      ((_camera)->frameBuffer)
#define RwCameraGetZRaster(_camera)     ((_camera)->zBuffer)
#define RwCameraGetNearClipPlane(_camera)   ((_camera)->nearPlane)
#define RwCameraGetFarClipPlane(_camera)    ((_camera)->farPlane)
#define RwCameraGetFogDistance(_camera)     ((_camera)->fogPlane)
#define RwCameraGetViewWindow(_camera)      (&(_camera)->viewWindow)
#define RwCameraGetViewOffset(_camera)      (&(_camera)->viewOffset)
#define RwCameraGetViewMatrix(_camera)      (&(_camera)->viewMatrix)
#define RwCameraGetProjection(_camera)      ((RwCameraProjection)(_camera)->projection)

//--------------------------------------------------------------------------------------------------
// RpAtomic / RpClump / RpGeometry / RpMaterial / RpLight
//--------------------------------------------------------------------------------------------------
#define RpAtomicGetGeometry(_atomic)    ((_atomic)->geometry)
#define RpAtomicGetFrame(_atomic)       ((RwFrame*)rwObjectGetParent(_atomic))
#define RpAtomicGetClump(_atomic)       ((_atomic)->clump)
#define RpAtomicGetFlags(_atomic)       (rwObjectGetFlags(_atomic))
#define RpAtomicSetFlags(_atomic, _flags) (rwObjectSetFlags(_atomic, _flags))
#define RpClumpGetFrame(_clump)         ((RwFrame*)rwObjectGetParent(_clump))
#define RpLightGetFrame(_light)         ((RwFrame*)rwObjectGetParent(_light))
#define RpLightGetFlags(_light)         (rwObjectGetFlags(_light))
#define RpLightSetFlags(_light, _flags) (rwObjectSetFlags(_light, _flags))
#define RpLightGetType(_light)          ((RpLightType)rwObjectGetSubType(_light))
#define RpLightGetColor(_light)         (&(_light)->color)
#define RpLightGetRadius(_light)        ((_light)->radius)
#define RpMaterialGetTexture(_mat)      ((_mat)->texture)
#define RpMaterialGetColor(_mat)        (&(_mat)->color)
#define RpMaterialGetSurfaceProperties(_mat) (&(_mat)->surfaceProps)
#define RpGeometryGetFlags(_g)          ((_g)->flags)
#define RpGeometryGetNumVertices(_g)    ((_g)->numVertices)
#define RpGeometryGetNumTriangles(_g)   ((_g)->numTriangles)
#define RpGeometryGetTriangles(_g)      ((_g)->triangles)
#define RpGeometryGetNumMaterials(_g)   ((_g)->matList.numMaterials)
#define RpGeometryGetMaterial(_g, _i)   ((_g)->matList.materials[_i])

//--------------------------------------------------------------------------------------------------
// RwTexture / RwRaster
//--------------------------------------------------------------------------------------------------
#define RwTextureGetRaster(_tex)        ((_tex)->raster)
#define RwTextureGetName(_tex)          ((_tex)->name)
#define RwTextureGetMaskName(_tex)      ((_tex)->mask)
#define RwTextureAddRef(_tex)           ((_tex)->addRef(), (_tex))
#define RwRasterGetWidth(_r)            ((_r)->width)
#define RwRasterGetHeight(_r)           ((_r)->height)
#define RwRasterGetDepth(_r)            ((_r)->depth)
#define RwRasterGetStride(_r)           ((_r)->stride)
#define RwRasterGetFormat(_r)           ((RwRasterFormat)((_r)->format & rwRASTERFORMATPIXELFORMATMASK))
#define RwRasterGetType(_r)             ((RwRasterType)((_r)->type & rwRASTERTYPEMASK))
#define RwRasterGetParent(_r)           ((_r)->parent)

//--------------------------------------------------------------------------------------------------
// Maths (rwplcore.h): vector / matrix / colour helpers
//--------------------------------------------------------------------------------------------------
#define RwMatrixGetRight(m)     (&(m)->right)
#define RwMatrixGetUp(m)        (&(m)->up)
#define RwMatrixGetAt(m)        (&(m)->at)
#define RwMatrixGetPos(m)       (&(m)->pos)
#define RwV3dAssign(o, i)       (*(o) = *(i))
#define RwV2dAssign(o, i)       (*(o) = *(i))
#define RwMatrixCopy(dst, src)  (*(dst) = *(src))
#define RwRGBAAssign(o, i)      (*(o) = *(i))
#define RwRGBARealAssign(o, i)  (*(o) = *(i))

//--------------------------------------------------------------------------------------------------
// Constants / misc macros of rwplcore.h
//--------------------------------------------------------------------------------------------------
#ifndef rwPI
#define rwPI            ((RwReal)(3.1415926535897932384626433f))
#define rwPIOVER2       (rwPI / (RwReal)(2.0f))
#endif
#define RWRAD2DEG(_x)   ((_x) * (((RwReal)180) / (rwPI)))
#define RWDEG2RAD(_x)   ((_x) * (rwPI / ((RwReal)180)))
#undef RwRealMAXVAL
#define RwRealMAXVAL    ((RwReal)(3.40282347e+38))
#define RWASSERT(condition) ((void)0)
#define RWPLUGINOFFSET(_type, _base, _offset)       ((_type*)(((RwUInt8*)(_base)) + (_offset)))
#define RWPLUGINOFFSETCONST(_type, _base, _offset)  ((const _type*)(((const RwUInt8*)(_base)) + (_offset)))
#define rwLLLinkGetData(linkvar, type, entry)   ((type*)(((RwUInt8*)(linkvar)) - offsetof(type, entry)))
#define rwLLLinkGetNext(linkvar)                ((linkvar)->next)
#define rwLLLinkGetPrevious(linkvar)            ((linkvar)->prev)
#define rwLinkListGetFirstLLLink(list)          ((list)->link.next)
#define rwLinkListGetLastLLLink(list)           ((list)->link.prev)
#define rwLinkListGetTerminator(list)           (&(list)->link)
#define rwMatrixSetFlags(m, flagsbit)           ((m)->flags = (flagsbit))
#define rwMatrixGetFlags(m)                     ((m)->flags)
#define rwMatrixTestFlags(m, flagsbit)          ((m)->flags & (flagsbit))
// the original RwFree went through the engine's memory functions; the shim implements it on rw::Engine::memfuncs
void RwFree(void* ptr);
#define RwDebugSendMessage(type, funcName, message) ((void)0)
typedef void (*RwDebugHandler)(RwDebugType type, const RwChar* string);
#define RwDebugSetHandler(handler)      ((void)0)
#define RwDebugSetTraceState(state)     ((void)0)

//--------------------------------------------------------------------------------------------------
// RwMatrix / RwV3d / RwV2d macros (rwplcore.h)
//--------------------------------------------------------------------------------------------------
#define RwMatrixSetIdentity(m)  ((m)->setIdentity(), (m))
#define RwV2dSub(o, a, b)       ((o)->x = (a)->x - (b)->x, (o)->y = (a)->y - (b)->y)
#define RwV3dAdd(o, a, b)       ((o)->x = (a)->x + (b)->x, (o)->y = (a)->y + (b)->y, (o)->z = (a)->z + (b)->z)
#define RwV3dSub(o, a, b)       ((o)->x = (a)->x - (b)->x, (o)->y = (a)->y - (b)->y, (o)->z = (a)->z - (b)->z)
#define RwV3dScale(o, a, s)     ((o)->x = (a)->x * (s), (o)->y = (a)->y * (s), (o)->z = (a)->z * (s))
#define RwV3dIncrementScaled(o, a, s) ((o)->x += (a)->x * (s), (o)->y += (a)->y * (s), (o)->z += (a)->z * (s))
#define RwV3dNegate(o, a)       ((o)->x = -(a)->x, (o)->y = -(a)->y, (o)->z = -(a)->z)
#define RwV3dDotProduct(a, b)   ((a)->x * (b)->x + (a)->y * (b)->y + (a)->z * (b)->z)
#define RwV3dCrossProduct(o, a, b) ((o)->x = (a)->y * (b)->z - (a)->z * (b)->y, (o)->y = (a)->z * (b)->x - (a)->x * (b)->z, (o)->z = (a)->x * (b)->y - (a)->y * (b)->x)

//--------------------------------------------------------------------------------------------------
// RwCamera / RwFrame object setters
//--------------------------------------------------------------------------------------------------
inline void rwObjectHasFrameSetFrame(void* object, RwFrame* frame) { static_cast<rw::ObjectWithFrame*>(object)->setFrame(frame); }
#define RwCameraSetFrame(_camera, _frame)       (rwObjectHasFrameSetFrame(&(_camera)->object.object, (_frame)), (_camera))
#define RwCameraSetRaster(_camera, _raster)     ((_camera)->frameBuffer = (_raster), (_camera))
#define RwCameraSetZRaster(_camera, _raster)    ((_camera)->zBuffer = (_raster), (_camera))
#define RpClumpSetFrame(_clump, _frame)         (rwObjectSetParent(_clump, _frame), (_clump))
#define RpLightSetFrame(_light, _frame)         (rwObjectHasFrameSetFrame(&(_light)->object.object, (_frame)), (_light))

//--------------------------------------------------------------------------------------------------
// RpAtomic: bounding sphere + render callback. librw's `renderCB` returns void while the game's callback returns the atomic:
// the shim keeps the game callback in an atomic plugin slot and installs a trampoline (P2B-04c); these are real functions there.
//--------------------------------------------------------------------------------------------------
#define RpAtomicGetBoundingSphere(_atomic)  (&(_atomic)->boundingSphere)
RpAtomic*               RpAtomicRender(RpAtomic* atomic);
#define RpAtomicRenderMacro(_atomic)        RpAtomicRender(_atomic)
RpAtomic*               RpAtomicSetRenderCallBack(RpAtomic* atomic, RpAtomicCallBackRender callback);
RpAtomicCallBackRender  RpAtomicGetRenderCallBack(const RpAtomic* atomic);
RpAtomic*               AtomicDefaultRenderCallBack(RpAtomic* atomic);
#define RpAtomicGetInterpolatorFlags(_atomic)   (0)

//--------------------------------------------------------------------------------------------------
// RpGeometry / RpMorphTarget / RpMaterial
//--------------------------------------------------------------------------------------------------
#define RpGeometryGetMorphTarget(_g, _index)    (&(_g)->morphTargets[_index])
#define RpGeometryGetNumMorphTargets(_g)        ((_g)->numMorphTargets)
#define RpGeometryGetPreLightColors(_g)         ((_g)->colors)
#define RpGeometryGetVertexTexCoords(_g, _uvIndex) ((_g)->texCoords[(_uvIndex) - 1])
#define RpGeometryGetNumTexCoordSets(_g)        ((_g)->numTexCoordSets)
#define RpGeometrySetFlags(_g, _flags)          ((_g)->flags = (_flags), (_g))
#define RpMorphTargetGetBoundingSphere(_mt)     (&(_mt)->boundingSphere)
#define RpMorphTargetSetBoundingSphere(_mt, _s) ((_mt)->boundingSphere = *(_s), (_mt))
#define RpMorphTargetGetVertices(_mt)           ((_mt)->vertices)
#define RpMorphTargetGetVertexNormals(_mt)      ((_mt)->normals)
#define RpMaterialSetColor(_m, _c)              ((_m)->color = *(_c), (_m))
#define RpMaterialSetSurfaceProperties(_m, _sp) ((_m)->surfaceProps = *(_sp), (_m))
#define RpMaterialGetPipeline(_m)               ((_m)->pipeline)
#define RpLightSetRadiusMacro(_l, _r)           ((_l)->radius = (_r))

//--------------------------------------------------------------------------------------------------
// RwTexture / RwImage / RwRaster
//--------------------------------------------------------------------------------------------------
#define RwTextureGetFilterMode(_tex)            ((RwTextureFilterMode)((_tex)->filterAddressing & rwTEXTUREFILTERMODEMASK))
#define RwTextureSetFilterMode(_tex, _filtering) ((_tex)->setFilter((rw::Texture::FilterMode)(_filtering)), (_tex))
#define RwTextureSetFilterModeMacro(_tex, _filtering) RwTextureSetFilterMode(_tex, _filtering)
#define RwTextureGetAddressingU(_tex)           ((RwTextureAddressMode)(((_tex)->filterAddressing & rwTEXTUREADDRESSINGUMASK) >> 8))
#define RwTextureGetAddressingV(_tex)           ((RwTextureAddressMode)(((_tex)->filterAddressing & rwTEXTUREADDRESSINGVMASK) >> 12))
#define RwTextureSetAddressingU(_tex, _a)       ((_tex)->setAddressU((rw::Texture::Addressing)(_a)), (_tex))
#define RwTextureSetAddressingV(_tex, _a)       ((_tex)->setAddressV((rw::Texture::Addressing)(_a)), (_tex))
#define RwTextureSetAddressing(_tex, _a)        ((_tex)->setAddressU((rw::Texture::Addressing)(_a)), (_tex)->setAddressV((rw::Texture::Addressing)(_a)), (_tex))
#define RwImageGetPixels(_image)                ((_image)->pixels)
#define RwImageGetPalette(_image)               ((_image)->palette)
#define RwImageGetWidth(_image)                 ((_image)->width)
#define RwImageGetHeight(_image)                ((_image)->height)
#define RwImageGetDepth(_image)                 ((_image)->depth)
#define RwImageGetStride(_image)                ((_image)->stride)

//--------------------------------------------------------------------------------------------------
// Im2D / Im3D / object-space vertex setters. RwIm2DVertex is the game-owned D3D9 form {x,y,z,rhw,emissiveColor,u,v};
// RwIm3DVertex = RxObjSpace3DVertex = rw::d3d::Im3DVertex {position, normal, color, u, v} (original: objVertex, objNormal, color, u, v).
//--------------------------------------------------------------------------------------------------
#define RwIm2DGetNearScreenZ()  (RwEngineInstance->dOpenDevice.zBufferNear)
#define RwIm2DGetFarScreenZ()   (RwEngineInstance->dOpenDevice.zBufferFar)
#define RwIm2DGetNearScreenZMacro() RwIm2DGetNearScreenZ()
#define RwIm2DVertexSetScreenX(vert, scrnx)         ((vert)->x = (scrnx))
#define RwIm2DVertexSetScreenY(vert, scrny)         ((vert)->y = (scrny))
#define RwIm2DVertexSetScreenZ(vert, scrnz)         ((vert)->z = (scrnz))
#define RwIm2DVertexSetCameraZ(vert, camz)          ((void)0)
#define RwIm2DVertexSetRecipCameraZ(vert, recipz)   ((vert)->rhw = (recipz))
#define RwIm2DVertexSetU(vert, texU, recipz)        ((vert)->u = (texU))
#define RwIm2DVertexSetV(vert, texV, recipz)        ((vert)->v = (texV))
#define RwIm2DVertexSetIntRGBA(vert, red, green, blue, alpha) \
    ((vert)->emissiveColor = (((RwUInt32)(alpha)) << 24) | (((RwUInt32)(red)) << 16) | (((RwUInt32)(green)) << 8) | ((RwUInt32)(blue)))
#define RwIm2DVertexSetRealRGBA(vert, red, green, blue, alpha) \
    RwIm2DVertexSetIntRGBA(vert, (RwUInt32)((red) * 255.0f + 0.5f), (RwUInt32)((green) * 255.0f + 0.5f), (RwUInt32)((blue) * 255.0f + 0.5f), (RwUInt32)((alpha) * 255.0f + 0.5f))
RwBool RwIm2DRenderLine(RwIm2DVertex* vertices, RwInt32 numVertices, RwInt32 vert1, RwInt32 vert2);
RwBool RwIm2DRenderTriangle(RwIm2DVertex* vertices, RwInt32 numVertices, RwInt32 vert1, RwInt32 vert2, RwInt32 vert3);
RwBool RwIm2DRenderPrimitive(RwPrimitiveType primType, RwIm2DVertex* vertices, RwInt32 numVertices);
RwBool RwIm2DRenderIndexedPrimitive(RwPrimitiveType primType, RwIm2DVertex* vertices, RwInt32 numVertices, RwImVertexIndex* indices, RwInt32 numIndices);

#define RxObjSpace3DVertexSetPos(_vert, _pos)       ((_vert)->position = *(_pos))
#define RxObjSpace3DVertexGetPos(_vert, _pos)       (*(_pos) = (_vert)->position)
#define RxObjSpace3DVertexSetPreLitColor(_vert, _col) \
    ((_vert)->color = (((RwUInt32)(_col)->alpha) << 24) | (((RwUInt32)(_col)->red) << 16) | (((RwUInt32)(_col)->green) << 8) | ((RwUInt32)(_col)->blue))
#define RxObjSpace3DVertexSetU(_vert, _imu)         ((_vert)->u = (_imu))
#define RxObjSpace3DVertexSetV(_vert, _imv)         ((_vert)->v = (_imv))
#define RxObjSpace3DVertexSetNormal(_vert, _nx, _ny, _nz) ((_vert)->normal.x = (_nx), (_vert)->normal.y = (_ny), (_vert)->normal.z = (_nz))
#define RxObjSpace3DLitVertexSetU(_vert, _imu)      RxObjSpace3DVertexSetU(_vert, _imu)
#define RxObjSpace3DLitVertexSetV(_vert, _imv)      RxObjSpace3DVertexSetV(_vert, _imv)
#define RwIm3DVertexSetU(_vert, _imu)               RxObjSpace3DVertexSetU(_vert, _imu)
#define RwIm3DVertexSetV(_vert, _imv)               RxObjSpace3DVertexSetV(_vert, _imv)
#define RwIm3DVertexSetPos(_vert, _imx, _imy, _imz) ((_vert)->position.x = (_imx), (_vert)->position.y = (_imy), (_vert)->position.z = (_imz))
#define RwIm3DVertexGetPos(_vert)                   (&((_vert)->position))
#define RwIm3DVertexSetRGBA(_vert, _r, _g, _b, _a)  ((_vert)->color = (((RwUInt32)(_a)) << 24) | (((RwUInt32)(_r)) << 16) | (((RwUInt32)(_g)) << 8) | ((RwUInt32)(_b)))

//--------------------------------------------------------------------------------------------------
// Render states: the original macro dereferenced RwEngineInstance->dOpenDevice.fpRenderStateSet; here real functions (P2B-02c maps RW's
// render-state numbering onto librw's, including the stencil states)
//--------------------------------------------------------------------------------------------------
RwBool RwRenderStateSet(RwRenderState state, void* value);
RwBool RwRenderStateGet(RwRenderState state, void* value);

//--------------------------------------------------------------------------------------------------
// RtQuat: the original macros are inline math; the shim implements them (P2B-05c)
//--------------------------------------------------------------------------------------------------
void RtQuatConvertToMatrix(const RtQuat* qpQuat, RwMatrix* mpMatrix);
void RtQuatUnitConvertToMatrix(const RtQuat* qpQuat, RwMatrix* mpMatrix);
void RtQuatSlerp(RtQuat* qpResult, const RtQuat* qpFrom, const RtQuat* qpTo, RwReal rT, RtQuatSlerpCache* sCache);

//--------------------------------------------------------------------------------------------------
// RtAnim / RpHAnim / RpSkin plugin-data accessors
//--------------------------------------------------------------------------------------------------
#define rtANIMGETINTERPFRAME(anim, nodeIndex) \
    ((void*)(((RwUInt8*)&((anim)[1])) + ((nodeIndex) * (anim)->currentInterpKeyFrameSize)))
inline RpSkin* RpSkinGeometryGetSkin(RpGeometry* geometry) { return rw::Skin::get(geometry); }
inline RpHAnimHierarchy* RpSkinAtomicGetHAnimHierarchy(const RpAtomic* atomic) { return rw::Skin::getHierarchy(atomic); }
