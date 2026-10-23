# MAC-P2a — P-2 infrastructure: FileRecord trait, 64-bit scalar guard, layout_diff gate

Host: Apple Silicon Mac. Rules `.notes/agent_rules/v4.md` (Windows-only parts do not apply). Repo
/Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main` (remote `fork`). Main checkout. Agent MAC-W0 works in parallel
(zones: source/app/**, oswrapper, Audio/Hardware, CdStream/FileMgr platform parts, source/portable/**, ALL CMake files and presets,
conan profile, tools/portable/** EXCEPT the files named below). `.notes/` is gitignored (`git add -f`). Wall-clock cap 100 min.

## Spec
`.notes/P2_PLAN.md` sections 0, 2 (2.1 trait, 2.2 layout_diff, 2.3 assert policy), 4 row **P-2a**, 6. Background: .notes/reports/MAC-P1.md,
.notes/reports/MAC-P2-DESIGN.md, TSV .notes/reports/MAC-P1-file-structs.tsv. Implement P-2a exactly as the plan says, with this change:
do NOT edit CMake (MAC-W0 owns it) — build and run `tests/portable/file_records_test.cpp` with a small script
`tools/portable/run_file_records_test.sh` (direct clang++ invocation with the compile flags taken from build/PortableMac/compile_commands.json);
CMake wiring is a later one-liner, list it as an open item.

## Acceptance
- Trait + typed Save/Load dispatch only under NOTSA_PORTABLE; MSVC raw path unchanged: StandaloneRelease compile commands identical and
  wine MSVC A/B (code + relocs) identical on 5 TUs that include the touched headers (script/method: .notes/reports/MAC-A1b.md).
- The 64-bit scalar guard fires (compile-time) on `long`/`size_t`/pointer in the portable typed Save/Load — prove with a negative test
  (a TU that must fail to compile) — and lists today's offenders (expected: the 5 size_t prefixes + TheScripts.cpp 1299/1323/1327/1331);
  do not fix them here (slices b..k), but the portable build must still be able to proceed: offenders are listed, not silenced.
  If the guard would break the portable census today, make it a `[[deprecated]]`-style warning in portable with the list, and say so.
- `layout_diff.py` runs over all 131 TSV rows (i686-pc-windows-msvc vs arm64-apple-macos) and writes `.notes/reports/MAC-P2a-layout.md`:
  per struct size/offset/bit-field differences; rows that differ beyond the known 26 are new findings (headline them).
- Test harness green.

## Zones
Yours: source/game_sa/FileRecord.hpp (new), the typed Save/Load template header(s) the plan names (GenericGameStorage or wherever they
are — confirm none is in MAC-W0's zone; if one is, stop and report), RwStreamRead<T> / File::Read<T> template headers named in the plan,
tools/portable/layout_diff.py, tools/portable/gen_x86_golden.py, tools/portable/run_file_records_test.sh, tests/portable/file_records_test.cpp.
Commit only your files (`git commit -- <files>`, trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`), push after each
(pull --no-rebase on reject; never force; never push reverse/interior_c). ≤ 1 own sub-agent. Background + low priority + timeout.

## Report
`.notes/reports/MAC-P2a.md`; return ≤ 15 lines: commits, guard offenders, layout_diff headline, no-op result, open items.
