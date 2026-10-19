# ORACLE-W3-ENTITY report

Agent ORACLE-W3-ENTITY (rules v3), start 19:10, end ~20:30 (2026-10-19), worktree C:/gta-wt/oracle-w3-entity, branch oracle-w3-entity. New target `wave3_entity_oracle_test` (W2 harness: deterministic arena, whole exe data image + the fixture
compared after every case), rows in `tests/standalone/wave3_oracle_entity.inc` (fixture + helpers) and `_a.inc` (CEntity / CPhysical / CAnimatedBuilding), `_b.inc` (CPlaceable + matrix list), `_c.inc` (damage record, lighting, friction,
pool pointers), `_d.inc` (occlusion, lighting early-outs); TU `tests/standalone/wave3_entity_tu.cpp` (Entity.cpp compiled in, exports the file-static `SetCompAlphaCB`). **51 functions, PC24 0 mismatches** (also at 3-4x the case counts,
`W3E_SCALE`); PC53 (CRT default precision, informational) differs in the x87-order rows.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 3772 | **3824** |
| game_sa/Entity covered | 39 | **87** |
| uncovered A / B in game_sa/Entity | 19 / 29 | **0 / 0** (SetupLighting / RemoveLighting: early-out branches only, see below) |
| port bugs fixed (one commit each, asm address in the subject) | | **11** |

## Fixtures
One block `g_fx` (same addresses on both sides): three physical entities A / B / C (garbage of the full vehicle size, then type, model index, matrix node or placement-only, collision counters / entries), the two model infos + collision models
behind the exe's `ms_modelInfoPtrs` (0xA9B0C8; the table is restored after each case), the 12 matrix nodes behind the exe's `gMatrixList` (0xB74288, three intrusive lists, free list sometimes empty so the oldest link is evicted), placeables
owning the other nodes, a collision point, output vectors. All calls are qualified (no vptr read). Row-specific state is set by a tweak hook after the standard build: timer / wind / weather, `CReferences` (RegisterReference chains for the damage
entity), TimeCycle colours + `gfLaRiotsLightMult`, glass entity count, fake building / dummy pools (16 slots, random slot states) and the rf ped / vehicle / object pools for `IsEntityPointerValid`, camera view matrix + active occluders built
around the entity (lines containing its screen centre, planes separating camera and entity) for `IsEntityOccluded`.

## Port bugs fixed (rows failed pre-fix; counts are PC24 failing cases of the first run on the unfixed code, logs C:/gta-scratch/w3entity/run*.log)
| function | asm | bug | pre-fix failing |
|---|---|---|---|
| CPhysical::RemoveRefsToEntity | 0x544280 | `while (i < n);` with an empty body and i = n: never removed anything; exe walks from 0 with an int16 index and re-tests the slot after a shift | 414/600 |
| CEntity::SetupBigBuilding | 0x533150 | set m_bStreamingDontDelete (bit 10); exe sets bit 16 (m_bDontCastShadowsOn) | 171/200 |
| CEntity::BuildWindSockMatrix | 0x532DB0 | wind direction not negated (x, y) | 500/500 |
| CEntity::ModifyMatrixForBannerInWind | 0x535040 | scaled by CWeather::Wind (the selected 0.2/0.43/0.66 constant went to an unused variable), z via pow() | 350/500 |
| CEntity::GetIsTouching(centre, radius) | 0x5344B0 | float sum x2+y2+z2 (overflow to inf, wrong order, NaN handling); exe (z2+x2)+y2 in extended range, ordered R2 > sum (verbatim x87 block) | 1/600 |
| CEntity::CalculateBBProjection | 0x535340 | axis lengths used the 3D magnitude, the exe uses XY only (changes the dominant axis for tilted entities); fabs vs `v < 0 ? -v : v` | 206/800 |
| CPhysical::GetLightingFromCol | 0x5447B0 | `lighting *= (scale1 + RGB2 term)`, exe `lighting * scale1 + RGB2 term` (also GetLightingTotal) | 267/800 (266/1000) |
| IsDummyPointerValid | 0x532730 | called CPool::IsObjectValid (slot in use, idx < size); exe: null -> false, no slot check, `idx <= size` (reached via IsEntityPointerValid, type DUMMY) | 176/2000 |
| IsPointInsideLine | 0x71E050 | `>= radius`; exe returns `!(d < radius)` (equal and NaN inside) (also CActiveOccluder::IsPointWithinOcclusionArea) | 17/2000 (6/2000) |
| CEntity::IsEntityOccluded | 0x71FAE0 | corners transformed through the placement and all six precomputed; exe goes through GetMatrix() one by one (a placement-only entity gets a matrix allocated, also after a failed corner), NaN-aware size / distance compares, signed 16-bit distance, longEdge `x > y ? x : y` | 99/3000 |
| CPhysical::ApplyFriction(other) | 0x545980 | speed magnitudes via CVector::Magnitude (double sum x,y,z); exe sqrt((x2+z2)+y2) (z,y,x in the second branch) with 24-bit rounding and extended exponent range, reciprocal multiply in the first branch | 65/1500 |

Two helper asm-vs-source comparisons (read-only, one at a time) were used for CalculateBBProjection and ApplyFriction.

## Observed, not changed
- The exe ApplyFriction(other) reads the other entity matrix without a null check (`[edi+0x14]`, 0x545BD7): placement-only entities fault there, the port handles them. The row keeps both entities matrices non-null (the game physicals always have one).
- Rows keep float-overflow-prone inputs out of the float helpers: squares of 1e30-range values overflow float but not the exe extended registers (CalculateBBProjection, lighting, ApplyFriction: `ClampHuge` / `g_flMask`), denormal-range colour / speed values are flushed to 0 (extended registers keep more bits), ApplyFriction takes NaN / 0 specials only (inf inputs give compare outcomes that differ between float and extended evaluation). Not game values.
- RenderWare objects: all entities have m_pRwObject == null, so UpdateRwMatrix / UpdateRwFrame (called by BuildWindSockMatrix / ModifyMatrixForBannerInWind) are the no-op branch; the matrix part is compared.

## Not covered (tests/standalone/tools/w3_entity_skip.tsv)
- CEntity::SetupLighting 0x553DC0 / RemoveLighting 0x553370: the rows compare only the early-outs (`m_bLightObject == false`, `reset == false`); the full paths call RpLightSetColor / RpLightSetFlags on the global lights and CPointLights::GenerateLightsAffectingObject (RenderWare light API: exe code vs librw, point-light tables). The coverage tool counts them as covered.
- Class C (22) / S (11) / D (30) of game_sa/Entity are outside the brief.

## Gates
`wave3_entity_oracle_test` 51 functions PC24 0 mismatches, exit 0 (final tree: run17 / run19 logs). run_all_tests: see the hand-back / PHASE2_STATUS.
