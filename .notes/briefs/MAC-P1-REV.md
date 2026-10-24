# MAC-P1-REV — line-by-line review of the P-1 codemod

Host: Apple Silicon Mac. Rules `.notes/agent_rules/v4.md` (Windows-only parts do not apply). Repo
/Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main` (remote `fork`). Agent MAC-A1b works in the same checkout with
uncommitted edits (CMakePresets.json, conan profile, source/CMakeLists.txt, WinMain.cpp, utility.hpp, fakerw.h) — never touch those.
`.notes/` is gitignored (`git add -f`). Wall-clock cap 80 min.

## What to review
MAC-P1's commits: `git log --oneline 1db39c22..83068710 -- source/Base.h source/game_sa` (21 commits, 162 files, 508+/508-).
Design (read `.notes/reports/MAC-P1.md` first): VALIDATE_SIZE / VALIDATE_OFFSET / VALIDATE_LAYOUT = x86 in-memory layout, compiled out
under NOTSA_PORTABLE; `_FILE` variants = on-disk layout, enforced in every build. Evidence list `.notes/reports/MAC-P1-file-structs.tsv`.

## Check every hunk (all 508), record per-file verdict
1. Semantics preserved on MSVC: the converted assert checks exactly the same expression and value as before (same struct, member, number;
   a split `&&` chain covers every conjunct; no assert dropped; message-only differences OK). Any `static_assert` that was NOT a layout
   check (e.g. enum counts, type traits) must not have been converted.
2. Classification: `_FILE` only for structs with raw-byte file I/O evidence in the TSV; spot-check 15 TSV rows against the code. Flag
   any in-memory struct marked `_FILE` and any obvious file struct (save blocks, IMG/COL/IPL/IFP headers, script structures read from
   main.scm) left as plain VALIDATE_*.
3. Line counts unchanged per file (so __LINE__-based code and hook line numbers do not shift) — `git diff --numstat`.
4. Base.h macros: correct under both modes; `VALIDATE_LAYOUT` variadic form safe with commas in template args.
Then compile-check: run the MSVC-mode clang census (`clang_census.py run --mode msvc`, 1–5 min, see STREAM_C_PLAN section 2) on the
changed TUs — must be clean (catches a mis-typed assert that MSVC would reject).

## Fixes
Fix clear mistakes yourself (one commit, `git commit -- <files>`, trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`,
push to fork portable/main; never force, never push reverse/interior_c), keeping line counts. Anything debatable: list, don't change.

## Report
`.notes/reports/MAC-P1-REV.md` with a table of findings (file:line, kind, fixed/listed). Return ≤ 15 lines.
