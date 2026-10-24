# MAC-R<n> — one stream-R slice (generic brief; the slice id is given in the launch prompt)

Host: Apple Silicon Mac. Rules `.notes/agent_rules/v4.md` (Windows-only parts do not apply). Repo
/Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main` (remote `fork`). Main checkout, shared with ONE parallel agent
named in the launch prompt — never touch its files or uncommitted edits. `.notes/` is gitignored (`git add -f`). Wall-clock cap 100 min.

## Spec
`.notes/STREAM_R_PLAN.md`: architecture sections, the row of your slice (content, deps, acceptance), the Win32-untouched rules, and the
census command. Background: .notes/reports/MAC-R-DESIGN.md, MAC-A1b.md (preset, census), MAC-W0.md (portable app layer, source/portable glob).

## Rules
- Win32 MSVC build unchanged: new code under source/portable/render/ (portable-only glob), whole D3D-only files excluded in the portable
  CMake list, mixed files guarded with `#ifndef NOTSA_PORTABLE` on their own lines. Proof per slice: StandaloneRelease compile commands
  identical + wine cl.exe A/B (code+relocs) on every MSVC-compiled TU you touched; `__LINE__`-only differences are neutral (classify).
  Method: .notes/reports/MAC-W0-REV.md / MAC-P2b.md.
- vendor/librw (fork littlemyx/librw, branch notsa): patches go to the fork's `notsa` branch (commit there, push `fork notsa`), then bump
  the submodule pin in a separate gta-reversed commit. Never force-push the fork. The Win32 builds use librw too: a librw change must be
  backend-local (gl3/) or guarded so d3d9/null builds are byte-identical — say how you proved it.
- Honest stubs only (log + failure), listed with file:line as R-n items.
- Commit only your files (`git commit -- <files>`, trailer exactly `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`), push after
  each (pull --no-rebase on reject; never force; never push reverse/interior_c). ≤ 1 own sub-agent. Builds/census in the background,
  low priority (`taskpolicy -b nice -n 15`), `timeout`. Leave no background tasks running when you finish.

## Report
`.notes/reports/MAC-R<n>.md`; return ≤ 15 lines: commits, census R/total before/after, no-op proof, stubs, open items.
