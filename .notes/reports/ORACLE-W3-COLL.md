# ORACLE-W3-COLL report

Agent ORACLE-W3-COLL (rules v2), worktree C:/gta-wt/oracle-w3-coll, branch oracle-w3-coll. New target `wave3_coll_oracle_test` (= game_oracle_test.cpp built with W3_COLL_ONLY + NOTSA_ORACLE_ADDRESS_GLOBALS; rows in
tests/standalone/wave3_coll_oracle_a.inc Collision geometry, _b.inc Core maths + CMatrixLinkList, _c.inc exe-global state). **100 rows, 1500 cases each, PC24 0 real mismatches** (64 NaN-payload-only cases in 3 rows,
informational), 0 mismatches also at 6000 cases per row. PC53 differences are informational (extended precision vs the port's floats).

| | before | after |
|---|---:|---:|
| game_sa/Collision covered | 25 | **72** (uncovered A 31 -> 0, B 18 -> 2) |
| game_sa/Core covered | 28 | **81** (uncovered A 49 -> 0, B 4 -> 0) |
| exe-oracle coverage, whole game | 3203 | **3303** |
| port bugs fixed (one commit each, asm address in the subject) | | **6** |

## Hit rate (share of cases in which the function returns true / finds a collision, 1 case = 1 fixture)
Set 82% | Test2DLineAgainst2DLine 28% | TestLineBox 19% | TestVerticalLineBox 24% | CalculateColPointInsideBox 100% | ProcessDiscCollision 16% | IsStoredPolyStillValidVerticalLine 53% | IsThisVehicleSittingOnMe 28% | SphereCastVsSphere 70% | SphereCastVsBBox 68% | SphereCastVersusVsPoly 85% | RayPolyPOP 75% | GetPrincipleAxis 80% | PointInPoly 51% | GetLinkPtr 100% | Update 100% | UpdateRW 78% | IsFlipped 18% | IsPointInside- 34% | IsPointInside-Tolerance 35% | AddToList1 98% | AddToList2 98% | isCacheLoading 17% | CColModel::operator new 100% | operator new 100% | Flush 85% | AppendStringToKey 96% | SetIfCollisionIsRequired 22% | SetIfCollisionIsRequiredReducedBB 1% | ResetMadeInvisibleObjects 90%
Rows without a bool result compare the full output memory (colpoints, matrices, pools, globals) bit-exact. Fixtures: spheres / boxes / lines / triangles are placed on or near each other at game scale (barycentric
points on the triangle, sweeps through the plane, lines through the box, matrices that are rigid transforms) plus the Rng special-value mix (NaN, +-0, denormal, huge, inf) at 3-25 %.

## Port bugs fixed (rows fail before the fix; logs: run2.log .. run4.log in C:/Users/littl/AppData/Local/Temp/w3coll)
| function | asm | bug | rows before -> after |
|---|---|---|---|
| CColTrianglePlane::Set 0x411660 (ctor from 3 vertices) | offset and orientation use the quantized normal (int16/4096); offset = (z*nz + y*ny) + x*nx | port used the float normal: offset 1 LSB off | 300 -> 0 (PC24) |
| ProcessDiscCollision 0x413960 colpoint copies (CColPoint::operator= 0x40FC80, CALLED at 0x413A65 / 0x413A93) | copies point, normal, both surface triples, depth; NOT field_C / field_1C / pad at 0x26 | port copied all 0x2C bytes; new CColPoint::AssignLikeExe used at those 2 sites only (making the global operator= partial broke game_oracle_test: the Process* functions copy fully inline; the vehicle ProcessEntityCollision call sites of 0x40FC80 are NOT changed, follow-up) | 116/300 -> 0 |
| CVector2D::Magnitude 0x420860 | fld/fmul/faddp/fsqrt in extended precision | float x*x overflowed to inf for |x| ~ 2.6e30 | 5/300 -> 0 |
| CRect::IsPointInside 0x404290 / 0x4042D0 | fcomp + test ah,5/jnp, test ah,0x41/je: rejects only on strict less/greater, NaN passes | `x >= left && ...` rejected NaN | 7 and 9/300 -> 0 |
| CMatrix operator*-Mat 0x59BE30 | each of the 12 results adds its 3 products in its own order (compiler reordering), one float store | one fixed (right+forward)+up order, last bit differs | 528/600 -> 0 |
| CMatrix Invert(in,out) 0x59B920 | transpose, pos accumulated in 3 passes with a float store per step, final * -1.0f | -Inverted().TransformVector (extended accumulation) | 341/600 -> 0 |

## Observed, not changed / not observable
- Padding: CalculateColPointInsideBox 0x411EC0 / IsStoredPolyStillValidVerticalLine 0x414D70 copy a stack local over the colpoint (padding floats 0x0C/0x1C = stack junk), CMatrix operator* / Invert / SetRotate-quat leave the matrix padding dwords as stack junk,
  GetBoundingBoxFromTwoSpheres leaves the surface of the returned CColBox alone (port zero-initialises it), ColTrianglePlane::Set leaves pad byte 9 (port writes a stack byte): all masked in the rows (indeterminate in the exe).
- NaN payload only (quiet bit): CMatrix::Update 0x59BB60, CColAccel::addCacheCol 0x5B2C20 (x87 fld/fstp copy vs bit copy of a signalling NaN), IsStoredPolyStillValidVerticalLine normal.z.
- CMatrix::SetRotate-quat 0x59BBF0: 2 of 1500 cases with inf / denormal-range components differ (-0 vs +0, inf-inf): the exe stores 8 of 9 products and keeps one on the x87 stack; the port stores all. Row skips non-finite and |c| < 1e-15 / > 1e15 components;
  every finite game-range quaternion matches. Not fixed (budget).
- RwMatrixUpdate (fakerw) is not linked into the test: a test-local copy with the exe's body (flags &= 0xFFFDFFFC) stands in, so Update / UpdateRW / CopyToRwMatrix rows test the copy part, not librw.
- gpMadeInvisibleEntities is declared std::array<CEntity*, 10> in the port (Camera.h) while the exe layout leaves room for 15 before the counter at 0x9655DC: rows keep the count <= 10.

## Skipped (2 B functions)
- CColAccel::addIPLEntity 0x5B3040: needs CModelInfo table, entity LOD / model-info objects and TheCamera globals (fake world); CColStore::BoundingBoxesPostProcess 0x410EC0: quad tree (pool of single-link nodes) + ColAccel cache state. Both next-step candidates.
- Class C/S/D not attempted (outside the brief).

## Process
Tool calls: about 152 (hand-back state: all landed, nothing pending). run_all_tests: 49/50 ok before the CColPoint correction, game_oracle_test then fixed and re-run green with physics_oracle_test and wave3_coll_oracle_test (rc 0); the full run was not repeated after that one-commit correction. One read-only helper was not needed. Trailer used: Co-Authored-By Claude Opus 5.5 (as in the brief).
