# MAC-P2-DESIGN — design of the 64-bit file-layout fixes (slice P-2), read-only

Host: Apple Silicon Mac. Rules `.notes/agent_rules/v4.md`. Repo /Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main`.
**Read-only on source**: you write only `.notes/P2_PLAN.md` and `.notes/reports/MAC-P2-DESIGN.md` (git add -f, commit -- those files,
trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, push fork portable/main; never force). Agent MAC-A1b has uncommitted
edits in the checkout — don't touch them. Wall-clock cap 90 min. ≤ 1 own read-only sub-agent.

## Input
- `.notes/reports/MAC-P1.md` (FILE-struct table, 26 failing on arm64 + offending members), `.notes/reports/MAC-P1-file-structs.tsv`
  (131 rows with file:line evidence), `.notes/reports/MAC-P1-REV.md` (MemoryHeap HeapBlockDesc invariant).
- Failing FILE structs: CRunningScript, tScriptParam, CPickup, tRadarTrace, CWeapon, CPedSaveStructure, CVehicleSaveStructure, CMatrix,
  CPathNode, BreakInfo_t, BreakablePlugin, ExtraVertColour, tReplayCameraBlock (+ offsetof asserts in Group*.cpp), rw::HAnimKeyFrame
  (fakerw/hanim), CSimpleVariablesSaveStructure (size 1 instead of 312 in the census — find out why).
- Save-game size prefixes written as `size_t` (CPed, CPlayerPed, CPlayerInfo, CObject, GangWars).
- `.notes/PORTABILITY_INVENTORY.md` slices P-2..P-5 and STREAM_C_PLAN.md section 5 "P".

## Questions the plan must answer, per struct
1. Which bytes go to/come from a file, and from which file (save game, main.scm image, IMG/COL/IPL/IDE-bin, IFP, DFF plugin, replay
   buffer)? Is the struct itself the in-memory object (raw fwrite of a live object) or a dedicated file record?
2. What exactly differs on arm64 (pointer members, long, size_t, bool, enum size, padding/alignment, packing)?
3. Recommended fix, choosing the smallest that keeps Win32 MSVC bit-identical:
   (a) member type change to fixed-width with the same x86 size (e.g. pointer-as-`uint32` handle/offset) — only if all users allow;
   (b) a separate `*_File` record + explicit (de)serialisation used only under NOTSA_PORTABLE (MSVC keeps the raw path);
   (c) serialiser used in both builds — only if provably same bytes and same code-bytes risk acceptable (then a wine A/B gate).
   Pointer fields saved by the original (e.g. script IP pointers saved as offsets, pickup object pointers): state what the original
   writes and how to reproduce it from 64-bit pointers (base-relative offset, pool index …).
4. Dependencies/order and conflicts with the Windows host (files it edits often: Scripts, Pickups, Weapons, Peds — anything in the 9
   latest reverse/interior_c commits counts as hot).
Plus: the CSimpleVariablesSaveStructure mystery, the size_t prefixes, and HeapBlockDesc (is CMemoryHeap used by the portable build at all?).

## Output
`.notes/P2_PLAN.md`: one table row per struct (file source, diff, fix kind, files touched, risk, est. size) + ordered slices P-2a..P-2n
each sized for one sonnet agent (≤ 100 min) with acceptance tests (round-trip tests on real save / main.scm / IMG data from the Steam
copy under "~/Library/Application Support/Steam/steamapps/common/grand theft auto - san andreas/…/GTA San Andreas/", never committed).
Return ≤ 15 lines.
