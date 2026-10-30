# ASM-S84 (phase 3 tier A: rest of CAEVehicleAudioEntity per-class processing, CAEWaterCannonAudioEntity) - 2026-10-30
Result: 36 rows: 17 compared (12 verdict FIXED in 9 commits incl. 4 only for a shared constant / rotor speed, 5 MATCH), 19 NOT_REACHED (budget). Table in S84.result.tsv.

Method: asm read function by function with exe float constants resolved (annotasm), jump tables decoded (model -> sound params for ProcessMovingParts / RoadNoise), struct offsets mapped from the header (this+0x80 = m_AuSettings, +0xA8 = m_IsWreckedVehicle, ...). Every deviation re-read a second time. No emulator (functions read directly).

Fixes (hash: function: what):
- d24873a60 ProcessVehicleRoadNoise (0x4F8B00): non-trailer models also cancel when the contact wheel count is 0 (the exe tests Transmission AND wheels at 0x4F8B65-0x4F8B72); silence threshold is 8.0e-5 (0x862CF4), the port used 8.0e-4 (10x); NaN speed cancels; volume = log10*20 + (offset + base) (association); camera distance summed (z^2+x^2)+y^2.
- 8ef2947b3 ProcessReverseGear: NaN gas pedal keeps type 20 (jnp). 5d66bdac3 ProcessRainOnVehicle: NaN rain volume returns.
- 6769424be ProcessEngineDamage (0x4FAE20): the "fucked" (broken engine) sound is GENRL_VEHICLE_GEN sound 2 (`push 2` at 0x4FAFA5); the port played sound 28 (spray paint) - copy of the steam call.
- c474fb458 ProcessMovingParts (0x4FB260): smoothing step used useTimeStep=true (pow(step, timestep)); exe is a fixed 0.2 step -> GenericFrameRate. CEMENT (524) is in the exe's model list (smoothing runs, jump table has no entry -> return); NaN delta picks the first param set.
- 4f9d142ef rotor frequency step: exe constant 0x862D04 = 0x3BAEC33C, the port used 1.f/187.5f = 0x3BAEC33E in RC plane, RC heli, AI prop, player/dummy prop (hovercraft/jet already had the exe constant).
- 107b67afe GetHeliRotorSpeed: `m_fHeliRotorSpeed * 0x862D1C` (0x4091745D = 1/0.22f) in all four heli functions (RC/player/AI/dummy), the port divided by 0.22f (last-bit difference).
- 508f50f80 ProcessDummyRCHeli (0x4FAA80): FrqSpeedFactor is the exe float 0x862D00 = 0x3E4CCCCC (1 ulp below 0.2f); frequency = ((sf*k + tilt*0.2) + accel) + 0.8 (base was added first).
- a335faa15 Service (0x502280): JustWreckedVehicle runs before m_IsWreckedVehicle is stored, JustGotInVehicleAsDriver / JustGotOutOfVehicleAsDriver run BEFORE the radio on/off calls (port order was reversed).

MATCH: ProcessDummyRCPlane (apart from the step constant), ProcessNitro, ProcessDummyHovercraft, AddAudioEvent(event, CPhysical*), WaterCannon Constructor and Service (all four sounds' bank/slot/sfx/speed/volume/position).
Open (NOT_REACHED, budget): 0x4FCA10 0x4FD290 0x4FD8F0 0x4FDFD0(body) 0x4FE420 0x4FEE20 0x4FE940 (heli bodies) 0x4FF320 0x4FF7C0 0x501650 0x501960 0x4FF900 0x4F9710 0x4FFDC0 0x500040 0x500CE0 0x500DC0 0x501270 0x501480 0x501AB0 0x4FBB10 0x501E10 0x4F6420.
Leads for the next agent: (1) the exe PlayerHeli/AIHeli/DummyHeli (0x4FE420/0x4FEE20/0x4FE940, lines ~216/238 of the dump) step the rotor frequency by 0x862D04; the port uses Heli.RotorFreqStepLo/HiSpeed (0.03 / 0.005) via UpdateHeliRotorFrequency - verify what the 0x862D04 step is there. (2) ProcessDummyRCPlane/RCHeli: when neither accel nor brake is pressed the exe adds an UNINITIALISED stack float ([esp+0xc]); the port adds 0.
Gate: StandaloneRelease + StandaloneRW built after the last code edit; wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=compact exe) 1142 functions, mismatches PC24 0 / PC53 0; PortableWin64 (x64 cl) compile of AEVehicleAudioEntity.cpp 0 errors; v6 pre-commit grep clean; combat regress PASS=14 (run once at the end).
