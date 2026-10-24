# R-6 Win32 no-op proof: A = 850907e3 (before), B = 6d26c5e5 (after), TU = source/game_sa/Pipelines/CustomBuilding/CustomBuildingDNPipeline.cpp (main target gta_reversed)
Method = R-5 harness replayed (scripts copied from r5_ab, paths adapted; r5_ab untouched; repo working tree untouched, only read via git archive). git archive A,B -> /private/tmp/claude-501/r6_ab/{A,B}; SAME vendor/librw tree in both (30785b1, B's pin, `diff -r` = empty); conan generators + ConanPresets.json copied from r5_ab. Compiles under `taskpolicy -b nice -n 15`.
`git diff --name-only A B` outside source/portable, tests/portable, tools/portable, vendor/librw = exactly 1 file: CustomBuildingDNPipeline.cpp (+10 lines: `#ifndef NOTSA_PORTABLE` / `#else` ... `#endif` in AtomicHasNVCPipeline). source/CMakeLists.txt and CMakePresets.json are unchanged A..B.

| step | check | A | B | verdict |
|---|---|---|---|---|
| 2 | `cmake --preset StandaloneRelease` rc | 0 | 0 | ok |
| 2 | TUs in compile_commands.json | 1281 | 1281 (only-A 0 / only-B 0) | identical set |
| 2 | compile commands (paths normalised) | 4712 | 4712 | 4712/4712 identical |
| 2 | build.ninja (normalised) | 20,603,437 B | 20,603,437 B | identical |
| 2 | source/portable, texture_gl3, rw_texture_gl, gl_smoke, run_txd in ninja/compdb; NOTSA_PORTABLE in compdb | 0 | 0 | invisible to Win32 |
| 3 | wine cl.exe CustomBuildingDNPipeline.cpp: code+relocs | 46899 B, 12604 relocs | same | identical |
| 3 | non-debug sections (.text/.rdata/.data/.bss/.xdata/.pdata sizes+bytes+relocs+flags) | 1394 sections | 1394, 0 differ | identical (sizes 46899/3889/126277/116268/5428/0) |
| 3 | symbols (name,value,section,class) multiset | 4107 | 4107 | identical |
| 3 | section order | | | identical (0 positions differ) |
| 3 | byte-level code section diff (bytecmp) | | | none |
| 3 | /P preprocessed text (no #line, blank lines ignored) | | | A-only 0 / B-only 0 non-blank lines |
| 4 | w0a_guardlines.py check FILE --base A (run in a scratch git repo holding A's file as HEAD and B's file as worktree; script byte-identical to checkout's) | | | TEXT OK (rc 0) |

## Neutral differences
* Debug info only: .debug$S same size 1150560 B (35 differing subsections = shifted line numbers), .chks64 same size; .debug$T same size 4013300 B and 61597 records each, only 3 LF_UDT_SRC_LINE records differ (line 134/169/275 -> 144/179/285, i.e. +10 shift from the guard lines). Preprocessed output differs only in #line directives/blank lines. No __LINE__-dependent code/data difference (symbols and .rdata/.data bytes identical).

## Real differences
None.

## Verdict
Windows build unchanged: yes (configure output identical; the TU is bit-identical in code, relocations, all non-debug sections, symbols and section order).
Limits: no full link/ninja build; other 1280 TUs covered by identical compile commands + git diff showing no other Win32-visible file. Only the `gta_reversed` target variant of the TU compiled (3 test-target variants rw_custom_pipelines_test / wave3_*_oracle_test have their own commands, identical A vs B, not recompiled).
