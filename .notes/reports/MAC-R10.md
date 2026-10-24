# MAC-R10: slice R-10, skinned-ped pipeline audit on GL3 (2026-10-24, portable/main)

Commits: librw fork `notsa` fea0117 (fog/palette/weights/indices), 4f8dd81 (stream refresh after lock), both pushed; gta-reversed pins 68e6c3fd, 7c24c0e1; test + script f25f799b; skinstat log + route (last commit).

## Findings (real data, all of gta3.img + player.img: 494 skinned DFFs, 391,713 vertices)
| item | result |
|---|---|
| bone count vs 64 cap | max numBones 61 (hierarchy 61 nodes), 0 skins over 64; peds / CJ parts 32 |
| hierarchy | every skinned clump has an HAnim hierarchy; hierarchy.numNodes == skin.numBones in all 494 |
| weights | sum in [1.000000, 1.000000], worst |sum-1| 4.8e-8, 0 vertices with sum 0, 0 negative / NaN |
| indices | 0 indices >= numBones (weighted or not) |
| palette upload | one `mat4[64]` uniform (4096 B) per skinned draw; matrices built from hierarchy x inverse bind x inverse atomic LTM (same as the exe's non-local-space path) |
| normals | `mat3(bone) * normal` blended by weights, no renormalise (like default.vert; bones are rigid) - skinned == static lighting proven numerically (below) |
| suspected librw fog bug | CONFIRMED: skin.vert fed `gl_Position.z` to DoFog, default.vert feeds `.w`. With fog on, skinned geometry was fogged on the non-linear clip z (test: 5660 of 29001 px differ, colour (167,167,196) vs static (164,164,197); the gap grows with the far plane). Fixed |
| identity fallback | exists; but the loops ran to `hier->numNodes` / `skin->numBones` unbounded: a hierarchy with fewer nodes left stale matrices of the previous draw in the (static) palette buffer, one with more nodes read / wrote past the skin's inverse matrices, > 64 bones overflowed the 64-entry buffer. Fixed (bounded to min(numBones, 64), missing bones identity) |
| weight renormalisation | data is clean, so not needed for SA assets; added defensively in the GL vertex stream only (CPU skin data untouched): vertices whose sum is outside 1 +- 1e-3 are rescaled; indices >= min(numBones,64) go out as bone 0 |
| CClothesBuilder (CJ) | BlendGeometry / CopyGeometry rewrite weights + indices through the Skin pointers under RpGeometryLock(ALL): that rebuilds the mesh header -> full re-instance, so the GPU copy is current (proven in test and in game: fat / muscle / skinny cheats change CJ's body, no exploded vertices). A lock without LOCKPOLYGONS used to keep stale weights/indices on the GPU; now the stream is refreshed after any lock (counter `geosRefreshed`) |

## Changes
* librw (gl3 only: src/gl/gl3skin.cpp, rwgl3.h, shaders/skin.vert + skin_gl.inc): fog `.w`; bounded palette; stream sanitising + refresh; `rw::gl3::skinStats` counters.
* `source/portable/render/pipelines_gl3.{h,cpp}`: `NotsaGl3LogSkinStats()` (also at pipeline shutdown), `source/portable/play_cmd.cpp` `cmd:skinstat`, route `tools/portable/play1/r10.txt` (spawn peds, CJ fat / muscle / skinny cheats, fog + night).
* `tests/portable/rw_pipeline_gl_test.cpp` + `tools/portable/run_pipe_test.sh` (also: -DX87W define, the F-loop 3 commit had left this test unbuildable). **PIPE RESULT OK, 153 checks** (was 55 + R-9 checks). New R-10 checks: skinned == static quad (35 deg tilt + offset, ambient 0.1 / directional 0.3, fog on / off, z 3 / 6: 0 px differ, max diff 0; fails on the old shader: 5660 px) = palette + normals + lights + fog; per-bone regions (right half moves up by 1 with bone 1, left half stays); renormalised weights; no hierarchy = bind pose; 1-node hierarchy on a 3-bone skin (no stale palette); 70 bones / indices 69 and 200; indices rewritten under LOCKALL and LOCKVERTICES; real male01 / wmycr / bmycg / player (32 nodes): rest-pose palette from the frame LTMs == identity fallback (0 px differ), all 32 bones bent: GPU skin == CPU skin drawn with the default pipeline (0 px differ, pose moved 5-7k px), other yaws without GL error; audit of every skinned DFF (table above).
* In game (native, caffeinate, own lock, "Loading the Game" seen): skin stats after the CJ cheat sequence + peds: 9 skinned geometries instanced, 0 renormalised, 0 clamped, 0 over cap, 0 draws without hierarchy, 0 hierarchy/bone mismatches. Screenshots .notes/w0/r10/: r10_day_peds_cj.png, r10_cj_fat_blendgeometry.png (CJ built by CClothesBuilder, fat cheat), r10_night_fog_peds_cj.png, r10_night_clear_peds_cj.png.

## No-op proof (Win32)
No MSVC-compiled gta-reversed TU touched: gta-reversed diff = tests/, tools/, source/portable/** (portable-only glob), vendor/librw pin. librw diff 30785b1..4f8dd81 = 4 files in src/gl/ (GL3 backend only, not part of the d3d9 / null library builds: CMake lists gl/* only for LIBRW_PLATFORM=GL3). Census R/total not affected (no census-visible source changed).

## R-n items / open
| id | what |
|---|---|
| R10-1 | lighting of skinned peds is librw's (R6-3 / R-12): no exact FF/VS light model; no look tuning done |
| R10-2 | `skin.vert` ignores the SA skin vertex-shader specifics (exe VS text) beyond rigid 4-weight skinning: behaviour-preserving substitute, not bit-exact |
| R10-3 | zero-sum weight vertices (none in data) still collapse to the model origin (as in the exe's VS); not "repaired" |
| R10-4 | cutscene heads / weapons / vehicle parts not skinned: not exercised here (cutscene PROLOG1 plays per R-6); CJ clothes parts of player.img are covered by the audit (all hierarchies match) and by the CClothesBuilder in-game run, but no per-clothes-combo screenshots (no command to dress CJ) |
| R10-5 | foggy weather cheat frames: marks r10_fog_* came out as night-clear / night-fog (script clock vs cheat timing); fog equality is proven numerically in the test instead |
