# MAC-P2i report: BreakInfo_File + Breakable plugin (2026-10-24, portable/main)

## Commit
* dbc43b87 P-2i (pushed to fork portable/main). Source: FileRecords/BreakFile.hpp [new], Plugins/BreakablePlugin/BreakablePlugin.h + .cpp. Tests/tools: tests/portable/p2i_breakable_test.cpp, tests/portable/p2i_stubs/StdInc.h, tools/portable/run_p2i_test.sh, tools/portable/p2i_scan_img.py (independent Python census), tools/portable/gen_x86_golden.py (BreakInfo_File + 3 cases). No CMake, no FileRecord.hpp / GenericGameStorage.h change, BreakObject_c.* untouched (plan did not require it; it only reads the in-memory info).

## Design
| item | change |
|---|---|
| BreakInfo_File (0x34) | posRule u32@0, numVertices u16@4, 9 pointer slots as u32 (@8,0xC,0x10,0x18,0x1C,0x24,0x28,0x2C,0x30), numTriangles u16@0x14, numMaterials u16@0x20, explicit pads (6,0x16,0x22) written 0. (Plan said "7 pointers": it is 9; in-memory BreakInfo_t is 0x60 on arm64, not 0x58.) |
| FileRecord<BreakInfo_t> | ToFile: counts + rule, pointer slots/pads 0. FromFile: `i = {}` then rule/counts (pointers null until the reader re-points them). x86 `static_assert(sizeof(void*) != 4 || sizeof+13 member offsets raw BreakInfo_t == record)` in BreakablePlugin.h (evaluated in the i686 msvc census). |
| BreakableStreamRead/Write (`#ifdef NOTSA_PORTABLE` blocks, original code kept in `#else`) | 4 header bytes read into a local uint32 (the plugin slot is a pointer-sized union: on LP64 the read filled only its low half); info read via BreakInfo_File + FromFile. Writer: header = `info ? 1 : 0` in a local (the exe wrote the pointer's low 4 bytes; on LP64 that half can be 0), info written via ToFile. Real data: all 254 non-empty chunks have a distinct garbage header (raw pointer), so the reader must only test non-zero. |
| payload alignment | `BreakablePluginData::kTexturesAlign = sizeof(void*) > 4 ? alignof(RwTexture*) : 1`; GetRequiredAllocationSize and GetTexturesOffset round up with it (folds away on x86; a helper function was tried first and the wine A/B caught it as an extra out-of-line COMDAT). |
| GetSize | `sizeof(BreakInfo_File)` (= 0x34 = what the exe returned). |
| destructor | portable only: `::operator delete(info)` pairs with `operator new(allocSize)` in the reader (original `delete BreakInfo_t*` is a sized delete of the wrong size; ASan new-delete-type-mismatch). MSVC branch unchanged. |
| asserts | `VALIDATE_SIZE_FILE(BreakInfo_t)/(BreakablePlugin)` -> `VALIDATE_SIZE` (x86 in-memory: 0x34 / 4; arm64 0x60 / 8); BreakInfoColor stays `_FILE`; on-disk 0x34 is a static_assert in BreakFile.hpp. |

## Results
| gate | result |
|---|---|
| host test `tools/portable/run_p2i_test.sh mutate` (real BreakablePlugin.cpp on stub StdInc; ASan+UBSan) | ALL OK. Real data gta3.img (Steam copy, not committed): 12953 DFFs, **16264 geometry-extension 0xFD chunks (16010 header 0, 254 with data)**; 56288 verts / 36972 tris / 3071 materials / 1.88 MB payload. For every chunk: BreakableStreamRead consumes exactly the chunk length, blob contiguous in the documented order, pointers inside the allocation, RwTexture* array 8-aligned and ending exactly at the allocation end, contents == file bytes, one RwTextureRead per material (mask null for empty), read -> write -> read round trip (write == original with pointer slots/pads zeroed and header 1; second write byte-identical), destructor balances the texture count (0 leaked). STATS line equals the independent Python census (p2i_scan_img.py): MATCH. Data sanity: 0 bad triangle vertex/material indices, 0 unterminated names, all posRule=1. Synthetic: header 0 / high-bit / 1 / 0xFFFFFFFF, empty counts, 25-cell count sweep, alignment sweep nv 0..6 x nt 0..6 x nm 0..9 (pad exercised), writer with a live pointer whose low 32 bits are 0 (mmap'd 4 GB-aligned page) -> header 1, goldens (exe / port / max) exact, GetSize 0x34. **Mutants caught 9/9**: no alignment, alloc without pad, offset without pad, raw 0x60 record read, header = pointer low bits, triangles/materials count swap, rule dropped, GetSize 0x60, pointers kept in FromFile |
| layout_diff.py run (fresh work dir) | `BreakInfo_File` 52/52 **identical** (i686 vs arm64); in-memory BreakInfo_t 52/96 and BreakablePlugin 4/8 known-diff (intended). Totals identical 120, known-diff 13, NEW 2 (P-2a HAnimNodeInfo pair, pre-existing), not found 2. `gen_x86_golden.py selfcheck`: OK (BreakInfo_File offsets == i686 BreakInfo_t dump, all 13 members) |
| native census (BreakablePlugin, BreakObject_c, app, InjectHooksMain) vs build/census_p1_after | BreakablePlugin 2 -> 0, BreakObject_c 2 -> 0, app 2 -> 0 (the `_FILE` size asserts), InjectHooksMain 39 -> 1 (D3DLIGHT9 in CustomCarEnvMapPipeline.h, R/other stream); 0 new |
| msvc-mode census (same 4 TUs, force) | 0 errors |
| wine cl.exe A/B (A = HEAD 05e50081 via git archive + vendor/librw, B = A + my 3 source files; NOTSA_SIG_NORM_STRINGS=1, no PCH; /private/tmp/claude-501/p2i_ab) | BreakablePlugin, BreakObject_c, app, InjectHooksMain: **code+relocs+data identical**. First round BreakablePlugin differed (one extra 5-byte out-of-line `AlignTexturesOffset` COMDAT) -> helper replaced by the folded constant, re-run identical |

## Consumer audit
* BreakInfo_t file bytes / the 0xFD stream: only `BreakableStreamRead/Write/GetSize` (registered via RpGeometryRegisterPluginStream in BreakablePluginAttach, hooks 0x59CEC0/0x59CDE0/0x59D0F0); all three go through the record. No other reader or writer (grep source/tests/tools).
* In-memory consumers of BreakInfo_t / the blob: BreakObject_c.cpp (`BREAKABLEPLG(...)->m_pBreakableInfo`, `SetBreakInfo` reads info->m_p* / counts / m_pTextures[i]): uses the typed members only, no offsets or sizeof; unaffected by the 0x60 size and the alignment pad. BreakablePluginAttach (app.cpp), InjectHooks: unchanged.
* The plugin slot `m_SectionHeader` is no longer used on the portable path (kept in the union for MSVC).

## Open items
1. CMake: wire `tools/portable/run_p2i_test.sh` (W0 owns CMake). It needs build/PortableMac/compile_commands.json for flags and the Steam GTA copy (GTA / GTA_IMG env overrides); without the img the real-data part prints SKIPPED.
2. gta_int.img DFFs were not scanned (brief: gta3.img); `GTA_IMG=.../gta_int.img tools/portable/run_p2i_test.sh` runs the same walk (needs the matching `p2i_scan_img.py <img>` – the script compares against gta3's census only through GTA_IMG, which it passes on).
3. `BreakableStreamGetSize` returns 0x34 (as the exe); it ignores the 4 header bytes and payload, so a *write* through RpClumpStreamGetSize under-reports the chunk size. Not changed (hook-faithful); matters only if the port ever writes DFFs.
4. Plan table said 7 pointers / 0x58; corrected in code comments (9 pointers, 0x60). P2_PLAN.md not edited (not my file).
