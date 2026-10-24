# RD-NOISE state (cap reached 08:53)
CONTINUE: branch origin/rd-noise (33e119bb + this note). Done: see RD-NOISE.md. Next, in order:
1. gate: Release soak (NOVSYNC 3 min) + run_all_tests on branch rd-noise (the GenerateRandomCars tail is new behaviour: trains, planes, emergency services cars).
2. measure pin 128 (audio partition): run.sh intro --reps 3 --pin 511 --frames 3000 (RD_PX must point at a proxy built from this tree: build_orig.sh <dir>); expected: the orig-orig 1520/2177 single-call divergences (ResolveProbability) disappear.
3. port-port frame 7-9: one extra CGeneral::RandomBool (General.cpp:216) at the tail of the frame; run with --rfrom 4 --rlog 14 (RD_RFROM/NOTSA_LOCKSTEP_RFROM env) and resolve with resolve_stack.py (second +rva = RandomBool's caller).
4. A-B: scr hash at frame 1, rand count at frame 3 (189 vs 163).
Scratch: C:/gta-scratch/rdd/rdn3 (3x3000 traces), rdn4/rdn5 (R logs), px_rdn3 (orig proxy with all pins). Never edit run.sh while it runs.
