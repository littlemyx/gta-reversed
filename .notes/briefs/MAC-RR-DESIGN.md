# MAC-RR-DESIGN — deterministic record/replay state-hash harness (design + first slice), milestone 4

Host: Apple Silicon Mac. Rules `.notes/agent_rules/v4.md`. Repo /Users/Andrei.Mukhin/github/recomp/gta-reversed, branch portable/main.
Parallel agent MAC-R6 (render pipelines: source/portable/render/**, standalone/rw/**, Pipelines/**, vendor/librw) — don't touch its files.
`.notes/` gitignored (`git add -f`). Wall-clock cap 100 min. Commit trailer exactly `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Goal (from .notes/MAC_PORTABLE_PROMPT.md / PORTABILITY_GOAL.md)
Same input script + seed -> per-frame hash of player, peds, vehicles, RNG and timer; compiles into BOTH builds (Win32 reference and
portable); Win32 reference traces come from the Windows host on request (PORTABLE_SYNC.md); target: portable trace == Win32 trace.

## Existing pieces to reuse (find and read, don't re-invent)
- Key/command injector used by soak/gameplay_regress and by the Mac boot harness (NOTSA_STANDALONE_INPUT, cmd:… commands; tools/portable/
  mac_boot_run.sh; tools/standalone/soak.sh, gameplay_regress.sh, soak_route; .notes/reports/MAC-BOOT1.md, MAC-W3.md, MAC-W2.md).
- Fixed-timestep / NOVSYNC options, frame limiter, CTimer; RNG: CGeneral/rand (portable msvc_rand.h per-thread LCG, MAC-P4b.md).
- Stream F status (x87 vs portable float: .notes/STREAM_F_PLAN.md, MAC-F4a.md) — divergence is expected until F lands; the harness must
  localise the FIRST diverging frame and field.

## Design questions (answer in .notes/RR_PLAN.md)
1. Determinism sources on each build: wall-clock time (CTimer from performance counter), frame time, streaming/CdStream thread timing,
   audio thread, input timing, uninitialised memory, pointer-order iteration (pools are index-ordered?), hash containers, float mode.
   How to pin each: fixed timestep from the script, synchronous streaming in record/replay mode (LoadAllRequestedModels at fixed points?),
   audio off or deterministic, etc.
2. Script format (frame-indexed inputs + commands incl. seed, new game, teleport…), recorder from live input, player.
3. Hash content and format: per frame a 64-bit hash per subsystem (player pos/heading/health/state, every ped/vehicle in pool order:
   pos, velocity, model, state, RNG state, CTimer counters, game clock, wanted level…) + an optional full dump for the diverging frame;
   endianness-independent, float hashed by bit pattern (and a quantised variant for "close enough" diagnostics).
4. Win32 neutrality: the harness code is behind an opt-in define (e.g. NOTSA_STATEHASH, OFF in StandaloneRelease) so the reference
   build stays bit-identical; the Windows host builds a variant (StandaloneRelease + -DNOTSA_STATEHASH) to produce traces — note: that
   variant's code differs only by the hook calls; argue why its game state is the reference.
5. Diff tool: compare two traces, report first divergence frame/subsystem/entity/field.
6. Slices RR-1..RR-n (one sonnet agent each, ≤ 100 min) with acceptance.

## First slice to implement now (if time allows after the plan): RR-1
Hash module + per-frame hook + trace writer + diff tool, portable build only behind NOTSA_STATEHASH, and a self-determinism test:
run the native exe twice from the same script/seed for N frames (new game, stand still, walk a few seconds — whatever the boot harness
reaches; if New Game still crashes (R-6), use the frontend/attract or a later point and say so) and require identical traces; any
self-divergence is a finding with its source. Win32: no MSVC TU changes beyond guarded hook lines (w0a_guardlines TEXT OK / wine A/B).

## Report
.notes/RR_PLAN.md + .notes/reports/MAC-RR-DESIGN.md; final answer ≤ 15 lines. Leave no background tasks or game processes running.
