# ORACLE-W3-CAMERA — exe-oracle coverage for the camera root files (Camera.cpp, Cam*.cpp) classes A+B

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-cam.
## Evidence
The camera is what the player feels every frame (follow-ped / follow-car / aim / cutscene cams). CCamera::Init is already exact
(.notes/reports/CAM-INIT.md); the per-mode Process_* maths and the camera helpers are mostly unchecked. Count the A+B uncovered
functions of Camera.cpp / Cam.cpp / CamPath* / CameraHelpers in ORACLE_COVERAGE (oracle_coverage.py --json) and state them.
Lessons: ORACLE-W3-ENTITY.md, ORACLE-W3-COLL.md (x87 precision — the cams are float-heavy), ORACLE-W3-AUDIO.md (CRT initialisers).
## Task
Rows for the A+B uncovered camera functions with a real CCamera/CCam fixture (TheCamera at its exe address, target entity with a
matrix); fix every mismatch (one commit per fix, asm address and pre-fix failure count in the subject) or skip with reason. Aim under 300k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run the new target + targets linking the changed TUs on the rebased tree (the orchestrator
runs the full gate); report ≤15 lines with start/end time.
