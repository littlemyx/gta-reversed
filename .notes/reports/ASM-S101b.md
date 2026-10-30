# ASM-S101b (CStencilShadows: RenderForVehicle, RenderForObject, SunSetPositionFromEntity) - 2026-10-30
Result: 3 rows: 2 DEVIATION (1 commit, shared body), 1 NOT_REACHED. Table in S101b.result.tsv.

Fix 5a066293b (RenderForVehicle 0x70FAE0 / RenderForObject 0x710310; the port shares BuildShadowVolume): the per-triangle "faces the light" test is `fcomp 0.0; fnstsw; test ah,1; jne <extrude branch>`, so the extruded-copy branch is also taken when the dot product is unordered (NaN). The port used `dot < 0.0f` (NaN went to the edge/silhouette branch). Now `!(dot >= 0.0f)`. Checked twice: the flag path (C0 set for less and for unordered) and both branch targets (0x70FFA4 extrude / fall-through 0x70FEE9 edges); same code at 0x7106BD in the object version.

Compared line by line, MATCH (stack slots decoded; Invert and 0x59C890 are cdecl and leave 8/12 bytes until the `add esp,0x20`):
- matrix: vehicle = entity matrix (AllocateMatrix + UpdateMatrix when null = CPlaceable::GetMatrix); bikes (vehicle type 9) use CalculateLeanMatrix + the lean matrix at +0x5CC; extrude 40.0 for vehicle subtype 4/3 (plane/heli) else 5.0; object 60.0 (0x858B34); early-out when colData is null or numShadowTriangles != obj->numFaces (before the size reset).
- light direction = (inv*sun) - (inv*(0,0,0)) per component; 0x59C890 sum order (up*z + fwd*y) + right*x is what CMatrix::TransformPoint does; points via GetShadTrianglePoint + 0x54EEF0 (TransformPoints).
- normal n = (e2z*e1y - e2y*e1z, e2x*e1z - e2z*e1x, e2y*e1x - e2x*e1y) from the object-space points; dot = dirZ*nz + ny*dirY + nx*dirX (same order).
- lit face: ext = world + sun*dist (z,y,x products first, single adds), output ext0, ext2, ext1; unlit face: edges (a,b),(b,c),(c,a) through 0x70FA70, output w0, w2, w1; capacity break `size+3 >= cap` (signed) before the write, size += 3; loop count cached from the int16 face count.
- 0x70FA70 (edge add / swap-remove with the last edge, 16-bit compares) matches AddOrRemoveShadowEdge.
- side quads: for each remaining edge (p,q): p, q, p+ext, q, q+ext, p+ext; capacity `size+6 >= cap` break; size += 6.
- The exe returns al=0/1 but the only caller (Process 0x711D90) ignores it.
- RenderForObject is the same instruction stream as RenderForVehicle (mnemonic diff: only prologue, the bike/extrude selection, scheduling, and `fmul [0x858B34]` instead of a stack copy of the distance).

SunSetPositionFromEntity 0x710AF0: no call and no pointer to it exists in the exe (all sections scanned). The exe takes an object whose [+4] points to a structure with a position at +0x30 (not a CEntity), normalises that and stores it in 0x8D5244..0x8D524C; the port body uses the entity position. Unreachable, left unchanged.

Tests (after 5a066293b, before rebase): StandaloneRelease build OK; StandaloneRW wave2_oracle_test rebuilt after the edit and run with RW_EXE_ORACLE = GTA_EXE = gta_sa_compact.exe: 1142 functions, mismatches PC24 0 / 0, PC53 0 / 0. PortableWin64 syntax compile (cl /Zs with the PortableWin64 compile_commands flags, x64 env in a subshell) of StencilShadows.cpp: 0 errors. Combat regress: regress left to the gate (GPU reservation held by the orchestrator gate).
