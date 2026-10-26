# MAC-INT1: interiors render black (R11-1) - root cause and fix (2026-10-25, portable/main)

Commits: librw fork `notsa` c7b8750 (pushed); gta-reversed 03a04e1c (pin), final commit = diagnostics + pipe test + route + this report. Own build dir build/PortableMacInt1, own NOTSA_LOCK_FILE, every game run printed "Loading the Game". Win32: no MSVC TU touched (below).

## Root cause (not lighting, not shell textures, not NVC / DN, not alpha ref)
The shell was drawn, textured, with the right prelit colours and ambient (0.08), but **0 samples passed the depth test** (GL_SAMPLES_PASSED query around every shell mesh in the main frame) and the default framebuffer depth read **0.0 everywhere** before the shell draw.
* Johnson house (area 3) has a mirror: `CMirrors::RenderMirrorBuffer` (Mirrors.cpp:109) draws a full-screen Im2D quad at the near plane with ZTEST 0 / ZWRITE 0 before the world (the reflection image is the background, the world is drawn over it).
* librw gl3 `clearCamera` calls `glDepthMask(GL_TRUE)` / `glDepthMask(rwStateCache.zwrite)` directly, behind the GL state cache (`oldGlState.depthMask` is not updated). Sequence: zwrite 0 flushed (cache old = 0), zwrite 1 requested (cur = 1, not flushed), camera clear (real GL mask becomes 1), zwrite 0 requested (cur = 0 == old = 0, so no `glDepthMask(0)` is ever issued): the real mask stays ON while the game believes depth writes are off. Probe at the quad draw: `rw ZTEST 0 ZWRITE 0; GL depthTest 1 mask 1 func GL_ALWAYS`, centre depth 0.9933 -> 0.0000. Every surface drawn afterwards (the whole shell, the player) failed GL_LEQUAL against depth 0; the only thing on screen was the mirror image itself (its 1024x512 reflection buffer, drawn first, plus a few door boards).
* Same mechanism darkened / cut interiors that reuse the state (Binco, Well Stacked Pizza showed half a room before the fix, frames below).

Fix (librw, gl3-local): after the raw `glDepthMask(rwStateCache.zwrite)` in `clearCamera`, `oldGlState.depthMask = rwStateCache.zwrite` (cache == real GL). `NOTSA_GL3_CLEAR_DEPTHMASK_BUG=1` restores the old behaviour for A/B frames.

## Evidence
* Pipe test (tools/portable/run_pipe_test.sh, `RunDepthMask`, new case): sequence "ZWRITE 0 flush, ZWRITE 1 pending, RwCameraClear, ZTEST 0 + ZWRITE 0 full-screen near quad, world quad drawn after it must be visible" - **FAIL on librw 8d0a145 (pixel (200,0,0), the quad hid the geometry), OK on c7b8750**; control variant (no pending ZWRITE) passes on both. Full run on c7b8750: **PIPE RESULT OK, 187 checks** (was 182 + the INT1 cases).
* New `RunInterior` (INT1): carlscrap.dff + carlslounge.txd from gta_int.img (the Johnson shell: 17 materials, flags 0x6f = prelit, lit, modulate, no normals), camera inside, ambient-only lighting: 4 views, 94-100 % coverage, lit on the stock pipeline - proves shell data / textures / prelit / lighting are fine (`INT1_AMB=0.082` = the game's value: still visible, mean (35..55)).
* Frames .notes/w0/int1/{before,after}_{johnson,binco,pizza,ammu}_b.jpg (cmd:interior routes tools/portable/play1/int1.txt, mean luminance before -> after: Johnson 2.0 -> 59, Binco 28 -> 168, Pizza 45 -> 120, Ammu-Nation 80 -> 83 (no mirror, unchanged)). "before" = same exe with NOTSA_GL3_CLEAR_DEPTHMASK_BUG=1.

## Diagnostics left in (env-gated, zero cost off)
`NOTSA_INT1_LOG=<tris>` (pipelines_carenv_gl3.cpp): per mesh of the geometry with that triangle count: lights, texture / raster, prelit mean, GL_SAMPLES_PASSED, depth before the draw, LTM / camera; `NOTSA_INT1_DEPTH=<secs>` (im2d_im3d_gl3.cpp): logs Im2D draws that change the centre depth value; `cmd:gfxq` now also logs camera near/far/fog plane, timecycle fog start / far clip / ambient / sky colours (play_cmd.cpp).

## R-n items / open
| id | what |
|---|---|
| INT1-1 | other raw GL state calls bypassing the cache were looked for (grep glDepthMask / glDepthFunc / glEnable(GL_DEPTH_TEST) in source/ and librw: only gl3device.cpp); stencil mask is restored from `oldGlState.stencilWriteMask` (consistent). Not audited: glColorMask / scissor outside gl3device.cpp |
| INT1-2 | interiors have no sky / far scenery by design; Ammu-Nation / Binco colours compared by eye only (no Windows reference frame) |
| INT1-3 | `cmd:interior` leaves the camera at the previous interior for the first frames after a jump (RestoreWithJumpCut lag), frames taken 7 s after |
| INT1-4 | exterior frames with an active mirror (e.g. outside special zones) went through the same bug; not re-shot |

## No-op proof (Win32)
Touched: librw `src/gl/gl3device.cpp` (compiled only for LIBRW_PLATFORM=GL3: d3d9 / null library builds do not contain it, same argument as R-3b/R-4/R-12/R-11); `source/portable/{play_cmd.cpp,render/pipelines_carenv_gl3.cpp,render/im2d_im3d_gl3.cpp}` (portable-only glob); tests/, tools/. No MSVC-compiled TU changed, so no wine cl.exe A/B.
