# ORACLE-W3-IK — game_sa/Ragdoll (IKChain, BoneNode, IKChainManager, BoneNodeManager) classes A+B

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-ik.
## Evidence
Coverage JSON: Ragdoll/IKChain.cpp 14 and Ragdoll/BoneNode.cpp 13 uncovered A/B; Ragdoll overall A 29 / B 9. This is the IK that turns
heads, aims arms and points guns (look-at, aim-at, IsFacingTarget). Playtest 2 bug U16 (cops not reacting in the LSPD) was partly
IKChain_c::IsFacingTarget — fixed by hand then, never oracle-checked. Lessons: ORACLE-W3-ANIM.md (real clump/hierarchy fixtures),
ORACLE-W3-AI.md, ORACLE-W3-COLL.md (x87 precision: quaternion/matrix maths), ORACLE-W3-INPUT.md "hang" section (oracle_fast helpers
if the target gets slow; run_all_tests kills oracle targets at 900 s).
## Task
Rows for the A+B uncovered Ragdoll functions with real fixtures (a ped clump with an RpHAnim hierarchy, bone nodes with limits, an
IK chain set up by the exe's own setup path); fix every mismatch (one commit per fix, asm address and pre-fix failure count in the
subject) or skip with reason. Keep the target under 5 minutes. Aim under 300k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run the new target + targets linking the changed TUs; report ≤15 lines with start/end time.
