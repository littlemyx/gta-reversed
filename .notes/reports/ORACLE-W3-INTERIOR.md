# ORACLE-W3-INTERIOR report

Agent ORACLE-W3-INTERIOR (rules v3), start 20:51, end 21:58 (2026-10-19), worktree C:/gta-wt/oracle-w3-int, branch oracle-w3-int. New target `wave3_interior_oracle_test` (W2 harness: deterministic arena, whole exe data image + the scratch
block compared after every case), rows in `tests/standalone/wave3_oracle_interior.inc` (fixture + helpers), `_a.inc` (furniture groups / sub-groups / manager), `_b.inc` (interior groups / interior manager), `_c.inc` (Interior_c).
47 functions, PC24 0 mismatches (also at 3x the case counts, `W3I_SCALE=3`); PC53 (CRT default precision, informational) differs only in the x87-order rows.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 3824 | **3871** |
| game_sa/Interior covered | 0 | **47 (34 A + 8 B + 5 S: GetFurniture / GetRandomId of the group and the manager, SetupPaths)** |
| uncovered A / B in game_sa/Interior | 34 / 8 | **0 / 0** |
| uncovered S / C / D in game_sa/Interior | 16 / 14 / 41 | 11 / 14 / 41 |
| port bugs fixed (one commit each, asm address and pre-fix failure count in the subject) | | **10** (9 in game_sa/Interior, 1 in PathFind.cpp; IsVisible and CalcIsVisible failed only through the IsPtInside one) |

## Fixtures
The interior manager is the fixture itself, at its exe address (`g_interiorMan` 0xBAF670, same address on both sides): eight `Interior_c` (box = a `C2dEffectInterior`, orthonormal matrix, 30x30 tile grid with rectangles of every status incl. 5, goto points,
16 interior infos, furniture-entity list), eight `InteriorGroup_c` (dense or holey interior slots, peds, entry/exit), the group list / pool and interior pool as real linked lists, the stealable-object table and interior-id table (values drawn from a small
pool so lookups hit), entry/exit and the manager's rectangle. The furniture manager likewise (`g_furnitureMan` 0xBAB380: nine groups with real sub-group lists out of the sub-group store 0xBAD3F8 and furniture lists out of the store 0xBAE1F8,
`g_currSubGroupId` / `g_currFurnitureId` at / near / past the end). Scratch block `g_sc`: boxes, peds (placement or matrix), entities with matrices, four model infos + collision models wired behind `ms_modelInfoPtrs` (bounding boxes incl. fractions
around the 0.02 round-up threshold), furniture entities, entry/exits, interior-effect infos, out parameters; camera and player peds (`CWorld::Players`) placed inside / near / far from the interiors. Tweak hooks (`Tw`) choose arguments that hit
(a point inside an interior of the group, an existing sub-group / furniture id / wealth, a stealable-object entry, a camera inside the entity box ...). ThePaths (64 loaded map areas without nodes, interior-id slots) for ArePathsLoaded / SetupPaths.

## Port bugs fixed (rows failed pre-fix; counts are PC24 failing cases of the first run on the unfixed code, logs C:/gta-scratch/w3int_run1.log / run2)
| function | asm | bug | pre-fix failing |
|---|---|---|---|
| FurnitureGroup_c::AddFurniture | 0x5C0230 | stopped at the first sub-group with the id and returned its result / false; exe walks the whole list, ignores the results and returns 1 | 448/500 |
| InteriorGroup_c::FindClosestInteriorInfo | 0x594A50 | squared distance (dx2+dy2)+dz2; exe (dz2+dy2)+dx2 (1 ulp) | 9/1200 |
| InteriorManager_c::SetEntryExitPtr | 0x598180 | returned early when the rectangle was unchanged, so m_EnEx stayed the old pointer; exe only skips the counter / alive-state reset and always stores pointer + rectangle | 67/800 |
| InteriorManager_c::GetVectorsInterior | 0x5983D0 | only the first m_numInteriors slots of a group; exe tests all 8 slots (null skipped) | 14/1000 |
| InteriorManager_c::GetPedsInterior | 0x598620 | `GetVectorsInterior(ped pos)` over every group; exe: the ped's own group (GetPedsInteriorGroup, null -> null), its 8 slots | 68/800 |
| Interior_c::IsPtInside | 0x5913E0 | `!(\|d\| <= limit)` rejects NaN; exe rejects only `limit < \|d\|` (fcompp, jnp), NaN is inside (also IsVisible 120/3000, CalcIsVisible 1/800) | 15/3000 |
| InteriorManager_c::PruneVisibleEffects | 0x598A60 | "anything to cull" test counted infos with NumFx == 0 (`rng::count` with a value) instead of summing NumFx; std::sort (exe: exchange sort, different tie / NaN order); cull `DistSq > max` (exe `!(DistSq <= max)`) | 804/1200 |
| InteriorManager_c::IsInteriorEffectVisible | 0x598690 | camera / player in the entity frame vs the local box; exe: world-space AABB of the 8 transformed corners (bigger for rotated entities), inclusive, NaN outside, player 1 optional (port dereferenced null) | 180/1500 |
| FurnitureSubGroup_c::AddFurniture | 0x5C00C0 | width / depth in double with `0.02f <= frac`; exe float (24-bit x87) with `!(frac < 0.02f)` (unordered rounds up) | 1/500 (via the group row) |
| CPathFind::AddInteriorLink (reached by InteriorGroup_c::SetupPaths) | 0x44DED0 | searched the node's six link entries for -1 and dereferenced the end iterator when all were used (assert / UB); exe: first negative entry, entry 6 = the first entry of the next node's row | 94/1000 |

## SetupPaths (0x595590, the exits / goto points -> path nodes of an interior)
Row fixture: a group of 1-3 interiors with 0-6 goto points and up to four exits each (inside / outside points, goto-point indices), the 64 map areas of ThePaths loaded but without nodes (so every exit takes the "no exterior node nearby"
branch and the unlinked-exit pass runs), interior-id slots partly occupied; compared: the whole exe image (ThePaths, the given-node arrays, the allocated interior area) and the arena (CompleteNewInterior's three allocations).
1000 cases, PC24 0 mismatches after the AddInteriorLink fix; the StartNewInterior / AddNodeToNewInterior / RemoveLinksToNewInteriorNode / CompleteNewInterior / AddNewInteriorNodeLink chain of PathFind.cpp is exercised with it and agrees with the exe.
Not covered by this row: the branch with a found exterior node (needs real exterior nodes with links in ThePaths: FindNearestExteriorNodeToInteriorNode, AddInteriorLinkToExternalNode, AddDynamicLinkBetween2Nodes_For1Node) - follow-up.

## Observed, not changed
- Rows pass `bool` / `uint8` arguments as clean 0/1 (ActivatePeds, SetStealableObjectStolen, AddSubGroup flags): the exe stores the raw byte, the port a normalised bool; every caller passes 0/1.
- InteriorGroup_c::CalcIsVisible assumes the game's dense slots (the first m_numInteriors non-null); the exe has no null check either.
- The fixture entities always have a matrix (ArePathsLoaded / IsInteriorEffectVisible / PruneVisibleEffects allocate one when missing, like the exe: needs the matrix list fixture of the entity target).
- The port's function-local lookup tables (FurnitureManager_c::GetGroupId / GetSubGroupId) are built once by a warm-up call outside the arena window (inside it they would live in the per-case arena).

## Not covered
- Class C (14): Interior_c::GetTileCentre / CalcMatrix / AddGotoPt / AddInteriorInfo / CalcExitPts / GetBoundingBox / Lounge_AddChairInfo / Lounge_AddSofaInfo / Office_FurnishCenter / Unfurnish / Exit, InteriorGroup_c::RemovePed,
  InteriorManager_c::AddSameGroupEffectInfos / GetBoundingBox: RenderWare matrix calls (RwV3dTransformPoints, RwMatrixRotate ... : exe code vs librw) or CPopulation::RemovePed.
- Class S (11): the furnishing functions (Interior_c::PlaceObject and everything that calls it: Bedroom_AddTableItem, FurnishKitchen, Lounge_AddHifi, Office_PlaceDesk / FurnishEdges / PlaceDeskQuad, Shop_Place3PieceUnit, FurnishShop: CObject creation, CWorld::Add), InteriorGroup_c::UpdatePeds (CPed::IsPointerValid -> RemovePed), InteriorManager_c::Exit.
- Class D (41): file IO (furnitur.dat loading), heap.

## Gates
Final tree (rebased on origin/reverse/interior_c, worktree C:/gta-wt/oracle-w3-int, logs C:/gta-scratch/w3int_gate_*.log): `wave3_interior_oracle_test` 47 functions PC24 0 mismatches (also 46 functions at 3x cases before the SetupPaths row was added), exit 0;
the targets that link PathFind.cpp (the only changed TU outside game_sa/Interior; no other target links game_sa/Interior): `physics_oracle_test` exit 0 (PC24 74 strict / 0 excluding NaN-payload-only), `x87_oracle_test` exit 0 (PC24 0),
`review_oracle_test` exit 0 (PC24 0), `wave3_tasks_oracle_test` exit 0 (PC24 0). The full run_all_tests / Release rebuild is left to the orchestrator.

## Follow-ups
- SetupPaths with real exterior nodes (the "exterior node found" branch: AddInteriorLinkToExternalNode, the dynamic-link insertion in CompleteNewInterior).
- Interior_c::PlaceObject and the furnishing functions (S) and the RW-matrix helpers (C).
