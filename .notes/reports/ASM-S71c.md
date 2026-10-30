# ASM-S71c report (CRope::Update + S80 trivial rows)

Base 7683b1acb (origin/reverse/interior_c). Result tables: `.notes/asm_sweep/S71c.result.tsv`, `.notes/asm_sweep/S80.result.tsv`.

## S71c: CRope::Update (0x557530-0x558CE6, 1658 instructions): MATCH, no change

Traced end to end against the port (which already carries per-expression x87 annotations): damping pow, 200 m camera cull, expired-rope fall, ground-z refresh, the 32-segment simulation (wind, blend, gravity, ground clamp, length constraint, speed), analog/digital crane controls and clamps, the hanging-object mass blend, UpdateWeightInRope + speed projection + holder/hanging share, matrix blend, hook follow, release rules, the flags1 countdown, the jump table at 0x558CE8 (1 vehicles, 2 peds, 3 magnet, 4 magno, 5 none, 6 quarry, 7 site crane without vehicles) and the vehicle/ped/object searches (model globals 0x8CD6C8..0x8CD73C, rotation 2/3/1, OnPickedUp 1/mass terms, 0.01 floor), Finish. The exe's stale stack slot read for the ped case is replicated by the port's scratch vector (verified that nothing in the ped search overwrites it). Only a style note: the OnPickedUp lambda takes a bare `double` (x87d is double in the non-portable build; numerically identical when portable).

## S80: 15 rows

| verdict | count |
|---|---|
| MATCH | 12 |
| DEVIATION (fixed) | 3 |

Not trivial: GetControllerSettingTextKeyBoard (0x52FE10, ~800 bytes + 55 entry jump table) and CGamma::SetGamma (full gamma ramp). Both compared in full.

| row | fix | commit |
|---|---|---|
| CControllerConfigManager::GetControllerSettingTextKeyBoard | '^' (not German) and 178 (not French) returned null, the exe falls to the generic path; generic path maps the key through the exe's 0x719360 (below 0x80 as is, else byte table at 0x8D5AD8, 0 gives the glyph of '#'), port stored the raw key; F~n~/NUM~n~ texts use buffer 0xB7149C | f9946520d |
| CGamma::SetGamma | missing unconditional store of level into this+0xC (m_PrevLevel; write-only in the exe) | 50a1a73f8 |
| CPad::GetMouseState | success returns the HRESULT, not 0 | 95f3a236e |

Key tables (1001..1055 text ids, numpad numbers, mouse 1..7 ids) were decoded from the exe jump tables by script and compared with the port; all equal. The remaining 12 rows are MATCH (see the result table for the asm ranges).

## Tests

- StandaloneRelease built; `gameplay_regress.sh combat`: PASS=14.
- wave2_oracle_test (RW_EXE_ORACLE and GTA_EXE = gta_sa_compact.exe), rebuilt after the last edit: 1142 functions, mismatches PC24 0 / 0, PC53 0 / 0.
- v6 pre-commit grep clean; PortableWin64 compile (x64 subshell) of ControllerConfigManager.cpp, Gamma.cpp, Pad.cpp: 0 errors.
