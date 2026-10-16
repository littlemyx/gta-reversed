# Brief for stream-D codemod batches (B01..B20) — read fully before starting
Repo /Users/Andrei.Mukhin/github/recomp/gta-reversed, branch reverse/interior_c. Read first: `.notes/PORT_BRIEF.md` (rules, build mutex,
`perl -e 'alarm shift; exec @ARGV' 2400 …` timeout, report format), `.notes/DETACH_DATA_PLAN.md` §3.3–3.4, §4 Phase B, §8,
`.notes/reports/D-A2.md` (codemod usage, verifier, obj_code_sig.py), `.notes/reports/D-A4.md` (aliases.json schema).
gta_sa_compact.exe at repo root is dev-time input only — NEVER commit it.

Per batch:
1. Get your batch's exact file list from the generator (`batches.json` / gen_globals.py), convert ONLY categories a/b/c with
   `tools/standalone/codemod_globals.py` following `.notes/aliases.json`; d-objects stay address-mode StaticRef (Phase C).
2. Review every rewritten line by hand (types, extents, `inline` vs `static`, zero-init for rows in `.notes/DETACH_ctor_nonzero.tsv`,
   initial values vs the exe image). Under-declared extents from aliases.json must be applied.
3. Acceptance:
   a) option ON (`build/StandaloneDetached`, preset StandaloneDetached): `verify_globals.py` 0 mismatches for all globals converted so far;
      the exe boots to the main menu (SKIP_VIDEOS + screenshot, docs/STANDALONE.md);
   b) option OFF: `obj_code_sig.py` identical object code for your batch's TUs vs the pre-batch commit;
   c) Debug DLL config compiles your TUs.
4. Commit per header/file group (`git commit -- <files>`; shared PCH headers alone), message ending
   `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Delete stale `cmake_pch.cxx.*` after header changes. ninja -j6 max.
5. Never touch files of other batches or of running non-D agents (listed in your launch prompt); never revert foreign uncommitted edits.
Report: full report `.notes/reports/D-<BATCH>.md`; final message ≤15 lines (globals converted, verify result, boot, obj-identical, open items).
6. Lessons from B11 (`.notes/reports/D-B11.md`): header `extern T& name;` must become `NOTSA_GLOBAL_EXTERN(name, (T))` — the codemod does it
   for the sibling `<stem>.h` only; externs in headers with a different stem must be fixed by hand (grep the name). Base.h and PCH headers:
   delete stale PCHs in EVERY build dir you build. Run `verify_globals.py` over ALL converted globals so far (262 after B11), not only yours.
7. The oracle/test targets (tests/standalone/*_oracle_test, built in build/StandaloneRW) link game TUs too: after your batch they must still
   LINK in address mode (e.g. world_oracle_test broke on AEAudioChannel.h g_numSoundChannelsUsed during B01/B02). Build at least
   world_oracle_test and physics_oracle_test before you report.
8. From B01: header `inline auto&` -> `NOTSA_GLOBAL_HDR_EXT` (codemod emits it); `NOTSA_SCOPED_GLOBAL` in address mode may lack `static` —
   the first batch that uses it checks with obj_code_sig.py and fixes Base.h. verify_globals.py cross-checks other batches' rows: report
   failures on rows that aren't yours, don't fix them.
9. From D-FIX1: **stray check before every report.** `python3 -I tools/standalone/codemod_globals.py --check-converted [--boot-dump <NOTSA_VERIFY_GLOBALS dump>] [--fix [--apply]]`
   scans the WHOLE tree (all batches, headers, source/standalone) for address-mode uses (StaticRef / ScopedStaticRef / raw cast / any hex literal) of a converted global's
   address OR of any address inside its extent, and must print `0 strays` (exit 0). A stray = an alias/expr site in a file of a later batch (BoundingBoxMargin, CurDistForCam,
   s_Magic1/2, ScriptRoute ... were exactly that): convert it to the owner NOW, whatever batch the file belongs to (`--fix --apply` runs the normal codemod with
   `--only-addr <stray>`; indexed/computed accesses and raw-storage owners are hand edits: a detached-only branch inside `#ifdef NOTSA_GLOBALS_DETACHED` is invisible to
   the check, the address-mode branch is not). The same run also reports TEMPLATE-OWNER (a NOTSA_GLOBAL owner inside a class/function template = one variable per
   instantiation: use a namespace-scope NOTSA_GLOBAL_SYNTH + NOTSA_GLOBAL_ALIAS), MULTI-OWNER (two defining macros for one address = two objects; "views" exempt) and OVERLAP
   (a converted extent running into a not yet converted declared global: the poison / the new storage would overlay live image data). After the build also run the
   full-poison soak (`NOTSA_POISON_CONVERTED=<boot dump of YOUR build>`, all ranges, `tools/standalone/soak.sh build/StandaloneDetached 3`) and `census.py` (STRAY_CONVERTED must stay 0).
