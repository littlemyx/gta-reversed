# ASM-HC6: audio logic in RYDER_1 without a verdict

Zone: 89 rows (ryder1.tsv, verdict NONE, not in any result tsv; the AESound* match also pulled in AESoundManager rows). Compared 25 (23 MATCH, 2 FIXED), 64 SKIP-AUDIO-OUTPUT (no rand draw, no game-state write: volume/frequency maths, radio/engine queries, bank bookkeeping).
No rand draw is reachable from any row of the zone: the only draws (CAESound::CalculateFrequency, SetNextPlayTime, ChooseCollisionSoundID) already have verdicts.

Fixes
- CAudioZones::Update (exe 0x5083C0): sphere test is `sqrt(dz*dz+dy*dy+dx*dx) < r` on unrounded values, box test is strict `min < p < max` per axis (port: squared <= r^2, inclusive CBox); the 20 m skip test uses the same z,y,x sum.
- CAEVehicleAudioEntity::UpdateGasPedalAudio (exe 0x4F5EB0): only type 0 (+0x964) and 9 (+0x808) write; heli/plane/boat/bmx/quad/mtruck return. The port wrote through AsAutomobile() into an unrelated field of those vehicles (it is called for boats, helis, planes, bmx).

Verified by script: GetVoice name tables (all 5, 345 names) and GetAudioPedType names equal the exe's.
Rows: see .notes/asm_sweep/HC6.result.tsv.
