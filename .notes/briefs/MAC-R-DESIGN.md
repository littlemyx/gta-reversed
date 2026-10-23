# MAC-R-DESIGN — design of stream R for the portable build (read-only on source)

Host: Apple Silicon Mac. Rules `.notes/agent_rules/v4.md`. Repo /Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main`.
You write only `.notes/STREAM_R_PLAN.md` and `.notes/reports/MAC-R-DESIGN.md` (`git add -f`, `git commit -- <those>`, trailer
`Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, push fork portable/main; never force). Agent MAC-P2d edits ped/weapon save
files in the same checkout — don't touch anything else. Wall-clock cap 90 min. ≤ 1 own read-only sub-agent.

## Context
- Portable build: preset PortableMac (brew llvm 23, NOTSA_PORTABLE, librw `RW_GL3` + SDL3; vendor/librw = littlemyx/librw branch notsa).
  Census: .notes/reports/MAC-A1b.md (+ -census.md, -classified.tsv) and MAC-W0.md: ~1,998 unique root diagnostics left, stream R ~1,315
  — fakerw.h and source/standalone/rw/* (and game render code) are D3D9-specific and fail against GL3 librw.
- Win32 reference build: StandaloneRW (librw d3d9 backend) + the game's custom D3D9 pipelines (skin vs, building, reflections, shadows…)
  re-implemented in source/standalone/rw/* with exe-oracle bit-exact tests. Read .notes/LIBRW_NOTES.md, .notes/LIBRW_BUILD.md,
  .notes/P2B_SHIM_PLAN.md, .notes/P2B_SKIN_PIPELINE.md, PORTABILITY_INVENTORY.md stream R (R-1..R-5), STREAM_C_PLAN section 5 "R".

## Questions
1. Inventory of the R diagnostics by file/cause (use the classified TSV; group: fakerw API surface vs librw GL3, D3D9 types/calls in
   standalone/rw, D3D9 in game_sa render code, shaders).
2. Architecture: what stays shared between Win32 and portable (fakerw API, pipeline CPU-side math that is bit-exact), what gets a GL3
   twin (device/raster/im2d/im3d/pipelines), how to select (NOTSA_PORTABLE / RW_GL3), without touching the Win32 path. Where does librw's
   own GL3 backend already cover the need (raster formats, im2d/im3d, default pipes, skin/matfx), what is missing (SA custom pipelines:
   building/night vertex colours, vehicle reflections/env map, skin with SA specifics, shadows, water, post effects, 2D/fonts)?
3. Platform glue: librw GL3 needs a window/GL context — SDL3 (W-1 already started in source/portable/app_*.cpp): who creates the context,
   how rw::Engine::open/start get it; macOS GL is 4.1 core max — check librw GL3 shader versions work on 4.1 core.
4. Slice plan R-1..R-n for one sonnet agent each (≤ 100 min), ordered for (a) compile gate (census R → 0) first, (b) boot to the main
   menu (2D, fonts, textures), (c) world render. Acceptance per slice (census numbers; later: screenshot comparisons against the Win32
   build of the same frame — say how to get reference frames; the Windows host can produce them on request via PORTABLE_SYNC).
5. Hot files on the Windows host (git log fork/reverse/interior_c --since=3.days -- source/standalone/rw source/fakerw source/game_sa/*Render*).

## Output
`.notes/STREAM_R_PLAN.md` (inventory table, architecture, slice table with deps/acceptance/est). Return ≤ 15 lines.
