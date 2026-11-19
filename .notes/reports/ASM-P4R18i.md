# ASM-P4R18i: scripted verdicts for trivial-pattern functions (Tasks + Events)

Tool: `tools/standalone/asm/trivialcheck.py --nv noverdict.tsv --out result.tsv` (stdlib; batches exedis, one call per 150 addresses).
Input: noverdict.py output regenerated at origin tip (1142 rows in Tasks/Events without a verdict).

| pattern | rows | result |
|---|---|---|
| const-return virtuals (GetEventType/GetTaskType/GetEventPriority/GetLifeTime/GetLocalSoundLevel/CanBeInterruptedBySameEvent/IsCriminalEvent/IsInterruptable/IsSimple/MakeAbortable/WanderType...), exe `mov eax|al,imm / xor; ret [N]` vs port `return <const>` (enums resolved from sources, `Type` via class header) | 173 | 173 MATCH (166 script, 7 by hand: 6 GetTaskType rows P4R18h could not resolve + DuckToggle::MakeAbortable/BeInShop::GetWanderType) |
| destructors = vtable store + jmp base dtor (or bare jmp): port base class equals the exe jump target's class, port dtor default/empty | 49 | 48 MATCH, 1 INTENTIONAL (CTaskComplexUsePairedAttractor: exe dtor never CleanUpOldReference's the registered m_Partner; port EntityRef does - safe, kept) |
| ctors = base ctor call (+ pushed constants) + vtable store only: port class has no own members and an empty ctor body, or init-list args equal exe pushes (anim group/id, blend, task type; names not compared) | 32 | 32 MATCH |

SKIP (not a constant pattern, left for other slices): 86 rows (AffectsPed, GetSourceEntity, most MakeAbortable, ...). Ctors that store args/bitfields were not scripted (needs member offsets).
Mismatches found: 0 real; no source changed, no build needed. Result: .notes/asm_sweep/P4R18i.result.tsv (254 rows).
