# MAC-P4 — remaining 64-bit integer/pointer-width diagnostics: triage + safe fixes + plan

Host: Apple Silicon Mac. Rules `.notes/agent_rules/v4.md` (Windows-only parts do not apply). Repo
/Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main` (remote `fork`). Main checkout. Parallel agent MAC-P2h edits
PathFind.*, path-node headers, FileRecords/*Path*, tests/portable/*p2h* — never touch those. `.notes/` is gitignored (`git add -f`).
Wall-clock cap 100 min.

## Input
Latest portable census (after R-3b): `.notes/w0/r3b_after.tsv` (308 rows: category, file, line, msg). Non-R remainder:
P pointer/int width 101, other conversion/overload 88, P integer width / mixed overload 16, P size_t operator new/delete 20,
F/C inline asm 36 (NOT yours — stream F), C syntax 9 + nested DMI 5, imgui headers 4, misc. Top dirs: Entity 59, Scripts 30,
WaterLevel 27, CarGenerator 22, Core 11, Pools 10, Garage 9, Population 8, Events 8. Census tool: tools/portable/snap_census.sh
(see .notes/reports/MAC-R2.md). P-2 context: .notes/P2_PLAN.md, FileRecord pattern; inventory P-3/P-4/P-5 in PORTABILITY_INVENTORY.md.

## Task
1. Triage ALL non-R, non-inline-asm rows into root-cause classes with a recipe each, e.g.: pointer stored in a 32-bit field/var
   (script var, pool handle, user data) — semantics question; `std::min/max` with mixed `int32`/`long`/`size_t`; operator new/delete
   signatures with `unsigned int`; printf `%d` with size_t; enum forward decls; nested DMI; imgui include paths. For every class: is a
   behaviour-neutral mechanical fix possible that leaves MSVC bytes identical (e.g. `size_t` operator new signature is the same type on
   x86; explicit template args on min/max), or does it need design (pointer-in-32-bit storage)?
2. Implement ONLY the mechanical, provably-neutral classes (one commit per class; game function bodies get minimal edits, no logic
   change; keep line counts where possible). Prove: wine cl.exe A/B code+relocs identical on every MSVC-compiled TU you touched
   (`__LINE__`-only differences neutral; method .notes/reports/MAC-P2b.md). Before touching files the Windows host edits often
   (Entity/**, Scripts/**, Tasks/**), check `git log fork/reverse/interior_c --since=2.days -- <file>`; if hot, list instead of editing.
3. For the design classes write `.notes/P4_PLAN.md`: per site, what the original stores and how to make it 64-bit correct (handle
   table, pool index, `uintptr_t` under NOTSA_PORTABLE …), sliced for later agents.
4. Census before/after (snap_census).

## Git / report
Commit only your files (`git commit -- <files>`, trailer exactly `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`), push after
each (pull --no-rebase on reject; never force; never push reverse/interior_c). ≤ 1 own sub-agent. Leave no background tasks running.
Report `.notes/reports/MAC-P4.md`; return ≤ 15 lines (classes table summary, fixed counts, census before/after, A/B, plan pointer).
