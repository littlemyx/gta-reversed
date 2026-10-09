#pragma once
/*
    fakerw: game-visible RW structs/globals that librw has no counterpart for (P2B-00c). Included from fakerw.h before rwapi.h.
    Definitions of the globals declared here: source/standalone/rw/rwglobals.cpp.
*/

//--------------------------------------------------------------------------------------------------
// Engine globals: RwEngineInstance (the game reads stringFuncs / curCamera / renderFrame / the 2D z range), RwInitialized
//--------------------------------------------------------------------------------------------------
typedef int   (*vecSprintfFunc)(RwChar* buffer, const RwChar* format, ...);
typedef int   (*vecVsprintfFunc)(RwChar* buffer, const RwChar* format, va_list argptr);
typedef RwChar* (*vecStrcpyFunc)(RwChar* dest, const RwChar* srce);
typedef RwChar* (*vecStrncpyFunc)(RwChar* dest, const RwChar* srce, size_t size);
typedef RwChar* (*vecStrcatFunc)(RwChar* dest, const RwChar* srce);
typedef RwChar* (*vecStrncatFunc)(RwChar* dest, const RwChar* srce, size_t size);
typedef RwChar* (*vecStrrchrFunc)(const RwChar* string, int findThis);
typedef RwChar* (*vecStrchrFunc)(const RwChar* string, int findThis);
typedef RwChar* (*vecStrstrFunc)(const RwChar* string, const RwChar* findThis);
typedef int   (*vecStrcmpFunc)(const RwChar* string1, const RwChar* string2);
typedef int   (*vecStrncmpFunc)(const RwChar* string1, const RwChar* string2, size_t max_size);
typedef int   (*vecStricmpFunc)(const RwChar* string1, const RwChar* string2);
typedef size_t (*vecStrlenFunc)(const RwChar* string);
typedef RwChar* (*vecStruprFunc)(RwChar* string);
typedef RwChar* (*vecStrlwrFunc)(RwChar* string);
typedef RwChar* (*vecStrtokFunc)(RwChar* string, const RwChar* delimit);
typedef int   (*vecSscanfFunc)(const RwChar* buffer, const RwChar* format, ...);

struct RwStringFunctions {
    vecSprintfFunc  vecSprintf;
    vecVsprintfFunc vecVsprintf;
    vecStrcpyFunc   vecStrcpy;
    vecStrncpyFunc  vecStrncpy;
    vecStrcatFunc   vecStrcat;
    vecStrncatFunc  vecStrncat;
    vecStrrchrFunc  vecStrrchr;
    vecStrchrFunc   vecStrchr;
    vecStrstrFunc   vecStrstr;
    vecStrcmpFunc   vecStrcmp;
    vecStrncmpFunc  vecStrncmp;
    vecStricmpFunc  vecStricmp;
    vecStrlenFunc   vecStrlen;
    vecStruprFunc   vecStrupr;
    vecStrlwrFunc   vecStrlwr;
    vecStrtokFunc   vecStrtok;
    vecSscanfFunc   vecSscanf;
};

// RwDevice / RwGlobals: only the members the game reads. Filled by the shim (RwEngineInit / RwEngineStart / RwCameraBeginUpdate).
struct RwDevice {
    RwReal zBufferNear;     // RwIm2DGetNearScreenZ
    RwReal zBufferFar;      // RwIm2DGetFarScreenZ
};
struct RwGlobals {
    RwCamera*           curCamera;
    RwStringFunctions   stringFuncs;
    RwDevice            dOpenDevice;
    RwUInt16            renderFrame;    // CCustomCarEnvMapPipeline's per-frame env-map update guard
};
extern RwGlobals* RwEngineInstance;
extern bool       RwInitialized;
#define RWSRCGLOBAL(variable) (RwEngineInstance->variable)
#define rwsprintf RWSRCGLOBAL(stringFuncs).vecSprintf

//--------------------------------------------------------------------------------------------------
// Resource entries (RwResEntry = instanced data block in RW's resource arena). The game casts it to RwResEntrySA (RenderWare.h) in the
// 4 custom pipelines; the real pipelines on librw are batches 07/08, here the struct only has to exist.
//--------------------------------------------------------------------------------------------------
struct RwResEntry;
typedef void (*RwResEntryDestroyNotify)(RwResEntry* resEntry);
struct RwResEntry {
    RwLLLink                link;
    RwInt32                 size;
    void*                   owner;
    RwResEntry**            ownerRef;
    RwResEntryDestroyNotify destroyNotify;
};

//--------------------------------------------------------------------------------------------------
// RxPipeline node graph (emulated over rw::ObjPipeline by P2B-07): opaque here
//--------------------------------------------------------------------------------------------------
struct RxNodeDefinition {
    const RwChar* name;     // the game looks nodes up by RxNodeDefinitionGetD3D9AtomicAllInOne()->name
};
struct RxPipelineNode;
RwBool RxPipelineDestroy(RxPipeline* pipeline);
typedef RxPipeline RxLockedPipe;
typedef rw::d3d9::VertexStream RxD3D9VertexStream;
typedef RwBool (*RxD3D9AllInOneInstanceCallBack)(void* object, RxD3D9ResEntryHeader* resEntryHeader, RwBool reinstance);
typedef RwBool (*RxD3D9AllInOneReinstanceCallBack)(void* object, RwResEntry* resEntry, RxD3D9AllInOneInstanceCallBack instanceCallback);
typedef void   (*RxD3D9AllInOneRenderCallBack)(RwResEntry* repEntry, void* object, RwUInt8 type, RwUInt32 flags);

//--------------------------------------------------------------------------------------------------
// Dictionary schema (RtDict): the game only uses it for the UV-animation dictionary (Streaming.cpp)
//--------------------------------------------------------------------------------------------------
struct RtDictSchema { int dummy; };
typedef rw::UVAnimDictionary RtDict;
extern RtDictSchema RpUVAnimDictSchema;

//--------------------------------------------------------------------------------------------------
// MatFX environment-map data of a material: the game's building pipelines read it through MATFXD3D9ENVMAPGETDATA (a private RW macro that
// reached into RW's material plugin data). Layout = rw::MatFX::Env (static_assert below), implemented in P2B-05b.
//--------------------------------------------------------------------------------------------------
enum MatFXPass { rpSECONDPASS = 0, rpTHIRDPASS = 1, rpMAXPASS = 2 };
typedef enum MatFXPass MatFXPass;
struct MatFXEnvMapData {
    RwFrame*   frame;
    RwTexture* texture;
    RwReal     coef;
    RwBool     useFrameBufferAlpha;
};
static_assert(sizeof(MatFXEnvMapData) == sizeof(rw::MatFX::Env) && offsetof(MatFXEnvMapData, texture) == offsetof(rw::MatFX::Env, tex) &&
              offsetof(MatFXEnvMapData, coef) == offsetof(rw::MatFX::Env, coefficient));
MatFXEnvMapData* MatFXD3D9EnvMapGetData(RpMaterial* material, RwInt32 pass);
#define MATFXD3D9ENVMAPGETDATA(material, pass) MatFXD3D9EnvMapGetData((material), (pass))

//--------------------------------------------------------------------------------------------------
// D3D9-specific RW API the game's fixed-function pipelines use. These were macros over the exe's _RwD3DDevice + last-used-state cache;
// here real functions over librw's d3d state cache (P2B-02c / 07).
//--------------------------------------------------------------------------------------------------
void RwD3D9SetIndices(void* indexBuffer);
void RwD3D9SetVertexDeclaration(void* vertexDeclaration);
void RwD3D9SetVertexShader(void* shader);
void RwD3D9SetPixelShader(void* shader);
void RwD3D9DrawIndexedPrimitive(RwUInt32 primitiveType, RwInt32 baseVertexIndex, RwUInt32 minIndex, RwUInt32 numVertices, RwUInt32 startIndex, RwUInt32 primitiveCount);
void RwD3D9DrawPrimitive(RwUInt32 primitiveType, RwUInt32 startVertex, RwUInt32 primitiveCount);
inline IDirect3DDevice9* GetD3D9Device() { return rw::d3d::d3ddevice; }

// 02c: the rest of the RwD3D9 fixed-function/device state API (source/standalone/rw/rwd3d_ff.cpp). The game only needs the functions above plus
// the ones declared in rwapi.h; these are the matching Get* / helper entry points (same names and argument order as the original RW 3.6 D3D9 API).
void   RwD3D9GetTextureStageState(RwUInt32 stage, RwUInt32 type, void* value);   // *(RwUInt32*)value = state
void   RwD3D9GetTransform(RwUInt32 state, void* matrix);                          // D3DMATRIX*
void   RwD3D9GetLight(RwInt32 index, void* light);                                // D3DLIGHT9*
void   RwD3D9SetStreamSource(RwUInt32 streamNumber, void* streamData, RwUInt32 offset, RwUInt32 stride);
void   RwD3D9SetFVF(RwUInt32 fvf);

// Ambient light colour clamped to [0,1] (RW kept it in a global that RpWorldAddLight/RpLightSetColor refresh; read by the car pipeline)
extern RwRGBAReal AmbientSaturated;

//--------------------------------------------------------------------------------------------------
// Misc preprocessor API of rwplcore.h
//--------------------------------------------------------------------------------------------------
#define RWSTRING(x)             x
#define RWFUNCTION(name)        ((void)0)
#define RWRETURN(value)         return (value)
#define RWRETURNVOID()          return
#define RWRGBALONG(r, g, b, a)  ((RwUInt32)(((a) << 24) | ((r) << 16) | ((g) << 8) | (b)))
// sin/cos minimax coefficients (rwplcore.h), used by the game's re-implementation of RtQuatSlerp
#define _RW_C1  ((float) 4.1666667908e-02)
#define _RW_C2  ((float)-1.3888889225e-03)
#define _RW_C3  ((float) 2.4801587642e-05)
#define _RW_C4  ((float)-2.7557314297e-07)
#define _RW_C5  ((float) 2.0875723372e-09)
#define _RW_C6  ((float)-1.1359647598e-11)
#define _RW_S1  ((float)-1.6666667163e-01)
#define _RW_S2  ((float) 8.3333337680e-03)
#define _RW_S3  ((float)-1.9841270114e-04)
#define _RW_S4  ((float) 2.7557314297e-06)
#define _RW_S5  ((float)-2.5050759689e-08)
#define _RW_S6  ((float) 1.5896910177e-10)

// 01 ---------------------------------------------------------------------------------------------------------------------------------
// P2B-01 (math + streams): memory-stream descriptor and the RW file interface. Implemented in source/standalone/rw/stream.cpp.
//--------------------------------------------------------------------------------------------------
// RwStreamOpen/_rwStreamInitialize(rwSTREAMMEMORY, ...) take a pointer to this (== the game's tRwStreamInitializeData); RwStreamClose(stream, &mem)
// of a memory stream opened for writing fills it with the buffer (allocated through the engine, release with RwFree) and the number of bytes written.
struct RwMemory {
    RwUInt8* start;
    RwUInt32 length;
};
// RwOsGetFileInterface(): the CRT file functions all RW file access goes through (11 pointers, 0x2C bytes, same order as the SDK).
struct RwFileFunctions {
    RwBool  (*rwfexist)(const RwChar* path);
    void*   (*rwfopen)(const RwChar* path, const RwChar* mode);
    int     (*rwfclose)(void* fp);
    size_t  (*rwfread)(void* ptr, size_t size, size_t count, void* fp);
    size_t  (*rwfwrite)(const void* ptr, size_t size, size_t count, void* fp);
    RwChar* (*rwfgets)(RwChar* buf, int maxLen, void* fp);
    int     (*rwfputs)(const RwChar* str, void* fp);
    int     (*rwfeof)(void* fp);
    int     (*rwfseek)(void* fp, long offset, int origin);
    int     (*rwfflush)(void* fp);
    long    (*rwftell)(void* fp);
};
static_assert(sizeof(RwFileFunctions) == 0x2C);
RwFileFunctions* RwOsGetFileInterface();

//--------------------------------------------------------------------------------------------------
// 02b (standalone/rw/camera.cpp)
//--------------------------------------------------------------------------------------------------
// RwCamera::recipViewWindow (1 / viewWindow, kept in sync by RwCameraSetViewWindow in RW): not a librw field, derived on demand.
// Its only reader was WindowedMode.cpp (projection matrix of _rwD3D9CameraBeginUpdate), which the librw build does not compile.
inline RwV2d RwCameraGetRecipViewWindow(const RwCamera* camera) {
    RwV2d recip;
    recip.x = 1.0f / camera->viewWindow.x;
    recip.y = 1.0f / camera->viewWindow.y;
    return recip;
}

// 03a --------------------------------------------------------------------------------------------------------------------------------
// P2B-03a (raster + image, standalone/rw/{raster,image}.cpp): RW functions that are not in rwapi.h because the game calls them only through
// the stock RW headers (the generated rwapi.h lists what game code calls). Same signatures as RW 3.6.
//--------------------------------------------------------------------------------------------------
RwUInt8*  RwRasterLockPalette(RwRaster* raster, RwInt32 lockMode);
RwRaster* RwRasterGetOffset(RwRaster* raster, RwInt16* xOffset, RwInt16* yOffset);
RwInt32   RwRasterGetNumLevels(RwRaster* raster);
RwRaster* RwRasterSubRaster(RwRaster* subRaster, RwRaster* raster, RwRect* rect);
RwRaster* RwRasterGetCurrentContext(void);
RwRaster* RwRasterRender(RwRaster* raster, RwInt32 x, RwInt32 y);
RwRaster* RwRasterRenderScaled(RwRaster* raster, RwRect* rect);
RwBool    RwRasterClear(RwInt32 pixelValue);
RwBool    RwRasterClearRect(RwRect* rpRect, RwInt32 pixelValue);
RwRaster* RwRasterShowRaster(RwRaster* raster, void* dev, RwUInt32 flags);
RwRaster* RwRasterRead(const RwChar* filename);
RwRaster* RwRasterReadMaskedRaster(const RwChar* filename, const RwChar* maskname);
void      RwRasterSetFreeListCreateParams(RwInt32 blockSize, RwInt32 numBlocksToPrealloc);

RwImage*       RwImageFreePixels(RwImage* image);
RwImage*       RwImageCopy(RwImage* destImage, const RwImage* sourceImage);
RwImage*       RwImageResize(RwImage* image, RwInt32 width, RwInt32 height);
RwImage*       RwImageResample(RwImage* dstImage, const RwImage* srcImage);
RwImage*       RwImageCreateResample(const RwImage* srcImage, RwInt32 width, RwInt32 height);
RwImage*       RwImageApplyMask(RwImage* image, const RwImage* mask);
RwImage*       RwImageMakeMask(RwImage* image);
RwImage*       RwImageRead(const RwChar* imageName);
RwImage*       RwImageReadMaskedImage(const RwChar* imageName, const RwChar* maskname);
RwImage*       RwImageWrite(RwImage* image, const RwChar* imageName);
const RwChar*  RwImageSetPath(const RwChar* path);
RwChar*        RwImageGetPath(void);
RwImage*       RtBMPImageWrite(RwImage* image, const RwChar* imageName);
RwImage*       RtPNGImageRead(const RwChar* imageName);

// Not macros in RW 3.6 headers' function form; as in RW they only store the pointer / value (the image does not own what it is given).
inline RwImage* RwImageSetStride(RwImage* image, RwInt32 stride)    { image->stride = stride; return image; }
inline RwImage* RwImageSetPixels(RwImage* image, RwUInt8* pixels)   { image->pixels = pixels; return image; }
inline RwImage* RwImageSetPalette(RwImage* image, RwRGBA* palette)  { image->palette = reinterpret_cast<uint8_t*>(palette); return image; }

//--------------------------------------------------------------------------------------------------
// 04ab (standalone/rw/{light,world,material,geometry}.cpp): light / world / material / geometry API beyond rwapi.h, plus shim-only helpers.
// None of the extras is called by the game outside the stock RW headers (the game's own calls are declared in rwapi.h).
//--------------------------------------------------------------------------------------------------
RpLight* RpLightSetConeAngle(RpLight* light, RwReal angle);
RwReal RpLightGetConeAngle(const RpLight* light);
RpLight* RpLightStreamRead(RwStream* stream);
RwUInt32 RpLightStreamGetSize(const RpLight* light);

RpWorld* RpWorldAddAtomic(RpWorld* world, RpAtomic* atomic);
RpWorld* RpWorldRemoveAtomic(RpWorld* world, RpAtomic* atomic);
RpWorld* RpWorldAddClump(RpWorld* world, RpClump* clump);
RpWorld* RpWorldRemoveClump(RpWorld* world, RpClump* clump);
RpWorld* RpWorldForAllClumps(RpWorld* world, RpClumpCallBack callback, void* data);
RpWorld* RpWorldForAllLights(RpWorld* world, RpLightCallBack callback, void* data);

RpMaterial* RpMaterialClone(RpMaterial* material);
RpMaterial* RpMaterialStreamRead(RwStream* stream);
const RpMaterial* RpMaterialStreamWrite(const RpMaterial* material, RwStream* stream);
RwUInt32 RpMaterialStreamGetSize(const RpMaterial* material);
inline RpMaterial* RpMaterialAddRef(RpMaterial* material) { material->refCount++; return material; }

RwInt32 RpGeometryAddMorphTargets(RpGeometry* geometry, RwInt32 mtcount);
RwInt32 RpGeometryAddMorphTarget(RpGeometry* geometry);
const RpGeometry* RpGeometryForAllMeshes(const RpGeometry* geometry, RpMeshCallBack fpCallBack, void* data);
RpGeometry* RpGeometryStreamRead(RwStream* stream);
const RpGeometry* RpGeometryStreamWrite(const RpGeometry* geometry, RwStream* stream);
RwUInt32 RpGeometryStreamGetSize(const RpGeometry* geometry);
inline RpGeometry* RpGeometryAddRef(RpGeometry* geometry) { geometry->refCount++; return geometry; }
// the value last passed to RpD3D9GeometrySetUsageFlags (0 = never set); read by the custom pipelines' instance callbacks
RwUInt32 RpD3D9GeometryGetUsageFlags(const RpGeometry* geometry);
// 03b --------------------------------------------------------------------------------------------------------------------------------
// P2B-03b (texture + texdict, standalone/rw/{texture,texdict}.cpp): RW functions that are not in rwapi.h (the game reaches them only through the
// stock RW headers or not at all). Same signatures as RW 3.6.
//--------------------------------------------------------------------------------------------------
RwBool                  RwTextureGetMipmapping(void);
RwBool                  RwTextureGetAutoMipmapping(void);
RwTextureCallBackRead   RwTextureGetReadCallBack(void);
RwTexture*              RwTextureSetMaskName(RwTexture* texture, const RwChar* maskName);
RwTexture*              RwTextureStreamRead(RwStream* stream);
const RwTexture*        RwTextureStreamWrite(const RwTexture* texture, RwStream* stream);
RwUInt32                RwTextureStreamGetSize(const RwTexture* texture);
RwInt32                 RwTexDictionaryRegisterPluginStream(RwUInt32 pluginID, RwPluginDataChunkReadCallBack readCB, RwPluginDataChunkWriteCallBack writeCB, RwPluginDataChunkGetSizeCallBack getSizeCB);
RwTexDictionary*        RwTexDictionaryStreamRead(RwStream* stream);
RwUInt32                RwTexDictionaryStreamGetSize(const RwTexDictionary* texDict);

// RW's filterAddressing masks (rwcore.h / rwtexture): used by the RwTextureGet{FilterMode,AddressingU,AddressingV} accessor macros in rwaccessors.h, which
// were never given the constants. Layout: bits 0-7 filter, 8-11 U, 12-15 V (librw's Texture::getFilter / getAddressU / getAddressV).
#ifndef rwTEXTUREFILTERMODEMASK
#define rwTEXTUREFILTERMODEMASK     0x000000FF
#define rwTEXTUREADDRESSINGUMASK    0x00000F00
#define rwTEXTUREADDRESSINGVMASK    0x0000F000
#define rwTEXTUREADDRESSINGMASK     (rwTEXTUREADDRESSINGUMASK | rwTEXTUREADDRESSINGVMASK)
#endif


//--------------------------------------------------------------------------------------------------
// 04c (standalone/rw/{atomic,clump,plugins}.cpp): atomic / clump API beyond rwapi.h + plugin helpers. Only RwShimEnsureAtomicRenderSlot is
// shim-specific; the rest is stock RW API the game does not call outside the stock headers.
//--------------------------------------------------------------------------------------------------
// Registers the shim-private atomic render-callback plugin (idempotent per Engine::init cycle); needs numAllocated(Atomic) == 0 the first time.
void RwShimEnsureAtomicRenderSlot();
const RwSphere* RpAtomicGetWorldBoundingSphere(RpAtomic* atomic);

RpClump* RpClumpStreamWrite(RpClump* clump, RwStream* stream);
RwUInt32 RpClumpStreamGetSize(RpClump* clump);
RpClump* RpClumpAddLight(RpClump* clump, RpLight* light);
RpClump* RpClumpRemoveLight(RpClump* clump, RpLight* light);
RpClump* RpClumpForAllLights(RpClump* clump, RpLightCallBack callback, void* data);
RwInt32  RpClumpGetNumLights(RpClump* clump);
RpClump* RpClumpAddCamera(RpClump* clump, RwCamera* camera);
RpClump* RpClumpRemoveCamera(RpClump* clump, RwCamera* camera);
RpClump* RpClumpForAllCameras(RpClump* clump, RwCameraCallBack callback, void* data);
RwInt32  RpClumpGetNumCameras(RpClump* clump);

RwInt32 RpAtomicGetPluginOffset(RwUInt32 pluginID);
RwInt32 RpClumpGetPluginOffset(RwUInt32 pluginID);
RwInt32 RpGeometryGetPluginOffset(RwUInt32 pluginID);
RwInt32 RpMaterialGetPluginOffset(RwUInt32 pluginID);
RwInt32 RpAtomicSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB);
RwInt32 RpGeometrySetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB);
RwInt32 RpMaterialSetStreamAlwaysCallBack(RwUInt32 pluginID, RwPluginDataChunkAlwaysCallBack alwaysCB);
RwInt32 RpAtomicSetStreamRightsCallBack(RwUInt32 pluginID, RwPluginDataChunkRightsCallBack rightsCB);
RwInt32 RpMaterialSetStreamRightsCallBack(RwUInt32 pluginID, RwPluginDataChunkRightsCallBack rightsCB);
// 09 ---------------------------------------------------------------------------------------------------------------------------------
// P2B-09 (standalone/rw/platform.cpp): lost-device / Reset watch behind _rwD3D9DeviceSetRestoreCallback. engine.cpp calls the first two from
// RwEngineStart / RwEngineStop; the counter is for diagnostics and tests.
void     NotsaRwPlatform_OnEngineStarted();
void     NotsaRwPlatform_OnEngineStopping();
unsigned NotsaRwPlatform_DeviceResetCount();



//--------------------------------------------------------------------------------------------------
// 05a (standalone/rw/{skin,hanim}.cpp): skin / hanim API beyond rwapi.h. The game only calls what rwapi.h declares; these are the rest of the
// stock RW API the shim implements anyway (used by the unit test).
//--------------------------------------------------------------------------------------------------
RpHAnimHierarchy* RpHAnimHierarchyCreate(RwInt32 numNodes, RwUInt32* nodeFlags, RwInt32* nodeIDs, RpHAnimHierarchyFlag flags, RwInt32 maxInterpKeyFrameSize);
RpHAnimHierarchy* RpHAnimHierarchyDestroy(RpHAnimHierarchy* hierarchy);   // returns NULL like the exe
RpHAnimHierarchy* RpHAnimHierarchyAttach(RpHAnimHierarchy* hierarchy);
RpHAnimHierarchy* RpHAnimHierarchyDetach(RpHAnimHierarchy* hierarchy);
RpHAnimHierarchy* RpHAnimHierarchyAttachFrameIndex(RpHAnimHierarchy* hierarchy, RwInt32 nodeIndex);
RpHAnimHierarchy* RpHAnimHierarchyDetachFrameIndex(RpHAnimHierarchy* hierarchy, RwInt32 nodeIndex);
RwBool            RpHAnimFrameSetID(RwFrame* frame, RwInt32 id);
RwInt32           RpHAnimFrameGetID(RwFrame* frame);
RpSkinType        RpSkinAtomicGetType(RpAtomic* atomic);
