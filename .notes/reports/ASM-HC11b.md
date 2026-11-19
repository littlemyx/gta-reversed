# ASM-HC11b: compare-strictness / NaN class, continuation of HC11

Convention (re-derived): exe helpers 0x404330 `min(a,b)=a<b?a:b` and 0x420800 `max(a,b)=a>b?a:b` take a = LAST pushed argument ([esp+4]), b = the earlier one; NaN returns b. Port equivalent: std::min(b,a) / std::max(b,a) (note std::min(a,b)=b<a?b:a, std::max(a,b)=a<b?b:a return the FIRST argument for NaN). Also max(0,-0) = -0 in the exe, +0 with std::max(0,x).

Fixed (explicit ternary / CmpMax in exe operand order): CAutomobile::HydraulicControl (4x max(0,+-x), d4ef8da6a), CAutomobile::NitrousControl (max(0.25,1-gas), min(1,temp), 305d8dd2b), CBmx::ProcessAI (max(0.5,v), max(0,v-0.4), mult clamp [0,2] which gave 0 for NaN/-0 where the exe keeps it, a2b63e42b), CTrain::ProcessControl (gas, d37f6f02f), CVehicle::DoVehicleLights (zr350 pop-up step up/down; notsa::step_up_to/step_down_to use std::min/max, left unchanged for the audio users, d1341c4a6).

Verified MATCH: rest of HydraulicControl (max/min/clamp inlines), ProcessCarWheelPair, ProcessAI mouse clamp, UpdateMovingCollision, FlyingControl, Bike::ProcessAI, ProcessBikeWheel, ImproveNearClip, FollowCar max, PossiblyRemoveVehicle, GenerateOneRandomCar, PreRender, Garage::Update, ProcessDrivingAnims steer clamp (its 0x6DFE9E/0x6E024D helper sites are UpdateTrailerLink/UpdateTractorLink, port already explicit; not hit in RYDER_1; Say sites 0x5F1251/68 are also another, non-hit function).

Not reached: ~25 other float min/max/clamp inlines in hit functions (ProcessEntityCollision 1917/18, VehicleDamage, CalculateDriveAcceleration, ProcessBuoyancy x2, Bike::ProcessControl, PedCreationDistMultiplier, Weather, PlayerPed); NaN-only differences, none changes a non-NaN run.

Effect: only NaN / signed-zero operands differ; no behaviour change for finite non-zero-sign inputs. Build: StandaloneRelease links clean; no game run (regress left to the gate).
