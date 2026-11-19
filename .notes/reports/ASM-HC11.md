# ASM-HC11: compare-strictness / NaN class (std::min/max/clamp vs exe)

Tool: tools/standalone/asm/cmpscan.py (rows of the RYDER_1 hit-count tsv with exe_addr: port min/max/clamp sites + decoded x87 compare idioms incl. NaN branch; CMPSCAN_SITES_ONLY=1 = fast mode, only functions that have port sites). Run over all 3523 hit functions: 98 functions with min/max/clamp/Limit sites, most int. Float sites with an exe compare checked: ~20 sites in 6 functions.

Key finding: the exe has two NON-INLINED helpers, 0x404330 `min(a,b) = a < b ? a : b` and 0x420800 `max(a,b) = a > b ? a : b` (NaN -> second operand). 37 + 54 call sites (callers.py list in scratch). std::min(a,b) = b < a ? b : a and std::max(a,b) = a < b ? b : a return the FIRST operand for NaN. Mapped call sites to 38 hit functions; most already have explicit ternaries. The remaining std::min/max users were fixed.

Fixed: UpdateBlend (73c7abee6), CAutomobile::ProcessControl turn mass (6de5b4c1f), CAutomobile::ProcessAI + ProcessCarWheelPair min(1,3000/mass) (57ebe9f28), CBike::ProcessAI max (24ce7d402). Verdicts: .notes/asm_sweep/HC11.result.tsv. Not reached: Bmx 989, Train 1567/1569, HydraulicControl and the other float sites in the tsv.

Rule for the TS port: JS Math.min/Math.max return NaN if EITHER operand is NaN and Math.min(-0,0) = -0. The exe: helper min = `a < b ? a : b`, max = `a > b ? a : b` (NaN -> b), inline fld K/fcomp st(1)/test 5/jp = `K < v ? K : v` NaN -> v; std::clamp keeps NaN. Port each site as the explicit ternary in the exe operand order, never Math.min/max.

Effect size: only NaN (and +-0) operands differ; no equality difference exists (equal floats are identical).
