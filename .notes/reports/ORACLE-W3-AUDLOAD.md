# ORACLE-W3-AUDLOAD report

Agent ORACLE-W3-AUDLOAD (rules v4), start 15:43, end see the last line (2026-10-23), worktree C:/gta-wt/oracle-w3-audload (base origin b62ce34e). Logs C:/gta-scratch/audload/.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game (tool count) | 4751 | **4786** (+35) |
| game_sa/Audio/Loaders covered / uncovered A / B | 0 / 38 / 0 | **35 / 3 / 0** |
| port bugs fixed | | **6 commits, 7 functions (+ 4 delegating twins)** |

## New target `wave3_audload_oracle_test` (35 functions, PC24 0 mismatches, ~190 s wall clock)
Same W2/W3-audio harness as `wave3_audio_oracle_test` (flag `W3_AUDLOAD`, rows `tests/standalone/wave3_oracle_audload.inc`, the loader TUs of Audio/Loaders linked for real, the exe-global/stub plumbing of `wave3_oracle_audio.inc` reused). None of the A functions does file IO, so the "fake bank" is the compared fixture itself: a CAEMP3BankLoader object, 4 bank slots (0x12D4 bytes, 400 sounds each with cumulative offsets), 64 bank lookups, a 32 KB buffer, a 50-entry request queue and the m_BankSlotSound table, all garbage-mixed with game-range values (bank / sound biased to hit the "already loaded" and "duplicate request" paths). Decoder objects (WMA is 0x20038 bytes) live in a separate buffer; rows are pure getters compared through the return value. spdlog is switched off in the target (the port's warnings allocate from the compared arena; the exe has none).
Rows: CAEMP3BankLoader GetBankSlotBuffer / GetSoundHeadroom / IsSoundBankLoaded / GetSoundBankLoadingStatus / IsSoundLoaded / GetSoundLoadingStatus / GetSoundBuffer / LoadSoundBank / LoadSound, CAEBankLoader GetBankLookup (valid calls: the exe returns null for bank >= count, the port asserts) / CalculateBankSlotsInfosOffsets, CAEStreamTransformer Initialise / TransformBuffer, CAEDataStream Constructor (vptr and the unread m_pFileHandle zeroed after the call) and the 8 E_NOTIMPL IStream stubs (stdcall, argument sizes checked by the `ret N`), CAEMP3TrackLoader Constructor, CAEMFDecoder / CAEVorbisDecoder / CAEWMADecoder / CAEWaveDecoder getters (stream id taken through the data-stream pointer the exe reads), WMA GetStreamPlayTimeMs (64-bit sums, x87), Wave GetStreamLengthMs.

## Port bugs fixed (one commit each, asm address in the subject; pre-fix = PC24 failing cases of the final rows on the unfixed code, run3)
| function | asm | pre-fix | cause / fix |
|---|---|---:|---|
| CAEMP3BankLoader::GetBankSlotBuffer | 0x4DFDE0 | 407/1200 | slot >= count: the exe writes 0 to outLength, the port left it; unsigned compare against the signed count |
| IsSoundBankLoaded / GetSoundBankLoadingStatus | 0x4E0220 / 0x4E0250 | 50 / 50 of 1200 | bank argument zero-extended vs slot value sign-extended: bank -1 (UNK) is never "loaded" in the exe, the port said true for an empty slot |
| IsSoundLoaded / GetSoundLoadingStatus | 0x4E03B0 / 0x4E0400 | 3 / 3 of 1500 | same for bank and sound id (-1 never matches) |
| GetSoundBuffer | 0x4E0280 | 430/2500 | sound id movzx (negative id: null, the port indexed Sounds[-1]); slot not range checked (port: null from GetBankSlotBuffer + offset = garbage pointer); raw array indexing; two debug asserts that are not in the exe removed |
| LoadSoundBank / LoadSound | 0x4E0670 / 0x4E07A0 | 41/3000, 5/3000 (fault) | exe order: init, sound id, bank, slot range checks, then the loaded test (the port looked at the slot first: negative slot = std::array fault); LoadSoundBank's duplicate scan compares bank+slot only (the port also demanded SoundID -1, so a pending single-sound request did not block a bank request) |
| CAEWaveDecoder::GetStreamLengthMs | 0x503310 | 710/1500 | exe: data bytes / (sampleRate * 4) * 1000 in x87 (fild, +2^32 for "negative", quotient rounded once), blockAlign ignored; the port divided by blockAlign (integer) and by the rate |
Consumer audit (per commit message): GetBankSlotBuffer has no external caller; the Is*Loaded / *LoadingStatus family is read by CAEAudioHardware, AudioEngine, collision / door / frontend entities and CAEScriptAudioEntity (already casts ids to uint16, i.e. the exe convention); GetSoundBuffer only by CAEAudioHardware::PlaySound (null -> hwNoBuffer); requests only by CAEMP3BankLoader::Service (same TU, reads the same fields); the wave length only through the streaming-decoder virtual. No layout / index / size convention of shared state changed. Game check: see the Gate section.

## Not covered / observed, not changed
- tests/standalone/tools/w3_audload_skip.tsv: CAEMP3BankLoader::Constructor (port also stores NSDMI -1 defaults the exe zeroes, overwritten by Initialise before any read), CAEMFDecoder::SetCursor / GetStreamPlayTimeMs (QuickTime replaced by MediaFoundation by design).
- Request queue padding: the port's struct assignment writes the 1-byte tail pad of CAESoundRequest (and zeroes StreamBuf/StreamData/BankNumBytes), the exe leaves them; rows zero the pad on both sides, the other dead fields are zero in the fixture (never read before Service sets them). CAEDataStream ctor: the port clears m_pFileHandle, the exe leaves it; every reader tests m_bIsOpen first except GetCurrentPosition (called after Initialise) - masked.
- CAEStreamTransformer::Initialise: the exe builds the table as base[i] ^ i, the port stores the precomputed result: identical bytes (row ok).
- WMA GetStreamPlayTimeMs: PC24 0, PC53 1720/3000 (informational): the port converts the 64-bit sum to float before the multiply, the exe multiplies in extended precision; game mode (PC24) is exact. Wave length PC53 158/1500, same reason.
- Class D, not oracle-able: CAEDataStream FillBuffer / Seek / GetCurrentPosition / Stat (the exe uses ReadFile / SetFilePointer imports, the port CFileMgr), the decoders' FillBuffer / SetCursor, bank loader Initialise / Service / file loaders. Checked against the asm by eye instead: FillBuffer (`min(size, cur - length - start)` unsigned, then TransformBuffer(dest, read, cur) and cur += read), Seek (SEEK_SET adds start, SEEK_END -> (length + start) - offset as SEEK_SET) and GetCurrentPosition (tell - start) match the port line for line.

## Gate
- `wave3_audload_oracle_test` (final tree = the pushed commits): 35 functions, PC24 0 / 0 mismatches, 190 s wall clock under load (target < 5 min met); PC53 1878 informational (WMA play time 1720, Wave length 158, see above). Log C:/gta-scratch/run_all_tests.22436.
- `wave3_audiohw_oracle_test` (links the changed AEMP3BankLoader.cpp): 19 functions, PC24 0, 62 s. AEWaveDecoder.cpp / the other Loaders TUs link into no other test target. run_all_tests (full) not run by me: origin had not moved since b62ce34e, the orchestrator's gate covers it.
- Game check (v4 rule 3): Release build of the final tree + `soak.sh build/StandaloneRelease 3` with NOTSA_STANDALONE_NOVSYNC=1: RESULT OK, errors 0, 59 screenshots, 24.9 fps, peak committed 548 MB (log C:/gta-scratch/audload/soak.log).
- Regenerated .notes/ORACLE_COVERAGE.md (4786 covered).

Start 15:43, end 16:36 (2026-10-23), 53 min.
