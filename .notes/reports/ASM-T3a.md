# ASM-T3a: static script-command usage scan (PARTIAL, decoder does not resync yet)

Status: tools/standalone/scmscan/scmscan.py decodes the typed-arg stream (arg type bytes per RunningScript.h:19-52,
ReadArg.hpp:120-200), parses main.scm segment 3 (header at 0xcfa4+8: 135 missions, mission offsets) and script.img
(streamed scripts), and joins with the C++ registration table (REGISTER_COMMAND_HANDLER / _UNIMPLEMENTED / _NOP /
_UNSUPPORTED regexes over source/game_sa/Scripts/**/*.cpp; opcode names from eScriptCommands.h).

Blocker: the SCM carries NO per-opcode argument counts (typed args only; the next opcode has no marker), and the repo has
no count table (counts live implicitly in handler signatures; the 713 UNIMPLEMENTED ones have none). The tool learns
counts: seed from fall-through ip deltas in the two scmtraces (203 opcodes), then lookahead voting. Result: 203 of 215
segments still desync (main after 4 instructions, missions after 15..1800), so the TSV
(.notes/asm_sweep/T3_SCM_USAGE.tsv) is NOT trustworthy and no totals are reported. Main code entry = follow the leading
GOTO chain (0xcfa4 -> 0xd1d8 -> 0xda8c).

Next step (not done, budget): obtain real arg counts. Cheapest reliable sources: the exe handlers (CollectParameters(n)
immediates via exedis.py over the 1.0 exe command tables) or a Sanny Builder sa.ini opcode DB. Plug into `counts` in
scmscan.py (dict opcode -> n, -1 = variadic); join/TSV/report generation then work unchanged. Validate with the traces:
every X row (base, ip) must land on a decoded instruction boundary.
