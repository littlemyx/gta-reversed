# MAC-GLSTATE: raw GL state vs librw gl3 caches (2026-10-25, portable/main)

Scope: every direct gl* state call in vendor/librw/src/gl/* (gl3 files; wdgl.cpp is the PS2 WDGL backend, not built for GL3) and source/portable/render/*, tests/ and tools/ checked by grep for the same families. Caches in librw: `oldGlState` (blend/depth/stencil/cull, lazy flush), `boundTexture[]`/`activeTexture`, `currentFramebuffer`, `currentShader`, `glGlobals.present*` (viewport), per-raster `filterMode/addressU/V/maxAnisotropy`, `uniformStateDirty`. Not cached (no cache to go stale): scissor, glColorMask (never used by librw), glClearColor/Depth/Stencil, pixel store, VAO (RW_GL_USE_VAOS is off), array/element buffers, UBOs (RW_GL_USE_UBOS off).

Commits: librw `notsa` (fixes), gta-reversed pin + test + report (hashes in the last section).

## Table
| call site | cached? | verdict | fixed? |
|---|---|---|---|
| gl3device flushGlRenderState: glEnable/Disable BLEND/DEPTH_TEST/STENCIL_TEST/CULL_FACE, glBlendFunc, glDepthFunc, glDepthMask, glStencilFunc/Op/Mask, glCullFace | yes (old vs cur) | the only writer of these; init memset 0xFE forces a full first flush | ok |
| gl3device clearCamera glDepthMask x2 | bypassed | stale (INT1) | fixed in INT1 (c7b8750) |
| clearCamera glStencilMask(0xFFFFFFFF) ... glStencilMask(oldGlState.stencilWriteMask) | bypassed but restores the cached value | consistent (also before the first flush: old holds the 0xFE.. value that was just set) | ok |
| clearCamera glScissor + glEnable/glDisable(SCISSOR_TEST) | not cached | disabled unconditionally at the end (would kill a foreign scissor) | fixed: previous enable + box restored |
| clearCamera glClearColor | not cached | stateless for rendering | ok |
| setDepthTest/setDepthWrite (ztest/zwrite -> DEPTH_TEST+DEPTHFUNC+MASK) | yes | traced all 4 combinations incl. ZTEST 0 + ZWRITE 1 (ALWAYS) and back: consistent | ok |
| setActiveTexture / bindTexture (glActiveTexture, glBindTexture) | yes | OK while the bound texture is alive; deleted bound texture left its name in `boundTexture[]` (GL unbinds, glGenTextures reuses the name, the bind of the new object is skipped, glTexImage2D hits texture 0 = GL_INVALID_OPERATION) | fixed: `forgetTexture()` in raster destroy and device stop |
| device start (resetRenderState): bind loop `bindTexture(whitetex)` | yes | stale caches of a previous run (restart, fresh context, new names) skip the binds | fixed: caches invalidated before the loop, fbo 0 forced |
| gl3raster create/lock/unlock temporary `bindTexture(prev)` pairs | yes, restore previous | consistent | ok |
| gl3device rasterRenderFast: setActiveTexture(0), raw glBindTexture(dst) ... glBindTexture(boundTexture[0]) | restores cached | consistent for the texture; but glCopyTexSubImage2D reads the READ fbo = whichever is bound (a camera texture while one is the target) -> copied black | fixed: bind the source raster's fbo, restore after |
| gl3device setFilterMode | per-raster cache | `natras->filterMode != filter` correct | ok |
| gl3device setAddressU / setAddressV | per-raster cache | **inverted test** (`==`, upstream typo): a TEXTUREADDRESS change while the raster stays bound never reached GL (only a re-bind applied the stage mode). SA sets clamp/wrap after the raster (Radar, Shadows, ...) | fixed (`!=`) |
| bindFramebuffer / currentFramebuffer | yes | destroy of a camera texture whose fbo is bound: GL goes to 0, cache kept the dead name (next fbo with the same name never bound) | fixed: `forgetFramebuffer()` |
| gl3raster rasterCreateCameraTexture: bindFramebuffer(fbo) ... bindFramebuffer(0) | yes | cache consistent, but creating a raster while rendering into another camera texture silently redirected the following draws to the window | fixed: previous fbo restored |
| gl3raster destroy ZBUFFER: bindFramebuffer(oldfb) + detach | yes | left the camera texture's fbo bound | fixed: previous restored |
| gl3raster lock GLES path: bindFramebuffer(fbo) ... (0) + glDeleteFramebuffers | yes | same two issues (GLES only, not exercised on macOS) | fixed: previous restored, name forgotten |
| gl3raster lock CAMERA: glReadBuffer(GL_BACK) + glReadPixels | not fbo-aware | reads the bound fbo (camera texture bound -> GL error + wrong pixels) | fixed: window fbo bound for the read, restored |
| gl3raster lock CAMERA size = `glGlobals.present*` (last viewport) | - | after a camera-texture update this is the texture size, not the window: wrong/overrunning read | OPEN (GLSTATE-1) |
| gl3device setFrameBuffer: glFramebufferTexture2D/Renderbuffer on the bound fbo | yes (bindFramebuffer first) | consistent | ok |
| gl3device setViewport / glViewport | `present*` cache, reset to 0 at start/stop | only writer of glViewport (grep: none in portable/ or tests/) | ok |
| gl3shader Shader::use (glUseProgram) / Shader::create (glUseProgram program, restores currentShader) | yes | consistent; **destroy left `currentShader` dangling**: a new Shader at the same address was never `glUseProgram`ed | fixed (cleared in destroy; also covers the stop path that destroys the default shaders) |
| gl3immed / gl3render / gl3pipe / gl3skin glBindBuffer, glBindVertexArray, attrib pointers | no cache, VAOs off | element/array buffer bound before every draw; enable/disable balanced per draw (Im2D) or Transform/End (Im3D); portable layer pairs Transform/Render/End | ok |
| gl3device UBO glBindBuffer (RW_GL_USE_UBOS off) | no cache | dead code in this build | ok |
| uniform-side caches (alphaRef/func, fog start/end/enable/colour: `uniformStateDirty`, shader serial numbers) | yes | every setter marks dirty; ALPHATESTREF lazily; GSemu draw restores func/ref/zwrite; fog colour order covered by the pipe test (RE3 f027c3af) | ok |
| portable raster_gl3.cpp NotsaGl3DeviceClear: bindFramebuffer via librw + glScissor/ColorMask | yes / saved+restored | binds with librw's cache, restores fbo, scissor, box and colour mask | ok |
| portable camera_gl3.cpp ReadBackBuffer / DefaultFramebufferStencilBits: raw glBindFramebuffer(0) | bypassed, restored | prev read with glGetIntegerv and rebound, cache never observes the change | ok |
| portable engine_gl3.cpp NotsaRwGetGlInfo: raw glBindFramebuffer(0), never restored | bypassed | left the default fbo bound when called mid-frame (cmd:gfxq) | fixed: restored |
| portable camera_gl3.cpp RwCameraClear glClearDepth/glClearStencil | not cached | set before every clear that uses them | ok |
| portable pipelines_carenv_gl3.cpp / im2d_im3d_gl3.cpp INT1 diagnostics (queries, glReadPixels, glGet*) | read-only | no state writes | ok |
| portable texture_gl3.cpp glTexParameteri(MAX_LEVEL) | wrapped in bindTexture(prev) | consistent | ok |
| tests/portable/{gl_smoke,rw_*_gl_test}.cpp raw glClear/glColorMask/glStencilMask | - | test-only, they re-sync (flush) where it matters (gl_smoke.cpp:224) | ok |

## Tests (tests/portable/rw_pipeline_gl_test.cpp, `RunGlState`, `GLSTATE_ONLY=1` to run alone)
20 new checks (pipe test: 187 -> 207 ok, 0 failed; run_gl_smoke SMOKE RESULT OK).
Before the librw fixes (same exe otherwise, with only the new accessors stubbed): FAIL on TEXTUREADDRESS wrap while bound (blue instead of red), fbo cache after destroy (cache 1 / GL 0), camera texture create/zbuffer create/destroy redirecting the target (binding 0 vs 1), camera lock and RenderFast reading the camera texture instead of the window ((0,0,0) vs (40,60,90)), current-shader cache after destroy, plus GL errors 0x501/0x502. After: all ok. The deleted-bound-texture check did not fail before on this driver (the cache happened to be corrected elsewhere) - kept as a guard for the defensive `forgetTexture`.

## Open
| id | what |
|---|---|
| GLSTATE-1 | Raster::lock on CAMERA uses `glGlobals.presentWidth/Height` (last viewport) as the window size; should be the window pixel size (like getFramebufferRect). Only the screenshot/lock path; not changed here to keep the fix set cache-only |
| GLSTATE-2 | scissor/colour-mask are not cached at all; any future raw `glColorMask` would silently change glClear (clearCamera) - keep save/restore discipline as in NotsaGl3DeviceClear |
| GLSTATE-3 | no game run (display inactive); the fixes are exercised by the pipe/smoke tests only. Likely visible effects in game: radar/shadow tiles with the clamp mode set after the raster; mirror/water camera-texture paths that create rasters mid-update |

## No-op proof (Win32)
Touched: librw `src/gl/{gl3device,gl3raster,gl3shader}.cpp`, `src/gl/rwgl3.h` (compiled only for LIBRW_PLATFORM=GL3); `source/portable/render/engine_gl3.cpp` (portable-only glob); tests/, .notes/. No MSVC-compiled TU changed.
