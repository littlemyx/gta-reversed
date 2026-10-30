# ASM-S117 (phase 3, tier C): 2dEffect / platform / Securom / LoadMonitor / MemoryMgr / common

Scope: `.notes/asm_sweep/S117.tsv`, 41 rows. Result table: `.notes/asm_sweep/S117.result.tsv`. Agent rules v6.

Verdicts: 35 MATCH, 6 FIXED (5 commits), 0 UNSURE.

## Fixes

| Function | Exe | Deviation | Commit |
|---|---|---|---|
| Rwt2dEffectPluginDataChunkReadCallBack | 0x6F9FD0-0x6FA612 | ped attractor (type 3): the exe also stores the low byte of `PedExistingProbability` (+0x35), `unk1` (+0x36), `unk2` (+0x37) and copies the 8-byte script name (+0x38); the port never wrote them (malloc garbage; the fields are read by `CEventAttractor`, `CPopulation`, `CEntity::CreateEffects`, script commands 22a/23). ENEX (type 6): `m_nTimeOff` was taken from the stream's `TimeOn` instead of `TimeOff` (stream byte +0x29 -> +0x39). SUN_GLARE (type 4): the exe silently moves on (reads/skips nothing), the port hit `NOTSA_UNREACHABLE`. | 1c7bb3c27 |
| CLoadMonitor::EndFrame | 0x53D0B0-0x53D47D | proc-level thresholds are the float constants 0x863AD8 = 0.66666f and 0x863AD4 = 0.33333f, the port used 2/3 and 1/3. Move speed: components * 50 first, then `sqrt((z^2+y^2)+x^2)` (port: `Magnitude() * 50`). Per-second sums and the 8-entry history sum: fild + fadd with one rounding (port rounded the integer to float first); smoothed = `sum*0.125 <= (float)max ? sum*0.125 : (float)max`. | 9076537c1 |
| RsInitialize | 0x619600-0x61966B | `maximumHeight` is the literal 480 (0x1E0); port used `DEFAULT_SCREEN_HEIGHT` = 448. | 47134be49 |
| RsErrorMessage / RsWarningMessage | 0x619B00 / 0x619B30 | the key-up event is sent with `keyScanCode = 0x420` (the exe stores it before the `keyboard.used` test); the port passed an uninitialised `RsKeyStatus` (could release a random key slot < 255). | 6c88b1918 |
| SecuromStateDisplay | 0x744AE0-0x744D4F | labels are `"Trigger: %d"` / `"Trap: %d"` (0x872ECC / 0x872EC0); the port printed them without the colon. No DRM check is involved or added. | df052d55f |

Each deviation was checked twice against the disassembly (stack offsets re-derived after the un-popped call arguments for the 2dfx stream buffer; field offsets cross-checked with the struct layouts).

## Notes (no change)

- `RsRwInitialize`: the exe passes 0x400000 as the resource-arena size, the port's macro depends on the driver model; `RwEngineInit` of the standalone engine ignores it.
- `RsCameraBeginUpdate`: the port passes `nullptr` to `psCameraBeginUpdate`; the exe function ignores its argument (uses the Scene camera).
- `Rwt2dEffectPluginDataChunkReadCallBack`, ENEX with a 40-byte section: the exe leaves `m_nFlags2` as an uninitialised stack byte, the port stores 0 (deterministic, harmless). Particle names: the exe copies up to the NUL, the port copies all 24 bytes.
- `CMemoryMgr::Malloc/Free/Realloc/Calloc`: plain CRT in the exe and in the standalone build (the RR-mode zeroing in `Malloc` is a documented port branch; allocator backend untouched).
- Exe FPU is PC=24, so the extended-precision remarks of the old comments do not apply; the new `EndFrame` code follows the `x87d` convention of `CVector::Magnitude`.

## Tests

- StandaloneRelease `gta_reversed.exe` builds; StandaloneRW `wave2_oracle_test` rebuilt after the last edit and run (RW_EXE_ORACLE = GTA_EXE = gta_sa_compact.exe): 1142 functions, 0 mismatches (PC24 and PC53).
- PortableWin64 compile (x64 subshell) of Securom.cpp, LoadMonitor.cpp, platform.cpp, 2dEffect.cpp: OK. v6 pre-commit grep on the diff: clean.
- Combat regress: queued behind the orchestrator gate's gpu.prio reservation for more than 30 minutes (from 16:44), my queued process was killed by PID: regress left to the gate. Note for the gate: `RsInitialize` now starts with maximumHeight 480 (initial window size before the video mode is selected) and the 2dfx ped attractors now carry their script name / probability.
