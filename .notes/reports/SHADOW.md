# SHADOW - shadow flicker in the librw standalone (Release)

## Root cause
`RwCameraClear` (source/standalone/rw/camera.cpp) passed `rwCAMERACLEARZ` through to `IDirect3DDevice9::Clear` for cameras that have **no z raster**.
librw's `setRenderSurfaces` binds *no* depth surface for such a camera (`SetDepthStencilSurface(NULL)`), and `Clear(TARGET | ZBUFFER)` without a depth
buffer fails as a whole (D3DERR_INVALIDCALL), so the colour clear is lost too. The exe's `_rwD3D9CameraClear` drops the bits itself:
`0x7F7DC8: test al,6 / mov edx,[camera+0x64] (zBuffer) / test edx,edx / jne / and al,0xF9`.

Victims: every camera-texture camera the game creates without a z raster - the real-time shadow cameras (`CShadowCamera::Update_Internal` clears with Z|IMAGE every frame):
the player's real-time shadow texture was never cleared, `InvertRaster` (INVDESTCOLOR) then inverted the accumulated previous frame again,
so the texture alternated every frame between "white background + silhouette" (no visible shadow) and "black background + silhouette" (a huge dark quad
projected on the ground, see BEFORE picture). Period 2 frames = the flicker the user sees.

## Evidence (trace build, 128x128 camera raster, per frame, R5G6B5 mode)
Before: `a1.cleared` (read back right after `RwCameraClear`) == previous frame's final image (corner 0xFFFF / 0x0000 alternating), `a3.inverted`
alternating corner 0 / 0xFFFF, `D.gradient` mean RGB 93 / 14 alternating. After: `a1.cleared` = all 0xFFFF every frame, `a3.inverted` identical every frame, `D.gradient` mean 13.

## Frame counts (start alley, ~noon after waiting 240 s so that the sun is up; 40 consecutive Presents, region around CJ's feet 330..620 x 440..590)
| build | frames with the dark RT-shadow quad | mean luminance of the region (min..max) | max frame-to-frame diff |
|---|---|---|---|
| before | 20 / 40 (every odd frame) | 51.7 (42.0 .. 61.4), alternating 61.0 / 42.0 | 18.96 |
| after  | 0 / 40 | 61.1 (60.2 .. 62.1), smooth drift | 5.0 (CJ idle animation) |
Pictures: `.notes/user_reports/shadow_flicker_BEFORE_consecutive_frames.jpg` (frames N, N+1: left no shadow, right the big dark quad),
`.notes/user_reports/shadow_flicker_AFTER_consecutive_frames.jpg` (both frames identical shadow state).

## What I could not reproduce / not verified
* At game start (06:3x - 06:5x) the alley is in the shade: the real-time shadow, stored/static shadows and the BMX blob/stencil shadow are not visible in either build
  (consistent with FPS' observation for both exes); stored shadows were stable (1 stored per frame, same batch every frame).
* The original exe was not run: the DLL/ASI route would need a rebuild with a hook toggle for the Shadows category. The fix follows the exe asm directly.
* Video mode: the run used `GcurSelVM=1` = 960x600 **R5G6B5** (16 bit; from the settings value m_nDisplayVideoMode; the "first 32 bit mode" fallback in
  `CheckDefaultVideoModeSupported` is only used when the setting is invalid). In 16 bit `GraphicsHighQuality()` is false (camera raster depth 16 < 32): no stencil
  shadows (vehicles/objects use blob shadows instead), D16 depth. The user's screenshots are 1920x1200, mode unknown: if they run a 32 bit mode the stencil-shadow
  path (CStencilShadows) was NOT exercised by me and could have separate issues. Worth a follow-up with a 32 bit mode forced.

## Changes
* `source/standalone/rw/camera.cpp`: `RwCameraClear` drops Z and STENCIL when `camera->zBuffer == nullptr` (exe behaviour).
* Same file, run-build debug aid: `NOTSA_STANDALONE_SCREENSHOT_ARM=<mark>` - the every-k-th-Present screenshot cadence starts only after the input script's `mark:<mark>`
  (frame-exact bursts without burning `SCREENSHOT_MAX` during the menu/loading). Not yet in docs/STANDALONE.md (file is dirty from another agent).
* Diagnostics used (per-frame RT raster statistics via RwRasterLock, `[shdw]`/`[rts]`/`[rtu]` log lines) were temporary and reverted.
