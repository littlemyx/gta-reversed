# ASM-S22h - CarCtrl.cpp: GenerateOneRandomCar (exe 0x430050-0x431F3A)

Result file: `.notes/asm_sweep/S22h.result.tsv`. Counts: 1 MATCH, 0 DEVIATION, 0 UNSURE. No code changed (so no oracle / gameplay_regress run was needed).

All 2169 asm lines compared in 8 chunks against the port blocks (main body + ChooseRandomCarModel, ChooseRandomCarSearchDirection, InitRandomCarMission, OrientRandomCar, PlaceRandomCarOnCurve, SnapRandomCarToGround, SetRandomCarStatus, IsRandomCarPositionOK):

1. 0x430050-0x4302D1 setup, car-count gates, police/ChooseModel, LA riots: fimul/fmul order, `<=` polarity incl. NaN, 5000/8000 ms law-enforcer timer, `rand()&0x7F < 0x37`.
2. 0x4302D1-0x430465 search direction + GenerateCarCreationCoors2 call: jump tables 0x431F3C/0x431F4C decoded, `inv*vy`/`vx*inv` order, 13 pushed args mapped one by one. The port's `nodeA` is the exe's node at stack slot +0x4C (param 9), `nodeB` is slot +0x48 (param 10); the port names are consistent with every later use (water flag, link scan, navi link).
3. 0x430465-0x430709 spawn probability, boat/predator (RequestModel flags 8), FindObjectsKindaColliding flags, link scan, lane counts, bike/bus/coach gates, zone pop types 17..19.
4. 0x430709-0x430943 InitRandomCarMission (all min/max pairs and ftol points), FBI colours, siren, lane `rand()%lanes`, mad-driver chance (80/50/10/200), cheat index 31.
5. 0x430943-0x430C80 OrientRandomCar: halfLen, `0.5*dist<halfLen`, ratio clamps, NodeLess comparators (0x420980), matrix right/forward/up, Normalise.
6. 0x430C80-0x4315CB PlaceRandomCarOnCurve: d1..d7 float/extended mix, t clamp, dir*0.01*sign, 5.4 lane factor, BMX +1.458, highway bits, speed mult, `1000/speed` ftol, `now - t*scale` ftol, CalcSpeedScaleFactor/CalcCurvePoint argument order.
7. 0x4315CB-0x43194C SnapRandomCarToGround, status bits: zA rounding, 1e9, 7.0, abs compares, GetWaterLevel / ProcessVerticalLine arg lists, hover height.
8. 0x43194C-0x431F3A tail: clump alpha, hydraulics cheat, on/off-screen distance gates (170/150/120, 1/170*45), relative-speed dot, colour, CWorld::Add, tractor/combine/BMX /3, riots health, damage jump table (bytes at 0x431F68), bike driving style, mad-driver chase timer (NaN skips), convoy models, SetUpDriverAndPassengersForVehicle args, +10 speed ftol, forward*speed*0.02, ChangeLawEnforcerState, model times-used cap 120.

Helpers checked against the exe: 0x420980 (NodeAddress <), 0x420920 (==), 0x420a60 / 0x420a10 (navi/node position), 0x44db00 (lane offset), 0x424130 (speed multiplier returns exact float constants), 0x4082c0 (Magnitude: (x2+y2)+z2), 0x4119d0 (divide), 0x421120, 0x420800, 0x41ccb0.

Nothing left open.
