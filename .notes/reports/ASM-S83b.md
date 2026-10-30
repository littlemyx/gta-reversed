# ASM-S83b (phase 3 tier A: CAEVehicleAudioEntity ctor/dtor, init/terminate, engine models, horn/siren, skidding, boat/train/aircraft) - 2026-10-30
Result: 21 of 21 rows compared in full (15 FIXED in 16 commits, 6 MATCH). Table in S83b.result.tsv. Extra item from the orchestrator (ProcessPropOrJetStall stall table) verified and fixed as its own commit.

Method: exe functions emulated in unicorn (copies of the S83 scripts in C:/gta-scratch/asm-s83b, extended with x87 PC24 via an injected fldcw stub, fs: patching for SEH prologues and native __ftol): Initialise for all 212 models + a type x bank x init sweep (220 cases) against a Python model of the port; RequestNewPlayerCarEngineSound st 1-5 and 1288 AC inputs. Everything else read from the asm; every deviation re-read against the exe a second time.

Fixes (hash: function: what):
- 441d9e004 Initialise (0x4F7670): m_DummyEngineBank stored before the m_IsInitialized test for bike/BMX/special.
- aef548f98 Terminate (0x4FB8C0): dummy-slot refcount only dropped if the slot still holds our bank (exe inlines DoesBankSlotContainThisBank); passenger branch also clears s_pPlayerAttachedForRadio / s_pVehicleAudioSettingsForRadio (dangling pointer into m_AuSettings otherwise).
- 7f0de76ea CancelAllVehicleEngineSounds(except) (used by JustGotIn/Out PLAYER_ID paths): cancelled ONLY the excepted sound instead of all but it.
- 5462ab4d3 JustGotInVehicleAsDriver: special vehicles always ensure the player bank; ONE_GEAR/NO_VEHICLE no-ops. b2111be71 JustGotOutOfVehicleAsDriver: NO_VEHICLE no-op (both were NOTSA_UNREACHABLE).
- e7dd96719 JustWreckedVehicle: horn/siren stopped with SetFlags+StopSound, not StopSoundAndForget.
- 7ef07269b ProcessVehicleSkidding: BMX speed/volume swapped; bike x1.2 on the speed (was volume); (VolBase+vol)+log10*20 association; NaN plays.
- a003de3de PlayHornOrSiren: siren part in exe order (standard, then fast), exe early returns, stops only if previous request state set.
- 3387da996 RequestNewPlayerCarEngineSound: cancels the existing sound of that type first (was leaked, kept playing); AC start position percent*(1-t) (lerp() differed in the last bit, 2 of 1288 inputs).
- caf1687f1 ProcessBoatMovingOverWater: FrqSpeedFactor exe float 0x3E4CCCCC.
- e0b19cf98 ProcessBoatEngine: tests bBoatEngineInWater (bit 1), on-water factor 0x3ECCCCCC, exe sum association, no m_State write.
- 75c9486fc ProcessTrainTrackSound: speed factor clamped to 1.0; silent cases still pass the frequency.
- 0a7b65660 PlayAircraftSound: new sound no longer gets m_EventVolume added.
- e626eeec6 ProcessDummyTrainEngine (shared helpers): train speed negated when NOT clockwise (was inverted); pad factor x(1/255 float).
- 45ab4d662 ProcessPlayerTrainBrakes: own constants (9.0, 0.68..1.0), pedal selection (brake forward / accelerate backward / none when slow), 1/255 multiply, exe piecewise table.
- (last) ProcessPropOrJetStall (0x4FD76B/0x4FDE47/0x5018A9): silent-stall slow interval 400..410 (0x8CBDBC/0x8CBDC0).

MATCH: Constructor, Destructor, UpdateParameters, UpdateGenericVehicleSound, StartVehicleEngineSound, GetAircraftNearPosition, ProcessAircraft.
Notes / not changed: ProcessTrainTrackSound etc. drive-type 'other than 4/F/R' in skidding calls NonDrive where the exe adds 0 (unreachable, handling.cfg only has 4/F/R). PlayTrainBrakeSound(-1) is called twice in Terminate (before the engine stops in the port, after in the exe); harmless. RequestNewPlayerCarEngineSound / PlayAircraftSound keep NOTSA_UNREACHABLE for st values the exe would use uninitialised sound memory for. ProcessDummyTrainEngine: bank prologue, tilt, config constants and final formula compared; ProcessBoatEngine/DummyTrain steps use GenericFrameRate-gated step helpers that equal the exe with the fix off.
Gate: see the report line appended after the final runs.
Gate: StandaloneRelease + StandaloneRW built after the last code edit; wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=compact exe) 1142 functions, mismatches PC24 0 / PC53 0; PortableWin64 (x64 cl) compile of AEVehicleAudioEntity.cpp 0 errors; v6 pre-commit grep clean; combat regress PASS=14 (run once at the end).
