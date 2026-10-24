# MAC-BOOT1: path layer (W-7) + boot loop of the native PortableMac exe

Branch portable/main, commits 6e8de59c + d2b9c124 (pushed to fork). Harness: `tools/portable/mac_boot_run.sh <tag> <timeout_s> ["input script"]` (overlay cwd with symlinks to the Steam data, own logs/, user files in $OUT/userdir; NOVSYNC; hang-dump; hard kill). Scratch: /private/tmp/claude-501/boot/.

## Result
The exe now boots through game init to the MAIN MENU (SCREEN_MAIN_MENU=34, menu active, gameState FRONTEND, ~0.3 s after start; verified by logging screen ids while driving it with the key injector). "Start game > New game" starts CGame::Initialise: IDE/IPL/TXD slot registration of the whole map runs, then it stops in CCarFXRenderer::InitialiseDirtTexture (render, see below).

## Fixes (all NOTSA_PORTABLE-only; Windows view proven text-identical with w0a_guardlines.py check: FileMgr.cpp/.h, AEMP3TrackLoader.cpp, FxManager.cpp, stream.cpp = TEXT OK)
| # | blocker | fix |
|---|---|---|
| 1 | `CFileMgr::OpenFile("models\\coll\\peds.col")` null (no '\\'->'/', no case folding) | `source/portable/path_resolve.{h,cpp}` (std only): '\\'->'/', per-component case-insensitive match against the real directory (cached listings, invalidated by dir mtime), `..`/`.`/`//`, missing tail kept verbatim (write modes create with the game's spelling), absolute paths, thread safe (CdStream thread), **Win32 trailing space/dot normalisation** (`IDE DATA\\DEFAULT.IDE ` in default.dat). Wired in: CFileMgr::OpenFile / ChangeDir / SetDir / SetDirMyDocuments, CdStreamOpen (cdstream_portable.cpp), the RW file interface (`FsOpen`/`FsExist` in standalone/rw/stream.cpp = RwStreamOpen of TXDs) and librw's `filefuncs.rwfopen` (engine_gl3.cpp), AEMP3TrackLoader (STRMPAKS/TRAKLKUP; a missing file is no longer CREATED in the game dir), FxManager sanity check. |
| 2 | root dir buffers were 128 bytes (Steam path ~220) | portable: `DIRNAMELENGTH`=1024, `ms_dirName`/`ms_rootDirName` char[1024], root terminated with '/' |
| 3 | `PedEvent.txt` etc.: Windows text mode (`"r"`) translates CRLF; POSIX does not => blank "\\r\\n" lines counted => `m_EventIndices` wrong => heap overflow in CDecision::SetDefault (found with an ASan build; looked like a smashed stack) | `OpenResolvedText`: "r"/"rt" = MSVC text mode (CRLF->LF, Ctrl-Z = EOF) via an in-memory funopen stream; CFileMgr::OpenFile uses it |
| 4 | `CAudioEngine::Initialise()` false (DirectSoundCreate8 failed) => audio tables unset => frontend null deref | `source/portable/dsound_null.cpp`: a SILENT software DirectSound device (buffers own memory with Lock wrap-around, play/write cursor advance on the steady clock, DS value ranges, 3D buffer / listener objects, no EAX/FX). The whole audio engine runs; nothing is played. W-3 (OpenAL) replaces its role. |
| 5 | diagnostics: lldb/sample cannot attach (hang forever), crash handler's logger dump itself crashed | app_platform.cpp: raw async-safe first dump (signal, fault addr, backtrace + frames of the interrupted code from ucontext), `NOTSA_HANG_DUMP_SECS=<n>` turns a hang into a main-thread stack; `CFileMgr::OpenFile ... failed (errno)` warning |
Host test: `tools/portable/run_path_resolve_test.sh [mutate]` = tests/portable/path_resolve_test.cpp, 62 checks (mixed case, backslashes, missing file, '..', abs, cwd-relative chdir, text mode, write modes, cache refresh, threads); 9 mutants all killed. NOT yet in portable_tests.cmake (P4b's file; add `path_resolve` to the three lists).
Writes: nothing goes into the Steam copy by the exe (settings/saves: `<Documents>/GTA San Andreas User Files/` as the exe, or `NOTSA_STANDALONE_USERDIR`). My very first run (r1, before the overlay existed) appended to `standalone.log` and `logs/log.log` in the Steam dir; both files were created there by MAC-BUILD1 runs at 02:46, they are not part of the install and can be deleted.

## Run log
| run | reached | blocker |
|---|---|---|
| r1 (45 s) | RwEngineStart, CFont::Initialise (LoadTxd "models\\fonts.txd" via fopen) | RW file interface had no path layer; process sat in open() (loop) |
| r5-r7 | text/handling/surface/pedstats load | SIGSEGV/SIGBUS in changing places = heap overflow in CDecision::SetDefault (CRLF, fix 3); crash handler's own logging crashed (fix 5) |
| r11-r12 | LoadDefaultDecisionMaker finished | stack/heap garbage on return (same cause) |
| r13 | InitialiseCoreDataAfterRW ok | `Failed to initialise Audio Hardware` then CAEFrontendAudioEntity::AddAudioEvent null deref (fix 4) |
| r14-r16 | frontend loop (FrontendIdle) runs, no crash for 60 s | none (blank window then: Im2D stub; later R-4 landed) |
| r19-r21 | first CMenuManager::Process never returns with vsync | SDL3 Cocoa SwapWindow waits forever for a CVDisplayLink tick (display asleep / window not visible). NOTSA_STANDALONE_NOVSYNC=1 for agent runs. Real foreground run not verified. |
| r22-r23 | main menu, key injector works (injected "return" opened Options: the pointer hovers a row; use `key:up` first) | none |
| r24-r25 | New Game: CGame::Initialise -> LoadLevel | `IDE DATA\\DEFAULT.IDE ` trailing blank => null FILE* in LoadObjectTypes (fix 1, Win32 normalisation) |
| r26-r28 (final) | LoadLevel registers ~2900 TXD slots, vehicles.ide, then CCarFXRenderer::InitialiseDirtTexture | **SIGSEGV in CClothesBuilder::CopyTexture**: `RwRasterLock(srcRaster, 0, READ)` of the 256x256x32 `vehiclegrunge256` texture raster returns NULL (dst lock works). Backtrace: CopyTexture <- CCarFXRenderer::InitialiseDirtTexture <- CFileLoader::LoadLevel <- CGame::Initialise <- InitialiseGame. |

## Next blockers
| owner | blocker |
|---|---|
| R (raster_gl3 / standalone/rw/raster.cpp RwRasterLock) | READ lock of an uploaded texture raster returns NULL (needs a CPU copy / glGetTexImage); also `GLD_TEXTURE_INDEX_2D is unloadable` driver warning at the first frame; RwShimPipelineEnsure (R-6) stub still logged |
| W | W-3 OpenAL hardware layer (dsound_null.cpp is the silent stand-in); W-4 user-track decoders (W-B5 log); real foreground check that vsync swap works (CVDisplayLink) or a timeout/own pacing instead of SDL's unbounded wait |
| P/F | data quirks that are NOT bugs: "Bad timecyc line" (R* data), vehicles.ide skimmer line has 11 of 14 fields |
| tests | add `path_resolve` to tools/portable/portable_tests.cmake (names/scripts/mutate lists) |
