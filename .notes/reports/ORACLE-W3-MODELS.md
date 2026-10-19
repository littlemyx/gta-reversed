# ORACLE-W3-MODELS report

Agent ORACLE-W3-MODELS (rules v3), start 14:38, end ~16:00 (2026-10-19), worktree C:/gta-wt/oracle-w3-models, branch oracle-w3-models. New target `wave3_models_oracle_test` (W2 harness: deterministic arena, whole exe data image + the fixture compared after every
case): rows in `tests/standalone/wave3_oracle_models.inc` (+ `_models2.inc` vehicle, `_models3.inc` txd / material / atomic, `_models4.inc` RW-calling callbacks), TU `tests/standalone/wave3_models_vehicle_tu.cpp` (VehicleModelInfo.cpp included, exports
the file-static free functions). **95 rows, PC24 0 mismatches** (also at 10x the case counts: `W3M_SCALE=10`, 0). PC53 (CRT default precision, informational) differs only in SetEnvMapCoeffCB (7 of 4000, the exe's x87 add of 2^32 is not rounded to 24 bits there).

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 3476 | **3574** |
| game_sa/Models covered | 10 | **105** |
| uncovered A / B in game_sa/Models | 67 / 28 | **0 / 0** |
| port bugs fixed (one commit each, asm address in the subject) | | **8** |

## Fixtures
One 0x310-byte model-info object (garbage with plausible fields; cast to CBase/Atomic/Clump/Vehicle/Ped/Time...) called through qualified member calls (no vptr needed); vehicle structure, upgrade list, fake vehicle (render lights), fake player + vehicle
behind `CWorld::Players` for ChooseVehicleColour, the exe's `ms_modelInfoPtrs` (0xA9B0C8, 20000 entries to 16 infos, with/without nulls), 2dfx store (0xB4C2D8), vehicle statics (colour, light, wheel-upgrade tables, gRestoreEntries), weather globals,
handling table (tandem flag), a fake CTxdPool at 0xC8800C, raw RW-layout materials / textures / atomics (visibility plugin at +0x50, node-name plugin frame), camera LOD multiplier. Callbacks that call RenderWare (FindTextureCB, Use/StopUsing...,
MoveObjectsCB, GetMatFXEffectMaterialCB, SetEnvMapCoeffCB, SetAtomicRendererCB(_Heli)) run on BOTH sides against the same recording stubs: the exe's RW entry points are `oracle::Patch`ed to host stubs, the port's fakerw entry points (unlinked
standalone/rw/*.cpp) are defined in the test with the same stubs; what is compared is the order / values of the RW calls and every fixture byte.

## Hit rates (share of cases returning true / non-null, 1 case = 1 fixture)
GetAtomicFromDistance 32% | GetModelInfoFromHashKey 62% | flag getters 23-29% | IsBreakableStatuePart / IsTagModel 41% | IsUpgradeAvailable 64% | GetNumRemaps 86% | FindOtherUpgrade 91% | GetMaximumNumberOfPassengersFromNumberOfDoors 58% | IsValidCompRule 94% |
GetListOfComponentsNotUsedByRules 79% | HasAlphaMaterialCB 50% | FindTextureCB 68% | GetMatFXEffectMaterialCB 9% (the callback stops on the first effect) | rest of the pointer / void rows 100% (compared on the whole fixture + exe data image).

## Port bugs fixed (each row failed on the pre-fix code; logs C:/gta-scratch/w3models_run4.log, _run13.log)
| function | asm | bug | pre-fix failing cases |
|---|---|---|---|
| SetBaseModelInfoFlags 0x5B3AD0 | `test cl,0xC` -> bDrawLast | port used the draw-last item flag only; additive implies draw last | 45/400 (rows via SetAtomic / SetClumpModelInfoFlags: 145/600, 68/600) |
| SetAtomicModelInfoFlags 0x5B3B20 | special type: ..., tag (bit 20), breakable statue (bit 22), unknown (bit 19) last | port assigned unknown before tag / statue (last flag wins) | 102/600 (draw-last fixed) |
| CAtomicModelInfo::SetupVehicleUpgradeFlags 0x4C4570 | `and eax,0xFF / mov [info+0x12],ax` before matching | upper flag byte (bIsRoad, nSpecialType, old CarMod) not cleared | 183/800 |
| same | chassis table string at 0x85BB58 is "wg_r_" | port table had "wg_r" (matched any "wg_r..." name) | 12/800 (after the first fix) |
| CAtomicModelInfo::GetAtomicFromDistance 0x4C44B0 | `fcomp; test ah,0x41; jne` rejects unordered | port `<=` is false for NaN: returned the atomic | 47/600 |
| CBaseModelInfo::SetHasBeenPreRendered 0x4C42F0 | `test eax,eax` | port assigned the int to a 1-bit field (2 -> false) | 46/200 |
| CVehicleModelInfo::GetNumDoors 0x4C73C0 | `movsx eax, byte [ecx+0x4C]` | field is a signed byte in the exe (port zero-extended) | 15/40 |
| CVehicleModelInfo::SetEnvMapCoeffCB 0x4C88B0 | `fild; (neg: fadd 2^32); fmul 0.001f` | port: signed int / 1000.0f (last bit differs; PC24 rounding of the add reproduced) | 215/800 |

## Observed, not changed (exe quirks without a game-visible effect, masked or restricted in the rows)
- Flag getters HasComplexHierarchy 0x4C4E00 / GetIsDrawLast / HasBeenPreRendered / IsBackfaceCulled / IsLod / IsRoad return `flags & mask` in the whole eax (0x200, 0x80 ...), the port a bool; the exe has no caller of any of them (inlined everywhere): rows compare non-zero.
- SetRenderPipelinesCB 0x4C88F4 is a bare `ret` (eax = whatever the caller left); the port returns the atomic: rows compare non-zero.
- CountCompsInRule 0x4C7F80 loops forever on a negative rule in the exe (sar), the port's `/= 16` ends; GetListOfComponentsNotUsedByRules writes past its 6-entry list for a nibble > 5 in both; AddRemap with 4 remaps writes a 5th (port asserts in the debug `std::array`): fixtures keep to game-reachable values.
- SetEditableMaterialsCB 0x4C8220: the "lights on" branch writes the surface properties at material+8 (librw layout, standalone port) vs +0xC (exe): not comparable, ms_lightsOn is 0 in the row (remap texture and light-colour branches are compared). CClumpModelInfo::SetAtomicRendererCB / SetAtomicRendererCB_Heli compare the stored callback through the mapping port function -> exe address.

## Not covered
Class C (56) / S (13) / D (28) of game_sa/Models are outside the brief (virtual calls, file IO, RW object creation).

## Process
Tool calls ~130. Trailer: Co-Authored-By Claude Sonnet 5.5. Gates: see the hand-back (the header edit CBaseModelInfo::SetHasBeenPreRendered invalidates every test target's PCH, so the full run_all_tests rebuild is long).
