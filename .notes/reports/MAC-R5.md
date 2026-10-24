# MAC-R5: slice R-5, TXD (D3D9 native textures) -> GL3 rasters (2026-10-24, portable/main)

Commits: librw fork `notsa` cd3689a (pushed); gta-reversed 36a02280 (pin bump), 91358d81 (code + tests); pushed.

## What
* librw gl3 (gl3raster.cpp, rwgl3.h): runtime flag `rw::gl3::d3dTexelOrder` (default 0 = upstream). When set (RwEngineOpen in engine_gl3.cpp sets it) TEXTURE/NORMAL rasters hold d3d9 texel words: 8888 = B,G,R,A bytes (GL_BGRA), 888 = X8R8G8B8 (4 bytes), 1555 = A1R5G5B5 (GL_UNSIGNED_SHORT_1_5_5_5_REV), plus new 565 / 555 / 4444 (A4R4G4B4) / LUM8 (GL_R8 + swizzle). This resolves the R-3b hand-over: rwpixel.h, RwRGBAToPixel, game lock users (ClothesBuilder, plates, JPegCompress) work unchanged, no swizzle at lock time. NotsaGl3RasterSetFromImage now uses rwpixel EncodeTexel (same as D3D build). CAMERATEXTURE / CAMERA keep gl3 RGBA / RGB (R-11b).
* portable/render/texture_gl3.{h,cpp}: NotsaGl3ReadD3D9Texture = the platform-9 reader (called from standalone/rw/texture.cpp, D3D reader guarded with `#ifndef NOTSA_PORTABLE`). DXT1/3/5 uploaded as compressed blocks untouched, top row first (R-4 rule); DXT1 always RGBA-DXT1 internal format (1-bit alpha as D3D), hasAlpha follows the file flag; PAL8/PAL4 expanded to 8888/888 like the D3D build; mip chain = exactly the file's levels (never auto-generated); raster->format keeps the file's mip bits; legacy R8G8B8 widened. Finding: SA DXT chains store 0 bytes for the 2x2 and 1x1 levels (10051 levels in the Steam copy); gl3 needs complete levels, the reader repeats the 4x4 block (counted, `zeroLevels`).
* Test: tests/portable/rw_texture_gl_test.cpp + tools/portable/run_txd_test.sh (+ ctest `portable.txd_gl`, label portable, 22 s cold / ~3 s warm; SKIP when no game data / no display). Own independent TXD parser; loads every dictionary of models/*.txd, models/{txd,generic,grass}/, gta3.img, gta_int.img, player.img, cutscene.img (read in place) plus a synthetic dictionary for formats absent from the Steam copy (DXT5, 1555, 565, 555, 4444, LUM8, PAL8 x2, PAL4, short chains).
* gl_smoke.cpp updated to B,G,R,A / X8R8G8B8 expectations (R-3b/R-4 sections); SMOKE RESULT OK.

## Results (real data, build/txd_test/run.log)
4041 dictionaries (0 failed), 32823 textures, 707 MB: DXT1 29208 (7010 mipped, 84937 levels), DXT3 2263, 888 1061, 8888 291; 0 failures in load / size / level count / format / alpha flag / lock / data (every level locked for read == file bytes, DXT blocks included); glGetError 0 after every upload; levelSizeMismatch 0; NPOT 0. Synthetic: DXT5 2, 1555, 565, 555, 4444, LUM8, 888, 8888, PAL8 2, PAL4: all pass. Draw check (Im2D 1:1 nearest vs own CPU DXT1/3/5 + texel decoders, 6 per format): 0 pixels beyond tolerance (6 DXT, 9 others); proves top-row-first + no swizzle. S3TC works on Apple M5 Max GL 4.1. Skipped: 3 platform-8 (D3D8) files (misc.txd, generic/wheels.txd, txd/outro.txd) that the game never loads (not referenced in source).
Lock contract (BOOT1 extra acceptance): RwRasterLock READ and READ|WRITE on a TXD-loaded 8888 raster return the pixel copy (bytes == file, top row first), edit persists via unlock, two simultaneous locks (CopyTexture pattern) work, RwRGBAToPixel(200,100,50) texel draws as (200,100,50).
Game: full PortableMac build, mac_boot_run New Game script: CGame::Initialise / InitialiseDirtTexture (vehiclegrunge256 lock) now pass; next crash is NVCPipelineProcess(rw::Atomic*) <- CEntity::CreateRwObject during the PROLOG1 cutscene LoadScene (R-8/R-9/R-6 pipelines, not R-5).

## No-op proof
* librw: only gl3raster.cpp (GL3-only source list) and a declaration `extern bool32 d3dTexelOrder;` in rwgl3.h (no code/symbol); default 0 keeps gl3 behaviour.
* Win32: only MSVC-compiled TU touched = standalone/rw/texture.cpp (+10 guard/include lines). Sub-agent A/B (/private/tmp/claude-501/r5_ab/REPORT.md, A=bbfe62bf B=91358d81, same librw tree): StandaloneRelease 1281/1281 TUs, 4668/4668 compile commands identical, build.ninja identical (20,401,377 B), no portable file visible; wine cl.exe texture.cpp code+relocs (44784 B, 12261), 1354 non-debug sections, 3982 symbols, section order identical; /P text A-only 0 / B-only 0; w0a_guardlines TEXT OK. Debug line info only. Limit: single TU, no full link.

## R-n items
| id | what | owner |
|---|---|---|
| R5-1 | platform 8 textures (3 unused files) not read on GL3 (librw d3d8 reader creates null-driver rasters) | none |
| R5-2 | sub raster still not a usable texture (R3b-2); RwRasterLockPalette still nullptr (R3b-6, PAL expanded at load, nothing needs indices) | - |
| R5-3 | zero-size DXT tail levels are copies of the 4x4 block, not true averages (only visible at extreme minification) | R-12 tuning |
| R5-4 | CAMERATEXTURE locks stay RGBA (not d3d order) | R-11b |
| R5-5 | librw's own rasterFromImage/rasterToImage/gl3 native TXD with d3dTexelOrder=1 assume RGBA (unused: shim has its own SetFromImage) | - |
| R5-6 | full texture/material render with the game (menu/HUD sprites) not exercised: R-6/R-7 | R-7 |

## CONTINUE
None for R-5. Next: R-6 (pipelines must apply the v flip rule), cutscene crash above belongs to the pipeline slices.
