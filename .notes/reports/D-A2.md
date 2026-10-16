# D-A2 report: NOTSA_GLOBAL macro family, CMake option, codemod, verifier, pilot (slice A2 of DETACH_DATA_PLAN)

Commits (branch reverse/interior_c): `f1b92dac` Base.h macros + GlobalsVerify.h (alone) | `f96e90f2` Base.h aliases.json forms (alone) | `e4fd8982` CMake options / preset / boot dump /
codemod / emitter / verifier / obj_code_sig | `603da36d` verifier by linker map | `3aa8c2df` pilot conversions. (docs/STANDALONE.md paragraph was swept into another agent's commit 4e94cc64.)

## What exists now
| piece | where | notes |
|---|---|---|
| macro family | `source/Base.h` (after `ScopedStaticRef`) | `NOTSA_GLOBAL`, `_HDR` (header namespace scope: ONE `inline` definition), `_LOCAL`, `_DECL`/`_DEF` (class member declared in the header, value in the .cpp: function-pointer tables), `_ALIAS`, `_EXPR`, `_SYNTH`, `NOTSA_SCOPED_GLOBAL`, `_LOCAL_REF`, `_VERIFY`. Modes: ADDRESS (default, = the old `auto& x = StaticRef<T>(addr)` text), DETACHED (`NOTSA_DETACHED_GLOBALS`: real `std::type_identity_t<T> x init`), ORACLE (`NOTSA_ORACLE_ADDRESS_GLOBALS` forces ADDRESS inside a detached configuration). |
| option | `source/CMakeLists.txt`: `GTASA_DETACHED_GLOBALS` (OFF), `GTASA_VERIFY_GLOBALS` (OFF), both need the run build; defines only on the exe target; `world_oracle_test` / `logic_oracle_test` (they copy the exe target's definitions) get `NOTSA_ORACLE_ADDRESS_GLOBALS`. Preset `StandaloneDetached` (build/StandaloneDetached). |
| boot dump | `source/standalone/GlobalsVerify.{h,cpp}`; `NOTSA_VERIFY_GLOBALS_BOOT()` (Fixups.h, one line in WinMain after `ApplyToDataImage`, expands to `((void)0)` unless VERIFY). Env: `NOTSA_VERIFY_GLOBALS=<file>`, `_EXIT=1`, `_AFTER=<s>` (late snapshot). |
| emitter | `tools/standalone/globals_emit.py` (gen_globals step 4): bit-exact literals from the image: floats shortest round-trip / `std::bit_cast`, ints (hex for hashes), bool, enum, pointers (code pointers -> `&Cls::Fn` from the `RH_Scoped*Install` lines and `0xADDR, Cls::Fn` tables, else NEEDS-MANUAL), C arrays / std::array (zero tail dropped), char text, opaque <= 8 B via `std::bit_cast<T>`; all-zero -> `{}` |
| codemod | `tools/standalone/codemod_globals.py` | dry run by default; consumes DETACH_GLOBALS.tsv, the image and `.notes/aliases.json` (aliases / members / synthetics / ScopedStaticRef / per-site manual list); every edit that changes the line count is followed by `#line N` (so `__LINE__` in RH_*/NOTSA_UNREACHABLE sites never moves); DEF blocks go to the END of the .cpp; `--check`, `--only-addr`, `--skip-addr`, `--gen-shared` (DetachedShared.h) |
| verifier | `tools/standalone/verify_globals.py` + `standalone_deviations.tsv` | image bytes vs dump; code pointers by linker-map name == the repo's own hook name for that address; deviations file; cross-check size vs table; `--cover` lists unconverted rows |
| object-code proof | `tools/standalone/obj_code_sig.py` | per-.obj fingerprint of the code sections (bytes + relocation names) for `sig` / `cmp` |

## Acceptance
| item | result |
|---|---|
| option ON (`build/StandaloneDetached`, VERIFY on): `verify_globals.py` | 98 globals / 5,048 B: 97 identical, 65 code-pointer dwords (CCheat::m_aCheatFunctions) named by the linker map as the repo's hook functions, **0 mismatches, 0 cross-check failures**; mutation test (flipped float, two swapped function pointers) -> 2 MISMATCH lines, FAIL |
| option ON boot | exe reaches the main menu (`NOTSA_STANDALONE_SKIP_VIDEOS=1`, screenshot frame_5, Wine; no trap/assert); same screenshot as the OFF exe |
| option OFF: object code | 1,119 objects of build/StandaloneRelease compared before/after (all of them rebuilt twice with the new PCH): the pilot TUs Timer.cpp, Hud.cpp, Cheat.cpp, Pickups.cpp, Bike.cpp, WinMain.cpp are code-identical (`obj_code_sig.py cmp`: 0 differences); Pickups and WinMain additionally by an in-place A/B (HEAD source vs working tree compiled under one temporary name: identical). 1,072 objects identical, 47 differ: all 47 are TUs that include files other agents changed in the meantime (FixedFloat/FixedVector reciprocal commit, Game.h/GameLogic.h, Coronas, Occlusion, ... ; 46 verified by include closure, `standalone/rw/hanim.cpp` depends on the PCH-implied FixedQuat commit) |
| option OFF: tests | NOT a full `run_all_tests.sh` run (35 tests = hours under Wine). Ran `run_all_tests.sh <dir> 'logic_oracle|x87_oracle|world_oracle'` (the tests that compile Timer/Hud/Cheat/Pickups) in BOTH `build/StandaloneRelease` (OFF) and `build/StandaloneDetached` (ON, tests in ORACLE address mode): identical rows: world_oracle ok (114 checks), logic_oracle FAIL 224 checks / 800 PC24 mismatches (ChooseCarModelToLoad, GetDefaultCopCarModel, ... = another agent's pop-area work in progress, none touches the pilot globals), x87_oracle NORESULT (runs, `67 rows ... PC24 2/2`: a result-line format the script's parser does not know). So option ON does not change a test result and OFF is code-identical; no pre-change baseline row exists for these three (I did not run one before editing) |
| ASI/DLL config | `build/Debug` (GTASA_STANDALONE=OFF): PCH + Timer/Hud/Cheat/Pickups/Bike compile (0 errors). DUMP_HOOKS_ONLY config not built (same address-mode text) |

## Pilot (98 globals in the dump)
Timer.h 25 + Hud.h 67 + Cheat.h 5 (m_aCheatFunctions 65 non-null pointers, m_aCheatHashKeys 92, m_CheatString, m_aCheatsActive, m_bHasPlayerCheated) = 97 table rows + the Pickups colour table
(raw cast `0x8A5FB0`, 54 entries x 8 B, `constexpr kPickupColours` + `NOTSA_GLOBAL_VERIFY` + `NOTSA_GLOBAL_LOCAL_REF`; in the old code the extractor classified those packed RGB dwords as code
pointers and trap-stubbed them, TRAP_READERS #3: gone in detached mode). Not converted: `CHud::Sprites` (category d, phase C). The plan said 95; the table has 98 rows for these files.
`m_aCheatsActive` overlaps `s_BikeHandlingCheat` (0x96914C, Bike.cpp:79 = element 28): converted together with that one alias (`NOTSA_GLOBAL_ALIAS(..., *reinterpret_cast<uint8*>(reinterpret_cast<uint8*>(&CCheat::m_aCheatsActive) + 28))`
and `#include "game_sa/Cheat.h"`; Bike.cpp's other 4 globals untouched).

## Findings
* MSVC emits a dynamic initialiser for every namespace-scope `auto& x = StaticRef<..>(addr)` and the extra `StaticRef` call even changed the inlining of `CPickups::DoPickUpEffects`: the read-only table is therefore accessed through a raw-cast POINTER in address mode (`NOTSA_GLOBAL_LOCAL_REF`), then the TU is code-identical.
* Inserting lines changes object code (`__LINE__` in RH_Scoped* / NOTSA_UNREACHABLE): all conversions are line-preserving; the codemod adds `#line N` after any edit that changes the line count and appends DEF blocks at the end of the .cpp.
* `Cheat.cpp:InjectHooks` uses `RH_ScopedNamedGlobalInstall(cheat.method, ...)` = `&cheat.method` (address of a `void*` field, not the function): the standalone fixup for the 65 cheat functions registers garbage; it only worked because the same loop overwrites `m_aCheatFunctions` entries. In detached mode the table is initialised with the real functions, so the loop is a no-op. Not fixed (not mine).
* `DETACH_GLOBALS.tsv` is generated from `StaticRef` text: after conversion the pilot files vanish from a re-scan (D-A4 saw the warning). The scanner must learn the `NOTSA_GLOBAL*` forms (or keep the committed table as the input) before A1 is re-run on a converted tree. `.notes/DETACH_overlaps.tsv` is stale for CGameLogic (FIXADDR fixed GameState/ActivePlayers): codemod still skips them as "undecided" until the table is regenerated; `new_globals` (e.g. 0xB6B99C) are not rows and are not handled by the codemod.

## Codemod dry run over the whole tree (`StaticRef<` in 366 files, not applied)
GLOBAL 2,024 | HDR 106 | LOCAL 197 | DECL+DEF 2 | ALIAS 26 | MEMBER 26 | EXPR 26 | SCOPED 2 | SYNTH-TU 4 | INCLUDE 25 (owner headers / DetachedShared.h) | skipped 257: category d 120, ctor NONZERO 39, dead in run 34,
overlap undecided 9, manual initialiser 17 (CVector / CRGBA / RGBAf members with non-zero values), private-access aliases 3, views / virtual / retype (0xC4D958 arena) / readdress ~5, non-plain declarations 3.
`--gen-shared` output covers the 10 shared synthetics (NoNode / NextDesiredMoveState via bit_cast of the opaque value).

## Not done / open
* Full 35-test `run_all_tests.sh` baseline + after (see above), DUMP_HOOKS_ONLY configure.
* The 0xC4D958 arena (one union), `views`, `virtual`, owners with link/init_in_source/grow stay manual by design (reported by the codemod).
* `NOTSA_VERIFY_GLOBALS` does not cover function-local `NOTSA_GLOBAL_LOCAL` / `NOTSA_SCOPED_GLOBAL` statics (they cannot register at static-init time).
* Build artefacts: `build/StandaloneDetached` (new, ~1,100 objects) stays; scratch in the session scratchpad dA2-64644.
