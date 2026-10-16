# Portability goal (user, 2026-10-10)
"Цель — исходники, которые можно собрать подо что угодно." The Win32/x86 exe is NOT the end state.

Decisions (orchestrator):
- Win32 x86 /arch:IA32 build stays as the FIDELITY REFERENCE: exe-oracle tests need the original exe in-process (32-bit x86 only).
- Portable targets (first: macOS arm64 native, then Linux x64, Win x64) are verified against the reference by
  deterministic record/replay: same input script + seed -> per-frame state hash (player pos, peds/vehicles, RNG, timer).
- Work streams, in dependency order:
  D  detach the data image (no gta_sa_compact.exe at build or run time)  -> .notes/DETACH_DATA_PLAN.md
  A  no fixed addresses: StaticRef/plugin calls/hook tables compiled out in the portable build
  P  64-bit cleanliness: pointer-size struct layouts, file formats read as raw structs (RW streams, col, IPL/IDE bin,
     saves, script vars holding pointers), VALIDATE_SIZE split into "file layout" vs "in-memory"
  F  float without x87: per-op float rounding (PC24 ≈ IEEE single), software x87 transcendentals (fsin/fcos/fpatan/
     fyl2x…) bit-exact, ftol2 semantics, ExeRecip constants already portable
  W  platform layer: Win32 API, threads, file paths (case-insensitive), registry, DirectSound/EAX audio -> portable
     backend (OpenAL/SDL), DirectInput -> SDL, Bink/videos
  R  render: librw GL3 backend + GL versions of the game's custom D3D9 pipelines (skin vs, building, reflections)
  C  compilers: clang/gcc (thiscall/naked/inline asm/SEH/__declspec, MSVC-isms)
