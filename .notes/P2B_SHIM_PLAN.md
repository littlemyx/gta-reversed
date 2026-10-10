# P2B: RenderWare -> librw shim plan (gta-reversed standalone, stream S3 / decision D4)

Date 2026-10-09, HEAD 40c7d1b6. Read-only analysis of `source/`; librw @18532c2 (`~/tools/librw`), re3 fakerw (`~/github/re-GTA/src/fakerw`) as design reference only.
Scratch scanners: `/private/tmp/claude-501/-Users-Andrei-Mukhin-github-recomp-gta-reversed/aa4942c3-1b0c-4e82-a9c0-0c095cb1fe07/scratchpad/p2b_shim/*.py` (counts below are reproducible from them).

## 0. Verdict and counts

* The shim is **feasible and mostly mechanical**: of the 245 distinct RW C functions the game touches, 137 are 1:1 librw (D), 71 need a small adapter (A), 27 must be written (W), 10 are stubs/dropped (S).
  Weighted by call sites (1658 sites to the 232 called ones): D 1239, A 356, W 56, S 7.  The hard part is NOT the function count: it is (1) the RxPipeline/RwD3D9 fixed-function
  custom pipelines (3 render callbacks, ~1.9k lines of game code), (2) SA-specific RW patches (`*GtaStreamRead*`, UV-anim dict, RtAnim scheme), (3) ~330 direct field accesses on RW structs.
* Count reconciliation: 868 wrapper bodies in `rw/*.cpp` (task said 864); 232 are called from game code (1658 sites; scan: `Name(`), +13 only used as function pointers
  (`PluginAttach` x7 in `app.cpp:140-155`, `RpHAnimKeyFrame{Blend,Add,MulRecip,StreamRead,StreamWrite,StreamGetSize}` in `RpAnimBlend.cpp:88-95`) = **245**. The other **623 wrappers are not needed:
  in standalone they are simply not provided (a stray reference becomes a link error, which is the desired trap).
* Header-defined API (macros/inline in `rw/*.h`, not wrappers): 124 more names, 2255 call sites (see 1b). `RwRenderStateSet` alone is 908 sites (macro -> `RwEngineInstance->dOpenDevice.fpRenderStateSet`).
* re3 fakerw already has the NAME for 165 of the 232 called functions (141 as real bodies, 16 of them >3 lines) and 102 of 124 header names: **it is a design template, not code to copy**
  (`~/github/re-GTA` has no LICENSE file; librw is MIT) -> write our own fakerw headers against the RW 3.6 names that the game uses.
* Struct layout: ~35 renamed/retyped fields on 16 types, ~330 direct access sites in ~20 files (section 2). Binary-layout dependence on RW that is NOT a field name: 7 spots (2b).
* D3D9: 48 distinct `RwD3D9*/_rwD3D9*/RxD3D9*/RpD3D9*` functions, 218 sites, **92% inside 5 files** (3 custom pipelines + PipelinesCommon.hpp = 162 sites = 74%; WindowedMode.cpp 38, dropped). 9 TSS types, 10 render states, 1 FF light.
* librw correction to LIBRW_NOTES: the D3D9 backend is shader-based for its OWN pipelines, but `rw::d3d::setRenderState/setTextureStageState/setSamplerState/setTexture/setMaterial/setVertexShader(nil ok)/
  setIndices/setStreamSource/setVertexDeclaration` + a full state cache and `lightingCB_Fix` (FF lights, d3drender.cpp:115) exist. Stencil states are implemented (d3ddevice.cpp:685; D24S8 depth). Fixed-function custom
  pipelines therefore CAN run on librw-d3d9 unchanged-in-spirit. UV animation (`uvanim.cpp`) is marked "TODO: implement fully" in librw -> must be written/verified.
* Plugins: every SA chunk (Node Name, 2dfx, Collision, Breakable, Env/Spec mat, Extra colours, Pipeline id, visibility, txd-parent, anim-blend) is registered by GAME code through
  `Rp*/Rw*RegisterPlugin`; offsets come from the return value (no hardcoded offsets) -> section 4: nothing to register in the shim except thunks that map the C API to librw's `registerPlugin`.

## 1. API surface (the work list)

Status key: D = direct (typedef/one-line forward to librw), A = adapter (signature/semantics differ, <=30 lines), W = must be written (no librw counterpart / emulation), S = stub or dropped (no-op in standalone).
Format: `Name(call sites)=S librw-equivalent`.  Module headers: fakerw files are named after the original RW headers (rwplcore.h, rwcore.h, rpworld.h, ...).

### 1a. Functions (wrappers in `source/game_sa/RenderWare/rw/*.cpp` -> bodies of `source/standalone/rw/*.cpp`)
**rwplcore.engine** (15: D12 A3)
RwEngineInit(1)=A Engine::init(MemoryFunctions*) +map RwMemoryFunctions ; RwEngineOpen(1)=A Engine::open(EngineOpenParams{HWND}) ; RwEngineStart(1)=D Engine::start ;
RwEngineStop(1)=D Engine::stop ; RwEngineClose(2)=D Engine::close ; RwEngineTerm(3)=D Engine::term ; RwEngineGetNumSubSystems(1)=D Engine::getNumSubSystems ;
RwEngineGetSubSystemInfo(1)=D Engine::getSubSystemInfo ; RwEngineGetCurrentSubSystem(6)=D ; RwEngineSetSubSystem(3)=D ; RwEngineGetNumVideoModes(7)=D ;
RwEngineGetVideoModeInfo(6)=D ; RwEngineGetCurrentVideoMode(5)=D ; RwEngineSetVideoMode(2)=D ; RwEngineGetVersion(5)=A return 0x36003 (rw::version)
**rwplcore.matrix/v3d** (16: D14 A2)
RwMatrixCreate(7)=D Matrix::create ; RwMatrixDestroy(8)=D destroy ; RwMatrixInvert(6)=D Matrix::invertOrthonormal/invert (check flags semantics) ; RwMatrixMultiply(13)=D
Matrix::mult ; RwMatrixRotate(28)=D Matrix::rotate(axis,angle,op) ; RwMatrixScale(13)=D Matrix::scale ; RwMatrixTransform(9)=D Matrix::transform ; RwMatrixTranslate(9)=D
Matrix::translate ; RwMatrixUpdate(32)=D Matrix::update ; RwV2dLength(1)=D length(V2d) ; RwV3dLength(4)=D length(V3d) ; RwV3dNormalize(11)=D normalize ; RwV3dTransformPoint(8)=A
V3d::transformPoints(out,in,1,mat) ; RwV3dTransformPoints(34)=D V3d::transformPoints ; RwV3dTransformVector(5)=A transformVectors n=1 ; RwV3dTransformVectors(3)=D
V3d::transformVectors
**rwplcore.stream** (8: D1 A6 W1)
RwStreamOpen(11)=A StreamFile::open / StreamMemory::open per type (re3 51 lines) ; RwStreamClose(24)=A Stream::close+delete (arg2 RwMemory* ignored) ; RwStreamRead(51)=A read8
(returns size or 0) ; RwStreamWrite(15)=A write8 ; RwStreamSkip(4)=A seek(n,1) ; RwStreamFindChunk(12)=D findChunk(stream,type,&len,&ver) ; RwStreamReadChunkHeaderInfo(1)=A
readChunkHeaderInfo->RwChunkHeaderInfo ; _rwStreamInitialize(3)=W game-side rewrite (Streaming.cpp x3: static rw::StreamMemory instead of StaticRef<RwStream>(0x8E48AC))
**rwcore.frame** (17: D13 A4)
RwFrameCreate(24)=D Frame::create ; RwFrameDestroy(18)=D destroy ; RwFrameAddChild(7)=D addChild ; RwFrameRemoveChild(1)=D removeChild ; RwFrameCount(1)=D count ;
RwFrameForAllChildren(21)=D forAllChildren ; RwFrameForAllObjects(36)=A iterate objectList (re3 6 lines) ; RwFrameGetLTM(34)=D getLTM ; RwFrameOrthoNormalize(4)=A matrix ortho-
normalize (check librw has ; RwFrameRotate(9)=D ; RwFrameTranslate(6)=D ; RwFrameTransform(4)=D ; RwFrameSetIdentity(6)=D matrix.setIdentity()+dirty ; RwFrameUpdateObjects(22)=D
updateObjects ; _rwFrameCloneAndLinkClones(1)=D cloneAndLink ; RwFrameRegisterPlugin(2)=A Frame::registerPlugin (thunk const-sig) ; RwFrameRegisterPluginStream(1)=A
Frame::registerPluginStream
**rwcore.camera** (12: D10 A1 W1)
RwCameraCreate(3)=D Camera::create ; RwCameraDestroy(3)=D ; RwCameraBeginUpdate(14)=D beginUpdate (also must upload D3DTS_VIEW/PROJECTION for FF pipelines: hook beginUpdateCB) ;
RwCameraEndUpdate(21)=D ; RwCameraClear(9)=A Camera::clear (+stencil clear value via RwD3D9SetStencilClear) ; RwCameraShowRaster(1)=D showRaster ; RwCameraFrustumTestSphere(2)=D
; RwCameraSetFarClipPlane(7)=D setFarPlane ; RwCameraSetNearClipPlane(39)=D ; RwCameraSetProjection(1)=D ; RwCameraSetViewWindow(2)=D (also fill recipViewWindow shim field) ;
RwD3D9CameraAttachWindow(2)=W reset swapchain to new HWND (Engine has no API)
**rwcore.raster** (10: D9 A1)
RwRasterCreate(18)=D Raster::create (flag/format enums equal) ; RwRasterDestroy(19)=D ; RwRasterLock(21)=D lock(level,mode) ; RwRasterUnlock(21)=D unlock(level) ;
RwRasterUnlockPalette(1)=D unlockPalette ; RwRasterPushContext(3)=D pushContext ; RwRasterPopContext(3)=D popContext ; RwRasterRenderFast(3)=D renderFast ;
RwRasterSetFromImage(1)=D setFromImage ; RwRGBAToPixel(1)=A pixel packing helper (write 10 lines)
**rwcore.image** (7: D3 A4)
RwImageCreate(2)=D Image::create ; RwImageDestroy(3)=D ; RwImageAllocatePixels(2)=D allocate ; RwImageFindRasterFormat(1)=A re3 4 lines ; RwImageSetFromRaster(1)=A
raster->toImage ; RtBMPImageRead(1)=A Image::registerFileFormat bmp (re3 17 lines) ; RtPNGImageWrite(1)=A png write via librw png.cpp (re3 16 lines)
**rwcore.texture** (10: D9 A1)
RwTextureCreate(5)=D Texture::create ; RwTextureDestroy(48)=D ; RwTextureRead(39)=D Texture::read ; RwTextureSetName(9)=A strncpy ; RwTextureSetRaster(2)=D ;
RwTextureSetMipmapping(2)=D ; RwTextureSetAutoMipmapping(1)=D ; RwTextureSetReadCallBack(1)=D Texture::readCB ; RwTextureSetFindCallBack(3)=D Texture::findCB ;
RwTextureGetFindCallBack(1)=D
**rwcore.texdict** (10: D7 A3)
RwTexDictionaryCreate(3)=D ; RwTexDictionaryDestroy(3)=D ; RwTexDictionaryAddTexture(7)=D add ; RwTexDictionaryRemoveTexture(2)=D remove ; RwTexDictionaryFindNamedTexture(15)=D
find ; RwTexDictionaryForAllTextures(4)=A iterate textures list ; RwTexDictionaryGetCurrent(4)=D getCurrent ; RwTexDictionarySetCurrent(5)=D setCurrent ;
RwTexDictionaryStreamWrite(1)=A streamWrite (re3 4 lines) ; RwTexDictionaryRegisterPlugin(1)=A TexDictionary::registerPlugin
**rwcore.im3d** (5: D4 A1)
RwIm3DTransform(32)=D im3d::Transform(verts,n,world,flags) (+vertex struct = librw Im3DVertex layout) ; RwIm3DRenderIndexedPrimitive(28)=D ; RwIm3DRenderPrimitive(4)=D ;
RwIm3DRenderLine(1)=A im3d::RenderLine (re3 7 lines) ; RwIm3DEnd(32)=D im3d::End
**rwcore.rx** (11: W11)
RxPipelineCreate(3)=W emulated RxPipeline handle (d3d9::ObjPipeline + saved cbs) ; RxPipelineLock(3)=W ; RxLockedPipeAddFragment(3)=W ; RxLockedPipeUnlock(3)=W ;
RxPipelineFindNodeByName(3)=W ; RxNodeDefinitionGetD3D9AtomicAllInOne(4)=W ; RxD3D9AllInOneSetInstanceCallBack(4)=W ; RxD3D9AllInOneGetInstanceCallBack(4)=W (returns
d3d9::defaultInstanceCB adapter) ; RxD3D9AllInOneSetReinstanceCallBack(4)=W ; RxD3D9AllInOneGetReinstanceCallBack(1)=W ; RxD3D9AllInOneSetRenderCallBack(3)=W
**rwcore.d3d9-state** (17: D4 A9 W3 S1)
RwD3D9SetRenderState(21)=D d3d::setRenderState ; RwD3D9GetRenderState(3)=D d3d::getRenderState ; RwD3D9SetTextureStageState(79)=D d3d::setTextureStageState (79 sites, args same
order) ; RwD3D9SetTexture(9)=A d3d::setTexture(stage,tex) swapped args ; RwD3D9SetTransform(5)=A d3ddevice->SetTransform via flushCache (VIEW/PROJ from Camera, WORLD per atomic,
TEXTUREn) ; RwD3D9SetLight(1)=A d3ddevice->SetLight (FF lights ; RwD3D9EnableLight(2)=A LightEnable ; RwD3D9SetMaterial(1)=A d3d::setMaterial ; RwD3D9SetSurfaceProperties(3)=A
setMaterial(color,surfProps,flags)+matsource states ; RwD3D9GetCurrentD3DDevice(5)=D d3d::d3ddevice ; RwD3D9GetCaps(3)=A cache D3DCAPS9 at Engine::start ;
RwD3D9SetStencilClear(1)=W store value used by Camera::clear ; RwD3D9DeviceSupportsDXTTexture(1)=A CheckDeviceFormat ; _rwD3D9SetStreams(3)=A d3d::setStreamSource x2 ;
_rwD3D9EnableClippingIfNeeded(3)=S no-op (D3D clips) ; _rwD3D9DeviceSetRestoreCallback(1)=W hook device Reset path ; _rwD3D9DeviceGetRestoreCallback(1)=W
**rwcore.d3d9-device** (6: D2 A1 W1 S2)
RwD3D9ChangeVideoMode(1)=W Engine::setVideoMode + device Reset ; RwD3D9ChangeMultiSamplingLevels(3)=A Engine::setMultiSamplingLevels ; RwD3D9EngineGetMaxMultiSamplingLevels(1)=D
Engine::getMaxMultiSamplingLevels ; RwD3D9EngineSetMultiSamplingLevels(1)=D ; RwD3D9EngineSetRefreshRate(1)=S no-op/store ; RwCoreInjectHooks(2)=S hook installer, dropped
**rpworld.light/world** (10: D8 A2)
RpLightCreate(4)=D Light::create ; RpLightDestroy(2)=D ; RpLightSetColor(34)=D setColor ; RpLightSetRadius(2)=A light->radius= ; RpWorldCreate(1)=A World::create(BBox*) ;
RpWorldDestroy(1)=D ; RpWorldAddCamera(1)=D ; RpWorldRemoveCamera(1)=D ; RpWorldAddLight(3)=D ; RpWorldRemoveLight(1)=D
**rpworld.atomic** (8: D4 A4)
RpAtomicCreate(2)=D Atomic::create ; RpAtomicDestroy(11)=D ; RpAtomicClone(15)=D clone ; RpAtomicSetFrame(23)=D setFrame ; RpAtomicSetGeometry(3)=A setGeometry(geo,flags=0) ;
RpAtomicRegisterPlugin(3)=A ; RpAtomicRegisterPluginStream(1)=A ; _rpAtomicResyncInterpolatedSphere(2)=A bounding sphere recompute (write 10 lines)
**rpworld.clump** (13: D7 A3 W3)
RpClumpCreate(3)=D ; RpClumpDestroy(13)=D ; RpClumpClone(1)=D Clump::clone ; RpClumpAddAtomic(11)=D ; RpClumpRemoveAtomic(5)=D ; RpClumpForAllAtomics(52)=A iterate atomics list
(52 sites) ; RpClumpRender(8)=D Clump::render ; RpClumpStreamRead(7)=D Clump::streamRead ; RpClumpRegisterPlugin(3)=A ; RpClumpRegisterPluginStream(1)=A ;
RpClumpGtaStreamRead1(1)=W full Clump::streamRead into pending slot ; RpClumpGtaStreamRead2(1)=W take pending + finalize ; RpClumpGtaCancelStream(1)=W drop pending
**rpworld.geometry** (12: D7 A4 S1)
RpGeometryCreate(3)=D Geometry::create ; RpGeometryDestroy(5)=D ; RpGeometryLock(7)=D lock ; RpGeometryUnlock(10)=D ; RpGeometryForAllMaterials(24)=A iterate matList (24 sites) ;
RpGeometryTriangleGetMaterial(1)=D ; RpGeometryTriangleSetMaterial(4)=A (re3 9 lines) ; RpGeometryTriangleSetVertexIndices(3)=D ; RpGeometryRegisterPlugin(3)=A ;
RpGeometryRegisterPluginStream(3)=A ; RpMorphTargetCalcBoundingSphere(2)=D calculateBoundingSphere ; RpD3D9GeometrySetUsageFlags(1)=S store flag, honored by instanceCB later
**rpworld.material** (7: D5 A2)
RpMaterialCreate(2)=D ; RpMaterialDestroy(3)=D ; RpMaterialSetTexture(13)=D ; RpMaterialRegisterPlugin(2)=A ; RpMaterialRegisterPluginStream(2)=A ;
_rpMaterialListAppendMaterial(1)=D MaterialList::appendMaterial ; _rpMaterialListDeinitialize(1)=D deinit
**rpworld.rx-d3d9** (0: )
**rpskin** (9: D5 A4)
RpSkinGeometryGetSkin(14)=D Skin::get ; RpSkinGeometrySetSkin(1)=D Skin::set ; RpSkinAtomicGetHAnimHierarchy(4)=D Skin::getHierarchy ; RpSkinAtomicSetHAnimHierarchy(3)=D
Skin::setHierarchy ; RpSkinAtomicSetType(1)=A Skin::setPipeline(atomic,type) ; RpSkinCreate(1)=A Skin::init+alloc (ClothesBuilder only) ; RpSkinGetNumBones(2)=D ->numBones ;
RpSkinGetSkinToBoneMatrices(3)=A (RwMatrix*)->inverseMatrices (float[16]) ; RpSkinGetVertexBoneIndices(3)=A ->indices (uint8 x4 per vert)
**rphanim** (13: D4 A9)
RpHAnimFrameSetHierarchy(1)=A HAnimData::get(frame)->hierarchy= ; RpHAnimHierarchyCreateFromHierarchy(1)=A HAnimHierarchy::create(n,flags,ids,flags,maxKey) from src ;
RpHAnimHierarchyGetMatrixArray(33)=D ->matrices ; RpHAnimHierarchyGetNodeMatrix(2)=A matrices[getIndex(id)] ; RpHAnimHierarchyUpdateMatrices(1)=D updateMatrices ;
RpHAnimIDGetIndex(36)=D getIndex(id) (36 sites) ; RpHAnimPluginAttach(0)=D registerHAnimPlugin (already in Engine::init -> S) ; RpHAnimKeyFrameBlend(0)=A hanim.cpp statics ->
expose/copy ; RpHAnimKeyFrameAdd(0)=A hanim.cpp statics -> expose/copy ; RpHAnimKeyFrameMulRecip(0)=A hanim.cpp statics -> expose/copy ; RpHAnimKeyFrameStreamRead(0)=A hanim.cpp
statics -> expose/copy ; RpHAnimKeyFrameStreamWrite(0)=A hanim.cpp statics -> expose/copy ; RpHAnimKeyFrameStreamGetSize(0)=A hanim.cpp statics -> expose/copy
**rpmatfx** (7: D4 A2 S1)
RpMatFXAtomicQueryEffects(1)=D MatFX::getEffects(atomic) ; RpMatFXMaterialGetEffects(6)=D MatFX::getEffects(mat) ; RpMatFXMaterialSetEffects(1)=D setEffects ;
RpMatFXMaterialGetEnvMapTexture(1)=D ; RpMatFXMaterialSetEnvMapCoefficient(2)=A re3 6 lines ; RpMatFXMaterialSetEnvMapFrame(1)=A re3 6 lines ; RpMatFXPluginAttach(0)=S
**rpuvanim** (4: W3 S1)
RpMaterialUVAnimExists(1)=W librw uvanim is 'TODO fully' ; RpMaterialUVAnimAddAnimTime(1)=W ; RpMaterialUVAnimApplyUpdate(1)=W ; RpUVAnimPluginAttach(0)=S (+ UVAnim interp
registration check)
**rtanim/dict/quat** (15: D5 A5 W1 S4)
RtAnimAnimationCreate(1)=D Animation::create ; RtAnimAnimationDestroy(1)=D ; RtAnimInterpolatorSetCurrentAnim(1)=D AnimInterpolator::setCurrentAnim ;
RtAnimRegisterInterpolationScheme(1)=A RtAnimInterpolatorInfo->AnimInterpolatorInfo::registerInterp (field/CB map) ; RtDictSchemaStreamReadDict(1)=A UVAnimDictionary::streamRead
; RtDictSchemaSetCurrentDict(1)=A currentUVAnimDictionary= ; RtDictDestroy(1)=A UVAnimDictionary::destroy ; RtQuatConvertFromMatrix(3)=D Quat::fromMatrix?(write if absent) ;
RtQuatRotate(14)=D rotate ; RtQuatTransformVectors(1)=A ; RtQuatSetupSlerpCache(2)=W librw has slerp() only, no cache ; RtAnimInitialize(0)=S ; RpAnisotPluginAttach(0)=S
registerAnisotropyPlugin ; RpWorldPluginAttach(0)=S no BSP ; RpSkinPluginAttach(0)=S
**rwtexdict(GTA)** (3: W3)
RwTexDictionaryGtaStreamRead(2)=W find chunk+TexDictionary::streamRead ; RwTexDictionaryGtaStreamRead1(1)=W read all, park ; RwTexDictionaryGtaStreamRead2(1)=W finish/setCurrent

Not-in-wrapper functions the game also calls (write as shim inlines, status W/A): `RwOsGetFileInterface` (5 sites FileMgr/preset_view: W, a struct of 11 CRT fn pointers: fexist,fopen,fclose,fread,fwrite,fgets,fputs,feof,fseek,fflush,ftell);
`RwInitialize/RwTerminate` are game functions (app_game.cpp), not RW; `RpAnimBlend*` (~50 names, 340 sites) and `RpGeometryGet2dFx*` are game code, ignore.

### 1b. Header-defined API (macros/inline in the 81 `rw/*.h`; 124 names, 2255 sites) -> inline functions in fakerw headers
* `RwRenderStateSet/Get` (908/22 sites) = **A**: the original macro dereferences `RwEngineInstance->dOpenDevice.fpRenderStateSet`. Replace by an inline switch `RwRenderState -> rw::SetRenderState/SetRenderStatePtr`
  (RW enum order differs from librw's; re3 `fake.cpp:451-543` is the shape). Used states (sites): DESTBLEND 116, VERTEXALPHAENABLE 114, SRCBLEND 114, TEXTURERASTER 110, ZTESTENABLE 108, ZWRITEENABLE 107, FOGENABLE 58, CULLMODE 52,
  TEXTUREFILTER 41, ALPHATESTFUNCTIONREF 35, TEXTUREADDRESS 27, SHADEMODE 23 (no-op), ALPHATESTFUNCTION 16, STENCIL* 33 (re3 left these unsupported; **SA needs them** for StencilShadows: librw has STENCIL* enums),
  TEXTUREPERSPECTIVE 5 (no-op), FOGTYPE 5, FOGCOLOR 4 (R/B swap), BORDERCOLOR 4, FOGDENSITY 1, TEXTUREADDRESSU/V 2.  `RWRSTATE(x)` stays `(void*)(x)`.
* Accessors (D, ~70 macros, 1500+ sites): `RwFrameGetMatrix 145` (&frame->matrix), `RpAtomicGetGeometry 64`, `RwTextureGetRaster 55`, `RwMatrixGetPos 48`, `RpClumpGetFrame 47`, `RpAtomicGetFrame 39`, `RwRasterGetWidth/Height 33/30`,
  `RwObjectGetType 30`, `RwCameraGetRaster 27`, `RwCameraGetNearClipPlane 26`, `RwCameraGetFrame 22`, `RwFrameGetParent 20`, `RpMaterialGetColor 18`, `RpMaterialGetTexture 10`, `RpGeometryGet{Flags,NumVertices,Material,MorphTarget,
  VertexTexCoords,Triangles,PreLightColors,NumMaterials,NumTriangles}`, `RpMorphTargetGetVertices/Normals/BoundingSphere`, `RwMatrixGet{At,Up,Right}`, `RwTextureSet{FilterMode,Addressing(UV)}`, `RwCameraGet/Set{Raster,ZRaster,Frame,ViewWindow}`,
  `RpLight{SetFlags,GetFlags,GetFrame}`, `RpAtomic{SetFlags,GetFlags,GetClump,GetBoundingSphere,Render}`, `RwV3d{Assign,Add,Sub,Scale,CrossProduct,DotProduct}`, `RwMatrix{SetIdentity,Copy}`, `RwTextureAddRef`, `RwRasterGet{Stride,Depth,Type,Format,Parent}`.
* Im2D/Im3D vertex API (A/D, ~330 sites): `RwIm2DVertexSet{ScreenX/Y/Z,CameraZ,RecipCameraZ,U,V,IntRGBA,RealRGBA}` (116), `RwIm3DVertexSet{Pos,U,V,RGBA}`+`Get{Pos}` (115), `RxObjSpace3DVertexSet{Pos,PreLitColor,U,V}` (64, missing in re3),
  `RwIm2DRender{Primitive 22,IndexedPrimitive 8,Line 3,Triangle 1}`, `RwIm2DGet{Near,Far}ScreenZ 20`. Decision: game-owned `RwIm2DVertex {x,y,z,rhw,emissiveColor,u,v}` (28 B, keeps the 33 `.emissiveColor` + 52 `.rhw` direct uses),
  converted (w = 1/rhw) into a scratch `rw::Im2DVertex[]` inside `RwIm2DRender*` (librw has `w` = camera Z, not rhw). `RwIm3DVertex`/`RxObjSpace3DVertex` = typedef of `rw::Im3DVertex` (36 B, same field order; `objVertex`->`position`: 20 direct uses).
* Misc: `RwD3D9{SetIndices,SetVertexDeclaration,SetPixelShader,SetVertexShader,DrawIndexedPrimitive,DrawPrimitive}` (9 sites, A -> `d3d::set*` + `d3ddevice->Draw*`), `RwDebug{SendMessage,SetTraceState,SetHandler}` (8, S),
  `RtQuat{ConvertToMatrix,UnitConvertToMatrix,Slerp}` (3, A), `RtCharset*` (6, D -> librw charset.cpp), `RwFree` (D), `_rpMaterialSetDefaultSurfaceProperties` (A), `RwV3dTransformPoint(s)/Vector(s)` (A).
* Types to alias (`typedef rw::X RwX`): Frame, Camera, Light, Clump, Atomic, Geometry, Material, Texture, TexDictionary, Raster, Image, World, Matrix, V2d, V3d, RGBA, RGBAf, Sphere, BBox, TexCoords, Plane, Rect, Stream,
  MorphTarget, Triangle, MeshHeader, Mesh, HAnimHierarchy, HAnimNodeInfo, HAnimKeyFrame, Skin, Animation, AnimInterpolator, SurfaceProperties, LLLink, LinkList, VideoMode(+RefRate/format adapter), SubSystemInfo.
  Enums to copy by VALUE after a static_assert pass against librw: rwFILTER*, rwTEXTUREADDRESS*, rwBLEND*, rwCULL*, rwRASTER{TYPE,FORMAT}*, rwRASTERLOCK*, rpGEOMETRY*, rpMATFX*, rwCAMERACLEAR*, rwSTREAM*, rpLIGHT*, rpATOMIC*, rwCOMBINE*.

## 2. Struct-layout strategy

Decision (re3 approach, confirmed by counts): **`typedef rw::T RwT` and rewrite game code that touches differently-named fields** through a handful of inline accessors in fakerw (`RwFrameGetModelling`, `RwRasterGetPixels` ...). Do NOT try to keep RW 3.6 binary layouts:
no `VALIDATE_SIZE`/`offsetof` of an RW type exists in game code except `preset_view.cpp:144` (`offsetof(RwCamera,nearPlane)==0x80`, drop for standalone) and `RwObjectNameIdAssocation.h` (game struct). Pointer-size (x86) is unchanged.
Direct field accesses were counted by `fcount2.py/floc.py` (name-based, +-10%); everything else goes through the macros of 1b.

| RW struct | direct uses (files) | librw field | verdict |
|---|---|---|---|
| RwV3d/V2d/RGBA/RGBAReal/Sphere/BBox/TexCoords/LLLink | 39 by-value members in game headers (2dEffect.h, Shadows.h, FxPrtMult.h, ...) | identical layout (V3d 12, RGBA 4, RGBAf 16, Sphere 16, BBox 24, TexCoords 8, LLLink 8) | same: typedef + `static_assert(sizeof)` |
| RwMatrix | 8 by-value members (CMatrix, BoneNode.h, FxSystem.h, ...); `.pos .right .up .at` ~140 uses; `memcpy(.., sizeof(RwMatrix))` x7 | `rw::Matrix` 64 B {right,flags,up,pad1,at,pad2,pos,pad3}; flags: TYPENORMAL.. = RW values | same |
| RwFrame | `modelling` 11 (ShadowCamera 5, Heli 3, Trailer, Vehicle, Automobile), `root` 1 | `matrix`, `ltm`, `child,next,root`, `object`, `objectList`; `inDirtyListLink`->`inDirtyList` | rename: accessor `RwFrameGetModelling()` |
| RwObject / RwObjectHasFrame | `rwObjectHasFrameSetFrame(&cam->object.object,f)` Game.cpp:613 + 52 macro sites | `Object{type,subType,flags,privateFlags,parent}`; `ObjectWithFrame{object,inFrame,syncCB}` | same names for `object.parent/type`; macro layer |
| RwRaster | `nOffsetX/Y` 8, `cType` 2, `width/height` 21 (WindowedMode.cpp, Game.cpp) | `offsetX/Y`, `type`, `flags`, `format`, `pixels`(cpPixels), `palette`, `parent`, + d3d ext via `GETD3DRASTEREXT` | rename (mostly WindowedMode.cpp which is dropped) |
| RwTexture | `raster 4`, `refCount 1`, `filterAddressing 1`, `lInDictionary` 0 | `raster,dict,inDict,name,mask,filterAddressing,refCount` | `lInDictionary`->`inDict` (RwTextureGetFilterMode etc. via macros) |
| RpMaterial | `texture 31`, `color 9`, `surfaceProps 8`, `pipeline 1` | `texture,color,surfaceProps,pipeline(Pipeline*),refCount(int32 vs RW int16)` | same except `pipeline` type (never assigned by game) |
| RpGeometry | `preLitLum 2`, `texCoords 6`, `morphTarget 2`, `numVertices 5`, `numTriangles 2`, `triangles 2`, `mesh 3`, `flags 2` (ClothesBuilder, BreakObject_c, CustomBuildingRenderer) | `colors`, `texCoords[8]`, `morphTargets`, `numVertices`, `numTriangles`, `triangles`, `meshHeader`, `flags`, `matList` | rename x6: accessors `RpGeometryGetPreLitLum` exist already (macro) -> convert the 22 raw uses |
| RpMorphTarget | `verts 30` (WaterLevel 15 = own vertex array?, ClothesBuilder 7, Collision 8 = own CColModel), `normals 2`, `boundingSphere 1` | `vertices`, `normals`, `boundingSphere` | rename ~10 real uses (ClothesBuilder, RealTimeShadow) |
| RpMeshHeader/RpMesh | `RpGeometryGetMesh` macro in `common.h:80` (uses `mesh->firstMeshOffset`), `numMeshes 4`, `indices`, `material 14` | `MeshHeader{flags,numMeshes,serialNum,totalIndices,pad}`+`getMeshes()`; `Mesh{indices,numIndices,material}` | **layout-dependent**: replace macro by `header->getMeshes()[i]` |
| RpAtomic | `geometry 4`, `boundingSphere 2`, `interpolator 2`, `pipeline 7`, `clump 1`, `renderCallBack` call (Automobile.cpp:7452) | `geometry,boundingSphere,worldBoundingSphere,clump,pipeline(ObjPipeline*),renderCB(void(*)(Atomic*)); NO interpolator` | `renderCB` type differs (returns void) -> trampoline plugin, see 2b; `interpolator` (VisibilityPlugins, RealTimeShadow): bounding-sphere only -> accessor |
| RpClump | none (all via macros) | `atomics,lights,cameras,world,inWorld` | iterate via shim `RpClumpForAllAtomics` |
| RwCamera | `frameBuffer 12`, `zBuffer 8`, `viewWindow 7`, `recipViewWindow 6`, `viewOffset 4`, `nearPlane 8`, `farPlane 12`, `fogPlane 2`, `frustumPlanes 4` (FxManager), `viewMatrix 1`, `projectionType 1` (WindowedMode 38, Renderer 9, RealTimeShadow, Game) | `frameBuffer,zBuffer,viewWindow,viewOffset,nearPlane,farPlane,fogPlane,projection,viewMatrix,frustumPlanes[6]{plane,closestX..}`; **no `recipViewWindow`** | accessors; `recipViewWindow` = shim-computed (1/viewWindow) |
| RpLight | `color/radius` via macros | `color(RGBAf),radius,minusCosAngle` | same |
| RwTexDictionary | none | `textures,inGlobalList` | via macros |
| RpHAnimHierarchy / NodeInfo | `pNodeInfo 4`, `currentAnim 4`, `numNodes 2`, `nodeID 2`, `pFrame` (RpAnimBlend.cpp, ClothesBuilder, ClumpModelInfo, RwHelper) | `matrices,matricesUnaligned,nodeInfo{id,index,flags,frame},numNodes,flags,interpolator(AnimInterpolator*)` | rename x5 + type change `RtAnimInterpolator`->`AnimInterpolator` |
| RpSkin | via 9 API fns; `RwMatrixWeights` cast of weights (ClumpModelInfo.cpp:196) | `Skin{numBones,inverseMatrices(float*),indices(uint8*),weights(float*)}` | A: 4-weights/vertex layout equal |
| RwImage | `RwImageGetPixels` macro 2; JPegCompress/WinPs index `sizeof(RwRGBA)*w` | `pixels,bpp,stride` | macro |
| RwGlobals/RwDevice | `RwEngineInstance->stringFuncs.{vecSscanf,vecSprintf,vecStrlen,vecStrcpy}` (preset_view.cpp, 5), `curCamera` 1, RWSRCGLOBAL 7 (all inside rw macros) | none | **W**: tiny emulated `RwGlobals{stringFuncs,curCamera}` or rewrite the 5 sites to `sscanf/sprintf/strlen/strcpy`; `dOpenDevice` function table NOT needed once 1b macros are replaced |
| RwIm2DVertex / RwIm3DVertex / RxObjSpace3DVertex | `.rhw 52`, `.emissiveColor 33`, `.objVertex 20`; arrays in Sprite2d(91 refs), Boat(50), WaterLevel(35), PostEffects(27), Clouds(26), Shadows(20) | Im2DVertex{x,y,z,w=camZ,color,u,v}; Im3DVertex{position,normal,color,u,v} | 2D: own struct + conversion (1b); 3D: typedef |
| RwStream | `gRwStream = StaticRef<RwStream>(0x8E48AC)` Streaming.cpp:13 + `_rwStreamInitialize(&gRwStream,..)` x3 | `rw::Stream` is an abstract class with vtable | **rewrite 3 sites** to a function-local `rw::StreamMemory`; `RwStreamRead(stream, buf, n)` ok |
| RwEngineOpenParams / RwVideoMode / RwSubSystemInfo | `{.displayID=param}` platform.cpp:255; VideoMode.cpp/h (`GsubSysInfo` etc. StaticRefs) | `EngineOpenParams{HWND window}`, `VideoMode{width,height,depth,flags}` (no refRate/format) | A + port VideoMode.cpp |

### 2b. Places that depend on RW 3.6 binary behaviour (beyond field names)
1. `RpGeometryGetMesh` (common.h:80): pointer arithmetic over `RpMeshHeader.firstMeshOffset`. 2. `RWPLUGINOFFSET(T,obj,off)` = `obj + off` -- identical in librw (offset relative to object start); offsets are runtime values from `*RegisterPlugin` (OK).
3. `MATFXD3D9ENVMAPGETDATA(material,pass)` (rpmatfx.h:138, used CustomBuilding*Pipeline x3) reads RW's private `rpMatFXMaterialData` through `StaticRef<RwInt32> MatFXMaterialDataOffset = 0xC9AB74` -> replace by `MatFX::get(mat)->fx[pass].env.tex`.
4. `RpAtomic->renderCallBack` returns `RpAtomic*`; librw `renderCB` returns void; 10 sites (VisibilityPlugins:1081-1085, ShadowCamera:214-218 sets NULL to mean default, VehicleModelInfo:644, ClumpModelInfo:233, Automobile:7452).
   Plan: shim atomic plugin slot storing the game callback + trampoline installed into `rw::Atomic::renderCB`; `Get` returns the stored pointer; `NULL` = `Atomic::defaultRenderCB`.
5. `RxD3D9ResEntryHeader/InstanceData` (`RwResEntry+1` casts in the 3 pipelines; `RenderWare.h:56 RwResEntrySA`) -> `d3d9::InstanceDataHeader/InstanceData` (names of the fields we use are the same: vertexStream[], useOffsets, vertexDeclaration, indexBuffer, primType, numMeshes, inst[] vs `(header+1)` array of meshes!). Note the mesh array: RW appends meshes after the header, librw uses `header->inst` pointer.
6. RW statics read by name from the exe data: `_RwD3DDevice, _rwD3D9Last*Used (RwD3D9GetCaps cache), RpUVAnimDictSchema (RenderWare.h:26), geometryTKList, AmbientSaturated, GetD3DViewTransform()/GetD3DProjTransform() (0xC9BC80/0x8E2458)`, `_RwD3D9AdapterInformation, _RwD3D9DeviceCaps, StencilClearValue` (WindowedMode.cpp): all must be replaced by shim members (see 3). `RenderWare.h:22-27` becomes `#ifndef NOTSA_STANDALONE_RUN`.
7. Win32 window handling hacks (`WindowedMode.cpp`, 38 D3D9 sites, hooks `RwD3D9CameraAttachWindow` etc.): not ported; librw `Engine::setVideoMode` + `d3d9Globals.present.Windowed` (d3ddevice.cpp:1608) owns the swap chain -> W `RwD3D9ChangeVideoMode/CameraAttachWindow` on top of it.

## 3. D3D9 surface

Inventory (`d3d.py`, outside rw/): 48 functions, 218 sites. Per file: CustomCarEnvMapPipeline.cpp 66, CustomBuildingDNPipeline.cpp 41, WindowedMode.cpp 38, CustomBuildingPipeline.cpp 36, PipelinesCommon.hpp 19, rest 18
(WinPs 4, Gamma 3, MenuManager_Process 3, Game 2, WndProc/VideoMode/WinPlatform/WinMain/StencilShadows/MenuManager 1 each).
* `RwD3D9SetTextureStageState` 79 sites: only COLOROP 22, ALPHAOP 12, COLORARG2 9, ALPHAARG2 9, TEXCOORDINDEX 7 (incl. `D3DTSS_TCI_CAMERASPACENORMAL`), TEXTURETRANSFORMFLAGS 7 (`D3DTTFF_PROJECTED|COUNT3`), COLORARG1 5, ALPHAARG1 5, COLORARG0 3 (MULTIPLYADD). -> `rw::d3d::setTextureStageState(stage,type,value)` same arg order (D).
* `RwD3D9SetRenderState` 21 + Get 3: TEXTUREFACTOR 7, SPECULARENABLE 2, AMBIENT 2, AMBIENTMATERIALSOURCE 2, COLORVERTEX 2, EMISSIVEMATERIALSOURCE 2, SPECULARMATERIALSOURCE 1, LOCALVIEWER 1, FOGSTART/END 1+1, LIGHTING (Get) -> `d3d::setRenderState/getRenderState` (cached; flushed by `d3d::flushCache` before draws, which `drawInst` does).
* `RwD3D9SetTexture` 9 (A: arg order), `SetTransform` 5 (VIEW/PROJ in WindowedMode; TEXTURE1 in CarEnvMap: via device), `SetLight/EnableLight` (3: one specular directional light `g_GameLight` at slot 1), `SetMaterial`, `SetSurfaceProperties` (3),
  `GetCurrentD3DDevice` 5 (Gamma ramp, WinPs screenshot, WndProc, WinPlatform), `GetCaps` 3, `_rwD3D9SetStreams` 3, `RwD3D9SetIndices/VertexDeclaration` 3; raw `IDirect3DDevice9->` calls: 63, of which 56 are `imgui_impl_dx9.cpp` (take the device from `rw::d3d::d3ddevice`, invalidate librw's cache after imgui draws) and 7 in WinPs/D3DResourceSystem/WindowedMode/WinPlatform.
* `D3DResourceSystem` (421 lines + D3DTextureBuffer/D3DIndexDataBuffer): a pooling cache that the exe's RW calls when creating D3D textures/index buffers (hooked inside RW; game callers: Game.cpp x7, MenuManager_Process x2). librw's `d3d::createTexture` never calls it -> **S**:
  keep the API as no-ops in standalone (its StaticRef arrays live in the data image; harmless), revisit as an optimisation (pool in a librw patch) only if profiling asks.
* The 4 custom pipelines (game files, 1.9k lines): 

| pipeline | files (lines) | RW surface it uses | rewrite shape on librw |
|---|---|---|---|
| Building (non-DN) `CCustomBuildingPipeline` | CustomBuildingPipeline.cpp (177) | `RxPipelineCreate/Lock/AddFragment/FindNodeByName`, `RxD3D9AllInOneSet{Instance,Reinstance,Render}CallBack`, FF TSS (stage 0/1 env add via MULTIPLYADD + CAMERASPACENORMAL) | `d3d9::ObjPipeline::create()` with `instanceCB=defaultInstanceCB`, `uninstanceCB=defaultUninstanceCB`, `renderCB=ours` (port of `CustomPipeRenderCB`: `setStreamSource/setIndices/setVertexDeclaration`, per-mesh FF states, `setVertexShader(nil); setPixelShader(nil)`, `drawInst(header,inst)`); `RxPipeline` shim type wraps it |
| Building DN `CCustomBuildingDNPipeline` | CustomBuildingDNPipeline.cpp (418), Renderer (96) | same + `ExtraVertColour` geometry plugin (day/night colours, streamed 0x253F2F9), `PreRenderUpdate` writes blended colours into the vertex buffer (`header->vertexStream[0].vertexBuffer` Lock) | `instanceCB` = call `defaultInstanceCB` then patch colour stream; reinstance on DN balance change = `Geometry::lockedSinceInst`/`instData` re-instance via `d3d9::ObjPipeline` `instanceCB(reinstance=true)`; RW's `RxD3D9AllInOneGetInstanceCallBack` (returns default) = `&defaultInstanceCB` adapter |
| Car env-map + specular `CCustomCarEnvMapPipeline` (+EnvMat/SpecMat/EnvAtm plugins) | CustomCarEnvMapPipeline.cpp (533) + 3 plugin files (362) | FF specular light (`SetLight(1,&g_GameLight)`), `D3DTSS_TCI_CAMERASPACENORMAL` + projective `D3DTTFF`, `SetTransform(TEXTURE1)`, `D3DRS_*MATERIALSOURCE`, `RwD3D9SetSurfaceProperties` | stage A (D3D9): verbatim port on `rw::d3d::*` + `lightingCB_Fix`-style light upload + camera VIEW/PROJ/WORLD uploaded as D3D transforms (librw uploads only shader constants: add in a `Camera::beginUpdate`/pre-draw hook). stage B (GL3/Vulkan later): re3 `custompipes_d3d9.cpp`/`.gl` neoVehicle/gloss model: HLSL/GLSL + `uploadMatrices/uploadLights/setVertexShader`, shaders embedded as `.inc` |
| PipelinesCommon.hpp helpers | 43 | `RxD3D9InstanceDataRender(Lighting)`, `SetTextureStagesForLighting` | `drawInst`-based; trivial |

  Recommendation: **stage A = keep the fixed-function logic** (exact look, ~1 batch per pipeline, runs on Wine's wined3d FF emulation which re3-d3d8 style paths already use) and only introduce shaders when a non-D3D9 backend is added; the FF-specific API is confined to the 4 files + `rwd3d_ff.cpp` in the shim.
* librw `d3d9` pipelines for stock things (default/skin/matfx): `makeDefaultPipeline`, `makeSkinPipeline`, `makeMatFXPipeline` are registered by `Engine::init`; SA's MatFX env maps, UV transforms, skinned peds/vehicles use them (shader lighting; FF vs shader lighting parity is a visual risk, not an API one).
  The building DN and car pipelines coexist with stock ones through the `pipeline` pointer of the Atomic (`atomic->pipeline = ObjPipeline` as in `CustomPipeAtomicSetup`, kept: `rw::Atomic::pipeline` exists) -- verify librw `Atomic::getPipeline()` honours a non-null user pipeline (geometry.cpp/clump.cpp), else patch librw (2 lines).

## 4. Plugins (item 4): nothing is "pre-registered" by the shim except librw's own

librw's `Engine::init` registers (see rwplugins.h, probe in LIBRW_BUILD.md): Skin 0x116, HAnim 0x11E, MatFX 0x120, Mesh 0x50E, NativeData 0x510, Rights 0x1F, UserData, UVAnim, native raster/texture. `PluginAttach()`
(app.cpp:132) then runs the game's attach list; every game attach calls the C registration API, so the shim provides **13 thunks** (`RwFrame/RpAtomic/RpClump/RpGeometry/RpMaterial/RwTexDictionary Register{Plugin,PluginStream}`, status A) mapping onto `rw::X::registerPlugin(size,id,ctor,dtor,copy)` and `registerPluginStream(id,read,write,getSize)`.
Signature gaps (all trivial): RW `copy(void* dst,const void* src,off,size)` vs librw `copy(void*,void*,..)`; RW write/getSize take `const void* obj`; return of `registerPlugin` = offset or -1 (same). Thunk by cast (x86 cdecl, same ABI) or small generic adapters.

| game plugin (registered by) | object | chunk id (0x253F2xx) | size | notes |
|---|---|---|---|---|
| CVisibilityPlugins atomic/clump/frame (VisibilityPlugins.cpp:239-258) | RpAtomic / RpClump / RwFrame | 00 / 01 / 02 | sizeof(tXVisibilityPlugin) | no stream; offsets `ms_*PluginOffset` runtime |
| TxdStore parent (TxdStore_Plugin.cpp:11) | RwTexDictionary | F5 | 4 | `RwTexDictionaryRegisterPlugin` = **A**: librw has `TexDictionary::registerPlugin`; no stream |
| RpAnimBlend (RpAnimBlend.cpp:70) | RpClump | FB | 4 | + `RtAnimRegisterInterpolationScheme` (fn ptr table `RtAnimBlendKeyFrameApply` etc.) -> `AnimInterpolatorInfo::registerInterp` (id=rwID_RPANIMBLENDPLUGIN) |
| Node Name (NodeName.cpp:76) | RwFrame | FE | 24 | stream: raw name bytes |
| Pipeline id (PipelinePlugin.cpp:64) | RpAtomic | F3 | 4 | stream read/write pipelineId |
| Env map material / atomic (CustomCarEnvMapPipeline.cpp:56,69) | RpMaterial / RpAtomic | FC / F4 | 4 (ptr) | stream material only |
| Specular map material (…:75) | RpMaterial | F6 | 4 (ptr) | stream |
| Extra vert colours (CustomBuildingDNPipeline.cpp:12) | RpGeometry | F9 | sizeof(ExtraVertColour) | night colours; stream |
| 2d Effect (2dEffect.cpp:103) | RpGeometry | F8 | sizeof(t2dEffectPlugin) | stream = raw 2dfx entries |
| Breakable (BreakablePlugin.cpp:19) | RpGeometry | FD | sizeof(BreakablePlugin) | stream |
| Collision (CollisionPlugin.cpp:82) | RpClump | FA | **0** | stream only (COL3 blob); size 0 should be legal in librw (offset == end; verify in `PluginList::registerPlugin`) |
| shim-owned: atomic render-callback slot (2b.4) | RpAtomic | none (not streamed) | 4 | NEW, vendor id unused |
| shim-owned: camera `recipViewWindow` shadow (optional) | RwCamera | none | 8 | or compute in accessor |

Order/timing: RW forbids registration after `RwEngineOpen`; `RsRwInitialize` (platform.cpp:243) does Init -> PluginAttach (event) -> Open -> Start: preserved. `Engine::init` MUST run before the first `Register*` thunk; the shim's `RwEngineInit` does it.
No ID collisions: librw uses core ids (0x116,0x11E,0x120,0x135,0x1F,...), SA uses vendor 0x0253F2xx. The probe (LIBRW_BUILD.md) confirmed that, once these are registered, librw stops dropping the 0x253F2F8/F9/FA/FC/F6/FD/FE chunks (they are skipped only when no plugin claims them).
Also needed from librw at engine level: `rw::registerHAnimPlugin/MatFX/Skin/UVAnim/Anisotropy` run inside `Engine::init`; the game's `RpWorldPluginAttach/RpSkinPluginAttach/RpHAnimPluginAttach/RpMatFXPluginAttach/RpUVAnimPluginAttach/RpAnisotPluginAttach/RtAnimInitialize` therefore become S (return true),
except UVAnim/RtAnim-scheme verification (librw registers UV anim interpolators 0x1C0/0x1C1 -- confirm the ids SA uses, and that `UVAnimDictionary::streamRead` matches the chunk 0x2B read by `RtDictSchemaStreamReadDict`, `Streaming.cpp:470-483`).
SA-specific RW patches to emulate (W): `RwTexDictionaryGtaStreamRead{,1,2}` (TxdStore.cpp:80-103; FileLoader:114), `RpClumpGtaStreamRead1/2/CancelStream` (FileLoader.cpp:351-367, Streaming.cpp:2453): original splits loading into a read half (streaming thread) and a GPU-upload half
(main thread). Shim = Read1 reads everything with `Clump::streamRead`/`TexDictionary::streamRead` into a pending slot (the memory stream buffer is released after Read1), Read2 returns/finalises it (instances geometry, sets current TXD), Cancel frees the slot.
`TxdStoreFindCB/LoadCB` (RwTextureSet{Find,Read}CallBack, same signature as librw `Texture::findCB/readCB`) and VehicleModelInfo's temporary FindTextureCB swap work unchanged.

## 5. Build integration

* librw: **vendor as git submodule `vendor/librw`** pinned to 18532c2 (re3 also pins; `add_subdirectory(vendor/librw EXCLUDE_FROM_ALL)`), options set BEFORE it: `LIBRW_PLATFORM=D3D9, LIBRW_TOOLS=OFF, LIBRW_INSTALL=OFF, LIBRW_EXAMPLES=OFF`. Consumers must see `RW_D3D9` (PUBLIC on target `librw`) and
  `WITH_D3D`; include `<d3d9.h>` BEFORE `rw.h` (rwd3d.h guards on `_D3D9_H_`). librw sets `librw_MAINPROJECT=OFF` when a parent project exists. Local ~/tools/librw stays as the scratch clone.
* Toolchain facts: top-level sets `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded[Debug]` and C++23 -> inherited by librw (static CRT; consistent with D3 "our CRT"). librw's `-W` warnings: build the `librw` target without `COMPILE_WARNING_AS_ERROR` (set only on `gta_reversed`); verified clean build in LIBRW_BUILD.md (80 steps, 21 s).
* Switch: `NOTSA_STANDALONE` is set also for the CI hook-dump build, which must keep real RW headers. Use **`NOTSA_STANDALONE_RUN`** (already defined for STANDALONE && !DUMP_HOOKS_ONLY, source/CMakeLists.txt:26-28) -> new CMake option `GTASA_STANDALONE_RW` (default ON in that mode) defining `NOTSA_RW_LIBRW`.
  When on: (a) `target_include_directories(... BEFORE source/standalone/rw)` ahead of `game_sa/RenderWare/rw/`; fakerw headers use the SAME file names (rwcore.h, rwplcore.h, rpworld.h, rpskin.h, rphanim.h, rpmatfx.h, rpuvanim.h, rtanim.h, rtdict.h, rtquat.h, rtbmp.h, rtpng.h, rtslerp.h,
  rpanisot.h, rpdbgerr.h, rwtexdict.h, rperror.h, rpcriter.h + empty stubs for the other 60 headers StdInc/others include); (b) `StdInc.h:42-61` and `RenderWare/RenderWare.h` get an `#ifdef NOTSA_RW_LIBRW` branch (paths `RenderWare/rw/...` are relative, so the include-dir trick alone is not enough);
  (c) `list(REMOVE_ITEM SOURCE_FILES_WITHOUT_MODULES_LIST game_sa/RenderWare/rw/*.cpp D3DResourceSystem...)` -- the GLOB at CMakeLists.txt:60-66 picks up everything under `source/`, new `source/standalone/rw/*.cpp` are added automatically; (d) link `librw` + `d3d9 xinput`.
  ASI and dump-hook builds are untouched; CI must build both so a game-code edit cannot break the other header set (add a standalone compile-only job after P2B-00).
* Header hygiene: librw leaks macros/typedefs (`nil`, `nelem`, `PLUGINOFFSET`, `PLUGINBASE`, `RWALIGN`, `LLLinkGetData`, `FORLIST`, `rwMalloc...`, `M_PI` guarded). Game code does not use `nil/nelem`. Keep librw includes inside the fakerw headers only, never `using namespace rw;` in a header
  (`rw::byte/uint/int32` would clash with game `byte`, `int32`). fakerw `.cpp` files that need `rw.h` internals go in `source/standalone/rw/` and are excluded from the unity build.
* Unity build (`GTASA_UNITY_BUILD`, batch 16 + PCH `StdInc.cpp`): PCH will transitively include `rw.h` -> every TU pays librw's parse cost and sees its macros; fakerw .cpp files get `SKIP_UNITY_BUILD_INCLUSION ON` (they `#undef` nothing but define `extern "C"`-free C++ functions with RW names; duplicates of static helpers across a unity batch would collide).
* `/BASE:0x10000000 /FIXED` (D1): librw is statically linked, no relocation concerns; its allocations go through `rw::Engine::memfuncs` = `RwEngineInit(memFuncs)` -> our CRT (D3), same as re3's mallocWrap.

## 6. Batch plan (ordered; ids P2B-nn; a batch = one agent, port + review pass per PORT_BRIEF)

Estimate: **~36 implementation batches + ~9 review/test batches** (D4's 100-120 in PHASE2_PLAN assumed per-function batches; with 137 of 245 functions being one-liners and the 4 pipeline files being the real work, ~45 is realistic). Gate after each batch: standalone target compiles (-Werror) + `ctest` shim unit tests (valid case + one mutation).

| id | content | funcs (D/A/W) | depends | parallel lane |
|---|---|---|---|---|
| 00a | submodule vendor/librw, CMake wiring (`GTASA_STANDALONE_RW`, include order, REMOVE_ITEM of rw/*.cpp), link smoke: `Engine::init/open(nil)/start` + `Clump::streamRead` of an SA DFF in a 20-line test exe | build only | - | serial |
| 00b | type layer headers: rwplcore.h/rwcore.h typedef/enum/static_assert layer (section 1b types+enums, RwV3d..RwMatrix inlines), `RenderWare.h` branch | ~60 types, 40 inline | 00a | serial |
| 00c | rpworld.h/rpskin.h/rphanim.h/rpmatfx.h/rpuvanim.h/rtanim.h/rtdict.h/rtquat.h stub-complete + accessor macros (70 D) + `RpGeometryGetMesh`/`MATFX..` replacements; **gate: all ~1000 TUs compile against fakerw (link not required)** | ~115 macro names | 00b | serial |
| 00d | call-site rewrites of direct field uses (2): frame/camera/raster/morph/hanim accessors; ~330 sites in ~20 files (ClothesBuilder, RpAnimBlend, ShadowCamera, Heli/Trailer/Vehicle/Automobile, Renderer, FxManager, RealTimeShadow, CustomBuildingRenderer, preset_view, Streaming `gRwStream`) -- split in 3 file groups | - | 00c | 3 parallel (disjoint files) |
| 01 | math + streams: RwMatrix* (8), RwV2d/V3d* (8), RwStream* (8), RwOsGetFileInterface | 14D 3A 2W | 00c | L1 |
| 02a | engine: RwEngine* (15), RwGlobals-less init glue, `RwEngineOpenParams`, VideoMode/SubSystem adapter, port `VideoMode.cpp`, `RwD3D9{ChangeVideoMode,MultiSampling*,Refresh,DXTSupport,GetCaps}` | 12D 4A 3W | 00c | L2 |
| 02b | frame (17) + camera (14) incl. `recipViewWindow`, view/proj upload hook, stencil clear | 22D 5A 1W | 00c, 02a for camera clear | L3 |
| 02c | render-state mapper (`RwRenderStateSet/Get` 930 sites incl. stencil/fog colour swap), `RwD3D9Set/GetRenderState`, `TextureStageState`, `SetTexture`, `SetTransform`, light/material (FF state API, 'rwd3d_ff.cpp') | 4D 8A 1W | 00c | L4 |
| 03a | raster (10) + image (6) + RtBMP/RtPNG | 12D 5A | 00c | L5 |
| 03b | texture (10) + texdict (10) + find/read callbacks | 15D 5A | 03a | L5 |
| 03c | TXD streaming `RwTexDictionaryGtaStreamRead*` + CTxdStore integration test | 3W | 03b, 01 | L5 |
| 04a | light (4) + world (6) | 8D 2A | 00c | L1 |
| 04b | material (6) + geometry (13) + MaterialList + MorphTarget | 12D 5A 1S | 00c | L2 |
| 04c | atomic (8) + clump (10) + 13 plugin-registration thunks + atomic render-callback trampoline plugin | 11D 12A | 04b | L3 |
| 04d | `RpClumpGtaStreamRead1/2/Cancel` + FileLoader/Streaming tests with SA DFFs (infernus, male01, vgsnbuild07) | 3W | 04c, 03c | L3 |
| 05a | skin (9) + hanim (6 + 6 key-frame callbacks) | 9D 8A | 04c | L4 |
| 05b | matfx (6) + uvanim (3, W) + RtDict/UVAnimDictionary (3) | 4D 4A 3W 1S | 04c | L5 |
| 05c | rtanim (4) + rtquat (5, incl. slerp cache) + `RtAnimRegisterInterpolationScheme` adapter | 5D 3A 1W | 05a | L1 |
| 06 | Im2D/Im3D (5 fns + ~330 setter/vertex sites), own `RwIm2DVertex` conversion, `RwIm3DVertex` typedef | 4D 1A | 00c, 02c | L2 |
| 07 | RxPipeline emulation (`RxPipelineCreate/Lock/AddFragment/Unlock/FindNodeByName`, `RxNodeDefinitionGetD3D9AtomicAllInOne`, `RxD3D9AllInOne*CallBack`) over `d3d9::ObjPipeline` | 11W | 04c, 02c | L3 |
| 08a | CustomBuildingPipeline + Renderer (FF verbatim) | game files | 07, 02c | L4 |
| 08b | CustomBuildingDNPipeline (ExtraVertColour plugin, colour blend instanceCB, env 2nd stage) | game | 08a | L4 |
| 08c | CustomCarEnvMapPipeline + EnvMat/SpecMat/EnvAtm plugins (FF + specular light) | game | 07, 02c | L5 |
| 09 | `D3DResourceSystem` stubs, `RwD3D9SetStencilClear`, restore callbacks, `GetD3D9Device()`, Gamma/WinPs device uses, drop `WindowedMode.cpp` | 3W 2S | 02a | L1 |
| 10 | integration tests: Wine headless exe loads gta3.img DFF/TXD sets through the SA code path (CStreaming::ConvertBufferToObject), compares plugin payloads (2dfx/col/breakable/night colours) with the probe dump; first triangle render under Wine+D3D9 | tests | all above | serial |
| 11x | review passes (line-by-line, sonnet) for each of: 01,02,03,04,05,06,07,08 | - | after each | follows |

First three: **P2B-00a, P2B-00b, P2B-00c** (nothing else can compile or link until the fakerw type layer exists); 00d is the first parallel fan-out (3 agents). Then lanes L1-L5 (<=5 agents; disjoint files `source/standalone/rw/<module>.{h,cpp}`, shared headers frozen after 00c; additions to a shared header go through the orchestrator).

## 7. Top risks
1. **RxPipeline/FF custom pipelines on librw-d3d9** (3 render callbacks, ~1.9k lines): mixing FF draws with librw shader draws needs strict state hygiene (`setVertexShader(nil)/setPixelShader(nil)`, transforms, lights via cache); fog, alpha-test and `GSALPHATEST` emulation interplay unverified. Mitigation: 08a first as the canary (simplest FF, building view).
2. **Lighting parity**: stock pipelines (peds, vehicles, props via MatFX/skin) switch from RW FF lighting to librw shader lighting; ambient/directional model (`app_light.cpp`) must be checked against `uploadLights`. Visual diff vs Wine-run original needed.
3. **SA RW patches unknown**: `Gta*StreamRead` semantics, `RpMaterialUVAnim*`/RtDict (librw uvanim "TODO fully"), HAnim/RtAnim scheme (key-frame callbacks are `static` in hanim.cpp -> librw patch needed to export); `RtQuatSetupSlerpCache`. Each a W with possible librw patch (keep patches in `vendor/librw` as a fork branch, not local edits).
4. **Header duality**: two RW header sets live in the tree (real SDK for ASI/dump build, fakerw for standalone); any game edit must compile under both -> CI job. Rewriting ~330 field sites must be written so that BOTH compile (accessor macros defined in the real-header build too).
5. **Direct `IDirect3DDevice9` use** outside pipelines (imgui_impl_dx9 56 calls, Gamma ramp, screenshot, MenuManager) rely on librw's state cache not being bypassed (cache desync -> wrong state): route via `rw::d3d::` or call `rw::d3d::flushCache`/invalidate after raw calls.
6. **Plugin registration order/size**: Collision plugin size 0, plugin registered after objects exist (RwFrame created before PluginAttach?) -> librw `registerPlugin` asserts; the `app.cpp` order is preserved but `Engine::init` must precede it and no Frame/Atomic/Material may exist yet: add an assert in the register thunks.
7. Wine's wined3d FF emulation and `D3DTSS_TCI_CAMERASPACENORMAL` + projective texture transforms (car env map) -- a known weak spot of wined3d; fall back to stage B shaders early if broken.
8. Effort drift: the 245-function count hides the ~330 raw-field rewrites and the Im2D vertex conversion cost on a hot path (Sprite2d: 91 vertex refs; per-frame scratch conversion, bounded by `RenderBuffer` sizes).
