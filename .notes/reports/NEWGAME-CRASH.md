# NEWGAME-CRASH - new game crashed at HEAD (2026-10-21, 03:25 - ~04:55 WEST)

## Result
* Root cause: `CTheScripts::Process` (0x46A000) walked on from a script that had just been terminated by an earlier script of the same pass. The exe saves `m_pNext` before
  `CRunningScript::Process` and continues only while that successor is active: 0x46A220 `cmp edi,ebx; mov esi,edi; je end; cmp byte [edi+0xC4],bl; jne loop` (`[edi+0xC4]` = m_IsActive).
  The port's `for (it = pActiveScripts, next; it; it = next)` has no such test.
* Trigger (new game): INTRO (`START_NEW_SCRIPT`, t=5644 ms) starts A_TERM at the head of the active list; A_TERM runs `TERMINATE_ALL_SCRIPTS_WITH_THIS_NAME 'A_CONT'` (and A_ALAP), and A_CONT is
  A_TERM's successor. A_CONT moves to the idle list, its m_IsActive is 0; the exe stops the pass there, the port went on from A_CONT into the idle list = every script that ended in the first
  frame (LOWGAME, IMPEXPM, GFINIT, CELLFON, ... 6-7 scripts started by MAIN whose first command is END_THREAD) was processed again from the IP after its END_THREAD.
  CELLFON (ip 162151) then took the SWITCH branch, hit RETURN at m_StackDepth 0 (m_StackDepth wraps to 0xFFFF, `m_IP = m_IPStack[0xFFFF]` = `this + 0x40014` = 2 in the port, outside the 0x5400 byte
  ScriptsArray), and the next `GetAtIPAs` read 0 -> EXCEPTION_ACCESS_VIOLATION reading 0, ECX = 0, EIP at `CRunningScript::Process+0x140` (ProcessOneCommand inlined, `mov ax,[ecx]`).
  The log line before it (`CTxdStore::AddTxdSlot("script")`) is only the last thing the first game frame does.
* Fix: commit 505a4aef (rebased copy on origin, see git log) `CTheScripts::Process 0x46A000`: loop exactly like the exe (next saved before Process, stop when next is null or inactive) and
  `ScriptsUpdated` (0xA447F8) reset at 0x46A028 / incremented per processed script (0x46A1F0), which the port never maintained.
* Verification: scripted new-game route (menu > newgame_loading > cutscene skip > control, NOVSYNC, windowed, silent, private userdir): unfixed HEAD crashes every time (5 of 5 own runs with
  instrumentation + cdb runs + the 3/3 of POSTFX-RASTER), fixed: OK (route reaches the quit). soak.sh 3-minute route on the fixed tree: see the end of this file.

## How it was found (no bisect was needed)
cdb on the 272acc53 Release exe (pdb): fault in `CRunningScript::Process+0x140`, script slot 63 name CELLFON, m_BaseIP null, m_IP 2, m_StackDepth 0xFFFF, m_IsActive 0. A temporary ring buffer of the
executed commands (not committed) showed CELLFON waking at t=5647 with IP just after its own END_THREAD at t=380 and `act=0`; logging the list heads / `next` in the Process loop and in
TerminateThisScript showed A_TERM's saved successor A_CONT already in the idle list (`LOOP it=A_CONT next=<idle head>`). The exe's loop was then read with tools/standalone/exedis.py 0x46A000.

## First bad commit: not named (reasoned, not bisected)
The missing `m_IsActive` test is old (upstream loop, unchanged since 2023/2025: git blame 24fbd0525 / 940d8a627). It was harmless on 2026-10-18 21:27 because the chaos it causes depends on what
the out-of-object read `m_IPStack[0xFFFF]` finds: in the old image layout that read did not produce a faulting IP (or the stale scripts ran into something harmless); the ~480 commits since moved
globals (NOTSA_GLOBAL conversions, HOOK-ADDR moves, new oracle statics), so the value became 2. None of the ~10 script commits since 67087158 (zone commands, Init name copy,
ReadParametersForNewlyStartedScript, ...) touch the loop, the lists, or START_NEW_SCRIPT, and the wave-3 oracle rows were not wrong. A real bisect needs ~9 full-tree rebuilds of ~30 min each
(the incremental build only works for a few TUs; a header-level layout change recompiles ~1200), which does not fit the 100-minute cap; if the exact layout-changing commit is wanted it is a separate job.
