# MAC-R3b: slice R-3b, camera / raster / renderstate under GL3 (2026-10-24, branch portable/main)

Commits: librw fork `notsa` 7375f94 (pushed); gta-reversed 8352992c (pin bump only), 11e9acf8 (code); pushed to fork.

## Census (PortableMac native, snap_census.sh on HEAD + my files, compdb with engine_gl3/platform_gl3/camera_gl3/raster_gl3 in, platform.cpp out; TSV .notes/w0/r3b_after.tsv)
| item | R-3a after | R-3b after |
|---|---|---|
| total unique diagnostics | 462 | **308** |
| R (r_census.py) | 152 | **6** = Pipelines 1 (DN.cpp pointer cast, P) + Gamma/Radar 5 (inline asm, F): the planned stragglers, no diagnostic left in standalone/rw, RenderWare/D3D*, fakerw, portable/render |
| camera / raster / renderstate | 61 / 33 / 52 | 0 / 0 / 0 |

## Smoke (tools/portable/run_gl_smoke.sh, fresh build, exit 0, 86 checks ok, 0 FAIL; shader-error grep clean)
New R-3b section runs through the game's RW API (camera.cpp, raster.cpp, renderstate.cpp portable views + camera_gl3/raster_gl3 are linked in):
* camera: RwCameraCreate/SetNear/SetFar/SetViewWindow, BeginUpdate (null rejected), Clear (image|z|stencil), EndUpdate, ShowRaster (flag 0 -> swap interval 0).
* **z convention proved**: gl3 device range is -1..1 (not D3D 0..1); dOpenDevice.zBufferNear/Far copy it; zScale/zShift evaluate to -0.9998/+0.9998 at near/far; an Im3D triangle at camera z=2 reads back depth 0.7895 vs 0.7894 predicted from zScale/zShift. Nothing feeds a D3D projection into GL (librw builds devProj; camera_sync only fills planes/viewMatrix).
* Im3D triangles land on the predicted pixels (identity frame, view window 1x0.75; X is mirrored by the RW view matrix, as in the d3d9 backend), depth test keeps the nearer triangle, z=12 (> far) and z=0.2 (< near) triangles are clipped.
* Z-only clear of the main camera clears stencil too (exe rule) to the RwD3D9SetStencilClear value even with stencil write mask 0x0F; depth cleared to 1.0 even after a stray glClearDepth(0.5) (found by the test: camera_gl3 now sets glClearDepth(1)).
* camera without z raster (camera texture): BeginUpdate/Clear work (librw patch), Z/STENCIL bits dropped.
* renderstate: RW defaults after RwEngineStart, set/get round trips, out-of-range rejected, **64-bit TEXTURERASTER pointer** returned whole (the old u32 Get path left the upper half unwritten on arm64).
* rasters: 8888 texture create/lock(write,nofetch)/unlock(upload)/lock(read) round trip, second lock rejected, size restored; mip level 2 round trip (lock level remembered for the unlock); RwRasterSetFromImage 8888/888 + size mismatch; RwRasterRender textured quad pixel; RwRasterClearRect on camera raster and camera texture (top-left rect, rows read back top-first); NotsaRwReadBackBuffer top-down; glGetError clean after each block.
* vsync leg is opt-in (NOTSA_SMOKE_VSYNC=1): a swap with interval 1 BLOCKS FOREVER here (window not on screen in this session); the mapping flag -> librw SDL_GL_SetSwapInterval is code-verified only.

## Changes
* librw (gl3 only): SDL3 viewport/scissor/read-back in pixels (SDL_GetWindowSizeInPixels; Retina fix), setFrameBuffer tolerates cam->zBuffer == nil (was a null deref), clearCamera opens the stencil write mask for glClear and restores it, rasterLock/Unlock use PRIVATELOCK_READ/WRITE like d3d9 (shim tests them), CAMERA lock sets PACK_ALIGNMENT 1, CAMERA rasters always get their RGB description (a 0x0 camera raster was locked with bpp 0 -> overflow).
* standalone/rw/{camera,raster,renderstate}.cpp: D3D parts under `#ifndef NOTSA_PORTABLE` (own lines). camera.cpp: BeginUpdate/EndUpdate/Clear/ShowRaster(+screenshot)/AttachWindow whole functions, rwd3dimpl/crtdbg includes, lifted camera sync; `NotsaRwGetStencilClear()` portable accessor. raster.cpp: HasLockableStorage, LockCameraTexture, DeviceClear, DrawQuad, Lock/Unlock level, palette lock, NumLevels, SubRaster, RenderFast, ClearRect, ShowRaster, SetFromImage get GL branches. renderstate.cpp: D3D fog/shade/border/alpha-func side effects and the FF defaults tail guarded, DeviceLive without d3ddevice.
* New portable/render/camera_gl3.{cpp,h}, raster_gl3.{cpp,h}; rw_stubs_gl3: the two RwD3D9Set*State stubs removed (audit: no kept caller; `rw_symbol_audit.py --check` rc 0).
* run_gl_smoke.sh builds camera/raster/renderstate + twins (incremental by mtime); gl_smoke.cpp stubs RwImageRead*/FindRasterFormat (image.cpp not linked).

## No-op proof
* librw: gl3device.cpp preprocessed byte-identical (-DRW_D3D9, -DRW_NULL, -DRW_D3D9 -DLIBRW_SDL3, none) 7178c5b vs 7375f94; gl3raster.cpp identical after normalising the `__FILE__/__LINE__` text of RWERROR (and it is only in the GL3 source list of src/CMakeLists.txt). Differs only for RW_GL3+SDL3 (intended).
* gta-reversed A=6da60679 B=11e9acf8 (sub-agent, /private/tmp/claude-501/r3b_ab/REPORT.md, same method as R3a): StandaloneRelease 1281/1281 TUs, 4668/4668 compile commands identical, build.ninja identical (20,401,377 B), no portable file in the Win32 compdb/ninja. wine cl.exe on camera.cpp, raster.cpp, renderstate.cpp: code + relocs + all non-debug sections (1431/1328/1306) + symbols + section order identical, /P text identical modulo guard blank lines, 0 `__LINE__` literals; debug residue = line tables/checksums/UDT_SRC_LINE only. w0a_guardlines.py: TEXT OK x3. Not run: full link/ninja, PCH-exact command.

## Stubs / gaps (R-n items)
| id | where | behaviour | owner |
|---|---|---|---|
| R3b-1 | camera_gl3.cpp RwD3D9CameraAttachWindow | other window than the device window: log + FALSE | - |
| R3b-2 | raster.cpp SubRaster (portable) | sub raster has texid 0 (must not own the parent's texture): not usable as a texture | R-5 |
| R3b-3 | camera.cpp RwCameraCreate | portable keeps librw's cameraSync; the lifted exe callback (camera_sync.h) marshals 32-bit pointers and SEGFAULTS on arm64 (found by the smoke). Frustum planes/viewMatrix differ ~1e-4 from the exe (sphere-test edge cases) | F stream: 64-bit-safe lift |
| R3b-4 | renderstate.cpp | FOGTYPE (exp/exp2), FOGDENSITY, SHADEMODE flat, BORDERCOLOR, exact alpha compare (gl3 has ALWAYS/GEQUAL/LESS only; nearest used), alpha-test-only-while-blending nuance: shadow only | R-6 / shaders |
| R3b-5 | camera_gl3.cpp ShowRaster | NOTSA_STANDALONE_MEMLOG (Windows heap stats) not ported | - |
| R3b-6 | RwRasterLockPalette | nullptr (gl3 has no paletted rasters) | R-5 |

## Hand-over
* R-5: **pixel byte order**: gl3 8888 = R,G,B,A bytes, 888 = 3 bytes/texel, 1555 = R5G5B5A1; the d3d9 convention in rwpixel.h (EncodeTexel/RwRGBAToPixel: B,G,R,A, 888 = 4 bytes X8R8G8B8) is NOT adapted: game code writing texels into locked rasters (JPegCompress, player skin, radar) and RwRGBAToPixel need the swizzle. gl3 refuses PAL/565/4444/LUM8 rasters (RwRasterCreate returns null).
* R-11b: camera textures are bottom-up in GL (rows are flipped only for CPU lock); sampling a rendered camera texture needs a v flip or a y-flipped projection. RwRasterRenderFast uses glCopyTexSubImage from the bound read framebuffer (main FB inside a main camera update only).
* R-7: vsync swap blocks when the window is not visible (observed here); WinMain/frame limiter should keep NOTSA_STANDALONE_NOVSYNC available. Retina: window is created without HIGH_PIXEL_DENSITY (1x); viewport code now uses pixels so enabling it later is safe for librw, but game-side screen sizes stay in points.
* camera textures created with a zBuffer need glad depth+stencil texture support: verified only without z raster in the smoke (main camera z path verified).
* lldb/sample hang in this session (attach blocked): use ~/Library/Logs/DiagnosticReports/*.ips for crashes.
