# ASM-S19c (Trailer.cpp, Train.cpp, Vehicle.cpp; 104 rows + Task 0)

Partial. Compared ProcessControl (Task 0 polarity pass), DoTrainGenerationAndRemoval (FIXED), TrainHitStuff (MATCH), CreateMissionTrain (MATCH); 101 rows NOT_REACHED (budget). Result table: S19c.result.tsv. Continue in S19d from CTrain::AddNearbyPedAsRandomPassenger (0x6F8170), then the rest of CTrain, then the CVehicle per-frame functions in the S19 order (PreRender, ProcessWeapons, InflictDamage, ReactToVehicleDamage, DoVehicleLights/DoHeadLight*, DoTailLightEffect, FirePlaneGuns, DoDriveByShootings), then Trailer.

## Task 0: CTrain::ProcessControl NaN-edge polarities (commit baee15d3)
Method: every exe `fcom/fcomp + fnstsw + test ah,M + jcc` was mapped to the predicate that runs the body (test 5/jp: body on a<b; test 5/jnp: skip on a<b; test 0x41/jne: body on a>b; test 0x41/je: body on !(a>b); test 1/jne: body on a>=b; fcompp operand order resolved from the stack). MSVC (this toolchain, /O2 /arch:IA32 /fp:precise) was checked with a runtime NaN probe: `a>=b`, `a<=b`, `a>b`, `a<b` are false for NaN, `!(a<b)` etc. are true, so the source form decides the branch.

| Site (exe) | Port had | Now |
|---|---|---|
| 0x6F8984 stop speed vs FindMaximumSpeedToStopAtStations | `>=` | `!(speed < max)` |
| 0x6F89B6 / 0x6F89EF gas/brake clamp to 1 | `>= 1.0f` | `!(x < 1.0f)` |
| 0x6F8AA1 forbidden crossing dot (max vs min) | `<= 0` | `!(dot > 0)` |
| 0x6F8BB6 brake: abs speed vs fBreak | `>=` | `!(speed < fBreak)` |
| 0x6F91B5 node direction pick | `cur <= rail` | `!(cur > rail)` |
| 0x6F962A heading wrap | `<= PI` else-branch | `> PI` / `< -PI` chain |
| 0x6F9795-0x6F98B4 move/turn speed clamps | std::clamp | `!(v > lo) -> lo; !(v < hi) -> hi` |
| 0x6F9B42/0x6F9B83 force/torque vs max (fcompp, test 1) | `sq > max` | `!(max >= sq)` |
| 0x6F9B98 moving speed | `>= max` | `!(speed < max)` |
| 0x6F9CD5 buoyancy time step | `>= 0.01f` | `!(0.01f > ts)` |

Verified unchanged (already the exe polarity): every `<`-body site (0x6F87B7, 0x6F87C0, 0x6F8A6A, 0x6F8B18, 0x6F8B89, 0x6F8BC7, 0x6F8C59, 0x6F8DAF, rail-distance wrap loops, node-step loop, interpolation > total, damage > 0, speed > 0, 0.95 and 1.0 thresholds).

## CTrain::DoTrainGenerationAndRemoval 0x6F7900 (all 613 lines) - commit 8760cacd
| Deviation | Exe | Fix |
|---|---|---|
| station distance < 60 used float-rounded differences/sum/root | 0x6F7941-0x6F795E: unrounded ext difference, squares, sum, root, compared unrounded | `DistRaw` (x87d) |
| camera-removal distance < 220 | 0x6F7A70-0x6F7AA6: both differences stored as floats, sum/root unrounded | `DistRoundedXY` |
| status-1 node dist > 60, Unity slowdown < 300, step distances | x difference stored as float, y unrounded (0x6F7BB8, 0x6F7C93, 0x6F7F48) | `DistRoundedX` |
| generation-node pick | dist stored as float ([esp+14]) but the range compare uses the unrounded value; the first `< 170` loop test reads the stored float, later tests the unrounded step value; final `> 220` / `< 220` read the stored float | `distRaw` + `keepGoing` loops |
| NaN branches | tunnel z test (0x6F7E59) takes the check on !(z+6 > playerZ); track-1 range return (0x6F805F) on !(d < range) | `!( )` forms |

Checked and matching: 3000 ms / interval gating, pool walk down with free-slot and type-6 checks, front/mission flag filter, FindPlayerVehicle before the distance, Remove + virtual delete chain, random-train disable, status 0/1 split, rand()%nodes, tunnel attribute masks (0x880 player, 0x800 node), direction rand&1, node stepping (idiv for +1, wrap for -1), LastConfig (+1)&7, config 8+(rand&1), model request/loaded loop (flag 8), CreateMissionTrain arguments, speed rand%30+15 / %7+7, speed/2, min with FindMaximumSpeedToStopAtStations, SetTrainSpeed, cruise byte, deletable calls, status reset. (An exe quirk, the generation node written from a stale stack slot when the first distance is already >= 170, is unreachable: that path needs dist < range <= 100.)

## TrainHitStuff 0x6F5CF0: MATCH. CreateMissionTrain 0x6F7550: MATCH
CreateMissionTrain: rail distance from uint16 * (1/3), constructor args, translate, node/status/lock/flags/track byte, mission placement and bStopsAtStations, +-bbox length with float accumulation, link flags, both RegisterReference calls, ProcessControl, Remove/Add loop, tram test, AddPedInCar + decision maker 6, streak flag on every carriage.

## Tests
StandaloneRelease gta_reversed.exe builds (1186/1186, then the Train.cpp rebuild after the second commit). wave2_oracle_test (RW_EXE_ORACLE and GTA_EXE = gta_sa_compact.exe): 1142 functions, 0 mismatches PC24 and PC53. gameplay_regress combat (-b worktree StandaloneRelease): PASS=14. Not run: soak, PortableWin64 (Train.cpp only uses x87d/X87R spellings; the pre-commit grep is clean).
