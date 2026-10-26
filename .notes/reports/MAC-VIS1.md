# MAC-VIS1: visual verification tour of the native macOS build + render leftovers (2026-10-25, portable/main)

Commits: librw fork `notsa` e3db1cf (pushed); gta-reversed 906f0d42 (pin), 7a627379 (CGamma on GL3, injector commands, pipe test, routes), final commit (routes, cmd:face, report; see git log). Own build dir build/PortableMacVis1, own NOTSA_LOCK_FILE, every game run printed "Loading the Game" (139 lines). Frames: `.notes/w0/vis1/*.jpg` (36 files; numbers below). Tools: `tools/portable/play1/vis1_*.txt` (tour A/B/C, fx, ab, sun, sky, night, fire, gam, menu2, country, sf), `NOTSA_STANDALONE_SHOT_GAMMA=1` (default in my runner) puts the final gamma pass into the mark BMPs.

## 1. Unverified effects
| effect | result | evidence |
|---|---|---|
| heat haze (CPostEffects::HeatHazeFX) | **works.** Weather 17 (desert) at 13:00: `HeatHazeFXFade` ramps 0.15 -> 1.0 over game minutes (fxstat log, 0.05 / game minute as coded), per-tile redraw of the front buffer: haze on vs off frame difference (x10, frame 30) shows the 47 px grid tiles as textured diff blocks (shifted copies), the other tiles are identical; static whole-frame mean diff is only 0.8-1.3 (alpha 80 / 255 by design, so it is subtle in stills, as in the exe) | 26_haze_on, 27_haze_off, 30_haze_diff_x10 |
| underwater | **reached and rendering.** `cmd:vehtp` (new) drops the player's car into the sea at z -12: `UnderWaterness 1.0`, camera z -13..-34, water level 0: murky grey-green fog, car visible, radar shows sea, "stunt bonus" text on screen, no GL error/crash for 4 s; the ripple distortion (UnderWaterRipple, same front-buffer mechanism as the haze) cannot be judged in a still | 17_underwater_a, 18_underwater_c; water from above (waves, specular, pier) 16_beach_water |
| gamma / brightness slider | **implemented (was inert).** `CGamma::Init` enables it under NOTSA_PORTABLE; `SetGamma` stores the D3D ramp exponent `1 - level + 0.2` (level = brightness/512 * 0.7) in `rw::gl3::presentGamma`; librw gl3 `showRaster` runs a final full-screen pass (copy window fb to a texture, `pow((v*255+1)/256, power)` = the ramp of CGamma::SetGamma). Default brightness 256 -> exponent 0.85, i.e. the whole game is now brighter than all earlier Mac frames (mean RGB of the same Grove day frame (76,91,99) -> (90,104,111); explains F-6 "darker than PS2/PC"). Real menu check: Options > Display > Brightness, 4x right: exponent 1.200 -> 1.069 (prefs 0 -> 96), frame mean 11.2 -> 13.6; `cmd:bright:0/384`: means 60 / 115 on the same scene. `NOTSA_GL3_GAMMA=<power>` overrides (=1: old look) | 19-21 gamma, 24/25 before/after, 36 |

## 2. Mirrors, radar, shadows
* **No exterior scene has an active mirror**: `cmd:mirrorzones` lists the 65 CCullZones mirror attribute zones: 64 of them are interior boxes (z 998..1157, interior coordinate space), 1 is at z 198..203. So the INT1 depth-mask bug only ever hit interiors (already re-shot in INT1; Binco and Burger Shot (mirror zone 10) frames after the fix: 09 / 10, both lit and complete). INT1-4 closed: nothing to re-shoot outside.
* **Radar / shadows after the GLSTATE address-mode fix**: A/B in one exe (`NOTSA_GL3_ADDRMODE_BUG=1` restores the upstream `==` test) on 4 scenes (Grove day, with a car, LSPD, SF): the radar disc interior is pixel-identical (diff x12 black, frames 28 / 29); CJ / car shadows differ by no more than scene noise (mean diff 0.2..2.7, `>16`: 0.0..4.7 %, all from moving peds / cars). The fix has no visible effect in the game (the radar sets CLAMP before binding the raster); it stays for correctness (pipe test).

## 3. GLSTATE-1 / GLSTATE-2 (librw gl3, e3db1cf) and camera lag
* GLSTATE-1: `Raster::lock` on CAMERA now sizes the read by `getWindowPixelSize` (SDL pixel size), not `glGlobals.present*` (the last glViewport, = a camera texture's size after an update).
* GLSTATE-2: scissor (enable + box) and colour write mask are cached (`rw::gl3::setScissorTest/getScissorTest/setColorMask/getColorMask`, filled from GL on first use and after a device start). `clearCamera` writes all channels (D3D Clear ignores the colour mask, like the stencil mask already handled) and restores mask + scissor exactly.
* Pipe test (`RunGlState`, +10 checks): lock with presentWidth forced to 16x16 returns the window size (old code: 16x16 by construction); clear with client scissor box + red masked writes (200,100,50) inside the box, leaves the outside, restores mask 0111 + scissor; caches == GL; gamma pass (grey 128, power 0.5 -> 181, glGetError clean, a following frame correct, power 1 no-op). **PIPE RESULT OK 217 checks** (was 207); run_gl_smoke: see below.
* Camera lag after `cmd:interior` (INT1-3): not a bug. The injector command runs before the frame's `CCamera::Process`, so the log line printed the pre-jump position; `cmd:playerstat` 9 s later reports camera mode 4, dist 3.26 / dz 1.07 (Binco), dist 0.92 in Burger Shot (tight room: collision push-in). The log text now says so.

## 4. Tour (~36 frames in .notes/w0/vis1; things that look wrong or suspicious vs the original)
| # | scene / frame | verdict |
|---|---|---|
| 01 LS day (Grove) | sky, palms, houses, soft CJ shadow, radar: look right; the road is slightly purple-grey (timecycle data) | ok |
| 02 LS night | lit windows, street-lamp coronas, orange-brown road, green-ish sky/ambient (timecycle) | ok |
| 03 LS rain (weather 8, 15:00) | grey fog, thin streaks (the original draws 32 rain streaks + particles, so thin is expected) | ok, density not compared |
| 04 SF fog (weather 9) | grey-green fog, distance fades | ok |
| 05 SF day docks | clear sky, ship / containers, kerb | ok |
| 06 LV neon night | Caligula's towers, red floodlights, coronas, palms lit | ok |
| 07 desert town | wood buildings, sand | ok |
| 08 countryside (weather 13, 11:00) | **road / pier boards look pink** | data: timecycle ambient (0.086, 0.031, 0.031) + obj (0.784, 0.737, 0.737) at that weather/hour, matches the sampled road pixel (171,142,136); not a render bug as far as the shared code goes (R-VIS1-4, compare to a Windows frame) |
| 09 / 10 interiors | Binco, Burger Shot: textured, lit, no black interiors | ok (INT1 fix holds) |
| 11 / 12 car day / dusk | Infernus white with shading, sky orange at 21:00, neon-green roof sign | ok |
| 13 ped crowd | peds + shadows present | ok |
| 14 / 15 pause menu, map | menu fonts, Vinewood backdrop, map tiles and legend text | ok |
| 22 / 35 sun at 07:00 / 18:00 | sun glow behind trees, **lens-flare ghosts (pink / yellow) over CJ's legs** drawn | plausible (flare elements), not compared |
| 23 cloudy LA (weather 4, 11:00) | **sky is pale mint-white (222,253,229)** while the timecycle skyTop is (125,145,151) | R-VIS1-3: clouds layer / sky tint brighter and greener than the table + gamma explain; compare with an original frame |
| 31 night drive | street lamps with coronas, red tail lights (coronas), traffic, exhaust smoke lit | ok |
| 32 night rain in a car | strongly sepia / yellow wash with grain, rain grain post effect active | R-VIS1-5 suspicious (weather 8 at 22:00 data?), not compared |
| 33 / 34 explosions day / night | fireballs, smoke, debris, wrecks, glow | ok |
| 36 Display menu | brightness page: **the backdrop image covers the right part of the first two rows (slider bar, LEGEND value)** | R-VIS1-6, check against the original layout |
| flicker / z-fighting | a/b frame pairs (1 s apart) show no flicker; no z-fight seen | ok |
Not covered: bikes / planes / boats, mission cutscenes, first-person weapons, a real mouse look, moon / stars at night (sky looks plain at 23:00 from the follow camera), long soak.

## 5. R-n items / open
| id | what |
|---|---|
| R-VIS1-1 | **intermittent crash after rapid far teleports (`cmd:tpg` ~10 times in 60 s)**: `CWorld::Process` first loop over the moving list (`GetIsTypePed` on a stale entity, fault address 0x41dd810 = looks like a truncated pointer), 1 of 3 long tour runs; split routes (tour A/B/C, <= 6 scenes each) never crashed (0 of 12). Probably streaming removal + a dangling moving-list entry (F-3 family, W/P). Also `cmd:spawnmany` (existing command, ids 64 / 65 in its model list) crashes in `CStreaming::RequestModel` (null model info, fault 0xe) - harness bug; use 9 or fewer peds |
| R-VIS1-2 | 1 of ~25 runs: SIGABRT inside AppleMetalOpenGLRenderer (`GLDContextRec::buildPipelineStateDescriptor`) while drawing the menu, with a second GL test process (pipe test) running at the same time; not reproduced alone (24 clean starts, same exe) |
| R-VIS1-3 | cloudy sky colour / brightness (frame 23), see table |
| R-VIS1-4 | warm / pink grounds at countryside weather (timecycle data) vs original |
| R-VIS1-5 | night rain colour wash (frame 32) |
| R-VIS1-6 | Display menu backdrop overlaps the first rows' values |
| R-VIS1-7 | a first far teleport raises the wanted level to 4 (chaos 1220) in the harness (cops shoot CJ in the desert / SF frames: HUD shows stars); `cmd:wanted:0` does not hold. Harness issue, not a render bug; cmd:health:5000 keeps CJ alive |
| R-VIS1-8 | gamma applies to the window only (no cut-scene / movie special case): the intro videos are skipped in these runs; the brightness is not applied to `NotsaRwReadBackBuffer` screenshots unless `NOTSA_STANDALONE_SHOT_GAMMA=1` (a D3D screenshot would not contain the display ramp either) |

## Tests
* PIPE RESULT OK 217 checks (own dirs build/pipe_vis1, build/gl_smoke_vis1). run_gl_smoke: **SMOKE RESULT OK**.
* Mutation: gamma pass off (`NOTSA_GL3_GAMMA=1`) = the grey-128 check would read 128 (frames 24 vs 25); address-mode A/B above.

## No-op proof (Win32)
Touched MSVC-compiled TU: `source/game_sa/Gamma.cpp` only (`#ifdef NOTSA_PORTABLE` blocks on their own lines: extern declaration, `m_IsEnabled = true` in the existing `#else` branch of `Init`, new `#else` branch in `SetGamma`); `w0a_guardlines.py check source/game_sa/Gamma.cpp`: **TEXT OK** (Windows view == original text, line for line, so no `__LINE__` shift either); the wine cl.exe A/B was not run. librw: `src/gl/gl3device.cpp`, `gl3raster.cpp`, `rwgl3.h` (compiled only for LIBRW_PLATFORM=GL3; d3d9 / null library builds do not contain them; rwgl3.h is only included by gl3 files). Portable-only: `source/portable/play_cmd.cpp`, `source/portable/render/camera_gl3.cpp`; tests/, tools/, .notes/.
