# MAC-F4a — inline asm / naked functions out of the portable build (compile gate), via X87Soft

Host: Apple Silicon Mac. Rules `.notes/agent_rules/v4.md` (Windows-only parts do not apply). Repo
/Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main` (remote `fork`). Main checkout; parallel agent MAC-P2k (test
wiring in the portable CMake branch: portable_tests.cmake, FileRecord guard, Replay.h/WaterLevel.h) — never touch its files.
`.notes/` gitignored (`git add -f`). Wall-clock cap 100 min.

## Input
- Census after P-4: `.notes/w0/p4_final.tsv` — 36 rows "F/C: inline asm / naked" (+ 9 design rows that are NOT yours: P4_PLAN.md).
  Files include Radar.cpp/Gamma (5), AEAudioUtility.cpp (naked member, the known clang-cl failure), standalone/FtolExe.cpp (7), x87
  helpers, others — list them all.
- `.notes/STREAM_F_PLAN.md` sections 0, 2, 3 (notsa::fp, Fp.h), 3.5 (software x87 transcendentals, source/game_sa/Core/X87Soft.h,
  .notes/reports/F1_soft.md), 6 rows F-4 and F-5.
- Census tool: tools/portable/snap_census.sh (MAC-R2.md); wine A/B method: .notes/reports/MAC-P2b.md.

## Task
For every inline-asm / naked site: under NOTSA_PORTABLE provide the non-asm branch, MSVC keeps the asm in `#else` untouched.
- x87 transcendental / rounding sequences (fsin/fcos/fpatan/fyl2x/f2xm1/fscale/frndint/fistp…): use X87Soft.h (bit-exact software x87
  at the precision the exe ran — check F1_soft.md for what is verified) or `notsa::fp` helpers. If X87Soft lacks an op, add it there
  only if you can test it against known x87 results (golden vectors computed under wine/x86 from the same asm — tests/portable host
  test + a tiny x86 generator run under wine); otherwise a plain libm fallback marked `// F-4 TODO: not bit-exact` and listed.
- ftol (`FtolExe.cpp`): portable equivalent with the exe's truncation/overflow semantics (0x80000000 on out-of-range etc.).
- AEAudioUtility naked member: rewrite as a normal function under NOTSA_PORTABLE with the same math.
- FPU control word functions (_controlfp/_statusfp/_clearfp/_set_SSE2_enable left from MAC-C6): portable no-ops that log once — the
  portable build has no x87; list them.
Acceptance: census "F/C: inline asm" = 0; host test of every new non-asm branch vs golden x87 outputs (bit-exact where claimed, ULP
error table otherwise); Win32: StandaloneRelease compile commands identical + wine cl.exe A/B code+relocs identical on every touched TU
(`__LINE__`-only neutral).

## Git / report
Commit only your files (`git commit -- <files>`, trailer exactly `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`), push after
each (pull --no-rebase on reject; never force; never push reverse/interior_c). ≤ 1 own sub-agent. Leave no background tasks running.
Report `.notes/reports/MAC-F4a.md`: site table (file:line, asm op, portable branch, bit-exact? evidence), census, A/B. Return ≤ 15 lines.
