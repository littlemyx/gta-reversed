# ASM-S84c (phase 3 tier A: remaining CAEVehicleAudioEntity per-class rows) - 2026-10-30
Result: 16 rows: 8 FIXED (7 code commits), 5 MATCH, 3 NOT_REACHED (budget). Table in S84c.result.tsv.

Method: asm from the S84b dumps (re-traced stack offsets by hand, cdecl AudioLog10 leaves its argument pending), jump tables decoded from the exe image (jet models, special-vehicle models, vehicle type, dummy state), exe data tables read with a small memory reader; every deviation re-read twice.

Fixes (hash: what):
- 1110a5de8 ProcessPlayerSeaPlane (0x4FF320): the port computed `CalculatePlanePropFreq(...)` and threw the result away; the exe steps m_CurrentRotorFrequency towards it (0x862D04 both ways) and plays the front/rear sounds with the new value. Base argument of the distant volume verified: clamp(prop speed, 0.2, 1) (0x4FF4B2-0x4FF511); NaN prop speed -> 0.2.
- 94e3395d8 ProcessDummySeaPlane (0x4FF7C0): the exe plays only the near sound (sfx 17) with log10(speed factor)*20+8; the port used the distant-fade volume with base 0.
- 422c55c2e ProcessDummyJet (0x501960): pedals go through ftol(x*255) and `raw * (1/255)` for the prop speed AND the accel/brake arguments; the port passed `pedal * (1/255)` of the raw float.
- 66c274ae5 ProcessPlayerJet (0x501650): generic-bank check is the first thing (the port ran pedals + stall timers + rand before the check inside ProcessGenericJet); shared stall helper: the swap runs when now > swapTime (`jbe skip`), port used >= (also affects prop 0x4FD788 / 0x4FDE64).
- e6dc2573c PlayBicycleSound (0x4F9710): bank check precedes the update of an already playing tyre / sprocket sound.
- b710b5cb8 Dummy/PlayerBicycle (shared body): NaN speed ratio takes the 1.0 arm (`fcomp 1.0; jp`), std::min kept the NaN.
- f5177d634 ProcessDummyRCCar (0x500DC0): speed and volume tables were swapped (UpdateGenericVehicleSound(speed, volume)); both tables carried a 5th junk point {0,1} / {-12,-6} that made CGeneral::GetPiecewiseLinear return the junk y for every x >= 0 (RC car was constant speed 1.0 / -6 dB); ratio is |speed / max velocity| (port used the signed ratio, reversing was silent).
- 0c154edc7 ProcessDummyGolfCart (0x501270): volume table 0x8CBFD8 ends at x = 1.0001f (0x3F800347), the port used 1.0; NaN ratio plays.

MATCH: ProcessGenericJet (model jump table: Shamal/Hydra/AT-400/Andromada rows, tilt, PoV 0.67, thrust fade, rotor step, 4 sounds), ProcessPlayerCombine, ProcessDummyVehicleEngine (state table: 0/1/10 -> 0.2, 2 -> 0.15, others -> CAR_OFF), ProcessSpecialVehicle (model table), ProcessVehicle (type table, vp setup incl. z,y,x dot, per-type call order); the Player/Dummy bicycle bodies otherwise.

Open (NOT_REACHED, budget): 0x4FCA10 ProcessDummyStateTransition, 0x4FBB10 ProcessPlayerVehicleEngine, 0x4F6420 AddAudioEvent (dumps in C:/gta-scratch/asm-s84c/d_0x*.txt).
Note: the jet globals 0xB6B9F4..FC (jet ThrustVolBase for Shamal/Hydra/Andromada) are .bss zero in the image; no writer was searched for.
Gate: StandaloneRelease + StandaloneRW built after the last code edit; wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=compact exe) 1142 functions, mismatches PC24 0 / PC53 0; PortableWin64 (x64 cl) compile of AEVehicleAudioEntity.cpp 0 errors; v6 pre-commit grep clean; combat regress PASS=14 (run once at the end).
