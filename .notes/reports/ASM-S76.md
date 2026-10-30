# ASM-S76 (GenericGameStorage, ClothesBuilder, CustomRoadsignMgr)

Slice: 46 rows (`.notes/asm_sweep/S76.tsv`), results in `.notes/asm_sweep/S76.result.tsv`.

| verdict | rows |
|---|---|
| MATCH | 23 (GenericGameStorage 14, ClothesBuilder 8, RoadsignGenerateTextRaster 1) |
| FIXED | 3 (GenericGameStorage) |
| NOT_REACHED | 20 (ClothesBuilder 13, CCustomRoadsignMgr 7) |

## Fixes
- `LoadDataFromWorkBuffer` (exe 0x5D1300): the number of bytes taken from the current chunk before refilling is `0xC800 - pos` (the overflow test itself still uses the current chunk size 0x8D2BE0).
- `SaveDataToWorkBuffer` (exe 0x5D1270): the overflow test and the remainder use the constant 0xC800. The port used `ms_WorkBufferSize`, which a load leaves at the size of the last chunk read (49148 for a normal save): a save after a load in the same session flushed at different offsets (same bytes, but the final-chunk > 0xC7FC checksum double-add could differ).
- `GenericLoad` (exe 0x5D17B0): the exe calls `GetCurrentVersionNumber()` and throws the result away: there is no save-version check, every save loads. The port's check (and the `LoadSavesWithMismatchingVersion` config) is now `FIX_BUGS` only.

## Save format (byte compatibility with exe saves)
Block order from the exe jump tables (save 0x5D1734, load 0x5D1B00) equals the port's; 28 blocks, each preceded by the 5 byte tag `BLOCK`; simple variables struct is 0x138 bytes; chunks are 0xC800; file is padded to 0x317FC (202748) bytes; the trailer is a 4 byte sum of all bytes (byte adds, uint32), with the exe quirk that the chunk is summed again when the trailer does not fit and a recursive flush happens. `CheckDataNotCorrupt` re-sums all chunks and subtracts the last 4 bytes. All of that is identical in the port.

## Notes / open
- `ReportError`: the exe's header-mismatch call passes error 2 (SAVING in the port enum), the port passes LOADING; text goes to a log only.
- `AddWeightToBoneVertex`: when all 8 slots are used the exe adds to `weights[8]` (UB); the port reaches `NOTSA_UNREACHABLE` (logged by the run build). Needs more than 8 distinct nonzero-weight bones on one vertex.
- `StoreBoneArray`: exe stores a single `-1` sentinel after the last node, the port fills the whole row with -1 (only the tail behind the sentinel differs).
- Not compared (budget): `BlendGeometry` x2, `CopyGeometry`, `ConstructGeometryArray`, `DestroySkinArrays`, `BuildBoneIndexConversionTable`, `CopyTexture`, `PlaceTextureOnTopOfTexture`, `BlendTextures` x3, `ConstructGeometryAndSkinArrays`, `CreateSkinnedClump`, and all CCustomRoadsignMgr functions except `RoadsignGenerateTextRaster`.
- `ConstructTextures`: only control flow and blend factor maths were read (factorB<0 fold, face blend `(A+C)/(A+B+C)`, `B/(A+B+C)`); the per-name strings and accessory tail were not.
