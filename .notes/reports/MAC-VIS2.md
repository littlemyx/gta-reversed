# MAC-VIS2: native macOS vs Win32 D3D9 (wined3d under wine) colour comparison (2026-10-25, portable/main)

Commits: gta-reversed only (tools + notes: `tools/portable/play1/vis2_{pair,pair_win}.txt`, `vis2_run.sh`, `vis2_cmp.py`, frames + this report). **No code change was needed, no librw change, no pin bump** (nothing to A/B on Win32: no MSVC TU touched). Frames: `.notes/w0/vis2/` (native_*.jpg, win32wine_*.jpg = the wine reference, sheet_A.png / sheet_B.png = native top row, Win32 bottom row).

## 1. Method
* Reference = `build/StandaloneReleaseRR/bin/gta_reversed.exe` (the Win32 StandaloneRelease variant of 2026-10-24, D3D9 over wined3d, via `tools/portable/rr_run_win32.sh`), native = `build/PortableMacVis1` (HEAD 97a058ef). Both in RR mode (`NOTSA_RR=1`, virtual 20 ms steps, seed 12345, audio off), windowed 1280x720 (`NOTSA_STANDALONE_WINDOWED`), fresh user dirs, mark BMPs (`mark:` + `NOTSA_STANDALONE_SCREENSHOT`), serialised runs (never two GL processes), own NOTSA_LOCK_FILE. Native BMPs are the back buffer before the gamma pass (`NOTSA_STANDALONE_SHOT_GAMMA=0`), like a D3D screenshot (the D3D ramp is a display setting).
* The Win32 exe has no `cmd:weather` / `tpg` / `face` (those are in portable-only play_cmd.cpp) and a rebuild of the RR variant was not worth it (175 source files changed since its build), so state is pinned with commands both builds have: `cmd:cheat:N` (weather cheats: 6 = SUNNY_LA 1, 7 = EXTRASUNNY_LA 0, 8 = CLOUDY_LA 4, 9 = RAINY_COUNTRYSIDE 7, 10 = FOGGY_SF 9; same `ForceWeatherNow` the native `cmd:weather` calls), `cmd:hour`, `cmd:tp` to Grove Street (2495,-1665,14). Far teleports (countryside) make the Win32 CJ get busted (wanted level), so countryside colours are tested through weather 7 / 1 / 9 at Grove, not at a countryside place. Weather 13 (the "pink" one) is not reachable on this Win32 exe.
* Routes differ in two menu details (see R-VIS2-1): native = `vis2_pair.txt` (no `key:up`, with the intro skip block), Win32 = `vis2_pair_win.txt` (`key:up`, no skip block). End state is the same scene (Grove, CJ standing, same clock 11:04 / 22:04), frames line up pixel for pixel (HUD, CJ, houses).

## 2. Numbers (mean RGB native / Win32; boxes in 1280x720: sky 700,0-900,60; ground 64,480-480,640; roof 380,225-620,245; hud 992,128-1248,160; full frame)
| scene (weather, hour) | sky | ground | roof | HUD | full frame |
|---|---|---|---|---|---|
| s1 clear noon (0, 12) | (99,194,255) / (99,194,255) | (95,90,87) / (92,90,85) | (160,163,173) / (163,167,177) | (34,63,43) / (33,62,42) | (77,91,99) / (76,90,99) |
| s2 cloudy LA (4, 11) | (218,252,223) / (218,252,223) | (118,116,93) / (110,110,90) | (199,200,170) / (203,204,173) | (48,71,41) / (47,70,40) | (117,122,100) / (116,121,100) |
| s4b night clear (0, 23) | (92,100,79) / (92,100,79) | (55,50,34) / (55,50,34) | (41,46,38) / (42,47,39) | (32,54,26) / (31,54,26) | (51,50,33) / (51,50,33) |
| s3 sunny LA (1, 11) | (60,237,255) / (60,237,255) | (94,99,88) / (92,96,85) | (167,183,175) / (166,181,173) | (33,68,41) / (32,66,41) | (76,111,104) / (73,108,101) |
| s6 dawn (0, 7) | (157,255,255) / (155,255,255) | (94,98,91) / (94,98,90) | (158,176,174) / (160,178,177) | (45,67,41) / (44,67,41) | (89,110,104) / (88,108,103) |
| s6b dusk (0, 18) | (108,200,255) / (109,201,255) | (99,96,86) / (97,93,83) | (177,177,173) / (176,177,173) | (35,64,43) / (34,63,42) | (86,102,105) / (84,100,102) |
| s3b rain countryside (7, 11) | (138,137,133) / (140,140,136) | (109,109,99) / (105,104,96) | (184,181,172) / (194,192,188) | (46,67,40) / (48,68,42) | (113,112,103) / (116,115,108) |
| s5 fog SF (9, 14) | (209,220,189) / (214,223,192) | (118,120,101) / (112,114,95) | (203,207,176) / (193,197,167) | (53,75,45) / (51,73,43) | (131,136,112) / (133,137,114) |
| s4 night rain (7, 22) | (71,72,71) / (60,60,59) | (58,54,46) / (54,49,41) | (55,56,57) / (76,77,81) | (38,58,33) / (35,56,31) | (60,56,50) / (58,54,48) |
* Display menu (pause > options > display, s7 / s7b): mean abs diff 2.1 / 2.0 / 2.4 over the frame, backdrop (65,53,65) vs (64,52,64), text identical; **the backdrop photo covers the right part of the BRIGHTNESS / LEGEND rows in the Win32 frame too** (sheet in .notes/w0/vis2/native_s7_*.jpg vs win32wine_s7_*.jpg).
* Lighting A/B (one frame set, native `cmd:lighting:old|new` vs Win32 full-frame mean): sunny LA new (76,111,104) / old (95,99,86) / Win32 (73,108,101); dusk new (87,102,105) / old (101,95,84) / Win32 (84,100,103). The R-12 lighting model is 1 to 3 % from the reference, the old librw one is 15 to 25 % off in hue: R-12 confirmed.

## 3. Verdict per open item
| item | verdict |
|---|---|
| R-VIS1-3 cloudy LA sky "pale mint" | **same in the Win32 reference**: sky patch (218,252,223) in both, pixel-exact. Not a bug (the timecycle skyTop (125,145,151) is lifted by the cloud layer + colour filter, identically in both). |
| R-VIS1-4 countryside / pier pink | data, not render: weather 7 / 1 / 9 / 0 / 4 match within 1 to 3 %; weather 13 not reachable on Win32 without a rebuild (its ambient (22,8,8) comes straight from timecyc.dat) |
| R-VIS1-5 night rain sepia | **same in Win32** (sheet_A column 3: both brown / sepia with grain); the grain noise level matches (2.8 vs 3.2 mean abs high-pass) |
| R-VIS1-6 Display menu backdrop over the values | **original behaviour**, Win32 identical; not a portable bug, no fix |
| colour filter, channel order (d3dTexelOrder), sky gradient / clouds, HUD colours, night | sky patches are pixel-exact in 6 of 9 scenes, HUD within 1 level, night clear within 0 to 1: no channel swap, no missing / extra colour pass, no gamma difference |

## 4. What still differs (not fixed; wined3d is not the real D3D driver)
1. Native is 1 to 3 % brighter on lit surfaces in daytime (ground +2 to +5 on sunny scenes, 0 to +1 at dawn / cloudy / night-clear). Scales with the sun, not an offset: suspect directional-light N.L / D3D FF light model vs shader, or texture LOD. Not a structure difference.
2. Rain / fog scenes: Win32 is hazier on mid-distance objects. Linear fit native -> Win32 on 16 px blocks, house band y 200..330: rain a = 0.92 b = +19, fog SF a = 0.81 b = +31, night rain a = 1.06 b = +5 (clear scenes a = 1.00 b = 0). Native fog values are the timecycle ones (`cmd:gfxq`: far 650 / fogPlane 186.9 at 22:00 rain, far 650 / 5 at 11:00, far 220.5 / -41.8 foggy), the linear formula `clamp((w - far) / (fogStart - far))` is the D3D one, so at 25 m the night rain should have NO fog; the extra haze is therefore either wined3d table / vertex fog (the Win32 build picks table fog only if the caps have FOGTABLE + WFOG, else vertex fog, `standalone/rw/renderstate.cpp`) or a rain-only effect missing natively. Not decidable without a real Windows frame. Night rain sky: native 72 vs Win32 60 (cloud layer or a lightning phase, one frame each).
3. Clock: Win32 shows 12:08 where native shows 12:07 in the first scene (F-class timing, same as RR).

## 5. R-n items
| id | what |
|---|---|
| R-VIS2-1 | route-dependent state: with the same keys (no skip block) the native new game sits in the letterboxed intro cinematic (LSPD car, no HUD, no radar) at 16 s virtual where Win32 is already at Grove with HUD + radar; the first native scenes lack the radar disc until about 25 s. With the skip block native reaches the same scene. Win32 needs `key:up` before the first Return (without it the Win32 run stalls in the menu), native needs it NOT (with it native stalls): the first menu row differs between builds (fresh user dir). Camera pitch after the teleport differs by route (Win32 with the skip block looks down; one native run of 184 s also looked down: not deterministic in RR?) |
| R-VIS2-2 | brightness bias and rain haze above (needs a Windows reference: PORTABLE_SYNC.md) |
| R-VIS2-3 | Win32 RR exe lacks the portable `cmd:` set (weather, tpg, face); a rebuild with `weather` under `#ifdef NOTSA_STATEHASH` in SDLWrapper.cpp would allow weather 13 pairs (about 25 to 40 min wine build) |

## 6. Tools
`tools/portable/play1/vis2_run.sh native|win32 <tag> <timeout_s> <route>` (OUT default /tmp/vis2, frames in $OUT/shots/<tag>/), `vis2_cmp.py <nativeDir> <win32Dir> marks...`. Native run 75 to 90 s, Win32 under wine 70 to 90 s (one start-up retry seen).
