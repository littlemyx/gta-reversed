# ASM-S16g (Automobile.cpp, last 6 functions) - 2026-10-28
Result: 3 compared over their whole exe range: 1 MATCH, 2 FIXED (2 commits); 0 UNSURE, 3 NOT REACHED (token budget; table in S16g.result.tsv). Slice is PARTIAL.
Fixes:
- HydraulicControl (0x6A07A0-0x6A1459), 40e8bfd0: SUSPENSION_OFF event fires when the angle counts down to 0 (port: 1); the normal-height horn branch ends at 0x6A13B8 and skips the per-frame suspension block (port ran it); maxDelta keeps the SIGNED delta when |delta| > |maxDelta| (port stored the absolute value, which changed the sign of f = (limitDiff+maxDelta)*0.5/limitDiff and so the audio events / setPrevRatio).
- TankControl (0x6AE850-0x6AF102), 02a7a0d5: RC Tiger gun point = Transform(gunPos) + ts*moveSpeed (port transformed the sum); Rhino camera-follow vertical doom rotation snaps to the target within one step (port left it unchanged); fire force scalar (-0.1*mass) rounded first; doomOffset.y = POS.y + (cos*dy + sin*dx); cos-branch horizontal limit = MIN - max(-1, cos*1.3)*(m3-MIN) (no separate < -1 branch).
- UpdateWheelMatrix (0x6AA290-0x6AAB2F): MATCH (node table, steer, scale, solid axle, hover particles, ground angle, atan2, 19 constants read from the exe).
Consumer audit: no signature/layout change; both functions are per-vehicle (ProcessControl callers only).
Not reached (3): ProcessBuoyancy (0x6A8C00, 705 asm lines), VehicleDamage (0x6A7650, 1039), PreRender (0x6AAB50, ~2300).
Gate: Release build OK (0 errors); no oracle target covers Automobile; soak (NOVSYNC, 3 min) RESULT: OK.
Scratch: C:/gta-scratch/asm-s16g (asm dumps, tools/kconst.py, patches, build.log, soak output).
