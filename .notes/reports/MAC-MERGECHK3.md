# MAC-MERGECHK3 (2026-10-27): portable build after the merge of reverse/interior_c (175 commits, a186b6ff)
- 31 source files changed by the merge. PortableMac ninja: FAILED as landed (1 error, Automobile.cpp), green after the fixes (link ok).
- ctest -L portable: 19/19 passed. ctest -L mutation: 12/12 passed (0 failed), both re-run after the fixes.
## Fixes (commit e97af1f5), with the convention each belongs to
| file | problem | convention |
|---|---|---|
| Automobile.cpp:2698 | `static_assert(offsetof(CAutomobile, m_swingingChassis) == 0x70C)` fails on LP64 (offset differs) | layout asserts: `VALIDATE_LAYOUT(...)` (x86 in-memory layout) |
| Object.cpp, GroupEventHandler.cpp | MERGE DEFECT: resolving the 2 conflicts took the whole file from the Windows side; every portable edit outside the conflict hunks was silently lost: 7x `NOTSA_FTOL(uint8, ...)` / `rope.m_fSegmentLength * 32.0F`, 2x `GetObjectPool()->NotsaCheckAllocSize` (STAB1 pool alloc-size check), `-NOTSA_FTOL(int32, rand*...)`. Restored from `git merge-tree` (clean auto-merge), only the 2 conflict hunks keep the Windows body (DoBurnEffect now `NOTSA_FTOL_SC(int32, ... * 0.05F)`) | float->int: NOTSA_FTOL; Pool.h portable check |
| Population.cpp:816 | new `(int32)std::floor((double)(v*0.02f+60.f))` sector index (class A) | x87d + `NOTSA_FTOL_P(int32, std::floor((x87d)(...)))` |
| Population.cpp:775, 1694 | older portable conversions that were not token-faithful: `(int32)lim` spelled as `NOTSA_FTOL(int32, lim)` (extra parens), `static_cast<int32>(...)` spelled as C-cast `NOTSA_FTOL` | NOTSA_FTOL_P / NOTSA_FTOL_SC (keep the original spelling) |
| World.cpp:982-985 | new `static_cast<int32>(GetLodSectorfX/Y(...))` loop bounds of FindLodOfTypeInRange (class A) | NOTSA_FTOL_SC |
| GroupEventHandler.cpp:361 | new `-(int32)(rand*(1/32768)*500.f)` (class B) | NOTSA_FTOL |
| TaskSimpleIKChain.cpp:142 | new `(int32)CTimer::GetTimeStepInMS()` (class B) | NOTSA_FTOL_P |
- Win32: wine cl.exe code+relocs A/B (ftol_ab.py): vs HEAD^2 (the Windows tip) IDENTICAL on Object, Automobile, GroupEventHandler, TaskSimpleIKChain, World; vs HEAD (pre-fix merge) IDENTICAL on Population.
  Population vs HEAD^2 shows DIFFERENT only because of __LINE__-style shifts from the merge (a HEAD^2 file + HEAD headers = IDENTICAL; vs HEAD with my edits = IDENTICAL) - not an edit effect.
- Scans on the 30 changed TUs: pc24_scan: new AUTO site only Population:816 (converted); Physical.cpp:2461 already DONE; the rest of the merge code already carries x87d/X87R. ftol_scan: A = Population:816, World:982-985; B = GroupEventHandler:361, IKChain:142 - all converted. No new Win32 API / REPLAY / Lockstep code outside guards (Lockstep.cpp +7 lines sit in the existing `!NOTSA_PORTABLE` guard).
## Boot check
- UserIsActive was 0, ran under `caffeinate -dimsu` (killed). tools/portable/play1 s1: run 1 HUNG (stuck after "Loading camera paths for cutscene:" with an empty name in the PROLOG3 intro cutscene, hang dump at timeout-2, exit -6, ~58 s idle in SDL_WaitEvent); runs 2, 3 and 4 (fresh OUT dir) exit 0 after 30.8 s, "Loading the Game" present. Run 1 could not be reproduced (display was waking up); not analysed further.
## For the Windows host
- The gate would have caught: layout assert (PortableWin64 compile), static_cast/C-cast spellings (not catchable by build; ftol_scan). The merge itself must be done with `git merge-tree` to compare: resolve conflicts hunk by hunk, never take a whole file.
No background tasks, caffeinate, wine or game processes left.
