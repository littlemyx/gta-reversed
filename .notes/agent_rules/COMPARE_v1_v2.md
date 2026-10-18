# v1 vs v2 — first comparison (2026-10-19, after 8 v2 agents)

Source: metrics.tsv (subagent_tokens / tool_uses / minutes from the completion notices), quota.tsv (weekly all-models %).

| | v1 (41 agents, 10-16..10-18) | v2 (8 agents, 10-18..10-19) |
|---|---:|---:|
| tokens per agent, median / mean | 354k / 371k | 266k / 279k |
| tool calls, median | 170 | 114 |
| minutes, median / mean | 125 / 174 | 69 / 84 |
| agents over the 150-call cap | 28 of 41 (no cap) | 3 of 8 (INFRA-FAST 322, SKIN-FLAKY 218, X87-TRIG 184) |
| parallel agents | 3–5 | 2–3, then 1 from 10-18 20:00 |
| weekly quota pace | ~0.7 %/h (3 agents), ~1 %/h (5) | ~1.2 %/h (2 agents), ~0.5 %/h (1 agent) |

v2 outcomes: ~40 port fixes in 5 fix agents (X87-TRIG 3, ARREST0 9, HEADING-AUDIT 19, SWIM-DUCK 7, W2-OPEN 2) for 1.32M tokens
→ ~33k tokens per fix; 2 infra agents (INFRA-FAST, USERDIR); 1 failed hunt (SKIN-FLAKY, 334k, not reproduced — stopped by the 60-run limit).
v1 for comparison: ORACLE-W2 57 fixes / 494k (~9k per fix, oracle waves are the cheapest per fix); playtest investigations 360–770k per bug.

## Reading
- Smaller tasks are ~25% cheaper and ~2x faster per agent; the real gain is predictability (no 8-hour 770k agents).
- Quota pace per running agent is about the same (~0.5 %/h per agent incl. orchestrator); v2 saves quota mainly by fewer parallel agents
  and by NOVSYNC/USERDIR/gpu_locked cutting wait time, not by cheaper tokens per hour.
- The context-cap hand-off is not followed reliably (3 of 8 overran; X87-TRIG wrote the state file and kept going). Stating the cap as a
  HARD rule in the brief (W2-OPEN) worked once. Candidate for v3: put the cap in every brief as "STOP at 150 calls", count-based.
- Not comparable 1:1: v2 tasks were audits/small fixes, v1 included the big playtest investigations and oracle waves.

Verdict: keep v2 (CURRENT stays v2). Re-check after 8 more agents; consider v3 = v2 + hard cap wording in briefs.
