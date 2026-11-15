# ASM-P4T1: asmsig triage tooling (exe-vs-build signature diff)

Tool: `tools/standalone/asmsig/asmsig.py` (python3 + capstone, run from the repo root), calibration `calibrate.py`. Triage only: nothing here writes a verdict into any result.tsv.

## Use
- `asmsig.py annot 0xADDR [--chunk K] [--exe gta_sa_compact.exe]`: annotated exe disassembly (stack depth tracked through push/pop/sub esp/callee `ret N`, `[esp+N]`/`[ebp+N]` -> argK/locN, read-only floats/ints resolved, callees named from the hook registry, jumps -> `Lk` labels, functions >150 insns cut into basic-block chunks).
- `asmsig.py diff 0xADDR [--port build/StandaloneRelease/bin/gta_reversed.exe]`: prints both signatures, score and the one-sided signals. The port address->name map is the MSVC `.map` next to the exe (copy exe+map first if a build is running).
- `asmsig.py batch LIST.tsv -o OUT.tsv`: LIST has an `addr` column; OUT = addr, function, score, triage (AUTO_MATCH if score < 2, else REVIEW; NO_DATA if no hook site or no map symbol), signals; sorted by score descending.
- `calibrate.py --pre-port PRE.exe --post-port POST.exe --picks picks.tsv`: recall / false-positive table (below).

## Signature (same extraction for exe and build)
Named callees (exe: hook registry; build: MAP symbol, comparable only if the name is a registered hook; small callees under 40 insns are merged into the caller and their one-sided names ignored, i.e. inlining tolerant), virtual call slots, constant floats (value-keyed: 0.1f and 0.1 differ; float literals materialised as `mov r, 0x437f0000` count as floats; 0, 1 and powers of two are weak), immediates (address-like values, shifts, masks, rand / x87 control word / strlen magic dropped; +-1 tolerance), `[reg+disp]` member offsets (tiny weight), conditional-branch count, size. Constants of any callee body (depth 1, under 150 insns) count as present on that side (inlining tolerance). Per signal type the score is capped so large functions do not swamp the ranking. Weights and caps: top of `compare()`.

## Calibration (honest)
PRE build = tip with 45 FIXED-row fix commits (one source .cpp each, taken from the sweeps' result.tsv, reverse-applied) built as StandaloneRelease; POST = the current build. 44 of 45 had data. MATCH = 2831 verified MATCH rows of .notes/asm_sweep/*.result.tsv, scored on POST.

| T | recall (PRE score >= T) | fix-visible (PRE minus POST >= T) | MATCH rows >= T |
|---|---|---|---|
| 1 | 91% | 23% (10/44) | 72% |
| 2 | 77% | 14% (6/44) | 59% |
| 4 | 64% | 5% | 41% |
| 6 | 52% | 5% | 30% |
| 8 | 43% | 0% | 22% |

Reading: only about a quarter of the real deviations change the signature (a fix that flips a condition, reorders operands or swaps a flag bit leaves calls, constants and branch counts untouched), and the noise on correct code is high (x87 vs MSVC rewrites, helpers inlined differently). Known-bad rows score somewhat higher than verified-good ones (52% vs 30% at T>=6), so the score is a useful READING ORDER, not a skip filter: AUTO_MATCH (under 2) still contained 23% of the known-FIXED functions, so do not skip them on its strength. Strongest signals in the FIXED set: one-sided float constants (F-), one-sided big immediates (I-), one-sided named callees (C-). Disp, small-imm, J and N signals are mostly noise.

## Batch
`.notes/asm_sweep/P4T1_batch.tsv`: the 5067 oracle-only rows scored against the main-checkout build of 15.11: AUTO_MATCH 3480, REVIEW 1328 (558 with score >= 5), NO_DATA 259 (no unique map symbol: lambdas, templates, folded or never-instantiated functions).

## Limits
The exe side only knows names of hooked functions (other callees are expanded or counted as unknown); port overloads are resolved by the best-scoring candidate; free functions in a namespace label are matched by bare name; no data-flow, x87 operand order or condition polarity check, which is where most FIXED rows live. If more recall is wanted: compare canonical per-basic-block opcode skeletons instead of multisets.
