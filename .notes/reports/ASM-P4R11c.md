# ASM-P4R11c

Rand sweep of the gameplay rows of the P4R11b draw-count list (Audio, Clouds, Fx, Cam skipped by brief).
Port-side counting was redone with lambda/helper-aware matching; each exe draw (0x821B1E / 0x407180 / 0x41BD90) was mapped to a port draw.
Result: 37 functions MATCH (all differences = inlined helpers, lambdas used several times, unrolled loops, duplicated switch/branch copies, or disasm overrunning into the next function), 1 UNSURE (CTaskComplexGangFollower::CreateNextSubTask, 12 exe draws vs 11 + 2 SaySomethingRandom calls, not mapped one-to-one), 7 equal-count rows not examined. No real extra/missing draw found, no code changes, no build needed.
