# ASM-S116 (slices S110 + S116, tier B/C)

Worktree asm-s116 (from origin/reverse/interior_c 14a774cbb). Result files: `.notes/asm_sweep/S110.result.tsv` (12 rows), `.notes/asm_sweep/S116.result.tsv` (45 rows).

## Verdicts

| slice | rows | MATCH | FIXED | UNSURE / NOT_REACHED |
|---|---|---|---|---|
| S110 CQuadTreeNode | 12 | 7 | 5 (4 functions in 2 commits) | 0 |
| S116 plugins / pipelines | 45 | 36 | 8 | 1 (JPegDecompressToRaster, NOT_REACHED) |

## Fixes

| commit | function (exe range) | deviation |
|---|---|---|
| 957b050b2 | CQuadTreeNode::DeleteItem(item, rect) 0x552A90 | children recursion must use the 2-arg overload with the rect; the port called the 1-arg one and flushed the whole subtree |
| 5c36bf150 | FindSector(rect) 0x5525A0, FindSector(vec) 0x552640, InSector 0x5526A0 | NaN arms only (exe sends unordered compares to -1 / pass arms); ordered cases already identical |
| b746a68d4 | NodeName copy callback 0x72F9F0 | strncpy_s vs exe strncpy (zero pad, no terminator write); SetFrameNodeName shares the helper |
| b5a222ffe | JPegCompressScreenToBuffer 0x5D0740 / 0x5D02D0 | port never called JPegCompressScreen; term callback copied a whole buffer instead of the used bytes. JPegDecompressToVramFromBuffer was hooked on the fill callback 0x5D0320, real function is 0x5D07A0 (bool). Both are dead code in the game |
| 30974c381 | RpAnimBlendClumpAddAssociation 0x4D6790 | port was NOTSA_UNREACHABLE, exe implements it (no callers) |
| 6f3618683 | RpAnimBlendClumpUpdateAnimations 0x4D34F0 | exe tests the hierarchy sequence pointer (+4), port tested the sequence count |
| 20895b8ec | CCustomCarEnvMapPipeline::CreateCustomObjPipe 0x5D9F80 | Unlock takes AddFragment's result |

Each claimed deviation was checked twice against the asm (first against the instruction stream, then against the struct layout / call arity: e.g. DeleteItem recursion by the `push` count and `ret 8` of the callee, hierarchy +4 by the CAnimBlendHierarchy layout, JPeg ToBuffer by the missing `call 0x5D0470` and the `add [0xC02B80], 0x32000` constant).

## Notes / left as is

- JPegDecompressToRaster (0x5D05F0): only caller is 0x5D07A0 which has no callers; the exe returns bool and converts rows through 0x5D0570, the port is a different inline loop. Not reached, not rewritten.
- PipelinePluginAttach: exe registers the atomic stream with a NULL write callback; the port passes a no-op writer because librw calls the write callback unconditionally (DFF writing never happens in the game).
- CQuadTreeNode::DeleteItem(item, rect): exe has no null-child check (would crash); the port keeps its guard.
- portable (NOTSA_PORTABLE) branches (GL3 pipelines, P-2i breakable file records) are deliberate adaptations and were compared only for the x86 path.

## Tests

- StandaloneRelease (gta_reversed.exe): builds, 1186/1186, exit 0.
- wave2_oracle_test (StandaloneRW, RW_EXE_ORACLE = GTA_EXE = gta_sa_compact.exe): 1142 checks, 0 failures, 0 mismatches.
- PortableWin64 (x64) `cl /Zs` of JPegCompress.cpp, NodeName.cpp, RpAnimBlend.cpp, CustomCarEnvMapPipeline.cpp (QuadTreeNode.h is included through them): 0 errors.
- v6 pre-commit grep on the diff: no hits (no double, float->int casts, pointer-to-int casts, Win32 or size constants added).
- Review: the standalone RW shim's RxLockedPipeAddFragment can return null (wrong node def / not locked); RxLockedPipeUnlock(null) returns null, so the pipe creation fails the same way the exe does. 0x5D07A0 in gta_sa_compact.exe is `mov eax,[esp+8]; sub esp,1Ch; test eax,eax; jnz; xor al,al; ret` = (raster, enable) bool function, 0x5D0320 is the fill_input_buffer callback; no other hook uses 0x5D07A0.
- Combat gameplay regress: NOT run. The GPU lock was reserved for the orchestrator gate (gpu.prio, pid 1010717) for the whole 30 minutes and ~8 other regress runs were queued; the queued run was killed by PID. Regress left to the gate.
