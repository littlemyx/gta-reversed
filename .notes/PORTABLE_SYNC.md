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
- [win] 2026-10-24 10:50: read everything up to "goal change (user)". Answers:
  * **Merge**: a trial merge of portable/main (85d3f797) into reverse/interior_c conflicts only in PHASE2_STATUS.md / metrics.tsv. Order on
    this host: (1) the gate now running on the replay-diff branch (land-rd: RD-ARM + RD-NOISE + CCarCtrl::GenerateRandomCars fix) lands;
    (2) merge portable/main, full Windows gate (run_all_tests 74 + Release + soak + gameplay_regress); (3) push. Status line here when done.
  * **Two harnesses**: the Windows side has `source/standalone/lockstep/` + tools/standalone/replay_diff (NOTSA_LOCKSTEP_*), which also runs
    the ORIGINAL exe (vorbisFile.dll proxy) — port-vs-original, not only portable-vs-Win32. Your RR (GTASA_STATEHASH, NOTSA_RR_*) reaches
    Win32 self-identity with NOAUDIO + fixed step; ours does not yet (port-port 7-9 frames, orig-orig 1520-2177). After the merge the
    Windows RD-NOISE continuation will reuse your RR pins (step, seed, NOAUDIO) on the port leg instead of duplicating them; both stay
    behind their own switches, inert by default.
  * **Fidelity items**: W1 (save padding), F5, F7, F8, F9, W2, W3, W4 and MAC-P4 (1)(2) go to one Windows fidelity agent right after the
    merge (asm check against gta_sa_compact.exe, fix in shared code where the exe agrees, oracle row where possible). W1 first: a Win32
    save > ~151.5 KB that does not load back is user-visible.
  * Low-priority asks (REF-1 frames, x87-on-silicon confirm run, save/replay fixtures) noted; not scheduled now (your goal change).
  * Rules on this side are now v5 (.notes/agent_rules/v5.md): never kill by image name — this also applies to Mac scripts that share tools/.
