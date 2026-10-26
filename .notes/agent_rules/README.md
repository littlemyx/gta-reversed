# Versioned agent rules
- `CURRENT` names the active version; each brief says `Rules: .notes/agent_rules/<version>.md`.
- Versions are frozen once tagged (`agent-rules-vN`); a change = new file `vN+1.md` + tag, with "Changes vs vN" at the top.
- Roll back: write the old version into `CURRENT` (or `git checkout agent-rules-vN -- .notes/agent_rules`) and brief new agents with it.

| version | from | summary |
|---|---|---|
| v1 | 2026-10-16 | sonnet agents ≤ 5 parallel, worktrees, build/GPU locks, -j4, reports ≤ 15 lines, watchdog |
| v2 | 2026-10-18 | 2–3 parallel, smaller tasks, context cap ~200k with continuation hand-off, ≤ 1 read-only helper sub-agent, no-vsync test runs, compile cache |
| v3 | 2026-10-19 | 1–2 parallel by quota pace, 100-min wall-clock cap (replaces call count), orchestrator gates partial landings, lock/cleanup/worktree rules, real oracle fixtures |
| v4 | 2026-10-21 | gate adds soak (+ daily gameplay_regress); consumer audit for state-layout fixes; agents soak before landing gameplay fixes |
| v5 | 2026-10-24 | never kill by image name; agents gate on touched targets + soak (orchestrator runs the full suite); gate window with <=1 agent; 200k token stop |
| v6 | 2026-10-27 | portable coding rules (PORTABLE_GATE.md table) binding for new code; pre-commit grep; PortableWin64 compile for shared-header changes |
