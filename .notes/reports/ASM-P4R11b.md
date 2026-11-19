# ASM-P4R11b

Review of P4R11 fixes e629899ba, b3015a4a8, 93afb7744 against the exe: all three CONFIRMED (formulas, strictness, draw order).
CGeneral::RandomBool has no callers left (only the definition); every former call site carries an exe-address comment and was re-checked (P4R11b.result.tsv): all MATCH.
Broader sweep: static draw-count heuristic (exe call sites of 0x821B1E/0x407180/0x41BD90 vs port tokens, 379 functions) gave 100 count differences; 12 CarCtrl/Automobile/Task ones inspected, all explained by inlined helpers or compiler-merged cases. No port bug found, no code changes.
Open (cosmetic): CAutomobile 0x6A9B25 first turn-speed draw is rounded to float in the exe before *0.08-0.04; port keeps it unrounded.
Not reached: Audio, BreakObject, Clouds, Interior, Population, Plane, Vehicle.cpp, Fx, Cam rows of the heuristic list.
