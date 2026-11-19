# ASM-P4R27: scripted one-liner verdicts over all of source/ (exe 1.0 oracle)

Tool: `tools/standalone/asm/trivialcheck.py --scan --nv <noverdict.tsv>` (needs the MSVC env + build/StandaloneRelease/compile_commands.json).
Port-side member offset / width / float-ness is proven by a generated TU of `static_assert`s compiled with the build flags (mutation-checked: wrong offset/size -> error on that line).

| pattern | rows | verdict |
|---|---|---|
| CONST (mov/xor imm; ret) | 74 | 74 MATCH |
| FCONST (fld [abs]; ret, float read from the exe image) | 9 | 9 MATCH |
| GETTER (mov/movzx/fld/lea [ecx+off]; ret) | 70 | 70 MATCH (1 by hand: CEventDeadPed::GetSourceEntity, private member) |
| SETTER (mov eax,[esp+4]; mov [ecx+off],eax; ret 4) | 20 | 20 MATCH |
| SETCONST (mov [ecx+off],imm; ret) | 11 | 11 MATCH |
| NOP (bare ret) | 51 | 50 MATCH (ZoneScoped-only / empty bodies), 1 FIXED |
| THISRET | 1 | 1 MATCH |
| total | 236 | 235 MATCH/hand + 1 FIXED |

Categories: logic 189, audio 37, render 10. Directories: Tasks 62, root 46, Entity 39, Audio 37, Models 18, Events 12, Fx 6, Ragdoll 6, Interior 4, Animation 3, others 3.

Mismatches (script CHECK, hand-checked): 3
- CTheScripts::ScriptDebugLine3D (0x485DE0): exe bare ret, port stored the line in the run build -> FIXED 7a89f37a2 (gated under NOTSA_STANDALONE_RUN; nothing renders them there). StandaloneRelease links.
- CDebug::DebugDisplayTextBuffer (0x532260): MATCH by hand (empty buffer returns at once; no caller of DebugAddText).
- CEventDeadPed::GetSourceEntity (0x4ADF40): MATCH by hand (offset 0x14 = first member after the 0x14 base).

Not scripted (left without row, ~32): patterns whose port side is not a one-liner or is not found by name (CONST port bodies returning pointers/globals, 13 NOP/6 CONST bodies not found, complex getters), global getters/setters, bit-test getters (`test [ecx+off],mask; setnz`), const-returning virtual thunks to base calls.
