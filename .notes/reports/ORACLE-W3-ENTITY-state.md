# ORACLE-W3-ENTITY state (100-minute cap reached 20:50, 2026-10-19)

All work is landed and pushed (origin/reverse/interior_c ad628cfd): wave3_entity_oracle_test (51 functions, PC24 0), 11 port fixes, report, coverage, status line. Nothing in progress.

## Gate on the landed tree (run_all_tests.sh in the main checkout, build complete: 2431 steps ok)
The full run was cut at the cap (the runner is sequential and was at wave3_audio after 46 of ~54 targets; killed, build dir left complete). Results seen: every oracle target that finished showed PC24 0 hard mismatches
(fixed, logic, review, wave1, wave1_consts, wave2, wave3_anim, game, gameplay, physics: strict counts are NaN-payload-only, second number 0); script_oracle_* passed; rw_* targets do not include the changed TUs.
Run by hand on the same exes (all rc 0, PC24 0): wave3_ped, wave3_vehicle, world, wave3_events, wave3_gameplay, wave3_models, wave3_tasks, x87, wave3_coll (64 NaN-payload-only), wave3_entity (also at 3-4x cases).
Not seen finishing: wave3_audio_oracle_test (slow, was past CAEPedSpeechAudioEntity rows with 0 mismatches when stopped).

CONTINUE (orchestrator, ~10 min): `SKIP_BUILD=1 tools/standalone/run_all_tests.sh build/StandaloneRW "wave3_audio|wave3_entity|wave3_models|wave3_coll|wave3_events|wave3_tasks|wave3_ped|wave3_vehicle|wave3_gameplay|world_oracle|x87|script_oracle|pool_oracle|audio_fade|rw_skin"` (build dir is current for the landed tree), then refresh Release / C:\gta-play per v3 rule 2.

## Open
- CEntity::SetupLighting 0x553DC0 / RemoveLighting 0x553370: full paths (RpLightSetColor / RpLightSetFlags on the global lights, CPointLights) not compared, only the early-outs (tests/standalone/tools/w3_entity_skip.tsv).
