# SHADOW2 - shadows at the user's resolution, 16/32 bit, original-exe comparison (2026-10-11)

## Which mode does the user play?
`.notes/user_reports/shadow_flicker_1/2.webp` are 1920x1200, but the picture is a **2x pixel-doubled 960x600** (mean abs diff of horizontally adjacent pixel pairs 2.1 inside a pair vs 12.3 across pairs).
That is the Retina capture of the default **video mode 1 = 960x600 R5G6B5 (16 bit)** (settings file `gta_sa.set`: `m_nDisplayVideoMode` dword at byte 0xB36 = 1; the shared settings file in `~/Documents/GTA San Andreas User Files/` contains 1).
So the previous agent's 16-bit test (SHADOW.md) was the user's configuration. 1920x1200 X8R8G8B8 is mode 35 (mode 13 = 1920x1200 R5G6B5); `NOTSA_STANDALONE_VIDEOMODE` is ignored when the settings file holds a valid mode, the mode has to be patched into `gta_sa.set`.
WARNING for all agents: in a prefix cloned from `~/.wine-msvc` the `Documents` folder is a symlink to the user's real `~/Documents`: every test run overwrites the user's/other agents' `gta_sa.set` (my private prefix has a real Documents dir).

## Depth/stencil format (shim vs exe)
Wine/wined3d accepts (CheckDeviceFormat, logged by the new `rw depth formats` line): D16, D24S8, D24X8; NOT D15S1, D24X4S4, D32.
Exe (`0x7F6CB0` .. `0x7F6FED`): 16 bit tries D15S1 (unsupported) then D16 = no stencil (`_RwHasStencilBuffer` 0xC97C3C = 0); 32 bit tries D24S8 first (= stencil 1). The shim (`ChooseFormats` / librw startD3D) picks D16 / D24S8 the same way -> identical formats on this host.
`GraphicsHighQuality()` is false in 16 bit (camera raster depth 16): no stencil shadows, vehicles use `StoreShadowForVehicle` blobs and `StoreShadowForPole` is active; in 32 bit stencil shadows are on (log: 16..43 stencil objects, 12k-44k shadow-volume vertices per frame, pole shadows off).
`CStencilShadows::RenderStencilShadows` render-state sequence checked call by call against the exe asm (0x7113B0): identical.

## Deviation found and fixed (exe asm 0x7F7730 `_rwD3D9CameraClear`)
Main camera (frame buffer raster type != CAMERATEXTURE, `cmp byte [raster+0x20], 5` at 0x7F7C48): the exe builds the D3D clear flags as TARGET (mode&1) | ZBUFFER (mode&2) and ORs STENCIL when **Z or STENCIL was requested** and the device has a stencil buffer (0x7F7F03..0x7F7F3B). The shim cleared the stencil only when `rwCAMERACLEARSTENCIL` was passed.
Callers with a Z-only clear of the main camera: Idle() (app_game.cpp:417), the menu path (app_game.cpp:318), WinMain, PostEffects alpha mask - the exe clears stencil there too. No visible effect in the scenes I tested (the next clear in `CMirrors::BeforeMainRender` passes STENCIL anyway), but the stencil buffer now follows the exe in every sequence.
Camera-texture path (type 5) is unchanged (requested bits, Z/STENCIL dropped without a z raster = f9b8b892).
Fix: `source/standalone/rw/camera.cpp` (RwCameraClear). Tests added to `tests/standalone/rw_frame_camera_test.cpp`: Z-only clear resets the stencil to the stencil clear value, stencil-only clear leaves the colour, camera-texture camera without z raster still clears its colour (regression test for f9b8b892).

## Frame counts: 40 consecutive Presents per scene (`NOTSA_STANDALONE_SCREENSHOT_BURST=40`, new run-build debug aid: `mark:burst_<x>` in the input script saves the next 40 Presents)
Measure: `analyze_burst.py` (luminance of the frame and of the ground region, per frame; flicker = sign alternation of the frame-to-frame luminance delta, period 2, with |delta| > 0.5 / 1.0, and frames deviating > 1.0 from the local median). Clock set with the faster-clock cheat (YSOHNUL, ~16 game minutes per second) + sunny weather (AFZLLQLL), night = NIGHTPROWLER; CJ on the street, standing / running, parked NPC cars in view.

| exe | mode | scene | game time | frames | alternations >0.5 / >1.0 | frames deviating >1.0 | max frame-to-frame lum delta |
|---|---|---|---|---|---|---|---|
| ours | 1 (960x600x16) | foot, sun (ped RT shadow + parked cars) | 11:07 | 40 | 0 / 0 | 0 | 0.62 |
| ours | 1 | foot running | 11:22 | 40 | 0 / 0 | 0 | 0.22 |
| ours | 1 | night, street lights + headlights of NPC cars | 00:00 | 40 | 0 / 0 | 0 | 0.23 |
| ours | 35 (1920x1200x32, stencil shadows on) | foot, sun | 10:01 | 40 | 0 / 0 | 0 | 0.43 |
| ours | 35 | in/near car (sun) | 11:08 | 40 | 0 / 0 | 0 | 1.69 |
| ours | 35 | night | 00:00 | 40 | 1 / 1 (a car arrives in the last frames) | 2 | 7.4 (scene change, not flicker) |
| orig | 1 | foot / car / drive / night | 12:12 / 12:30 / 12:34 / 00:00 | 40 each | 0 / 0 in all | 0,0,1,0 | 1.4 / 0.4 / 2.0 / 0.6 |
| orig | 35 | foot / car / drive / night | 12:09 / 12:28 / 12:32 / 00:00 | 40 each | 0 in all (5 / 1 at the 0.5 threshold in night / drive, none > 1.0) | 0,0,4,0 | 0.4 / 0.2 / 2.6 / 1.1 |
Earlier runs of the same kind (before the burst facility, route C: sunny street, moving camera) in both modes also showed no period-2 pattern. The first-gen HQ run shows stored shadows 1..9 and 16..43 stencil objects per frame, rendering stable.
Missing / doubled shadows: none seen. CJ's real-time shadow, the NPC-car blob shadows (16 bit) and the stencil shadows (32 bit) are present in every frame of every burst; the ped shadow is visible on the road when the sun is to the side (picture below). Only 3 triangles (verts=9) carry the RT shadow of CJ on the road, which is what the geometry gives (flat road).
Pictures (ours on top, original at the bottom, 4 consecutive frames; the scenes differ because the two exes run different scripted inputs, so this is a consistency check, not a pixel diff):
`.notes/user_reports/shadow2_mode1_960x600_ours_top_orig_bottom_consecutive.jpg`, `.notes/user_reports/shadow2_mode35_1920x1200_ours_top_orig_bottom_consecutive.jpg`.
(A before/after of my RwCameraClear change would be two identical pictures: the old flicker fix f9b8b892 already removed the only visible defect; the stencil-clear change is invisible in these scenes and is covered by the new unit test instead.)

## Original exe comparison (helper, sonnet; files in /tmp/shd2-orig, 1.6 GB, not committed)
Original `gta_sa_compact.exe` under Wine in the same two modes, silent, d3d9 proxy dumping 40 consecutive back buffers, memory-write clock (12:xx) instead of the faster-clock cheat; scripted input otherwise as ours. Result: the original is flicker-free too (table above); our frames are statistically the same as the original's (no scene where the original has a shadow that ours lacks, no scene where ours alternates).

## Not verified
* Pixel-exact shadow shapes vs the original (different scripted scenes; sun position/time differ). A deterministic scene (save game / same car and camera) would be needed.
* The JQNTDMH car spawn fails often in ours (`until:invehicle` timed out in 3 of 6 of my route runs; the original spawn is also unreliable according to the helper: F enters a passing NPC car) - not investigated.
* Stencil shadows were only judged by consistency + presence (log counters) + render-state sequence; the volume geometry (`BuildShadowVolume`) has no exe oracle.

## Changes (commit): camera.cpp (RwCameraClear, burst debug aid), engine.cpp (depth-format probe log), rw_frame_camera_test.cpp, docs/STANDALONE.md (screenshot ARM/BURST, video-mode note).
Process note: I briefly had `[shdwdiag]` log lines in Shadows.cpp / StencilShadows.cpp / RenderBuffer.cpp (shared tree 03:20-04:15, my own ninja call compiled two objects into build/StandaloneRelease; the orchestrator's 04:08 builds picked up the sources too). Sources are reverted; objects of those three files in StandaloneRelease/StandaloneDetached still contain the logging until the next ninja run recompiles them (mtime of the sources is newer).
