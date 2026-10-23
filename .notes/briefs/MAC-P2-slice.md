# MAC-P2<x> — one P-2 slice (generic brief; the slice letter is given in the launch prompt)

Host: Apple Silicon Mac. Rules `.notes/agent_rules/v4.md` (Windows-only parts do not apply). Repo
/Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main` (remote `fork`). Main checkout, shared with ONE parallel agent
named in the launch prompt — never touch its files or uncommitted edits. `.notes/` is gitignored (`git add -f`). Wall-clock cap 100 min.

## Spec
`.notes/P2_PLAN.md`: section 2 (trait, layout_diff, assert policy), section 3 rows of your structs, section 4 row of your slice + the
COMMON ACCEPTANCE above the table (native census on touched TUs: your `_FILE` diagnostics gone, 0 new; msvc-mode census clean; host
test green; wine cl.exe A/B code+relocs identical on every touched TU — `__LINE__`-only differences are neutral, classify them), section 6.
Infra and methods: .notes/reports/MAC-P2a.md (FileRecord.hpp, guard, layout_diff.py, gen_x86_golden.py, run_file_records_test.sh),
.notes/reports/MAC-P2b.md (worked example of a slice incl. wine A/B and host test script).

## Rules
- MSVC raw path untouched: `X_File` + `FileRecord<X>` specialisation used only under NOTSA_PORTABLE, specialised in your struct's own
  header (not in FileRecord.hpp / GenericGameStorage.h — those are P-2a's; if you need a change there, make it minimal and say so).
- Consumer audit (rules v4): every reader/writer of the struct's file bytes is listed in the report and goes through the record.
- Don't edit CMake (tests run through a tools/portable/run_p2<x>_test.sh script like P-2b's).
- Commit only your files (`git commit -- <files>`, trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`), push after each
  (pull --no-rebase on reject; never force; never push reverse/interior_c). ≤ 1 own sub-agent. Builds/census background, low priority.

## Report
`.notes/reports/MAC-P2<x>.md`; return ≤ 15 lines: commits, layout/golden results, census delta, wine A/B summary, open items.
