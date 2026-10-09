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
// Plugin data accessors (RW: RpSkin*/RpHAnim* functions that are plain plugin-offset reads)
//--------------------------------------------------------------------------------------------------
inline RpSkin* RpSkinGeometryGetSkin(RpGeometry* geometry) { return rw::Skin::get(geometry); }
inline RpHAnimHierarchy* RpSkinAtomicGetHAnimHierarchy(const RpAtomic* atomic) { return rw::Skin::getHierarchy(atomic); }
