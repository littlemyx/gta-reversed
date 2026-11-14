# ASM-P4A1: oracle-only sample, deviation rate

- Population: 5067 hooked addresses not asm-read (9313 sites - 4386 addresses in S*.result.tsv - 94 PHASE3_REPLACED), `.notes/asm_sweep/P4_ORACLE_ONLY.tsv` (expected ~5068: matches).
- Sample: seed 20261114, 40 rows from 2894 eligible (Tasks, Events, Entity/Ped, Entity/Vehicle, Scripts, Collision, Camera, Streaming/Pools/General/World, Weapons-adjacent, CarCtrl, Buoyancy, Garages ...; P4LEFT files excluded; exe size < 0x20 and small getters skipped). `.notes/asm_sweep/P4A1.tsv`.
- Verdicts: MATCH 32, FIXED 3, NOT_REACHED 5 (SphereCastVersusVsPoly, AddSplashParticles, PreCalcSetup, SimpleCalcBuoyancy, ProcessBuoyancyBoat: 0x3d7-0x61c bytes, budget).
- Deviation rate: 3 / 35 compared = 8.6% (small sample; 95% interval roughly 2-23%). Extrapolated to ~5000: ~400 functions, most likely in the larger bodies (the 3 hits were 0x26, 0xf9, 0x8a bytes).
- Classes: (1) inverted/wrong condition on a flag/state byte (ActivateGarage: `== CLOSED` vs `!= CLOSED`); (2) enum-valued input outside the two expected values handled by an if/else instead of the exe's two compares (ProcessFade flag 2+); (3) early-out writes a result the exe leaves untouched (ComputePedSoundQuietResponse wrote nullptr to m_EventResponseTask).
- Why the oracle tests did not catch them: all three sit on inputs the generators do not produce (garage door state with type 11, script-supplied fade flag, response task pre-set non-null with a null source entity); the oracle only compares what the random drivers reach, and a never-hit branch is a pass.
- Commits: e5a388f01 ActivateGarage, 09c9b925e ProcessFade, 1a3c62e5e ComputePedSoundQuietResponse.
- Tests: see the results block at the end of this file.

## Results
run_all_tests (exe oracle ON): 74/74 ok. gameplay_regress combat+missions: 25 PASS. Release build ok.
