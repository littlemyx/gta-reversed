# MAC-P2e report: CPickup_File (2026-10-23, portable/main)

## Commit
* 612880b7 P-2e (source: FileRecords/PickupFile.hpp [new], Pickup.h, Scripts/Commands/Ported/Group16_17a.cpp:69 + Group25_26a.cpp:71 (assert lines only); tests: tests/portable/p2e_pickup_test.cpp, tools/portable/run_p2e_test.sh; tools/portable/gen_x86_golden.py: CPickup_File cases). No CMake, no FileRecord.hpp / GenericGameStorage.h change, **Pickups.cpp untouched** (the trait does the work).

## Design
| item | change |
|---|---|
| CPickup_File (0x20) | float rev@0, u32 object@4, ammo@8, regen@0xC, int16 pos[3]@0x10, u16 moneyPerDay@0x16, i16 model@0x18, i16 ref@0x1A, u8 type@0x1C, u8 flags@0x1D, explicit pad1E[2] written 0. Flags byte packed by hand (bDisabled b0, bEmpty b1, help b2, bVisible b3, propertyText b4..6, b7 = 0), not by memcpy of the bit-field. |
| FileRecord<CPickup> (Pickup.h) | ToFile: object = m_pObject ? 1 : 0 (exe wrote the raw pointer; only null/non-null is ever read); pos via FixedFloat::Raw(). FromFile = the exe loader rule: m_pObject = nullptr always; `type != PICKUP_NONE && file.object != 0` => bVisible=false; PICKUP_NONE entries keep bVisible. `CPickups::Load`'s own `if (type != NONE && m_pObject)` is dead in portable (pointer already null), unchanged in MSVC. x86 static_assert: sizeof + 10 member offsets raw CPickup == record. |
| asserts | Pickup.h `VALIDATE_SIZE_FILE(CPickup,0x20)` -> `VALIDATE_SIZE` (in-memory, x86 only; 0x28 on LP64); Group16_17a:69 and Group25_26a:71 likewise (`VALIDATE_OFFSET_FILE(m_nPickupType)` -> `VALIDATE_OFFSET`). On-disk size is `VALIDATE_SIZE_FILE` in PickupFile.hpp. |

## Results
| gate | result |
|---|---|
| layout_diff.py run (fresh work dir) | `CPickup_File` 32/32 **identical**; in-memory CPickup 32/40 stays known-diff (intended). `m_nFlags` bit-fields identical relative to the byte (29:0-0, 1-1, 2-2, 3-3, 4-6 -> 37:...), i.e. same LSB-first order on both targets. Totals: identical 119 (118+1), known-diff 13, NEW 2 (P-2a HAnimNodeInfo pair, pre-existing), not found 2. `gen_x86_golden.py selfcheck`: OK (CPickup_File offsets == i686 CPickup dump) |
| host test `tools/portable/run_p2e_test.sh` | ALL OK. Real Pickup.h / GenericGameStorage.h / FileRecord.hpp. Goldens (gen_x86_golden.py, independent): `basic` (ON_STREET, obj=1, flags 0x5D), `noobj` (ONCE, extremes int16/uint16, flags 0x0A), `none_obj` (PICKUP_NONE + stale obj, bVisible). Checks: per-bit flag order of the compiler, FromFile(basic) => m_pObject null + bVisible cleared + all fields, ToFile == golden (obj written 1, pads 0), noobj keeps bVisible, none_obj keeps bVisible (control: same bytes with type ONCE clears it; with obj=0 does not), exe-garbage in object/pad/flag-bit-7 ignored and normalised on re-save, 620-entry Save/Load loop = 620*0x20 bytes, per-entry loader rule vs random objects, re-save writes object 0 everywhere, truncated stream fails, source scan of Pickups.cpp Save/Load (typed calls, no sizeof). Mutations caught (6/6): bVisible not cleared, raw truncated pointer in ToFile, pos x/y swapped, text-index mask 3 bits->2, clear-for-NONE, pointer kept from file |
| native census (Pickup, Pickups, Group05_08, 11, 13_15, 16_17a, 23, 25_26a, GenericGameStorage, InjectHooksMain, CPickupsDebugModule) vs build/census_p1_after | Group16_17a 3 -> 0, Group25_26a 3 -> 0 (CPickup lines gone; P-2d's remaining one), Group23 1 -> 0, GenericGameStorage 2 -> 0, Pickups 4 -> 4 (pointer-to-int casts at 194/213/249/268, P-4), Group05_08/11/13_15 -1 each, InjectHooksMain 39 -> 3 (other streams). 0 diagnostics mention CPickup/Pickup.h/PickupFile; 0 new |
| msvc-mode census (8 TUs, force) | 0 errors, rc=0 all |
| wine cl.exe A/B (A = 28424de5, B = 612880b7 via git archive; NOTSA_SIG_NORM_STRINGS=1, no PCH; /private/tmp/claude-501/p2e_ab) | 9 TUs **code+relocs+data identical**: Pickup, Pickups, Group16_17a, Group25_26a, GenericGameStorage, CPickupsDebugModule, Group23, Group11, InjectHooksMain (obj byte-size deltas = debug/line info only). No source change in any x86 code path, so no `__LINE__` classification needed |

## Consumer audit
* CPickup file bytes: only `CPickups::Save/Load` (Pickups.cpp:1055-1082), via typed `Save/LoadDataFromWorkBuffer(pickup)` => trait. No other reader/writer (grep source+tests of `aPickUps`, `sizeof(CPickup)`, `0x4D80`): in-memory users only (Radar, Group11/16/25, GameplayRegress, debug module, Windows-only oracles that use x86 `sizeof(CPickup)`, unchanged on x86).
* tPickupReference / collected-pickup arrays / CollectedPickUpIndex / DisplayHelpMessage are separate scalar records already correct (uint32/int32 -> no width issue; guard silent).

## Open items
1. CMake: wire `tools/portable/run_p2e_test.sh` (W0 owns CMake).
2. In portable, a loaded PICKUP_NONE entry that carried a stale object pointer on the exe keeps bVisible (as in the exe); pointer is null (exe: stale garbage). Not observable.
3. Remaining P-2 failures in these TUs are not mine (CRunningScript offsets in Group05_08/13_15 = P-2g).
