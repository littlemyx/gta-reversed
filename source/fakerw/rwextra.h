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
