# RD-NOISE-2 — port self-identity using the Mac RR pins, then the next A-B divergences

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh rd-noise-2 on origin/reverse/interior_c (now contains
lockstep/replay-diff AND the Mac RR harness from portable/main).
## Evidence
.notes/reports/RD-NOISE-state.md + RD-NOISE.md (orig-orig equal prefix 1520-2177; port-port 7-9; port frame 8/19 extra CGeneral::RandomBool /
GetRandomNumber, caller unknown; pin 128 audio partition built, not measured; orig-vs-port: script hash differs at frame 1, rand count at
frame 3 189 vs 163). The Mac RR harness (.notes/RR_PLAN.md, GTASA_STATEHASH, NOTSA_RR_SEED / NOTSA_RR_STEP_MS / NOTSA_RR_NOAUDIO,
tools/portable/rr_diff.py) reaches Win32 port self-identity (6/6 runs) with fixed step + seed + no audio.
## Task
1. Port leg of tools/standalone/replay_diff: reuse the RR pins (do not duplicate: call or mirror the same switches); measure pin 128 and
   NOAUDIO; get port-port equal prefix >= 3000 frames on the intro scenario (3 reps). Resolve the frame-8 RandomBool caller.
2. orig-vs-port: explain the frame-1 script-hash difference and the frame-3 rand count (189 vs 163) with the R logs (call sites on both
   sides); fix port deviations as the exe does (asm address, consumer audit; oracle row if feasible).
Gate (v5): Release + touched targets + soak. HARD budget 250k / 100 min — at 200k stop and land.
## Care
Original exe needs the foreground; never fight USER-PLAY. Never kill by image name. Data under C:/gta-scratch/rd*/.
