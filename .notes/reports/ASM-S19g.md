# ASM-S19g (Trailer.cpp, Vehicle.cpp; 77 rows)

Partial. 15 rows touched (6 FIXED, 8 MATCH, 1 UNSURE); 62 rows NOT_REACHED (budget). Result table: S19g.result.tsv. Continue in S19h with the CVehicle rows in S19g.tsv order from Constructor 0x6D5F10 (not reached: everything in S19g.tsv not listed in the result table as MATCH/FIXED/UNSURE), CTrailer::SetupSuspensionLines / SetTowLink (the source already carries earlier asm comments, not re-verified here) and the second half of AddExhaustParticles 0x6DE240.

| Function | Verdict |
|---|---|
| CVehicle::FireUnguidedMissile 0x6D5110, FireHeatSeakingMissile 0x6E05C0, GetPlaneWeaponFiringStatus 0x6E3440 (jump table), ScanAndMarkTargetForHeatSeekingMissile 0x6E0400 | MATCH |
| CVehicle::DoHeadLightReflectionSingle 0x6E1440 / Twin 0x6E1600 | FIXED f7bed935 |
| CVehicle::FillVehicleWithPeds 0x6E2900 | FIXED 2d77eb6b |
| CTrailer::ProcessSuspension 0x6CF6A0, ProcessAI 0x6CF590, PreRender 0x6CFAC0 | FIXED 523a2b24 |
| CTrailer::Constructor, ScanForTowLink, ProcessControl, BreakTowLink | MATCH |

## Deviations fixed
1. DoHeadLightReflectionSingle/Twin: the shared impl derived "twin" from the combine harvester model when called without TWIN, so Single on a combine drew the twin shadow (exe Single never does; the harvester rule lives in 0x6E1720 only). The single-light position is `right2D*x + (pos + fwd2D*od)` in the exe's x87 order (the port summed `fwd*od + right*x` first, a different float rounding); od = (1+2*size)+y stays unrounded.
2. FillVehicleWithPeds: the exe loops seats -1..max-1, requests the model per seat, DELETES the previous driver/passenger ped after creating the new one and seats passengers with isDriver = false; the port nulled the pointers (leaking the old peds) and passed true for every seat.
3. CTrailer::ProcessSuspension: the exe applies the spring impulse for both supports, then reads both contact speeds, then applies both dampenings; the port did spring/speed/dampening per support (support 0's speed missed support 1's spring impulse). The wheel-pair traction constant is the static-init value 0x3B83126E (0xC1CB28, `(1/2500)*10`), not 0.004f (0x3B83126F).
4. CTrailer::ProcessAI: the wheel-steer / brake copy branch is gated by status 10 (STATUS_IS_TOWED); the port tested STATUS_REMOTE_CONTROLLED (8) so a hitched trailer never copied the tractor's brake or (luggage trailers) steered. Headings are x87d now.
5. CTrailer::PreRender: the FARMTR1 propeller rotation wraps with a single `if (r > 2*PI) r -= 2*PI` (the rotation only decreases), not fmod (which also folded negative values).

## Tests
StandaloneRelease gta_reversed.exe rebuilt in the worktree (Vehicle.cpp, Trailer.cpp). wave2_oracle_test (RW_EXE_ORACLE and GTA_EXE = gta_sa_compact.exe): 1142 functions, 0 mismatches PC24 and PC53. gameplay_regress combat (-b worktree StandaloneRelease): PASS=14. Not run: soak, PortableWin64 (pre-commit grep clean).
