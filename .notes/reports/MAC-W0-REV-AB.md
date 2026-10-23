# MAC-W0-REV-AB: wine MSVC A/B no-op proof, A = 3f46d831~1 (8fcde553) vs B = 7203bc53

Scratch: /private/tmp/claude-501/w0_rev_ab (trees A/ B/, objs sa/ sb/, scripts b3.py cmp3.py keyed.py keycmp.py cls.py txt.py pp3.py pp4.py symcmp.py, disassembly dis/). Repo working tree not touched, nothing committed.
Method as in MAC-W0b: `git archive` trees + rsync of vendor/librw, source/libs/imgui, build/{Release,Debug}/generators, ConanPresets.json; `cmake --preset StandaloneRelease` under msvc-wine;
cl.exe under wine with /Yu /Fp /FI /Fd /Fo /MP /FS dropped, `/FI source/StdInc.cpp` added; variant `main` (gta_reversed.dir); same-path symlink `cur -> A|B`; outputs in equal-length dirs sa / sb; 2 parallel jobs. Each side used its OWN compile_commands.json.

## 1. Configure (A vs B, paths normalised)
| item | A | B | result |
|---|---|---|---|
| TUs in compile_commands.json | 1284 | 1284 | same set, 0 only-in-one |
| compile command entries | 4670 | 4670 | 4670/4670 identical |
| build.ninja (20,409,947 B) | | | identical after path normalisation |
(source/CMakeLists.txt differs A vs B, but only in `GTASA_PORTABLE`-guarded blocks and the `else()` glob that removes source/portable/*.cpp, which yields the same Windows source list.)

## 2/3. Per-TU results (11 TUs, all compiled rc=0 on both sides)
Code = sum of code-section bytes (obj_code_sig); "strict identical" = obj_code_sig code+relocation signature equal (name-sorted COMDATs).
Independent check (keyed.py): every section keyed by (name, COMDAT symbol / associated section) compared on raw bytes + relocation list (offset, target name, type) + flags: 0 sections only-in-one.

| TU | code bytes | code identical (strict) | differences classified | reloc identical | .text/.rdata/.data/.bss/.xdata sizes identical | .debug$T identical |
|---|---|---|---|---|---|---|
| Audio/AESmoothFadeThread | 51389 | yes | 0 | yes | yes | no (-40 B, see below) |
| Audio/AEStreamThread | 51100 | yes | 0 | yes | yes | no (-40 B) |
| Audio/Hardware/AEAudioHardware | 60641 | yes | 0 | yes | yes | no (-40 B) |
| Audio/AESound | 45695 | yes | 0 | yes | yes | no (-40 B) |
| Audio/Managers/AESoundManager | 58410 | yes | 0 | yes | yes | no (-40 B) |
| Audio/Managers/AEUserRadioTrackManager | 78259 | NO | 3 `__LINE__` imm, 0 OTHER | yes (lists equal in all sections) | yes | no (-40 B) |
| game_sa/FileMgr | 68111 | yes | 0 | yes | yes | no (-40 B) |
| app/platform/win/WinPs | 143992 | NO | 16 `__LINE__` imm, 0 OTHER | yes | yes | no (-40 B) |
| app/platform/win/WinMain | 163018 | NO | 5 `__LINE__` imm, 0 OTHER | yes | yes | no (-40 B) |
| app/platform/win/WinPlatform | 43313 | yes | 0 | yes | yes | no (-40 B) |
| app/app_debug | 162080 | NO | 25 `__LINE__` imm, 0 OTHER | yes | yes | no (-40 B) |

Totals: 7/11 TUs byte-identical code; 4/11 differ, with 49 differing instructions = 49 `__LINE__` immediates, 0 OTHER. Same code sizes, same section sizes, same relocation target multiset (e.g. WinMain 40605 relocs), same symbol multiset (name, value, section, class), same symbol-name set in all 11.
Byte-level cross-check on the 4 differing TUs (bytecmp.py): the differing bytes are only inside 1/5/5/3 `.text$mn` sections, as 3/5/16/25 contiguous runs (one per immediate), sections of equal size, relocation lists equal.

### `__LINE__` immediates (each verified: line text identical in A and B, and B-line = A-line mapped through a difflib alignment of the changed file)
* AEUserRadioTrackManager (3): AEUserRadioTrackManager.cpp 380->397 NOTSA_UNREACHABLE, 384->401 NOTSA_LOG_ERR, 394->411 NOTSA_UNREACHABLE.
* WinMain (5 instrs, 3 source lines): WinMain.cpp 238->250 `default: NOTSA_UNREACHABLE()` (2 instrs: lambda in ProcessGameLogic + its `$LN15` funclet), 419->442 NOTSA_UNREACHABLE(SDL init); extensions/debug.hpp 19->21 NOTSA_LOG_INFO in `notsa::debug::WaitForDebugger` (2 instrs: the function and its inlined copy in `_WinMain@16`).
* WinPs (16 instrs, 15 source lines): WinPs.cpp 134->158, 153->180 (x2), 426->503, 437->515, 441->519, 488->566, 554->640, 579->665, 589->675, 593->679, 594->680, 600->686, 619->705, 623->709, 634->720 (all NOTSA_LOG_DEBUG / NOTSA_UNREACHABLE).
* app_debug (25 instrs, 9 source lines): app_debug.cpp 76->81 (x7), 81->86, 82->87, 83->88, 88->93, 90->95, 99->104 (x8), 148->153, 172->177 (all SPDLOG_INFO / NOTSA_LOG_INFO in WindowsExceptionHandler and its lambdas).
The shifts are the guard lines added by W0/W0a (the `#line N // W0a` directives were removed in 7203bc53, so these lines now move).
Preprocessed cross-check (`cl /P` of all 11 TUs, A vs B): after deleting `#line` and blank lines, the digit-normalised multiset of lines is identical across all 11 TUs
(A-only 6 lines, B-only 48 lines, the same set in every TU): i.e. the only preprocessed differences are (a) one header-level block common to all TUs and (b) literal line numbers
(`source_loc{"...", N,` / `UnreachableHit(..., N)`), at exactly the 3/3/15/10 source lines above. Block (a) = new FileRecord.hpp (notsa::FileRecord/FileSizeOf/FileScalarCheck, inlined via GenericGameStorage.h),
`if constexpr (sizeof(void*) > 4) { notsa::FileScalarCheck<T>(); }` guards in RwStreamRead / LoadDataFromWorkBuffer / SaveDataToWorkBuffer (discarded on x86) and the removal of `void* pParam; char* szParam;` from `union tScriptParam`.

## 4. Non-code differences (informational, not blockers)
1. `.debug$T`: all 11 TUs are exactly 40 bytes smaller in B. Cause (llvm-readobj type dump of WinPlatform): `union tScriptParam` lost the members `void* pParam` and `char* szParam` (LF_MEMBER records, 20 B each; MemberCount 11 -> 9; SizeOf stays 4) from P-2b (ScriptParam.h). No other type record differs beyond the resulting type-index shifts.
2. `.debug$S`: +28..+196 B per TU (different line tables, per-function associative `.debug$S` COMDAT sizes change by multiples of 4-12 B, source-file checksum records of changed files, S_OBJNAME 1 byte). Expected from the shifted lines/guards.
3. COMDAT emission order: in AEStreamThread and AEUserRadioTrackManager one read-only inline-variable COMDAT is emitted earlier in B (`.rdata` `?nullopt@std@@3Unullopt_t@1@B`: 87 -> 75 of 1952; `.rdata` `??$_Digit_pairs@D@std@@...`: 159 -> 145 of 2674), shifting 13/15 neighbouring `.rdata` GUID COMDATs by one position. Contents, sizes, relocations identical; the symbol tables differ only by that order. The other 9 TUs have identical section and symbol order. Effect on a final link: at most a different `.rdata` COMDAT layout; no code or data content change. I did not track down which new header include triggers the order change (likely FileRecord.hpp / the changed headers touching `<optional>`/`<charconv>` instantiation order).

## Verdict
For the 11 TUs the Windows MSVC build of B is a no-op relative to A: code, relocations, symbols, data and section sizes are identical, except 49 instruction immediates that are `__LINE__` values of assert/log macros (0 OTHER). Debug info differs as expected (tScriptParam member removal, line tables); one COMDAT order swap in 2 TUs is the only non-`__LINE__` object-level difference and is content-neutral.
Cleanup: wineserver -k run; no wine/cl processes left. `cur` symlink and trees remain in /private/tmp/claude-501/w0_rev_ab.
