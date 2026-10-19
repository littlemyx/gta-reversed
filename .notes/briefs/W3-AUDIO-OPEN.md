# W3-AUDIO-OPEN — the 4 open audio rows left by ORACLE-W3-AUDIO

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh w3-audio-open.
## Evidence (.notes/reports/ORACLE-W3-AUDIO.md, tests/standalone/tools/w3_audio_skip.tsv)
- CAEPedSpeechAudioEntity::CanPedSayGlobalContext 0x4E5730: 97/3000 cases, port true / exe false (peds speak when the exe keeps them silent)
- GetSpecificSpeechContext 0x4E4470: 1/3000
- GetRepeatTime 0x4E47E0: 10/1500
- CAEVehicleAudioEntity::UpdateJetPack 0x4E0EE0: 6/500, last bit
The rows were removed to keep the target green.
## Task (four small functions)
Restore each row, find the differing inputs, read the asm path for them, fix the port (one commit per fix, asm address in the subject,
row fails pre-fix) or prove a fixture artefact. Then also fill GetVol/GetFreqForPlayerEngineSound: seed the exe statics they read
in the harness image so the rows carry information.
## Acceptance
4 rows FIXED/EXPLAINED and back in the target; wave3_audio_oracle_test 0 PC24 mismatches; run_all_tests green on the rebased tree; report ≤15 lines.
