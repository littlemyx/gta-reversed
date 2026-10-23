# P-4 plan: 64-bit integer / pointer-width diagnostics left after MAC-P4 (2026-10-24)

Source: native census `.notes/w0/p4_before.tsv` (320 rows, HEAD 0c792ad9) -> `.notes/w0/p4_final.tsv` (45 rows = 36 inline asm (stream F) + 9 design rows listed below); triage and fixes in `.notes/reports/MAC-P4.md`.
Convention: "x86 neutral" = on MSVC x86 the new spelling denotes the same type / the same instruction (wine cl.exe A/B code+relocs identical).

## What MAC-P4 already fixed (mechanical classes, commits P-4a..g)
| class | recipe | sites |
|---|---|---|
| A. `operator new(unsigned/uint32 size, ...)` | first parameter `size_t` (clang rejects any other type as "allocation function", which also made every `new T` of that class fail with "no matching function for call to operator new") | 32 decl/def lines, 28 files -> 105 census rows |
| B. `std::min/max/GetRandomNumberInRange` mixing `uint32`/`size_t` (or `int32`/`ptrdiff_t`) | explicit template argument = the type the x86 deduction had (`<size_t>` is `unsigned int` on x86; `<int32>` for the pool index) | 15 sites |
| C. pointer used as an opaque 32-bit key (corona / rope / streak / crime / marker / shadow ids, srand seed, script highlight id) | `NOTSA_PTR_ID32(p)` (Base.h macro): `uint32(uintptr_t(p))`, identical code on x86 (a function template, even `__forceinline`, is emitted by cl as an unreferenced COMDAT: the A/B caught it); 64-bit: low 32 bits, only compared, never converted back | 58 sites |
| D. small int smuggled through `void*` callback data, `void*` as int, function pointer as `void*` | `(void*)(uintptr_t)x` / `(int32)(uintptr_t)data`; `(void*)Func` | 21 sites |
| E. pointer stored in an `int32` field of an in-memory struct | give the field its pointer type (same 4 bytes on x86) | `CAttractorScanner::field_18/field_40`, `CPlane::field_9E0` |
| F. scalar typing | lambda return types `ptrdiff_t{0}` / `size_t{0}`; `size_t modelSizeSectors` (passed to `size_t&`); `enum E : int` forward declarations (eCarPiece, ePedPieceTypes) | 8 sites |
| G. shims / tooling | `GetACP` (= CP_UTF8), `_BASETSD_H_` (libjpeg `INT32`), `OALBase::trashCan` out of line, Dummy.cpp explicit instantiation inside the unnamed namespace, `snap_census.sh` copies the conan-generated `source/libs` | |

## Design slices that remain (each is independent; order = value / risk)

### P4-S1  `rand()` / `RAND_MAX` (HIGH: silent wrong behaviour on arm64, 70+ sites, 1 census row)
* Facts: the exe uses MSVC CRT `rand()` (LCG `s = s*214013 + 2531011; (s>>16)&0x7FFF`, RAND_MAX 0x7FFF). macOS `rand()` is 31 bit, RAND_MAX 0x7FFFFFFF.
  `General.cpp:199` `static_assert(RAND_MAX == 0x7FFF)` is the only census hit, but it hides: `CGeneral::GetRandomNumber()` returns `uint16` (truncates a 31-bit value),
  `GetRandomNumber() >= RAND_MAX / 2` (General.cpp:221, Explosion.cpp:182, Ped.cpp:1402, FireManager.cpp:397, Vehicle.cpp:302) is never true, `RAND_MAX_FLOAT_RECIPROCAL = 1/RAND_MAX` (General.h:17) is 4.7e-10 instead of 3.05e-5
  (so `GetRandomNumber() * RAND_MAX_FLOAT_RECIPROCAL` is ~0 in ~35 places: Fx.cpp, CarCtrl.cpp, GangWars.cpp, PedGroupPlacer.cpp, Weapon.cpp, PopCycle.cpp, Task*.cpp ...). Every `rand()`/`srand()` call (135 incl. `srand(NOTSA_PTR_ID32(this)+days)` in Entity.cpp, AngledRect.cpp) is affected.
* Recipe: portable shim in `source/portable/crt.h` (already the CRT shim): `notsa::msvc_rand()/msvc_srand()` (LCG above, thread-local not needed: the game is single-threaded), `#undef RAND_MAX / #define RAND_MAX 0x7FFF`, `#define rand ::notsa::msvc_rand`, `#define srand ::notsa::msvc_srand` at the END of StdInc.h (after all std headers, so libc++ internals are untouched). Zero game-source edits. Test: first 10 values for seed 1 and 0x1234 equal a table taken from the exe/wine CRT (`rand()` in wine cl.exe test program).
* Acceptance: General.cpp:199 census row gone; golden table test; `GetRandomNumber() >= RAND_MAX/2` ~50% in a 100k loop.
* NOTE the same constant trap exists wherever `RAND_MAX` is used for float scaling (list above), so the fix must be the macro, not the call sites.

### P4-S2  Root-frame model-info pointer in the frame visibility plugin (3 census rows)
* What the original stores: `tFrameVisibilityPlugin::m_hierarchyId` (4 bytes, `VisibilityPlugins.h:67`, plugin size registered at VisibilityPlugins.cpp:247) holds a component hierarchy id for child frames and, for a clump ROOT frame, the `CClumpModelInfo*` itself
  (`SetClumpModelInfo` VisibilityPlugins.cpp:~1109 writes `(int32)clumpModelInfo`; `FrustumSphereCB` :~364 and `GetClumpModelInfo` :~397 read it back). Hooks 0x732A00 / 0x732A20 are `Set/GetFrameHierarchyId(frame, int32)`.
* 64-bit recipe: keep the int32 id as is; under `NOTSA_PORTABLE` add a second pointer slot to the plugin struct (`CClumpModelInfo* m_modelInfo`, plugin size 8/16) and route the three sites through new `GetFrameModelInfo/SetFrameModelInfo` (Win32: the same int32 field via the old code). Consumers to audit: every `GetFrameHierarchyId` caller on a possibly-root frame (Vehicle.cpp:~2394 `!GetFrameHierarchyId(frame)`, ClumpModelInfo.cpp:283/297/311, RpAnimBlend.cpp:317 index into `outFrameData`: these treat the value as a small component id, so they must NOT see a pointer: portable `GetFrameHierarchyId` of a root frame returns 0 / asserts).
* Alternative (less code): store the model id (`CBaseModelInfo` index) instead of the pointer and look it up in `CModelInfo::ms_modelInfoPtrs`; needs the id on CBaseModelInfo (not present), so the second slot is simpler.

### P4-S3  Vehicle material "restore entries" (1 census row + 6 silent over-reads)
* `tRestoreEntry { void* m_pAddress; void* m_pValue; }` (VehicleModelInfo.h:86, VALIDATE_SIZE 8, global `gRestoreEntries`) records a field address and its old raw 32-bit content so `ResetEditableMaterials`/`SetEditableMaterials` can restore it (VehicleModelInfo.cpp: save 765 texture ptr, 783/817/1678 `*reinterpret_cast<void**>(RpMaterialGetColor(m))` = 4 colour bytes read as a pointer (8-byte over-read on LP64), 792 texture, 796 `*(void**)RpMaterialGetSurfaceProperties(m)` = first float; restore 900 `*(uint32*)addr = (uint32)value`).
* Recipe: `struct tRestoreEntry { void* m_pAddress; union { void* ptr; uint32 raw; } m_value; uint8 m_size; }` under NOTSA_PORTABLE (size is in-memory only; the x86 layout stays 8 with `VALIDATE_SIZE_X86`), restore by `memcpy(addr, &value, size)`; save sites set `ptr` (RwTexture*) with size sizeof(void*) and `raw` (colour / float bits) with size 4. The texture/colour/surface field ADDRESSES come from `RpMaterial` (librw layout: `texture` offset differs) -> do this after R-9 (material access goes through `RpMaterialGet*`/`RpMaterialSet*` accessors, never raw field addresses). Consumers: only VehicleModelInfo.cpp (6 save sites, 2 restore loops) + `gRestoreEntries` extern.

### P4-S4  `CKeyArray` GXT key table (1 census row + wrong entry count)
* `CKeyEntry { const GxtChar* string; uint32 hash; }` (Text/KeyArray.h) is ALSO the TKEY file record: `Load` does `m_size = length / sizeof(CKeyEntry)` and `CFileMgr::Read(file, m_data, length)` (KeyArray.cpp:~25): 16 bytes on LP64, so the count is half and the pointer slot is 8 bytes of file data; `Update(base)` turns a file offset into a pointer (`(uint32)m_data[i].string`, :38).
* Recipe (P-2 FileRecord pattern): `struct CKeyEntry_File { uint32 offset; uint32 hash; }` (`VALIDATE_SIZE_FILE 8`); `Load` reads `length/8` file records into the in-memory `CKeyEntry` array converting `offset` to a temporary `uintptr_t`-free field (`uint32 m_fileOffset` + `const GxtChar* string` both present on 64-bit, pointer filled by `Update(base)`). Consumers: `CText::GetKeys()/GetMissionKeys()` (Text.h:71/72, span over `CKeyEntry`; used by debug UI), `CKeyArray::Search/BinarySearch` (hash + string only), Text.cpp Load paths. Slice with the other Text structs (P-2 text slice, `.notes/P2_PLAN.md`).

### P4-S5  `AnimManager.h:30` `sizeof(std::array<CAnimBlendHierarchy, 2500>) == 60000` (1 row)
* `CAnimBlendHierarchy` has pointers (24 B x86, 40 B LP64). In-memory only. Recipe: `VALIDATE_SIZE_X86`/`VALIDATE_LAYOUT` per P-1 after the Windows ANIM-CRASH work lands (`.notes/P2_PLAN.md` line 79). No code change expected.

### P4-S6  Items owned by somebody else / not width problems (listed for completeness)
| census row | owner / note |
|---|---|
| `Replay.h:143,166,171`, `WaterLevel.h/.cpp` (nested DMI, `PolyInfo::PType`, 35 rows) | fixed by MAC-P2k b2bc35cd (constructors without default member initializers under `__clang__`); census p4_final = 0 |
| `standalone/rw/platform.cpp` (13 rows in p4_before) | gone in p4_final (R-3a) |
| F/C inline asm (36 rows) | stream F |
| `SDLWrapper.cpp:6`, `UIRenderer.cpp:17`, `BugsDebugModule.cpp:4`, `MissionDebugModule.cpp:4` (imgui headers) | NOT code: the census snapshot (`git archive`) lacked the conan-generated, untracked `source/libs/imgui`; `snap_census.sh` now rsyncs it |
| `UIRenderer.cpp:63` (`ImGui_ImplDX9_Init` no match) and `:100` (`SetFocus`): newly VISIBLE once the imgui include path works in the snapshot | W/R: the DX9 backend call must be `#ifdef`-ed for the SDL/GL3 backend (`imgui_impl_sdl3` + `imgui_impl_opengl3`), `SetFocus` -> `SDL_RaiseWindow`; W-4 / R-6 |
| `jmorecfg.h:141` (`INT32`) | fixed by `_BASETSD_H_` in portable/wintypes.h (libjpeg then reuses our `INT32 = int`) |

### Findings for the orchestrator / Windows host (bug-compatible casts kept as they were, now spelled `NOTSA_PTR_ID32`)
* `TaskComplexStareAtPed.cpp:97,112`: `LookAt(..., (int32)&gDefaultTaskTime, ...)` passes the ADDRESS of the global as the duration. `gDefaultTaskTime = 9'999'999 = 0x98967F` (common.h:70) is exactly what the exe pushes as an immediate (Ghidra shows an immediate that lies in .data as a label). Very likely the VALUE was meant; on arm64 the address' low 32 bits are an arbitrary number. One-line fix `gDefaultTaskTime`, but it changes Win32 behaviour -> decision for the host.
* `ObjectCommands.cpp:53` `RemoveEntityFromList(bit_cast<int32>(object), ...)` passes the (already deleted) object POINTER as a script handle, so it never matches a handle; likely should be `CPools::GetObjectRef(object)` taken before the `delete`. Same remark.
* `CustomBuildingDNPipeline.cpp:162` `(NOTSA_PTR_ID32(atomic) / 0x70) % 16` picks a day/night phase from the atomic's ADDRESS: nondeterministic across runs on arm64 (harmless, the exe has the same dependence).

## Not found by the census (width issues that compile but are wrong; candidates for a grep-based audit)
* `General.h:17` (RAND_MAX, see S1).
* `MemoryMgr.cpp:440,460` global `operator new[]/new(uint32)` (dead while `MEMORY_MGR_OVERLOAD_NEW_DELETE` is undefined; with it defined on arm64 they would NOT replace the global allocator). `Core/LinkList.h:24` non-standalone branch hard-codes exe addresses (Win32 only).
* `Pool.h` `GetIndex` returns `ptrdiff_t` (long on LP64): `int32 idx = pool->GetIndex(p)` sites truncate silently only if a pool exceeds 2^31 slots - fine, but `auto idx = GetIndex(p)` then `uint32 handle = idx << 8 | flags` shifts need review in P-5.
