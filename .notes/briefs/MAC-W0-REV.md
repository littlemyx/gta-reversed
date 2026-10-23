# MAC-W0-REV — review of MAC-W0 + remove the `#line` hack

Host: Apple Silicon Mac. Rules `.notes/agent_rules/v4.md` (Windows-only parts do not apply). Repo
/Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main` (remote `fork`). Main checkout. Agent MAC-P2b works in parallel
(GangWars.cpp, PlayerInfo.cpp, PlayerPed.cpp, Ped.cpp, Object.cpp, TheScripts.cpp, IplStore.cpp, tScriptParam/CSimpleVariables/
ExtraVertColour/MemoryHeap headers, tests/portable/**, new source/portable/systemtime.h) — never touch those. `.notes/` gitignored
(`git add -f`). Wall-clock cap 90 min.

## Input
MAC-W0 commits 3f46d831, 79cbd8c5, 550597e7 (+ tools/portable/w0a_guardlines.py). Reports .notes/reports/MAC-W0.md, MAC-W0a.md,
MAC-W0b.md. Decision in .notes/PORTABLE_SYNC.md (last entry): `__LINE__`-only differences are neutral; `#line N // W0a` directives go.

## Task
1. Remove every `#line … // W0a` directive (git grep '#line' -- source; only the W0a ones, leave pre-existing #line untouched) and drop
   w0a_guardlines.py (or keep it only as a checker that reports, never writes). Keep the guards themselves.
2. Line-by-line review of all W0 hunks in files the MSVC build compiles (everything outside source/portable/** and portable-only CMake):
   (a) every Win32 hunk unchanged inside `#ifndef NOTSA_PORTABLE` / `#else` — no body edits leaked to the Windows path; (b) guards balanced,
   no code moved out of order; (c) the portable replacement is behaviourally honest (same semantics or a logged failure; no silent fake
   success) — list doubtful ones. Then skim source/portable/*.cpp for obvious bugs (thread/CdStream correctness: lifetime, races, error
   codes vs Win32 originals) — list, fix only clear bugs.
3. No-op proof after your edit: StandaloneRelease compile commands identical; wine MSVC A/B (A = 6310e2ce~0 parent before W0 =
   `git rev-parse 3f46d831~1`, B = your HEAD) on the 11 TUs of MAC-W0b: code + relocation signatures identical modulo `__LINE__`
   immediates — classify each difference, any non-__LINE__ difference is a blocker to fix.
4. Portable census on the touched TUs: no regression vs MAC-W0 (W 3).

## Git
Commit only your files (`git commit -- <files>`, trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`), push after each
(pull --no-rebase on reject; never force; never push reverse/interior_c). ≤ 1 own sub-agent.

## Report
`.notes/reports/MAC-W0-REV.md` (findings table file:line/kind/fixed-listed, A/B table). Return ≤ 15 lines.
