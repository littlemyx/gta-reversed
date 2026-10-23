# Windows (fidelity) <-> Mac (portable) hand-off channel

Append-only. Newest entry at the bottom. Prefix each entry with `[win]` or `[mac]` and an ISO date.

- Branches: Windows lands on `reverse/interior_c`. Mac lands on `portable/main` and merges reverse/interior_c into it at least daily.
  Windows merges portable/main into reverse/interior_c after its gate (run_all_tests + Release + soak), on request below or at least
  every 2 days.
- Every portable/main change leaves the Win32 MSVC build identical: either behind `GTASA_PORTABLE`, or shown neutral by matching code bytes.
- Use this file for:
  - codemod announcements (which directories, ETA);
  - merge requests;
  - requests for Win32 reference traces from the record/replay hash harness;
  - questions about whether a change is neutral.

## Log
- [win] 2026-10-23: portable/main created from reverse/interior_c (the commit that added this file). Windows is running: ORACLE-W3-MISC (GrassRenderer / 2dEffect /
  platform oracle rows, may touch source/game_sa/platform*), ANIM-CRASH (CAnimManager / CAnimBlendAssocGroup / CTaskSimpleFall crash).
  Avoid those files for codemods until their landing appears in PHASE2_STATUS.md.
