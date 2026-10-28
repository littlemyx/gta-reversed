# ASM-S18d (leftovers of S18c: SearchLightCone, PreRender, ProcessFlyingCarStuff) - 2026-10-28
Result: 3 functions: 1 FIXED, 1 MATCH, 1 PARTIAL (token budget). Table in S18d.result.tsv.
Fix (commit 328b2d35): CPlane::ProcessFlyingCarStuff (exe 0x6CB7C0-0x6CC4B0): the camera distance^2 gating the smoke particle (compare with 6400, exe 0x6CBE6C..) is summed (dz^2+dy^2)+dx^2 on the x87 stack; the port summed x,y,z, so the compare could differ by 1 ULP.
MATCH: CHeli::SearchLightCone (0x6C58E0-0x6C6520): whole function incl. helpers CrossProduct 0x59C730 and Normalise 0x59C910 (both equal to the port helpers), colour loop (unrolled x4 in the exe, same per-vertex maths), shadow call arguments.
PARTIAL: CPlane::PreRender - compared 0x6C94A0-0x6C9FD9 (wheels, wheel matrices, prop angle, control surfaces, propeller passes, gear/misc model switch via jump table 0x6CAAE0/0x6CAAF8, Andromada, Hydra nozzles and dust): equal.
OPEN for a follow-up (S18e): CPlane::PreRender 0x6C9FD9-0x6CAB70 (Hydra jet-thrust fx loop 0x6C9FD9-0x6CA2A0, Vortex hover skirt, bouncing panels 0x6CA638..).
Verification: StandaloneRelease built (1186/1186); StandaloneRW build and oracle test not run (one-line float-sum reorder, budget).
Consumer audit: no signature/layout change.
