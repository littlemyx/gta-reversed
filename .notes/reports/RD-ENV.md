# RD-ENV - the orig-port prefix 98 -> 42 was the harness: RD-AB-16's port exes were run without their linker .map, so pin 128 (audio RNG partition) was silently off; with the map HEAD measures 145

Agent RD-ENV, rules v5, 2026-10-26 15:56 - ~16:35 WEST. Worktree C:/gta-wt/rd-env (removed). Data C:/gta-scratch/rd-env (bf_map, bf_nomap, head_map: run.sh reports + traces; bf/ head/ = exe+map). Orig traces = RD-AB-15's fin/o1..o3 (orig leg is deterministic 300/300 and unchanged; no orig run this session, Documents untouched, run.sh signature ee0a22fe... before/after).

## The input that flips 98 -> 42
Not the data dir, not env, not time: the **.map file next to the port exe**. rd_port_run.sh derives NOTSA_LOCKSTEP_AUDIO_RVA (audio code ranges, pin 128) from `<exe>.map`; with no map it only printed "WARNING pin 128 without NOTSA_LOCKSTEP_AUDIO_RVA" into p?.runner.log and ran with the partition off. Every RD-AB-16 run log has that warning (fin/p1..p3, r1..r4, pa, pb, ob, oc: exes copied as probe*.exe/final.exe/main.exe without a map); RD-AB-15's runs (exe in its worktree bin with map) did not. Without the partition the first audio rand (footsteps, traced frame 42, RD-AB-8) draws from the gameplay stream: ncalls exe 78 / port 129 at frame 42, peds differ at 76 - exactly RD-AB-16's "vehicle generation" divergence. No port bug.

| run (pin 1023, intro, 300 fr, pair vs RD-AB-15 orig o1..o3) | map next to exe | prefix |
|---|---|---|
| bf7792b0 exe, 3 reps | yes | 98 / 98 / 98 (p-p 300/300) |
| same exe copied alone (bf_nomap) | no | 42 (rng@42: A=ad20bfec B=ec0c6ae1, ncalls 78/129 - identical to RD-AB-16) |
| HEAD 76e12e2f (with RD-AB-16's ProcessShiftSectorList fix), 3 reps | yes | **145 / 145 / 145** (p-p 300/300), first: rng@145 ncalls exe 6 / port 10 |

So RD-AB-16's fix is worth 98 -> 145; the next divergence is at frame 145 (fc 1048721) - not analysed.

## Harness fix (tools/standalone/replay_diff)
- rd_port_run.sh: pin 128 without an audio RVA is now FATAL (exit 2, before the GPU lock). Sources: `<exe>.map` (validated: map header "Timestamp is" must equal the exe's PE TimeDateStamp, audio_rva.py 3rd arg; stale/foreign map -> exit 3), or `<exe>.audio_rva` (output of audio_rva.py, small file for exes copied without the 40 MB map), or NOTSA_LOCKSTEP_AUDIO_RVA; `RD_NO_AUDIO_RVA_OK=1` to run knowingly without. Prints "audio_rva ranges: N" (349 for a good exe, 0 = bad) into every runner log.
- run.sh: same preflight up front (no 3 retries of a doomed run). Verified: exe without map -> rc 2; HEAD map with the 13:06 exe -> rc 2.
- Rule for briefs/agents: copy `gta_reversed.map` together with every port exe used for A-B (or write `<exe>.audio_rva`); quote the "audio_rva ranges" line when reporting a prefix.
