# ORACLE-W3-PATHS report

Agent ORACLE-W3-PATHS (rules v3), start 02:28, end see the hand-back (2026-10-20), worktree C:/gta-wt/oracle-w3-paths (branch oracle-w3-paths, base 6b03f214).
Rows are added to the existing `wave3_world_oracle_test` (W2 harness, ORACLE-W3-WORLD fixture): `tests/standalone/wave3_oracle_world_d.inc` (route finding), `_e.inc` (WaterLevel), `_f.inc` (Shadows), `_g.inc` (CWorld sector-list templates).
The target now also links Collision.cpp, Rect.cpp, RenderBuffer.cpp, Shadows.cpp, WaterLevel.cpp. 100 rows in the target (75 + 25 new), PC24 0 mismatches (exit 0). PC53 (CRT default precision, informational) differs in x87-order rows only.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 4032 | **4057** |
| game_sa root files covered | 1097 | **1122** |
| uncovered A / B in the root files | 314 / 457 | **310 / 436** |
| port bugs fixed (one commit each, asm address + pre-fix failing count in the subject) | | **7 commits** |

## Route finding (share of cases that find a route)
Fixture for the route rows: the 64 map areas get a SYMMETRIC graph (a->b and b->a with the same length, up to 4 links per node at link slots 4n..4n+3, link lengths 1..60 mostly and 0..255 in 1/12), all nodes start with
m_totalDistFromOrigin = 32766, 90% of the cases have one flood-fill group, car-path links (`m_pNaviLinks`) point to links whose attachedTo is the linked node in 2/3 of the cases, 85% of the areas loaded.
Symmetric because the exe walks back from the origin along links of decreasing distance and loops forever when none matches (one-way random links hang both the exe and the port).
Hints: valid map node 80%, interior node 10%, invalid (area 0xFFFF) 10%; positions: node position + noise, random, or specials (NaN / inf / zero / tiny) for the closest-node fallback. Parameters: maxNodes 0..20, maxDist / maxDepth (999999.9, random, NaN), sameLaneOnly 25%, water transitions 33%, boats 17%, forbidden node 33%, ptype 0/1/2.

| row | cases (both precision passes) | route found (>= 1 node) | >= 3 nodes |
|---|---:|---:|---:|
| CPathFind::DoPathSearch 0x4515D0 | 3000 | 1070 = **35.7%** | 29.1% |
| CPathFind::ComputeRoute 0x452760 | 1600 | 957 = **59.8%** | 59.3% |
| CPathFind::FindNextNodeWandering 0x451B70 (standard directed fixture) | 5000 | 4350 target found = 87.0% | n/a |

0 mismatches in all three, no change needed (the digest's "same logic, minor deviations" holds: resolve order, address vs node-id compare and the walk-back stores are equivalent). ComputeRoute's share is higher than DoPathSearch's because it passes no
target hint and no distance limits. The remaining non-routes are: origin == target, different flood-fill group, interior / invalid hints, unloaded areas, depth / distance limits, forbidden node, lane filter.

## Port bugs fixed (rows failed pre-fix; counts are PC24 failing cases of the first clean run on the unfixed code)
| function | asm | bug | pre-fix failing |
|---|---|---|---|
| CWaterLevel::AddWaterLevelVertex | 0x6E5A40 | coordinate <= -3000 or >= 3000 (bound included) is clamped AND the parameters reset to {z 0, bigWaves 1.0f, smallWaves 0, flow 0} (port: strict via CRect::DoConstrainPoint, bigWaves 0). Also fixes AddWaterLevelQuad / Triangle (1477/1500 and 1470/1500 failing before the vertex fix, all through the vertex table) | 490/1500 |
| CWaterLevel::FindNearestWaterAndItsFlow | 0x6E9D70 | NaN camera coordinate passes the four world-bound tests (port rejected it) | 2/1200 |
| CStaticShadow::Free | 0x707670 | m_pPolyBunch / m_nId cleared also when there is no poly bunch (the `je` of the null test jumps to the stores) | 121/600 |
| CShadows::RemoveOilInArea | 0x7074F0 | strict bounds (minX < x < maxX ...), NaN rejects (port: CRect::IsPointInside, inclusive, NaN passes) | 555/1000 |
| CWorld::FindObjectsInRangeSectorList | 0x563500 | distance^2 must be strictly < radius^2 (equal / NaN out) | 12/1500 |
| CWorld::FindObjectsOfTypeInRangeSectorList | 0x5635C0 | same | 6/1500 |
| CWorld::FindNearestObjectOfTypeSectorList | 0x565450 | the radius argument is never read: an entity wins only when strictly nearer than the running *outNearestDist (the callers pass the same variable); the port compared with its by-value copy of radius, so the last entity in range won, not the nearest. Also the 3D distance sums (dx2 + dz2) + dy2 (1 ulp) | 453/1500 |

## Rows that pass without a change
DoPathSearch, ComputeRoute, FindNextNodeWandering; WaterLevel: AddWaterLevelQuad, AddWaterLevelTriangle (after the vertex fix), BlockHit (with MarkQuadsAndPolysToBeRendered), FillQuadsAndTrianglesList (with AddPolyToBlock), RenderSeaBedSegment, RenderDetailedSeaBedSegment;
Shadows: TidyUpShadows, PrintDebugPoly, StoreShadowToBeRendered (Texture and Type overloads), AffectColourWithLighting, UpdateStaticShadows; CWorld: FindObjectsIntersectingCubeSectorList, FindMissionEntitiesIntersectingCubeSectorList, FindObjectsIntersectingAngledCollisionBoxSectorList.

## Observed, not changed (fixture limits / not game states)
- FindNextNodeWandering: the node id of an address whose area is 0xFFFF is stale stack in the exe (the hidden result slot of FindNodeClosestToCoors is the caller's uninitialised local); the row normalises it.
- AddWaterLevelQuad: with corners that collapse onto each other after the world clamp (coordinates beyond +-3000) the exe's 2+2 corner sort reads uninitialised stack slots (12/1500 cases before the rows were restricted to -3000..3000); the rows use coordinates inside the world.
  AddWaterLevelVertex / Quad / Triangle write caller stack garbage into the vertex padding (+0x12) and into the unused bits / padding after a polygon's flag byte; the rows mask those bytes.
- CWorld rows: very small distances (denormal range, 1e-20) differ between the x87 (extended exponent range) and the float code, no game values; the entity scatter avoids them. FindNearestWaterAndItsFlow differs at PC53 in 88 cases (informational, PC24 0).
- The CWorld template instances the port emits are the typed ones (CObject* / CPed* / CVehicle* / CBuilding* / CDummy*); the rows call the CObject* instance (identical layout). The hook name `<CPtrListDoubleLink<CPhysical*>>` has no instance in World.cpp.
- AddWaterLevelVertex pre-fix count 490/1500 was measured after the padding mask was added; the world edge (+-3000) is a real water.dat value, so the edge vertices of the ocean got bigWaves 0 instead of 1.0 in the port.

## Not covered
- Shadows: RenderExtraPlayerShadows 0x707FA0 (vehicle with col model + point lights + player), the C/S/D Cast / Store / Render functions.
- WaterLevel: the C / D render and split functions (RenderWater*, SplitWater*AlongXLine, Render*Rectangle ...).
- CWorld: FindObjectsKindaCollidingSectorList (virtual GetBoundCentre needs a real vtable), FindLodOfTypeInRange (needs the LOD sector grid), SetAllCarsCanBeDamaged / StopAllLawEnforcersInTheirTracks / FindUnsuspectingTargetCar (vehicle pool: logic_oracle_refs fake pool + Enter/Leave, not started),
  CallOffChaseForAreaSectorList*, TestForUnusedModels / TestForBuildingsOnTopOfEachOther, CastShadowSectorList, ProcessLineOfSight* / ProcessVerticalLine* / GetIsLineOfSightSector* (need collision data), TriggerExplosionSectorList, TestSphereAgainstSectorList, Process*.
- PathFind: UpdateStreaming / Shutdown / LoadPathFindData (file IO / allocator), GeneratePedCreationCoors*.

## Gates
`wave3_world_oracle_test` 100 rows PC24 0 mismatches, exit 0. Targets linking the changed TUs (Shadows.cpp: physics_oracle_test; WaterLevel.cpp: world_oracle_test; World.cpp: only this target): see the hand-back. The orchestrator runs the full run_all_tests.
