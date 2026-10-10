# Static-initialiser (`_initterm`) audit of gta_sa_compact.exe

Date 2026-10-09. Scope: everything the ORIGINAL exe's CRT startup writes into .data/.bss before `WinMain` (0x748710) runs, which is
therefore NOT in the initial .data bytes that `original_data.bin` carries (`.notes/PHASE2_PLAN.md` D1/D3). Method: PE parsed by hand,
CRT startup disassembled (capstone), every initialiser decompiled (Ghidra, `.notes/decomp/<VA>_FUN_<va>.c`, 194 files = all 178 non-float
initialisers + CRT pieces) and, to get ground truth instead of eyeballing 1667 functions, the whole table was EXECUTED in Unicorn over the
exe image (script in the appendix; result cross-checked against an independent static x87 evaluator for all 1491 float stores: 0 mismatches).

## 1. Summary

* `WinMainCRTStartup` = 0x824570 (entry RVA 0x424570). Order (function names inferred from their shape/the MSVC 2005-era CRT): `_heap_init` 0x82C471, `_mtinit` 0x827D25, `_RTC_Initialize` 0x82BEDA, `_ioinit` 0x82C0B9,
  `GetCommandLineA`, `_crtGetEnvironmentStringsA` 0x82DBC8, `_setargv` 0x82DB26, `_setenvp` 0x82D8F3, **`_cinit` 0x823B76**, `GetStartupInfoA`, `WinMain` (0x748710).
* `_cinit`: calls `_FPinit` pointer (0x8E2B74 -> **0x821397 `_fpmath`**), then `_initterm_e(__xi_a=0x8A5A14, __xi_z=0x8A5A30)` (6 C initialisers),
  `atexit(_RTC_Terminate 0x82BF1E)`, then `_initterm(__xc_a=0x8A4000, __xc_z=0x8A5A10)`.
* The tables live in .data (not .rdata): `__xc_a` 0x8A4000 .. `__xc_z` 0x8A5A10 = 1668 slots, slot 0 is NULL, **1667 initialisers**, ascending order.
* **1489** of them are single-float constant computations (`static const float K = a*b` / `a/b` / `cos(..)` of other .rdata/.data constants), **178** are the rest.
* **No initialiser touches Windows APIs** (except the CRT security cookie 0x8339CA, which we skip) and **none allocates heap**; only `rand()` (19 calls, see 4) and `atexit` (140 registrations) are reached.
* Net effect of the whole table: **128,021 bytes** change in .data/.bss (127,968 in .bss, 53 in .data) across ~18k small ranges.

Class counts (class a/b/c/d as in the task; `n` = executes but changes no byte):

| group | count | a (replicated in source) | b (needs replica) | c (CRT/RW) | d (atexit) | n (no net effect) |
|---|---|---|---|---|---|---|
| float constants (1489 inits / 1491 stores) | 1489 | 9 stores (constexpr in source, bit-exact) | 7 live (5 `StaticRef` reads + 2 header-default mismatches) + 1475 dormant (address not referenced in source) | 0 | 0 | 1 (value 0) |
| ctor / vector-ctor / fills / copies | 178 | 0 | 91 (80 live: object address is a `StaticRef` in source) | 1 (security cookie) | 20 (atexit-only) | 66 |
| `_initterm_e` (xi) | 6 | 0 | 0 | 6 | 0 | 0 |
| CRT start (before `_cinit`) | see 2 | | | all | | |

Every class-b row is covered by ONE generic mechanism (section 3); per-item C++ is only needed if you prefer constexpr over a data image.

## 2. CRT-internal state (class c, no action: our CRT/D3 replaces it) and the FPU control word

| global | by | meaning |
|---|---|---|
| 0xC9C2F4 / 0xC9C2F8 | `_heap_init` 0x82C471 | `_crtheap` = `HeapCreate(0,0x1000,0)`; `__active_heap` = 3 for XP-class OS (SBH). Our CRT: irrelevant |
| 0x8E3110 | `_mtinit` 0x827D25 | `__tlsindex`; per-thread data `_tiddata` created there, its `_holdrand` (+0x14) starts at **1** => `rand()` is the MSVC LCG `x*214013+2531011`, `(x>>16)&0x7FFF` seeded 1 until `srand` |
| 0xC9AC08 / 0xC9AC0C / 0xC9AC10 / 0xC9AC14 / 0xC9AC18 | startup 0x824590.. | `_osplatform/_osver/_winver/_winmajor/_winminor` from `GetVersion` |
| 0xC9C414 / 0xC9AC50 | startup | `_acmdln` (GetCommandLineA), `_aenvptr` |
| 0x8E30F0..0x8E3104, 0xC9AC00 | `_fpmath` 0x82134F / 0x826F04 | CRT float-conversion fn ptrs; result of `IsProcessorFeaturePresent`-style probe |
| 0xC9D42C | xi[2] 0x821CBE | `_onexit` table (0x80 bytes malloc = 32 slots) |
| 0xC9C418 / 0xC9D420 | xi[3] 0x823EE5 | stdio `__piob` table (default 0x200 FILE*; `_nstream`) |
| 0xC9C400 / 0xC9C404 | xi[4] 0x8289F5 | CPU feature (CPUID/SSE2) flags |
| 0xC9D430 | xi[5] 0x833494 | `__initmbctable` (`_setmbcp(-3)`) |
| 0xC9AC74 | xi[6] 0x82AC54 | stores return of an OS call taking handler 0x82AC0E (unhandled-exception filter style) |
| 0x8E37B4..0x8E37BB = 0xFF | xi[1] 0x826A72 | 8 bytes of lowio state |
| 0x8E31BC | xc[0] 0x8339CA | `__security_init_cookie` (time/pid/tid/tick/QPC xor) - non-deterministic by design |

**x87 control word (important for float fidelity).** `_FPinit` -> `_fpmath` 0x821397 ends with `0x826EB2` = `_controlfp(_PC_53 = 0x10000, _MCW_PC = 0x30000)` (0x825136), then `fnclex`:
the CRT sets **precision control = 53-bit (double)**, round-to-nearest, all exceptions masked => hardware CW 0x027F (same as the Windows process default, so it is a no-op there).
The same call pair also appears as a save/set/restore guard in a class ctor/dtor at 0x76B8xx. BUT the game later creates the D3D9 device at 0x7F6781 with behaviour flags
`{0|4 (MULTITHREADED)} | {0x20 SW / 0x40 HW / 0x50 HW+PURE}` and **without `D3DCREATE_FPU_PRESERVE` (0x2)**; D3D9 then drops the creating thread's x87 precision to **24-bit** (single) for the
rest of the game. Consequences: (1) all static initialisers ran at PC=53; I re-ran the whole table at PC=24: **0 byte differences**, so the delta is precision independent;
(2) run-time game math after device creation is x87 at 24-bit mantissa, i.e. every intermediate rounds to float - this matches a SSE2 `float` build (`/arch:SSE2 /fp:precise`) except for denormal/
exponent range, so SSE2 is the right target; build only the code that executes before device creation (static ctors, `CGame::InitialiseOnceBeforeRW`) with care. For bit-exact comparison against the exe
use `/arch:IA32` (x87) and `_controlfp(_PC_24, _MCW_PC)` after device creation. Not verified: whether anything re-widens precision later (no other `_controlfp` call sites exist except 0x76B91D/0x76B932/0x76B9C8 and the CRT one).

## 3. How to replicate (recommended): apply the post-initterm delta once

Static `StaticRef` never runs constructors (confirmed: no `construct_at`/placement-new on any exe global in `source/`; reversed ctors such as `CCamera::Constructor()` exist only as
hooks that the exe's own `_initterm` called in hybrid mode). In standalone nothing calls them, so objects like `TheCamera` (0xB6F028), the model-info `CStore`s (vtable pointers in all
14000+ pre-constructed elements, 70 KB for the atomic store alone), `CPedGroup ms_groups`, `CColModel ms_colModel*` bboxes (=1.0f scale seeds), `CPools`, `-1` index arrays (`aNodesToBeCleared`, `RoadBlockNodes`, `ms_aAnimations`...) would stay all-zero.

Options per object: (i) data delta (exact, includes original vtable pointers that D2 then fixes up, includes `rand()` results, needs no repo class to be correct) or (ii) `std::construct_at(&obj)`
with the repo's own ctor (writes the NEW build's vtable, depends on every ctor being reversed correctly; the 'repo has reversed ctor' rows). **Use (i) for everything; use (ii) only to cross-check.**
Implementation in S2: let the build-time extractor emit the image AFTER the initterm run - i.e. `original_data.bin` = (.rdata + .data initial bytes) with the delta below applied (the delta is deterministic: no
Windows API, no heap pointer, 19 `rand()` calls with seed 1; apply it BEFORE the D2 vtable fixup pass so the new vtable/pointer words get relocated too).
The script in the appendix produces `{ranges:[[va,hex],..], atexit:[..], rand_calls:19}` in 0.5 s from the user's exe (needs `pip install unicorn`); no exe bytes are committed.
Standalone start-up then does `memcpy` of those ranges after copying `original_data.bin`; `atexit` registrations (140 dtor wrappers) can be ignored (process exit).

Equivalent C++ for the dormant-or-live floats if a literal table is preferred (1491 `{addr,bits}` pairs, section 6):
```cpp
struct InitTermF32 { uint32 addr; uint32 bits; };
extern const InitTermF32 kInitTermFloats[1491];                // from the table in section 6
for (auto& e : kInitTermFloats) *reinterpret_cast<uint32*>(e.addr) = e.bits;   // addresses are the original VAs (D1)
```
The five floats the repo ALREADY reads through `StaticRef` (and would currently read as 0.0f in standalone) - exact fix if the table is not used:
```cpp
StaticRef<uint32>(0xC1BFF0) = 0x3BB60B6A; // Automobile.cpp s_AutomobileSpeedDivisor = 0.277778f/50      (0x853520)
StaticRef<uint32>(0xC1C81C) = 0x3BB60B6A; // Bike.cpp s_BikeExhaustSpeedDivisor                          (0x853620)
StaticRef<uint32>(0xC1C818) = 0x3B83126E; // Bike.cpp s_BikeTractionScale = 10/2500                      (0x853600)
StaticRef<uint32>(0xC279CC) = 0x3C03126F; // Boat.cpp fTimeMult = 1.2f/CBoat::WAKE_LIFETIME              (0x8540B0) = 0.008
StaticRef<uint32>(0xC18D48) = 0x3F6C835E; // TaskSimpleGoTo ms_fLookAtThresholdDotProduct = cos(22.5 deg) (0x852840, x87 fcos)
```
(bit values taken from the emulated image; recompute with the script if a different exe revision is used.)

Fidelity findings in already-replicated code: 9 stores are replicated as `constexpr` and are bit-exact (`AEPedAudioEntity.cpp` WIND_*, `AEVehicleAudioEntity.cpp` FRQ_TYRE/SPROCKET_RANGE, `cHandlingDataMgr.cpp`
ACCEL/VELOCITY_CONST, `Shadows.h` 225, `MenuManager_Input.cpp` 1/3200). Two header defaults do NOT match the exe: `AEVehicleAudioEntity.h` Rev `VolumeBase = -4.5f // 0xB6BA2C` (exe stores **+4.5** there:
`[0xB6B9D0] - [0x8CBC14]`, i.e. a range, so the label/sign is wrong) and `FrqEngineInAirFactor{0.7f} // 0xB6BA74` (exe 0x3F333332 vs literal 0x3F333333, 1 ulp).

## 4. Other observations

* `rand()` is consumed by two initialisers only: `CHandShaker` x6 array ctor 0x84D6D0 (18 calls) and the `CMenuManager` ctor 0x84EE40 (1 call). The game reseeds with `srand(RsTimer())` (`app_game.cpp:52`) so the seed-1 state is not needed afterwards, but the VALUES written into `gHandShaker` / `FrontEndMenuManager` come from `rand()` seed 1 (the delta has them; our own CRT would produce different values if constructors were re-run).
* `CMenuManager` ctor (0x84EE40, obj 0xBA6748) also writes outside its object (52 B over 25 ranges between 0x8CB6FC and 0xC1CC04: `.data` neighbours + other statics); this is the reason to prefer the delta over ctor replay.
* 4 `ftol_store` initialisers (0x854820/40/60/80) compute `(int)(float(g)*k*k)` from `RsGlobal`-like zero BSS (0xC17044/0xC17048) => 0; nothing to do.
* 20 `atexit`-only initialisers register a destructor wrapper for a global whose ctor is trivial (no write); 116 more rows also register (marked `+atexit(..)` in section 5); 136 initialisers = 140 emulated `atexit` calls in total. Exit-time only.
* Initialisers are laid out with 6-byte-`nop` padding after tail-`jmp` ctors (0x84A740, 0x84B2A0, 0x84D920, 0x852010, 0x853E10, 0x855670/80/90); table entries point into the middle of those chains - that is expected.
* Reversed-code coverage: the compact exe's own ctor targets that have a name in `.notes/symbols.txt` are listed in the table; unnamed ones are small class ctors (`0x40FB60` is the ctor used for every `CColModel` global; `0x7170C0` takes 4 byte args = an RGBA colour ctor; `0x7281E0/0x727230` = trivial vec-ctor/dtor of 4-byte elements, no byte changes).

## 5. All non-float initialisers (178). Columns: table index, slot VA (in __xc_a..z), function VA, code bytes, what, nearest `StaticRef` symbol of the first changed byte, net changed bytes/ranges, class, action

| # | slot | fn | size | what | symbol | net delta | cl | action |
|---|---|---|---|---|---|---|---|---|
| 0 | 8A4004 | 8339CA | 86 | __security_init_cookie -> 0x8E31BC | - | - | c | CRT-internal: our CRT runs its own `__security_init_cookie`; skip |
| 44 | 8A40B4 | 849470 | 22 | this=0x968A00 ctor 0x40FB60 | `ms_colModelBBox+0x2` | 13B/7r 0x968A02..0x968A2A | b | delta **(live: StaticRef in source)**; +atexit(856030) |
| 45 | 8A40B8 | 849490 | 36 | 20 x 0x30 @0x968A30 ctor 0x40FB60 | `ms_colModelCutObj+0x2` | 260B/140r 0x968A32..0x968DEA | b | delta **(live: StaticRef in source)**; +atexit(856040) |
| 46 | 8A40BC | 8494C0 | 22 | this=0x968DF0 ctor 0x40FB60 | `ms_colModelPed1+0x2` | 13B/7r 0x968DF2..0x968E1A | b | delta **(live: StaticRef in source)**; +atexit(856060) |
| 47 | 8A40C0 | 8494E0 | 22 | this=0x968E20 ctor 0x40FB60 | `ms_colModelPed2+0x2` | 13B/7r 0x968E22..0x968E4A | b | delta **(live: StaticRef in source)**; +atexit(856070) |
| 48 | 8A40C4 | 849500 | 22 | this=0x968E50 ctor 0x40FB60 | `ms_colModelDoor1+0x2` | 13B/7r 0x968E52..0x968E7A | b | delta **(live: StaticRef in source)**; +atexit(856080) |
| 49 | 8A40C8 | 849520 | 22 | this=0x968E80 ctor 0x40FB60 | `ms_colModelBumper1+0x2` | 13B/7r 0x968E82..0x968EAA | b | delta **(live: StaticRef in source)**; +atexit(856090) |
| 50 | 8A40CC | 849540 | 22 | this=0x968EB0 ctor 0x40FB60 | `ms_colModelPanel1+0x2` | 13B/7r 0x968EB2..0x968EDA | b | delta **(live: StaticRef in source)**; +atexit(8560A0) |
| 51 | 8A40D0 | 849560 | 22 | this=0x968EE0 ctor 0x40FB60 | `ms_colModelBonnet1+0x2` | 13B/7r 0x968EE2..0x968F0A | b | delta **(live: StaticRef in source)**; +atexit(8560B0) |
| 52 | 8A40D4 | 849580 | 22 | this=0x968F10 ctor 0x40FB60 | `ms_colModelBoot1+0x2` | 13B/7r 0x968F12..0x968F3A | b | delta **(live: StaticRef in source)**; +atexit(8560C0) |
| 53 | 8A40D8 | 8495A0 | 22 | this=0x968F40 ctor 0x40FB60 | `ms_colModelWheel1+0x2` | 13B/7r 0x968F42..0x968F6A | b | delta **(live: StaticRef in source)**; +atexit(8560D0) |
| 54 | 8A40DC | 8495C0 | 22 | this=0x968F70 ctor 0x40FB60 | `ms_colModelBodyPart1+0x2` | 13B/7r 0x968F72..0x968F9A | b | delta **(live: StaticRef in source)**; +atexit(8560E0) |
| 55 | 8A40E0 | 8495E0 | 22 | this=0x968FA0 ctor 0x40FB60 | `ms_colModelBodyPart2+0x2` | 13B/7r 0x968FA2..0x968FCA | b | delta **(live: StaticRef in source)**; +atexit(8560F0) |
| 56 | 8A40E4 | 849600 | 22 | this=0x968FD0 ctor 0x40FB60 | `ms_colModelWeapon+0x2` | 13B/7r 0x968FD2..0x968FFA | b | delta **(live: StaticRef in source)**; +atexit(856100) |
| 111 | 8A41C0 | 849CE0 | 36 | 13 x 0x1C @0x96A9B8 ctor 0x441E00 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856110) |
| 124 | 8A41F4 | 849E90 | 39 | 50 x 0xD8 @0x96C048 ctor 0x4470E0 | `aGarages+0x50` | 150B/50r 0x96C098..0x96E9F3 | b | delta **(live: StaticRef in source)**; +atexit(856130) |
| 137 | 8A4228 | 84A040 | 28 | fill loop | `aNodesToBeCleared` | 10000B/5000r 0x972CD0..0x977AEE | b | delta **(live: StaticRef in source)** |
| 138 | 8A422C | 84A060 | 28 | fill loop | `aNodesToBeCleared+0x4e20` | 64B/32r 0x977AF0..0x977B6E | b | delta (not referenced by address in source) |
| 139 | 8A4230 | 84A080 | 28 | fill loop | `aExteriorNodeLinkedTo` | 16B/8r 0x977B7C..0x977B9A | b | delta **(live: StaticRef in source)** |
| 152 | 8A4264 | 84A220 | 22 | this=0x977BE8 ctor 0x4541E0 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856150) |
| 177 | 8A42C8 | 84A540 | 22 | this=0xA430B0 ctor 0x571920 CPlayerInfo::CPlayerInfo | `PlayerInfo+0x1e` | 13B/7r 0xA430CE..0xA43158 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CPlayerInfo::CPlayerInfo`; +atexit(856160) |
| 185 | 8A42E8 | 84A640 | 28 | fill loop | `RoadBlockNodes` | 650B/325r 0xA435A0..0xA43AB2 | b | delta **(live: StaticRef in source)** |
| 192 | 8A4304 | 84A720 | 28 | fill loop | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 193 | 8A4308 | 84A740 | 10 | this=0xA90CF0 ctor 0x46B260 | `ScriptsForBrains` | 490B/140r 0xA90CF0..0xA91260 | b | delta **(live: StaticRef in source)** |
| 194 | 8A430C | 84A750 | 36 | 96 x 0x44 @0xA913E8 ctor 0x463750 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856190) |
| 195 | 8A4310 | 84A780 | 39 | 128 x 0x3C @0xA92D68 ctor 0x463770 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(8561B0) |
| 196 | 8A4314 | 84A7B0 | 39 | 128 x 0x4 @0xA94B68 ctor 0x727230 CSprite2d::CSprite2d | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856170) |
| 276 | 8A4454 | 84B1C0 | 28 | fill loop | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 283 | 8A4470 | 84B2A0 | 10 | this=0xA9A810 ctor 0x5A8020 CPedClothesDesc::CPedClothesDesc | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 309 | 8A44D8 | 84B5D0 | 22 | this=0xA9AE00 ctor 0x49E620 Fx_c::Fx_c | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(8561D0) |
| 313 | 8A44E8 | 84B650 | 22 | this=0xA9AE80 ctor 0x4A9470 FxManager_c::FxManager_c | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(8561E0) |
| 344 | 8A4564 | 84BA30 | 36 | 8 x 0xC @0xA9AFB8 ctor 0x4AC0D0 | `ms_informFriendsEvents+0x8` | 32B/8r 0xA9AFC0..0xA9B018 | b | delta **(live: StaticRef in source)**; +atexit(8561F0) |
| 345 | 8A4568 | 84BA60 | 36 | 8 x 0x10 @0xA9B018 ctor 0x4AC350 CInformGroupEvent::CInformGroupEvent | `ms_informGroupEvents+0xc` | 32B/8r 0xA9B024..0xA9B098 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CInformGroupEvent::CInformGroupEvent`; +atexit(856210) |
| 357 | 8A4598 | 84BBF0 | 22 | this=0xAAE950 ctor 0x4C5CB0 | `ms_atomicModelInfoStore+0x4` | 70000B/28000r 0xAAE954..0xB1BF40 | b | delta **(live: StaticRef in source)**; +atexit(856230) |
| 358 | 8A459C | 84BC10 | 22 | this=0xB1BF58 ctor 0x4C5D10 | `ms_damageAtomicModelInfoStore+0x4` | 350B/140r 0xB1BF5C..0xB1C91C | b | delta **(live: StaticRef in source)**; +atexit(856240) |
| 359 | 8A45A0 | 84BC30 | 22 | this=0xB1C934 ctor 0x4C5D70 | `ms_lodAtomicModelInfoStore+0x4` | 5B/2r 0xB1C938..0xB1C944 | b | delta **(live: StaticRef in source)**; +atexit(856250) |
| 360 | 8A45A4 | 84BC50 | 22 | this=0xB1C960 ctor 0x4C5DD0 | `ms_timeModelInfoStore+0x4` | 1183B/339r 0xB1C964..0xB1E128 | b | delta **(live: StaticRef in source)**; +atexit(856260) |
| 361 | 8A45A8 | 84BC70 | 22 | this=0xB1E128 ctor 0x4C5E30 | `ms_lodTimeModelInfoStore+0x4` | 7B/3r 0xB1E12C..0xB1E154 | b | delta **(live: StaticRef in source)**; +atexit(856270) |
| 362 | 8A45AC | 84BC90 | 22 | this=0xB1E158 ctor 0x4C5E90 | `ms_weaponModelInfoStore+0x4` | 255B/102r 0xB1E15C..0xB1E938 | b | delta **(live: StaticRef in source)**; +atexit(856280) |
| 363 | 8A45B0 | 84BCB0 | 22 | this=0xB1E958 ctor 0x4C5EF0 | `ms_clumpModelInfoStore+0x4` | 460B/184r 0xB1E95C..0xB1F634 | b | delta **(live: StaticRef in source)**; +atexit(856290) |
| 364 | 8A45B4 | 84BCD0 | 22 | this=0xB1F650 ctor 0x4C5F50 | `ms_vehicleModelInfoStore+0x4` | 11236B/637r 0xB1F654..0xB478F4 | b | delta **(live: StaticRef in source)**; +atexit(8562A0) |
| 365 | 8A45B8 | 84BCF0 | 22 | this=0xB478F8 ctor 0x4C67D0 | `ms_pedModelInfoStore+0x4` | 1390B/556r 0xB478FC..0xB4C29C | b | delta **(live: StaticRef in source)**; +atexit(8562B0) |
| 388 | 8A4614 | 84BFD0 | 39 | 2500 x 0x18 @0xB4EA40 ctor 0x4CF270 CAnimBlendHierarchy::CAnimBlendHierarchy | `ms_aAnimations+0xc` | 10000B/2500r 0xB4EA4C..0xB5D498 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CAnimBlendHierarchy::CAnimBlendHierarchy`; +atexit(8562C0) |
| 389 | 8A4618 | 84C000 | 29 | fill loop | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 402 | 8A464C | 84C1A0 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x8562E0 |
| 409 | 8A4668 | 84C270 | 22 | this=0xB5F8B8 ctor 0x4D83E0 CAEAudioHardware::CAEAudioHardware | `AEAudioHardware+0x5` | 1341B/71r 0xB5F8BD..0xB60198 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CAEAudioHardware::CAEAudioHardware`; +atexit(8562F0) |
| 410 | 8A466C | 84C290 | 22 | this=0xB608D0 ctor 0x4EE9A0 | `AESmoothFadeThread` | 4B/1r 0xB608D0..0xB608D4 | b | delta **(live: StaticRef in source)**; +atexit(856300) |
| 429 | 8A46B8 | 84C4F0 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x856310 |
| 430 | 8A46BC | 84C500 | 22 | this=0xB612D8 ctor 0x4F1750 CAEStreamTransformer::Initialise | `AEStreamTransformer` | 16B/1r 0xB612D8..0xB612E8 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CAEStreamTransformer::Initialise`; +atexit(856320) |
| 467 | 8A4750 | 84C9A0 | 36 | 6 x 0x1C @0xB61C38 ctor 0x4E3330 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856330) |
| 487 | 8A47A0 | 84CC30 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x856350 |
| 495 | 8A47C0 | 84CD20 | 22 | this=0xB62CB0 ctor 0x4EFD30 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856360) |
| 497 | 8A47C8 | 84CD60 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x856370 |
| 511 | 8A4800 | 84CF10 | 11 | copy global->global | `s_DummyEngineSlots+0x9f` | 1B/1r 0xB6BA3F..0xB6BA40 | b | delta (not referenced by address in source) |
| 517 | 8A4818 | 84CFC0 | 30 | other | `s_DummyEngineSlots+0xb4` | 4B/1r 0xB6BA54..0xB6BA58 | b | delta (not referenced by address in source) |
| 518 | 8A481C | 84CFE0 | 11 | copy global->global | `s_DummyEngineSlots+0xba` | 2B/1r 0xB6BA5A..0xB6BA5C | b | delta (not referenced by address in source) |
| 532 | 8A4854 | 84D190 | 23 | copy global->global | `s_PrevServicePlayTime+0x653` | 3B/2r 0x8CC0BB..0x8CC0C4 | b | delta (not referenced by address in source) |
| 558 | 8A48BC | 84D4D0 | 22 | this=0xB6BB18 ctor 0x4F63B0 | `m_sRainSoundL` | 3B/1r 0xB6BB18..0xB6BB1B | b | delta **(live: StaticRef in source)**; +atexit(856380) |
| 559 | 8A48C0 | 84D4F0 | 22 | this=0xB6BBC0 ctor 0x4F63B0 | `m_sRainSoundR` | 3B/1r 0xB6BBC0..0xB6BBC3 | b | delta **(live: StaticRef in source)**; +atexit(856390) |
| 566 | 8A48DC | 84D5D0 | 22 | this=0xB6BC90 ctor 0x507670 CAudioEngine::CAudioEngine | `AudioEngine+0xb4` | 39B/12r 0xB6BD44..0xB6DBE3 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CAudioEngine::CAudioEngine`; +atexit(8563A0) |
| 574 | 8A48FC | 84D6D0 | 39 | 6 x 0x94 @0xB6ECA0 ctor 0x517740 | `gHandShaker+0xc` | 353B/25r 0xB6ECAC..0xB6F018 | b | delta **(live: StaticRef in source)**; +atexit(8563B0) |
| 575 | 8A4900 | 84D700 | 22 | this=0xB6F028 ctor 0x51A450 CCamera::CCamera | `TheCamera` | 147B/74r 0xB6F028..0xB6FD68 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CCamera::CCamera`; +atexit(856410) |
| 576 | 8A4904 | 84D720 | 22 | this=0xB6FDA0 ctor 0x50E6D0 CIdleCam::Init | `gIdleCam+0x12` | 54B/25r 0xB6FDB2..0xB6FE30 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CIdleCam::Init`; +atexit(8563D0) |
| 577 | 8A4908 | 84D740 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x8563E0 |
| 578 | 8A490C | 84D750 | 24 | other | `gpCamColVars` | 3B/1r 0xB6FE88..0xB6FE8B | b | delta **(live: StaticRef in source)** |
| 579 | 8A4910 | 84D770 | 39 | 1 x 0x9C @0xB6FEC0 ctor 0x5173F0 | `gDWHeliChaseCamSettings+0x1a` | 41B/20r 0xB6FEDA..0xB6FF5A | b | delta **(live: StaticRef in source)**; +atexit(8563F0) |
| 592 | 8A4944 | 84D920 | 10 | this=0xB70198 ctor 0x531EE0 CControllerConfigManager::CControllerConfigManager | `ControlsManager+0x224` | 1254B/248r 0xB703BC..0xB71452 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CControllerConfigManager::CControllerConfigManager` |
| 635 | 8A49F0 | 84DE70 | 22 | this=0xB71F80 ctor 0x539DA0 CFireManager::CFireManager | `gFireManager` | 363B/181r 0xB71F80..0xB728E3 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CFireManager::CFireManager`; +atexit(856420) |
| 648 | 8A4A24 | 84E010 | 22 | this=0xB72978 ctor 0x53CFA0 CLoadMonitor::CLoadMonitor | `g_LoadMonitor+0x4` | 10B/6r 0xB7297C..0xB729F4 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CLoadMonitor::CLoadMonitor`; +atexit(856430) |
| 655 | 8A4A40 | 84E0F0 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x856440 |
| 656 | 8A4A44 | 84E100 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x856450 |
| 664 | 8A4A64 | 84E1F0 | 39 | 2 x 0x134 @0xB73458 ctor 0x541D80 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856460) |
| 671 | 8A4A80 | 84E2E0 | 27 | this=0xB74240 ctor 0x59AED0 CMatrix::SetScale | `gDummyMatrix+0x2` | 6B/3r 0xB74242..0xB7426C | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CMatrix::SetScale`; +atexit(856480) |
| 672 | 8A4A84 | 84E300 | 22 | this=0xB74288 ctor 0x54F440 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856490) |
| 703 | 8A4B00 | 84E6E0 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x8564A0 |
| 710 | 8A4B1C | 84E7B0 | 33 | this=0xB7CB10 ctor 0x7170C0 | `m_BelowHorizonGrey` | 4B/1r 0xB7CB10..0xB7CB14 | b | delta **(live: StaticRef in source)**; +atexit(8564B0) |
| 711 | 8A4B20 | 84E7E0 | 13 | copy global->global | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 712 | 8A4B24 | 84E7F0 | 13 | copy global->global | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 725 | 8A4B58 | 84E980 | 39 | 2 x 0x190 @0xB7CD98 ctor 0x571920 CPlayerInfo::CPlayerInfo | `Players+0x1e` | 26B/14r 0xB7CDB6..0xB7CFD0 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CPlayerInfo::CPlayerInfo`; +atexit(856500) |
| 726 | 8A4B5C | 84E9B0 | 39 | 14400 x 0x8 @0xB7D0B8 ctor 0x564040 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856520) |
| 727 | 8A4B60 | 84E9E0 | 39 | 256 x 0xC @0xB992B8 ctor 0x563190 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856540) |
| 728 | 8A4B64 | 84EA10 | 39 | 900 x 0x4 @0xB99EB8 ctor 0x43E3F0 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(8564C0) |
| 729 | 8A4B68 | 84EA40 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x8564E0 |
| 730 | 8A4B6C | 84EA50 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x8564F0 |
| 762 | 8A4BEC | 84EE40 | 22 | this=0xBA6748 ctor 0x574350 CMenuManager::CMenuManager | `AERadioTrackManager+0x4` | 52B/25r 0x8CB6FC..0xC1CC04 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CMenuManager::CMenuManager`; +atexit(856560) |
| 781 | 8A4C38 | 84F0A0 | 36 | 6 x 0x4 @0xBA86D4 ctor 0x463360 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856570) |
| 782 | 8A4C3C | 84F0D0 | 36 | 64 x 0x4 @0xBAA250 ctor 0x727230 CSprite2d::CSprite2d | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856590) |
| 789 | 8A4C58 | 84F1C0 | 36 | 6 x 0x4 @0xBAB1FC ctor 0x727230 CSprite2d::CSprite2d | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(8565B0) |
| 790 | 8A4C5C | 84F1F0 | 22 | this=0xBAB22C ctor 0x58FDA0 CHudColours::CHudColours | `HudColour` | 56B/3r 0xBAB22C..0xBAB268 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CHudColours::CHudColours`; +atexit(8565D0) |
| 797 | 8A4C78 | 84F2D0 | 36 | 7 x 0x4 @0xBAB35C ctor 0x727230 CSprite2d::CSprite2d | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(8565E0) |
| 798 | 8A4C7C | 84F300 | 22 | this=0xBAB380 ctor 0x591260 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856640) |
| 799 | 8A4C80 | 84F320 | 39 | 128 x 0x1C @0xBAD3F8 ctor 0x590E20 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856600) |
| 800 | 8A4C84 | 84F350 | 39 | 256 x 0x14 @0xBAE1F8 ctor 0x590EC0 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856620) |
| 831 | 8A4D00 | 84F740 | 22 | this=0xBAF670 ctor 0x5984C0 | `g_interiorMan+0x3ec` | 44B/20r 0xBAFA5C..0xBB3DC0 | b | delta **(live: StaticRef in source)**; +atexit(856650) |
| 857 | 8A4D68 | 84FA80 | 22 | this=0xBB4240 ctor 0x59E620 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856660) |
| 883 | 8A4DD0 | 84FDC0 | 22 | this=0xBB7CB0 ctor 0x5A3E20 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856670) |
| 886 | 8A4DDC | 84FE20 | 22 | this=0xBC1290 ctor 0x5A7570 | `gOctTreeBase` | 19B/2r 0xBC1290..0xBC12AA | b | delta **(live: StaticRef in source)**; +atexit(856690) |
| 887 | 8A4DE0 | 84FE40 | 22 | this=0xBC12C0 ctor 0x532290 CDirectory::CDirectory | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856680) |
| 888 | 8A4DE4 | 84FE60 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x8566A0 |
| 895 | 8A4E00 | 84FF30 | 39 | rep stosd zero | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 903 | 8A4E20 | 850040 | 22 | this=0xBC4020 ctor 0x4CDE70 CAnimBlendAssocGroup::CAnimBlendAssocGroup | `ms_cutsceneAssociations+0x10` | 4B/1r 0xBC4030..0xBC4034 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CAnimBlendAssocGroup::CAnimBlendAssocGroup`; +atexit(8566F0) |
| 971 | 8A4F30 | 8508C0 | 34 | other | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 973 | 8A4F38 | 850910 | 42 | this=0xC03A44 ctor 0x7170C0 | `m_AmbientColor` | 4B/1r 0xC03A44..0xC03A48 | b | delta **(live: StaticRef in source)**; +atexit(856710) |
| 974 | 8A4F3C | 850940 | 39 | 256 x 0x54 @0xC03A48 ctor 0x5DB2B0 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856720) |
| 975 | 8A4F40 | 850970 | 36 | 40 x 0x14 @0xC08E48 ctor 0x5DB290 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856740) |
| 1002 | 8A4FAC | 850CE0 | 36 | 10 x 0x10 @0xC091F0 ctor 0x5DE520 | `Gang` | 10B/10r 0xC091F0..0xC09281 | b | delta **(live: StaticRef in source)**; +atexit(856760) |
| 1021 | 8A4FF8 | 850F50 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x856780 |
| 1022 | 8A4FFC | 850F60 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x8567C0 |
| 1047 | 8A5060 | 851270 | 22 | this=0xC09908 ctor 0x5F6450 | `ms_FollowAnyMeansAllocator` | 3B/1r 0xC09908..0xC0990B | b | delta **(live: StaticRef in source)**; +atexit(856800) |
| 1048 | 8A5064 | 851290 | 22 | this=0xC0990C ctor 0x5F6480 | `ms_FollowLimitedAllocator` | 3B/1r 0xC0990C..0xC0990F | b | delta **(live: StaticRef in source)**; +atexit(856810) |
| 1049 | 8A5068 | 8512B0 | 22 | this=0xC09910 ctor 0x5F64B0 | `ms_StandStillAllocator` | 3B/1r 0xC09910..0xC09913 | b | delta **(live: StaticRef in source)**; +atexit(856820) |
| 1050 | 8A506C | 8512D0 | 22 | this=0xC09914 ctor 0x5F64E0 | `ms_ChatAllocator` | 3B/1r 0xC09914..0xC09917 | b | delta **(live: StaticRef in source)**; +atexit(856830) |
| 1051 | 8A5070 | 8512F0 | 22 | this=0xC09918 ctor 0x5F6510 | `ms_RandomAllocator` | 3B/1r 0xC09918..0xC0991B | b | delta **(live: StaticRef in source)**; +atexit(856840) |
| 1052 | 8A5074 | 851310 | 22 | this=0xC0991C ctor 0x5F6540 | `ms_SitInLeaderCarAllocator` | 3B/1r 0xC0991C..0xC0991F | b | delta **(live: StaticRef in source)**; +atexit(856850) |
| 1053 | 8A5078 | 851330 | 39 | 8 x 0x2D4 @0xC09920 ctor 0x5FC150 CPedGroup::CPedGroup | `ms_groups+0x4` | 1184B/296r 0xC09924..0xC0AFBC | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CPedGroup::CPedGroup`; +atexit(856860) |
| 1066 | 8A50AC | 8514E0 | 22 | this=0xC0B058 ctor 0x6023A0 CInterestingEvents::CInterestingEvents | `g_InterestingEvents+0x60` | 95B/6r 0xC0B0B8..0xC0B1AD | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CInterestingEvents::CInterestingEvents`; +atexit(856880) |
| 1079 | 8A50E0 | 851680 | 39 | 128 x 0x14 @0xC0B1E8 ctor 0x608330 CPedScriptedTaskRecordData::CPedScriptedTaskRecordData | `ms_scriptedTasks` | 640B/256r 0xC0B1E8..0xC0BBE5 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CPedScriptedTaskRecordData::CPedScriptedTaskRecordData`; +atexit(8568B0) |
| 1110 | 8A515C | 851A70 | 22 | this=0xC10820 ctor 0x617330 BoneNodeManager_c::BoneNodeManager_c | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(8568D0) |
| 1117 | 8A5178 | 851B50 | 22 | this=0xC15448 ctor 0x617FC0 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(8568E0) |
| 1154 | 8A520C | 851FF0 | 22 | this=0xC17824 ctor 0x40FB60 | `col1+0x2` | 13B/7r 0xC17826..0xC1784E | b | delta **(live: StaticRef in source)**; +atexit(8568F0) |
| 1155 | 8A5210 | 852010 | 10 | this=0xC17854 ctor 0x40F030 | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1162 | 8A522C | 8520E0 | 36 | 64 x 0x40 @0xC178F0 ctor 0x632BD0 Constructor | `ms_taskSequence+0x1` | 128B/64r 0xC178F1..0xC188B3 | b | delta **(live: StaticRef in source)**; +atexit(856920) |
| 1163 | 8A5230 | 852110 | 36 | 48 x 0x10 @0xC188F0 ctor 0x62EC40 CScriptedBrainTaskEntry::CScriptedBrainTaskEntry | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856900) |
| 1206 | 8A52DC | 852680 | 11 | copy global->global | `ms_vecPedChairAnimOffset+0x6e` | 2B/1r 0xC18CE6..0xC18CE8 | b | delta (not referenced by address in source) |
| 1207 | 8A52E0 | 852690 | 11 | copy global->global | `ms_vecPedChairAnimOffset+0x72` | 2B/1r 0xC18CEA..0xC18CEC | b | delta (not referenced by address in source) |
| 1208 | 8A52E4 | 8526A0 | 11 | copy global->global | `ms_vecPedChairAnimOffset+0x76` | 2B/1r 0xC18CEE..0xC18CF0 | b | delta (not referenced by address in source) |
| 1209 | 8A52E8 | 8526B0 | 11 | copy global->global | `ms_vecPedChairAnimOffset+0x7a` | 2B/1r 0xC18CF2..0xC18CF4 | b | delta (not referenced by address in source) |
| 1223 | 8A5320 | 852850 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x856940 |
| 1224 | 8A5324 | 852860 | 11 | copy global->global | `ms_fLookAtThresholdDotProduct+0x6f` | 1B/1r 0xC18DB7..0xC18DB8 | b | delta (not referenced by address in source) |
| 1225 | 8A5328 | 852870 | 22 | this=0xC18DB8 ctor 0x66D440 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856950) |
| 1238 | 8A535C | 852A10 | 22 | this=0xC19518 ctor 0x40FB60 | `ms_ClimbColModel+0x2` | 13B/7r 0xC1951A..0xC19542 | b | delta **(live: StaticRef in source)**; +atexit(856960) |
| 1239 | 8A5360 | 852A30 | 22 | this=0xC19548 ctor 0x40FB60 | `ms_StandUpColModel+0x2` | 13B/7r 0xC1954A..0xC19572 | b | delta **(live: StaticRef in source)**; +atexit(856970) |
| 1240 | 8A5364 | 852A50 | 22 | this=0xC19578 ctor 0x40FB60 | `ms_VaultColModel+0x2` | 13B/7r 0xC1957A..0xC195A2 | b | delta **(live: StaticRef in source)**; +atexit(856980) |
| 1241 | 8A5368 | 852A70 | 22 | this=0xC195A8 ctor 0x40FB60 | `ms_FindEdgeColModel+0x2` | 13B/7r 0xC195AA..0xC195D2 | b | delta **(live: StaticRef in source)**; +atexit(856990) |
| 1278 | 8A53FC | 852F10 | 22 | this=0xC196E8 ctor 0x694850 | `ms_offsets+0x2` | 50B/25r 0xC196EA..0xC1976C | b | delta **(live: StaticRef in source)**; +atexit(8569B0) |
| 1297 | 8A5448 | 853170 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x8569C0 |
| 1304 | 8A5464 | 853240 | 30 | rep stosd zero | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1305 | 8A5468 | 853260 | 30 | rep stosd zero | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1306 | 8A546C | 853280 | 30 | rep stosd zero | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1321 | 8A54A8 | 853460 | 22 | this=0xC1B340 ctor 0x6A00F0 CText::CText | `TheText+0x20` | 1B/1r 0xC1B360..0xC1B361 | b | delta **(live: StaticRef in source)**; repo has reversed ctor `CText::CText`; +atexit(856A00) |
| 1353 | 8A5528 | 853880 | 22 | this=0xC1C890 ctor 0x6C2740 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856A20) |
| 1395 | 8A55D0 | 853DC0 | 36 | 4 x 0x30 @0xC1CC78 ctor 0x40FB60 | `m_aSpecialColModel+0x2` | 52B/28r 0xC1CC7A..0xC1CD32 | b | delta **(live: StaticRef in source)**; +atexit(856A30) |
| 1396 | 8A55D4 | 853DF0 | 22 | this=0xC1CD38 ctor 0x40FB60 | `s_TestBladeCol+0x2` | 13B/7r 0xC1CD3A..0xC1CD62 | b | delta **(live: StaticRef in source)**; +atexit(856A50) |
| 1397 | 8A55D8 | 853E10 | 10 | this=0xC1CD68 ctor 0x40F030 | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1398 | 8A55DC | 853E20 | 39 | 30 x 0x94 @0xC1CDC0 ctor 0x5BD420 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856A60) |
| 1405 | 8A55F8 | 853F10 | 22 | this=0xC1DF30 ctor 0x6E3EB0 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856A80) |
| 1443 | 8A5690 | 8543D0 | 49 | this=0x3189 ctor 0x6D0450 | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1457 | 8A56C8 | 8545B0 | 36 | 64 x 0x24 @0xC3A200 ctor 0x6FA6D0 | `ms_userLists` | 2048B/64r 0xC3A200..0xC3AAFC | b | delta **(live: StaticRef in source)**; +atexit(856A90) |
| 1458 | 8A56CC | 8545E0 | 39 | 64 x 0x94 @0xC3BB00 ctor 0x6FA790 | `ms_effectPairs+0x4` | 7168B/512r 0xC3BB04..0xC3DFFC | b | delta **(live: StaticRef in source)**; +atexit(856AB0) |
| 1475 | 8A5710 | 854810 | 11 | copy global->global | `m_fNightVisionSwitchOnFXCount+0x2` | 2B/1r 0xC40302..0xC40304 | b | delta (not referenced by address in source) |
| 1476 | 8A5714 | 854820 | 29 | (int)(f*f) of zero BSS | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1477 | 8A5718 | 854840 | 29 | (int)(f*f) of zero BSS | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1478 | 8A571C | 854860 | 29 | (int)(f*f) of zero BSS | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1479 | 8A5720 | 854880 | 29 | (int)(f*f) of zero BSS | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1480 | 8A5724 | 8548A0 | 11 | copy global->global | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1481 | 8A5728 | 8548B0 | 11 | copy global->global | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1488 | 8A5744 | 854980 | 22 | this=0xC40350 ctor 0x706770 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856AD0) |
| 1503 | 8A5780 | 854B60 | 36 | 64 x 0x1C @0xC6A198 ctor 0x70F8D0 CStencilShadowObject::CStencilShadowObject | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856AE0) |
| 1511 | 8A57A0 | 854C70 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x856B00 |
| 1512 | 8A57A4 | 854C80 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x856B10 |
| 1519 | 8A57C0 | 854D50 | 39 | 32 x 0x150 @0xC6E9A8 ctor 0x717110 CEscalator::CEscalator | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856B20) |
| 1521 | 8A57C8 | 854DA0 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x856B80 |
| 1522 | 8A57CC | 854DB0 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x856B90 |
| 1523 | 8A57D0 | 854DC0 | 36 | 2 x 0x4 @0xC71AD0 ctor 0x727230 CSprite2d::CSprite2d | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856B40) |
| 1524 | 8A57D4 | 854DF0 | 36 | 15 x 0x4 @0xC71AD8 ctor 0x727230 CSprite2d::CSprite2d | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856B60) |
| 1531 | 8A57F0 | 854EE0 | 36 | 45 x 0x70 @0xC71BF8 ctor 0x71A8B0 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856BA0) |
| 1564 | 8A5874 | 855310 | 39 | 32 x 0xA0 @0xC7DD58 ctor 0x720F60 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856BD0) |
| 1572 | 8A5894 | 855420 | 39 | 3 x 0x3CC @0xC80740 ctor 0x728B10 | `aCannons+0x32c` | 9B/3r 0xC80A6C..0xC81207 | b | delta **(live: StaticRef in source)**; +atexit(856BF0) |
| 1579 | 8A58B0 | 855510 | 22 | this=0xC81360 ctor 0x72A620 | `m_WeatherAudioEntity` | 3B/1r 0xC81360..0xC81363 | b | delta **(live: StaticRef in source)**; +atexit(856C10) |
| 1590 | 8A58DC | 855670 | 10 | this=0xC87B40 ctor 0x72E860 | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1591 | 8A58E0 | 855680 | 10 | this=0xC87B88 ctor 0x72E860 | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1592 | 8A58E4 | 855690 | 10 | this=0xC87BD0 ctor 0x72E860 | - | - | n | none (net write is zero over zero BSS / identical bytes) |
| 1619 | 8A5950 | 8559E0 | 22 | this=0xC888D0 ctor 0x5074B0 | `m_ExplosionAudioEntity` | 3B/1r 0xC888D0..0xC888D3 | b | delta **(live: StaticRef in source)**; +atexit(856C20) |
| 1638 | 8A599C | 855C40 | 22 | this=0xC8A7DC ctor 0x40FB60 | `ms_PelletTestCol+0x2` | 13B/7r 0xC8A7DE..0xC8A806 | b | delta **(live: StaticRef in source)**; +atexit(856C30) |
| 1645 | 8A59B8 | 855D20 | 36 | 2 x 0x2C @0xC8A838 ctor 0x742A90 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856C40) |
| 1652 | 8A59D4 | 855E10 | 36 | 80 x 0x70 @0xC8AAB8 ctor 0x743C30 | - | - | n | none (net write is zero over zero BSS / identical bytes); +atexit(856C60) |
| 1665 | 8A5A08 | 855FC0 | 12 | atexit only | - | - | d | none (dtor-at-exit only, no data written) dtor-wrapper 0x856C80 |
| 1666 | 8A5A0C | 855FD0 | 29 | fill loop | - | - | n | none (net write is zero over zero BSS / identical bytes) |

(`(live: StaticRef in source)` = the object's start address appears in `source/` as a `StaticRef`/address. `n`-class rows that "write" zeros to zero BSS are listed for completeness.)

## 6. Float-constant initialisers (1489 functions, 1491 stores)

All are `fld/fmul/fdiv/fdivr/fsub/fild/fcos .. fstp dword [dest]` over constants in .rdata / .data (plus a few that read an earlier initialiser's output; order is table order). Destinations span 0x8E3EE0..0xC92130.
Classification of the 1491 stores: a = 9, b live = 7 (5 `StaticRef` reads + 2 header-default mismatches), b dormant = 1475 (address never referenced in `source/`: the consumer was reversed with an inline constant, or is still unreversed - re-check this list whenever a function that reads a `StaticRef<float>` at an address from this table is reversed).
Format per line: `destVA f32bits initFnVA`. Apply with `*(uint32*)dest = bits`.

```
8E3EE0 39A3D70A 848F10
8E3EE4 3CA3D70A 848F30
8E3EE8 41DE38E4 848F50
8E3EEC 39D1B717 848F70
8E3EF0 3B83126E 848F90
8E3EF4 3BB60B6A 848FB0
8E3FB8 39A3D70A 848FD0
8E3FBC 3CA3D70A 848FF0
8E3FC0 41DE38E4 849010
8E3FC4 39D1B717 849030
8E3FC8 3B83126E 849050
8E3FCC 3BB60B6A 849070
9654D8 39A3D70A 849090
9654DC 3CA3D70A 8490B0
9654E0 41DE38E4 8490D0
9654E4 39D1B717 8490F0
9654E8 3B83126E 849110
9654EC 3BB60B6A 849130
96553C 39A3D70A 849150
965540 3CA3D70A 849170
965544 41DE38E4 849190
965548 39D1B717 8491B0
96554C 3B83126E 8491D0
965550 3BB60B6A 8491F0
965568 39A3D70A 849210
96556C 3CA3D70A 849230
965570 41DE38E4 849250
965574 39D1B717 849270
965578 3B83126E 849290
96557C 3BB60B6A 8492B0
9655E8 39A3D70A 8492D0
9655EC 3CA3D70A 8492F0
9655F0 41DE38E4 849310
9655F4 39D1B717 849330
9655F8 3B83126E 849350
9655FC 3BB60B6A 849370
9689E4 39A3D70A 849390
9689E8 3CA3D70A 8493B0
9689EC 41DE38E4 8493D0
9689F0 39D1B717 8493F0
9689F4 3B83126E 849410
9689F8 3BB60B6A 849430
9689FC 39A3D70A 849450
969000 39A3D70A 849620
969004 3CA3D70A 849640
969008 41DE38E4 849660
96900C 39D1B717 849680
969010 3B83126E 8496A0
969014 3BB60B6A 8496C0
969040 39A3D70A 8496E0
969044 3CA3D70A 849700
969048 41DE38E4 849720
96904C 39D1B717 849740
969050 3B83126E 849760
969054 3BB60B6A 849780
969058 39A3D70A 8497A0
96905C 3CA3D70A 8497C0
969060 41DE38E4 8497E0
969064 39D1B717 849800
969068 3B83126E 849820
96906C 3BB60B6A 849840
9690D0 39A3D70A 849860
9690D4 3CA3D70A 849880
9690D8 41DE38E4 8498A0
9690DC 39D1B717 8498C0
9690E0 3B83126E 8498E0
9690E4 3BB60B6A 849900
969194 39A3D70A 849920
969198 3CA3D70A 849940
96919C 41DE38E4 849960
9691A0 39D1B717 849980
9691A4 3B83126E 8499A0
9691A8 3BB60B6A 8499C0
969A24 39A3D70A 8499E0
969A28 3CA3D70A 849A00
969A2C 41DE38E4 849A20
969A30 39D1B717 849A40
969A34 3B83126E 849A60
969A38 3BB60B6A 849A80
96A708 39A3D70A 849AA0
96A70C 3CA3D70A 849AC0
96A710 41DE38E4 849AE0
96A714 39D1B717 849B00
96A718 3B83126E 849B20
96A71C 3BB60B6A 849B40
96A7E4 39A3D70A 849B60
96A7E8 3CA3D70A 849B80
96A7EC 41DE38E4 849BA0
96A7F0 39D1B717 849BC0
96A7F4 3B83126E 849BE0
96A7F8 3BB60B6A 849C00
96A8BC 39A3D70A 849C20
96A8C0 3CA3D70A 849C40
96A8C4 41DE38E4 849C60
96A8C8 39D1B717 849C80
96A8CC 3B83126E 849CA0
96A8D0 3BB60B6A 849CC0
96ABA4 39A3D70A 849D10
96ABA8 3CA3D70A 849D30
96ABAC 41DE38E4 849D50
96ABB0 39D1B717 849D70
96ABB4 3B83126E 849D90
96ABB8 3BB60B6A 849DB0
96C02C 39A3D70A 849DD0
96C030 3CA3D70A 849DF0
96C034 41DE38E4 849E10
96C038 39D1B717 849E30
96C03C 3B83126E 849E50
96C040 3BB60B6A 849E70
96EA7C 39A3D70A 849EC0
96EA80 3CA3D70A 849EE0
96EA84 41DE38E4 849F00
96EA88 39D1B717 849F20
96EA8C 3B83126E 849F40
96EA90 3BB60B6A 849F60
96F034 39A3D70A 849F80
96F038 3CA3D70A 849FA0
96F03C 41DE38E4 849FC0
96F040 39D1B717 849FE0
96F044 3B83126E 84A000
96F048 3BB60B6A 84A020
977BA0 39A3D70A 84A0A0
977BA4 3CA3D70A 84A0C0
977BA8 41DE38E4 84A0E0
977BAC 39D1B717 84A100
977BB0 3B83126E 84A120
977BB4 3BB60B6A 84A140
977BCC 39A3D70A 84A160
977BD0 3CA3D70A 84A180
977BD4 41DE38E4 84A1A0
977BD8 39D1B717 84A1C0
977BDC 3B83126E 84A1E0
977BE0 3BB60B6A 84A200
97D648 39A3D70A 84A240
97D64C 3CA3D70A 84A260
97D650 41DE38E4 84A280
97D654 39D1B717 84A2A0
97D658 3B83126E 84A2C0
97D65C 3BB60B6A 84A2E0
97F634 39A3D70A 84A300
97F638 3CA3D70A 84A320
97F63C 41DE38E4 84A340
97F640 39D1B717 84A360
97F644 3B83126E 84A380
97F648 3BB60B6A 84A3A0
97F64C 39A3D70A 84A3C0
97F650 3CA3D70A 84A3E0
97F654 41DE38E4 84A400
97F658 39D1B717 84A420
97F65C 3B83126E 84A440
97F660 3BB60B6A 84A460
A43098 39A3D70A 84A480
A4309C 3CA3D70A 84A4A0
A430A0 41DE38E4 84A4C0
A430A4 39D1B717 84A4E0
A430A8 3B83126E 84A500
A430AC 3BB60B6A 84A520
A43310 39A3D70A 84A560
A43588 39A3D70A 84A580
A4358C 3CA3D70A 84A5A0
A43590 41DE38E4 84A5C0
A43594 39D1B717 84A5E0
A43598 3B83126E 84A600
A4359C 3BB60B6A 84A620
A90834 39A3D70A 84A660
A90838 3CA3D70A 84A680
A9083C 41DE38E4 84A6A0
A90840 39D1B717 84A6C0
A90844 3B83126E 84A6E0
A90848 3BB60B6A 84A700
A95148 39A3D70A 84A7E0
A9514C 3CA3D70A 84A800
A95150 41DE38E4 84A820
A95154 39D1B717 84A840
A95158 3B83126E 84A860
A9515C 3BB60B6A 84A880
A95160 39A3D70A 84A8A0
A95164 3CA3D70A 84A8C0
A95168 41DE38E4 84A8E0
A9516C 39D1B717 84A900
A95170 3B83126E 84A920
A95174 3BB60B6A 84A940
A95178 39A3D70A 84A960
A9517C 3CA3D70A 84A980
A95180 41DE38E4 84A9A0
A95184 39D1B717 84A9C0
A95188 3B83126E 84A9E0
A9518C 3BB60B6A 84AA00
A95194 39A3D70A 84AA20
A95198 3CA3D70A 84AA40
A9519C 41DE38E4 84AA60
A951A0 39D1B717 84AA80
A951A4 3B83126E 84AAA0
A951A8 3BB60B6A 84AAC0
A951AC 39A3D70A 84AAE0
A951B0 3CA3D70A 84AB00
A951B4 41DE38E4 84AB20
A951B8 39D1B717 84AB40
A951BC 3B83126E 84AB60
A951C0 3BB60B6A 84AB80
A951C4 39A3D70A 84ABA0
A951C8 3CA3D70A 84ABC0
A951CC 41DE38E4 84ABE0
A951D0 39D1B717 84AC00
A951D4 3B83126E 84AC20
A951D8 3BB60B6A 84AC40
A951DC 39A3D70A 84AC60
A951E0 3CA3D70A 84AC80
A951E4 41DE38E4 84ACA0
A951E8 39D1B717 84ACC0
A951EC 3B83126E 84ACE0
A951F0 3BB60B6A 84AD00
A9577C 39A3D70A 84AD20
A95780 3CA3D70A 84AD40
A95784 41DE38E4 84AD60
A95788 39D1B717 84AD80
A9578C 3B83126E 84ADA0
A95790 3BB60B6A 84ADC0
A95794 39A3D70A 84ADE0
A95798 3CA3D70A 84AE00
A9579C 41DE38E4 84AE20
A957A0 39D1B717 84AE40
A957A4 3B83126E 84AE60
A957A8 3BB60B6A 84AE80
A957AC 39A3D70A 84AEA0
A957B0 3CA3D70A 84AEC0
A957B4 41DE38E4 84AEE0
A957B8 39D1B717 84AF00
A957BC 3B83126E 84AF20
A957C0 3BB60B6A 84AF40
A957C4 39A3D70A 84AF60
A957C8 3CA3D70A 84AF80
A957CC 41DE38E4 84AFA0
A957D0 39D1B717 84AFC0
A957D4 3B83126E 84AFE0
A957D8 3BB60B6A 84B000
A957DC 39A3D70A 84B020
A957E0 3CA3D70A 84B040
A957E4 41DE38E4 84B060
A957E8 39D1B717 84B080
A957EC 3B83126E 84B0A0
A957F0 3BB60B6A 84B0C0
A957F4 39A3D70A 84B0E0
A95800 39A3D70A 84B100
A95804 3CA3D70A 84B120
A95808 41DE38E4 84B140
A9580C 39D1B717 84B160
A95810 3B83126E 84B180
A95814 3BB60B6A 84B1A0
A9A7F4 39A3D70A 84B1E0
A9A7F8 3CA3D70A 84B200
A9A7FC 41DE38E4 84B220
A9A800 39D1B717 84B240
A9A804 3B83126E 84B260
A9A808 3BB60B6A 84B280
A9A8A4 39A3D70A 84B2B0
A9A8A8 3CA3D70A 84B2D0
A9A8AC 41DE38E4 84B2F0
A9A8B0 39D1B717 84B310
A9A8B4 3B83126E 84B330
A9A8B8 3BB60B6A 84B350
A9A8BC 39A3D70A 84B370
A9AD7C 39A3D70A 84B390
A9AD80 3CA3D70A 84B3B0
A9AD84 41DE38E4 84B3D0
A9AD88 39D1B717 84B3F0
A9AD8C 3B83126E 84B410
A9AD90 3BB60B6A 84B430
A9AD98 39A3D70A 84B450
A9AD9C 3CA3D70A 84B470
A9ADA0 41DE38E4 84B490
A9ADA4 39D1B717 84B4B0
A9ADA8 3B83126E 84B4D0
A9ADAC 3BB60B6A 84B4F0
A9ADE8 39A3D70A 84B510
A9ADEC 3CA3D70A 84B530
A9ADF0 41DE38E4 84B550
A9ADF4 39D1B717 84B570
A9ADF8 3B83126E 84B590
A9ADFC 3BB60B6A 84B5B0
A9AE70 39A3D70A 84B5F0
A9AE74 39A3D70A 84B610
A9AE78 39A3D70A 84B630
A9AF3C 39A3D70A 84B670
A9AF40 3CA3D70A 84B690
A9AF44 41DE38E4 84B6B0
A9AF48 39D1B717 84B6D0
A9AF4C 3B83126E 84B6F0
A9AF50 3BB60B6A 84B710
A9AF54 39A3D70A 84B730
A9AF58 3CA3D70A 84B750
A9AF5C 41DE38E4 84B770
A9AF60 39D1B717 84B790
A9AF64 3B83126E 84B7B0
A9AF68 3BB60B6A 84B7D0
A9AF70 39A3D70A 84B7F0
A9AF74 3CA3D70A 84B810
A9AF78 41DE38E4 84B830
A9AF7C 39D1B717 84B850
A9AF80 3B83126E 84B870
A9AF84 3BB60B6A 84B890
A9AF88 39A3D70A 84B8B0
A9AF8C 3CA3D70A 84B8D0
A9AF90 41DE38E4 84B8F0
A9AF94 39D1B717 84B910
A9AF98 3B83126E 84B930
A9AF9C 3BB60B6A 84B950
A9AFA0 39A3D70A 84B970
A9AFA4 3CA3D70A 84B990
A9AFA8 41DE38E4 84B9B0
A9AFAC 39D1B717 84B9D0
A9AFB0 3B83126E 84B9F0
A9AFB4 3BB60B6A 84BA10
A9B098 39A3D70A 84BA90
A9B09C 3CA3D70A 84BAB0
A9B0A0 41DE38E4 84BAD0
A9B0A4 39D1B717 84BAF0
A9B0A8 3B83126E 84BB10
A9B0AC 3BB60B6A 84BB30
A9B0B4 39A3D70A 84BB50
A9B0B8 39A3D70A 84BB70
A9B0BC 39A3D70A 84BB90
A9B0C0 39A3D70A 84BBB0
AAE94C 39A3D70A 84BBD0
B4DBDC 39A3D70A 84BD10
B4DBE0 39A3D70A 84BD30
B4E6C0 39A3D70A 84BD50
B4E6C4 3CA3D70A 84BD70
B4E6C8 41DE38E4 84BD90
B4E6CC 39D1B717 84BDB0
B4E6D0 3B83126E 84BDD0
B4E6D4 3BB60B6A 84BDF0
B4E754 39A3D70A 84BE10
B4E758 3CA3D70A 84BE30
B4E75C 41DE38E4 84BE50
B4E760 39D1B717 84BE70
B4E764 3B83126E 84BE90
B4E768 3BB60B6A 84BEB0
B4EA04 39A3D70A 84BED0
B4EA10 39A3D70A 84BEF0
B4EA14 3CA3D70A 84BF10
B4EA18 41DE38E4 84BF30
B4EA1C 39D1B717 84BF50
B4EA20 3B83126E 84BF70
B4EA24 3BB60B6A 84BF90
B4EA38 39A3D70A 84BFB0
B5F860 39A3D70A 84C020
B5F864 3CA3D70A 84C040
B5F868 41DE38E4 84C060
B5F86C 39D1B717 84C080
B5F870 3B83126E 84C0A0
B5F874 3BB60B6A 84C0C0
B5F880 39A3D70A 84C0E0
B5F884 3CA3D70A 84C100
B5F888 41DE38E4 84C120
B5F88C 39D1B717 84C140
B5F890 3B83126E 84C160
B5F894 3BB60B6A 84C180
B5F89C 39A3D70A 84C1B0
B5F8A0 3CA3D70A 84C1D0
B5F8A4 41DE38E4 84C1F0
B5F8A8 39D1B717 84C210
B5F8AC 3B83126E 84C230
B5F8B0 3BB60B6A 84C250
B61290 39A3D70A 84C2B0
B61294 3CA3D70A 84C2D0
B61298 41DE38E4 84C2F0
B6129C 39D1B717 84C310
B612A0 3B83126E 84C330
B612A4 3BB60B6A 84C350
B612A8 39A3D70A 84C370
B612AC 3CA3D70A 84C390
B612B0 41DE38E4 84C3B0
B612B4 39D1B717 84C3D0
B612B8 3B83126E 84C3F0
B612BC 3BB60B6A 84C410
B612C0 39A3D70A 84C430
B612C4 3CA3D70A 84C450
B612C8 41DE38E4 84C470
B612CC 39D1B717 84C490
B612D0 3B83126E 84C4B0
B612D4 3BB60B6A 84C4D0
B612E8 39A3D70A 84C520
B61300 39A3D70A 84C540
B61304 3CA3D70A 84C560
B61308 41DE38E4 84C580
B6130C 39D1B717 84C5A0
B61310 3B83126E 84C5C0
B61314 3BB60B6A 84C5E0
B6136C 39A3D70A 84C600
B61370 3CA3D70A 84C620
B61374 41DE38E4 84C640
B61378 39D1B717 84C660
B6137C 3B83126E 84C680
B61380 3BB60B6A 84C6A0
B6138C 39A3D70A 84C6C0
B61390 3CA3D70A 84C6E0
B61394 41DE38E4 84C700
B61398 39D1B717 84C720
B6139C 3B83126E 84C740
B613A0 3BB60B6A 84C760
B613A4 42A60000 84C780
B613A8 00000000 84C7A0
B613AC 42AA0000 84C7C0
B613B0 BD8F5C28 84C7E0
B613B4 41100000 84C800
B613B8 3E9EB851 84C820
B613BC 3ECCCCCE 84C840
B613C0 3F4CCCCD 84C860
B613C4 3ECCCCCE 84C880
B613C8 3F19999A 84C8A0
B613CC 3F000000 84C8C0
B61C1C 39A3D70A 84C8E0
B61C20 3CA3D70A 84C900
B61C24 41DE38E4 84C920
B61C28 39D1B717 84C940
B61C2C 3B83126E 84C960
B61C30 3BB60B6A 84C980
B61CE0 39A3D70A 84C9D0
B61CE4 3CA3D70A 84C9F0
B61CE8 41DE38E4 84CA10
B61CEC 39D1B717 84CA30
B61CF0 3B83126E 84CA50
B61CF4 3BB60B6A 84CA70
B61D58 3CA3D70A 84CA90
B61D5C 41DE38E4 84CAB0
B61D60 39D1B717 84CAD0
B61D64 3B83126E 84CAF0
B61D68 3BB60B6A 84CB10
B61D6C 39A3D70A 84CB30
B61D74 39A3D70A 84CB50
B62C74 3CA3D70A 84CB70
B62C78 41DE38E4 84CB90
B62C7C 39D1B717 84CBB0
B62C80 3B83126E 84CBD0
B62C84 3BB60B6A 84CBF0
B62C88 39A3D70A 84CC10
B62C8C 39A3D70A 84CC40
B62C90 3CA3D70A 84CC60
B62C94 41DE38E4 84CC80
B62C98 39D1B717 84CCA0
B62C9C 3B83126E 84CCC0
B62CA0 3BB60B6A 84CCE0
B62CA8 39A3D70A 84CD00
B6B96C 39A3D70A 84CD40
B6BA08 39A3D70A 84CD70
B6BA0C 3CA3D70A 84CD90
B6BA10 41DE38E4 84CDB0
B6BA14 39D1B717 84CDD0
B6BA18 3B83126E 84CDF0
B6BA1C 3BB60B6A 84CE10
B6BA20 3EB33334 84CE30
B6BA24 40400000 84CE50
B6BA28 3F19999A 84CE70
B6BA2C 40900000 84CE90
B6BA30 3E800000 84CEB0
B6BA34 40000000 84CED0
B6BA38 3ECCCCCD 84CEF0
B6BA40 3F266666 84CF20
B6BA44 3FC00000 84CF40
B6BA48 3EA66666 84CF60
B6BA4C 40000000 84CF80
B6BA50 3B0DFEA2 84CFA0
B6BA5C 3F000000 84CFF0
B6BA60 3F800000 84D010
B6BA64 40800000 84D030
B6BA68 3E4CCCCD 84D050
B6BA6C 3DCCCCC8 84D070
B6BA70 3F333334 84D090
B6BA74 3F333332 84D0B0
B6BA78 3ECCCCCE 84D0D0
B6BA7C 41100000 84D0F0
B6BA80 3E4CCCD0 84D110
B6BA84 3ECCCCCE 84D130
B6BA88 3EA3D70A 84D150
B6BA8C 40400000 84D170
B6BA90 40C00000 84D1B0
B6BA94 3EB33332 84D1D0
B6BA98 40C00000 84D1F0
B6BA9C 3EB33334 84D210
B6BAA0 40400000 84D230
B6BAA4 3E4CCCD0 84D250
B6BAA8 40C00000 84D270
B6BAAC 3E99999C 84D290
B6BAB0 41000000 84D2B0
B6BAB4 3DB851E8 84D2D0
B6BAB8 41D80000 84D2F0
B6BABC 3E4CCCCC 84D310
B6BADC 39A3D70A 84D330
B6BAE0 39A3D70A 84D350
B6BAE4 3CA3D70A 84D370
B6BAE8 41DE38E4 84D390
B6BAEC 39D1B717 84D3B0
B6BAF0 3B83126E 84D3D0
B6BAF4 3BB60B6A 84D3F0
B6BB00 39A3D70A 84D410
B6BB04 3CA3D70A 84D430
B6BB08 41DE38E4 84D450
B6BB0C 39D1B717 84D470
B6BB10 3B83126E 84D490
B6BB14 3BB60B6A 84D4B0
B6BC78 39A3D70A 84D510
B6BC7C 3CA3D70A 84D530
B6BC80 41DE38E4 84D550
B6BC84 39D1B717 84D570
B6BC88 3B83126E 84D590
B6BC8C 3BB60B6A 84D5B0
B6EBA0 39A3D70A 84D5F0
B6EC88 39A3D70A 84D610
B6EC8C 3CA3D70A 84D630
B6EC90 41DE38E4 84D650
B6EC94 39D1B717 84D670
B6EC98 3B83126E 84D690
B6EC9C 3BB60B6A 84D6B0
B70160 39A3D70A 84D7A0
B70164 3CA3D70A 84D7C0
B70168 41DE38E4 84D7E0
B7016C 39D1B717 84D800
B70170 3B83126E 84D820
B70174 3BB60B6A 84D840
B7017C 39A3D70A 84D860
B70180 3CA3D70A 84D880
B70184 41DE38E4 84D8A0
B70188 39D1B717 84D8C0
B7018C 3B83126E 84D8E0
B70190 3BB60B6A 84D900
B714F0 39A3D70A 84D930
B714F4 3CA3D70A 84D950
B714F8 41DE38E4 84D970
B714FC 39D1B717 84D990
B71500 3B83126E 84D9B0
B71504 3BB60B6A 84D9D0
B717D4 39A3D70A 84D9F0
B717D8 3CA3D70A 84DA10
B717DC 41DE38E4 84DA30
B717E0 39D1B717 84DA50
B717E4 3B83126E 84DA70
B717E8 3BB60B6A 84DA90
B717EC 39A3D70A 84DAB0
B717F0 3CA3D70A 84DAD0
B717F4 41DE38E4 84DAF0
B717F8 39D1B717 84DB10
B717FC 3B83126E 84DB30
B71800 3BB60B6A 84DB50
B7180C 39A3D70A 84DB70
B71810 3CA3D70A 84DB90
B71814 41DE38E4 84DBB0
B71818 39D1B717 84DBD0
B7181C 3B83126E 84DBF0
B71820 3BB60B6A 84DC10
B71824 39A3D70A 84DC30
B71828 3CA3D70A 84DC50
B7182C 41DE38E4 84DC70
B71830 39D1B717 84DC90
B71834 3B83126E 84DCB0
B71838 3BB60B6A 84DCD0
B71A48 39A3D70A 84DCF0
B71A4C 3CA3D70A 84DD10
B71A50 41DE38E4 84DD30
B71A54 39D1B717 84DD50
B71A58 3B83126E 84DD70
B71A5C 3BB60B6A 84DD90
B71F68 39A3D70A 84DDB0
B71F6C 3CA3D70A 84DDD0
B71F70 41DE38E4 84DDF0
B71F74 39D1B717 84DE10
B71F78 3B83126E 84DE30
B71F7C 3BB60B6A 84DE50
B72928 39A3D70A 84DE90
B7292C 3CA3D70A 84DEB0
B72930 41DE38E4 84DED0
B72934 39D1B717 84DEF0
B72938 3B83126E 84DF10
B7293C 3BB60B6A 84DF30
B72960 39A3D70A 84DF50
B72964 3CA3D70A 84DF70
B72968 41DE38E4 84DF90
B7296C 39D1B717 84DFB0
B72970 3B83126E 84DFD0
B72974 3BB60B6A 84DFF0
B72C88 39A3D70A 84E030
B72C8C 3CA3D70A 84E050
B72C90 41DE38E4 84E070
B72C94 39D1B717 84E090
B72C98 3B83126E 84E0B0
B72C9C 3BB60B6A 84E0D0
B72CAC 39A3D70A 84E110
B73440 39A3D70A 84E130
B73444 3CA3D70A 84E150
B73448 41DE38E4 84E170
B7344C 39D1B717 84E190
B73450 3B83126E 84E1B0
B73454 3BB60B6A 84E1D0
B7421C 39A3D70A 84E220
B74220 3CA3D70A 84E240
B74224 41DE38E4 84E260
B74228 39D1B717 84E280
B7422C 3B83126E 84E2A0
B74230 3BB60B6A 84E2C0
B744C8 39A3D70A 84E320
B744CC 3CA3D70A 84E340
B744D0 41DE38E4 84E360
B744D4 39D1B717 84E380
B744D8 3B83126E 84E3A0
B744DC 3BB60B6A 84E3C0
B745A4 39A3D70A 84E3E0
B745A8 3CA3D70A 84E400
B745AC 41DE38E4 84E420
B745B0 39D1B717 84E440
B745B4 3B83126E 84E460
B745B8 3BB60B6A 84E480
B76858 39A3D70A 84E4A0
B7685C 3CA3D70A 84E4C0
B76860 41DE38E4 84E4E0
B76864 39D1B717 84E500
B76868 3B83126E 84E520
B7686C 3BB60B6A 84E540
B7689C 39A3D70A 84E560
B768A0 3CA3D70A 84E580
B768A4 41DE38E4 84E5A0
B768A8 39D1B717 84E5C0
B768AC 3B83126E 84E5E0
B768B0 3BB60B6A 84E600
B79518 39A3D70A 84E620
B7951C 3CA3D70A 84E640
B79520 41DE38E4 84E660
B79524 39D1B717 84E680
B79528 3B83126E 84E6A0
B7952C 3BB60B6A 84E6C0
B7C488 39A3D70A 84E6F0
B7C48C 3CA3D70A 84E710
B7C490 41DE38E4 84E730
B7C494 39D1B717 84E750
B7C498 3B83126E 84E770
B7C49C 3BB60B6A 84E790
B7CB90 39A3D70A 84E800
B7CB94 3CA3D70A 84E820
B7CB98 41DE38E4 84E840
B7CB9C 39D1B717 84E860
B7CBA0 3B83126E 84E880
B7CBA4 3BB60B6A 84E8A0
B7CD80 39A3D70A 84E8C0
B7CD84 3CA3D70A 84E8E0
B7CD88 41DE38E4 84E900
B7CD8C 39D1B717 84E920
B7CD90 3B83126E 84E940
B7CD94 3BB60B6A 84E960
B9B7D4 39A3D70A 84EA60
B9B7D8 3CA3D70A 84EA80
B9B7DC 41DE38E4 84EAA0
B9B7E0 39D1B717 84EAC0
B9B7E4 3B83126E 84EAE0
B9B7E8 3BB60B6A 84EB00
B9B97C 39A3D70A 84EB20
B9B980 3CA3D70A 84EB40
B9B984 41DE38E4 84EB60
B9B988 39D1B717 84EB80
B9B98C 3B83126E 84EBA0
B9B990 3BB60B6A 84EBC0
BA176C 39A3D70A 84EBE0
BA1770 3CA3D70A 84EC00
BA1774 41DE38E4 84EC20
BA1778 39D1B717 84EC40
BA177C 3B83126E 84EC60
BA1780 3BB60B6A 84EC80
BA18DC 39A3D70A 84ECA0
BA18E0 3CA3D70A 84ECC0
BA18E4 41DE38E4 84ECE0
BA18E8 39D1B717 84ED00
BA18EC 3B83126E 84ED20
BA18F0 3BB60B6A 84ED40
BA671C 39A3D70A 84ED60
BA672C 39A3D70A 84ED80
BA6730 3CA3D70A 84EDA0
BA6734 41DE38E4 84EDC0
BA6738 39D1B717 84EDE0
BA673C 3B83126E 84EE00
BA6740 3BB60B6A 84EE20
BA82C0 39A3D70A 84EE60
BA82C4 3CA3D70A 84EE80
BA82C8 41DE38E4 84EEA0
BA82CC 39D1B717 84EEC0
BA82D0 3B83126E 84EEE0
BA82D4 3BB60B6A 84EF00
BA82E4 39A3D70A 84EF20
BA82E8 3CA3D70A 84EF40
BA82EC 41DE38E4 84EF60
BA82F0 39D1B717 84EF80
BA82F4 3B83126E 84EFA0
BA82F8 3BB60B6A 84EFC0
BA86BC 39A3D70A 84EFE0
BA86C0 3CA3D70A 84F000
BA86C4 41DE38E4 84F020
BA86C8 39D1B717 84F040
BA86CC 3B83126E 84F060
BA86D0 3BB60B6A 84F080
BAB1E4 39A3D70A 84F100
BAB1E8 3CA3D70A 84F120
BAB1EC 41DE38E4 84F140
BAB1F0 39D1B717 84F160
BAB1F4 3B83126E 84F180
BAB1F8 3BB60B6A 84F1A0
BAB344 39A3D70A 84F210
BAB348 3CA3D70A 84F230
BAB34C 41DE38E4 84F250
BAB350 39D1B717 84F270
BAB354 3B83126E 84F290
BAB358 3BB60B6A 84F2B0
BAF5F8 39A3D70A 84F380
BAF5FC 3CA3D70A 84F3A0
BAF600 41DE38E4 84F3C0
BAF604 39D1B717 84F3E0
BAF608 3B83126E 84F400
BAF60C 3BB60B6A 84F420
BAF610 39A3D70A 84F440
BAF614 3CA3D70A 84F460
BAF618 41DE38E4 84F480
BAF61C 39D1B717 84F4A0
BAF620 3B83126E 84F4C0
BAF624 3BB60B6A 84F4E0
BAF628 39A3D70A 84F500
BAF62C 3CA3D70A 84F520
BAF630 41DE38E4 84F540
BAF634 39D1B717 84F560
BAF638 3B83126E 84F580
BAF63C 3BB60B6A 84F5A0
BAF640 39A3D70A 84F5C0
BAF644 3CA3D70A 84F5E0
BAF648 41DE38E4 84F600
BAF64C 39D1B717 84F620
BAF650 3B83126E 84F640
BAF654 3BB60B6A 84F660
BAF658 39A3D70A 84F680
BAF65C 3CA3D70A 84F6A0
BAF660 41DE38E4 84F6C0
BAF664 39D1B717 84F6E0
BAF668 3B83126E 84F700
BAF66C 3BB60B6A 84F720
BB3DCC 39A3D70A 84F760
BB3DD0 3CA3D70A 84F780
BB3DD4 41DE38E4 84F7A0
BB3DD8 39D1B717 84F7C0
BB3DDC 3B83126E 84F7E0
BB3DE0 3BB60B6A 84F800
BB3DE8 39A3D70A 84F820
BB3DEC 3CA3D70A 84F840
BB3DF0 41DE38E4 84F860
BB3DF4 39D1B717 84F880
BB3DF8 3B83126E 84F8A0
BB3DFC 3BB60B6A 84F8C0
BB4200 39A3D70A 84F8E0
BB4204 3CA3D70A 84F900
BB4208 41DE38E4 84F920
BB420C 39D1B717 84F940
BB4210 3B83126E 84F960
BB4214 3BB60B6A 84F980
BB421C 39A3D70A 84F9A0
BB4220 3CA3D70A 84F9C0
BB4224 41DE38E4 84F9E0
BB4228 39D1B717 84FA00
BB422C 3B83126E 84FA20
BB4230 3BB60B6A 84FA40
BB423C 39A3D70A 84FA60
BB4A40 39A3D70A 84FAA0
BB4A44 3CA3D70A 84FAC0
BB4A48 41DE38E4 84FAE0
BB4A4C 39D1B717 84FB00
BB4A50 3B83126E 84FB20
BB4A54 3BB60B6A 84FB40
BB4A58 39A3D70A 84FB60
BB4A5C 3CA3D70A 84FB80
BB4A60 41DE38E4 84FBA0
BB4A64 39D1B717 84FBC0
BB4A68 3B83126E 84FBE0
BB4A6C 3BB60B6A 84FC00
BB4A74 39A3D70A 84FC20
BB4A78 3CA3D70A 84FC40
BB4A7C 41DE38E4 84FC60
BB4A80 39D1B717 84FC80
BB4A84 3B83126E 84FCA0
BB4A88 3BB60B6A 84FCC0
BB7C90 39A3D70A 84FCE0
BB7C98 39A3D70A 84FD00
BB7C9C 3CA3D70A 84FD20
BB7CA0 41DE38E4 84FD40
BB7CA4 39D1B717 84FD60
BB7CA8 3B83126E 84FD80
BB7CAC 3BB60B6A 84FDA0
BBC8C4 39A3D70A 84FDE0
BC128C 39A3D70A 84FE00
BC1C60 39A3D70A 84FE70
BC1C64 3CA3D70A 84FE90
BC1C68 41DE38E4 84FEB0
BC1C6C 39D1B717 84FED0
BC1C70 3B83126E 84FEF0
BC1C74 3BB60B6A 84FF10
BC1CF0 39A3D70A 84FF60
BC4008 39A3D70A 84FF80
BC400C 3CA3D70A 84FFA0
BC4010 41DE38E4 84FFC0
BC4014 39D1B717 84FFE0
BC4018 3B83126E 850000
BC401C 3BB60B6A 850020
BC405C 39A3D70A 850060
BC4060 3CA3D70A 850080
BC4064 41DE38E4 8500A0
BC4068 39D1B717 8500C0
BC406C 3B83126E 8500E0
BC4070 3BB60B6A 850100
BC408C 39A3D70A 850120
BC40BC 39A3D70A 850140
BC40C0 3CA3D70A 850160
BC40C4 41DE38E4 850180
BC40C8 39D1B717 8501A0
BC40CC 3B83126E 8501C0
BC40D0 3BB60B6A 8501E0
BD00E0 39A3D70A 850200
BD00E4 3CA3D70A 850220
BD00E8 41DE38E4 850240
BD00EC 39D1B717 850260
BD00F0 3B83126E 850280
BD00F4 3BB60B6A 8502A0
BD00FC 39A3D70A 8502C0
BD0100 3CA3D70A 8502E0
BD0104 41DE38E4 850300
BD0108 39D1B717 850320
BD010C 3B83126E 850340
BD0110 3BB60B6A 850360
BD0118 39A3D70A 850380
BD011C 3CA3D70A 8503A0
BD0120 41DE38E4 8503C0
BD0124 39D1B717 8503E0
BD0128 3B83126E 850400
BD012C 3BB60B6A 850420
BD0130 3FBFFFFF 850440
BD0134 40800000 850460
BD0138 39A3D70A 850480
BD013C 3CA3D70A 8504A0
BD0140 41DE38E4 8504C0
BD0144 39D1B717 8504E0
BD0148 3B83126E 850500
BD014C 3BB60B6A 850520
BD0150 39A3D70A 850540
C02B84 39A3D70A 850560
C02B88 3CA3D70A 850580
C02B8C 41DE38E4 8505A0
C02B90 39D1B717 8505C0
C02B94 3B83126E 8505E0
C02B98 3BB60B6A 850600
C02B9C 39A3D70A 850620
C02BA0 3CA3D70A 850640
C02BA4 41DE38E4 850660
C02BA8 39D1B717 850680
C02BAC 3B83126E 8506A0
C02BB0 3BB60B6A 8506C0
C02BB4 39A3D70A 8506E0
C02BB8 3CA3D70A 850700
C02BBC 41DE38E4 850720
C02BC0 39D1B717 850740
C02BC4 3B83126E 850760
C02BC8 3BB60B6A 850780
C02C10 39A3D70A 8507A0
C02C24 39A3D70A 8507C0
C02D34 39A3D70A 8507E0
C02DC4 39A3D70A 850800
C02DC8 3CA3D70A 850820
C02DCC 41DE38E4 850840
C02DD0 39D1B717 850860
C02DD4 3B83126E 850880
C02DD8 3BB60B6A 8508A0
C03A40 39A3D70A 8508F0
C09178 39A3D70A 8509A0
C0917C 39A3D70A 8509C0
C09180 3CA3D70A 8509E0
C09184 41DE38E4 850A00
C09188 39D1B717 850A20
C0918C 3B83126E 850A40
C09190 3BB60B6A 850A60
C09194 39A3D70A 850A80
C09198 3CA3D70A 850AA0
C0919C 41DE38E4 850AC0
C091A0 39D1B717 850AE0
C091A4 3B83126E 850B00
C091A8 3BB60B6A 850B20
C091AC 39A3D70A 850B40
C091B0 3CA3D70A 850B60
C091B4 41DE38E4 850B80
C091B8 39D1B717 850BA0
C091BC 3B83126E 850BC0
C091C0 3BB60B6A 850BE0
C091C4 39A3D70A 850C00
C091C8 3CA3D70A 850C20
C091CC 41DE38E4 850C40
C091D0 39D1B717 850C60
C091D4 3B83126E 850C80
C091D8 3BB60B6A 850CA0
C091EC 39A3D70A 850CC0
C09290 39A3D70A 850D10
C09294 3CA3D70A 850D30
C09298 41DE38E4 850D50
C0929C 39D1B717 850D70
C092A0 3B83126E 850D90
C092A4 3BB60B6A 850DB0
C09828 39A3D70A 850DD0
C0982C 3CA3D70A 850DF0
C09830 41DE38E4 850E10
C09834 39D1B717 850E30
C09838 3B83126E 850E50
C0983C 3BB60B6A 850E70
C09844 39A3D70A 850E90
C09848 3CA3D70A 850EB0
C0984C 41DE38E4 850ED0
C09850 39D1B717 850EF0
C09854 3B83126E 850F10
C09858 3BB60B6A 850F30
C0987C 39A3D70A 850F70
C09880 3CA3D70A 850F90
C09884 41DE38E4 850FB0
C09888 39D1B717 850FD0
C0988C 3B83126E 850FF0
C09890 3BB60B6A 851010
C0989C 39A3D70A 851030
C098A0 3CA3D70A 851050
C098A4 41DE38E4 851070
C098A8 39D1B717 851090
C098AC 3B83126E 8510B0
C098B0 3BB60B6A 8510D0
C098B8 39A3D70A 8510F0
C098BC 3CA3D70A 851110
C098C0 41DE38E4 851130
C098C4 39D1B717 851150
C098C8 3B83126E 851170
C098CC 3BB60B6A 851190
C098F0 39A3D70A 8511B0
C098F4 3CA3D70A 8511D0
C098F8 41DE38E4 8511F0
C098FC 39D1B717 851210
C09900 3B83126E 851230
C09904 3BB60B6A 851250
C0AFC4 39A3D70A 851360
C0AFC8 3CA3D70A 851380
C0AFCC 41DE38E4 8513A0
C0AFD0 39D1B717 8513C0
C0AFD4 3B83126E 8513E0
C0AFD8 3BB60B6A 851400
C0B03C 39A3D70A 851420
C0B040 3CA3D70A 851440
C0B044 41DE38E4 851460
C0B048 39D1B717 851480
C0B04C 3B83126E 8514A0
C0B050 3BB60B6A 8514C0
C0B1B8 39A3D70A 851500
C0B1BC 3CA3D70A 851520
C0B1C0 41DE38E4 851540
C0B1C4 39D1B717 851560
C0B1C8 3B83126E 851580
C0B1CC 3BB60B6A 8515A0
C0B1D0 39A3D70A 8515C0
C0B1D4 3CA3D70A 8515E0
C0B1D8 41DE38E4 851600
C0B1DC 39D1B717 851620
C0B1E0 3B83126E 851640
C0B1E4 3BB60B6A 851660
C0BBF0 39A3D70A 8516B0
C0BBF4 3CA3D70A 8516D0
C0BBF8 41DE38E4 8516F0
C0BBFC 39D1B717 851710
C0BC00 3B83126E 851730
C0BC04 3BB60B6A 851750
C0BC18 39A3D70A 851770
C0BC1C 3CA3D70A 851790
C0BC20 41DE38E4 8517B0
C0BC24 39D1B717 8517D0
C0BC28 3B83126E 8517F0
C0BC2C 3BB60B6A 851810
C0E97C 39A3D70A 851830
C0E980 3CA3D70A 851850
C0E984 41DE38E4 851870
C0E988 39D1B717 851890
C0E98C 3B83126E 8518B0
C0E990 3BB60B6A 8518D0
C0FCC0 39A3D70A 8518F0
C0FCC4 3CA3D70A 851910
C0FCC8 41DE38E4 851930
C0FCCC 39D1B717 851950
C0FCD0 3B83126E 851970
C0FCD4 3BB60B6A 851990
C10288 39A3D70A 8519B0
C1028C 3CA3D70A 8519D0
C10290 41DE38E4 8519F0
C10294 39D1B717 851A10
C10298 3B83126E 851A30
C1029C 3BB60B6A 851A50
C15430 39A3D70A 851A90
C15434 3CA3D70A 851AB0
C15438 41DE38E4 851AD0
C1543C 39D1B717 851AF0
C15440 3B83126E 851B10
C15444 3BB60B6A 851B30
C16F00 39A3D70A 851B70
C16F04 3CA3D70A 851B90
C16F08 41DE38E4 851BB0
C16F0C 39D1B717 851BD0
C16F10 3B83126E 851BF0
C16F14 3BB60B6A 851C10
C1701C 39A3D70A 851C30
C17020 3CA3D70A 851C50
C17024 41DE38E4 851C70
C17028 39D1B717 851C90
C1702C 3B83126E 851CB0
C17030 3BB60B6A 851CD0
C17088 39A3D70A 851CF0
C1708C 3CA3D70A 851D10
C17090 41DE38E4 851D30
C17094 39D1B717 851D50
C17098 3B83126E 851D70
C1709C 3BB60B6A 851D90
C170A0 39A3D70A 851DB0
C170A4 3CA3D70A 851DD0
C170A8 41DE38E4 851DF0
C170AC 39D1B717 851E10
C170B0 3B83126E 851E30
C170B4 3BB60B6A 851E50
C170B8 39A3D70A 851E70
C170BC 3CA3D70A 851E90
C170C0 41DE38E4 851EB0
C170C4 39D1B717 851ED0
C170C8 3B83126E 851EF0
C170CC 3BB60B6A 851F10
C177B8 39A3D70A 851F30
C177BC 3CA3D70A 851F50
C177C0 41DE38E4 851F70
C177C4 39D1B717 851F90
C177C8 3B83126E 851FB0
C177CC 3BB60B6A 851FD0
C178D8 39A3D70A 852020
C178DC 3CA3D70A 852040
C178E0 41DE38E4 852060
C178E4 39D1B717 852080
C178E8 3B83126E 8520A0
C178EC 3BB60B6A 8520C0
C18BF0 39A3D70A 852140
C18BF4 3CA3D70A 852160
C18BF8 41DE38E4 852180
C18BFC 39D1B717 8521A0
C18C00 3B83126E 8521C0
C18C04 3BB60B6A 8521E0
C18C08 39A3D70A 852200
C18C0C 3CA3D70A 852220
C18C10 41DE38E4 852240
C18C14 39D1B717 852260
C18C18 3B83126E 852280
C18C1C 3BB60B6A 8522A0
C18C24 39A3D70A 8522C0
C18C28 3CA3D70A 8522E0
C18C2C 41DE38E4 852300
C18C30 39D1B717 852320
C18C34 3B83126E 852340
C18C38 3BB60B6A 852360
C18C84 39A3D70A 852380
C18C88 3CA3D70A 8523A0
C18C8C 41DE38E4 8523C0
C18C90 39D1B717 8523E0
C18C94 3B83126E 852400
C18C98 3BB60B6A 852420
C18C9C 39A3D70A 852440
C18CA0 3CA3D70A 852460
C18CA4 41DE38E4 852480
C18CA8 39D1B717 8524A0
C18CAC 3B83126E 8524C0
C18CB0 3BB60B6A 8524E0
C18CB4 39A3D70A 852500
C18CB8 3CA3D70A 852520
C18CBC 41DE38E4 852540
C18CC0 39D1B717 852560
C18CC4 3B83126E 852580
C18CC8 3BB60B6A 8525A0
C18CCC 39A3D70A 8525C0
C18CD0 3CA3D70A 8525E0
C18CD4 41DE38E4 852600
C18CD8 39D1B717 852620
C18CDC 3B83126E 852640
C18CE0 3BB60B6A 852660
C18D0C 39A3D70A 8526C0
C18D10 3CA3D70A 8526E0
C18D14 41DE38E4 852700
C18D18 39D1B717 852720
C18D1C 3B83126E 852740
C18D20 3BB60B6A 852760
C18D30 39A3D70A 852780
C18D34 3CA3D70A 8527A0
C18D38 41DE38E4 8527C0
C18D3C 39D1B717 8527E0
C18D40 3B83126E 852800
C18D44 3BB60B6A 852820
C18D48 3F6C835E 852840
C18F5C 39A3D70A 852890
C18F60 3CA3D70A 8528B0
C18F64 41DE38E4 8528D0
C18F68 39D1B717 8528F0
C18F6C 3B83126E 852910
C18F70 3BB60B6A 852930
C18F80 39A3D70A 852950
C18F84 3CA3D70A 852970
C18F88 41DE38E4 852990
C18F8C 39D1B717 8529B0
C18F90 3B83126E 8529D0
C18F94 3BB60B6A 8529F0
C19634 39A3D70A 852A90
C19638 3CA3D70A 852AB0
C1963C 41DE38E4 852AD0
C19640 39D1B717 852AF0
C19644 3B83126E 852B10
C19648 3BB60B6A 852B30
C1964C 39A3D70A 852B50
C19650 3CA3D70A 852B70
C19654 41DE38E4 852B90
C19658 39D1B717 852BB0
C1965C 3B83126E 852BD0
C19660 3BB60B6A 852BF0
C19688 39A3D70A 852C10
C1968C 3CA3D70A 852C30
C19690 41DE38E4 852C50
C19694 39D1B717 852C70
C19698 3B83126E 852C90
C1969C 3BB60B6A 852CB0
C196A0 39A3D70A 852CD0
C196A4 3CA3D70A 852CF0
C196A8 41DE38E4 852D10
C196AC 39D1B717 852D30
C196B0 3B83126E 852D50
C196B4 3BB60B6A 852D70
C196B8 39A3D70A 852D90
C196BC 3CA3D70A 852DB0
C196C0 41DE38E4 852DD0
C196C4 39D1B717 852DF0
C196C8 3B83126E 852E10
C196CC 3BB60B6A 852E30
C196D0 39A3D70A 852E50
C196D4 3CA3D70A 852E70
C196D8 41DE38E4 852E90
C196DC 39D1B717 852EB0
C196E0 3B83126E 852ED0
C196E4 3BB60B6A 852EF0
C1976C 39A3D70A 852F30
C19770 3CA3D70A 852F50
C19774 41DE38E4 852F70
C19778 39D1B717 852F90
C1977C 3B83126E 852FB0
C19780 3BB60B6A 852FD0
C19784 39A3D70A 852FF0
C19788 3CA3D70A 853010
C1978C 41DE38E4 853030
C19790 39D1B717 853050
C19794 3B83126E 853070
C19798 3BB60B6A 853090
C197B0 39A3D70A 8530B0
C197B4 3CA3D70A 8530D0
C197B8 41DE38E4 8530F0
C197BC 39D1B717 853110
C197C0 3B83126E 853130
C197C4 3BB60B6A 853150
C1A300 39A3D70A 853180
C1A304 3CA3D70A 8531A0
C1A308 41DE38E4 8531C0
C1A30C 39D1B717 8531E0
C1A310 3B83126E 853200
C1A314 3BB60B6A 853220
C1A558 39A3D70A 8532A0
C1A55C 3CA3D70A 8532C0
C1A560 41DE38E4 8532E0
C1A564 39D1B717 853300
C1A568 3B83126E 853320
C1A56C 3BB60B6A 853340
C1AEB0 39A3D70A 853360
C1AEB4 39A3D70A 853380
C1B328 39A3D70A 8533A0
C1B32C 3CA3D70A 8533C0
C1B330 41DE38E4 8533E0
C1B334 39D1B717 853400
C1B338 3B83126E 853420
C1B33C 3BB60B6A 853440
C1BFDC 39A3D70A 853480
C1BFE0 3CA3D70A 8534A0
C1BFE4 41DE38E4 8534C0
C1BFE8 39D1B717 8534E0
C1BFEC 3B83126E 853500
C1BFF0 3BB60B6A 853520
C1C208 00000000 853540
C1C20C 408B020C 853540
C1C210 3F2C8B44 853540
C1C808 39A3D70A 853580
C1C80C 3CA3D70A 8535A0
C1C810 41DE38E4 8535C0
C1C814 39D1B717 8535E0
C1C818 3B83126E 853600
C1C81C 3BB60B6A 853620
C1C824 39A3D70A 853640
C1C828 3CA3D70A 853660
C1C82C 41DE38E4 853680
C1C830 39D1B717 8536A0
C1C834 3B83126E 8536C0
C1C838 3BB60B6A 8536E0
C1C840 39A3D70A 853700
C1C844 3CA3D70A 853720
C1C848 41DE38E4 853740
C1C84C 39D1B717 853760
C1C850 3B83126E 853780
C1C854 3BB60B6A 8537A0
C1C878 39A3D70A 8537C0
C1C87C 3CA3D70A 8537E0
C1C880 41DE38E4 853800
C1C884 39D1B717 853820
C1C888 3B83126E 853840
C1C88C 3BB60B6A 853860
C1C974 39A3D70A 8538A0
C1C978 3CA3D70A 8538C0
C1C97C 41DE38E4 8538E0
C1C980 39D1B717 853900
C1C984 3B83126E 853920
C1C988 3BB60B6A 853940
C1CAC0 39A3D70A 853960
C1CAC4 3CA3D70A 853980
C1CAC8 41DE38E4 8539A0
C1CACC 39D1B717 8539C0
C1CAD0 3B83126E 8539E0
C1CAD4 3BB60B6A 853A00
C1CAE4 39A3D70A 853A20
C1CAE8 3CA3D70A 853A40
C1CAEC 41DE38E4 853A60
C1CAF0 39D1B717 853A80
C1CAF4 3B83126E 853AA0
C1CAF8 3BB60B6A 853AC0
C1CB00 39A3D70A 853AE0
C1CB04 3CA3D70A 853B00
C1CB08 41DE38E4 853B20
C1CB0C 39D1B717 853B40
C1CB10 3B83126E 853B60
C1CB14 3BB60B6A 853B80
C1CB18 39A3D70A 853BA0
C1CB1C 3CA3D70A 853BC0
C1CB20 41DE38E4 853BE0
C1CB24 39D1B717 853C00
C1CB28 3B83126E 853C20
C1CB2C 3BB60B6A 853C40
C1CB44 3CA3D70A 853C60
C1CB48 41DE38E4 853C80
C1CB4C 39D1B717 853CA0
C1CB50 3B83126E 853CC0
C1CB54 3BB60B6A 853CE0
C1CC5C 39A3D70A 853D00
C1CC60 3CA3D70A 853D20
C1CC64 41DE38E4 853D40
C1CC68 39D1B717 853D60
C1CC6C 3B83126E 853D80
C1CC70 3BB60B6A 853DA0
C1DF18 39A3D70A 853E50
C1DF1C 3CA3D70A 853E70
C1DF20 41DE38E4 853E90
C1DF24 39D1B717 853EB0
C1DF28 3B83126E 853ED0
C1DF2C 3BB60B6A 853EF0
C228F4 39A3D70A 853F30
C228F8 3CA3D70A 853F50
C228FC 41DE38E4 853F70
C22900 39D1B717 853F90
C22904 3B83126E 853FB0
C22908 3BB60B6A 853FD0
C279B4 39A3D70A 853FF0
C279B8 3CA3D70A 854010
C279BC 41DE38E4 854030
C279C0 39D1B717 854050
C279C4 3B83126E 854070
C279C8 3BB60B6A 854090
C279CC 3C03126F 8540B0
C2B950 39A3D70A 8540D0
C2B954 3CA3D70A 8540F0
C2B958 41DE38E4 854110
C2B95C 39D1B717 854130
C2B960 3B83126E 854150
C2B964 3BB60B6A 854170
C2B978 39A3D70A 854190
C2B97C 3CA3D70A 8541B0
C2B980 41DE38E4 8541D0
C2B984 39D1B717 8541F0
C2B988 3B83126E 854210
C2B98C 3BB60B6A 854230
C2B990 39A3D70A 854250
C2B994 3CA3D70A 854270
C2B998 41DE38E4 854290
C2B99C 39D1B717 8542B0
C2B9A0 3B83126E 8542D0
C2B9A4 3BB60B6A 8542F0
C2B9AC 3CA3D70A 854310
C2B9B0 41DE38E4 854330
C2B9B4 39D1B717 854350
C2B9B8 3B83126E 854370
C2B9BC 3BB60B6A 854390
C2B9C0 39A3D70A 8543B0
C3804C 39A3D70A 854410
C38050 3CA3D70A 854430
C38054 41DE38E4 854450
C38058 39D1B717 854470
C3805C 3B83126E 854490
C38060 3BB60B6A 8544B0
C39ED8 39A3D70A 8544D0
C3A1E4 39A3D70A 8544F0
C3A1E8 3CA3D70A 854510
C3A1EC 41DE38E4 854530
C3A1F0 39D1B717 854550
C3A1F4 3B83126E 854570
C3A1F8 3BB60B6A 854590
C3E03C 39A3D70A 854610
C3E040 3CA3D70A 854630
C3E044 41DE38E4 854650
C3E048 39D1B717 854670
C3E04C 3B83126E 854690
C3E050 3BB60B6A 8546B0
C3EFB4 39A3D70A 8546D0
C3F038 39A3D70A 8546F0
C3F044 39A3D70A 854710
C3F0D8 39A3D70A 854730
C402E8 39A3D70A 854750
C402EC 3CA3D70A 854770
C402F0 41DE38E4 854790
C402F4 39D1B717 8547B0
C402F8 3B83126E 8547D0
C402FC 3BB60B6A 8547F0
C40338 39A3D70A 8548C0
C4033C 3CA3D70A 8548E0
C40340 41DE38E4 854900
C40344 39D1B717 854920
C40348 3B83126E 854940
C4034C 3BB60B6A 854960
C40418 39A3D70A 8549A0
C4041C 3CA3D70A 8549C0
C40420 41DE38E4 8549E0
C40424 39D1B717 854A00
C40428 3B83126E 854A20
C4042C 3BB60B6A 854A40
C4B6B0 43610000 854A60
C6A160 39A3D70A 854A80
C6A17C 39A3D70A 854AA0
C6A180 3CA3D70A 854AC0
C6A184 41DE38E4 854AE0
C6A188 39D1B717 854B00
C6A18C 3B83126E 854B20
C6A190 3BB60B6A 854B40
C6A8A8 39A3D70A 854B90
C6AA84 39A3D70A 854BB0
C6AA88 3CA3D70A 854BD0
C6AA8C 41DE38E4 854BF0
C6AA90 39D1B717 854C10
C6AA94 3B83126E 854C30
C6AA98 3BB60B6A 854C50
C6E990 39A3D70A 854C90
C6E994 3CA3D70A 854CB0
C6E998 41DE38E4 854CD0
C6E99C 39D1B717 854CF0
C6E9A0 3B83126E 854D10
C6E9A4 3BB60B6A 854D30
C71A58 39A3D70A 854D80
C71BDC 39A3D70A 854E20
C71BE0 3CA3D70A 854E40
C71BE4 41DE38E4 854E60
C71BE8 39D1B717 854E80
C71BEC 3B83126E 854EA0
C71BF0 3BB60B6A 854EC0
C73C34 39A3D70A 854F10
C73C38 3CA3D70A 854F30
C73C3C 41DE38E4 854F50
C73C40 39D1B717 854F70
C73C44 3B83126E 854F90
C73C48 3BB60B6A 854FB0
C73C4C 39A3D70A 854FD0
C73C60 39A3D70A 854FF0
C785F4 39A3D70A 855010
C785F8 3CA3D70A 855030
C785FC 41DE38E4 855050
C78600 39D1B717 855070
C78604 3B83126E 855090
C78608 3BB60B6A 8550B0
C799B0 39A3D70A 8550D0
C799B4 3CA3D70A 8550F0
C799B8 41DE38E4 855110
C799BC 39D1B717 855130
C799C0 3B83126E 855150
C799C4 3BB60B6A 855170
C79A8C 39A3D70A 855190
C79A90 3CA3D70A 8551B0
C79A94 41DE38E4 8551D0
C79A98 39D1B717 8551F0
C79A9C 3B83126E 855210
C79AA0 3BB60B6A 855230
C7C72C 39A3D70A 855250
C7C730 3CA3D70A 855270
C7C734 41DE38E4 855290
C7C738 39D1B717 8552B0
C7C73C 3B83126E 8552D0
C7C740 3BB60B6A 8552F0
C80548 39A3D70A 855340
C80724 39A3D70A 855360
C80728 3CA3D70A 855380
C8072C 41DE38E4 8553A0
C80730 39D1B717 8553C0
C80734 3B83126E 8553E0
C80738 3BB60B6A 855400
C81344 39A3D70A 855450
C81348 3CA3D70A 855470
C8134C 41DE38E4 855490
C81350 39D1B717 8554B0
C81354 3B83126E 8554D0
C81358 3BB60B6A 8554F0
C81454 39A3D70A 855530
C815BC 39A3D70A 855550
C87AD0 39A3D70A 855570
C87AD4 3CA3D70A 855590
C87AD8 41DE38E4 8555B0
C87ADC 39D1B717 8555D0
C87AE0 3B83126E 8555F0
C87AE4 3BB60B6A 855610
C87B0C 39A3D70A 855630
C87B38 39A3D70A 855650
C88000 39A3D70A 8556A0
C8801C 39A3D70A 8556C0
C88058 39A3D70A 8556E0
C8805C 3CA3D70A 855700
C88060 41DE38E4 855720
C88064 39D1B717 855740
C88068 3B83126E 855760
C8806C 3BB60B6A 855780
C8870C 39A3D70A 8557A0
C88710 3CA3D70A 8557C0
C88714 41DE38E4 8557E0
C88718 39D1B717 855800
C8871C 3B83126E 855820
C88720 3BB60B6A 855840
C88728 39A3D70A 855860
C8872C 3CA3D70A 855880
C88730 41DE38E4 8558A0
C88734 39D1B717 8558C0
C88738 3B83126E 8558E0
C8873C 3BB60B6A 855900
C888B8 39A3D70A 855920
C888BC 3CA3D70A 855940
C888C0 41DE38E4 855960
C888C4 39D1B717 855980
C888C8 3B83126E 8559A0
C888CC 3BB60B6A 8559C0
C89190 39A3D70A 855A00
C89194 3CA3D70A 855A20
C89198 41DE38E4 855A40
C8919C 39D1B717 855A60
C891A0 3B83126E 855A80
C891A4 3BB60B6A 855AA0
C89678 39A3D70A 855AC0
C8967C 3CA3D70A 855AE0
C89680 41DE38E4 855B00
C89684 39D1B717 855B20
C89688 3B83126E 855B40
C8968C 3BB60B6A 855B60
C8A7C4 39A3D70A 855B80
C8A7C8 3CA3D70A 855BA0
C8A7CC 41DE38E4 855BC0
C8A7D0 39D1B717 855BE0
C8A7D4 3B83126E 855C00
C8A7D8 3BB60B6A 855C20
C8A81C 39A3D70A 855C60
C8A820 3CA3D70A 855C80
C8A824 41DE38E4 855CA0
C8A828 39D1B717 855CC0
C8A82C 3B83126E 855CE0
C8A830 3BB60B6A 855D00
C8AAA0 39A3D70A 855D50
C8AAA4 3CA3D70A 855D70
C8AAA8 41DE38E4 855D90
C8AAAC 39D1B717 855DB0
C8AAB0 3B83126E 855DD0
C8AAB4 3BB60B6A 855DF0
C8CDCC 39D1B717 855EA0
C8CDD0 39A3D70A 855E40
C8CDD4 3CA3D70A 855E60
C8CDD8 41DE38E4 855E80
C8CDDC 3BB60B6A 855EE0
C8CDE0 3B83126E 855EC0
C9211C 39A3D70A 855F00
C92120 3CA3D70A 855F20
C92124 41DE38E4 855F40
C92128 39D1B717 855F60
C9212C 3B83126E 855F80
C92130 3BB60B6A 855FA0
```

## Appendix: delta generator (tested; also at /private/tmp/claude-501/-Users-Andrei-Mukhin-github-recomp-gta-reversed/aa4942c3-1b0c-4e82-a9c0-0c095cb1fe07/scratchpad/ia_scratch/initterm_delta.py)

```python
#!/usr/bin/env python3
"""initterm_delta.py <gta_sa_compact.exe> <out.json>   (pip install unicorn)
Runs the exe's own MSVC static-initialiser table (_initterm(__xc_a,__xc_z)) in Unicorn against the .data/.rdata initial image
(.bss = zero) and writes every byte range of .data/.bss (VA >= 0x8A4000) whose final value differs from the initial image:
  {"ranges":[[va,"hex"],...], "atexit":[dtorVA..], "rand_calls":N}
Environment modelled: x87 CW 0x27F (PC=53, what the CRT sets), rand() = MSVC LCG seeded 1, atexit recorded, security-cookie init skipped
(CRT-only, 0x8E31BC). Run order = table order. No Windows API is reachable from the initialisers (verified: 0 unmapped fetches)."""
import sys, struct, json
from unicorn import *
from unicorn.x86_const import *
BASE, END = 0x400000, 0xcb1000
XC_A, XC_Z = 0x8a4000, 0x8a5a10
SEC = [  # va, raw offset, raw size  (PE section table of gta_sa_compact.exe)
    (0x401000, 0x400, 0x455e00), (0x857000, 0x456200, 0x600), (0x858000, 0x456800, 0x4b400),
    (0x8a4000, 0x4a1c00, 0x40000), (0xc9e000, 0x4e1c00, 0x10c00), (0xcaf000, 0x4f2800, 0x200)]
d = open(sys.argv[1], 'rb').read()
init = bytearray(END - BASE)
for va, raw, sz in SEC: init[va - BASE:va - BASE + sz] = d[raw:raw + sz]
uc = Uc(UC_ARCH_X86, UC_MODE_32); uc.mem_map(BASE, END - BASE); uc.mem_write(BASE, bytes(init))
STACK, FS, SENT, GDT = 0x2000000, 0x3000000, 0x4000000, 0x6000000
for a, n in ((STACK, 0x100000), (FS, 0x10000), (SENT, 0x10000), (GDT, 0x1000)): uc.mem_map(a, n)
uc.mem_write(FS, struct.pack('<I', 0xffffffff)); uc.mem_write(SENT, b'\xf4' * 16)
def desc(b, l, ac, fl): return struct.pack('<HHBBBB', l & 0xffff, b & 0xffff, (b >> 16) & 0xff, ac, ((fl & 15) << 4) | ((l >> 16) & 15), (b >> 24) & 0xff)
uc.mem_write(GDT, desc(0, 0, 0, 0) + desc(0, 0xfffff, 0xfb, 0xc) + desc(0, 0xfffff, 0xf3, 0xc) + desc(FS, 0xfff, 0xf3, 4) + desc(0, 0xfffff, 0x9b, 0xc) + desc(0, 0xfffff, 0x93, 0xc))
uc.reg_write(UC_X86_REG_GDTR, (0, GDT, 0x30, 0))
for r, v in ((UC_X86_REG_SS, 40), (UC_X86_REG_DS, 40), (UC_X86_REG_ES, 40), (UC_X86_REG_CS, 32), (UC_X86_REG_FS, 24)): uc.reg_write(r, v)
uc.mem_write(SENT + 0x100, b'\xd9\x2d' + struct.pack('<I', SENT + 0x200) + b'\xf4'); uc.mem_write(SENT + 0x200, struct.pack('<H', 0x27f))
uc.reg_write(UC_X86_REG_ESP, STACK + 0x80000); uc.emu_start(SENT + 0x100, SENT + 0x106)   # fldcw 0x27F
atexit, rnd, hold = [], [0], [1]
def ret_cdecl(uc, eax):
    esp = uc.reg_read(UC_X86_REG_ESP); ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
    uc.reg_write(UC_X86_REG_EAX, eax); uc.reg_write(UC_X86_REG_ESP, esp + 4); uc.reg_write(UC_X86_REG_EIP, ret)
def h_atexit(uc, a, s, u):
    esp = uc.reg_read(UC_X86_REG_ESP); atexit.append(struct.unpack('<I', uc.mem_read(esp + 4, 4))[0]); ret_cdecl(uc, 0)
def h_rand(uc, a, s, u):
    hold[0] = (hold[0] * 0x343fd + 0x269ec3) & 0xffffffff; rnd[0] += 1; ret_cdecl(uc, (hold[0] >> 16) & 0x7fff)
uc.hook_add(UC_HOOK_CODE, h_atexit, begin=0x821d1e, end=0x821d1f)   # atexit
uc.hook_add(UC_HOOK_CODE, h_rand, begin=0x821b1e, end=0x821b1f)     # rand
bad = []
uc.hook_add(UC_HOOK_MEM_FETCH_UNMAPPED | UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED, lambda uc, ac, a, s, v, u: bad.append((ac, a)) or False)
for ptr in range(XC_A, XC_Z, 4):
    fn = struct.unpack('<I', uc.mem_read(ptr, 4))[0]
    if fn in (0, 0x8339ca): continue                                 # NULL pad, __security_init_cookie
    esp = STACK + 0x80000; uc.reg_write(UC_X86_REG_ESP, esp - 4); uc.mem_write(esp - 4, struct.pack('<I', SENT))
    uc.emu_start(fn, SENT, count=50000000)
    assert uc.reg_read(UC_X86_REG_EIP) == SENT and not bad, (hex(ptr), hex(fn), bad)
fin = bytes(uc.mem_read(BASE, END - BASE)); out = []; i = 0x8a4000 - BASE
while i < len(fin):
    if fin[i] != init[i]:
        j = i
        while j < len(fin) and (fin[j] != init[j] or any(fin[k] != init[k] for k in range(j, min(j + 16, len(fin))))): j += 1
        out.append([i + BASE, fin[i:j].hex()]); i = j
    else: i += 1
json.dump({'ranges': out, 'atexit': atexit, 'rand_calls': rnd[0]}, open(sys.argv[2], 'w'))
print(len(out), 'ranges,', sum(len(h) // 2 for _, h in out), 'bytes,', len(atexit), 'atexit, rand', rnd[0])

```
