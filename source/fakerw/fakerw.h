#pragma once
/*
    fakerw: the RenderWare C API of the original game, provided on top of librw (vendor/librw, pinned) for the standalone build.

    This is the TYPE layer (P2B-00b): typedefs/aliases of the RW C type names onto rw::*, the RW enums with their ORIGINAL numbering
    (rwenums.h, generated), game-owned structs for what librw lacks, and static_asserts that pin every layout/value we rely on.
    The function/macro API is added module by module (P2B-00c ...). Design notes: .notes/P2B_SHIM_PLAN.md.

    Every header with a RenderWare file name (rwcore.h, rpworld.h, ...) in this directory just includes this file; the game picks them up
    instead of source/game_sa/RenderWare/rw/*.h when NOTSA_RW_LIBRW is defined (see source/CMakeLists.txt).
    librw is included ONLY from here: never `using namespace rw;` in a header (rw::int32/byte/uint clash with the game's names).
*/
#ifndef NOTSA_RW_LIBRW
#error "fakerw is only for the NOTSA_RW_LIBRW (librw) build; the DLL/hook-dump builds use source/game_sa/RenderWare/rw"
#endif

#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <type_traits>

// librw's D3D9 platform needs windows.h (EngineOpenParams::window) and d3d9.h (WITH_D3D) BEFORE rw.h; both are ordered here.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d9.h>
#ifndef WITH_D3D
#define WITH_D3D
#endif
#ifndef RW_D3D9
#define RW_D3D9
#endif

#include <rw.h>

// Macros leaked by librw: the game (and its other dependencies) must not see them.
#ifdef nil
#undef nil
#endif
#ifdef nelem
#undef nelem
#endif

//--------------------------------------------------------------------------------------------------
// Basic types (os/win/ostypes: ILP32, LP64 never matters here)
//--------------------------------------------------------------------------------------------------
#define rwLITTLEENDIAN
typedef long                RwFixed;
typedef int                 RwInt32;
typedef unsigned int        RwUInt32;
typedef short               RwInt16;
typedef unsigned short      RwUInt16;
typedef unsigned char       RwUInt8;
typedef signed char         RwInt8;
typedef char                RwChar;
typedef float               RwReal;
typedef RwInt32             RwBool;
typedef __int64             RwInt64;
typedef unsigned __int64    RwUInt64;
#define RWZERO64 ((RwUInt64)0)

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define RwRealMAXVAL (3.402823466e+38F)
#define RwRealMINVAL (-3.402823466e+38F)

//--------------------------------------------------------------------------------------------------
// Enums with the original RW numbering (generated), then the static_asserts against librw's own numbering
//--------------------------------------------------------------------------------------------------
#include "rwenums.h"

//--------------------------------------------------------------------------------------------------
// Math / plain data types: layout-identical to RW 3.6 (asserted below)
//--------------------------------------------------------------------------------------------------
typedef rw::V2d         RwV2d;
typedef rw::V3d         RwV3d;
typedef rw::V4d         RwV4d;
typedef rw::RGBA        RwRGBA;
typedef rw::RGBAf       RwRGBAReal;
typedef rw::TexCoords   RwTexCoords;
typedef rw::Sphere      RwSphere;
typedef rw::BBox        RwBBox;
typedef rw::Plane       RwPlane;
typedef rw::Rect        RwRect;
typedef rw::Line        RwLine;
typedef rw::Matrix      RwMatrix;
typedef rw::Matrix      RwMatrixTag; // the original struct tag, spelled out in a few game headers
typedef rw::LLLink      RwLLLink;
typedef rw::LinkList    RwLinkList;
typedef rw::SurfaceProperties RwSurfaceProperties;

// Objects
typedef rw::Object              RwObject;
typedef rw::ObjectWithFrame     RwObjectHasFrame;
typedef rw::Frame               RwFrame;
typedef rw::Camera              RwCamera;
typedef rw::Raster              RwRaster;
typedef rw::Image               RwImage;
typedef rw::Texture             RwTexture;
typedef rw::TexDictionary       RwTexDictionary;
typedef rw::Stream              RwStream;
typedef rw::World               RpWorld;
typedef rw::Light               RpLight;
typedef rw::Clump               RpClump;
typedef rw::Atomic              RpAtomic;
typedef rw::Geometry            RpGeometry;
typedef rw::Material            RpMaterial;
typedef rw::MaterialList        RpMaterialList;
typedef rw::MorphTarget         RpMorphTarget;
typedef rw::Triangle            RpTriangle;
typedef rw::Mesh                RpMesh;
typedef rw::MeshHeader          RpMeshHeader;
typedef rw::Skin                RpSkin;
typedef rw::HAnimHierarchy      RpHAnimHierarchy;
typedef rw::HAnimKeyFrame       RpHAnimKeyFrame;
typedef rw::Animation           RtAnimAnimation;
typedef rw::AnimInterpolator    RtAnimInterpolator;
typedef rw::Charset             RtCharset;
typedef rw::MemoryFunctions     RwMemoryFunctions;
typedef rw::SubSystemInfo       RwSubSystemInfo;

// RpHAnimNodeInfo with the RW field names (librw: id/index/flags/frame, same layout): the game reads hierarchy->nodeInfo through
// RwCompatHAnimNodeInfo(h) (reinterpreted), see game_sa/RenderWare/RwCompat.h
struct RpHAnimNodeInfo {
    RwInt32     nodeID;
    RwInt32     nodeIndex;
    RwInt32     flags;
    RwFrame*    pFrame;
};
static_assert(sizeof(RpHAnimNodeInfo) == sizeof(rw::HAnimNodeInfo) && offsetof(RpHAnimNodeInfo, nodeID) == offsetof(rw::HAnimNodeInfo, id) &&
              offsetof(RpHAnimNodeInfo, nodeIndex) == offsetof(rw::HAnimNodeInfo, index) && offsetof(RpHAnimNodeInfo, flags) == offsetof(rw::HAnimNodeInfo, flags) &&
              offsetof(RpHAnimNodeInfo, pFrame) == offsetof(rw::HAnimNodeInfo, frame));

// Vertex index types
typedef RwUInt16                RxVertexIndex;
typedef RxVertexIndex           RwImVertexIndex;

// 3D immediate-mode / object-space vertex: librw's D3D9 vertex (position, normal, color, u, v) = RW's RxObjSpace3DVertex field for field,
// only the first member is named `position` instead of `objVertex` (and `normal` for `objNormal`): ~20 direct uses are rewritten in P2B-00d.
typedef rw::d3d::Im3DVertex     RxObjSpace3DVertex;
typedef RxObjSpace3DVertex      RxObjSpace3DLitVertex;
typedef RxObjSpace3DLitVertex   RwIm3DVertex;

// ---- Game-owned types: librw has no equivalent (or a different one) and the game reads the fields directly ----

// 2D immediate-mode vertex: RW's D3D9 form carries 1/w (`rhw`) and the packed colour as `emissiveColor`; librw's Im2DVertex has camera Z instead.
// RwIm2DRender* (P2B-06) converts into a scratch rw::im2d vertex array.
struct RwD3D9Vertex {
    RwReal      x;
    RwReal      y;
    RwReal      z;
    RwReal      rhw;
    RwUInt32    emissiveColor;
    RwReal      u;
    RwReal      v;
};
typedef RwD3D9Vertex RwIm2DVertex;

// RtQuat: the game reads imag/real (Quaternion.h); same memory layout as rw::Quat {x,y,z,w}, converted by reinterpretation in the shim (RtQuat*)
struct RtQuat {
    RwV3d   imag;
    RwReal  real;
};

struct RwMatrixWeights {
    RwReal w0, w1, w2, w3;
};

struct RwChunkHeaderInfo {
    RwUInt32 type;
    RwUInt32 length;
    RwUInt32 version;
    RwUInt32 buildNum;
    RwBool   isComplex;
};

// librw's VideoMode has no refresh rate / raster format. The game reads these (VideoMode.cpp), RwEngineGetVideoModeInfo fills them in.
struct RwVideoMode {
    RwInt32         width;
    RwInt32         height;
    RwInt32         depth;
    RwVideoModeFlag flags;
    RwInt32         refRate;
    RwInt32         format;
};

// RwEngineOpenParams: the original is {void* displayID}; librw's rw::EngineOpenParams is {HWND window} (same size): RwEngineOpen converts.
struct RwEngineOpenParams {
    void* displayID;
};
static_assert(sizeof(RwEngineOpenParams) == sizeof(rw::EngineOpenParams));
#define RW_SUBSYSTEMNAME_MAXLEN 80

//--------------------------------------------------------------------------------------------------
// Callback types (the ones whose signature only needs the types above)
//--------------------------------------------------------------------------------------------------
typedef RwObject*       (*RwObjectCallBack)(RwObject* object, void* data);
typedef RwFrame*        (*RwFrameCallBack)(RwFrame* frame, void* data);
typedef RwCamera*       (*RwCameraCallBack)(RwCamera* camera, void* data);
typedef RwTexture*      (*RwTextureCallBack)(RwTexture* texture, void* pData);
typedef RwTexDictionary*(*RwTexDictionaryCallBack)(RwTexDictionary* dict, void* data);
typedef RwTexture*      (*RwTextureCallBackRead)(const RwChar* name, const RwChar* maskName);
typedef RwTexture*      (*RwTextureCallBackFind)(const RwChar* name);
typedef RwImage*        (*RwImageCallBackRead)(const RwChar* imageName);
typedef RwImage*        (*RwImageCallBackWrite)(RwImage* image, const RwChar* imageName);
typedef RpMaterial*     (*RpMaterialCallBack)(RpMaterial* material, void* data);
typedef RpMesh*         (*RpMeshCallBack)(RpMesh* mesh, RpMeshHeader* meshHeader, void* data);
typedef RpGeometry*     (*RpGeometryCallBack)(RpGeometry* geometry, void* data);
typedef RpLight*        (*RpLightCallBack)(RpLight* light, void* data);
typedef RpClump*        (*RpClumpCallBack)(RpClump* clump, void* data);
typedef RpAtomic*       (*RpAtomicCallBack)(RpAtomic* atomic, void* data);
// The game's atomic render callback returns the atomic; librw's `renderCB` returns void -> the shim installs a trampoline (P2B-04c)
typedef RpAtomic*       (*RpAtomicCallBackRender)(RpAtomic* atomic);
typedef void            (*rwD3D9DeviceReleaseCallBack)(void);
typedef void            (*rwD3D9DeviceRestoreCallBack)(void);
typedef RtAnimInterpolator* (*RtAnimCallBack)(RtAnimInterpolator* animInstance, void* data);
typedef RwStream* (*RwPluginDataChunkWriteCallBack)(RwStream* stream, RwInt32 binaryLength, const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
typedef RwStream* (*RwPluginDataChunkReadCallBack)(RwStream* stream, RwInt32 binaryLength, void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
typedef RwInt32   (*RwPluginDataChunkGetSizeCallBack)(const void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
typedef RwBool    (*RwPluginDataChunkAlwaysCallBack)(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
typedef RwBool    (*RwPluginDataChunkRightsCallBack)(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject, RwUInt32 extraData);
typedef void*     (*RwPluginObjectConstructor)(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
typedef void*     (*RwPluginObjectCopy)(void* dstObject, const void* srcObject, RwInt32 offsetInObject, RwInt32 sizeInObject);
typedef void*     (*RwPluginObjectDestructor)(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);

// ---- RtAnim interpolation schemes: the game fills RtAnimInterpolatorInfo with the RW field names (RpAnimBlend.cpp); RtAnimRegisterInterpolationScheme
// (P2B-05c) converts it into rw::AnimInterpolatorInfo (different names / field order) ----
typedef void              (*RtAnimKeyFrameApplyCallBack)(void* result, void* voidIFrame);
typedef void              (*RtAnimKeyFrameBlendCallBack)(void* voidOut, void* voidIn1, void* voidIn2, RwReal alpha);
typedef void              (*RtAnimKeyFrameInterpolateCallBack)(void* voidOut, void* voidIn1, void* voidIn2, RwReal time, void* customData);
typedef void              (*RtAnimKeyFrameAddCallBack)(void* voidOut, void* voidIn1, void* voidIn2);
typedef void              (*RtAnimKeyFrameMulRecipCallBack)(void* voidFrame, void* voidStart);
typedef RtAnimAnimation*  (*RtAnimKeyFrameStreamReadCallBack)(RwStream* stream, RtAnimAnimation* animation);
typedef RwBool            (*RtAnimKeyFrameStreamWriteCallBack)(const RtAnimAnimation* animation, RwStream* stream);
typedef RwInt32           (*RtAnimKeyFrameStreamGetSizeCallBack)(const RtAnimAnimation* animation);

struct RtAnimInterpolatorInfo {
    RwInt32                             typeID;
    RwInt32                             interpKeyFrameSize;
    RwInt32                             animKeyFrameSize;
    RtAnimKeyFrameApplyCallBack         keyFrameApplyCB;
    RtAnimKeyFrameBlendCallBack         keyFrameBlendCB;
    RtAnimKeyFrameInterpolateCallBack   keyFrameInterpolateCB;
    RtAnimKeyFrameAddCallBack           keyFrameAddCB;
    RtAnimKeyFrameMulRecipCallBack      keyFrameMulRecipCB;
    RtAnimKeyFrameStreamReadCallBack    keyFrameStreamReadCB;
    RtAnimKeyFrameStreamWriteCallBack   keyFrameStreamWriteCB;
    RtAnimKeyFrameStreamGetSizeCallBack keyFrameStreamGetSizeCB;
    RwInt32                             customDataSize;
};

// ---- Quaternion slerp caches (rtslerp.h), plain data used by TaskUtilityLineUpPedWithCar / BoneNode ----
struct RtQuatSlerpCache {
    RtQuat  raFrom;
    RtQuat  raTo;
    RwReal  omega;
    RwBool  nearlyZeroOm;
};
struct RtQuatSlerpArgandCache {
    RtQuat  logTo;
    RtQuat  logBase;
};

// ---- Custom pipelines (P2B-07 emulates the RxPipeline API over librw's ObjPipeline): atomics/materials point at the ObjPipeline directly ----
typedef rw::ObjPipeline                 RxPipeline;
typedef rw::d3d9::InstanceDataHeader    RxD3D9ResEntryHeader;
typedef rw::d3d9::InstanceData          RxD3D9InstanceData;

//--------------------------------------------------------------------------------------------------
// Layout / value pins. If one of these fires, the corresponding type/enum can no longer be a plain alias.
//--------------------------------------------------------------------------------------------------
static_assert(sizeof(RwChar) == 1 && sizeof(RwInt32) == 4 && sizeof(RwReal) == 4 && sizeof(RwFixed) == 4 && sizeof(void*) == 4, "x86 ILP32 expected");

static_assert(sizeof(RwV2d) == 8  && offsetof(RwV2d, x) == 0 && offsetof(RwV2d, y) == 4);
static_assert(sizeof(RwV3d) == 12 && offsetof(RwV3d, x) == 0 && offsetof(RwV3d, y) == 4 && offsetof(RwV3d, z) == 8);
static_assert(sizeof(RwV4d) == 16 && offsetof(RwV4d, w) == 12);
static_assert(sizeof(RwRGBA) == 4 && offsetof(RwRGBA, red) == 0 && offsetof(RwRGBA, green) == 1 && offsetof(RwRGBA, blue) == 2 && offsetof(RwRGBA, alpha) == 3);
static_assert(sizeof(RwRGBAReal) == 16 && offsetof(RwRGBAReal, red) == 0 && offsetof(RwRGBAReal, alpha) == 12);
static_assert(sizeof(RwTexCoords) == 8 && offsetof(RwTexCoords, u) == 0 && offsetof(RwTexCoords, v) == 4);
static_assert(sizeof(RwSphere) == 16 && offsetof(RwSphere, center) == 0 && offsetof(RwSphere, radius) == 12);
static_assert(sizeof(RwBBox) == 24 && offsetof(RwBBox, sup) == 0 && offsetof(RwBBox, inf) == 12);
static_assert(sizeof(RwPlane) == 16 && offsetof(RwPlane, normal) == 0 && offsetof(RwPlane, distance) == 12);
static_assert(sizeof(RwRect) == 16 && offsetof(RwRect, x) == 0 && offsetof(RwRect, y) == 4 && offsetof(RwRect, w) == 8 && offsetof(RwRect, h) == 12);
static_assert(sizeof(RwLine) == 24);
static_assert(sizeof(RwLLLink) == 8 && offsetof(RwLLLink, next) == 0 && offsetof(RwLLLink, prev) == 4);
static_assert(sizeof(RwLinkList) == 8);
static_assert(sizeof(RtQuat) == sizeof(rw::Quat) && offsetof(RtQuat, imag) == offsetof(rw::Quat, x) && offsetof(RtQuat, real) == offsetof(rw::Quat, w) &&
              offsetof(rw::Quat, y) == 4 && offsetof(rw::Quat, z) == 8, "RtQuat = {imag(x,y,z), real} = rw::Quat {x,y,z,w}");
static_assert(sizeof(RwSurfaceProperties) == 12);
static_assert(sizeof(RwMatrix) == 64 && offsetof(RwMatrix, right) == 0 && offsetof(RwMatrix, flags) == 12 && offsetof(RwMatrix, up) == 16 &&
              offsetof(RwMatrix, pad1) == 28 && offsetof(RwMatrix, at) == 32 && offsetof(RwMatrix, pad2) == 44 && offsetof(RwMatrix, pos) == 48 &&
              offsetof(RwMatrix, pad3) == 60, "RwMatrix = {right,flags,up,pad1,at,pad2,pos,pad3}");
static_assert(std::is_standard_layout_v<RwMatrix> && std::is_trivially_copyable_v<RwMatrix>);
static_assert(std::is_standard_layout_v<RwV3d> && std::is_trivially_copyable_v<RwV3d>);

// RwObject header: {type, subType, flags, privateFlags, parent}
static_assert(sizeof(RwObject) == 8 && offsetof(RwObject, type) == 0 && offsetof(RwObject, subType) == 1 && offsetof(RwObject, flags) == 2 &&
              offsetof(RwObject, privateFlags) == 3 && offsetof(RwObject, parent) == 4);
static_assert(sizeof(RpTriangle) == 8 && offsetof(RpTriangle, v) == 0 && offsetof(RpTriangle, matId) == 6, "RpTriangle = {vertIndex[3], matIndex}");
static_assert(sizeof(RwMatrixWeights) == 16 && sizeof(RwD3D9Vertex) == 28 && sizeof(RwIm2DVertex) == 28);
static_assert(sizeof(RxObjSpace3DVertex) == 36 && offsetof(RxObjSpace3DVertex, position) == 0 && offsetof(RxObjSpace3DVertex, normal) == 12 &&
              offsetof(RxObjSpace3DVertex, color) == 24 && offsetof(RxObjSpace3DVertex, u) == 28 && offsetof(RxObjSpace3DVertex, v) == 32,
              "RxObjSpace3DVertex = {objVertex, objNormal, color, u, v}");
static_assert(sizeof(RtQuatSlerpCache) == 40 && sizeof(RtQuatSlerpArgandCache) == 32);
static_assert(sizeof(RwChunkHeaderInfo) == 20 && sizeof(RwVideoMode) == 24);
static_assert(sizeof(RwSubSystemInfo) == RW_SUBSYSTEMNAME_MAXLEN);

// Enum values the shim passes through UNMAPPED to librw (the rest is translated by name in the function layer)
static_assert(rwCOMBINEREPLACE == rw::COMBINEREPLACE && rwCOMBINEPRECONCAT == rw::COMBINEPRECONCAT && rwCOMBINEPOSTCONCAT == rw::COMBINEPOSTCONCAT);
static_assert(rwMATRIXTYPEMASK == rw::Matrix::TYPEMASK && rwMATRIXTYPENORMAL == rw::Matrix::TYPENORMAL &&
              rwMATRIXTYPEORTHOGONAL == rw::Matrix::TYPEORTHOGONAL && rwMATRIXTYPEORTHONORMAL == rw::Matrix::TYPEORTHONORMAL);
static_assert(rwMATRIXINTERNALIDENTITY == rw::Matrix::IDENTITY);
static_assert(rwPRIMTYPELINELIST == rw::PRIMTYPELINELIST && rwPRIMTYPEPOLYLINE == rw::PRIMTYPEPOLYLINE && rwPRIMTYPETRILIST == rw::PRIMTYPETRILIST &&
              rwPRIMTYPETRISTRIP == rw::PRIMTYPETRISTRIP && rwPRIMTYPETRIFAN == rw::PRIMTYPETRIFAN && rwPRIMTYPEPOINTLIST == rw::PRIMTYPEPOINTLIST);
static_assert(rwCULLMODECULLNONE == rw::CULLNONE && rwCULLMODECULLBACK == rw::CULLBACK && rwCULLMODECULLFRONT == rw::CULLFRONT);
static_assert(rwBLENDZERO == rw::BLENDZERO && rwBLENDONE == rw::BLENDONE && rwBLENDSRCCOLOR == rw::BLENDSRCCOLOR && rwBLENDINVSRCCOLOR == rw::BLENDINVSRCCOLOR &&
              rwBLENDSRCALPHA == rw::BLENDSRCALPHA && rwBLENDINVSRCALPHA == rw::BLENDINVSRCALPHA && rwBLENDDESTALPHA == rw::BLENDDESTALPHA &&
              rwBLENDINVDESTALPHA == rw::BLENDINVDESTALPHA && rwBLENDDESTCOLOR == rw::BLENDDESTCOLOR && rwBLENDINVDESTCOLOR == rw::BLENDINVDESTCOLOR &&
              rwBLENDSRCALPHASAT == rw::BLENDSRCALPHASAT);
static_assert(rwSTENCILOPERATIONKEEP == rw::STENCILKEEP && rwSTENCILOPERATIONZERO == rw::STENCILZERO && rwSTENCILOPERATIONREPLACE == rw::STENCILREPLACE &&
              rwSTENCILOPERATIONINCRSAT == rw::STENCILINCSAT && rwSTENCILOPERATIONDECRSAT == rw::STENCILDECSAT && rwSTENCILOPERATIONINVERT == rw::STENCILINVERT &&
              rwSTENCILOPERATIONINCR == rw::STENCILINC && rwSTENCILOPERATIONDECR == rw::STENCILDEC);
static_assert(rwSTENCILFUNCTIONNEVER == rw::STENCILNEVER && rwSTENCILFUNCTIONLESS == rw::STENCILLESS && rwSTENCILFUNCTIONEQUAL == rw::STENCILEQUAL &&
              rwSTENCILFUNCTIONLESSEQUAL == rw::STENCILLESSEQUAL && rwSTENCILFUNCTIONGREATER == rw::STENCILGREATER &&
              rwSTENCILFUNCTIONNOTEQUAL == rw::STENCILNOTEQUAL && rwSTENCILFUNCTIONGREATEREQUAL == rw::STENCILGREATEREQUAL &&
              rwSTENCILFUNCTIONALWAYS == rw::STENCILALWAYS);
static_assert(rwTEXTUREADDRESSWRAP == rw::Texture::WRAP && rwTEXTUREADDRESSMIRROR == rw::Texture::MIRROR &&
              rwTEXTUREADDRESSCLAMP == rw::Texture::CLAMP && rwTEXTUREADDRESSBORDER == rw::Texture::BORDER);
static_assert(rwFILTERNEAREST == rw::Texture::NEAREST && rwFILTERLINEAR == rw::Texture::LINEAR && rwFILTERMIPNEAREST == rw::Texture::MIPNEAREST &&
              rwFILTERMIPLINEAR == rw::Texture::MIPLINEAR && rwFILTERLINEARMIPNEAREST == rw::Texture::LINEARMIPNEAREST &&
              rwFILTERLINEARMIPLINEAR == rw::Texture::LINEARMIPLINEAR);
static_assert(rpGEOMETRYTRISTRIP == rw::Geometry::TRISTRIP && rpGEOMETRYPOSITIONS == rw::Geometry::POSITIONS && rpGEOMETRYTEXTURED == rw::Geometry::TEXTURED &&
              rpGEOMETRYPRELIT == rw::Geometry::PRELIT && rpGEOMETRYNORMALS == rw::Geometry::NORMALS && rpGEOMETRYLIGHT == rw::Geometry::LIGHT &&
              rpGEOMETRYMODULATEMATERIALCOLOR == rw::Geometry::MODULATE && rpGEOMETRYTEXTURED2 == rw::Geometry::TEXTURED2);
static_assert(rpGEOMETRYLOCKPOLYGONS == rw::Geometry::LOCKPOLYGONS && rpGEOMETRYLOCKVERTICES == rw::Geometry::LOCKVERTICES &&
              rpGEOMETRYLOCKNORMALS == rw::Geometry::LOCKNORMALS && rpGEOMETRYLOCKPRELIGHT == rw::Geometry::LOCKPRELIGHT);
static_assert(rpATOMICRENDER == rw::Atomic::RENDER && rpATOMICCOLLISIONTEST == rw::Atomic::COLLISIONTEST);
static_assert(rpLIGHTDIRECTIONAL == rw::Light::DIRECTIONAL && rpLIGHTAMBIENT == rw::Light::AMBIENT && rpLIGHTPOINT == rw::Light::POINT &&
              rpLIGHTSPOT == rw::Light::SPOT && rpLIGHTSPOTSOFT == rw::Light::SOFTSPOT);
static_assert(rpLIGHTLIGHTATOMICS == rw::Light::LIGHTATOMICS && rpLIGHTLIGHTWORLD == rw::Light::LIGHTWORLD);
static_assert(rwCAMERACLEARIMAGE == rw::Camera::CLEARIMAGE && rwCAMERACLEARZ == rw::Camera::CLEARZ && rwCAMERACLEARSTENCIL == rw::Camera::CLEARSTENCIL);
static_assert(rwRASTERLOCKWRITE == rw::Raster::LOCKWRITE && rwRASTERLOCKREAD == rw::Raster::LOCKREAD && rwRASTERLOCKNOFETCH == rw::Raster::LOCKNOFETCH);
// chunk ids the game streams / registers plugins with
static_assert(rwID_CLUMP == rw::ID_CLUMP && rwID_TEXDICTIONARY == rw::ID_TEXDICTIONARY && rwID_UVANIMDICT == rw::ID_UVANIMDICT);

#include "rwaccessors.h"
#include "rwextra.h"
#include "rwapi.h"
