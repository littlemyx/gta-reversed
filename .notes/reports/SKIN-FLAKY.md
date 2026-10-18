# SKIN-FLAKY: intermittent null deref in rw::d3d9::instanceMesh (2026-10-18/19)

Status: NOT REPRODUCED (62 clean loads of the crashing scene + the original-state sample), no fix. The brief's premise was wrong; the real trigger is the story cutscene INTRO1B, not the Beach Party cheat.

## Finding 1: Beach Party is a red herring (log evidence, `C:/gta-scratch/gr_out/f5/cheats`)
* The f5 route was the OLD cheats route with `cmd:tp:2495:-1688:13.5` (Grove Street, the opening-mission trigger) after `free_ready`; the tp was removed from `gen_cheats_scenario.py` afterwards (f6+ never reached the scene).
* Timeline in f5 `log.log`: tp -> `Loading cutscene data ("INTRO1A")` 11:48:09 -> all 18 typed cheats are ignored (`CCheat::AddToCheatString` returns while `CCutsceneMgr::ms_running`; every probe of f5 shows money 350, cheated=0, no flag, unlike f9+ where HESOYAM gives 250350) -> `Loading cutscene data ("INTRO1B")` 11:50:00.145 -> `Reading animation csflower..cssweet` (.424-.435) -> crash 11:50:00.494 (350 ms later, first frames after the anims were read).
* The second Beach Party press had no effect (the second press is the OFF branch, which the exe leaves empty: 0x439230 `je 0x43935b`); the match with the cheat typing is a coincidence of the script clock (the 18th cheat was typed during the gap between the two cutscenes).
* The crash: `geo->meshHeader == nil` AND `geo->instData == nil` AND not NATIVE on the atomic handed to `rw::d3d9::instanceMesh` (fault address 0x6 = `meshHeader->serialNum`, ESI = 0, EBX/EAX = the Geometry). The atomic is rendered through `CEntity::Render` <- `CRenderer::RenderOneNonRoad` (a world entity of the visible list, skinned = a ped or a cutscene ped).

## Finding 2: reproduction attempts (all on HEAD a95935dd unless noted; INTRO1B load verified in `log.log` of every counted run)
Repro route = header_free (new game, free CJ) + `cmd:tp` to Grove Street + wait for INTRO1A, skip it with Return (`until:cutscene`, wait 9 s, `key:return`) or let it play (111 s), wait for `Loading cutscene data ("INTRO1B")` + 35 s, `cmd:quit`. About 50 s per run (skip) / 3.7 min (no skip). Scripts in `.notes/reports/SKIN-FLAKY-repro/` (loop.sh = GPU-locked loop, gen_skip2.py = route, stress_*.ps1 + busy.py = 16 idle-priority spinners that saturate the CPU while the game runs at idle priority).

| variant | exe | env | INTRO1B loads | crashes |
|---|---|---|---|---|
| tp, no skip (as f5) | HEAD | NOVSYNC | 6 | 0 |
| tp, no skip, instrumented | HEAD+diag | NOVSYNC | 3 | 0 |
| skip INTRO1A | gr_exe/head.exe (09:53 build, the oldest suite exe; f5's neighbour) | NOPRESENT (as f5) | 13 | 0 |
| skip, CPU saturated (16 spinners, game at idle priority) | HEAD+diag | NOVSYNC | 4 | 0 |
| skip, CPU saturated + the user's playtest-1 gta_sa.set (`USERDIR_SEED=playtest1`) | HEAD+diag | NOVSYNC | 36 | 0 |
| total | | | 62 | 0 |
(plus about 10 other runs that never reached INTRO1B: Beach Party x2 without the tp, skip route cut short: 0 crashes, not counted). Rate in f5 = 1 crash in 1 INTRO1B load seen there; across all later loads 0/62, so the true rate is below ~5% (95% bound 3/62) or the f5 state had a trait the loop lacks (user's Documents gta_sa.set at that time was the real one with its AA/fx settings, 3-4 other agents were compiling and playing with the GPU lock; tp then wait 111 s of INTRO1A playing).

## Diagnostics in place (diag.patch, not committed to the sources)
`.notes/reports/SKIN-FLAKY-repro/diag.patch` (apply on HEAD): `SkinPipeRender` logs `[SKINBAD]` (atomic, geometry words, the CEntity being rendered: type, model, streaming state, ped type/state, cutscene flag) when a geometry without mesh header and instance data reaches the skin pipeline; `ConstructGeometryAndSkinArrays` / `BlendGeometry` / `CopyGeometry` log `[SKINDBG]` when `RpGeometryUnlock` fails or a body part has geometry but a NULL material. Nothing was ever logged in 62 loads, so (a) no geometry without mesh header was rendered, (b) the clothes builder's unlock never failed in those loads.

## Hypotheses (not confirmed; ranked)
1. A geometry whose `RpGeometryUnlock` failed: `BuildMeshHeader` (source/standalone/rw/geometry.cpp) returns false on a triangle with matId >= numMaterials (e.g. 0xFFFF set by `RpGeometryTriangleSetMaterial(NULL)`), leaving `meshHeader == nil`; the exe would index `materials[0xFFFF]` instead. In the clothes path that needs `ms[i] == NULL` with `gs[i] != NULL` in `CClothesBuilder::CreateSkinnedClump` (0x5A6B2C: texture not found in the dict) - for the cutscene player (`RebuildCutscenePlayer`, `bCutscenePlayer` skips `ConstructTextures`, so the dict is csplay's streamed TXD). Not seen in 62 loads; would need a TXD that is not (fully) loaded when `RebuildCutscenePlayer` runs (streaming thread timing).
2. Streaming race: `CdStreamThread` / `CdStreamSync` are the exe's (non-atomic `nSectorsToRead || bInUse` check); a model converted from a half-read buffer would give a corrupt DFF (invalid triangles -> hypothesis 1). Disk/CPU contention at f5 time would raise the odds; the CPU-saturated loop did not.
3. Use-after-free of a Geometry (zeroed block seen as meshHeader=0, instData=0, flags without NATIVE): a ped/cutscene clone whose geometry refCount is one short (`ClothesBuilder.cpp` has "RpGeometryAddRef [0x74CCB0] missing" TODOs and does `refCount++` by hand) while `RemoveCurrentZonesModels` / `DeleteRwObjectsBehindCamera` run inside the cutscene load. No evidence yet.

## Next steps for a continuation
* Run `.notes/reports/SKIN-FLAKY-repro/loop.sh` with the diag exe in the background for a few hundred loads (50 s each with the skip route; no GPU needed beyond the lock) and keep stress + seeded userdir on; the first `[SKINBAD]` line names the entity, which settles 1 vs 3 (loadstate / ped state / model) without a crash.
* Add a `cmd:` injector item (diagnostics) that calls `CCutsceneMgr::LoadCutsceneData("INTRO1B")` repeatedly, to hit the clothes rebuild + zone-model removal path hundreds of times per minute instead of once per 50 s.
* Compare 0x5B13F0 (`LoadCutsceneData_overlay`), 0x40B080 (`RemoveCurrentZonesModels`), 0x4D5E80 against the port line by line (not done: no deviation was found by reading `CutsceneMgr.cpp` 330-450 and `Clothes.cpp` 145-220).
* Fix proposal only after a hit; do NOT add a meshHeader null guard in `instanceMesh`/`SkinPipeRender` (the exe has none).

## Housekeeping
Suite/doc notes to correct: `PROBE-SUITE.md` says the crash was "right after the second Beach Party press"; it was the INTRO1B cutscene load (see Finding 1). `gameplay_regress.sh cheats` (no tp) never reaches the cutscene and is unaffected.
