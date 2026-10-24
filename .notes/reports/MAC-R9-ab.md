# R-9 Win32 no-op proof: A = 8562ac03 (before), B = fd05fe6f (after), 3 TUs (target gta_reversed)
TUs: source/game_sa/Pipelines/CustomCarEnvMap/CustomCarEnvMapPipeline.cpp, source/game_sa/Pipelines/CustomBuilding/CustomBuildingPipeline.cpp, source/game_sa/Pipelines/CustomBuilding/CustomBuildingDNPipeline.cpp.
Method = MAC-R6-ab.md replayed (scripts copied from /private/tmp/claude-501/r6_ab to /private/tmp/claude-501/r9_ab, paths/commits adapted, r6_ab untouched; repo working tree untouched, only read via `git archive`). git archive A,B -> r9_ab/{A,B}; vendor/librw (submodule, pin 30785b1 in both A and B per `git ls-tree`) copied from r6_ab/A (byte-identical to the r6 tree; `diff -r` A vs B = empty); conan generators (build/Release, build/Debug) + ConanPresets.json copied from r6_ab/A. Compiles under `taskpolicy -b nice -n 15`, one TU at a time, each preceded by a wait-for-idle on the RR3 wine/ninja work (it was idle every time, no wait needed).

`git diff --name-only A B` outside source/portable, tests/portable, tools/portable, vendor/librw, .notes = exactly the 3 expected files (list does NOT differ). Other changed files: source/portable/render/{pipelines_carenv_gl3.cpp,pipelines_carenv_gl3.h,pipelines_saenv_game_gl3.cpp}, tests/portable/rw_pipeline_gl_test.cpp, tools/portable/{run_pipe_test.sh,play1/r9.txt,play1/r9n.txt} (all portable-only). source/CMakeLists.txt and CMakePresets.json unchanged A..B (so the portable files cannot enter the Win32 target list).
Text change in the 3 files (+10 / -3 lines in total): CustomBuildingDNPipeline.cpp:189-193 and CustomBuildingPipeline.cpp:26-30 only the `#else` branch of `#ifndef NOTSA_PORTABLE ... #else ... #endif` in CreatePipe (an `extern` decl + call + `return true`, replacing a one-line `return true;`); CustomCarEnvMapPipeline.cpp:108-111 the former bare `#endif // comment` became `#else <extern decl + call> #endif // comment`. The `#ifndef` branch (what MSVC sees) is untouched.

| step | check | A | B | verdict |
|---|---|---|---|---|
| 2 | `cmake --preset StandaloneRelease` rc | 0 | 0 | ok |
| 2 | TUs in compile_commands.json | 1281 | 1281 (only-A 0 / only-B 0) | identical set |
| 2 | compile commands (paths normalised) | 4712 | 4712 | 4712/4712 identical |
| 2 | build.ninja (normalised) | 20,603,437 B | 20,603,437 B | identical |
| 2 | source/portable, pipelines_carenv, pipelines_saenv, texture_gl3, rw_texture_gl, gl_smoke, run_txd in ninja; same + NOTSA_PORTABLE in compdb | 0 / 0 | 0 / 0 | invisible to Win32 |
| 3 | wine cl.exe, CustomBuildingDNPipeline.cpp: code+relocs | 46899 B, 12604 relocs | same | identical |
| 3 | wine cl.exe, CustomBuildingPipeline.cpp: code+relocs | 44217 B, 11695 relocs | same | identical |
| 3 | wine cl.exe, CustomCarEnvMapPipeline.cpp: code+relocs | 65657 B, 19418 relocs | same | identical |
| 3 | non-debug sections (.text/.rdata/.data/.bss/.xdata/.pdata sizes+bytes+relocs+flags), DN / Building / CarEnv | 1394 / 1315 / 2086 sections | same counts, 0 differ in all three | identical (e.g. DN .text/.rdata/.data/.bss/.xdata = 46899/3889/126277/116268/5428) |
| 3 | symbols (name,value,section,class) multiset + reloc-target multiset | 4107 / 3853 / 6202 | same | identical |
| 3 | section order | | | identical (0 positions differ, all 3) |
| 3 | byte-level code section diff (bytecmp) | | | none (no output, all 3) |
| 3 | /P preprocessed text (#line and blank lines ignored; multiset and sequence) | 259318 / 259135 / 259348 non-blank lines | same | A-only 0 / B-only 0, sequence identical |
| 4 | w0a_guardlines.py check FILE --base HEAD (scratch git repo with A's file committed, B's file in worktree; script byte-identical to checkout and to B) | | | TEXT OK, rc 0, for all 3 files |

## Neutral differences
* Debug info only: .debug$S same size in all three (DN 1150560, Building 1114888, CarEnv 1693232 B; 27 / 13 / 19 differing subsections = shifted line numbers, net size delta 0), .chks64 same size.
* .debug$T same size and same record count in all three; differing records are only LF_UDT_SRC_LINE (source line of a struct definition): DN 1 record (285 -> 287), Building 0, CarEnv 2 records (307 -> 310, 376 -> 379), i.e. the +2 / +2 / +3 line shift from the added `#else` text.
* Raw /P output differs only in `#line N "file"` directives (6 / 6 / 9 diff lines, line numbers shifted by +2 / +2 / +3) and blank lines. No `__LINE__`-dependent code or data: symbols, .rdata/.data/.text bytes identical (classified neutral per the 2026-10-23 decision).

## Real differences
None.

## Verdict
Windows build unchanged: yes. Configure output identical; each of the 3 touched TUs is bit-identical in code, relocations, all non-debug sections, symbols and section order. Only debug line info (+2/+2/+3 shift) differs.

Limits: no full link/ninja build; the other 1278 TUs of the main target are covered by identical compile commands + `git diff --name-only` showing no other Win32-visible file. Only the `gta_reversed` target variant of each TU compiled (test-target variants have their own compile commands, identical A vs B, not recompiled). The wine wait-for-idle used a precise process check (real wine-msvc.sh / cl.exe / ninja processes) instead of a literal `pgrep -f`, because other agents' polling shells contain the strings "ninja ... StandaloneReleaseRR" in their command lines and would match forever; no RR3 work was running at any compile start. Scratch: /private/tmp/claude-501/r9_ab (scripts, objs sa/ sb/, logs); no background processes left.
