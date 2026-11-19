# ASM-HC13 (audio logic, rand relevant)
Zone rows (exe_addr != "-", NONE, audio files): 139; 94 already had a verdict in asm_sweep/*result*.tsv; 45 remaining: 38 compared (37 MATCH, 1 FIXED), 1 UNSURE (LoadSound tail), 6 SKIP-OUTPUT (decoders/stream transformer/AllocateChannels).
Rand: Weather::Service draw order verified (GetRandomNumberInRange(-6,6) then ResolveProbability(0.5), one pair per switch per twin sound); TwinLoop PlayTwinLoopSound draws were not in this slice (already verdicted).
Fix: CAEAudioHardware::RequestVirtualChannelSoundInfo ignored vch >= 300 in the exe (OOB write in the port).
Not touched: other audio rows with an existing verdict (not re-verified).
