# ASM-S73 report

Slice: CutsceneMgr (29 rows), Formation (8), IdleCam (4), MentalHealth (1), Messages (2). Result table: `.notes/asm_sweep/S73.result.tsv`.
Verdicts: 28 MATCH, 14 FIXED (12 commits), 2 NOT_REACHED (no exe caller: CCutsceneMgr::RemoveEverythingBecauseCutsceneDoesntFitInMemory, CMessages::CutString), 0 open.

| commit | function | finding |
|---|---|---|
| 1d9f35b64 | CCutsceneMgr::LoadCutsceneData_preload | model-unload loop stopped at CUTOBJ19 (exe: 20 iterations, CUTOBJ01..CUTOBJ20, signed refcount `jg`); script-appended object/anim names were copied from the destination slot index instead of the i-th append slot (wrong names whenever the .CUT file defines objects too) |
| dfb2b68b8 | CCutsceneMgr::LoadCutsceneData_loading | waited for every model (exe waits only for the 300..319 cutscene models); bone attachments attached the ped object to itself (exe: attachment object = m_nCutsceneAttachmentObjectId, target = ped object) |
| edf1130de | CCutsceneMgr::Update_overlay | particle-effect stop and subtitle start tests are strict unsigned `>` (jbe), port had `>=` (one frame early) |
| 2d229fb5d | CCutsceneMgr::DeleteCutsceneData_overlay | objects freed last to first; `ms_useCutsceneShadows` (0x8AC158) reset to 1; `CPad::Clear` after the streaming restore, not before MakePlayerSafe |
| 6193fa214 | CCutsceneMgr::HideRequestedObjects (+ CalculateBoundingSphereRadiusCB) | the hidden-entity counter holds the number of `remove` lines on entry and must be reset to 0 before the entities are registered (port appended after it: wrong slots, 50-slot overrun); sphere CB stores only a strictly greater radius |
| 4d7d0d821 | CCutsceneMgr::UpdateCutsceneObjectBoundingBox + CreateCutsceneObject | radius starts at the col model's current radius and is capped at max(20, current); CreateCutsceneObject zeroes the radius first. Port started at 0 and never capped (cutscene object col spheres could be far larger than the exe) |
| 09c0236ec | CFormation::GenerateGatherDestinations | angle = (i/n*2pi - rot) + pi/n (association) |
| ebcb9208a | CFormation::DistributeDestinations (+0x69B1B0, CPointList::GetCentroid) | centroids are sum*(1/n); distances sqrt((dz2+dy2)+dx2); nearest-destination search compares the strict float distance starting from 1e7 and adds it to the cost (port: squared distance from FLT_MAX, sqrt'd afterwards); best cost starts at 999999.875 |
| 9f87a1eb7 | CFormation::DistributeDestinations_CoverPoints | NaN arm of the distance prefilter |
| 117aa1a58 | CFormation::DistributeDestinations_PedsToAttack | nearest pair by strict sqrt 2D distance from 999999.875 (port: squared from FLT_MAX) |
| 89a82d8e1 | CFormation::FindCoverPoints | speed < 0.005, max.z < 1.5, up.z > 0.95 prefilters skip on NaN like the exe |
| 4c90961a0 | CIdleCam::ProcessFOVZoom | FOV settle tests take the interpolation arm on NaN |

## Notes

- LoadCutSceneFile (the NOTSA split of the inlined .CUT parser in 0x5B05A0) uses CFileLoader::LoadLine (comma to space, tolerant of CRLF) and starts_with section names; the exe uses exact `strcmp`-style matches ("end" and section names must be the whole line). Equivalent for valid .CUT files. The port also allocates `size * 2048 * 2048` bytes for the file buffer (the exe `size << 11`), harmless but wasteful; left as is.
- `m_bDontClearZone` is never set (neither in the port nor any exe caller found), so the weapon-restore block in DeleteCutsceneData_overlay is dead.
- ProcessTargetSelection: the exe's two consecutive `SetTarget(player)` calls collapse to one, because SetTarget zeroes the LOS counter that guards the second call; the port's single `SetTargetPlayer` is equivalent.
- MentalHealth: vehicle health is a byte compared with 300/600 in the exe too, so no vehicle health event can ever fire; the port keeps that.
