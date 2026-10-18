# Versioned agent rules
- `CURRENT` names the active version; each brief says `Rules: .notes/agent_rules/<version>.md`.
- Versions are frozen once tagged (`agent-rules-vN`); a change = new file `vN+1.md` + tag, with "Changes vs vN" at the top.
- Roll back: write the old version into `CURRENT` (or `git checkout agent-rules-vN -- .notes/agent_rules`) and brief new agents with it.

| version | from | summary |
|---|---|---|
| v1 | 2026-10-16 | sonnet agents ≤ 5 parallel, worktrees, build/GPU locks, -j4, reports ≤ 15 lines, watchdog |
| v2 | 2026-10-18 | 2–3 parallel, smaller tasks, context cap ~200k with continuation hand-off, ≤ 1 read-only helper sub-agent, no-vsync test runs, compile cache |
