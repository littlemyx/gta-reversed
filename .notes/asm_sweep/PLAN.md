# ASM sweep — port source vs the original exe's machine code, function by function, no game runs

Why (user, 2026-10-26): lockstep A-B finds ~2 port bugs per agent-hour and every agent waits on the single GPU. A read-only comparison of
each function's source against the exe's disassembly needs no game, runs in parallel and found 6 bugs in 30 min (FID-MAC).
Lockstep (RD-AB-*) stays as the final check and keeps running at a low rate.

## Input
- `.notes/asm_sweep/INDEX.md` lists the slices; `.notes/asm_sweep/Snn.tsv` holds each slice's functions (addr, file, line, class, function, oracle class).
- Every function is one the exe-oracle does NOT cover (B/C/S/D). Slices go behaviour subsystems first: Tasks, Vehicle, Ped, CarCtrl, Population,
  PedIntelligence, GroupEventHandler, Physical, World, Streaming, Object, Camera, Radar, WaterLevel, Interior.

## Method per function
1. Run `py -X utf8 -I tools/standalone/exedis.py <ADDR> --exe gta_sa_compact.exe` from the main checkout path (call targets are named).
   Read the port function in source.
2. Compare the control flow (every branch and loop, its condition and polarity, early returns), the constants, the call order and arguments,
   the fields read and written (offsets via the class layout), and the rand() / CGeneral::GetRandom* calls (count, order, range, discarded
   results). An x87 operation order or a double intermediate is a finding only if it changes a stored float (note it, mark it UNSURE).
3. Verdict:
   - MATCH;
   - DEVIATION (sure, with the asm address range): fix it as the exe does, in one commit per function, with the address in the message;
   - UNSURE: log it, do not fix.
   - Deliberate documented deviations (`standalone_deviations.tsv`, `NOTSA_PORTABLE` / test switches) are not findings.
4. Consumer audit for any fix that changes shared state, meaning or a signature (grep readers and writers).

## Output (per slice)
- `.notes/asm_sweep/Snn.result.tsv`: addr, function, verdict, asm range, one-line note, fix commit.
- Do NOT edit INDEX.md (parallel agents would conflict) — the orchestrator updates the status from your result file.
- The report goes in `.notes/reports/ASM-Snn.md`, ≤15 lines.

## Gate per agent (rules v5)
1. Release build of the worktree.
2. The oracle targets of the touched files, if they exist (build and run them: 0 PC24).
3. If game code changed: `soak.sh` once at the end. It takes gpu.lock; that is the only GPU use.

The orchestrator's gate covers the rest.

## Budget
One slice (~40 functions) per agent. Stop new functions at 150k tokens by the agent's own estimate. Land what is done and mark the rest of the
slice `partial`.
