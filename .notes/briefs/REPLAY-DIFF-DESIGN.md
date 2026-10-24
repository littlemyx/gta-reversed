# REPLAY-DIFF-DESIGN — plan for whole-game lockstep comparison against the original exe (class D and everything above it)

Rules: .notes/agent_rules/v4.md. Mostly read-only; small probes allowed in a worktree (tools/standalone/windows/new_worktree.sh replay-diff).
## Why
Function oracles cost ~35k tokens per class-C row now; ~1990 class-D functions (file/OS/render) cannot be row-tested at all. A
deterministic run of the port and the original exe side by side, same inputs, compared per frame, covers everything at once and
points at the first diverging frame/subsystem. The portable build (Mac, portable/main) needs the same harness later (portable vs Win32).
## Questions to answer (with evidence: file:line, commands run, numbers)
1. What exists: tools/standalone/gameplay_regress/orig_run.sh and the `-o` live-original legs (how the original exe is run, which exe —
   gta_sa_compact.exe, never Steam gta-sa.exe v3.0 — how it is fed input, how its state is polled), the ASI/plugin build of this repo
   (can a small tracer ASI/DLL be loaded into gta_sa_compact.exe?), the game's own CReplay, NOTSA_STANDALONE_INPUT route files.
2. Determinism sources in both: CTimer (frame time / fixed step), RNG (rand / CGeneral::GetRandomNumber seeds), streaming / CdStream
   async order, audio-driven timing, threads, uninitialised memory, vsync/present. What must be pinned (hook or env) in each exe.
3. Input: one input stream driving both (CPad level) — injection point in the original exe.
4. State to hash per frame: player (pos, heading, health, wanted, weapon), ped/vehicle/object pools (pos/model/state per slot), RNG
   state, timer, script globals/IPs, camera; plus a detailed dump on the first mismatching frame.
5. Feasibility probe: run the original exe and the port on the same short route with pinned timer/RNG and report how many frames stay
   equal on a minimal hash (player pos + RNG + timer), if that can be done within budget.
## Deliverable
.notes/REPLAY_DIFF_PLAN.md: answers 1-5, design (components, where the code lives, how it builds for the original exe vs the port vs the
portable build), risks, and 4-8 agent slices with acceptance criteria. Report ≤15 lines. HARD budget 200k tokens / 100 min.
Never commit gta_sa_compact.exe. Never run the user's Documents user files (private USERDIR; the original exe always uses Documents —
use hostenv.sh docs_backup/docs_restore exactly as orig_run.sh does, or do not run it).
