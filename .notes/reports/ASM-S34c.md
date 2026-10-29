# ASM-S34c (Interior_c furnishing, 4 big functions) - 2026-10-29
Result: 4 compared by disassembly, 4 MATCH, 0 DEVIATION, 0 UNSURE. Table in S34c.result.tsv.
Compared (full range): FurnishBedroom (+ Bedroom_AddTableItem), Kitchen_FurnishEdges, FurnishLounge, FurnishOffice (+ Office_FurnishCenter).
Checked in each: rand() count and order (RandBelow(n) == ftol(r*(1/32768)*n); `_ftol(r*-k)` forms == base - RandBelow(-k); raw `rand() < 0x3FFF` calls without the & 0xFFFF; which roll is drawn before an early exit), tile index maths and signed/unsigned (int8 door/window ends compared with jg, uint8 status with jb), loop bounds and directions, furniture group/subgroup ids, rotation/offset constants (0.05, 1.05, 45/90/270/315, 0.5 adds), argument order of PlaceFurnitureOnWall / PlaceFurniture / AddInteriorInfo / PlaceObject read off the pushes, the 4-rotation tile tables, fcomp polarity (`test ah,0x44; jp` means "taken == 0" in the Kitchen spot loop), the 30-try spot-pick bug.
Notes (no code change):
- Uninitialised exe locals (bedRot/bedPos in the bedroom, cornerRot / sofa rot+pos in the lounge) when a piece cannot be placed: the port zero/-1 initialises (comment already in code); the exe value is stack garbage and not reproducible.
- Kitchen counter spots: the port caps the arrays at 32, the exe overflows into the neighbouring spot arrays past 32 spots (needs a very large kitchen; none known).
- FurnishOffice tail-jumps to Shop_FurnishCeiling (a bare ret); the port omits the call.
Gate: no code change.
