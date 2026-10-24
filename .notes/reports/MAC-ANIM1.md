# MAC-ANIM1: "corrupted" cutscene key frames -> new-game crash (CWorld::Process -> CPhysical::RemoveAndAdd) - FIXED (portable) - 2026-10-24
Rules v4 (Windows-only parts skipped). Branch portable/main. Scratch /private/tmp/claude-501/anim1/, build dir build/PortableMacAnim1, harness copy in `.notes/reports/MAC-ANIM1_harness/` (batch.sh, boot.sh, run1.sh, cload.sh, ng2.txt, diag.patch).

## Result
No heap corruption, no loader / LRU / RemoveLastAnimFile bug. The "foreign bytes" are IN THE IFP FILE, and the crash comes from an unbounded sector loop that the exe survives by luck of an x87 conversion.

1. `anim/cuts.img` PROLOG1.IFP (ANPK, KRT0) stores uninitialised exporter memory in the LAST key frame of at least 60 of its 183 sequences (csplay 33, csstew 19, cstaxi92 6, cssuitcase-2 2): rotation/translation 1e20..1e38, ASCII ("bump_front_", "D:\alienbrai", "Selected\0TG2"), time stamp 22.3333 = the real end. The ANIM headers carry MSVC debug fill (0xF6F6F6 / 0xF8F8F8). Parsed offline with a 40-line python reader of the file (sizes of KRT0 sections = frames*32, all consistent: the loader reads exactly what is there).
   The four offenders MAC-RACE1 captured match the file byte for byte: csplay " Neck" (625 fr) last Trans (2.9393e29, 1.7751e28, 8.02819); csplay " L Hand" (635) (6.977e22, 1.693e22, 29.5996); cstaxi92 wheel01 (42) (4.746e27, 4.563e30, 1.761e19); cstaxi92 door_rr_hi_ok (27) (2.539e20, 1.775e28, 1.703e25).
2. When the anim reaches its end `I_NextKeyFrame` parks every node on the last frame (cur = N-1, remaining 0, t = 1), so the junk frame is applied for 1+ frames; with a frame step of 0.047 s (assoc curTime 22.2867 + 0.047 >= total 22.3333) it is certain, with other frame patterns it may fall after the cutscene teardown, hence "load dependent". Retail has the same data and the same evaluation (glitch of a frame at the very end).
3. `UpdateCutsceneObjectBoundingBox` -> radius ~ sqrt(FLT_MAX); `CPhysical::RemoveAndAdd` / `Add` bound rect +-1.8e19; the loops `for sectorY.. for sectorX.. new CEntryInfoNode` run `PhysicalGetSectorIdx` = `static_cast<int32>(floor(v*0.02f+60))`. arm64 saturates -> INT_MIN..INT_MAX -> pool of 500 empty -> `new` null -> AV (fault address 0, RemoveAndAdd). The exe converts with `floor` + `_ftol2` (fistp qword, low dword; 0x5425AF..0x5425B4), i.e. a wrap, not a saturation.
   Even a 16x16-sector span per object exhausts the pool (several cutscene objects are re-added per frame): a plain "clamp to 16" is not enough (tested, still crashes with radius 1e10 / 3e5).

## Fix (portable only) - commit see PORTABLE_SYNC / git log
New header `source/game_sa/Entity/PhysicalSectors.h`: `notsa::PhysicalSectorIdx(v)` (NOTSA_PORTABLE: `notsa::fp::Ftol(floor(...))`, else the old cast) and `notsa::PhysicalLimitSectorSpan(min, max)` (NOTSA_PORTABLE: a span of more than 16 sectors, impossible for a real entity, collapses to its first sector; else identity). Physical.cpp: 1 line in `PhysicalGetSectorIdx` + the 4 `maxSector*` lines of `Add` / `RemoveAndAdd` (line count unchanged). Real entities (span <= 16) behave as before.
Win32 neutrality: wine cl.exe (/O2 /arch:IA32, flags of build/StandaloneRelease), HEAD vs patched Physical.cpp, llvm-objdump of the whole object: all existing functions and all `__LINE__` immediates identical; the only difference is two unreferenced `inline` COMDATs (the helpers; dead-stripped by /OPT:REF).
Consumer audit: sector index readers = `Add`, `RemoveAndAdd` only (`PhysicalGetSectorIdx`). Same float->int idiom elsewhere, not junk-driven (entity positions): Shadows.cpp:1593, EntityScanner.cpp:62 (not changed). `PhysicalGetRepeatSector` (& 0xF) unchanged; `Remove` walks the collision list, no rect.

## Evidence
| Experiment | Result |
|---|---|
| Offline parse of PROLOG1.IFP | 60 junk last frames; 4/4 RACE1 offenders matched |
| Load-time sanity scan + per-sequence checksum verifier (|rot|, translation, delta time of uncompressed; translation of compressed; 183 seqs; every 20 frames and at the BAD moment), 6 full runs incl. 3 under load avg 40 | 0 changes. Legit writes seen and excluded: whole-rotation sign flip (CalcThetaFromQuats / CalcDeltasCompressed) and the lazy absolute->delta time conversion of compressed anims (CalcTotalTimeCompressed) |
| malloc poison (0x7F on alloc, 0x7E on free) | the bad frame values are NOT 0x7F7F/0x7E7E: the bytes come from the file |
| Forced bound radius (diag env at cutscene time > 6 s), Ftol only, no span clamp | 1e10: crash at 8 s; 3e5: crash at 7 s; 1e13 and 1.8e19: survive (float ulp luck) |
| Same, with the span clamp | 1e10, 1e13, 3e5, 1.8e19: 4/4 survive 47 s |
| Baseline exe (HEAD 820fbc9c build) under 24 compile workers, load avg 41-53, 6 runs | 6/6 crash in RemoveAndAdd |
| **Final exe, same load (24 workers, load avg 47-55), 20 new-game runs, 40 s after "newgame_started"** | **20/20 exit 0, "Loading the Game" present in all, 0 crashes** (no run discarded) |
| exe + verifier, 3 runs under load | verifier clean (0 ANIMSUM / DIFF) |

Harness notes: `NGSCRIPT=ng2.txt` must be passed (run1.sh defaulted to a missing ng.txt: an idle menu looks like a hang); `caffeinate -dimsu` held for the whole session; the screen is LOCKED (CGSSessionScreenIsLocked) but the game rendered fine with the display kept awake. Run = 47.7 s (8 s start + 40 s).

## Not the same as ANIM-CRASH / CRASH-HUNT
That family (UncompressAnimation <- CopyAnimation <- BlendAnimation <- CTaskSimpleFall, null `m_BlendHier`) is a null static-association slot request; no huge floats. Not reproduced or touched here.

## Files
Source: `source/game_sa/Entity/Physical.cpp`, `source/game_sa/Entity/PhysicalSectors.h` (new). Notes: `.notes/PORTABLE_SYNC.md` (entry for the Windows host with the proposed shared fix: apply `notsa::PhysicalLimitSectorSpan` unconditionally if wanted), this report, `.notes/reports/MAC-ANIM1_harness/`. No background tasks, load generators, caffeinate or game processes left.
