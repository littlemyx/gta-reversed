# TP-HEAP — heap corruption (0xc0000374) after a teleport late in the soak route

Rules: .notes/agent_rules/v5.md. Worktree via tools/standalone/windows/new_worktree.sh tp-heap on origin/reverse/interior_c.
## Evidence (.notes/reports/ROUTE-FLAKES.md section 3)
Inserting `cmd:tp:2100:-1790:13.5` into the soak route AFTER the 3-min walk crashed 3 of 3 runs with APPCRASH 0xc0000374 (heap corruption,
ntdll+0xfd71f), ~10 s after the tp, at the quit sequence or at idle start. Same exe, unmodified route: 0 of 2 (and all gate soaks OK).
gameplay_regress scenarios tp early in a game and do not crash. Repro route: C:/gta-scratch/rf_route_tp.txt (copy it), exe/data:
C:/gta-scratch/rf_bin, C:/gta-rf. A real port bug is likely: a far teleport after a long session = mass streaming-out of models/
collision/IPL around the old position while entities still reference them (cf. 2026-10-23 shadow coldata garbage, AllocateData).
## Task
Catch the first corruption, not the late detection: run the repro under cdb with the page heap for the exe only — use the debugger's own
heap options (`cdb -hd`, or gflags via cdb `!gflag`), NOT a system-wide gflags/registry change (do not modify system settings). Or the
port's own allocator checks if there are any (CMemoryMgr / pools). Get the writer's stack, compare that code path with the exe, fix as the
exe does (asm address, consumer audit). Acceptance: 5 of 5 repro runs clean after the fix (pre-fix 3/3 crash); soak OK.
HARD budget 250k tokens / 100 min. Work ONLY in your worktree; never kill by image name; wait on gpu.lock (RD-AB-10 runs games too).
Do not touch tools/standalone/replay_diff/ or the lockstep code.
