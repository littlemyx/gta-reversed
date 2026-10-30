# ASM-S84b (phase 3 tier A: prop / heli / jet remainder of CAEVehicleAudioEntity) - 2026-10-30
Result: 3 shared-logic fix commits touching 8 rows (prop player/dummy/AI, jet stall + prop speed, 3 heli rotor steps, sea plane distant volume); 15 rows NOT_REACHED (budget). Table in S84b.result.tsv.

Method: asm of the prop functions read with stack offsets re-traced by hand (cdecl AudioLog10 leaves its argument on the stack, so `[esp+N]` shifts by 4 per pending push); Player vs Dummy prop dumps diffed against each other, which isolated the player-only stall branch; every deviation re-read against the raw (addressed) dump.

Fixes (hash: what):
- 5bd50ecb4 GetAircraftDistantSoundVolume (used by prop / AI prop / 3 heli / seaplane; exe 0x4FD577-0x4FD5F1, 0x4FF12E-0x4FF1E0): the exe compares the DISTANCE (sqrt) with 48/128 and fades the distant sound in between 48 and 128 m: (log10(base)*20 + log10((d-48)*0.0125)*20) + 8; d < 48 is -100. The port compared the squared distance with 128 (so the fade never ran) and did log10(base + invLerp). ProcessPlayerProp/DummyProp also passed the stall volume (dB) as the base; the exe passes the prop speed factor and ADDS the stall volume.
- 03b5ec43a ProcessPropOrJetStall (shared by ProcessPlayerProp 0x4FD62B-0x4FD7B3, DummyProp 0x4FDD03-0x4FDE8F, PlayerJet 0x501746-0x5018F2): the volume is zeroed when m_IsSilentStalled is CLEAR (`test al, al; jne skip; mov [esp+..], 0`), the port zeroed it when set; ProcessPlayerProp only: with m_StalledCount > 0 and 0 contact wheels (CAutomobile+0x960) the stall is forced to 0.8 / -12 dB regardless of health and the early out for hp >= 650 is skipped. Also: engine-off cancel is `!engineOn` for the player and `!engineOn && recording < 0` for the dummy (the port had the two swapped); prop speed is multiplied by the exe constant 0x862D18 (1/0.34f), not divided (differs in the last bit for ~10% of inputs; also used by jets); the player prop ignores the recording; stall + (log10*20 + 8) association for the front/rear/near sounds.
- 91fa5cc58 UpdateHeliRotorFrequency: S84 lead confirmed. All three heli functions step the rotor frequency by 0x862D04 (0x3BAEC33C) up and down; there is no 0.03 / 0.005 low/high speed step in the exe (no such constant in any of the three bodies). Config RotorFreqStepLo/HiSpeed replaced by RotorFreqStepUp/Down.

Checked and MATCH (no change): ProcessAIProp bank/slot/engine-off logic and its 2 sounds; heli sound ids/slots/volume formulas of ProcessAIHeli (front/rear/near 2.0 dB offset, cop heli 6.0 offset, underwater) and the shared player/dummy heli body; rotor freq variance (-0.1..0.1 + 0.75 at hp < 390), tilt (1 - up.z) * 0.5; prop camera PoV factor and rotor frequency sum order.

Open / notes:
- Exe ProcessPlayerProp/DummyProp (and RC plane/heli, S84 lead 2): when accel <= 0 and brake > 0 the exe subtracts 0.05 from an UNINITIALISED stack float; the port uses 0 - 0.05. Not reproducible.
- ProcessPlayerSeaPlane / ProcessDummySeaPlane use the same distant-fade helper now; the exe base argument was not verified, nor the rest of the bodies.
- NOT_REACHED (budget): 0x4FCA10 0x4FF7C0 0x501960 0x4FF900 0x4F9710 0x4FFDC0 0x500040 0x500CE0 0x500DC0 0x501270 0x501480 0x501AB0 0x4FBB10 0x501E10 0x4F6420 (ProcessPlayerJet / ProcessPlayerSeaPlane bodies also only partly compared).
- AIProp engine-off uses `recording == -1` vs the exe's signed `< 0` (only differs for ids <= -2, not produced by the game).

Gate: StandaloneRelease + StandaloneRW built after the last code edit; wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=compact exe) 1142 functions, mismatches PC24 0 / PC53 0; PortableWin64 (x64 cl) compile of AEVehicleAudioEntity.cpp 0 errors; v6 pre-commit grep clean; combat regress PASS=14 (run once at the end).
