# MAC-R2: slice R-2, exclusion list, guards, link stubs (2026-10-23, branch portable/main)

## Census (PortableMac native, snapshot of HEAD + my files via tools/portable/snap_census.sh; TSV .notes/w0/r2_after.tsv)
| item | R-1 after | R-2 after |
|---|---|---|
| total unique diagnostics | 1752 | 655 |
| R (tools/portable/r_census.py zones) | 1269 | **344** (plan: <= ~400) |
| standalone/rw | 974 | 338 (engine 179, camera 61, renderstate 52, raster 33, platform 13) |
| game_sa/Pipelines | 205 | 1 (DN.cpp:158 pointer-to-int cast = P stream, not R) |
| RenderWare/D3D* | 71 | 0 |
| Gamma + Radar | 19 | 5 (Radar inline asm = F stream) |
New files source/portable/render/rw_stubs_gl3.cpp, fakerw/d3d9_lite.h: 0 diagnostics. Excluding alone (no guards) gave 877 / R 564.

## Changes
* source/CMakeLists.txt (portable branch only): PORTABLE_EXCLUDED_RENDER_FILES = standalone/rw/{pipeline,pipeline_matfx,rwd3d_ff,pipeline_skin,pipeline_skin_cpu,skin_vs,im2d_im3d}.cpp, game_sa/RenderWare/D3D{ResourceSystem,TextureBuffer,IndexDataBuffer}.cpp. (StdInc.h untouched: the D3DResourceSystem/TextureBuffer/IndexDataBuffer headers compile as is; only the method bodies are replaced, by inert ones in the stub file. This deviates from the plan's d3d_resource_stub.h and keeps Win32 StdInc.h byte-identical.)
* Guards `#ifndef NOTSA_PORTABLE` (own lines): Pipelines/PipelinesCommon.hpp (FF helpers), CustomBuildingPipeline.cpp / CustomBuildingDNPipeline.cpp / CustomCarEnvMapPipeline.cpp (the three FF CustomPipeRenderCB bodies; DN/Building/CarEnv CreatePipe: portable returns true with ObjPipeline == nullptr so Game.cpp:584 InitialiseEssentialsAfterRW does not fail and atomics keep librw's stock pipeline; CarEnv still creates the plugin data pools; IsEnvironmentMappingSupported (D3DCAPS9) and PreRenderUpdate light fill and the g_GameLight D3DLIGHT9 global in the header guarded), Gamma.cpp (Init: portable m_IsEnabled=false; SetGamma tail), fakerw.h (`#else #include "d3d9_lite.h"`: D3DSHADE_*/D3DTADDRESS_* for Radar.cpp).
* New: source/portable/render/rw_stubs_gl3.cpp (32 symbols, see below), source/fakerw/d3d9_lite.h, tools/portable/{snap_census.sh, r_census.py, rw_symbol_audit.py}.
* snap_census.sh: snapshot = `git archive REF` (source, tools, tests) + vendor/librw working tree + `-f/-F` overlay files, compdb rewritten into the snapshot (entries whose file is not in the snapshot dropped), census at low priority, TSV mapped back; results in build/snap_census/NAME.{tsv,md,log}. A parallel agent's half-edits cannot reach it.

## Stubs / R-n items (honest: first call logs `R-2 stub: <fn> (<slice>)`, call fails or does nothing). Full list with callers: .notes/reports/MAC-R2-symbols.tsv (`rw_symbol_audit.py --check` = gate)
| id | symbols | file:line | behaviour | owner |
|---|---|---|---|---|
| R2-1 | RwIm2DRender{Primitive,Line,Triangle,IndexedPrimitive} | rw_stubs_gl3.cpp:28-31 | FALSE | R-4 |
| R2-2 | RwIm3DTransform (nullptr), RwIm3DEnd, RwIm3DRender{Primitive,IndexedPrimitive,Line} | :32-36 | FALSE / nullptr | R-4 |
| R2-3 | RwShimPipelineEnsure / Shutdown (called from engine.cpp) | :40-41 | log / no-op | R-3a, R-6 |
| R2-4 | RxPipelineCreate/Lock/Destroy, RxLockedPipeAddFragment/Unlock, RxNodeDefinitionGetD3D9AtomicAllInOne, RxPipelineFindNodeByName, RxD3D9AllInOne{Get,Set}*CallBack | :44-54 | nullptr / FALSE: custom pipelines are not created (CreatePipe guards skip them) | R-8, R-9 |
| R2-5 | RwD3D9SetRenderState, RwD3D9SetTextureStageState (renderstate.cpp only) | :57-58 | no-op | R-3b guards renderstate.cpp |
| R2-6 | D3DResourceSystem::{InjectHooks,CancelBuffering,Init,SetUseD3DResourceBuffering,Shutdown,TidyUpD3DIndexBuffers,TidyUpD3DTextures} | :61-67 | inert, no log | none (pools only held exe-RW D3D objects) |
Behaviour change vs the exe: Gamma disabled (R-11b), car/building FF env-map and day/night render callbacks absent until R-8/R-9 (R-8 DN CPU colour blend `PreRenderUpdate` still runs but has no GL target yet).

## Win32 no-op proof (sub-agent, scratch /private/tmp/claude-501/r2_ab; A = 8d4b18d3, B = A + the 10 files)
* `cmake --preset StandaloneRelease`: 1284/1284 TUs, 4670/4670 compile commands identical, build.ninja identical (20,409,959 B); rw_stubs_gl3 / d3d9_lite appear nowhere in the Win32 compdb/ninja.
* wine cl.exe, 10 TUs (Gamma, CustomBuildingPipeline, CustomBuildingDNPipeline, CustomBuildingRenderer, CustomCarEnvMapPipeline, CarFXRenderer, Radar, standalone/rw/pipeline, engine, Clouds): code + relocs + all non-debug sections + symbols + section order identical, 0 `__LINE__` immediates, 0 OTHER. Preprocessed (/P) text identical (A-only 0 / B-only 0) in all 10. Residue: .debug$T LF_UDT_SRC_LINE numbers and .debug$S checksums/line tables only.
* w0a_guardlines.py check: TEXT OK on all 7 edited C++ files.

## Open items / CONTINUE
* R-3a: engine.cpp 179 + platform.cpp 13 (RwShimPipelineEnsure caller, window adoption); R-3b: camera 61, raster 33, renderstate 52, then R must reach 0 (+ the P/F stragglers).
* Link is not attempted (no full link gate yet): the symbol audit is token-based (portable view of guards); a symbol only reachable through a macro in fakerw headers would not be seen.
* The census does not see CreatePipe/CustomPipeAtomicSetup semantics: first run (R-7) must confirm the game boots with atomic->pipeline == nullptr for buildings/cars and that RpD3D9GeometrySetUsageFlags (DN CustomPipeAtomicSetup) is harmless under GL3.
