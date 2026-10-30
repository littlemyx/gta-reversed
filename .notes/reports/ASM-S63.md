# ASM-S63 / ASM-S65 report (phase 2 sweep)

Base origin/reverse/interior_c 0012a95ed. Result tables: `.notes/asm_sweep/S63.result.tsv` (40 TSV rows + 1 extra), `.notes/asm_sweep/S65.result.tsv` (7 rows).

## Verdicts

| slice | rows | MATCH | FIXED | NOT_REACHED / open |
|---|---|---|---|---|
| S63 | 40 (+1 extra) | 33 | 7 (+1 extra row) | 0 |
| S65 | 7 | 4 | 1 (partial, UNSURE rest of LoadWeaponData) | 1 (Render) |

## Fixes (one commit per function)

| commit | function | finding |
|---|---|---|
| c27340a8b | CTaskSimpleRunNamedAnim::ProcessPed (extra) | blend-out distance sums squares z, y, x |
| 7695ad6f6 | CTaskSimpleUseGun ctor | burst length argument went to m_BurstShots (+0x36) instead of m_BurstLength (+0x34) |
| 1ff2015be | CTheCarGenerators::Load | u16 signed-compared counter, count <= 0 skipped |
| f78bfc033 | CTheZones::Update | CountSeconds = now after each update (port ran every frame after 5 s) |
| 68cf68bc2 | CUpsideDownCarCheck::IsCarUpsideDown | CanPedStepOutCar(false) must be TRUE (port negated) |
| 4f62f52db | CUpsideDownCarCheck::UpdateTimers | free/dead slot -> handle -1 + time 0 (port stored 0 = slot lost forever); handle 0 no longer skipped |
| 4fae45ca0 | CUpsideDownCarCheck::AddCarToCheck | only the first free slot (port filled all) |
| c74729d43 | CUpsideDownCarCheck::AreAnyCarsUpsideDown (unhooked) | strict `ja` |
| 9669674cf | CVehicleAnimGroup::GetGroup | removed NOTSA_UNREACHABLE in front of the exe default (2^animId) path; jump table verified |
| 2916c4144 | CWeaponInfo::LoadWeaponData | anim loop end association: (floor*0.02 - 0.006) + start |

## Hook address check

All RH install addresses of the touched files point at the claimed functions. The 9 `?` rows are scalar deleting destructors; each calls an inner dtor that sets the class vtable (0x859CFC, 0x85B8DC, 0x86DA74, 0x85A334, 0x86E3CC, 0x86E1DC, 0x86DD50; ScratchHead/LoopedMiddle tail-jump to CTaskSimpleAnim dtor 0x61A6F0). SlideToCoord Constructor(0x66C450) and MakeAbortable (0x66C4D0) rows are commented out in the source (not hooked), but the functions are equal.

## Open

- CWeaponEffects::Render (0x742CF0): not compared (size).
- CWeaponInfo::LoadWeaponData: only the anim-loop conversion compared; scanf field mapping of the `$`, `%` and melee branches not traced.
- CTheZones::Save/Load use `int16` loop counters; the exe uses unsigned 16 bit compares (differs only for corrupt negative counts).
- CTheZones::SetCurrentZoneAsUnlocked / GetZoneWasVisited: the exe truncates the sector index through `movzx al` after _ftol, the port (non FIX_BUGS) uses size_t (differs only for off-map positions); not changed here (other slice owns 0x572800/0x572130).

## Tests

See the final report of the agent (wave2_oracle_test, combat regress, PortableWin64 compile of touched files).
