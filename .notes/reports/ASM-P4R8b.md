# ASM-P4R8b: plant chain + P4R8 leftovers

Result: 16 rows, all MATCH, no source changes (no build needed). Oracle: gta_sa_compact.exe 1.0.

## Rand draw sequence (exact, as the exe does it)
Chain: PreUpdateOnceForNewCameraPos(0x5DCF30) -> _ProcessEntryCollisionDataSections(0x5DCD80) -> _AddLocTris(0x5DC8B0) -> CPlantLocTri::Add(0x5DC290).
Per colliding-entity (in m_CloseColEntListHead order), face groups in memory-DESCENDING order (or the whole 0..n-1 range if no face groups),
triangles i = first..last ascending. Triangle is considered when: (mask==0xFAFAFAFA || mask==(i&7)), entry.m_Objects[i]==0, unused LocTri list non-empty,
and at least one of 7 points (v0,v1,v2, centroid, 3 edge midpoints) has (z^2+y^2)+x^2 of (cam - point) < 10000; AND surface creates plants or objects.
Inside Add (no rand drawn before this point):
1. If createsObjects && !createsPlants: NO rand, list 3, return.
2. density_k = (uint16)ftol(area * surf.plants[k].density), k = 0,1,2 (area = 0.5*sqrt((z^2+y^2)+x^2) of the cross product); computed first, no rand.
3. If d0+d1+d2 > 0: exactly 3 rand() draws in this order: seed[0], seed[1], seed[2], each = (float)(rand() * (1/32767)) (the exe constant 0x858C7C = 3.0518509e-05; rand() is the 15-bit CRT LCG, no CGeneral wrapper, no range arithmetic).
4. Else if createsObjects: no rand, bit0 cleared, list 3. Else: no rand, return 0 (entry stays empty and is retried next time).
So: 3 draws per triangle that gets plants, 0 otherwise; ProcessTriangleAdded (objects) is not part of Add's draws.
Update(): only entries in mask (counter&7) are re-scanned; Remove runs first (releases far triangles), a released/failed slot is re-added on later passes (new draws).

## Leftovers
CColTrianglePlane::Set tail, Interior_c::ResetTiles (all), CTaskSimpleFacial::GetAnimId: MATCH (see tsv).
Float-vs-x87 noise: none can change a rand-dependent decision (all sums already in exe order, PC24 = float rounding).
