# ORACLE-W3-CARENV report

Agent ORACLE-W3-CARENV (rules v3), 2026-10-20 16:28 - ~17:05 WEST, worktree C:/gta-wt/oracle-w3-carenv (branch oracle-w3-carenv, base 02f0fa84). New target `wave3_carenv_oracle_test` (W2 harness: deterministic arena, the whole exe data image + a 4 KB fixture block compared after every case), rows in `tests/standalone/wave3_oracle_carenv.inc` (`W3C_DIV=n` divides the case counts).
**25 functions (24 A/B + 1 class C bonus), PC24 0 mismatches (PC53 0), run time 80 s**, no case skipped as "faulted identically".

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 4525 | **4550** |
| CustomCarEnvMapPipeline*.cpp uncovered A / B | 4 / 20 | **0 / 0** |
| game_sa/Pipelines covered | 10 | **35** |
| port bugs fixed (one commit each, asm address and pre-fix count in the subject) | | **6** |

## Fixtures
Two raw RpMaterial blobs and two RpAtomic blobs; the plugin offsets (ms_envMapPluginOffset 0x8D12C4, ms_envMapAtmPluginOffset 0x8D12C8, ms_specularMapPluginOffset 0x8D12CC, the matfx material offset 0xC9AB74) are randomised per case (distinct, 0x28..0x54) and the plugin slots hold null / the fake data (0xC02D18, random bytes + texture) / a used slot of the matching pool. The three CPool headers (0xC02D28 / 2C / 30) sit over 1..6 slot arrays with random used/free split (all used and all free included), ref counters and first-free index (-1..cap-1); the exe's inline pool New / Delete (flag 0x80, first-free = min) are compared bit for bit through the pool bytes and the data image. Four texture blobs (name[0] 'x' / 'a' / 0 / garbage, filterAddressing, refCount), four matfx data blobs (`*(data + 4)` = env texture). Float setters get game-range values mixed with NaN / inf / denormal / 3e38 / beyond-byte / beyond-int32 values; float getters are compared bit for bit.
RenderWare is not run: RpMatFXMaterialGetEffects (0x812140) and RwTextureDestroy (0x7F3820) are `oracle::Patch`ed to host stubs and the port's fakerw entry points of the same names have the same bodies; RpMatFXMaterialGetEnvMapTexture (the port calls it where the exe inlines `*(*(mat + [0xC9AB74]) + 4)`) is defined in the test with the exe's formula. The port's RpMaterial is the librw layout (surfaceProps at +8): CustomPipeMaterialSetup's flag word is moved from +0xC (port) to +0x10 (exe) before the compare.

## Port bugs fixed (rows failed pre-fix; counts = failing PC24 cases of the first full run on the unfixed code, logs C:/gta-scratch/w3ce_pre.log, w3ce_pre2.log)
| function | asm | bug | pre-fix |
|---|---|---|---|
| pluginEnvAtmConstructorCB | 0x5D8D30 | stores through the offset ARGUMENT; the port used ms_envMapAtmPluginOffset | 92/200 |
| pluginEnvAtmDestructorCB | 0x5D9730 | same (the inline pool Delete works on that slot) | 217/500 |
| pluginEnvAtmCopyConstructorCB | 0x5D9780 | a missing dst comes from the pool New 0x5D9260 WITHOUT the default writes (those are in AllocEnvMapPipeAtomicData 0x5D96F0), copies `sizeInObject` bytes (4 in the game), offset argument; the port went through AllocEnvMapPipeAtomicData and copied 12 bytes with the global offset | 295/800 |
| SetFxEnvTexture (and CustomPipeMaterialSetup through it) | 0x5DA230 | a full pool leaves a null in the slot; the port placement-new'ed on the null New() result (access violation) | 53/800 (27/800 via 0x5DA560) |
| pluginSpecMatCopyConstructorCB | 0x5D9830 | returns the destination OBJECT (the port: the plugin data pointer, null whenever the source had none), no AddRef on a null texture (port: null deref), stop on a failed pool New | 800/800 |
| pluginSpecMatDestructorCB (class C bonus) | 0x5D97D0 | `mov [edi],0` clears the plugin slot; the port nulled a local copy (`auto*`) and left a dangling pointer in the material | 383/500 |

## Rows that pass without a change
pluginEnvMatConstructorCB, pluginEnvMatStreamGetSizeCB, pluginSpecMatStreamGetSizeCB, SetCustomEnvMapPipeMaterialDataDefaults, pluginSpecMatConstructorCB, GetFxEnvScaleX/Y, GetFxEnvTransSclX/Y, GetFxEnvShininess, GetFxSpecSpecularity, GetFxSpecTexture, SetFxEnvScale, SetFxEnvTransScl, SetFxEnvShininess (the exe `_ftol` is 64 bit: for |value*8| >= 2^31 the low byte is always 0 in a 24-bit float, so the port's int cast matches), pluginEnvMatDestructorCB, pluginEnvMatCopyConstructorCB, AllocEnvMapPipeAtomicData.

## Observed, not changed
* SetCustomEnvMapPipeMaterialDataDefaults 0x5D8BB0 does not write the padding byte 5 (the port's value-initialisation zeroes it): masked in the row, nothing reads it.
* The copy constructors copy with `rep movsd/movsb` of `sizeInObject` bytes; for sizes above the 12 byte slot (unreachable: the plugins are registered with size 4) the exe's forward copy and the port's memcpy differ on overlapping slots: rows use <= 12.
* SetFxEnvTexture(mat, nullptr): the exe reads the matfx env texture inline (`*(*(mat + [0xC9AB74]) + 4)`, no effect-type check); the port calls RpMatFXMaterialGetEnvMapTexture (the librw version checks the effect type). It is only reached under `RpMatFXMaterialGetEffects == rpMATFXEFFECTENVMAP` from CustomPipeMaterialSetup; the row stubs the call with the exe formula, so the librw function itself is not under test.
* pluginSpecMatCopyConstructorCB under `notsa::IsFixBugs()` destroys the overwritten texture (the exe leaks it); IsFixBugs is false in the test target, so the row compares the exe behaviour.

## Not covered (CustomCarEnvMapPipeline*.cpp)
Class C RegisterPlugin / CustomPipeAtomicSetup / CreateCustomObjPipe / CustomPipeInstanceCB (RW pipeline objects, indirect call), class D CreatePipe / DestroyPipe (heap / virtual memory); PreRenderUpdate, CustomPipeRenderCB and the stream callbacks were already covered (rw_custom_pipelines_test).

## Tests run (worktree, final tree)
wave3_carenv_oracle_test 25 functions PC24 0 / PC53 0, exit 0, 80 s (log C:/gta-scratch/w3ce_post2.log); rw_custom_pipelines_test (the only other target linking the four CustomCarEnvMap TUs and CustomBuildingDNPipeline.cpp, run under gpu_locked.sh) 115 checks passed, 0 failed. The orchestrator runs the full run_all_tests.
