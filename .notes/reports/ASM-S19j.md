# ASM-S19j (Vehicle.cpp rows; 53 rows)

Partial. 6 rows compared (4 DEVIATION fixed in commit 1da207b2, 1 MATCH, 1 UNSURE = partly compared), 47 NOT_REACHED (budget). Result table: S19j.result.tsv.
Not reached: RemoveDriver 0x6D1950, the rest of AddSingleWheelParticles (spinning / skidding / fixed branches 0x6DEC3C-0x6DF3B9), all ctor/dtor/upgrade/render rows, ProcessBoatControl preamble 0x6DBCE0-0x6DBF0A.

| Function | Result |
|---|---|
| DoBladeCollision 0x6E2E50 | The blade centre is transformed by the `matrix` argument (port: this vehicle's matrix); sector range clamped `max(v,0)` / `min(v,120)` before the y-outer / x-inner loops; rotorType 0 or outside -3..3 leaves the cube (port asserted); `m_pSpheres` zeroed at exit. Bounding box, sphere, list order (Buildings, Vehicles, Peds with damage 0, Objects) equal. |
| FireFixedMachineGuns 0x73DF00 | MATCH (sqrt / 0.1 clamp / 1/len, float copies for the 2nd gun, rand order r1,r2,r3, jitter roundings, aiming + shot args, 20-round reload with +1400 ms). |
| AddDamagedVehicleParticles 0x6D2A80 | `health >= 650` is `jp` after FCOMP: NaN kills the fx too; port used `>=`. Rest equal (250 compare lets NaN through, engine-type string, const time 1-(h-250)*0.0025, velocity add). |
| AddWaterSplashParticles 0x6DDF60 | Distance gate summed z,y,x unrounded, returns only for `sq > 100` (NaN continues; port: IsPointInSphere). Particle count is a signed int (`1 - ftol(rain*-2)`; port used size_t, wraps for rain < 0). ONE random pair per particle: `u = (float)(r1/32767)`, `t = (r2/32767)*u` unrounded, `pos = v0 + e01*u + e12*t`; the port drew two independent randoms. Roundings: x/y `(float)((float)(e01*u + v0) + e12*t)`, z through float copies. |
| AddSingleWheelParticles 0x6DE880 | UNSURE: preamble (offscreen, z,y,x distance, 625/400/64 frame gates, arg2, water, arg3<1), sparks (condition, direction, force, AddSparks/ReportCollision args), MakeDirty, switch dispatch and the default branch incl. always-skid test equal. Spinning / skidding / fixed branches not compared. |
| SetDriver 0x6D16A0 | The port ended in AddPassenger's helper. The exe has its own tail: factor `mass*-0.02` for both vehicle types; bike = `ApplyMoveForce(up*s)` only (no turn force); others turn force (0,0,s) at `(driver.x-this.x, driver.y-this.y, 0)` from the raw matrix positions. Enforcer armour max keeps the armour for NaN. Freebies jump table (models 416/420/438/427/457/588/596-599) equal. |

## Tests
StandaloneRelease gta_reversed.exe rebuilt (0 errors). Pre-commit grep clean. wave2_oracle_test (RW_EXE_ORACLE and GTA_EXE = gta_sa_compact.exe): 1142 functions, 0 mismatches PC24 and PC53. gameplay_regress combat (-b worktree StandaloneRelease): PASS=14, 0 FAIL. Not run: soak, PortableWin64.
