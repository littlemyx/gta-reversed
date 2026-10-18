# USERDIR — test runs must not touch the user's Documents/GTA San Andreas User Files

Rules: .notes/agent_rules/v2.md. Worktree: C:/gta-wt/userdir.
## Evidence
Every agent soak/probe run rewrites the user's gta_sa.set (and may write saves/screens) in Documents; agents restore it from
C:/gta-scratch/playtest1/gta_sa.set by hand, and with 3 parallel runs the live file is whatever the last run wrote (PT2-AA, PT2-TRAILER reports).
## Task (one item)
`NOTSA_STANDALONE_USERDIR=<path>` env (run build only, default unchanged = exe behaviour): the user-files directory (exe: CFileMgr /
GetUserFilesFolder / InitUserDirectories — find the exact functions) resolves to that path. Make all agent run scripts (soak, probe, fps_run,
the injector harness wrappers) set it to a per-run dir under C:/gta-scratch/userdir/<run>, seeded from a copy of the playtest1 gta_sa.set.
play.cmd must NOT set it. Update PLAYTEST_AGENT_ENV.md (drop the manual restore step, keep "never touch Documents").
## Acceptance
A soak + a probe run leave Documents/GTA San Andreas User Files byte-identical (hash before/after); play.cmd still uses Documents; Release builds;
run_all_tests 45/45. Report ≤15 lines.
