# ASM-S71b report

Base 599510ff7 (origin/reverse/interior_c), rebased onto afb629303. Result table: `.notes/asm_sweep/S71b.result.tsv`.

| row | verdict | commit |
|---|---|---|
| CMatrix::WithUp (extra task) | FIXED: normalises right and forward (both exe users do) | 6709488f0 (header), c3446b6f1 (callers) |
| CRopes::CreateRopeForSwatPed (0x558D10) | FIXED: id always returned, counter always advanced + wrapped to a byte (port inverted the RegisterRope bool) | daee39645 |
| CRope::CreateHookObjectForRope | FIXED: unknown types return silently | 6ec5e682f |
| CRope::Render | FIXED: x87 sqrt strict `< 120` | dd8d9c642 |
| CRope::PickUpObject | FIXED: reference registered on the attach-object slot, hook pos via TransformPoint | 9b31d499d |
| CWaterCannon::PushPeds | FIXED: m_bUsesCollision filter, active-section check, 5.0 test, ped-speed add, strict bbox | f9cdbc20a |
| CWaterCannon::Render | FIXED: 31 iterations, curr-prev basis, vertex/LOS order, SmokeII3expand particle, draw on hit, ftol'd ms, extra rand() | 534868192 |
| CWaterCannons::Render | FIXED: skip idle cannons | 7ad3a6f29 |
| CRope::Remove, CRopes Shutdown/Update/Render/RegisterRope, C3dMarker AddMarker/DeleteMarkerObject | MATCH | - |
| CRope::Update (0x557530) | UNSURE: not traced (out of budget) | - |

## Notes

- WithUp: the only two exe users are `C3dMarker::Render` (0x72247E..0x7224F6) and the inlined arrow code in `DirectionArrowsDraw` (0x7213BC..0x721413); both call CVector::Normalise on right and forward and keep `up` as given. S72's local `ExeArrowUpMatrix` was folded back into `CMatrix::WithUp` (the unity early-out for up == (0,0,1) is the same).
- CWaterCannon::Render: in the exe `prev` is the slot of `m_nSectionsCount % 32` (EBP/ESI) and `curr` the previous one (EBX/EDI); the quad vertices are prev-axis, prev+axis, curr-axis, curr+axis per axis (up, right, fwd), `ProcessLineOfSight(curr, prev)`, up = cross(curr - prev, `TheCamera.m_mCameraMatrix` forward @0xB6F9AC). The hit sets a flag that ends the loop on the next iteration, so the hit section's quads are still drawn.
- CWaterCannon::Render calls rand() once on entry (0x728E28) and discards the result; this is why the idle-cannon skip in CWaterCannons::Render matters.
- PushPeds: `test byte [ped+0x1C], 1` is CEntity::m_bUsesCollision; the old code tested physicalFlags.bMakeMassTwiceAsBig.
- CRopes::RegisterRope was traced end to end (existing-rope reset, new slot, both jump tables, both unrolled segment loops): MATCH.
- Open: CRope::Update (about 1300 bytes at 0x557530) was not compared; the port already has per-expression x87 annotations (earlier review).
