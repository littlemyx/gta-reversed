# ASM-HC9: integer-arithmetic bug class (signed %, /, >> vs exe and/shr/unsigned)

Sites: grep of `% N`, `/ 2^n`, `>>` in source/game_sa .cpp inside functions with exe_addr in ryder1.tsv: 297 lines in 159 functions (list: C:/gta-scratch/hc9/cands.tsv). 123 of them (75 MATCH, 47 FIXED, 1 DEVIATION) already had MATCH/FIXED verdicts from earlier sweeps; I looked at the 36 NONE ones (about 15 read in detail, the rest by the op census) plus a mnemonic census (idiv/div/sar/shr) of all 95 mapped functions (the census run was cut off at about 95 of 159).
Checked against the exe: 17 rows in HC9.result.tsv. Real differences found: 0 (no code changed, no commits besides this report). Harmless (non-negative values): 8.
Not covered: inline functions in headers and functions the regex could not map to a hit-count row.
Rule for the TS port (one line): in JS `%`, `/`, `>>` are signed and `>>>`/`&` unsigned, so for every `x % 2^n` or `x / 2^n` on an int8/int16 field take the exe op (and mask / movzx vs movsx / shr vs sar / jb vs jl) and write `& (2^n-1)` or `>>> n` explicitly; never assume `% n` equals `and n-1` when the value can be negative.
