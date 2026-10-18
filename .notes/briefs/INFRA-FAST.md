# INFRA-FAST — no-vsync test switch and compile cache

Rules: .notes/agent_rules/v2.md (read it first, then .notes/PLAYTEST_AGENT_ENV.md). Worktree: C:/gta-wt/infra-fast, branch from reverse/interior_c HEAD.

## Why
Vsync-locked Present costs ~78 ms/frame on this PC → 30–75 s cold starts for every agent test run; without vsync boot is ~1.8 s.
Every fresh worktree rebuilds ~1200 TUs from scratch.
## Task
1. `NOTSA_STANDALONE_NOVSYNC=1` env switch, compiled only under NOTSA_STANDALONE_RUN (the user-facing default stays the exe behaviour):
   present with sync interval 0. Keep the 25 fps frame limiter as is (game logic timing must be unchanged) — state in the report what is
   and is not affected. Wire it into the soak/probe scripts as an opt-in (env passthrough), document next to the other NOTSA_STANDALONE_* vars.
2. Compile cache for MSVC Win32 presets (sccache or ccache with MSVC support; /Z7 or what is needed instead of /Zi with PDB server conflicts):
   opt-in via a cmake cache var / preset env so the default build is unchanged; measure cold vs warm rebuild of a fresh worktree (-j4 low prio).
   Document setup in docs/WINDOWS_SETUP.md. Do not install anything system-wide that needs admin; a user-local binary under C:/gta-tools is fine
   (download only from the official GitHub release, verify the checksum).
## Acceptance
Boot-to-first-marker time with NOVSYNC vs without (numbers); warm vs cold worktree build minutes; Release builds; run_all_tests 44/44; soak OK
without the switch. Report ≤15 lines.
