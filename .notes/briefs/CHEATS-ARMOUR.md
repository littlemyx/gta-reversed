# CHEATS-ARMOUR — cheats.state_matches_original armour 97/90/84 != 100, and the cheats free_start SETUP (mission=1) flake

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh cheats-armour.
## Evidence
gameplay_regress.sh cheats compares the player state after each cheat against the original exe's recorded values.
Seen 2026-10-24 in the overnight cdb loop (C:/gta-scratch/night/r1.log: c44_AFPHULTL_2, c70_PRIEBJ, c70_PRIEBJ_2 armour 97 != 100) and in
ANIM-CRASH-3's post-fix run n-4 (armour 97/90/84). Intermittent: other runs PASS. Hypotheses: CJ takes damage between the cheat and the
probe (peds/cars around; then the scenario needs a guard, not a port fix), or the port's armour cheat / damage absorption differs from the
exe (CCheat::ArmourCheat, CPlayerPed armour max from stats, CPedDamageResponseCalculator armour absorption) — check against the asm.
Also: cheats free_start SETUP "mission=1" (the opening mission still active after the bike mount/dismount) in 5 of 11 cdb runs
(.notes/reports/ANIM-CRASH-3.md, CRASH-HUNT.md); the route header is routes/header_free.inc (REGRESS-SETUP, .notes/reports/REGRESS-SETUP.md).
## Task
Root-cause both; a port bug is fixed as the exe does (asm address, oracle row if possible, consumer audit); a scenario artefact is fixed
in the route with a gate (until:...), never by loosening the check. Acceptance: 10 cheats runs (no cdb) with 0 armour FAIL and 0 SETUP;
soak OK if game code changed. HARD budget 250k tokens / 100 min.
## Do not touch
tools/standalone/replay_diff/, source/standalone/lockstep/, GameRand.h, Timer.cpp, WinPs.cpp (RD-ARM works there).
