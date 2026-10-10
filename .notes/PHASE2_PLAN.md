# Phase 2 plan: standalone executable (decisions, 2026-10-09)

Inputs: `.notes/STANDALONE_INVENTORY.md`, `.notes/LIBRW_NOTES.md`. Target: x86 Windows exe (MSVC via msvc-wine, runs under Wine
on this Mac). Portable backends (SDL3/OpenAL/GL3) come after the first exe boots. No original exe is loaded or injected.

## Decisions
D1. **Data image at the original VA.** The standalone exe is linked with `/BASE:0x10000000 /DYNAMICBASE:NO /FIXED`; at startup it
    `VirtualAlloc`s the original data range (.rdata 0x858000 … end of .bss ≈ 0xC9xxxx; ~4.6 MB incl. BSS) at the ORIGINAL address and
    copies in `original_data.bin` (.rdata + .data initial bytes, _TEXT_HA/_rwdseg if referenced), BSS zeroed. `StaticRef<T>(addr)`
    then keeps its meaning unchanged (every absolute pointer inside the data stays valid). The bin is generated AT BUILD TIME from the
    user's own exe (`-DGTASA_ORIGINAL_EXE=<path>`), never committed.
D2. **Hook table = relocation table.** Every dword in the data image that points into the original .text (vtables in .rdata,
    callback tables, function-pointer globals) is fixed up at startup using the same `RH_Scoped*Install(func, 0xADDR)` registrations:
    in standalone mode the macros do not patch code; they fill `std::unordered_map<uint32 exeAddr, void* ourFunc>` (VMT installs give
    slot→method). A startup pass scans the data image for dwords in [0x401000,0x858000) and replaces known ones; unknown ones get a
    **trap stub** (logs the exe address and the caller, then aborts) so that every leftover is found at runtime, not silently.
    `plugin::Call*<addr>` and `((T(*)(..))0xADDR)` casts become the same trap in standalone mode.
D3. **CRT**: our CRT everywhere (malloc/free/new/delete/rand/srand/ftol); `CMemoryMgr` redirects dropped; single `rand()` state.
    The exe's static constructors (`_initterm` table) are audited with Ghidra and replicated where they initialise BSS globals (phase-3
    note already lists 0xC1BFF0).
D4. **RenderWare → librw** via a fakerw-style C shim (re3 approach, MIT): keep the RW C API used by the game (232 functions, 1669
    sites + 864 wrappers), back it with librw's D3D9 backend; RW struct layouts the game reads directly are reconciled one by one;
    the four custom pipelines (building DN, car env-map, specular, env-atm) are rewritten as librw ObjPipelines with HLSL.
D5. **Platform**: first exe keeps D3D9, DirectInput, DirectSound, DirectShow (real system DLLs under Wine), using the existing
    `NOTSA_WinMain` port (0x748710) as the entry. SDL3/OpenAL/GL3 later.
D6. **Leftover game logic** (~100 functions still executed from the exe via function-pointer casts: Vehicle 20, Fx 16, Quaternion 11,
    Heli 5, Physical 3, MissionCleanup 3, …; plus the ~25 live game-ish `plugin::Call`) is reversed first (phase 1d), same port+review flow.

## Streams (parallel where independent)
| stream | content | batches | depends on |
|---|---|---|---|
| S1 (1d) | reverse the ~100 leftover functions (fn-pointer casts), inventory 3 → batches D01… | ~20 + review | — |
| S2 (P2a) | exe data extractor (python, build-time), `original_data.bin` loader at fixed VA, standalone `StaticRef`, hook-table-as-fixups, trap stubs, `/BASE` link flags, `_initterm` audit | ~14 | — |
| S3 (P2b) | librw build (msvc-wine, D3D9), DFF/TXD load test on SA assets, fakerw shim module by module (rwplcore, rwcore, rpworld, rpskin, rphanim, rpmatfx, rpuvanim, rtanim, rtdict, texdict), struct-layout reconciliation, custom pipelines | 100–120 | S2 for linking only |
| S4 (P2c/d) | CRT replacement, entry/main loop wiring, DInput/DSound/DShow init paths without exe, `RsGlobal`/`psGlobal` | ~40 | S2 |
| S5 (P2e) | link the exe, boot under Wine to the main menu, then into the game; fix traps as they fire | 25–40 | S1–S4 |

Status tracking: `.notes/PHASE2_STATUS.md` (per-stream table), briefs in `.notes/PORT_BRIEF.md` (ports) and new `.notes/P2_BRIEF.md`.

## Additions after the `_initterm` audit (`.notes/INITTERM_AUDIT.md`)
D1'. The data image is the POST-`_initterm` snapshot: the extractor replays the exe's 1667 static initialisers with Unicorn
    (`initterm_delta.py`, deterministic, 128 KB of .data/.bss changes, 19 `rand()` calls with seed 1) and bakes the delta in
    before the D2 fixup pass. No constructors are replayed at runtime; atexit dtors are ignored.
D7. **FPU precision.** The exe's CRT sets PC=53 but D3D9 `CreateDevice` (no `D3DCREATE_FPU_PRESERVE`) drops the main thread to
    24-bit mantissa for the whole game. Hence the original's x87 "extended" intermediates were effectively float-rounded at every
    op. Our ports used `double` intermediates (53-bit under SSE2) — to match the real runtime the standalone build compiles the game
    code with `/arch:IA32` (x87) and lets D3D9 set PC=24 (same as the original), or sets `_controlfp(_PC_24, _MCW_PC)` explicitly
    for non-D3D paths. To be verified with a differential test against the exe (phase 2e). The DLL build keeps its current flags.
D8. Fidelity fixes found by the audit (small tasks): `AEVehicleAudioEntity.h` VolumeBase sign (+4.5 at 0xB6BA2C), FrqEngineInAirFactor
    bits 0x3F333332 (0xB6BA74); the 5 BSS floats (0xC1BFF0, 0xC1C81C, 0xC1C818, 0xC279CC, 0xC18D48) come from the delta.
D7 corollary (2026-10-09): under PC=24 every x87 result is already rounded to a 24-bit mantissa, so "unrounded ST0 vs float
store" distinctions vanish at runtime (only exponent-range/denormal edge cases differ). Therefore: no further `*Ext` sweeps;
what matters is compiling the game code so that it also runs with 24-bit-rounded intermediates (/arch:IA32 + PC=24) and
verifying that with a differential test. The `double` intermediates written in phases 1–1d are harmless under that regime.
