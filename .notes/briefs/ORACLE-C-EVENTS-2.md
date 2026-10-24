# ORACLE-C-EVENTS-2 — fix the five open Events/Ped findings, then real event fixtures

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-c-events-2.
## Evidence
.notes/reports/ORACLE-C-EVENTS.md (open findings table, W3_KNOWN_MISMATCH rows, skip tables). In priority order:
1. CEventHandler::ComputeGunAimedAtResponse 0x4C2840: null tactive/tsimplest deref in the IsFleeTask lambda (exe tests at 0x4C2872);
   exe maps task types 0x38D/0x38C to CTaskComplexFleeEntity, port only the smart-flee types. 15/150.
2. CEventHandler::ComputeVehiclePotentialPassiveCollisionResponse 0x4B96D0: 62/150, not analysed.
3. CEventVehicleHitAndRun::ReportCriminalEvent 0x4B27D0: port-only AV in CEntity::GetType on a null entity, exe survives.
4. CEventHandler::ComputeDeadPedResponse 0x4B9470 (1/150) and CPed::DisablePedSpeech / DisablePedSpeechForScriptSpeech 0x5EFF60/0x5EFF80
   (1/60): one special-input case each — decide fixture artefact vs bug from the asm.
5. Then: real constructed events as `this` for the generated rows where 60-100% of cases fault identically on both sides.
## Task
Fix as the exe does (one commit per fix, asm address, pre-fix failure count, consumer audit; soak before landing). Remove the
W3_KNOWN_MISMATCH guards / skip rows you fix. HARD budget 250k tokens / 100 min — at 200k stop starting new work and land.
## Do not touch
game_sa/Models and game_sa/Entity (root) oracle tests (ORACLE-C-MODELS works there).
## Acceptance
Findings fixed or proven artefacts; 0 PC24 mismatches; soak OK; report ≤15 lines with start/end time; full report .notes/reports/ORACLE-C-EVENTS-2.md.
