# Stream R plan: render for the portable (macOS arm64, librw GL3 + SDL3) build

Author MAC-R-DESIGN, 2026-10-23, branch portable/main (b9f8d1a5). Read-only analysis; nothing in source/ changed.
Inputs: `.notes/w0/after.tsv` (census after MAC-W0, 1,998 unique root diagnostics, same file as `reports/MAC-W0.md` §2), `reports/MAC-A1b*.md`, `LIBRW_NOTES.md`, `LIBRW_BUILD.md`,
`P2B_SHIM_PLAN.md`, `P2B_SKIN_PIPELINE.md`, `PORTABILITY_INVENTORY.md` (R), `STREAM_C_PLAN.md` §5, vendor/librw @6f80bf3 (src/gl/*), source/standalone/rw/*, source/fakerw/*, source/game_sa/Pipelines/*.
Claims marked (UNVERIFIED) were read from code only and have to be proven by the first run on the Mac (slice R-3a smoke test).

## 0. Decisions in six lines

1. **Native GL3 pipelines, not a D3D9 fixed-function emulator.** The portable build renders atomics with librw's own gl3 default / skin / matfx pipelines; the three SA custom pipelines
   (building day/night, building env, car env-map + specular) get GL3 twins. The exe-faithful D3D9 façade (pipeline.cpp, pipeline_matfx.cpp, rwd3d_ff.cpp, HW skin VS: 2.4 kLoC, ~590 of the R diagnostics) is **excluded** from the portable build, not ported.
2. **Win32 path untouched.** Portable code lives in `source/portable/render/*.cpp` (already globbed GLOB_RECURSE only under GTASA_PORTABLE, `source/CMakeLists.txt:168-176`), whole D3D-only files go to `PORTABLE_EXCLUDED_FILES`,
   mixed files get `#ifndef NOTSA_PORTABLE` guards on their own lines (neutral by the 23:00 rule of PORTABLE_SYNC: only `__LINE__` shifts allowed). Every slice ships the usual no-op proof (StandaloneRelease compile_commands + build.ninja identical, wine cl.exe code+reloc signatures on the touched TUs).
3. **Shared = everything CPU-side that is already clean in the census**: frame/clump/atomic/geometry/material/light/texdict/stream/image/hanim/rtanim/rtquat/uvanim/skin(plugin)/world/math/camera_sync (3.1 kLoC lifted exe code)/matfx plugin data, plugin registration, UV-anim, skin sort (`sortLikeSA`). 0 R diagnostics there today.
4. **Window/context**: WinMain creates the `SDL_Window` (flag `SDL_WINDOW_OPENGL`, portable only); our librw fork gets a small `#ifdef LIBRW_SDL3` patch that *adopts* an existing window, requests depth 24 / stencil 8 / double buffer (it requests none today: SDL defaults give stencil 0, which kills stencil shadows) and creates the core 3.3 context (macOS returns 4.1 core, shaders are `#version 330`).
5. **Order**: (a) compile gate census R -> 0 in four slices R-1, R-2, R-3a, R-3b; (b) boot to the main menu (2D, textures, fonts): R-4, R-5, R-6, R-7; (c) world: R-8..R-11; the reference-frame harness R-12 starts early and runs beside.
6. **Acceptance for looks**: not bit-exact. Same-route frame marks from the Windows build vs the Mac build (section 6); SSIM / changed-pixel thresholds per frame class.

## 1. Inventory of the R diagnostics (post-W0 census, 1,998 total unique)

R-owned total now **~1,498**: standalone/rw 978 + game_sa/Pipelines 205 + game render code cascade 220 + game_sa/RenderWare/D3D* 69 + fakerw 15 + other DirectX in game code 11
(A1b counted 1,315 because the 220-diagnostic cascade below was filed under "unclassified"; fakerw.h/skeleton.h failing against GL3 librw is what produces it).

### 1.1 By group and root cause

| group | unique | root cause | fix | slice |
|---|---|---|---|---|
| **A. fakerw API surface vs GL3 librw** (fakerw.h 8, rwextra.h 3, skeleton.h 4) | 15 | `typedef rw::d3d::Im3DVertex RxObjSpace3DVertex` and `rw::d3d9::InstanceDataHeader/InstanceData` (fakerw.h:148,274-275) do not exist under RW_GL3; `static_assert(sizeof(void*)==4)` ILP32 (fakerw.h:280); `sizeof(RwEngineOpenParams)==sizeof(rw::EngineOpenParams)` (GL3/SDL3: `{SDL_Window**, bool32 fullscreen, int width, int height, const char* windowtitle}` = 32 B vs 8); `GetD3D9Device()` in rwextra.h:126; skeleton.h D3D (4) | game-owned portable types (see 2.2) | R-1 |
| **B. game render code, cascade of A** (Boat 49, Clouds 44, WaterLevel 20+, Shadows 16, Birds 8, FallingGlassPane 8, Lines 8, Mirrors 8, MotionBlurStreak 8, Heli 7, Fx 4, RenderBuffer 4, Rope 4, ShinyTexts 4, Skidmark 4, SpecialFX 4, StencilShadows 4, BrightLights 3, BulletTraces 3, ... 26 files) | 229 (220 outside D3D*) | `RwIm3DVertex` = `int` after the failed typedef: "member reference base type 'RwIm3DVertex' (aka 'int')" | disappears with A (no code change in these files) | R-1 |
| **C. D3D9 types/calls in standalone/rw** | 978 | see 1.2 | exclude / guard / GL3 twin | R-2, R-3a, R-3b |
| **D. D3D9 in game_sa/Pipelines** (CustomCarEnvMapPipeline.cpp 97, CustomBuildingDNPipeline.cpp 44, CustomBuildingPipeline.cpp 43, PipelinesCommon.hpp 20, CarEnvMap.h 1) | 205 | FF texture-stage / render-state / `RxD3D9*` calls written against D3D9 | `#ifndef NOTSA_PORTABLE` around the D3D sections + GL3 twin in source/portable/render | R-2 (guard), R-8, R-9 (twins) |
| **E. D3D resource pools** (game_sa/RenderWare/D3DResourceSystem.cpp 44, D3DTextureBuffer.cpp 15, D3DIndexDataBuffer.cpp 10) | 69 | pools of IDirect3DTexture9/IndexBuffer9 that only the *exe's* RW fed; the shim already leaves them inert (P2B_SHIM_PLAN s.3: S) | whole files to `PORTABLE_EXCLUDED_FILES`; the `D3DResourceSystem::*` callers (Game.cpp x7, MenuManager_Process x2, `StdInc.h:56-58` includes) get portable inline no-ops in `source/portable/render/d3d_resource_stub.h` | R-2 |
| **F. other DirectX in game code** (Gamma.cpp 12, Radar.cpp 2: `D3DSHADE_FLAT`/`D3DTADDRESS_CLAMP`) | 11 | `IDirect3DDevice9::SetGammaRamp`, D3DCAPS9 DEC3N, two D3D enum values | Gamma: portable body (gamma applied in R-11 as a final-pass uniform, no-op first); the two enums from `d3d9_lite.h` | R-1 (enums), R-2 (Gamma) |

Unrelated to R but visible in the same TSV: `operator new` size_t (CarGenerator 22, Pools 10) = P; `emmintrin/xmmintrin/mmintrin` 172 and `FtolExe.cpp` 7 = F; inline asm 36 = F.

### 1.2 standalone/rw, file by file (what the file is, what happens to it)

| file (lines) | unique | what it does | disposition |
|---|---|---|---|
| pipeline_matfx.cpp (814) | 351 | exe port of the FF MatFX env/UV render callbacks (TSS stages) | **exclude**; gl3matfx replaces (R-6); `RpMatFX*` public API (CPU: effects, env maps, UV anim) stays in matfx.cpp/uvanim.cpp (clean) |
| engine.cpp (739) | 179 | RwEngine*, sub-systems, video-mode list, windowed emulation, MSAA, `RwD3D9ChangeVideoMode`, `GetCaps`, `GetCurrentD3DDevice` | **GL3 twin** `source/portable/render/engine_gl3.cpp`; D3D body guarded in place (R-3a) |
| pipeline.cpp (1092) | 130 | `RxPipeline` façade over d3d9::ObjPipeline + exe AllInOne node port (instancing, tri sort, lights) | **exclude**; `RxPipeline*` API reduced to what R-8/R-9 need (stubs first, R-2) |
| rwd3d_ff.cpp (328) | 72 | `RwD3D9Set*/Draw*` FF state API | **exclude** (no FF device in GL) |
| camera.cpp (418) | 61 | RwCamera*, D3D view/proj upload, back-buffer screenshot dump, present | **split**: camera API guarded, GL parts in `camera_gl3.cpp` (R-3b); `camera_sync*` shared |
| im2d_im3d.cpp (365) | 57 | exe-port of Im2D/Im3D on D3D9 DrawPrimitiveUP | **exclude** in R-2 (stubs), **GL3 twin** `im2d_im3d_gl3.cpp` in R-4 |
| renderstate.cpp (391) | 54 | rwRENDERSTATE -> librw states, plus D3D fog/shade/alpha-func side effects | **guard the D3D branches in place** (R-3b); mapping core is librw-neutral (`rw::SetRenderState`) |
| raster.cpp (595) | 33 | RwRaster*, lock/unlock, camera-texture lock, DeviceClear, DrawQuad | **guard in place + GL3 twin parts** (R-3b/R-5) |
| platform.cpp (177) | 13 | D3D9 Reset watch + restore callback; `RpAnisotPluginAttach` | **exclude** the Reset watch; `RpAnisotPluginAttach` + no-op hooks move to `render/platform_gl3.cpp` (R-3a) |
| pipeline_skin.cpp (416) + pipeline_skin_cpu.cpp (196) + pipeline_skin_core.h + skin_vs.cpp (729) | 19+7 | HW skin VS (vs_1_1 text composer) and CPU-skin->FF draw | **exclude**; gl3 skin pipeline (R-10). Correction from P2B_SKIN_PIPELINE: SA peds have `numMeshes==0` so the exe CPU-skins them anyway; GPU skin is a behaviour-preserving substitute |
| hanim.cpp 1, pipeline_skin_core.h 1 | 2 | ILP32 assert; SIMD | R-1 / F-stream |
| texture.cpp (264), texdict.cpp, image.cpp, stream.cpp, frame.cpp, clump.cpp, geometry*.cpp, atomic.cpp, material.cpp, light.cpp, world.cpp, math.cpp, rtanim/rtquat/uvanim/skin/hanim/matfx/plugins | 0 | CPU/shared. NOTE texture.cpp includes `<src/d3d/rwd3dimpl.h>` and creates `PLATFORM_D3D9` rasters (compiles under GL3 because librw keeps a CPU fallback in `d3d::createTexture`); GL upload path is R-5 | keep, R-5 adds a GL3 branch |

### 1.3 The D3D vocabulary that the census sees (1,038 W/R "DirectX" diagnostics, per identifier class)

569 FF texture-stage constants (D3DTSS_*, D3DTOP_*, D3DTA_*, D3DTTFF_*), 189 device/resource types and formats (IDirect3D*, D3DFMT_*, D3DPRESENT_PARAMETERS, D3DCAPS9...), 137 render-state constants (D3DRS_*, D3DMCS_*, D3DSHADE_*),
98 FF data structs (D3DMATRIX, D3DLIGHT9, D3DMATERIAL9, D3DCOLORVALUE, D3DPT_*, D3DDECLTYPE_*). All are used only in the files that R-2/R-3 exclude or guard, except ~11 sites (Gamma, Radar).
So **no D3D constant header is added to the portable build** except `source/fakerw/d3d9_lite.h` (R-1; about 10 constants: `D3DSHADE_*`, `D3DTADDRESS_*`, `D3DGAMMARAMP`; only if Radar/Sprite code still names them after the guards).

## 2. Architecture

### 2.1 Layers

```
 game code (unchanged)  ->  RW C API (fakerw: rwcore.h, rwapi.h, rwaccessors.h, rwextra.h ...)
                              |                                   |
          shared CPU shim (standalone/rw/*.cpp, clean)     device-facing API
          frame/clump/geometry/texdict/stream/uvanim/        engine, camera, raster, renderstate, im2d/im3d, pipelines
          matfx data/skin data/camera_sync/math              |                           |
                              |                      Win32: standalone/rw (D3D9)   portable: source/portable/render/*_gl3.cpp
                              |                              |                           |
                              +--------------------------> vendor/librw: d3d9 backend   vendor/librw: gl3 backend (SDL3, GLSL 330)
```

The selection is the existing one: `NOTSA_PORTABLE` (game target, set by `GTASA_PORTABLE`) + `RW_GL3` (public define of the librw target) vs `RW_D3D9`. No new CMake option.
The switch happens at three levels and no other: (1) CMake exclusion list (whole files), (2) `#ifndef NOTSA_PORTABLE` around D3D sections of mixed files (own lines, no reflow), (3) typedef/struct selection inside fakerw headers.

### 2.2 fakerw under GL3 (R-1)

| item | Win32 (unchanged) | portable |
|---|---|---|
| `RwIm3DVertex` / `RxObjSpace3DVertex` / `RxObjSpace3DLitVertex` | alias of `rw::d3d::Im3DVertex` | game-owned struct, identical layout (pos 12, normal 12, `uint32 color` ARGB, u, v = 36 B) so `RxObjSpace3DVertexSetPreLitColor`, array code in Boat/Clouds/Water/... compile as is; converted to `rw::gl3::Im3DVertex` (r,g,b,a bytes) inside R-4 |
| `RwIm2DVertex` = `RwD3D9Vertex` {x,y,z,rhw,ARGB,u,v} | game-owned already | same type, converted to `rw::gl3::Im2DVertex` (w = 1/rhw, RGBA bytes) in R-4 |
| `RxD3D9ResEntryHeader`, `RxD3D9InstanceData` | aliases of `rw::d3d9::InstanceDataHeader/InstanceData` | portable structs with exactly the members the game reads (header: `primType, numMeshes, indexBuffer, vertexStream[], vertexDeclaration, useOffsets`; mesh: `material, vertexAlpha, numPrimitives, baseIndex, startIndex, numVertices, vertexShader`) so the pipeline `.cpp` sections that survive keep compiling; GL3 pipelines use rw::gl3::InstanceDataHeader directly |
| `RwEngineOpenParams` | `{void*}`, assert == `rw::EngineOpenParams` | `{void* window}` -> engine twin builds `rw::EngineOpenParams{SDL_Window**, fullscreen}` in static storage |
| ILP32 asserts (fakerw.h:280), RwTexture name[32] asserts | active | `VALIDATE_SIZE`-style guards (compiled out portable), same rule as P-1 |
| `GetD3D9Device()` (rwextra.h:126), `RwD3D9GetCurrentD3DDevice()` | returns the device | portable: returns `nullptr` (callers: Gamma, WinPlatform, WndProc, WinPs, SDLWrapper dump are all guarded / DLL-build-only) |

### 2.3 What librw GL3 already covers vs what is missing (librw @6f80bf3, src/gl)

| need | librw gl3 | gap -> plan |
|---|---|---|
| Rasters: 8888/888/1555/565/4444/PAL (expanded), DXT1/3/5 (`GL_EXT_texture_compression_s3tc`, `gl3Caps.dxtSupported`), mip levels, camera-texture (FBO) | yes (gl3raster.cpp, `d3d_to_gl3` cross-platform conversion) | SA's native-D3D9 TXD reader is game-owned (texture.cpp) and creates `PLATFORM_D3D9` CPU rasters -> add GL3 branch that builds gl3 rasters (compressed levels uploaded as is). R-5. S3TC on Apple Silicon GL (UNVERIFIED, expected yes) |
| Im2D / Im3D | yes (gl3immed.cpp, im2d.vert / im3d.vert, `rw::im2d::*`, `rw::im3d::*`) | vertex format conversion + RW semantics (framebuffer-offset of sub-rasters, primitive counts, return values). R-4 |
| Default pipeline lighting (ambient + directional/point/spot via `RpLight`), prelit colours, alpha test, fog | yes (gl3pipe.cpp `lightingCB`, default_vs) | look differs from exe's FF (R-12 tuning). Prelit * material colour semantics must be checked against RW (R-6) |
| Skin (4 weights, 64 bones, gl3skin) | yes | SA bone-set sorting is already done by the shared plugin (`sortLikeSA`); R-10 |
| MatFX env map / UV transform (gl3matfx) | yes, own env mapping | SA's env-map flavour (CustomCarEnvMap, camera-space-normal projective, `EnvAtm`) is a *custom* pipeline: R-9 |
| Stencil, blend, cull, z-test/z-write, alpha-ref, fog, texture filter/address, anisotropy | yes (gl3device.cpp render state cache incl. stencil) | `rwRENDERSTATE*` mapping already in renderstate.cpp (librw-neutral part) |
| Camera: view/proj into `devView/devProj` built **by the device's beginUpdate**, GL clip space | yes | the lifted `camera_sync` only fills frustum planes / `viewMatrix` for the game (not devProj) -> compatible; verify z convention (zScale/zShift are D3D-style) in the smoke test (R-3b) |
| Window / context | SDL3 `openSDL3/startSDL3` creates **its own window** and a core 3.3 context, depth/stencil not set | patch: adopt window + depth/stencil + hi-dpi policy (R-3a) |
| Building day/night, building env, car env-map + specular, Radiosity/heat-haze/colour filter (camera raster as texture), water, shadows, coronas | none (SA-specific) | R-8, R-9, R-11 |
| Gamma ramp | none | R-11: final-pass uniform (first: no-op `Unsupported`) |
| `glGetTexImage`-style framebuffer read-back (screenshots, `psGrabScreen`) | glReadPixels available through glad | R-7 |

### 2.4 SA custom pipelines on GL3

| pipeline | what it does (game_sa/Pipelines) | portable form | notes |
|---|---|---|---|
| Building day/night (`CCustomBuildingDNPipeline`, 418 lines + Renderer 96) | geometry plugin `ExtraVertColour` holds night colours (stream 0x253F2F9); `PreRenderUpdate` blends day/night by the time-of-day balance into the vertex-colour stream | **no new shader**: gl3 default instance, CPU blend into the colour attribute range via `glBufferSubData` (same as the exe's VB lock); render = gl3 default render callback | cheapest and exact w.r.t. the blend math; keeps the CPU code shared |
| Building (`CCustomBuildingPipeline`, 177) | default + env-add stage (camera-space-normal projective texture, MULTIPLYADD) for reflective surfaces | ObjPipeline with a small GLSL fragment term (env texture add) reusing gl3matfx env uniform plumbing | R-8 second half |
| Car env-map + specular (`CCustomCarEnvMapPipeline` 533 + EnvMat/SpecMat/EnvAtm plugins 362, all plugins clean/shared) | FF: spec light `g_GameLight`, camera-space-normal TCI, projective texture, material sources | ObjPipeline + GLSL (env map with `EnvAtm` scale/shininess, specular term); stock gl3matfx env as the first stand-in so cars render before R-9 | R-9 |
| `PipelinesCommon.hpp` helpers (`RxD3D9InstanceDataRender*`, `SetTextureStagesForLighting`) | FF helpers | guarded out, GL3 twins inside R-8/R-9 | |

How the game attaches them (`CustomPipeAtomicSetup`, `atomic->pipeline = ...`): the `RxPipeline*` calls become `rw::gl3::ObjPipeline` creation in the twin (`pipelines_gl3.cpp`, R-8). Skin and plain atomics use `atomic->pipeline == nullptr` -> librw default / `Skin::setPipeline` / `MatFX::setPipeline` for GL3 (registered by `Engine::init` for all platforms).

### 2.5 Platform glue (who creates the window, the context)

Today: `WinMain.cpp:466-474` (`NOTSA_USE_SDL3`) calls `SDL_CreateWindow(APP_CLASS, w, h, SDL_WINDOW_RESIZABLE)` (no `SDL_WINDOW_OPENGL`), stores the pointer in `PSGLOBAL(sdlWindow)` and in the HWND slot (`app_win32_compat.h`). Input (SDLWrapper) and the injected-input script use that window.
librw's `startSDL3` creates a second window and returns it through `*glGlobals.pWindow`; it sets no depth/stencil size; its profile loop is core 3.3 -> core 2.1 -> ES.

Plan (R-3a), one small `#if defined(LIBRW_SDL3)` patch in vendor/librw (littlemyx/librw `notsa`; it does not alter the D3D9 build; commit in the submodule, bump the gitlink in a separate one-file commit):
1. WinMain (portable branch only): `SDL_GL_SetAttribute` double buffer, depth 24, stencil 8, core 3.3 profile; window flags `SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL` (no HIGH_PIXEL_DENSITY first: 1x backing store; Retina handling = later, `presentWidth` uses `SDL_GetWindowSize` points).
2. librw `startSDL3`: if `*glGlobals.pWindow != nullptr` use it (skip `SDL_CreateWindow`, keep `SDL_SetWindowFullscreen` logic), then `SDL_GL_CreateContext(win)` over the same profile list; attributes depth/stencil/double buffer set once before the loop. `stopSDL3` must not destroy an adopted window (the game owns it).
3. Engine twin: `RwEngineOpen` -> `rw::Engine::open(&EngineOpenParams{&s_Window, fullscreen, w, h, title})` with `s_Window = (SDL_Window*)PSGLOBAL(window)`; `RwEngineStart` -> `rw::Engine::start()` creates the context and loads glad; video-mode list = librw's (`makeVideoModeList`, SDL display modes); `RwD3D9ChangeVideoMode` -> `rw::Engine::setVideoMode`; MSAA changes -> stored, applied at next start (`SDL_GL_MULTISAMPLESAMPLES` needs a new context: menu option shows "restart required", logged).
4. vsync: librw swaps in `showRaster` with `SDL_GL_SetSwapInterval(1|0)` from `rw::Raster::FORCEVSYNC`-style flag; keep `NOTSA_STANDALONE_NOVSYNC` env.

**macOS GL 4.1 core**: librw requests core 3.3; Cocoa SDL returns the highest core (4.1) (UNVERIFIED by run). All gl3 shaders use `shaderDecl330` (`#version 330`, `layout(location)`), no UBO (`RW_GL_USE_UBOS` off), no GL >= 4.2 call found in gl3*.cpp (no `glTexStorage`, `glBufferStorage`, DSA, compute; global VAO created for glversion >= 30). Known macOS limits: `glLineWidth > 1` ignored (Im3D lines), no `GL_LUMINANCE` formats (librw uses RED paths), GL is deprecated but functional. R-3a smoke test prints `GL_VERSION`/`GLSL`, compiles all gl3 shaders, draws and reads back pixels: that is the proof.

## 3. Hot files (Windows host, Q5)

`git log fork/reverse/interior_c --since=3.days -- source/standalone/rw source/fakerw source/game_sa/*Render* source/game_sa/Pipelines vendor/librw`: **4 commits, all `CRenderer.cpp` fidelity fixes (9f99461c, b9fa8e16, 77cb8028, 14378a8e)**, not an R file. `standalone/rw`, `fakerw`, `Pipelines`, `RenderWare/D3D*`, `vendor/librw`: no commit in 3 days.
Last 14 days: Pipelines/CustomCarEnvMap plugin oracle fixes (1a208459, 8b0fa60e, 40532208, 49600477, a16161b7, 8dc466d8: 10/20; plugin files are shared, no D3D), `rwmath_exact.h` (f01d66b4, 10/20), `engine.cpp`/windowed+MSAA (f00acfab U17, 2b85560a, 4c0f0322: 10/18). `git diff portable/main..fork/reverse/interior_c` on these paths: only the Mac-side guards in fakerw.h (2 lines), CustomBuildingDNPipeline.cpp/.h (6+2), CustomCarEnvMapPipeline.h (2) - i.e. no Windows-only change pending.
Risk list for the guard-in-place edits (merge-conflict candidates if the Windows side resumes them): `engine.cpp`, `camera.cpp`, `renderstate.cpp`, `raster.cpp` (all standalone/rw, cold), `CustomCarEnvMapPipeline.cpp` (oracle work 10/20, may recur), `CustomBuildingDNPipeline.cpp`. Mitigation: the guard commits are one slice each and announced in PORTABLE_SYNC before they start (text in section 6.3); `vendor/librw` is shared with Windows (branch `notsa`): portable changes only inside `#ifdef LIBRW_SDL3` / `RW_GL3`.
Note: `git status` shows `M vendor/librw` (checked out 6f80bf3 differs from the recorded gitlink) - the first R slice that touches librw must record the pin in its own commit.

## 4. Slice plan (one sonnet agent each, <= 100 min, review pass after each; Win32-neutral proof in every slice)

Notation: census = `nice -n 15 python3 tools/standalone/clang_census.py run --mode native --compdb build/PortableMac/compile_commands.json --out DIR -j 6` + `clang_census_report.py`; "R" = diagnostics located in the R-owned files of section 1 (script: filter of `after.tsv` as in this plan's inventory; R-2 adds `tools/portable/r_census.py` for the filter).
Baseline: R = 1,498 (post-W0), total 1,998.

| id | slice | deps | files (owner zone) | acceptance | est |
|---|---|---|---|---|---|
| **R-1** | fakerw under GL3 | - | source/fakerw/{fakerw.h,rwextra.h,rwaccessors.h,skeleton.h}, new fakerw/d3d9_lite.h, standalone/rw/hanim.cpp (assert) | group A 15 -> 0, group B 220 -> 0 (Boat/Clouds/Water/Shadows/... no diagnostics), R <= 1,265; PCH closure 0 errors from fakerw; Win32: compile_commands identical + wine code-sig on fakerw-including TUs (pipeline.cpp, engine.cpp, Clouds.cpp) | 90 |
| **R-2** | exclusion, guards, link stubs | R-1 | source/CMakeLists.txt (PORTABLE_EXCLUDED_FILES), `#ifndef NOTSA_PORTABLE` in Pipelines/*.cpp/hpp, Gamma.cpp, StdInc.h D3D resource includes; new `portable/render/{rw_stubs_gl3.cpp,d3d_resource_stub.h}`, `tools/portable/{r_census.py,rw_symbol_audit.py}` | excluded: pipeline.cpp, pipeline_matfx.cpp, rwd3d_ff.cpp, pipeline_skin*.cpp, skin_vs.cpp, im2d_im3d.cpp, RenderWare/D3D{ResourceSystem,TextureBuffer,IndexDataBuffer}.cpp; **R <= ~400** (engine 179, camera 61, raster 33, renderstate 54, platform 13, rest); `.notes/reports/MAC-R2-symbols.tsv` = every RW/RwD3D9 symbol referenced by portable game TUs and defined only in an excluded file, each with a stub (`Unsupported(...)` log, returns failure/null) or a named owner slice | 100 |
| **R-3a** | engine, platform, window adoption, smoke app | R-2 | `portable/render/{engine_gl3.cpp,platform_gl3.cpp}`, guards in standalone/rw/{engine,platform}.cpp, WinMain.cpp portable window block, vendor/librw `gl3device.cpp` (SDL3 adopt/depth/stencil), `tests/portable/gl_smoke.cpp` + `tools/portable/run_gl_smoke.sh` | engine.cpp + platform.cpp 192 -> 0 (R <= ~160); smoke app on the Mac: window opens, GL version line logged (core, >= 3.3), gl3 shaders compile, clear colour + Im2D quad read back by `glReadPixels` match expected; stencil bits >= 8 and depth >= 24 reported | 100 |
| **R-3b** | camera, raster, renderstate | R-3a | guards in standalone/rw/{camera,raster,renderstate}.cpp; `portable/render/{camera_gl3.cpp,raster_gl3.cpp}` (BeginUpdate/EndUpdate/Clear/ShowRaster, RwRasterCreate/Lock/Unlock/RenderFast/Push/Pop for plain + camera-texture rasters, back-buffer read-back helper) | those files 148 -> 0, **census R = 0** (only the 2 F/P stragglers left: pipeline_skin_core SIMD is F); smoke test extended: camera update with a perspective frustum draws an Im3D triangle at the expected pixels (z convention proof), `RwCameraClear` with z/stencil, `RwRasterShowRaster` present | 90 |
| **R-4** | Im2D / Im3D twin | R-3a (smoke app) | `portable/render/im2d_im3d_gl3.cpp` (+ unit test tests/portable/rw_im2d_im3d_gl_test.cpp) | stubs from R-2 replaced; read-back tests: textured/untextured quad, strip/fan/list, line list, indexed, sub-raster offset, alpha blend; RW return-value contract of the old test (tests/standalone/rw_im2d_im3d_test.cpp case list re-run on the GL build) | 80 |
| **R-5** | textures: native D3D9 TXD -> GL3 | R-3a | standalone/rw/texture.cpp (GL3 branch under portable), `portable/render/texture_gl3.cpp`, tests/portable/rw_texture_gl_test.cpp | every `.txd` in models/ (gta3.img, gta_int.img, player.img, generic/, fonts, menu) loads: counts per format (DXT1/3/5, 565, 1555, 4444, 8888, PAL8-expanded) with 0 failures and `glGetError()==0` after upload; mip count preserved; one DXT texture drawn via Im2D and compared to a CPU decode within tolerance | 100 |
| **R-6** | stock gl3 pipelines wired: default, matfx, lights, fog | R-4, R-5 | `portable/render/pipelines_gl3.cpp` (defaults), `standalone/rw/{matfx,light,renderstate}` portable hooks | smoke app renders infernus.dff + txd (static, MatFX env on) and a building DFF; pixel sanity (non-black coverage, bbox of the object, alpha-tested pixels), render-state unit cases (blend, cull, z, alpha ref, fog) vs expected read-back; prelit/material colour rule documented | 100 |
| **R-7** | boot to the main menu | R-6 + streams W-1 (window/input), W-2/W-7 (data root, case), link gate | `psGrabScreen`/screenshot env via glReadPixels (portable/app_platform.cpp is W-zone: only the GL read-back function in `render/`), `NOTSA_STANDALONE_SCREENSHOT*` GL path, Sprite2d/CFont/CHud run check | the run reaches the front end; `mark_menu.png` written; first diff vs Windows reference (section 6) reported, not gated | 100 |
| **R-8** | buildings: day/night + env | R-6 | `portable/render/pipelines_building_gl3.cpp`; guard-complete CustomBuilding*.cpp | `CCustomBuildingDNPipeline::PreRenderUpdate` CPU blend writes to the GL colour stream (unit test: balance 0/0.5/1 -> read-back colours); Pipelines D diagnostics for buildings 0 (already 0 by R-2 guard, now functional); world frames at two clock times show day/night difference | 100 |
| **R-9** | vehicles: car env map + specular | R-6 | `portable/render/pipelines_carenv_gl3.cpp` + GLSL | cars render with reflection on `infernus` and `vehicle.txd` env textures; EnvAtm/EnvMat/SpecMat plugin data consumed; frame diff on a vehicle close-up | 100 |
| **R-10** | skinned peds / hanim | R-6 | `portable/render/pipelines_skin_gl3.cpp` | male01 + wmycr skinned render, 28-bone hierarchy matches `RpHAnim` update; mesh pivots (no exploded vertices); optional CPU-skin comparison on a few frames | 60 |
| **R-11a** | world effects: water, shadows, sprites, coronas, Im3D lines | R-6 | render-code fixes only where GL differs (z-bias, lines, point-sprites) | frame marks around water / shadows within thresholds | 100 |
| **R-11b** | post effects, gamma, camera-texture rasters | R-3b, R-6 | `portable/render/postfx_gl3.cpp`, Gamma portable body | heat haze / motion blur / colour filter / radiosity run without GL errors; gamma = uniform in the final pass | 100 |
| **R-12** | frame-diff harness and reference frames | R-3a (parallel from the start) | `tools/portable/frame_diff.py`, `tools/portable/ref_frames/` index, doc | tool exits non-zero outside thresholds; first reference set landed (section 6) | 80 |

Order for the planner: R-1 -> R-2 -> R-3a -> {R-3b, R-4, R-5, R-12} (disjoint zones: camera/raster/renderstate | im2d | texture | tools) -> R-6 -> {R-7, R-8, R-9, R-10, R-11a, R-11b}. Max 4-5 parallel (R-8..R-11 touch disjoint files; R-7 touches app/ and is in W-stream territory: coordinate).
Totals: compile gate (R-1..R-3b) = 380 min agent time, 3 sequential steps (~4.5 h calendar with R-3a||R-3b impossible: R-3b needs R-3a's smoke app; R-3b can start in parallel only on the camera/renderstate guards). Menu (R-4..R-7) 380 min. World (R-8..R-11b) 460 min. Harness 80 min. Total ~1,300 agent-min, 14 slices.

Dependencies on other streams: link of the whole exe (A/W/P/F) is needed for R-7 onwards; R-1..R-6 are verified by compile census + the standalone smoke/unit executables (no game link required: they need only librw + fakerw + the portable/render files + the unchanged shared standalone/rw files).

## 5. Risks and open questions

1. **Prelit/material colour semantics and lighting** differ between RW's FF lighting and librw's shader lighting (P2B_SKIN_PIPELINE s.3). Expect an R-12 look-tuning pass; budget one extra 100-min slice after R-11.
2. **Camera z / zScale+zShift** are D3D-convention (lifted exe code). The gl3 device's own beginUpdate builds `devProj`; confirm no code path in the shim feeds the D3D projection into GL (R-3b test).
3. **S3TC** (and anisotropy) on Apple Silicon GL: expected (EXT_texture_compression_s3tc is exposed), UNVERIFIED. Fallback: decode DXT to 8888 at load (R-5 has a switch).
4. **MSAA at runtime**: GL needs a new context; menu antialiasing changes become "applies after restart" (logged). Acceptable for behavioural port.
5. **Retina**: first milestone at 1x (no high-pixel-density flag); `glGlobals.presentWidth` uses points. Revisit after R-7.
6. **Hot file conflicts** (section 3). Guard-in-place edits are small; the alternative (full copies of camera/engine) was rejected because it forks shared logic.
7. **Not covered**: video playback (W), audio (W), toolsmenu ImGui backend (imgui_impl_dx9: needs `imgui_impl_opengl3`; separate, post-R-7, debug UI is off in the run build), D3D-specific `RwD3D9EngineSetRefreshRate` (SDL fullscreen mode refresh).
8. The FF-emulation alternative (uber-shader over the `RwD3D9*` API, ~3-4 kLoC, exe-faithful look) is **deliberately not chosen**; it stays possible later as a different `pipelines_*_gl3.cpp` behind the same ObjPipeline hooks if the look gap after R-12 is judged too large.

## 6. Reference frames (Windows build as the oracle)

### 6.1 What exists
`NOTSA_STANDALONE_SCREENSHOT=<k>` / `_MS` / `_MAX` / `_ARM`, `mark_<name>.bmp` written for `mark:` lines of an input route (`tools/standalone/soak_route.txt`: menu, newgame_loading, cutscene_skip, walk_start, walk_0), `mark:burst_<x>`; `gameplay_regress.sh` scenario `render_firststart`. Old samples in `.notes/standalone_{mainmenu,loading,ingame,ingame_hud,cutscene,driving,idle10min}.png`.

### 6.2 Protocol
* Same resolution both sides (1280x720 windowed, no AA, anisotropy off, VSync off, fixed seed/clock: new game start, no replay, weather forced via the route if available).
* Reference set REF-1 (requested from Windows, produced once with `NOTSA_STANDALONE_WINDOWED`): `mark_menu`, `mark_newgame_loading`, `mark_walk_start`, `mark_walk_0`, plus 3 fixed-camera world frames (day, night, rain) via the existing camera/route facility, plus a 2D-only test frame (font page, HUD icons).
* Mac writes the same marks through the same script (input injection through SDL events is portable) into `tools/portable/ref_frames/mac/`; Windows frames are copied once into `tools/portable/ref_frames/win/` (PNG, git-lfs not needed: ~1 MB each, <= 12 files).
* Metrics (`frame_diff.py`): per frame class
  * 2D (menu, loading, font): >= 99.5% pixels within |delta| <= 8, SSIM >= 0.98 on non-video regions;
  * world: SSIM >= 0.80 and mean luminance within 15%, horizon/HUD bbox checks; the number is recorded per slice, tightened only after R-12 tuning;
  * never compare bit-exact. Mask regions for the clock/FPS counters.

### 6.3 Text for PORTABLE_SYNC.md (orchestrator appends when R-12 starts; not written by this agent)
`[mac] <date>: stream R plan landed (.notes/STREAM_R_PLAN.md). (1) Request: produce reference frames REF-1 (section 6.2) from the current reverse/interior_c Release build with soak_route marks and send/commit to tools/portable/ref_frames/win/. (2) Announcement: guard-in-place edits (#ifndef NOTSA_PORTABLE, own lines) coming in standalone/rw/{engine,camera,raster,renderstate,platform}.cpp, Pipelines/*.cpp, Gamma.cpp, WinMain.cpp window block, vendor/librw gl3device.cpp (LIBRW_SDL3 only). Please avoid reformatting those files; tell us if a Windows slice needs them.`

## 7. Out of scope / follow-ups
Exe-faithful pixel parity, Vulkan/Metal backend, ImGui GL backend, shaders for night-vision/infrared beyond what PostFX needs, multi-monitor/HiDPI, controller rumble. Nothing here changes game logic.
