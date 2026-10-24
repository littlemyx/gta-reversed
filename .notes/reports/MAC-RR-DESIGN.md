# MAC-RR-DESIGN report (2026-10-24)

Plan: `.notes/RR_PLAN.md` (determinism sources with pins, script/recorder/player, trace format, Win32 neutrality, diff tool, findings, slices RR-2..RR-7, CONTINUE).
Landed on portable/main (pushed to fork): a744c5f5, 90b8671a, 62433a54 = RR-1.

## RR-1 (done)
* `-DGTASA_STATEHASH=ON` (OFF default) -> `NOTSA_STATEHASH`; `source/portable/statehash.{h,cpp}`: virtual clock (CTimer & co advance 20 ms per Idle/FrontendIdle), synchronous CdStream, fixed seed, script clock = virtual clock, audio mixer on the virtual clock (or `NOTSA_RR_NOAUDIO=1`), pool slots and RenderWare allocations zeroed, `NOTSA_PTR_ID32` -> stable pool ids; per-frame trace (timer, clock, RNG word, population, player, camera, peds, vehicles, objects; exact + 1/256-quantised 64-bit hashes, detail window with every field); `cmd:rr_start/rr_stop/rr_stat`.
* Tools: `tools/portable/rr_run.sh`, `rr_selftest.sh`, `rr_diff.py` (+ `rr_diff_test.py`, 12 checks), `rr_route_walk.txt`, `statehash_neutral.py` (preprocessed-text neutrality proof, OK on 9 shared TUs).
* Win32: no TU changes except `#ifdef NOTSA_STATEHASH` blocks; CMake gets one `option()`.

## Self-determinism result
NOT yet 100%. After the pins the run is reproducible except for one binary choice at recorded frame 135 (held prop CJ_CIGGY gets NaN/garbage bone matrix in ~half of the runs; 1 of 12 crashed in `CPhysical::RemoveAndAdd`): bimodal, each mode reproducible (6/12 and 5/12 identical). Sources found and removed: audio hardware time (changed RNG consumer order), machine-wide lock file, frame-limiter gate on a virtual clock, uninitialised `CBike::m_vecAveGroundNormal` (real bug, pinned by zeroing pool slots). Full table: RR_PLAN.md section 9.

## Open / not verified
* F6 cause (skeleton animated before the prop reads the bone matrix?) - see CONTINUE in RR_PLAN.md.
* wine/MSVC code-byte A/B of the changed shared TUs not run (needs the Windows host or ~/tools/msvc); only the preprocessed-text proof.
* No Win32 trace exists yet (RR-3). Windows-side questions are in PORTABLE_SYNC.md (F5 exe constructor, F6 exe behaviour).
* The other agent's uncommitted files in the checkout (audio_cmd.cpp, play_cmd.cpp, TaskSimplePlayHandSignalAnim.cpp) were compiled into my build dir but not touched or committed.
