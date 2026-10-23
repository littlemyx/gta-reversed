# MAC-P2b — P-2 scalar fixes (slice P-2b of .notes/P2_PLAN.md)

Host: Apple Silicon Mac. Rules `.notes/agent_rules/v4.md` (Windows-only parts do not apply). Repo
/Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main` (remote `fork`). Main checkout. Agent MAC-W0 works in parallel
(owns ALL CMake files/presets, source/app/**, oswrapper, Audio/Hardware, CdStream/FileMgr platform parts, source/portable/** existing
files, tools/portable/** except P-2 tools). `.notes/` is gitignored (`git add -f`). Wall-clock cap 100 min.

## Spec
`.notes/P2_PLAN.md` sections 2, 3 (rows of the structs below), 4 row **P-2b** + the common acceptance above the table, 6.
Infra from P-2a: `.notes/reports/MAC-P2a.md` (FileRecord.hpp, guard, run_file_records_test.sh, layout_diff.py, wine A/B method).
Scope: the 5 size_t prefixes (GangWars.cpp:75, PlayerInfo.cpp:1465, PlayerPed.cpp:162, Ped.cpp:545, Object.cpp:610), the 4 GetIndex
writes TheScripts.cpp:1299/1323/1327/1331, **plus IplStore.cpp:801** (writes size_t, loader at 813 reads int32); tScriptParam union
cleanup; SYSTEMTIME + CSimpleVariablesSaveStructure (0x138 asserted on arm64); ExtraVertColour reclassify + portable writer;
HeapBlockDesc guard.

## Rules specific to this slice
- MSVC code bytes must stay identical. A prefix written as `size_t` on x86 is 4 bytes; the fix must produce the same x86 code (e.g.
  `uint32` typed write is the same instructions on x86 — PROVE it with wine A/B on every touched TU; if a TU's code changes, guard the
  change with NOTSA_PORTABLE instead).
- SYSTEMTIME: you may create ONE new file `source/portable/systemtime.h` (16-byte struct, exactly the Win32 layout) and include it
  where the save structure needs it under `!_WIN32`; don't edit MAC-W0's existing files in source/portable.
- After the slice the P-2a guard report must be empty (warn mode, 0 hits).
- Scripts files are watched by the Windows host: `git fetch fork && git merge fork/reverse/interior_c` is NOT yours (orchestrator does
  merges); just keep your TheScripts.cpp edits to the listed lines.

## Zones / git
Yours: the files named above (only the listed lines/structs), their headers, tests/portable/** (add cases), the new systemtime.h.
Commit only your files (`git commit -- <files>`, trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`), push after each
(pull --no-rebase on reject; never force; never push reverse/interior_c). ≤ 1 own sub-agent. Builds/census background, low priority.

## Report
`.notes/reports/MAC-P2b.md`; return ≤ 15 lines: commits, guard hits before/after, sizes asserted, wine A/B table summary, open items.
