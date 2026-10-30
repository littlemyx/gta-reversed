# ASM-S76b (CClothesBuilder geometry/texture blending, CCustomRoadsignMgr) - 2026-10-30

Slice: 20 rows (`.notes/asm_sweep/S76b.tsv`), results in `.notes/asm_sweep/S76b.result.tsv`. Continuation of ASM-S76 (the rows it did not reach).

| verdict | rows |
|---|---|
| MATCH | 12 (BlendGeometry x2, CopyGeometry, ConstructGeometryArray, DestroySkinArrays, ConstructGeometryAndSkinArrays, roadsign Initialise/Shutdown/CreateRoadsignTexture/CreateRoadsignAtomicA/CreateRoadsignAtomic/SetupRoadsignAtomic) |
| FIXED | 8 (BuildBoneIndexConversionTable, CopyTexture, PlaceTextureOnTopOfTexture, BlendTextures x3, CreateSkinnedClump, RenderRoadsignAtomic) |

## Fixes
- `BlendTextures` x3 (exe 0x5A5820, 0x5A59C0, 0x5A5BC0): the port's loop decremented the *pixel* counter in the inner condition, so it blended only the first width*height BYTES (a quarter of the image, alpha included). The exe blends the RGB bytes of every pixel and keeps alpha. The channel value is one x87 sum of `integer * float` products (`src2*r3 + src1*r2 + dst*r1`; the 2-source version `src*r2 + dst*r1`), truncated by `_ftol` and only the low byte stored (the exe's "clamp" tests an 8-bit register, it never clamps); the port truncated every term separately. Tattoo lerp: `int((tat - mix) * tatAlpha) * (float 1/255) + mix`. Lock/unlock order follows the exe.
- `CreateSkinnedClump` (exe 0x5A6B84-0x5A6C2B): the change flags. Exe: fat/muscle stat difference sets ratios AND geometry; different model keys set geometry; different texture keys set texture (texture flag starts false). The port overwrote geometry with `!equal(models)`, set ratios from the texture keys and forced texture=true, so an unchanged desc never returned null. Also the exe ORs `rpGEOMETRYPOSITIONS` into the merged geometry flags after creating the atomic.
- `BuildBoneIndexConversionTable` (exe 0x5A56E0): `RpHAnimIDGetIndex` returns -1 for "not found"; the exe tests only the low byte (`cmp al, 0xff`) and stores bone 0; the port stored 0xFF.
- `RenderRoadsignAtomic` (exe 0x6FF350): distance squared is summed z, y, x; the fade alpha is `(250000 - d2) * (float 1/248400) * 254` truncated to a byte (the port used 1 - invLerp and lerp: last-bit/off-by-one alpha).
- `CopyTexture` (exe 0x5A5730): palette bits of the source format are kept in the new raster flags (no effect for the asserted 32 bit rasters). `PlaceTextureOnTopOfTexture`: lock/unlock order (src first).

## Notes / open
- `BlendGeometry` (both): every term order (positions, normals, uv), the AddWeightToBoneVertex call order and the re-normalisation (`weights[4] != 0`, `1/(w3+w2+w1+w0)`) agree. The exe leaves `boneVertexIdxs[4..7]`/`weights[5..7]` uninitialised, but `AddWeightToBoneVertex` zeroes `weights[i+1]` when it takes slot i, so they are never read before being written.
- `ConstructGeometryAndSkinArrays`: the port zero-inits the bone conversion table (exe: uninitialised stack); only matters for a vertex pointing at a bone slot that the table did not fill.
- `CreateSkinnedClump` stores `odscr` into the dead global 0xBC1288 (never read anywhere in the exe); not ported.
- Roadsign functions are not reached by the combat regress; compared by asm only. `CreateRoadsignAtomic` keeps its `assert` (debug only).

## Tests
- wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=gta_sa_compact.exe, rebuilt after the last source edit): 1142 functions, mismatches PC24 0/0, PC53 0/0.
- StandaloneRelease build OK after the final rebase; PortableWin64 compile of ClothesBuilder.cpp and CustomRoadsignMgr.cpp OK (x64 subshell).
- v6 pre-commit grep: clean. Combat regress (once, after the last edit and rebase): PASS=14.
