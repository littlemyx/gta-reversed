# MAC-W0b: no-op proof of W0/W0a commits for the Windows x86 MSVC build

Scratch: /private/tmp/claude-501/w0_ab (scripts b.py, b2.py, cmp.py, cmpobj.py, cmp2.py; obj/ and obj2/). No repo file modified, nothing committed.

## Trees
* AFTER = `git archive HEAD` (HEAD = 1f1d7a53, P-2b; it sits on top of the four commits).
* BEFORE = same, with only the files of 3f46d831, 57b5a8f9, 79cbd8c5, 550597e7 (42 paths) restored to 3f46d831~1: 19 restored, 23 removed (added by those commits).
  No path of those four commits is touched by P-2a/P-2b (empty intersection), so P-2a/P-2b (FileRecord.hpp, GenericGameStorage.h, RenderWare.h, MemoryMgr.h, ScriptParam.h, ...) are identical in both trees.
* untracked deps (vendor/librw, source/libs/imgui, build/Release/generators, ConanPresets.json) rsynced identically into both.

## Check 1: `cmake --preset StandaloneRelease` (msvc-wine, -DGTASA_PYTHON=/opt/homebrew/bin/python3)
| item | BEFORE | AFTER | result |
|---|---|---|---|
| TUs in compile_commands.json | 1284 | 1284 | same set, 0 only-in-one |
| compile command entries (PCH Yc/Yu variants, test targets) | 4670 | 4670 | 4670/4670 identical (1284/1284 TUs), paths normalised |
| build.ninja (20,409,947 bytes) | | | byte-identical after path normalisation (incl. link statement) |
Source file list does not differ: source/portable/*.cpp is only globbed when GTASA_PORTABLE=ON.

## Check 2: cl.exe under wine, StandaloneRelease flags minus PCH (/Yu /Fp /FI /Fd /Fo /MP /FS dropped, `/FI source/StdInc.cpp` added), main target (gta_reversed.dir) variant, same-path symlink swap (`cur` -> before|after)
obj_code_sig.py (code bytes + relocation names; also with `--data`: .data/.bss/.rdata hashes):
| TU | code bytes | code+reloc | +data | .debug$T |
|---|---|---|---|---|
| Audio/AESmoothFadeThread | 51389 | identical | identical | identical |
| Audio/AEStreamThread | 51100 | identical | identical | identical |
| Audio/Hardware/AEAudioHardware | 60641 | identical | identical | identical |
| Audio/AESound | 45695 | identical | identical | identical |
| Audio/Managers/AESoundManager | 58410 | identical | identical | identical |
| Audio/Managers/AEUserRadioTrackManager | 78259 | identical | identical | identical |
| game_sa/FileMgr | 68111 | identical | identical | identical |
| app/platform/win/WinPs | 143992 | identical | identical | identical |
| app/platform/win/WinMain | 163018 (= A1b value) | identical | identical | identical |
| app/platform/win/WinPlatform | 43313 | identical | identical | identical |
| app/app_debug | 162080 | identical | identical | identical |
11/11 identical.

### Debug info (informational)
* First run (objects named before_X.obj / after_X.obj): .debug$S differs; same size except WinPs/app_debug (-4 B) - that is the S_OBJNAME string ("before"/"after" length).
* Second run, equal-length output dirs (obj2/sb vs obj2/sa): all 11 .debug$S have the SAME size, and differ in only 64..97 bytes: 1 byte of S_OBJNAME (offset 0x3a) plus 2-3 x 32-byte
  source-file checksum records (+ the matching .chks64 section hash) = the changed source files themselves (the TU .cpp and winincl.h / debug.hpp etc. in its include closure).
  No line-table / symbol record differs. .debug$T is byte-identical (0 differing bytes) in all 11.
* Preprocessed output (`cl /P`, cur paths normalised): after removing `#line` lines and blank lines the token stream is identical for all 11 TUs (0 diff lines). The raw diff is only extra
  blank lines + re-synchronised `#line` markers (e.g. winincl.h gains 2 lines: `#line 52` -> blank,blank,`#line 54`), so __LINE__ values are unchanged (the W0a `#line N // W0a` guard lines work).

## Not covered
Test-target variants of the Audio/WinPs TUs (W2_ONLY/W3_* defines, tests/ targets) were not compiled; their command lines are identical BEFORE/AFTER (Check 1) and they share the same sources.
