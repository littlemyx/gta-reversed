# ASM-S19e (Trailer.cpp, Train.cpp, Vehicle.cpp; 92 rows)

Partial. 8 rows touched (3 FIXED, 4 MATCH, 1 PARTIAL); 84 rows NOT_REACHED (budget). Result table: S19e.result.tsv. Continue in S19f from CVehicle::DoVehicleLights (0x6E1A60, the port restructured it with early returns: needs a full asm pass, the corona calls already go through the fixed DoLightEffectImpl), FirePlaneGuns (0x6D4D30), DoDriveByShootings (0x741FD0), the rest of CVehicle in the S19e.tsv order, Trailer, then CTrain::MarkSurroundingEntitiesForCollisionWithTrain (sub-ulp sector clamp).

| Function | Verdict |
|---|---|
| CVehicle::PreRender 0x6D6480 | MATCH |
| CVehicle::ProcessWeapons 0x6E3950 | MATCH (status 0/8 = player, jump table 1..4 = guns/bomb/lock-on/rockets, mission 35/59/63 list, heli 0xA0C / plane 0x9D4 fx loop; SelectPlaneWeapon leaves edx = status) |
| CVehicle::InflictDamage 0x6D7C90 | MATCH (all 410 lines: NaN polarities incl. the min at 0x404330, tyre chance tables 0x6D82C0/D0, rand order, gas-cap x87 chain, crime ids 12/16/20, max-passenger loop) |
| CVehicle::ReactToVehicleDamage 0x6D5490 | FIXED 99fcabde: the exe draws the duration (rand) BEFORE the coin flip and draws both durations even with an empty car; port flipped first and only drew durations when someone looked |
| CVehicle::DoHeadLightEffect 0x6E0A50 / DoTailLightEffect 0x6E1780 | FIXED 60e39876: corona size is `((1 - dist/150) * rotMult) * angle` (0.4 front, 0.2 rear); the port multiplied angle first then the constant (1-ulp). Everything else matches: zero secondary-dummy test, 0.05 fwd offset, x - 2x for the left light, z,y,x dot on -fwd for the tail, rear bumper panel 6 for Stallion/Sabre, brake test, 96/128 red, halogen colors, corona ids, NaN polarities, return value (front true unless disabled, rear = braking) |
| CVehicle::SetUpDriver 0x6D1A50 | MATCH |
| CVehicle::SetupPassenger 0x6D1AA0 | PARTIAL: 0x6D1AA0-0x6D1B7A match (RemovePassenger(null) in the exe is a no-op); the rest unread |

## Tests
StandaloneRelease gta_reversed.exe rebuilt (1186/1186, Vehicle.cpp). wave2_oracle_test (RW_EXE_ORACLE and GTA_EXE = gta_sa_compact.exe): 1142 functions, 0 mismatches PC24 and PC53. gameplay_regress combat (-b worktree StandaloneRelease): PASS=14. Not run: soak, PortableWin64 (no new double/ftol spellings; pre-commit grep clean).
