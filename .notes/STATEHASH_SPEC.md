# Statehash spec (SH1) - reproduce the per-frame hash bit-for-bit

Source of truth: `source/portable/statehash.cpp` (cited as `sh.cpp:LINE`; line numbers of the pre-SH1 file), hooks in `source/app/app_game.cpp`, `source/portable/msvc_rand.h`,
`source/standalone/GameRand.h`. Builds only with `-DGTASA_STATEHASH=ON` (NOTSA_STATEHASH). Same code for the Win32 and the portable (arm64) builds.

## 1. Hash primitive (sh.cpp:82-96)
* FNV-1a 64: offset `0xcbf29ce484222325`, prime `0x100000001b3`. Input is a stream of **32-bit words**, each fed as 4 bytes **little-endian**
  (`for i in 0..3: h ^= (v >> 8*i) & 0xFF; h *= prime`, uint64 wrap).
* Every accumulator `Acc` holds TWO hashes fed in parallel: `e` (exact) and `q` (quantised). Integers are fed identically to both.
* `U(v)`: feed uint32 `v` to e and q. Signed ints / enums are cast to uint32 (two's complement, sign-extended from the field type; unsigned 8/16-bit zero-extend; bool 0/1).
* `F(f)` (float32): `e` gets the IEEE-754 bit pattern as-is (NaN payloads and -0 are NOT canonicalised: -0 != +0 in `e`);
  `q` gets, if `isfinite(f) && |f| < 8e6f`, `(uint32)(int32)lround(f * 256.0f)` (f*256 is exact in float; lround = round half away from zero), else `0x7FFFFFFF` (NaN, Inf, huge).
  `q` hides x87-vs-SSE rounding drift; a real behavioural divergence shows in both.
* `Mix(o)`: feed `(uint32)o.e` then `(uint32)(o.e>>32)` into this.e; likewise o.q into this.q (sub-hash folding).
* A fresh `Acc` that got no field is `cbf29ce484222325` (both e and q); so an empty subsystem hashes to that.

## 2. When (sh.cpp:416-477, app_game.cpp:339-341)
`notsa::rr::OnFrame()` runs once per **Idle** frame at the end of the simulation part of `Idle()`: after `CTimer::Update(); CGame::Process();` and
`AudioEngine.Service()` (skipped with NOTSA_RR_NOAUDIO=1), before rendering. FrontendIdle frames are not hashed.
Recording starts with the injector command `cmd:rr_start[:SEED]` (does `srand(SEED)`, `g_frame = 0`); each OnFrame while recording writes frame `g_frame` then `g_frame++`
(0-based, counted in recorded frames, NOT CTimer frames). Stops at NOTSA_RR_MAXFRAMES / `cmd:rr_stop` / exit.
NOTSA_RR=1 pins: virtual clock (NOTSA_RR_STEP_MS=20 ms per Idle), sync CdStream, `srand(NOTSA_RR_SEED)` at GameInit (app_game.cpp:62), screen 1280x720 (NOTSA_RR_SCREEN),
zeroed `operator new` and rwMalloc (sh.cpp:36-48, 385-397).

## 3. Subsystems, in hash order (sh.cpp:221-289). Each is one `Acc`; fields fed in the listed order. 9 subsystems + `all`.
U = uint32 word, F = float32.

### 3.1 `tm` timer (sh.cpp:228-232) - 7 words
fc `CTimer::GetFrameCounter()` uint32 U; ms `GetTimeInMS()` uint32 U; msnc `GetTimeInMSNonClipped()` uint32 U; mspause `GetTimeInMSPauseMode()` uint32 U;
step `GetTimeStep()` float F; scale `GetTimeScale()` float F; paused `GetIsPaused()` bool U.

### 3.2 `ck` clock + weather (sh.cpp:233-237) - 6 words
h `GetGameClockHours()` uint8 U; m `GetGameClockMinutes()` uint8 U; s `GetGameClockSeconds()` uint16 U; d `GetGameClockDays()` U;
wnew `CWeather::NewWeatherType` enum U; wold `CWeather::OldWeatherType` enum U.

### 3.3 `rng` (sh.cpp:238, 271) - 1 word
`state` = current rand LCG state (the value the NEXT rand() steps from). Portable: main thread `msvc_crt::HoldRand()`; Win32 standalone: `notsa::g_RandSeed`; else 0 (sh.cpp:115-123).
LCG (msvc_rand.h, GameRand.h): `state = state*214013 + 2531011 (uint32); return (state>>16)&0x7FFF`; `srand(s)`: `state = s`.
Initial state = LCG^19(1) (19 rand() calls of the exe's static initialisers).

### 3.4 `pop` world counters (sh.cpp:242-248) - 12 words
peds `CPopulation::ms_nTotalPeds` uint32; civ `ms_nTotalCivPeds` uint32; gang `ms_nTotalGangPeds` uint32; cop `ms_nNumCop` uint32;
rcars `CCarCtrl::NumRandomCars` int32; law `NumLawEnforcerCars` uint32; mcars `NumMissionCars` int32; parked `NumParkedCars` uint32;
stmem `CStreaming::ms_memoryUsedBytes` uint32; streq `CStreaming::ms_numModelsRequested` int32;
npedpool `GetPedPool()->GetNoOfUsedSpaces()` (0 if no pool); nvehpool `GetVehiclePool()->GetNoOfUsedSpaces()`. All U.

### 3.5 `pl` player (sh.cpp:250-259) - 35 words (matrix case)
Only if `FindPlayerPed() != null`, else the Acc stays empty (cbf29ce484222325). Fields = `PedFields(ped)` (4.2; NO "slot" word), then
wl `FindPlayerWanted()->GetWantedLevel()` (-1 if no wanted) U; chaos `m_ChaosLevel` (-1 if none) U; money `FindPlayerInfo().m_nMoney` int32 U.

### 3.6 `cam` (sh.cpp:260-263) - 6 words
p x,y,z = `TheCamera.GetPosition()`; f x,y,z = `TheCamera.GetForward()`; 6 x F.

### 3.7 `pd` / `vh` / `ob` pools (sh.cpp:178-203, 266-268): `GetPedPool()`, `GetVehiclePool()`, `GetObjectPool()`
`total = Acc{}`. For `i = 0 .. pool->GetSize()-1` (slot order, never pointer order; `GetAt(i) == null` free slots skipped):
`s = Acc{}`; feed `U(i)` ("slot"), then the entity field list (4.x); `total.Mix(s)`. Subsystem hash = `total` (e and q).
`n` = number of non-null slots (printed in the RR trace line only, not hashed). Null pool -> empty `total`.
NOT hashed: buildings, dummies, pickups, script state/globals, streaming order, audio, particles, collision caches, any pointer values, ped/vehicle fields not listed below.

### 3.8 `all` (sh.cpp:270-273)
`all = Acc{}`; `all.Mix(part)` for the parts in this order: `tm, ck, rngA, pop, pl, cam, peds, vehs, objs` (rngA = the 3.3 Acc `{U(rng)}`; pl/peds/... are the Accs above, e and q). 9 parts.

## 4. Entity field lists
### 4.1 Physical (sh.cpp:132-143), first part of ped / vehicle / object: 20 words with a matrix (14 without)
model `m_nModelIndex` uint16 U; status `GetStatus()` (5-bit enum bitfield) U;
p x,y,z = `GetPosition()` = `m_matrix ? m_matrix->pos : m_placement.m_vPosn` (3 F);
if `m_matrix != null`: r x,y,z `GetRight()`, f x,y,z `GetForward()`, u x,y,z `GetUp()` (9 F) else `hd` = `m_placement.m_fHeading` (1 F);
mv x,y,z `m_vecMoveSpeed` (3 F); tv x,y,z `m_vecTurnSpeed` (3 F).
(matrix case: 2U + 3F + 9F + 6F = 20 words; no matrix: 2 + 3 + 1 + 6 = 12 words.)
### 4.2 Ped (sh.cpp:145-162) = Physical, then 12 words
ptype `m_nPedType` U; pstate `m_nPedState` U; mstate `m_nMoveState` U; hp `m_fHealth` F; arm `m_fArmour` F; rot `m_fCurrentRotation` F; aim `m_fAimingRotation` F;
veh `m_pVehicle ? GetVehiclePool()->GetIndex(m_pVehicle) : -1` (int32 as U); invh `bInVehicle` 0/1 U;
wtype `GetActiveWeapon().GetType()` U; ammo `GetActiveWeapon().GetTotalAmmo()` U;
tasks: `chain=0; n=0; for (t = GetIntelligence()->GetTaskManager().GetActiveTask(); t && n<6; t = t->GetSubTask(), n++) chain = chain*131 + (uint32)t->GetTaskType() + 1;` (uint32 wrap) U.
(12 words: ptype pstate mstate hp arm rot aim veh invh wtype ammo tasks.)
### 4.3 Vehicle (sh.cpp:164-174) = Physical, then 8 words
hp `m_fHealth` F; gas `m_GasPedal` F; brk `m_BrakePedal` F; steer `m_fSteerAngle` F; drv `m_pDriver ? GetPedPool()->GetIndex(m_pDriver) : -1` U;
mission `m_autoPilot.m_nCarMission` U; tempact `m_autoPilot.m_nTempAction` U; col `m_nPrimaryColor | (m_nSecondaryColor << 8)` U (both uint8 fields).
### 4.4 Object (sh.cpp:176) = Physical only.
Per entity words (matrix case): ped 1+20+12 = 33, vehicle 1+20+8 = 29, object 1+20 = 21. Total distinct named fields: tm 7 + ck 6 + rng 1 + pop 12 + pl (Physical 20 + ped 12 + 3 = 35) + cam 6 + 3 pool field lists.

## 5. Existing on-disk format: RR trace (kept) - NOTSA_RR_TRACE=<file>
Header `#RR 1 build=<tag> seed=<n> step_ms=<n> float=win32|portable`; per frame:
`F <frame> gf=<CTimer frame counter> rng=%08x tm=<e> ck=<e> pop=<e> pl=<e>/<q> cam=<e>/<q> pd=<n>:<e>/<q> vh=<n>:<e>/<q> ob=<n>:<e>/<q> all=<e>/<q>` (16-hex 64-bit values; note rng/tm/ck/pop show e only).
NOTSA_RR_DETAIL=A-B adds `D` lines (per subsystem `D <frame> <sub> name=hex ... f.name=<float bits>`, and per entity `D <frame> ped|veh|obj <slot> slot=.. model=.. ...`). Diff tool: tools/portable/rr_diff.py.

## 6. NEW per-frame hash file (SH1) - NOTSA_SH_FILE=<path> (works with NOTSA_RR=1 + cmd:rr_start; same hook, same frame numbering)
One line per frame, space separated, lowercase hex without prefix:
`frame randCount h_tm h_ck h_rng h_pop h_pl h_cam h_pd h_vh h_ob h_all`
* `frame`: decimal recorded frame number (0-based). `randCount`: decimal, cumulative number of `rand()` calls since process start at the END of that frame,
  the same counter as the rand log's `callidx` (the next call gets callidx == randCount). Process-wide.
* `h_*`: 64-bit `e` hash, 16 hex digits, of subsystems 3.1-3.8 in that order. (`q` remains in the RR trace only.)
* With NOTSA_RR_GAMEPLAY_RAND=1 (section 10) randCount excludes render/audio draws. First line `#SH 1 build=<tag> seed=<n> gprand=<0or1> subsys=tm,ck,rng,pop,pl,cam,pd,vh,ob,all` (skip lines starting with `#`).

## 7. NEW verbose dump (SH1) - NOTSA_SH_DUMP=<frame>[,<frame>...]  NOTSA_SH_DUMP_SUBSYS=<name|all>[,<name>...]  NOTSA_SH_DUMP_FILE=<path> (default sh_dump.txt)
Rows `frame subsys entity_key field value`, one row per word fed to the hash, in feed order (the checker relies on this order), for listed recorded-frame numbers.
`subsys` in `tm ck rng pop pl cam pd vh ob`; `all` selects all nine (the `all` hash needs all nine).
* entity_key: `-` for tm ck rng pop pl cam; `<slot>:<model>` for pd/vh/ob.
* value: `u:<hex>` for a U word, `f:<8-hex bits>:<%.9g>` for an F word (hex is authoritative).
`tools/standalone/statehash/shdump_check.py <dump> <hashfile>` re-hashes the rows following sections 1 and 3 and compares each dumped subsystem (and `all` when all nine dumped) with the hash file.

## 8. Rand call-site log (SH1) - NOTSA_SH_RANDLOG=<file> (alias NOTSA_RR_RANDLOG), independent of recording
Row per `rand()` call: `callidx frame value caller`. `callidx` = 0-based cumulative call number since process start (the per-frame hash file's `randCount` at the end of frame k = the callidx of the first
call of frame k+1 as long as recording is on); `frame` = recorded frame number the call belongs to (calls made before frame k's OnFrame belong to k), `-1` before `cmd:rr_start`; `value` = returned value 0..32767.
`srand` calls are not logged (they are visible as a jump in the `rng` hash; `srand(s)` sets state = s). Counted: every `rand()` macro use (GameRand.h / msvc_rand.h): main thread only on portable builds.
* Win32 caller = `file:line@RVA[+WRAPRVA]`: file:line is the source location of the `rand()` use (GameRand takes std::source_location), `RVA` = return address of the logging hook minus the module base,
  `WRAPRVA` = (only when file is General.cpp, i.e. via `CGeneral::GetRandomNumber`) its return address minus module base = the real caller. Resolve RVAs to function names offline:
  `py tools/standalone/statehash/randlog_resolve.py <log> <gta_reversed.map>` (map from a link with `/MAP`: `-DCMAKE_EXE_LINKER_FLAGS=/MAP`; no runtime symbolisation).
* portable caller = up to 5 frame-pointer return addresses relative to `notsa::rr::StableId`, hex, comma separated (resolve with nm/atos).
Rand sources: all gameplay/audio/render code draws through `rand()` (macro-redirected to the MSVC LCG) - grep finds no other source in the run build. Not routed (but only compiled under non-run options):
`std::random_device/mt19937` in General.h/General.cpp (`#ifdef BETTER_RNG`, static_assert'ed off), `std::mt19937` in GrassRenderer.cpp (`#if FIX_BUGS`). Time-seeded: `srand(RsTimer())` app_game.cpp:64 when NOTSA_RR is off
(RR pins `srand(NOTSA_RR_SEED)`). Address-seeded: `srand(NOTSA_PTR_ID32(this) + days)`-style keys and AngledRect.cpp:43 use `notsa::rr::StableId` (pool tag + slot, not the raw pointer) under STATEHASH.
Beware for a TS port: the rand stream is also advanced by RENDER-time code (GrassRenderer.cpp:85-87 `srand(ftol(seed))` + `GetRandomNumber()` per tuft) and by audio, so skipping rendering/audio changes the stream;
the RR runs use NOTSA_RR_NOAUDIO=1 (audio not serviced) but DO render (NOPRESENT only skips the swap).

## 9. Notes for SH2 (portable math / 64-bit reference build)
* The game is x87 (exe) vs SSE in the Win32 StandaloneRelease: `q` hashes quantise floats to 1/256 so benign drift is visible only in `e`. A TS port in float32 (Math.fround) will
  differ from `e` wherever the exe keeps x87 80-bit intermediates (the `x87d`/`X87R/X87W` sites, e.g. CGeneral::GetRandomNumberInRange float/int versions, General.h:150-170).
* Math functions the game calls (sinf/cosf/atan2f/sqrt/acos/pow/floor via CRT) need fdlibm-exact ports; ftol truncation helpers `NOTSA_FTOL*` define int conversion.
* Resource load order (streaming / IMG order, CdStream sync reads) influences model ids and pool slot allocation; pool slot order is part of the hash (slot words).

## 10. NOTSA_RR_GAMEPLAY_RAND=1 (render/audio isolation; the reference runs use it)
Needs NOTSA_RR=1. `notsa::rr::RandIsolate` (statehash.h) saves the rand state on entry and restores it (`srand(saved)`) on exit; rand() calls inside are NOT counted in `randCount` / `callidx` and NOT logged.
Drawing is unchanged (render/audio still consume random numbers privately); gameplay sees the stream as if they never ran. The hash file header carries `gprand=1`. Scopes (app_game.cpp, Idle):
1. `AudioEngine.Service()` (only when not NOTSA_RR_NOAUDIO=1, which skips it entirely);
2. everything after `OnFrame()` to the end of `Idle()`: lights, scene render, 2D stuff: CGrassRenderer.cpp:85-87 (`srand` + `GetRandomNumber` per tuft), PostEffects, Coronas, Clouds render, sprites, HUD, Fx render, ...
   Isolated as a whole, so there is no per-site list to maintain.
NOT isolated (they run inside `CGame::Process`, in simulation order, and a port must reproduce them although cosmetic): FxManager / FxSystem / FxEmitter updates and Fx blueprints (Fx.cpp, FxEmitter.cpp, FxInfo*),
CWeather::Update (21 sites), Clouds update, Birds, Fire / FireManager, Explosion, Glass, WaterCreature_c, WaterCannon, Rope, Population, CarCtrl, Train, Plane, Heli, ScriptsForBrains, SpecialFX update, Physical, Weapon,
WindModifiers, PlantMgr / PlantLocTri / ProcSurfaceInfo / PPTriPlantBuffer updates, audio ENTITY code called from game code (AEPedSpeech, AECollision, AEGlobalWeapon, AEVehicle, AEScript, AEAudioUtility) outside AudioEngine.Service.
Also not isolated: draws in the loading screen and FrontendIdle (menus). With gprand=1 `randCount` and the rand log therefore describe the gameplay stream only.

## 11. SH2: portable math (GTASA_RR_PORTABLE_MATH) and the 64-bit reference build
* Build: `tools/standalone/statehash/shbuild.sh` -> `build/PortableWin64RR/bin/gta_reversed.exe` (PortableWin64 preset = MSVC x64, SSE2, `/fp:precise`, no x87 code, `NOTSA_PORTABLE`; plus GTASA_STATEHASH, GTASA_SCMTRACE, GTASA_MDRIVER, GTASA_RR_PORTABLE_MATH). The x87 sites are the lifted portable branches (`NOTSA_PORTABLE`); floats are IEEE binary32 per operation like TS `Math.fround`.
* Math: `source/portable/v8_ieee754/` = V8 10.2.154.26 `src/base/ieee754.cc/.h` unmodified (see its README). `source/portable/rr_math.h` (force-included via StdInc.h, and via `/FI` for the librw target) turns
  `sin cos tan asin acos atan atan2 exp log log10 log2 log1p expm1 sinh cosh tanh cbrt pow` and the f forms (`sinf cosf tanf asinf acosf atanf atan2f expf logf log10f log2f sinhf coshf tanhf cbrtf powf`) into
  float arg(s): `(float)ieee754::f((double)x)`; double arg(s): `ieee754::f(x)`; integer args as double; mixed float/int or float/double -> double (the C++ overload rule). Covers `::f`, `std::f`, game code, CMaths / CGeneral helpers,
  the `x87::sin/cos/tan/atan/atan2/asin/acos/log2/log10/ln/sincos/pow` helper functions (X87Intrinsics.h / X87Pow.h take their `std::` branch under this option instead of the software x87 / exe CRT pow kernel) and librw (Quat exp/log/slerp, Matrix rotate, camera tan/atan, light angles).
* NOT routed, complete list of what I found:
  1. `sqrt sqrtf fabs floor ceil fmod modf round trunc ldexp frexp`: IEEE correctly rounded / exact in every libm. `x87::sqrt` is the soft-x87 sqrt at PC24 (kept; for float inputs it equals `Math.fround(Math.sqrt(x))`).
  2. The lifted x87 subroutines in `game_sa/Core/X87Sites.h` (`lift::Fpu::Sin/Cos/Patan/Yl2x`: RadarAngle* and 3 more helpers): exact software x87, still used. Radar / HUD side; nothing hashed reads them (not machine-verified).
  3. Math functions used without a call (address taken): none found by grep (only comments and variables named sin/cos/exp).
  4. `hypot`: 7 hits in source/, not routed, not audited. `<random>`: not compiled in the run build.
  5. Third-party code compiled without rr_math.h: spdlog/fmt/imgui/tracy/SDL3/libjpeg/ogg/vorbis, the CRT itself. Audio-side libm use does not run (NOTSA_RR_NOAUDIO=1).
  6. Compile-time constant folding of `sinf(constant)` by the compiler is not checked per TU.
* TS equivalent: `Math.fround(Math.f(x))` (float call) / `Math.f(x)` (double call) in Node 18.15: V8 10.2.154.26 implements Math.sin/cos/tan/asin/acos/atan/atan2/exp/log/log2/log10/log1p/expm1/cbrt/sinh/cosh/tanh/pow with exactly this ieee754.cc.

## 11. Verified (SH1)
Runs: tools/standalone/statehash/sh1_run.sh (route sh1_route.txt: New Game, 06:30, rr_start, 2000 frames, NOTSA_RR_GAMEPLAY_RAND=1). Two runs: hash files (2001 lines) and rand logs (1333158 rows) byte-identical; frame 1500 dump (2946 rows) re-hashes to every subsystem hash and `all` (shdump_check.py: 10 hashes, 0 mismatches). randCount at frame 0 = 362627 (boot/loading draws included).
