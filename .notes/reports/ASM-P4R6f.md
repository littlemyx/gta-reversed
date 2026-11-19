# ASM-P4R6f: never-audited Tasks/Events functions (continued)

Compared 15 of the 137 listed (largest first, skipping P4R6* rows): 13 MATCH, 2 FIXED. StandaloneRelease builds and links (ninja -j4). Tests/regress left to the gate.

Fixes:
- 68ea61b72 CTaskComplexPolicePursuit::SetPursuit (exe 0x68BBD0): m_Pursued is set only if CWanted::SetPursuitCop succeeds, otherwise nulled (port assigned it first).
- 7bafcdc3b CEventSoundQuiet::AffectsPed (exe 0x4B5240): start-time delta compared as signed (jl), port used uint32.

Noise only (not changed): float vs x87 dot/sum order in SetPursuit, WalkRoundCar::ControlSubTask, EventDamage::AffectsPedGroup.
Open: 122 functions of the list not reached (budget). Details in .notes/asm_sweep/P4R6f.result.tsv.
