# ASM-HC12: float compare strictness (equality case first, NaN second)

STEP 1: 64ed2a046 (rhino extra-wheel mins, exe 0x6AD24A-0x6AD33A) re-disassembled: `fld touch; fcomp saved; test ah,5; jp` = touch<saved?touch:saved (tie/NaN/unordered keep saved) = the new ternary. CONFIRMED.

STEP 2: tools/standalone/asm/cmpscan2.py = cmpscan.py + pairing of EVERY port float compare (literal compares matched by the exe constant incl. order/polarity -> OK/EQ/NAN/NOPAIR; var-var compares aligned sequentially on the ordered outcome set -> SEQ-OK/SEQ-NAN/SEQ-NOMATCH; unconsumed exe idioms = EXEVAR). Ran on the top 120 RYDER_1 hit functions that have no HC8*/HC11*/RVZ* row (cand list; 2523 candidates). Result: 8 real compares classes EQ/SEQ-NOMATCH were all scan artefacts (templates, wrong constant pairing, e.g. Ped 5810 = fadd 1.0 operand, ProcessLights 2084 = unrelated global<0); NO equality-case (a==b) mismatch found in the hand-verified set (11 functions, ~60 compares, see HC12.result.tsv).
Real finds, both NaN-only: ApplyFriction magnitude `<= 0` (exe returns on <=0 and NaN) and ProcessSwingingDoor `damage <= 100` (same idiom) -> `!(x > K)`. Not fixed: ApplySpeed snooker-table limits (NaN-only, minigame branch only; exe form is `new>max && v>0`).
Tool limits: jump polarity (jump to body vs. skip) is not decoded, so NAN rows need a look at the jump target; mixed x87d expressions are only paired by sequence.
Build: StandaloneRelease links (Physical/Automobile objects rebuilt). No game run (gate).
