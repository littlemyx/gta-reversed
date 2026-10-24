# MAC-P1 — split size/offset asserts: x86 in-memory layout vs file layout (slice P-1)

Host: Apple Silicon Mac. Rules: `.notes/agent_rules/v4.md` (Windows-only parts do not apply). Repo
/Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main` (shared; remote `fork`). Main checkout; agent MAC-C6 works in
parallel there (zones below). `.notes/` is gitignored: `git add -f` new files. Wall-clock cap 100 min.

## Context
- Portable build: CMake preset `PortableMac` (`GTASA_PORTABLE=ON` -> `NOTSA_PORTABLE`), compile_commands at build/PortableMac. Census:
  `python3 tools/standalone/clang_census.py run --mode native --compdb build/PortableMac/compile_commands.json --out DIR -j 8`
  + `clang_census_report.py` (see .notes/reports/MAC-A1.md for the exact command lines). Last run: 1,105 TUs, 404 clean, 3,643 unique
  root diags, P 1,027. STREAM_C_PLAN.md section 5 "P": ~812 failing size/offset asserts in 524 files, 797 of them grow (pointers/size_t).
- `source/Base.h:24-25` defines `VALIDATE_SIZE` / `VALIDATE_OFFSET` (static_assert). ~830 VALIDATE_* uses, ~514 raw
  `static_assert(sizeof|offsetof ...)` lines in source/.
- No-op rule: the Win32 MSVC build must stay bit-identical.

## Design (decided by the orchestrator — implement this, don't redesign)
1. Macro level, no 800-file codemod: `VALIDATE_SIZE` / `VALIDATE_OFFSET` keep their meaning "x86 in-memory layout of the original exe".
   Under `NOTSA_PORTABLE` they compile to nothing (in-memory structs may grow on 64-bit). Under MSVC/x86 unchanged.
2. New `VALIDATE_SIZE_FILE` / `VALIDATE_OFFSET_FILE`: the layout is an ON-DISK / raw-byte format (read or written with fread / CFileMgr::Read
   / memcpy from a file buffer / RwStreamRead into the struct / save-game blocks / IMG directory / COL, IPL-bin, IFP, DAT headers /
   script-file structures). These stay enforced in EVERY build, including portable.
3. Raw `static_assert(sizeof/offsetof)` lines: only those that FAIL in the portable census need a decision; convert each to
   `VALIDATE_SIZE`/`VALIDATE_OFFSET` (in-memory) or the `_FILE` form (file layout). Leave passing ones alone. Converting
   `static_assert(sizeof(T) == N, "...")` -> `VALIDATE_SIZE(T, N)` changes no MSVC code (compile-time only) — still sample-check below.
4. Classification: a struct is FILE-layout if any code path reads/writes its bytes from/to a file or a raw byte buffer that came from a
   file (grep the struct name next to Read/Write/fread/fwrite/memcpy/reinterpret_cast of a buffer, RwStreamRead, CFileMgr, the save
   blocks in GenericGameStorage). Write the evidence (file:line) per FILE struct. Doubt -> FILE.
5. FILE structs that fail on arm64 (contain pointers / long / size_t / bool-ish padding differences) are the P-2 work list: leave their
   `_FILE` assert enforced (it SHOULD fail in portable today) and list them with the offending members. Do NOT add a knob that disables
   `_FILE` asserts.

## Task
- Implement 1–2 in Base.h (one commit). Run the portable census; record P before/after.
- Do 3–4 in batches by top-level directory under source/game_sa (one commit per directory). Before your first batch append a line to
  `.notes/PORTABLE_SYNC.md` ("[mac] <date>: P-1 codemod static_assert(sizeof) -> VALIDATE_* in <dirs>, ETA <time>") and commit+push it.
  Skip files the Windows host has in flight: source/game_sa/platform*, CAnimManager*, CAnimBlendAssocGroup*, CTaskSimpleFall* (list
  skipped asserts in the report). Pull `git fetch fork && git merge fork/portable/main` before each batch.
- No-op proof: wine MSVC A/B code bytes on 6 sampled TUs (Base.h is in the PCH: pick TUs from 3 directories you converted). Method:
  STREAM_C_PLAN section 1 bullet 3 and .notes/reports/MAC-A1.md (it has a working wine A/B script — reuse it).
- Acceptance: census "P: size/offset assert" diagnostics = only the listed FILE structs; MSVC A/B identical; report lists every FILE
  struct with evidence and pass/fail on arm64.

## Zones
- Yours: source/Base.h (the two macros), raw static_assert lines across source/game_sa, source/app (only those lines), the report.
- NOT yours: source/portable/**, winincl.h, Configuration.hpp, SoundManagerDebugModule.*, AESoundManager.cpp, tools/standalone/winstub/**
  (agent MAC-C6, currently uncommitted — don't touch, don't commit them); CMake files, census scripts (only bug fixes, say so).
- Never push to reverse/interior_c; never force-push; commit only your files (`git commit -- <files>`, trailer
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`), push after each commit.
- Background + low priority for census/builds (`taskpolicy -b nice -n 15`, `timeout`). ≤ 1 own sub-agent at a time; if you use one
  for the bulk edits, review its diff line by line before committing.

## Report
`.notes/reports/MAC-P1.md`: macro design, census P before/after, FILE-struct table (struct, evidence, arm64 pass/fail, offending members),
skipped files, no-op table. Return ≤ 15 lines.
