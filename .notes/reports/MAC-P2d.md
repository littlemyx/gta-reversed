# MAC-P2d report: CWeapon_File, CPedSaveStructure_File, Ped.cpp assert/prefix (2026-10-23, portable/main)

## Commits
* e00a17a3 P-2d (source: FileRecords/PedFile.hpp [new], Weapon.h, Entity/Ped/PedSaveStructure.h, Entity/Ped/Ped.cpp (2 lines), Scripts/Commands/Ported/Group16_17a.cpp:56 (1 line); tests: tests/portable/p2d_ped_test.cpp, tools/portable/run_p2d_test.sh; tools/portable/gen_x86_golden.py: CPedSaveStructure_File record + golden case). No CMake, no FileRecord.hpp / GenericGameStorage.h change.
* ad597bca Ped.cpp prefix by value `uint32(notsa::FileSizeOf<CPedSaveStructure>)` (see wine A/B).

## Design
| item | change |
|---|---|
| CWeapon_File (0x1C) | type/state/ammoInClip/totalAmmo/timeForNextShotMs u32, 2 bools @0x14/0x15, explicit pad16[2], `fxSystem` u32 @0x18 (always 0). `FileRecord<CWeapon>` in Weapon.h; FromFile sets m_FxSystem = nullptr. x86 static_assert: raw object == record (size + 8 offsets). In-memory `VALIDATE_SIZE_FILE(CWeapon)` -> `VALIDATE_SIZE` |
| CPedSaveStructure_File (0x18C) | pos[3], health, armor, CWeapon_File[13] @0x14, createdBy u8 @0x180, activeWeaponSlot/areaCode i8, exitIndex i32 @0x184, fightingStyle/allowedAttackMoves u8, explicit pads (0x183, 0x18A..B) written 0. `FileRecord<CPedSaveStructure>` in PedSaveStructure.h (same x86 raw==record proof). In-memory assert -> `VALIDATE_SIZE` |
| Ped.cpp | `CPed::Load`: `assert(size == notsa::FileSizeOf<CPedSaveStructure>)`; `CPed::Save`: prefix `SaveDataToWorkBuffer<uint32>(uint32(notsa::FileSizeOf<CPedSaveStructure>))`. Typed Save/Load of the record unchanged call sites (trait dispatch). Applied in both builds (== sizeof on x86) |
| Group16_17a.cpp:56 | `VALIDATE_SIZE_FILE(CWeapon,0x1C)` -> `VALIDATE_SIZE` (it fired on arm64 for the in-memory 0x20) |
| PedSaveStructure.h include | `#include "Ped.h"` skipped only under `NOTSA_P2D_HOSTTEST_NO_PED_H` (host test; forward decl of ePedCreatedBy/CPed instead) |

## Results
| gate | result |
|---|---|
| layout_diff (`layout_diff.py run`, fresh work dir) | `CWeapon_File` 28/28 and `CPedSaveStructure_File` 396/396 **identical**; in-memory CWeapon 28/32, CPedSaveStructure 396/456 stay known-diff (intended, now x86-only asserts). Totals: identical 118 (116 + 2), known-diff 13, NEW 2 (P-2a HAnimNodeInfo pair), not found 2. `gen_x86_golden.py selfcheck`: OK |
| host test `tools/portable/run_p2d_test.sh` | ALL OK. Real Weapon.h/PedSaveStructure.h/GenericGameStorage.h/FileRecord.hpp. Golden CWeapon 0x1C + CPedSaveStructure 0x18C (13 slots, slot k: type 10+k / ammo 100k / clip k / state k%5 / bool bits) from gen_x86_golden.py: ToFile==golden, FromFile(golden) fields, typed Save writes exactly 0x18C (not the LP64 object), prefix stream [0x18C u32][record], garbage in exe-written pointer/pad slots ignored and normalised to 0 on re-save, truncated load fails, 2000 random objects: 13-slot round trip keeps Type/State/Ammo/AmmoInClip/time/bools/everything, all fx slots 0 on disk, re-save byte-equal; source scan of Ped.cpp (no `sizeof(save)`, FileSizeOf in assert+prefix). Mutations caught: swap ammoInClip/totalAmmo, fx pointer written, ToFile 12 slots, FromFile 12 slots, exitIndex dropped, raw `sizeof(save)` assert. (pad16 size change is not detectable: alignment hides it) |
| native census (Ped, PedSaveStructure, Weapon, Group16_17a, PlayerPed + 23 name-matching TUs) vs build/census_p1_after | Ped.cpp 5 -> 3 (CPedSaveStructure size assert gone; rest operator new / Pool.h min), PedSaveStructure.cpp 1 -> 0, Group16_17a 3 -> 1 (CWeapon + CPedSaveStructure gone; remaining CPickup = P-2e), Weapon.cpp 0. PlayerPed.cpp 1 -> 3: the 2 new are `RwIm3DVertex (aka int)` at PlayerPed.cpp:1560/1561 (RW stream work, not P-2d lines). 0 new from P-2d |
| msvc-mode census (6 TUs, force) | 0 errors (rc=0 all) |
| wine cl.exe A/B (A = e00a17a3^, B = ad597bca; NOTSA_SIG_NORM_STRINGS=1, no PCH; /private/tmp/claude-501/p2d_ab) | 10 TUs **code+relocs+data identical**: Ped, PedSaveStructure, Weapon, Group16_17a, PlayerPed, PedIntelligence, TaskSimplePlayerOnFoot, GenericGameStorage, Automobile, InjectHooksMain. First round (e00a17a3) Ped.cpp DIFFERED: `SaveDataToWorkBuffer<uint32>(notsa::FileSizeOf<X>)` passes a const& to the inline variable -> extern COMDAT reloc + 4-byte smaller frame in CPed::Save. Fixed with a by-value `uint32(...)`; re-run Ped identical (other 9 were identical in round 1 and unaffected by the 1-line change) |

## Consumer audit
* CWeapon bytes: only CPedSaveStructure (`m_weapons[13]`, Ped.cpp Save/Load via PedSaveStructure.cpp Construct/Extract). In-memory users (`CPed::m_aWeapons`, tasks, scripts) never touch file bytes. No CWeapon in replay blocks / pools / other save structs (grep source+tests).
* CPedSaveStructure: Ped.cpp Load/Save only (+ tests/standalone/wave3_oracle_gen_ped.inc: in-memory Construct oracle, `sizeof` in-memory, unaffected). Extract does not read the fx slot (hence 0).
* Other size prefixes from P-2b (GangWars.cpp:75, PlayerInfo.cpp:1465, PlayerPed.cpp:164, Object.cpp:610) still write `sizeof(<struct>)`: those structs have no FileRecord and contain no pointers (their `VALIDATE_SIZE_FILE` pass on arm64 in the census), so sizeof == file size; not changed (switching to FileSizeOf is a no-op there).

## Open items
1. CMake: wire `tools/portable/run_p2d_test.sh` (W0 owns CMake).
2. `CPedSaveStructure::Construct/Extract` need CPed and are not covered by the host test (covered by the wave-3 oracle on Windows; byte semantics unchanged).
3. Rule learned (also for P-2e..g): never pass `notsa::FileSizeOf<X>` by reference into x86 code (template Save<T>(const T&)); cast to a value.
