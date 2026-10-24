# MAC-R4: slice R-4, Im2D / Im3D on GL3 (2026-10-24, branch portable/main)

Commits: librw fork `notsa` df034bd (pushed); gta-reversed e71dfdc9 (pin bump only) + code commit (see git log, subject "R-4: ...").

## What
* New source/portable/render/im2d_im3d_gl3.cpp = game-facing RwIm2DRender{Primitive,IndexedPrimitive,Line,Triangle}, RwIm3D{Transform,RenderPrimitive,RenderIndexedPrimitive,RenderLine,RenderTriangle,End} on librw gl3 (rw::im2d::* / rw::im3d::* -> gl3immed.cpp). The 9 R-2 stubs (R2-1, R2-2) removed from rw_stubs_gl3.cpp; the exe link has no stub left for Im2D/Im3D (`nm` shows the real definitions, rw_symbol_audit.py --check rc 0).
* Conversions: RwIm2DVertex {x,y,z,rhw,ARGB,u,v} -> gl3 Im2DVertex (w = 1/rhw, rhw 0 -> 1, bytes R,G,B,A); RwIm3DVertex ARGB -> r,g,b,a bytes (the BGRA-in-DWORD question from R-1/R-3b: every game setter writes D3DCOLOR ARGB, so unpack as r = c>>16, b = c). Im2D z = RwIm2DGetNearScreenZ (-1..1 gl3 range from R-3b) passes straight to clip space. Frame buffer raster offset added to x/y (caller array untouched).
* Im3D: Transform records {verts, count, LTM, flags} like the D3D driver; each Render* converts, calls librw Transform -> Render -> End (attribute set-up balanced), so vertices are read at Render time. LTM null / IDENTITY-flagged = identity. VERTEXALPHAENABLE set from rwIM3D_ALLOPAQUE (persistent, as exe). No-UV draws are untextured and the caller's TEXTURERASTER is restored (librw's im3DTransform would clear it for good).
* RW return-value contract of the D3D9 port kept (counts rounded to whole lines/triangles, point lists and type 0 draw nothing, empty draw TRUE, negative count FALSE, > 10000 indices for fewer vertices FALSE, Transform > 0x10000 -> NULL, 0x10000 -> 0 vertices, Render without Transform FALSE). Draw outside RwCameraBeginUpdate: logs once, FALSE.
* **Texture orientation (finding, hand-over R-5/R-6)**: librw gl3 shaders sample (u, 1-v) = GL row order; game rasters are D3D order (top row first, uploaded as is). Im2D/Im3D send v' = 1-v whenever a non-CAMERATEXTURE raster is bound (`NotsaGl3TextureNeedsVFlip` in raster_gl3.h; camera textures are GL order and need no flip, verified). RwRasterRender (raster_gl3.cpp NotsaGl3DrawQuad) uses the same rule (was upside down for textures, uniform colours hid it in R-3b). **R-6 stock pipelines need the same compensation (or patch the shader flip out and drop it here)**; R-5 must upload TXD levels top-first as is (DXT blocks untouched).
* librw (gl3 only): gl3immed.cpp streaming buffers start at 10000 vertices/indices and now grow (Im3D allows 0x10000 vertices; glBufferSubData past the buffer uploaded nothing). Proof: gl3immed.cpp is only in the GL3 source list of src/CMakeLists.txt; preprocessed output byte-identical before/after for -DRW_D3D9, -DRW_NULL and no define; differs only for RW_GL3+SDL3 (intended).

## Tests (tools/portable/run_gl_smoke.sh, exit 0, SMOKE RESULT OK, 0 FAIL, shader-error grep clean, 170 ok)
Script now also links im2d_im3d_gl3.cpp and portable/path_resolve.cpp (the smoke link was broken at HEAD by BOOT1's engine_gl3 RwFopenResolved). New "R-4" section in tests/portable/gl_smoke.cpp, all through the game API with pixel read-back: Im2D triangle (exact colour (10,100,200): no R/B swap, bbox, area), strip / fan / indexed list / indexed strip quads (6400 px), RwIm2DRenderTriangle/Line, polyline, line list, gradient, textured quad (2x2 texture quadrant colours, top row first), texture*vertex colour, no texture, camera texture upright, vertex alpha (blend off / on), empty and invalid call contract, frame buffer offset (3 entry points), 12000 vertices / 12000 indices (buffer growth); Im3D: triangle at projected pixels, colour order, depth == zScale/zShift, LTM translate / scale / identity-flagged, End pairing, textured indexed quad quadrants, 5 indices = 1 triangle, no-UV untextured + TEXTURERASTER restored, vertex alpha / ALLOPAQUE (+ VERTEXALPHAENABLE state), RwIm3DRenderLine, indexed lines, RwIm3DRenderTriangle, 30003 vertices, 20004 indices, Im2D after Im3D; glGetError clean after each block.

## Build / no-op
* PortableMac: cmake re-glob, `ninja gta_reversed` links (im2d_im3d_gl3 + raster_gl3 + raster.cpp rebuilt), no duplicate/undefined symbol.
* Win32: no MSVC-compiled TU touched (only portable-only files, tests, tools, librw gl3immed.cpp). StandaloneRelease compile-commands/wine A/B therefore not re-run: the only shared input is the librw submodule pin, and the one changed librw file (gl3immed.cpp) preprocesses byte-identically under -DRW_D3D9 / -DRW_NULL (proof above).
* Census not re-run (R was 6 = the F/P stragglers after R-3b): the new/changed files compile clean with the PortableMac flags (smoke compile + the full game target).

## R-n items / gaps
| id | what | owner |
|---|---|---|
| R4-1 | rwIM3D_NOCLIP ignored (GL always clips); Im3D lines 1 px (macOS core GL ignores glLineWidth > 1) | R-11a |
| R4-2 | Im2D with a sub-raster frame buffer: librw's xform uses frameBuffer->width/height of the sub raster, offset added in the twin (tested with a fake offset on the full raster only) | none seen in game |
| R4-3 | Im2D has no pixel-centre (-0.5) D3D9 correction; fog applies if the caller enabled it (w = camera Z) | R-12 tuning |
| R4-4 | stock pipelines (R-6) must compensate the v flip (see above) | R-6 |
| R4-5 | not exercised: draws with the actual game (menu/HUD sprites need R-5 textures) | R-7 |

## CONTINUE
None required. Next: R-5 (texture upload top-first, uses NotsaGl3TextureNeedsVFlip convention), R-6.
