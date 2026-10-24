# MAC-R3a: slice R-3a, engine / window / GL context / smoke app (2026-10-23, branch portable/main)

Commits (gta-reversed, pushed to fork): ae9f38b9 (submodule bump only), 5fa35ddc (code). librw fork littlemyx/librw `notsa`: 7178c5b (pushed; f358ffd..7178c5b).

## Census (PortableMac native, tools/portable/snap_census.sh on HEAD 5fa35ddc, compdb adjusted for platform.cpp out / engine_gl3+platform_gl3 in; TSV .notes/w0/r3a_after.tsv)
| item | R-2 after | R-3a after |
|---|---|---|
| total unique diagnostics | 655 | 462 |
| R (r_census.py) | 344 | **152** (camera 61, raster 33, renderstate 52, Pipelines 1 = P cast, Gamma/Radar 5 = F asm) |
| standalone/rw/engine.cpp | 179 | 0 |
| standalone/rw/platform.cpp | 13 | 0 (file excluded) |
| WinMain.cpp, portable/render/* | - | 0 |
Plan target "R <= ~160": met.

## Smoke app: runs on this Mac (M5 Max, macOS 26.7.1), exit 0
`tools/portable/run_gl_smoke.sh` builds librw (GL3+SDL3, brew SDL3 3.4.12) + engine.cpp + rwglobals.cpp + engine_gl3.cpp + platform_gl3.cpp + tests/portable/gl_smoke.cpp with PortableMac flags (no PCH, no game link) into build/gl_smoke, runs it. Output (verbatim excerpt):
```
window created: cocoa     sub system 0: Built-in Retina Display, 24 video modes (mode 0 desktop windowed 1512x982, 1.. exclusive, 23 = synthetic windowed 640x480)
[log] GL: Apple | Apple M5 Max | 4.1 Metal - 90.5
[log] GL: GLSL 4.10 | context 4.1 core profile | default framebuffer: color 8 bit alpha 8, depth 32, stencil 8 (SDL reports depth 32 stencil 8)
[log] GL: S3TC 1 | max anisotropy 16.0 | max samples 8
window 640x480 points, 640x480 pixels (1x backing store)
frames 0..4: clear colour read back == expected (5 colours); Im2D triangle covers centre, leaves corner (frames 1, 4);
stencil clear 0x5A read back = 0x5a, depth 0.5 read back = 0.5000; glGetError clean after start / Im2D / frames; RwEngineStop/Close/Term ok; adopted window survived
SMOKE RESULT OK   exit code 0
```
Facts that were UNVERIFIED in the plan and are now verified: macOS returns 4.1 core for a 3.3 core request; S3TC is exposed; all gl3 shaders (default x4, im2d, im3d, skin, matfx) compile (no "Error in ... shader" on stderr, script greps it); depth 32 / stencil 8 default framebuffer.
Mutation: the same smoke against the unpatched librw (f358ffd) FAILS ("librw adopted the host window", stencil 0).

## Changes
* vendor/librw gl3device.cpp (startSDL3/stopSDL3 only, SDL3 branch): adopts a non-nil `*EngineOpenParams::window` (nil = old behaviour: librw creates its window); sets double buffer, RGBA 8888, depth 24, stencil 8, MULTISAMPLEBUFFERS (SDL ignores the sample count without it) before the context; context created per profile (core 3.3 -> 2.1 -> ES), falls through on failure instead of crashing on a nil context; exclusive mode applied to an adopted window; stopSDL3 does not destroy an adopted window.
* source/standalone/rw/engine.cpp: D3D9 parts under `#ifndef NOTSA_PORTABLE` (9 own-line pairs); neutral parts (alloc/string tables, Init/Stop/Close/Term, Version, counts) stay shared.
* source/standalone/rw/platform.cpp: not edited, excluded from the portable build (CMake PORTABLE_EXCLUDED_RENDER_FILES) -> portable/render/platform_gl3.cpp (restore callback stored and never fired, RpAnisotPluginAttach, no-op hooks).
* portable/render/engine_gl3.cpp(+.h): RwEngineOpen (SDL_Window* as displayID, failure rollback), RwEngineStart (context check; librw drops DEVICEINIT's result), sub-system/video-mode API on SDL displays (own SDL_GetDisplays name lookup: librw passes an index where SDL wants an id), windowed-run synthetic mode (NOTSA_STANDALONE_WINDOWED=WxH, DEFAULT 1280x720 in the portable build, "0/off" = real modes), RwD3D9* API (ChangeVideoMode live via SDL fullscreen/size), `NotsaRwGetGlInfo` (+ log line at start).
* WinMain.cpp: `SDL_WINDOW_OPENGL` in the portable build only (3 lines).
* tests/portable/gl_smoke.cpp, tools/portable/run_gl_smoke.sh (LIBRW_SRC / OUT overridable).
Symbol check: every public symbol of the old engine.cpp + platform.cpp is defined (engine.cpp portable view + engine_gl3 + platform_gl3), nm -C compared; rw_symbol_audit.py --check rc 0.

## No-op proof
* librw: gl3device.cpp is `#ifdef RW_OPENGL` only; preprocessed output (clang -E -P) for -DRW_D3D9, -DRW_NULL, -DRW_D3D9 -DLIBRW_SDL3 and no define is byte-identical f358ffd vs 7178c5b (md5 9121f1f167), differs only with RW_GL3+LIBRW_SDL3.
* gta-reversed A=ae9f38b9 B=5fa35ddc (sub-agent, /private/tmp/claude-501/r3a_ab/REPORT.md): StandaloneRelease 1281 TUs both sides, 4668/4668 compile commands identical, build.ninja identical (20,401,377 B), new files absent from the Win32 compdb/ninja. wine cl.exe on engine.cpp and WinMain.cpp: WinMain identical in everything non-debug; engine.cpp .text/relocs identical, the ONLY diff is the `__LINE__` string literal from rwNewT/RWHERE ("engine.cpp line: 206" vs 211, +5 guard lines; COMDAT name hash) = neutral per the 23:00 rule; after renaming that one symbol 0 diffs in 1443 sections. Debug residue: LF_UDT_SRC_LINE, line tables, checksums. Not run: full link/ninja build, exact PCH command (R2 harness variant), token-level audit.
* w0a_guardlines.py check engine.cpp / WinMain.cpp: TEXT OK.

## Stubs / honest gaps (R-n items)
| id | where | behaviour | owner |
|---|---|---|---|
| R3a-1 | engine_gl3.cpp RwD3D9EngineSetMultiSamplingLevels / ChangeMultiSamplingLevels | stored, applied at next RwEngineStart; runtime change logged "applies after a restart" and returns TRUE | R-11b / later |
| R3a-2 | RwD3D9EngineSetRefreshRate | stored + logged, not applied | later |
| R3a-3 | RwD3D9GetCaps / GetCurrentD3DDevice | nullptr (callers are D3D-only, guarded) | - |
| R3a-4 | RwD3D9DeviceSupportsDXTTexture before RwEngineStart | TRUE (no context yet; real value gl3Caps.dxtSupported after start, S3TC=1 here) | - |
| R3a-5 | RwEngineSetSubSystem | librw does not rebuild the mode list for another display; mode reset to 0 (one display on this Mac) | later |
| R3a-6 | platform_gl3 calls NotsaRwRenderState_OnEngineStarted (renderstate.cpp, still D3D, R-3b); RwShimPipelineEnsure is the R-2 stub (R-6) | the smoke provides its own empty ones | R-3b / R-6 |

## Open items for the next slices
* R-3b must check z convention: rw::engine->device.zNear/zFar for gl3 are -1/1 and PublishDeviceRange copies them into RwEngineInstance->dOpenDevice.zBufferNear/Far (D3D: 0/1); camera_sync's zScale/zShift are D3D style.
* gl3 `getFramebufferRect` uses SDL_GetWindowSize (points) while the viewport is in pixels: fine at 1x (no HIGH_PIXEL_DENSITY, smoke proves 640x480 == 640x480); needs a fix before Retina.
* librw `Engine::start` ignores DEVICEINIT failure and still runs DEVICEFINALIZE (GL calls without a loader would crash); RwEngineStart can only report the failure after the fact. Mitigation if it ever matters: librw patch returning early.
* WinPs.cpp already calls SDL_SetWindowSize(sdlWindow, vmi.w, vmi.h) after RwEngineSetVideoMode (W zone); the twin also sizes the window for the synthetic mode (harmless duplicate).
* vsync: gl3 showRaster takes FLIPWAITVSYNCH from the flags (camera.cpp, R-3b must map NOTSA_STANDALONE_NOVSYNC); smoke uses 0.
* Shared checkout note: build/gl_smoke and build/snap_census/*r3a* are scratch under build/ (ignored).
