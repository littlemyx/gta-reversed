# REGRESS-POLICE — police no longer respond to a wanted level (gameplay_regress combat FAIL)

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh regress-police. TOP PRIORITY.
## Evidence
Orchestrator gate 2026-10-21 07:16 on 2e75ef99: gameplay_regress.sh combat scenario FAIL=3 (log C:/gta-scratch/gate_soak/1021-0652.regress.log):
combat.chase.police_arrive (no cop/law car near within 40 s; original 1 s), combat.chase.police_engage (no cop on foot / pursuit task /
damage / bust within 70 s; original 13 s), combat.chase.player_shot_at (CJ never hurt). All other scenarios pass (66 PASS).
On 2026-10-18 (PROBE-SUITE, .notes/reports/PROBE-SUITE.md) combat passed 100%. ~480 commits since then; the new-game crash fix 505a4aef
(CTheScripts::Process successor test) is required for any build to reach the scenario — commits before it crash at new game
(.notes/reports/NEWGAME-CRASH.md).
## Task
1. Reproduce: GR_ONLY / run only the combat scenario (see gameplay_regress.sh usage), NOVSYNC.
2. Bisect from 67087158 (good on 10-18) to HEAD with a scripted step: checkout, apply 505a4aef on top (git cherry-pick -n; skip if it
   conflicts or is already contained), incremental Release build, run the combat scenario, good = police_arrive PASS. git bisect run.
   If full rebuilds per step are too slow (~30 min), first try the obvious suspects by reverting single commits on HEAD: wanted/police/
   population/CarCtrl/PedIntelligence/Events/Tasks fixes (ORACLE-W3-GAMEPLAY Wanted, W3-PED-OPEN SetWantedLevel, ORACLE-W3-AI CarCtrl,
   ORACLE-W3-EVENTS cop-car responses, ORACLE-W3-STREAM IsCarModelNeededInCurrentZone/AddToLoadedVehiclesList, ZONEINFO zone commands,
   ORACLE-W3-WORLD FindPlayerCentreOfWorld) and the script-loop fix 505a4aef itself.
3. Root-cause against the asm; the culprit "fix" may be right in isolation but wrong for a caller, or the oracle row may have been a fixture
   artefact — say which. Fix as the exe behaves (one commit, asm address in the subject).
## Acceptance
Culprit commit named; root cause with asm evidence; combat scenario PASS 3/3 on the fixed HEAD; full gameplay_regress PASS; report ≤15 lines.
HARD budget under 300k tokens.
