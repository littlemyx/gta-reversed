# ASM-P4R18b
Review of P4R18 fixes: all 3 CONFIRMED against the exe (0x626D90 case 0xCB, 0x6589B0 tail health check, 0x621E00 event forward).
Of the 70 remaining candidates, 19 compared (18 MATCH, 1 FIXED), 51 NOT_REACHED (budget), incl. KillCriminal::ControlSubTask 0x68E950.
Fix: CTaskSimpleSitIdle::MakeAbortable IMMEDIATE now nulls m_anim (exe 0x631207), aab007f4b.
Build: StandaloneRelease links (ninja -j4). No game runs, no regress.
Detail: .notes/asm_sweep/P4R18b.result.tsv
