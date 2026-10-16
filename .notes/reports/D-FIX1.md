# D-FIX1 report: full-poison gate (stray reads of converted globals)

Commits: 9f3bcd5c soak.sh (coordinator add-on: private prefixes get a real empty `Documents` dir instead of the symlink to the user's Documents; census.py inherits it via soak.sh; run_all_tests.sh uses the shared ~/.wine-msvc prefix and was left alone) | 98df6ca2 `codemod_globals.py --check-converted` | 4ed5e6ba stray sites | 8cede536 AnimManager. Brief point 9 added to `.notes/DETACH_B_BRIEF.md` (untracked/excluded file).

## Result
| gate | result |
|---|---|
| `--check-converted --boot-dump` (987 converted addresses, whole tree incl. headers and source/standalone) | 25 strays found -> 0 |
| full poison (`NOTSA_POISON_CONVERTED` = boot dump, 894 ranges / 139,391 B), `soak.sh build/StandaloneDetached 3` | RESULT OK three times: before the AnimManager fix (strays fixed only), after it, and inside `census.py run --poison` (8 min); 0 errors, atexit clean |
| census (8 min, full poison) | 4,058 touches, 871/1112 pages: **STRAY_CONVERTED 0 pages**, UNCLAIMED 13, PENDING 858, VTABLE 4 (`.notes/DETACH_census.tsv` refreshed) |
| verify_globals.py on the final exe | 894 globals, 0 mismatches, 0 cross-check failures |
| option OFF object code (build/StandaloneRelease, HEAD source vs working tree, same headers, 16 TUs) | identical (`obj_code_sig.py cmp`: same 16, different 0) |
| Debug config / oracle tests | not rebuilt (the edits are macro-level, same text as the OFF proof; PCH rebuild in Debug > 30 min under the current load) |

## The check (`codemod_globals.py --check-converted [--boot-dump F] [--out TSV] [--fix [--apply]]`)
Converted set = every address argument of a NOTSA_GLOBAL* macro in source/ (owners, aliases, locals, SCOPED, VERIFY); sizes = DETACH_GLOBALS.tsv, aliases.json (globals/extents/new_globals), boot dump (max). Stray = any hex literal in 0x858000..0xCB0000 inside such an extent (owner address or any address inside it) that is not an argument of a NOTSA_ macro: StaticRef / ScopedStaticRef / raw cast / literal, classified; comments and strings stripped; address-only branches (`#else` of `#ifdef NOTSA_GLOBALS_DETACHED`, `#ifndef`) ignored; DEAD_PREFIXES and source/standalone are listed in separate classes. Exit 1 on strays. Extra reports: TEMPLATE-OWNER (owner macro inside a class/function template; mutation tested), MULTI-OWNER (two defining macros for one address; aliases.json views exempt), OVERLAP (converted extent running into a declared, not converted global: the playerImgEntries class of bug), UNKNOWN-SIZE. `--fix` runs the normal codemod with `--only-addr <stray>` per file (tested by reverting a file: restored byte-identical). Limits: only literals (computed addresses and named constants are caught only via the literal they contain); out-of-bounds indexing is invisible to it (see AnimManager).

## Strays (25 sites in 16 files, all of later batches) and the fixes
* aliases.json aliases, converted with the codemod (`--only-addr`): BoundingBoxMargin 0x8D22B0 (PedGeometryAnalyser, TaskComplexWalkRoundCar/Object), CurDistForCam 0x8CCB84 + CarDensityMultiplier 0x8A5B20 (MissionCleanup), Population (0x8A5B20/24), s_Magic1/2 0xC02C14/18 (CustomBuildingDNPipeline/Renderer), ScriptRoute/NextDesiredMoveState/EntityPosChangeThreshold/ScriptPatrolRoute (Group13_15_task, 16_17b, 18, 19, 20a, 20b), NoNode 0x8A5F44 (PathFind). Those files now include DetachedShared.h / the owner header.
* TaskComplexPartnerChat: `StaticRef<int16>(0xB613F8 - count*4)` indexes the image around s_Conversation and with count 4 reaches s_ConversationLength (0xB613E8) and its padding. Detached branch (`#ifdef NOTSA_GLOBALS_DETACHED`, `#line` restores numbering) emulates exactly that window (offsets relative to 0xB613E8); address mode keeps the original text.
* OctTree.h ms_octTreePool: the `#else` branch of the raw-storage owner (address mode only): not a stray (check ignores address-only branches).

## AddAnimBlockRef crash: root cause
`m_BlendHier->m_nAnimBlockId` is -1 for a hierarchy without a block. The exe then does `RefCnt++` on "block -1" = 0x20 bytes before ms_aAnimBlocks = inside the unused last slot of ms_aAnimations (harmless). In the detached exe the C++ object `ms_aAnimBlocks` is preceded by live globals: `&ms_aAnimBlocks[-1].RefCnt` == `&ms_aAnimAssocGroups` (map: 0x2354868 vs 0x235485C), so the low half of the groups pointer was incremented -> AV in BlendAnimation/AddAnimBlockRef. Not an extent/alias/type problem. Fix: detached builds ignore indices outside [0,180) in AddAnimBlockRef / RemoveAnimBlockRef / RemoveAnimBlockRefWithoutDelete (`#ifdef NOTSA_GLOBALS_DETACHED`, `#line` restored; OFF object code identical). Not provoked by poison per se (it needs the -1 path), the first poison run after the stray fixes passed too; the claim rests on the layout analysis plus three green full-poison runs after the fix. General lesson: exe code that indexes/writes out of bounds into neighbouring globals depends on the exe's layout; the check cannot see it (grep for `[-1]`-like uses when a poison soak crashes inside an accessor).

## Class-template statics (D-B06 item)
`--check-converted` finds 0 TEMPLATE-OWNER hits in the tree. Checkpoints.h, AERadioTrackManager.h, AEPoliceScannerAudioEntity.h only contain helper templates (`MakeZeroCheckpoints`, `kScannerZero<T>`, `kRadioIndexHistoryZero<T>`, a ring-buffer class template without NOTSA_GLOBAL members); their globals are ordinary namespace/class statics, one object per address. QuadTreeNode.h (the real bug) is already the shared synthetic.

## Open / notes
* Builds under load 20-35 took 50-90 min per full relink of build/StandaloneDetached (ninja alarm raised to 7000 s; a 3000 s alarm killed one build and left "premature end of file; recovering").
* Soak gates: `[until] invehicle TIMEOUT` (2 gate timeouts) in every run - the car-entry step never gets its condition (known, route issue; RESULT stays OK).
* 1 UNKNOWN-SIZE address (0xB61368 = ScopedStaticRef flag word, dropped in detached mode): harmless.
* Unclaimed census pages: 13 (anim association tables etc., as in D-A3), to be assigned in phase D/E.
