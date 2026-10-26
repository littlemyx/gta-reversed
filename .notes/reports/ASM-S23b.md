# ASM-S23b (Population.cpp second half, 13 functions) - 2026-10-26 18:36 .. see hand-back
Result: 12 compared (3 FIXED in 3 commits, 9 MATCH), 0 UNSURE left open, 1 NOT REACHED (AddPedAtAttractor 0x614210; token budget). Slice is PARTIAL (table in S23b.result.tsv).
Fixes (exe address, one line each):
- RemovePedsIfThePoolGetsFull (0x616300): the closest ped is picked among peds that pass CPed::CanBeDeleted (0x5DF100), scanning from the last slot with strict `<` on the real
  distance (float, start 1e7). The port took the closest ped of the whole pool, so with the pool nearly full (<8 free, every 8th frame) the PLAYER ped or a group member could be deleted.
- GeneratePedsAtAttractors (0x615970): entity area must equal currArea exactly (port used IsInCurrentArea = also area 13); effect radius test is min < dist < max on the sqrt float distance
  (port: squared, >=/<=); police-station roll is the int GetRandomNumberInRange(0,100) <= 70 (port: CGeneral::RandomBool = float range).
- ManageDummy (0x616000): the exe compares sqrt(z2+y2+x2) < limit (80/750), the port compared squared values (differs only at the 1-ulp boundary).
MATCH (checked line by line incl. constants, rand order, field offsets): AddToPopulation (couples, gangs, sunbathers/towels/toys, loot tasks, cop need), Update, ManagePopulation, ManageObject,
GeneratePedsAtStartOfGame, ConvertAllObjectsToDummyObjects, ConvertToRealObject, PopulateInterior, LoadSpecificDriverModelsForCar (jump table decoded).
Notes: FPU precision control is 24 bit (x87d == float), so float math with the exe's operation order is exact. PopulateInterior passes nullptr node addresses to GeneratePedCreationCoors_Interior
where the exe passes locals initialised to 0xFFFF (no observable effect expected, not changed).
Consumer audit: no shared state/signature changed; RemovePed is unchanged. CPed::CanBeDeleted already existed.
Gate: Release build OK; no oracle target exists for Population (D/S classes); soak result in the hand-back.
