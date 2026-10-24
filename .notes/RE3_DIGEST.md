# RE3_DIGEST — what re3 / reVC learned that matters for our portable gta-reversed

Sources (read via git only, no checkout): `~/github/re-GTA` local branches `re3` (last upstream commit d0e4e15c, 2021-12-29, 3171 commits) and
`reVC` (1b9a5897, 2021-12-29, 5090 commits, 1951 not in re3); both pin librw 5501c4f (2021-08-19). reLCS not examined. Hashes/paths are
pointers (`git show <hash>`, `git show re3:<path>` / `reVC:<path>`). Tags: **[III]** = re3, **[VC]** = reVC (closer to SA: skinned peds
everywhere, bikes/helis/boats, PS2-style real-time shadow cameras, new PostFX, matfx water). No re3/reVC code is reproduced (DMCA'd; licence unclear).
Written 2026-10-24 by MAC-RE3-DIGEST. "SA?" = does it apply to San Andreas; "Rec" = one-line recommendation.
Our side: SDL3 platform layer, librw GL3 (notsa fork), OpenAL-emulated DirectSound, own path layer, pthread CdStream (PHASE2_STATUS "[mac]").
Main finding: re3 and reVC share ~all platform/shim code (reVC = re3 + VC game logic; its skel/glfw, fakerw, CdStreamPosix, premake/CI are copies synced
by the same authors), so platform items are tagged [III+VC]. Where VC has something III lacks, it is called out explicitly: **ShadowCamera/CutsceneShadow (2.5),
matfx water (2.8), skinned-ped hit colmodel FPE (3.16), stencil-clear for blur (2.4), colourfilterVC (2.4)**.

Key framing: re3 is a *source-level* port that compiles the game against `src/fakerw/` (a ~1000-line shim mapping the RW C API onto
librw) plus `src/skel/glfw/` (platform layer). That is the same architecture as our `source/portable/render` + `app_*.cpp`, so most
of re3's bugs are bugs we can still hit. re3 never needed what SA needs in rendering (no stencil/real-time shadows, no heat haze,
no night vision, no skin on every ped) — for R-10..R-12 re3 gives the *mechanism templates*, not the answers.

---------------------------------------------------------------------------------------------------------------------------------
## 1. Linux x64 build + CI (then Win x64)

### 1.1 Build system history (how re3 got there)
| Step | Commit | What |
|---|---|---|
| Linux support | 8a4fa58c (2020-05-11), 38a09c6e/c993e99e/a61736c5/fcac1f9a (follow-up "fix linux build" x4 in 2 weeks) | 54 files: POSIX CdStream (new `src/core/CdStreamPosix.cpp`), `crossplatform.cpp/h` (Win32 API emulation), glfw skeleton, premake Linux platforms |
| Travis Linux job | b55ba135 | first CI; disabled 3ae4c005 (2021-01) after GitHub Actions took over |
| macOS | 01ea7801 (2020-09-29), bbb578c5 | arm64 + x86_64 via `-target ...-apple-macos11/10.12`, `-std=gnu++14`; unnamed semaphores unsupported on macOS |
| CMake + conan | 1df6dda5 (2020-11-01), 8d0b4ede, b375e20c, 3519cbd3 | `CMakeLists.txt` + `src/CMakeLists.txt` + `cmake/Find{Mpg123,SndFile,Opusfile,MilesSDK}.cmake` + `conanfile.py`; audio/opus/libsndfile are options |
| GitHub Actions | d9f6a05b (2021-01-03), 8916fe18, a83ecc12 | `.github/workflows/build-cmake-conan.yml` (matrix win/ubuntu-18.04/macos x gl3-glfw/d3d9; conan install -> build -> package -> cpack -> upload-artifact), plus `re3_msvc_amd64.yml`/`re3_msvc_x86.yml` (premake + msbuild on windows-2019, Debug/Release x d3d9/gl3, bundle gamefiles + OpenAL/mpg123 DLLs as artifact). Optional configs marked `continue-on-error`. PR rules added 3baccc9d. |
| Sanitizers in CI-able form | f144a782, 1d3b4d1e, 8c170a62, 70fa7fc2, a8549e86 | CMake options WITH_ASAN / WITH_SANITIZERS (UBSan incl. integer, implicit-conversion, local-bounds, nullability). They found real bugs (int8 parameter fed uint8 ids in Garages; unclamped sscanf values; ped list overruns). |

[VC] delta vs [III] for build/CI: identical workflows renamed (`reVC_msvc_amd64.yml`, same conan workflow); extra commits 1d51734d (Mac M1: per-language `-target` flags for .c vs .cpp, Homebrew `/opt/homebrew` and MacPorts include/lib dirs),
3d5f0ef3 (librw bump "fix Mac hi-dpi": camera size from **framebuffer size** instead of window size, librw 8b2caf8), a92dcaa0/4e3fc354 (`--no-full-paths`, `--no-git-hash` for reproducible CI), 2f7ae611 (Vanilla = win-x86 only), 4a5f7464/4e40c6f2 (X11 autodetect).
Nothing else new for CI; the Linux/mac platform layer is the same code.

### 1.2 Concrete Linux/GCC pitfalls re3 hit
- **Pool `operator new` must be non-throwing** (9b5caa19 "Pool fixes - mostly for Linux"): GCC/clang assume a plain `operator new` never returns null and delete the
  caller's null check; the pools return null when full. re3 marked every class-specific new/delete `throw()`. SA? YES (CPed/CVehicle/CObject… pool-backed new in
  gta-reversed). Rec: grep our pool `operator new` overloads, make them `noexcept`, add a pool-exhaustion test under -O2 on Linux.
- **GCC 7.5 crash hunt** (424a6d90): crashes turned out to be dangling/null ped pointers in the "near peds" list, only visible with GCC's optimiser; fix = remove self from
  neighbours' lists in the destructor + asserts. SA? same lists exist (CPed::m_nearPeds/ pedlists). Rec: run the Linux build under -O2 with ASan from day one.
- **`sys/syscall.h` only under `__linux__`** (b60baf46); clang rejects comparing fixed-up values to NULL (56ea53a5); string literal -> `char*` (eeb26cfc);
  `strcasecmp/stricmp/strupr/strlwr/strsep` provided by `crossplatform.cpp` (c202fc3b). Rec: we already compile on AppleClang; add GCC + `-Wall -Wextra` leg to catch the rest.
- **MSVC-only flags** (f73450f5): static CRT, fast FP model, and `/Zc:sizedDealloc-` because class-specific `operator delete(void*, size_t)` on pooled classes breaks with
  sized deallocation. SA? the sized-delete part YES for Win x64 MSVC; the fast-FP part NO (see 1.4).
- **X11 link only for the keyboard hack** (bf7280b5, e0ecbeee autoconf probe). Not relevant with SDL3.
- Libraries linked on Linux: GL, glfw, openal, mpg123, sndfile, pthread (+X11). For us: SDL3, OpenAL, GL, pthread, dl; mpg123/sndfile only if we play MP3/WAV via them.

### 1.3 Filesystem / case sensitivity (Linux and case-sensitive APFS)
- `casepath()` in `src/skel/crossplatform.cpp` (bedc066c, 067200dd, eb8844fd "chaos", 98ec7bda buffer size, 22e8e0ef MP3 dir): walks each path component with `readdir` and
  case-insensitive compare; converts `\` to `/`; **trims trailing spaces** (game data lines have them); has a "path already correct, skip" fast path; later components after a
  miss are just slash-converted. Used by `fopen`/`rename`/`FindFirstFile` emulation (`_fcaseopen`, `_caserename`). No cache: every open re-scans directories.
  SA? YES (SA opens far more files: models/, data/maps, audio/, anim/, text, stream). We have `source/portable/path_resolve.cpp`.
  Rec: verify ours handles trailing-space, mixed `\`/`/`, absolute paths, and caches directory listings (invalidate on create/rename); add a test on a case-sensitive volume.
- `FindFirstFile` emulation keeps the wildcard-suffix only (`*.ext`), skips non-regular entries, tolerates `DT_UNKNOWN` (some filesystems). Rec: same behaviour check for our MP3/screenshot enumeration.

### 1.4 Floating point / determinism (our inference, not from re3)
re3 builds Windows with fast-FP; we are chasing bit-identical traces against the Win32 exe (MAC-FLOOP3). For Linux x64 and any arm64: build with strict IEEE (`-ffp-contract=off`,
no `-ffast-math`, SSE2 not x87; MSVC `/fp:precise`). GCC defaults to `-ffp-contract=fast` (FMA fuse) which only bites when `-march` enables FMA; AppleClang arm64 fuses by default
within a statement. Rec: set `-ffp-contract=off` globally in the portable CMake target; keep trace-diff test as the CI gate.

### 1.5 Win x64 specifics (1dc6fbda "64-bit on Windows", c54d3ba2, b21f49ad)
- Premake platform names `win-amd64-librw_{d3d9,gl3_glfw}-oal`; separate `.32/.64` dist DLLs for mpg123/libsndfile (a `ssize_t` typedef clash between the two builds: `#ifndef _WIN64`).
- Needed `<d3d9types.h>` instead of d3d8types for the librw D3D9 path; `Vanilla` config is x86-only by design (config.h errors for LP64/_WIN64).
- The 64-bit *code* fixes are all in section 3. Everything else (Win32 skeleton: sticky keys, power-off, registry "Documents\GTA3 User Files") is irrelevant to SDL3.
Rec for CI order: Linux x64 (apt + CMake) first; Win x64 as a second job reusing the same CMake with vcpkg/SDL3 binaries and `/Zc:sizedDealloc-`; macOS arm64 already local.

---------------------------------------------------------------------------------------------------------------------------------
## 2. Render completeness R-10 .. R-12 (what re3's librw GL3 experience tells us)

### 2.1 The shim layer (`src/fakerw/fake.cpp`) — recurring bug classes
| Issue | Commit | Mechanism | SA? | Rec |
|---|---|---|---|---|
| Fog colour channel order | f027c3af | game passes ARGB-style packed colour, librw wants RGBA: R and B must be swapped in the shim | YES (SA sets fog colour every frame) | Check our `rwRENDERSTATEFOGCOLOR` shim; wrong fog hue is the classic symptom (blue sky fog on red). |
| TEXTURERASTER is a pointer | c54d3ba2 | shim cast the void* value to uint32 before dispatch (truncates on LP64) | YES | Audit every `RwRenderStateSet(void*)` case for uint32 truncation; texture raster, and any pointer-valued state. |
| Foreign-platform DFF/TXD | 546ed0ff | after reading a clump, run a per-atomic "convert platform" pass so D3D8-format files work on GL pipelines | YES (SA assets are D3D9/PC) | Presumably done (game renders); confirm every load path (cutscene, car variants, IMG streamed, loose DFF) goes through it. |
| Anisotropy plugin, MSAA | c67273e9, 18d0fd2e, 7750758 | plugin registered at attach; MSAA hint only when samples > 1 (GLX rounds "1 sample" up to 2x/4x) | YES | Same condition on SDL_GL_MULTISAMPLESAMPLES. |
| `RwRenderStateGet` | 8614f686 | needed for save/restore of alpha-test state | YES | We need save/restore wrappers around alpha-ref changes (see 2.2). |

### 2.2 Lighting close to D3D fixed function (R-12)
- librw GL3 default pipe (`vendor/librw/src/gl/shaders/default.vert`, header.vert): per vertex = prelit colour + global ambient x material-ambient + sum(dynamic lights) x material-diffuse,
  clamped, **then multiplied by the material colour**; up to 8 lights (type/radius/cone/colour uniforms); fog factor from clip-space w; alpha test in fragment shader with a
  `NO_ALPHATEST` shader variant (bb7fb68) chosen per draw. D3D9 FF path (librw `d3d9render.cpp`) uses prelit colour as the *emissive* source and the material as ambient/diffuse
  source; geometry flag MODULATE gates whether the material colour is applied at all (librw 71d1328 "modulate flag check"). Note the structural difference: in FF the material
  colour scales only the lit terms, the prelit/emissive term is not scaled; librw-GL scales everything. For SA world geometry (white materials) invisible; for coloured materials
  with prelit vertex colours (some objects, peds' clothes tint pass) it can differ. SA? YES. Rec: R-12 starts with a CPU reference of the FF formula (ambient, directional x N, point
  with D3D attenuation, spot, emissive-from-prelit, MODULATE, clamp order) and a numeric A/B against the GL shader outputs for 20 synthetic cases; only then tune shaders.
- GS/PS2 alpha-test emulation exists in librw (d8d13d4, a312007): a mode that writes colour but not depth for failed alpha. Used by re3 for the "PS2 alpha test" option; SA PC
  uses plain alpha ref compare (RW `ALPHATESTFUNCTIONREF`). Rec: don't enable GSALPHATEST; keep ALPHATESTREF per draw (SA changes ref per entity type: 100 vs 0).
- Alpha-ref pitfall in re3 itself (581cb5ed "The real pickup reflection fix"): static shadow pass rendered with a leftover alpha ref and wiped pickup reflections; fix = explicit
  set/restore helpers (RwHelper `SetAlphaTest/RestoreAlphaTest`) around the pass. SA? YES (SA has more passes with their own refs). Rec: wrap every pass that changes ref.
- Clear pitfall (librw bae891f): `glClear` ignores depth when the depth mask is off; librw now forces depth mask on around clears. Same applies to **stencil write mask** for SA
  stencil work. Rec: in our clear path, restore all masks (depth, stencil, color) before clearing; test with a pass that leaves zwrite off.

### 2.3 Skin pipeline audit (R-10)
What re3/librw settled on (all in librw unless noted):
- GL3 skin vertex shader: 4 bone indices/weights, `u_boneMatrices[64]` (hard cap 64 bones, `gl3skin.cpp`), same lighting as default. Per-draw upload of bone matrices.
- **Missing hierarchy -> identity matrices** instead of crashing (librw 77b7229 for D3D9; same branch exists in gl3skin). Needed when a skinned clump is rendered before/without its HAnim hierarchy
  (cutscene heads, clump copies). SA? YES. Rec: assert-free identity fallback + a test that renders a skinned atomic with no hierarchy.
- **Weights are renormalised at model load** (re3 `ClumpModelInfo::SetClump`, 9b162554/a5447306 area): the shader assumes weights sum to 1, PC/Xbox skins can be off by rounding. SA? YES, PC peds.
  Rec: normalise on load (or in a geometry-instance callback) and add a debug check that flags sum != 1 +- 1e-3.
- **Fog uses the wrong clip coordinate in skin.vert** in the pinned librw snapshot (default.vert feeds clip w, skin.vert feeds clip z). Observed by reading both files; not found in re3's log.
  Rec: verify in our notsa fork; if present fix (visible as peds with wrong fog fade).
- hanim keyframe **stride**: re3 stopped using `sizeof` of the std keyframe struct and uses the animation's own `interpInfo->animKeyFrameSize` (8bbb6a2a); and the in-hierarchy
  interpolated frame is a *different, bigger struct on LP64* (pointers inside) — see 3.1. SA? YES, both. (User has uncommitted edits in AnimBlendNode.h / RpAnimBlend.cpp - likely this.)
- A new HAnim animation made "identity" must not have all keyframe times equal (division yields NaN) — re3 FIX_BUGS spreads times (RwHelper `HAnimAnimationCreateForHierarchy`). SA? check CAnimBlend identity/"static" anims.
- Skinned **cutscene head** vs player head are incompatible renderers (fab453ef, a5447306): head clump named like the player's must keep the special render callback unless the body is skinned; the head has no limbs (RenderLimb must not run). SA? cutscene heads (CCutsceneHead) yes. Rec: our cutscene (PROLOG1 already plays) – re-check Cutscene heads/hands with skin on.
- Weights/indices must be present in the *instanced* GL vertex layout: instancing happens per geometry the first time it renders; geometries edited after (re3 `InitClump`, material colour edits) need re-instancing. Rec: R-10 test list: ped, ped with clothes (CJ), vehicle damage parts (non-skinned), cutscene head, weapon models.

### 2.4 PostFX framework (R-11): colour filter, blur, heat haze, night/infrared, underwater distortion
re3 mechanism (`src/extras/postfx.cpp`, `src/renderer/MBlur.cpp`, shaders in `src/extras/shaders/*.frag`, librw hooks):
1. **Two off-screen "camera texture" rasters** (back buffer = this frame, front buffer = last frame), sized to the next power of two *above* the screen size in both axes (log2+1). Created lazily in `CPostFX::Open`; **must be closed and reopened on every
   resize** (RwHelper CameraSize ~line 504; our web port hit "picture froze when height crossed a power of two"). SA? YES.
2. **Frame grab** = `RwRasterPushContext(target); RwRasterRenderFast(cameraRaster, 0, 0); RwRasterPopContext()`. In librw GL3 that is a `glCopyTexSubImage2D` from the *currently bound read framebuffer*
   (`gl3device.cpp: rasterRenderFast`, copies the camera raster's width x height into the texture at a flipped Y). SA? YES: SA's CPostEffects does exactly this. Pitfalls: (a) if we render
   the scene into our own FBO (HiDPI scaling, MSAA resolve) the copy source must be that FBO; (b) on Retina the camera raster is pixel-sized (2x), so the POT texture doubles;
   (c) only CAMERA -> TEXTURE/CAMERATEXTURE copies are implemented, no other combos.
3. **Full-screen quad via Im2D** with a render-state set (no z test/write, no fog, nearest filter), alpha-blended overlay for simple colour tint. For the shader variants librw exposes
   `im2dOverrideShader` (librw 5e299fb): set a custom GL3 `Shader` (vertex = stock im2d.vert, fragment = effect) for the next Im2D draw, set uniforms, draw, clear the override. Effects are therefore
   fragment shaders sampling the grabbed raster; the "extended colour filter" = per-pixel blend with tint colour/strength; "mobile" = contrast mult/add pair.
4. **Motion blur** = previous-frame raster drawn with alpha; `bJustInitialised` guards the first frame (front buffer empty); front-buffer grab at end of frame only when needed.
5. **Half-pixel offsets** (1b561668): D3D9 needs -0.5; GL doesn't. re3 keeps the constant at 0 for librw and lets librw adjust Im2D coords. SA's D3D9 code already compensates in places; double compensation blurs text/HUD by 0.5 px.
   Rec: when we add the first full-screen pass, check 1:1 pixel mapping with a checkerboard test texture (any blur = half-pixel bug).
6. Shader sources are authored once as GLSL + HLSL and embedded as generated `.inc` (`make_glsl.sh` / `makeinc_glsl.sh`), so no runtime file IO. Rec: same for us (generated header in repo, script to regen).
SA-specific effects (heat haze, infrared/night-vision, underwater ripple, rain/fog overlays): SA's CPostEffects drives them with the same primitives (copy frame to raster, draw Im2D
meshes, vertex-coloured overlays, UV-offset grids), so **the Im2D + RwRasterRenderFast path is probably enough without new shaders** (our reading of SA, not from re3) — do the grab/restore
plumbing once (R-11a) and run the SA code as-is; add shaders only where Im2D fixed pipeline can't express it.
Skip: neo "screen droplets" (`screendroplets.cpp`, rain on lens) and neo pipelines (`custompipes*.cpp`, `shaders/neo*`) — re3 extras with no SA counterpart.

### 2.5 Offscreen cameras / real-time shadows — **[VC] is the template for SA's CRealTimeShadow/CShadowCamera**
- reVC `src/renderer/ShadowCamera.cpp` (548 lines) + `CutsceneShadow.cpp` (268) + `Shadows.cpp` (cutscene-ped shadow path, `USE_CUTSCENE_SHADOW_FOR_PED`, currently off in config.h
  because it requires COMPATIBLE_SAVES) implement the PS2 "shadow camera" technique: a small orthographic (parallel-projection) RwCamera per shadowed object with a
  CAMERATEXTURE colour raster + ZBUFFER raster of 2^n size; camera orientation copied from a directional RpLight, frustum from the object's bounding-sphere radius (far = 2r, near = 0.001r,
  view window = r), centre pushed back by half the far plane. Per update: clear (white, alpha 0), draw the clump/atomic with a **temporary geometry-flags edit** (strip prelit/lit/textured/modulate
  so it renders as a flat silhouette) and with the default (non-custom) atomic render callback, then an Im2D full-target quad with inverse-destination-colour blend to invert the silhouette.
  Post steps, each a *render-to-texture ping-pong*: (1) resample to half size, (2) N blur passes — bind raster A as source texture, draw a quad into camera B (swap the camera's raster with
  `RwCameraSetRaster`), 0.5-texel UV offset, (3) multiply by a gradient raster, (4) outline border. The result texture is projected as a normal static shadow.
  Commits: 1803dcc8 (2020-07-29 "miami shadows"), e2ded2d6, 89fd7759, **2f987e31 "update librw (shadows are working now)" = depends on librw a29bcef/b1507e5 (2020-08-10) custom render-target support for d3d9/GL**,
  5e299fb. SA? YES — this *is* SA's design (SA class names CShadowCamera/CRealTimeShadow/CRealTimeShadowManager). Rec: R-11b = port-by-test: (a) GL smoke: create camera with CAMERATEXTURE+ZBUFFER, clear, draw atomic, sample the raster as
  a texture in the main pass, `RwCameraSetRaster` swapping between two CAMERATEXTURE rasters; (b) check the librw GL camera-begin binds the FBO and sets viewport to the *raster* size (not window); (c) 0.5-texel offsets are D3D-specific
  and still required here because the original draws the same Im2D quads (don't "fix" them).
- Pitfalls specific to this path: geometry-flag edits must be restored (re-instancing: librw instances at first render, so changing flags/lighting after instancing must not leave a stale instance);
  per-frame creation of cameras is expensive (VC creates them once per shadowed object); clear colour alpha 0 vs white matters for the invert blend.
- [III+VC] Env-map camera precedent: `custompipes.cpp` CreateEnvMapCam (same CAMERATEXTURE+ZBUFFER pair, added to the librw World, `bRenderingEnvMap` flag makes the renderer skip things). Also needs the same GL render-target support.
- matfx env shader (`matfx_env.vert/frag`) + "fb alpha emulation" (2b797ae/6ff378b) exist; SA vehicles use matfx env + a specular pass (R-9, not re3's neo).

### 2.6 Shadows (static / pole / stencil)
- [III+VC] Blob/projected-texture shadows batched through `RenderBuffer` (`src/renderer/Shadows.cpp`), permanent/pole/static shadows keyed by `uintptr` IDs, 90689a17 "shadow fixes". Pickup reflection bug 581cb5ed (alpha-ref leak, see 2.2).
- **No SA-style stencil shadows in either game.** What exists: librw GL3 implements the stencil render states (STENCILENABLE/FUNCTION/PASS/FAIL/ZFAIL/REF/MASK/WRITEMASK; 9200810f "update librw with stencil states", 2020-12-16),
  and **librw 3c18728 forces D24S8** because the stencil buffer was missing. [VC] uses stencil for the colour-blur/motion-blur overlay (REPLACE on pass) and **688e277e "fix mblur stencil"**: under FIX_BUGS the per-frame camera clear
  adds the *stencil* clear (`rwCAMERACLEARSTENCIL`) — "no idea why it works in the original"; also used to clear water rects in the new renderer. SA? YES (SA stencil shadows + blur). Rec: request 8-bit stencil + 24-bit depth in SDL3 GL
  attributes (a core-profile macOS pixel format otherwise may lack stencil), clear stencil every frame, restore the stencil *write mask* before glClear (2.2), add a stencil smoke test (write ref, test NOTEQUAL) before wiring SA's shadow code.

### 2.7 Texture upload details that bit re3 on GL
- DXT: GL3 build converts/keeps S3TC per runtime caps (3e2080a, 7e80d45 "txd format changed!!", 4ac67e5 caps.dat); mip count off-by-one on `MIPMAP` rasters (207c2ee); destroyed rasters
  stayed bound -> use-after-free (8f1321f `evictRaster`). Rec: after any raster destroy, unbind from all texture units (we stream/destroy many TXDs).
- Loading is vsync-bound: re3 added `DISABLE_VSYNC_ON_TEXTURE_CONVERSION` and `DISABLE_LOADING_SCREEN` (config.h) because each loading-screen frame waited on vsync. Rec: disable swap interval while showing loading screens / texture conversion.

### 2.8 [VC] extras relevant to SA world/vehicle/ped rendering
- **Water as geometry + matfx**: `reVC src/renderer/WaterLevel.cpp` builds a "wavy" atomic (morph-target geometry rewritten per frame) and a "mask" atomic whose material uses the matfx **env-map** effect with a water env texture
  and a dedicated frame (RpMatFXMaterialSetupEnvMap) — proof that animated water + env reflections run on librw GL3 matfx. Fix history: 2fc6b542/84821807 (fine-block id stored as signed vs 0x80 sentinel: **plain `char` signedness bug**),
  595a000f (shutdown order: water atomics, textures, HUD re-init when the RW engine is re-opened), e0b3418e [III] UV scroll vs frame rate. SA? SA water is Im3D/sector based (R-11), so only the matfx-env and re-init lessons transfer.
  Rec: when water fails to show reflections check matfx env on a non-vehicle material first.
- **Pipelines + sorted blend pass**: `custompipes_gl.cpp` shows the librw-GL pipeline-authoring recipe: create an `ObjPipeline` with `defaultInstanceCB` (or `skinInstanceCB` for skinned atomics), `uninstanceCB = nil`, custom render callback that
  uploads uniforms then draws instance data; shaders are compiled from generated `.inc` strings; second UV set shader (`default_UV2.vert`, attribute slot TEXCOORDS1) exists. [III+VC] `Renderer.cpp` "new renderer"
  (e906a807, 6122f698, c58d98a3): first pass draws all opaque meshes and defers atomics needing blending to a sorted list rendered afterwards (AtomicFirstPass/RenderBlendPass, texture-transparency test per material). SA? YES: SA buildings have
  day/night colour sets (R-8 CCustomBuildingDNPipeline) and many alpha entities — this is the structure to copy for R-8/R-9 (instance with extra attribute, render CB, transparent deferral).
- **Skinned peds by default** (c17ccc50 removes the PED_SKIN ifdefs in VC): the whole path (SetClump weight renormalisation, hanim hierarchy flags UPDATEMODELLINGMATRICES|UPDATELTMS, bone-tag lookups) is exercised constantly in VC;
  `CPedModelInfo::CreateHitColModelSkinned` is where the original uses a wrong bone tag ("this is wrong" comment) — see 3.16.
- [VC] mblur/colour filter: `colourfilterVC.frag` differs from the III one by a doubled, clamped tint term and extra two additive taps inside the same 5-iteration loop that emulates the original's several stacked overlay passes in one
  fragment shader; V is flipped (1-v) because the grabbed raster is bottom-up. Rec: copy the idea (loop = N original passes), derive constants from SA's actual overlay passes, not from VC.
- [VC] Cutscene objects: back-face culling must be off/handled for cutscene objects (0e9a44dd); extra model flags for backface/alpha test from mobile (e97366c0 [III]); "Squalo render out poly" (355c260b) = renderer sort-order quirk. Low value.

---------------------------------------------------------------------------------------------------------------------------------
## 3. 64-bit (LP64 / LLP64) cross-check

Master list of what re3 had to touch, with SA relevance. Order = estimated risk for us.
| # | Item | re3 where / commit | SA? | Rec |
|---|---|---|---|---|
| 3.1 | **HAnim interpolated-frame struct contains pointers** (keyframe1/2) so offset+size differ on LP64; re3 introduced a 64-bit-correct `RpHAnimStdInterpFrame` and switched hierarchy math to `currentInterpKeyFrameSize` | b5b64ca3 (fakerw/rphanim.h, AnimBlendClumpData.h, FrameUpdate.cpp, Entity.cpp) | YES — skinned peds, CAnimBlendClumpData::frames[].hanimFrame | Verify every cast of `rpHANIMHIERARCHYGETINTERPFRAME` result; unit test with 2+ bones comparing x86 reference values. |
| 3.2 | **Save game: structs written/read raw** break on LP64 (pointers, size_t, long). re3's answer `COMPATIBLE_SAVES`: explicit field-by-field Save/Load for pools (74fde139), CPlayerInfo, vehicles (f372ce15, 18 files), script (fa29804c), zones (pointer<->index), camera; plus `FIX_INCOMPATIBLE_SAVES` (011aafa0, 2021-07-05 "finish") | `src/save/*`, `core/Pools.cpp`, `control/Script.cpp` | YES — SA saves (GTASAsf*.b) dump big structs (CPlayerInfo, CRunningScript, CVehicle/CPed state, cargens, ...) | Decide target: bit-compatible with Win32 saves (needs per-struct serialisers) vs native format only. If compat wanted, budget ~1 week and a round-trip test against a Win32-exe save. |
| 3.3 | Sizes written into saves must be fixed-width (a `sizeof` went in as 8 bytes) | 408f47fc "fixed linux saving" | YES | grep `Write*(…sizeof…)` in save code; static_assert widths. |
| 3.4 | Save-buffer alignment/UB: pools re-read for alignment, CPlayerInfo reworked, padding zeroed before writing for deterministic files | 4ac53960, 7aeac06f, 3f1001b8 (2020-05-13), 2071abd2 | YES | Use memcpy-based read/write helpers; zero scratch buffer before padding. |
| 3.5 | `RwMallocAlign` stored the raw pointer in a header sized by `align` instead of `sizeof(void*)` | c54d3ba2 (RwHelper.cpp) | YES if we own an aligned-alloc shim for streaming buffers | Check our `RwMallocAlign`/CdStream buffer alloc: header room >= sizeof(void*). |
| 3.6 | Pointer-valued RW render state cast to uint32 | c54d3ba2 | YES | see 2.1. |
| 3.7 | Streaming memory counters were int32/uint32: `ms_memoryUsed/Available`, `MakeSpaceFor(avail - size)` (unsigned underflow semantic), `RemoveReferencedTxds/DeleteRwObjectsBehindCamera(size_t)`; host memory query returned `unsigned long`; printf formats | 1dc6fbda (Streaming.cpp/.h) | YES — same CStreaming design in SA (SA: `ms_memoryAvailable`, `ms_memoryUsed`) | Keep game-visible arithmetic **32-bit and wrapping exactly as original** unless proven safe; widen only the host-derived budget, clamp it to the original maximum. |
| 3.8 | `fread/LoadFile/Read/Write` return `int` -> `size_t`; loop offsets `size_t` | 1dc6fbda (FileMgr, Replay, Directory, Camera, Ped*) | partly | Fine if our CFileMgr shim returns the same type as the hooked signature; watch signed/unsigned compare in `while(len>0)` loops. |
| 3.9 | IMG offset math: `sector * 2048` must be 64-bit (`lseek`), and "BIG_IMG" support for >2 GB | f442793d (CdStreamPosix.cpp, CdStream.cpp) | YES (gta3.img ~1.2 GB, mods bigger) | Verify our cdstream_portable uses `off_t`/`pread` with 64-bit multiply. |
| 3.10 | Object IDs derived from pointers (`(uintptr)this + n`) for coronas, static shadows, skidmarks, motion-blur streaks; re3 widened the ID parameter to `uintptr` everywhere | renderer/Coronas.cpp, Shadows.cpp, Skidmarks.h, SpecialFX.h | YES (SA: `CCoronas::RegisterCorona(uint32 id,…)`, shadows, skids) | Truncating to 32 bits is only safe while pool arrays are contiguous; prefer a 64-bit key or hash of (pool index, slot). |
| 3.11 | Zone/other structures that stored pointers in file-loaded data (child/parent/next as indices) relocated at load | Zones.cpp 770-810 | YES (SA zone/map zone arrays, path/nav nodes use ints) | Check any binary data file struct with pointer members (zone.dat is text; `*.dat` binary loaders in SA: paths NODES*.DAT, carrec, cullzones are plain ints). |
| 3.12 | CKeyArray / GXT: 32-bit file offset to chars was stored in a pointer field, so relocating "value" breaks; re3 stores the offset and computes the pointer on lookup | b21f49ad (Text.cpp/.h) | LOW for SA (CKeyEntry already has `uint32 offset`), check `CText::Get` | Skim our GXT loader for pointer-in-file-struct. |
| 3.13 | Audio: mpg123 reads return `size_t`; asserts if > 4 GB; `ssize_t` typedef collisions; `alDeviceName` pointer copy (strdup) | 1dc6fbda | different engine (SA uses own sound system) | n/a |
| 3.14 | `unsigned long` typed externs/`long` fields (host memory, timers) differ between LP64 and LLP64 | glfw.cpp, Streaming | YES | Prefer `uint32/int64` fixed types; add `-Wshorten-64-to-32 -Wpointer-to-int-cast -Wint-to-pointer-cast` as warnings in CI and triage once. |
| 3.15 | Debug `scriptToLoad` static default flipped (c54d3ba2) – dev loader only | n/a | n/a | — |
| 3.16 | **[VC] FPEs / NaN**: skinned ped hit-colmodel used a bogus bone tag whose lookup yields garbage -> floating-point exception on traps (VC 97008905 sets sphere centres to origin); particle screen-Z divides by zero (9382e86d); collision NaN/inf (be88a42b) | VC PedModelInfo.cpp, Particle.cpp, Collision.cpp | YES (SA's `CPedModelInfo` has the same hit-col build; particles & collision share code) | Run a debug build with FP traps (`feenableexcept` on Linux; on arm64 enable via FPCR) for the 900-frame trace; fix with the same "FIX_BUGS"-style guards only where output is NaN-poisoned. |
| 3.17 | Float->int conversion of NaN/out-of-range differs between x86 (cvttss2si -> INT_MIN) and ARM64 (saturate, NaN->0). Decomp code casting unchecked floats (`(int)x`, `(uint8)(x*255)`) may diverge. (Our inference; re3/VC only hit it indirectly via the FPE/NaN fixes above and "floats rounded" cleanups.) | n/a | YES | Provide a helper for the hot spots flagged by FP-trap run; the trace diff is the judge. |
| 3.18 | **[VC] enum widths & signed bytes**: `int8`/`uint8` enums fixed (912e71be, 4c074426) so struct layouts match; sentinel `0x80` compared against `uint8` vs `int8` (water fine-blocks 2fc6b542); `char` stats (c2672bd6) | VC | YES | Check enum-in-struct sizes with static_assert(sizeof) per saved struct; use `int8_t/uint8_t` not `char`. |
| 3.19 | **[VC] Garage save block wrote uninitialised bytes** (76558e93); work buffer zeroed before padding (2d38eb92, III 2071abd2) | VC saves | YES | Zero-init all save scratch buffers; compare two consecutive saves for byte equality. |
Process lesson: re3 got 32->64 *compiling* in ~2 days (1dc6fbda, 2020-07-22 plus earlier May fixes), but hot bugs (hanim, saves, pools) surfaced over the next 12 months on Linux. Plan an explicit 64-bit audit slice rather than waiting for crashes.

---------------------------------------------------------------------------------------------------------------------------------
## 4. Platform polish (patterns to copy / avoid)

### 4.1 Gamepad
- re3 uses the GLFW gamepad API (SDL mapping DB under the hood): ships `gamecontrollerdb.txt` next to the exe, loads it at pad init, then **overlays the `SDL_GAMECONTROLLERCONFIG` env var** (122c7aa4);
  falls back to raw buttons/axes for unmapped devices. `_InputInitialiseJoys` / `CapturePad` in `skel/glfw/glfw.cpp`.
  SDL3: `SDL_AddGamepadMappingsFromFile` + env var are built in. Rec: ship the DB, load before first `SDL_GetGamepads`.
- Hot-plug (226e3b83): connect assigns pad 1/2 if free, disconnect frees; **default bindings are only extended** for newly-seen button counts (not reset), otherwise a different pad wipes user bindings.
- **Premature settings save wiped bindings** (747ae042): saving the ini before loading controller sections. Rec: never write settings until load finished.
- Blacklist: wireless keyboard dongles exposing "joystick" HID nodes (f5345f59) -> only treat devices with a gamepad mapping (or user-confirmed) as pads.
- Triggers: GLFW reports 0.0 for a missing axis, so re3 only updates a trigger button when the axis is non-zero and treats > -0.8 as pressed (cb34060f). SDL3 gives proper 0..max; use threshold ~25-30%.
- Sticks: hard 0.3 deadzone then scale by 128; discontinuity near centre. Rec: radial deadzone with rescale, clamp to +-128 as SA's CPad expects.
- Rumble: desktop builds do nothing; only the Switch port implemented it. SDL3 `SDL_RumbleGamepad` is cheap: map CPad shake freq/duration.

### 4.2 Keyboard layouts
- GLFW keycodes are physical-ish US positions; re3 hit **input delay/ordering issues** and ignored GLFW repeat events (8846f50c); on Linux it polls X11 key state directly
  (bf7280b5; `XQueryKeymap` + keysym table, layout-aware) — X11-only hack, do not copy.
- Shift state is tracked separately for left/right because the original control config asks for it (`_InputTranslateShiftKeyUpDown`).
- No text-input path: names/cheats read letter key states. SA has only cheat strings and menu bindings. Rec: SDL3 gameplay input by **scancode** mapped to the original key codes (layout independent; AZERTY/Cyrillic users keep WASD positions),
  rebinding screen labels from `SDL_GetKeyFromScancode`. Cheat strings: decide scancode (physical QWERTY) vs keycode; original game used VK letters => keycode of Latin letters; with non-Latin layouts fall back to scancode.

### 4.3 HiDPI / Retina, mouse
- 5654347c: use the **framebuffer-size** callback (pixels), not window size; librw reads `glfwGetFramebufferSize` for the camera raster (the SDL2 branch of librw reads *window* size = bug on Retina; ours must use `SDL_GetWindowSizeInPixels`).
- b95accb8: mouse coordinates must be scaled by framebuffer/window ratio (cursor stuck in a quarter of the window on HiDPI). re3's `cursorCB` also rescales to `RsGlobal.maximum*` and only matters in the frontend.
- 0205960a/3b1debaa/f33ed289: cursor mode = *disabled* (relative) in game and in fullscreen menus, *hidden* only in windowed menus; toggled when the menu opens/closes and re-asserted every loop iteration. Pointer warping
  (recentring) is skipped on GL builds. SDL3: `SDL_SetWindowRelativeMouseMode` on menu transitions; no warping.
- Rec: after any `PIXEL_SIZE_CHANGED` / content-scale change recompute camera raster + postfx buffers (2.4) and the cursor scale.

### 4.4 Resize / window move / fullscreen
- Resize: `resizeCB` (framebuffer-size) redraws one frame if playing, then (if w,h > 0) updates `RsGlobal.maximum*` and sends the camera-size event (zbuffer + postfx rebuild). Ignore w/h == 0 (minimise). Known artefacts while dragging (TODO in source).
- Window move between monitors: **not handled** — re3 chooses `monitors[0]` at start; "subsystem" in the menu = monitor index and changing it re-initialises the engine. Rec (ours): handle `SDL_EVENT_WINDOW_DISPLAY_CHANGED` + `DISPLAY_SCALE_CHANGED`; refresh vsync/limiter and recompute scale; no engine re-init needed.
- Fullscreen: re3 uses exclusive video modes from GLFW (mode list built, deduped, highest refresh kept: 1b40693f), persisted as width/height/depth/windowed in the ini (`IMPROVED_VIDEOMODE`); every
  mode change = full RW close/open + frontend texture unload/reload (`_psSetVideoMode`). Slow and fragile. Rec: borderless desktop-fullscreen as default (no mode switch, no engine re-init), exclusive as option.
- Default resolution: native desktop size (DEFAULT_NATIVE_RESOLUTION) when nothing saved.
- Iconified/background: skip frontend idle while iconified; pause while not foreground (loop falls through to a "camera begin/end" probe until window usable).

- [VC] adds: GLFW cursor-disabled fix (1544acff, same as III 0205960a), Linux joystick handling enabled (91902a03), pad vibration setting saved (5f3e68ec), `NoMovies` ini option + "always save ini after loading" (781617d4 — note the later III fix 747ae042 reverses the *order* problem). Hi-dpi: see 1.1 delta (3d5f0ef3).

### 4.5 Frame limiter / vsync
- re3: limiter is an ini/menu flag + `RsGlobal.maxFPS` (default 30 for III); main loop busy-polls events and runs the frame only when elapsed ms exceed 1000/maxFPS (frontend capped at 100 fps, 39c9a058). Vsync flag is applied on **every
  present** by calling the swap-interval API inside `showRaster` (librw). No sleeping => 100% core. Menu/pause have separate rates.
- A decade of "high FPS" fixes shows what breaks when frame time shrinks: water UV (e0b3418e), cloud rotation + braking (9b09aa33), script waits (746f2aa4, 2588e698), police scanner (a446dbef), jittery cars at 15 fps (37b6d491), FPS counter (main.cpp FIX_BUGS block).
- Rec: keep SA's 30 fps limiter semantics as default (bit-identical timing for the traces), implement a sleep+spin limiter; set swap interval only on change; test at 60/144/uncapped with a timer-sensitive scene (water, clouds, scripts using timers).

### 4.6 Save / settings directories
- re3 non-Windows: `./userfiles` created under cwd (`_psGetUserFilesFolder`), `gta3.set` / `re3.ini` / saves live there or beside the exe; Windows: `Documents\GTA3 User Files` from the registry. Portable by design, not OS-conventional.
- Rec: `SDL_GetPrefPath("…","gta-sa")` (macOS `~/Library/Application Support`, Linux `$XDG_DATA_HOME`, Windows `%APPDATA%`) for saves/settings/screenshots, with: (1) env/CLI override, (2) legacy fallback to the game dir if files exist there,
  (3) create dirs with mode 0755 (re3's mkdir used `realpath` first — fails on nonexistent paths).
- Memory budget from OS: re3 reads free RAM (Linux `sysinfo.freeram` unscaled by `mem_unit`; macOS `vm_statistics.free_count` x page size — free pages exclude cache, so tiny) and derives streaming budget with a 50 MB floor. Rec: derive from total RAM and clamp (see 3.7).

---------------------------------------------------------------------------------------------------------------------------------
## 5. Pitfalls re3 hit that we may not have hit yet (mined from `git log`)
| # | Pitfall | Evidence | SA? | Rec |
|---|---|---|---|---|
| 5.1 | **Locale**: `setlocale(LC_ALL,"")` (used for language detection) switches decimal separator and case rules; re3 resets CTYPE/COLLATE/NUMERIC to "C" right after (glfw.cpp InitialiseLanguage tail). Comma-decimal locales break every `sscanf("%f")` in .ide/.dat/.ipl parsing; Turkish locale breaks `strcasecmp("I")` | 8a4fa58c; ASCII_STRCMP (c202fc3b) | YES | Never call setlocale(LC_ALL,""); use `SDL_GetPreferredLocales`; add a test running with `LC_ALL=de_DE.UTF-8` / `tr_TR.UTF-8`. |
| 5.2 | Text-mode file reader sign-extends `char` (bytes > 127 become negative ints; `EOF` collisions). `char` is unsigned by default on Linux ARM | 8c170a62 (FileMgr myfread/myfwrite -> unsigned char) | YES (language files, IDE comments) | Use `unsigned char` in our CFileMgr text path; add `-funsigned-char`-agnostic test. |
| 5.3 | **Named POSIX semaphores on macOS** with a fixed global name: second instance/crashed run collides; buffer overflow in name formatting (19b39f49); re3 later replaced the whole POSIX streamer design (8c170a62, 843dd9b5): thread-per-channel option, `flushStream` flags set when the request list is flushed so a blocked read is abandoned, SIGUSR1 dummy handler to interrupt blocking reads | CdStreamPosix.cpp | YES (we have our own CdStream) | Use mutex+condvar (we do); make sure `FlushRequestList` aborts an in-flight read; test cancel while reading. |
| 5.4 | **.ini written before load** wiped controller bindings; first run must create it after load | 747ae042, 226e3b83 | YES | Order: load -> merge defaults -> save. |
| 5.5 | Uninitialised data differs between platforms: Windows heap/pools were often zero, glibc/macOS malloc is not (uninit near-clip, UB fixes) | 64a4ed6d, 3f26250d, 9938d04c, 1a8a1c91, d52b917c | YES (decomp code relies on zeroed pools) | Zero-fill pool storage at init (calloc/mmap), run ASan+MSan/valgrind once on Linux. |
| 5.6 | Dangling pointers hidden by MSVC's layout surface under GCC -O2 (ped near lists, PedIK ctor with null ped, `IsPointerValid`) | 424a6d90, 55b62e21 | YES | ASan + `-O2` + replay of the 3k-frame trace on Linux CI. |
| 5.7 | `printf`-family with `size_t`/`uint64`: wrong `%d/%u/%llu` corrupts output or reads garbage (several 64-bit commits switch format per `__LP64__`) | 1dc6fbda | YES | Enable `-Wformat=2` as error. |
| 5.8 | MSVC sized dealloc vs pooled class `operator delete(void*,size_t)`; also pooled-class `operator new` default alignment | f73450f5 | YES (Win x64 only) | Add `/Zc:sizedDealloc-` in the Windows leg. |
| 5.9 | Fixed-size path buffers (`MAX_PATH` 260 on Windows vs 4096/1024 on POSIX): MP3 finder stack overflow (22e8e0ef), save-name buffers (3383d8c6), `casepath` output size (98ec7bda), `FindFirstFile` struct sizes | several | YES | Use PATH_MAX-sized buffers wherever a resolved/absolute path is stored (home directories are long on macOS). |
| 5.10 | Audio (OpenAL): channel buffers owned per channel (4cfb3b09); delete buffers only after the source is stopped/unqueued, timers must be signed (d94b36c2); mono 2D sources sound quiet/spatialised -> duplicate to stereo for non-positional sounds (4a9d890a); context created with explicit mono/stereo source counts (sampman_oal.cpp ~812); EFX only if extension present | listed | partly (our DSound emulation) | Check our dsound_al for 2D sources set `AL_SOURCE_RELATIVE` and stereo, and for delete-while-playing. |
| 5.11 | `wchar_t` is 4 bytes on POSIX, 2 on Windows — GXT/text needs a 16-bit type (`AllocUnicode` helper in crossplatform) | b21f49ad, crossplatform.cpp | YES | Use `char16_t`/uint16 throughout text code; static_assert. |
| 5.12 | GL state leaks: `glBindTexture` of a destroyed raster (8f1321f); blend enable passed constant true instead of the flag (fbb2d35 - always-blend bug); MSAA hint; depth mask on clear | librw log | YES | Add GL debug-output (KHR_debug) in debug builds (librw ad9954f added the loader) and fail tests on any GL error. |
| 5.13 | Resource ids assumed small ints: `uint8` param receiving id >= 128 (Garages) etc. found by UBSan | 8c170a62 | YES | One UBSan (integer + local-bounds) run of the smoke suite on Linux. |
| 5.14 | Settings: persisted video mode as indices broke when mode list changed -> re3 stores width/height/depth (IMPROVED_VIDEOMODE) | config.h | YES | Persist pixel size + fullscreen kind, not an index. |
| 5.15 | CI hygiene: Ubuntu 18.04 runner image retired, conan recipes (bincrafters/bintray) disappeared; their CI broke every few months | 4b2494c, 306aba8b | n/a | Pin runner (ubuntu-24.04), pin third-party versions, cache; do not depend on an external conan remote. |
| 5.16 | **[VC] Re-opening the RW engine (MSAA / windowed<->fullscreen change) breaks dependants**: water sound (3988fec6), water atomics/textures/HUD must be re-created (595a000f), postfx buffers (2.4) | VC | YES (our menu "apply display settings" would trigger it) | Avoid engine re-init for display changes (borderless fullscreen + resize path); where unavoidable keep a single `OnDeviceReset` hook list (postfx, shadow cameras, water, HUD, audio sources). |
| 5.17 | **[VC] Audio**: PS2-style channel counts raised (02655313), separate buffer for player comments to avoid overflow (669c8c89), several "oneshot"/heli/boat audio crashes (3a7cb9c5, 296e7ed9, 64269638), uninitialised audio fields (6753d39d, 1169437e) | VC | different engine (SA uses its own sound code) but our DSound->OpenAL emu sees the same buffer-lifetime hazards | Fuzz our emulated buffer API with short sounds + rapid stop/start under ASan. |
| 5.18 | **[VC] Bike/heli physics crashes were compiler-specific**: `optimize off` pragma on CBike::ProcessControl (3b2558bb) to dodge a compiler bug; "heli buoyancy", "falling from bike while shooting" crashes | VC | YES (SA bikes/helis/boats share this physics) | If a Linux/mac -O2 build misbehaves only in CBike, try `-fno-strict-aliasing`/per-function `optnone` before deeper debugging. |
Things NOT worth copying: Switch/PS2/MSS paths, X11 polling, premake/autoconf, neo pipelines, Miles audio, debug menu, web port (user's own branch `web-*`).

---------------------------------------------------------------------------------------------------------------------------------
## 6. Prioritised next slices (Mac/Linux work)
Sizes: S = <= half day, M = 1-2 days, L = 3-5 days of agent time.
| Pri | Slice | Content | Size | Why now |
|---|---|---|---|---|
| 1 | **LNX-1 Linux x64 build + CI** | CMake target for `source/portable` on ubuntu-24.04 (gcc + clang legs), deps via apt/FetchContent (SDL3, OpenAL-soft, librw notsa), `-ffp-contract=off`, `-fno-delete-null-pointer-checks` considered, noexcept pool `new`, case-insensitive path test on ext4; job = build + smoke + 900-frame trace diff vs Mac reference; artifact upload | M-L | Unblocks everything on the fork; surfaces GCC UB class early (5.6) |
| 2 | **X64-AUDIT 64-bit cross-check** | Warnings gate (`-Wshorten-64-to-32`, pointer<->int casts, `-Wformat=2`), census of pointer-in-uint32, hanim interp frame audit (3.1), RwMallocAlign/render-state casts, IMG 64-bit offsets, ID widening (3.10), streaming counters policy (3.7); deliver table + fixes + unit tests | M | Cheap, finds the crash classes re3 spent a year on |
| 3 | **SAVE-LP64** | Decide format policy (Win32-compatible vs native); implement field-wise serialisers for the structs that carry pointers/size_t; round-trip test with a Win32-exe save; fixed-width size fields, zeroed padding | L | Saves are the first thing a player notices; re3 needed ~1 week of focused work |
| 4 | **R-10 skin audit** | Identity fallback without hierarchy, weight renormalisation, >64-bone guard, fog coordinate (z vs w), hanim stride, cutscene head/hands, CPU-vs-GL skin numeric compare on 5 clumps | S-M | Peds are everywhere; bugs here read as "game broken" |
| 5 | **R-12 lighting A/B** | CPU reference of D3D9 FF lighting (ambient/dir/point/spot/emissive-from-prelit/MODULATE/clamp/material order) vs GL shader, 20 synthetic cases; then shader fixes (material multiply order, attenuation) | M | Foundation for every visual comparison; cheap to automate |
| 6 | **R-11a PostFX plumbing** (+ **R-11b shadow-camera RTT smoke per [VC] ShadowCamera**) | POT camera-texture pair + `RwRasterRenderFast` grab on GL (HiDPI/FBO-aware), Im2D full-screen pass with override shader, resize reopen, half-pixel checkerboard test, then colour filter / night+infrared / heat haze / underwater by running SA's CPostEffects; D24S8 stencil attribute + stencil smoke test | M-L | Unlocks R-11 effects and real-time shadow raster path |
| 7 | **PLAT-1 SDL3 polish pack** (split in 3 PRs): (a) HiDPI + display-change + resize + relative mouse/cursor modes + borderless fullscreen; (b) gamepad DB, hot-plug, binding merge, triggers, radial deadzone, rumble, scancode keyboard; (c) pref-dir for saves/settings + sleep-based limiter + vsync-on-change | M each | User-visible quality; mostly independent of render work |
| 8 | **WIN-1 Win x64 CI** | Same CMake on windows-2022 (MSVC, `/Zc:sizedDealloc-`, static CRT, `/fp:precise`), SDL3/OpenAL DLL bundle, artifact | S-M | After LNX-1; reuses its scripts |
Added from reVC: R-11b is the first consumer of librw GL render-target cameras (2.5) — if its smoke test fails, fix librw before touching SA shadow code; a debug FP-trap run (3.16) should be added to X64-AUDIT.
Dependency notes: LNX-1 -> WIN-1; X64-AUDIT feeds SAVE-LP64; R-10/R-12 are independent of platform slices; R-11a should land after PLAT-1(a) if the HiDPI/FBO path changes the grab source.
