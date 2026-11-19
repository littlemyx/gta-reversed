# ASM-HC7c
Step 1: 1a2b5529e (CPed::UpdatePosition) CONFIRMED (exe 0x5E1EEC fcomp [0x863244], float 0x3D23D70B).
Step 2: tools/standalone/asm/constscreen.py (committed) screened 3236 functions with an exe address (145 without a locatable port body): exe .rdata float constants vs port float literals, 1-64 ulp near-misses. 9 functions / 11 constants were real misses; all fixed (see result tsv). Render/HUD/audio names were skipped by regex except CWeaponEffects::Render (0x865034 vs 0x3bc90fdb: not examined).
Not done: the per-row top-40 instruction-level compare (budget went to the screen); the screen does not catch constants the port lacks entirely or ones hidden behind named constants/helpers.
Step 3: ApplyCollision-1 RECONFIRMED; ApplyCollision-2 only call/constant skeleton checked; SphereCastVsCaches already fully MATCH in S69c.
Build: StandaloneRelease links with all changes.
