# ASM-S17d (Bike.cpp, Bmx.cpp, Boat.cpp, 10 functions left from S17c) - 2026-10-28
Result: 5 compared over their whole exe range, all MATCH; 0 FIXED, 0 UNSURE, 5 NOT REACHED (token budget; table in S17d.result.tsv). Slice is PARTIAL.
MATCH (asm range): CBmx::ProcessAI (0x6C1470-0x6C2007), CBike::ProcessAI (0x6BC930-0x6BCF7A), CBmx::ProcessDrivingAnims (0x6BFB50-0x6C038C), CBmx::PreRender (0x6C0810-0x6C146B), CBoat::PreRender (0x6F1180-0x6F176C).
Checked beyond control flow: both status jump tables (Bmx 10 entries, Bike 13 with GHOST sharing PHYSICS), flag bits (0x3DB, 0x428, 0x42A, 0x42B, 0x614), node offsets (Bmx 0x5A0+4*i, Boat 0x5B0+4*i), PEDSTATE 0x3D/0x3A, anim ids 0xC4-0xD0, x87 dot/sum orders, all kconst constants (0.7/0.4, 0.95, 0.97 pow, 5.092958, 0.02 via ExeRecip, 150-alpha clamp), the static-argument-order of ProcessWheelRotation (front steer vector, rear forward, front wheel size for the rear z, preserved as in the exe).
No FIX_BUGS or literal-division deviations found in these functions (the 1/255, 0.02f and 32/2PI factors are multiplies in both).
Not reached (5): CBike::ProcessControlInputs, ProcessRiderAnims, PreRender, ProcessControl; CBoat::ProcessControl.
No source changed, so no build/oracle/soak was needed for this slice; only the two note files are committed. Scratch: C:/gta-scratch/asm-s17d (bikeai.txt).
