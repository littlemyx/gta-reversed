# Porting brief: phase 1b (functions still called via plugin:: stubs, no hook)

Repo /Users/Andrei.Mukhin/github/recomp/gta-reversed, branch reverse/interior_c. Goal: behavioural
reimplementation (not byte-match) of GTA SA 1.0 US compact. You port the functions of your batch so the
`plugin::Call*<0x...>` stub is replaced by real code and the function gets a reversible hook.

## Inputs
- Inventory: `.notes/UNHOOKED.md` (batches, per-file tables), `.notes/UNHOOKED_SIGS.md` (call kind,
  template args, enclosing declaration with file:line for every address).
- Ghidra decompilation: `.notes/decomp/<ADDR8>_<name>.c`; asm: `.notes/asm/<ADDR8>_<name>.asm`
  (`::` is `__` in file names). Ghidra asm omits jump-table bodies: for switch bodies and for callee
  bodies disassemble the exe at repo root `gta_sa_compact.exe` with capstone — use the venv interpreter
  `/private/tmp/claude-501/-Users-Andrei-Mukhin-github-recomp-gta-reversed/aa4942c3-1b0c-4e82-a9c0-0c095cb1fe07/scratchpad/capvenv/bin/python -I` (capstone 5.0.7 installed there; system python3 has no pip): ImageBase 0x400000; .text VA 0x401000 raw 0x400; .rdata VA 0x858000
  raw 0x456800; .data VA 0x8A4000 raw 0x4A1C00 (raw size 0x40000). Scripts go in YOUR OWN subdir of
  /private/tmp/claude-501/-Users-Andrei-Mukhin-github-recomp-gta-reversed/aa4942c3-1b0c-4e82-a9c0-0c095cb1fe07/scratchpad/
  run with `python3 -I`; never name a script dis.py.
- Address → name: `.notes/symbols.txt`. Callees that are already reversed exist in source/ (grep the
  address, e.g. `grep -rn 0x6E3B00 source`); use the C++ name, not a plugin call.

## Fidelity rules (what reviewers will check)
- Follow the asm for the ORDER of calls, side effects, and float expression evaluation; the decomp's x87
  expression order is unreliable. Where the asm keeps an intermediate on the x87 stack (extended
  precision) before a single float store, use `double` intermediates; where it spills to a float
  temp, round to float there.
- Float constants exact: read them from .rdata (π = 0x40490FDB → `std::numbers::pi_v<float>`,
  `#include <numbers>`; NOT common.h PI which is 3.14159f). Rand idioms: `CGeneral::GetRandomNumber() &
  0xFFFF` × 2^-15 (0x858B14 = 1/32768); CRT `rand()` × 1/32767 (0x858C7C = RAND_MAX_FLOAT_RECIPROCAL).
- Branch conditions: keep the exact comparison and NaN semantics (FCOMP+JP/JNP patterns).
- `RET N` ⇒ N/4 stack args for stdcall/thiscall. Do not change a signature unless the asm proves it.
- Original globals: use existing declarations (grep the address in source/) or `StaticRef<T>(0xADDR)`;
  never a fresh C++ static for storage the original keeps in .data/.bss (other hooked code reads it).
- Virtual functions: `RH_ScopedVMTInstall`; plain functions `RH_ScopedInstall(Name, 0xADDR)`;
  overloads `RH_ScopedOverloadedInstall(Name, "suffix", 0xADDR, ret(Class::*)(args))`. A plain install
  on a virtual recurses. Register-argument (usercall) functions cannot be hooked: port them anyway and
  leave the install out with a comment `// usercall: EDX = ..., can't hook`.
- Functions declared in the "wrong" class (inline plugin call with a foreign `this`): declare them as a
  proper member of the real class (header), implement there, hook there, and call them by name.
- Style: match the surrounding code (notsa conventions, `// 0xADDR` comments on key call sites, `// BUG:`
  with `notsa::IsFixBugs()` gates — NEVER a silent guard that changes behaviour; `// NOTSA:` for
  non-original code). Dead code with side effects stays. Keep `VALIDATE_SIZE`s honest.
- Do not invent member names; if a struct offset has no member, add it to the class at the right offset
  (check the VALIDATE_SIZE) instead of raw pointer arithmetic.
- No `sed -i` with a suffix (BSD sed). Do not weaken checks to get green.

## Build check (required, per batch)
```
export PATH="/Users/Andrei.Mukhin/tools/Wine Staging.app/Contents/Resources/wine/bin:$HOME/.local/bin:/opt/homebrew/bin:$PATH" WINEPREFIX=~/.wine-msvc WINEDEBUG=-all
cd /Users/Andrei.Mukhin/github/recomp/gta-reversed
ninja -C build/Debug source/CMakeFiles/gta_reversed.dir/game_sa/<Path/File>.cpp.obj 2>&1 | grep -E "error [CL][0-9]+|warning C4|FAILED|ninja:"
```
(object target path = `source/CMakeFiles/gta_reversed.dir/` + path relative to `source/`.) Build every
.cpp you touched AND every .cpp that includes a header you changed in a way that affects it. Other agents
build concurrently. EVERY ninja invocation (even a single object) goes under the build mutex:
`until mkdir "$SCRATCH/build.lock" 2>/dev/null; do sleep 15; done; ninja ...; rmdir "$SCRATCH/build.lock"`
(the orchestrator's periodic full builds hold it for up to ~30 min; just wait). Do NOT delete the PCH (`build/Debug/source/CMakeFiles/gta_reversed.dir/cmake_pch.cxx.*`)
unless your header change is in a PCH-included header and you get bogus "undeclared" errors; in that case
take the mutex first: `until mkdir "$SCRATCH/build.lock" 2>/dev/null; do sleep 20; done`, delete the PCH,
build your objects, then `rmdir "$SCRATCH/build.lock"` (SCRATCH = the scratchpad dir above). The
orchestrator runs the full link afterwards. Ignore D9035/D9036/D9025 and LNK4224 warnings.

## Commit
Only your files: `git add <files> && git commit -- <files>`. Message: `Reverse <Class::Func>[, ...]` (one
commit per batch is fine), body: deviations/bugs found in the original (`// BUG:` items), anything not
hookable and why. Trailer: `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`. Do NOT push, do NOT
touch files outside your zone, do NOT revert others' uncommitted changes (if `git status` shows foreign
dirty files, leave them alone and commit only yours).

## Report (short; numbers in tables; no code dumps)
Per function: address, name, size, result (ported / ported-not-hooked + reason / blocked + reason),
notable findings (bugs in original, deviations from the decomp, constants read from .rdata). Then:
objects built and error count, commit hash(es), anything you could not verify.

## Lessons from reviews (2026-10-09)
- Shared helpers `CVector::Normalise`, `CrossProduct`, `CMatrix::TransformPoint` are float-based; the original's 0x59C890
  (TransformPoint) and 0x59C910 (Normalise) accumulate in extended precision with a fixed term order. When the asm inlines
  those or keeps the result on the x87 stack, write a local double-precision helper in the same term order.
- Compare direction must preserve NaN behaviour: `JBE`/`JP` after `FCOMP` ⇒ `!(x > c)`, not `x <= c`.
- Constants: distinguish float (0x...4-byte in .rdata, e.g. 0.98f) from double (8-byte, e.g. 0.5); `(double)0.98f` ≠ 0.98.
- Flag bits: resolve by (offset, mask) against the header's bitfield order, not by a name that sounds right
  (e.g. mask 0x20000 at +0x38C was `bHydraulicInst`, not `bLowRider`).
- Ped type numbering: 0 = PED_TYPE_PLAYER1, 1 = PED_TYPE_PLAYER2.
- Unreachable `default:` in a switch over a clamped value: `NOTSA_UNREACHABLE()`, not bit_cast emulation of garbage.
- Overloaded virtuals: MSVC lays out same-name overloads in REVERSE declaration order in the vtable. Check the exe
  vtable slot order before declaring/hooking overloaded virtuals (FxPrim_c::AddParticle case, c3752435).
- Ped type checks `type == 0 || type == 1` ⇒ PED_TYPE_PLAYER1 / PED_TYPE_PLAYER2. PED_TYPE_PLAYER_NETWORK is 2 and
  is NOT what the exe tests (found three times in reviews).
- CVehicle: +0x590 is `m_nVehicleType`, +0x594 is `m_nVehicleSubType` (eVehicleType values); check the offset, not the name.
- Scratch dirs: create a UNIQUE directory for your scripts (e.g. `scratchpad/<task-name>-<random>`), never reuse an existing
  one and never `rm -rf` a directory you did not create in this run (another agent's files were deleted once).
- Hook kind must match the exe's calling convention, not the C++ declaration: a cdecl free function hooked as a
  non-static member (thiscall, callee pops) corrupts the caller's stack → "weird crashes" (TryToPassObject case).

## Phase 1d (inventory 3: `.notes/UNHOOKED3.md` / `UNHOOKED3_SIGS.md`)
These stubs are function-pointer casts `((R(__thiscall*)(..))0xADDR)(..)` (or inline-asm usercall stubs) with the `RH_` install
line commented out or missing. Port the body in place, delete the cast, enable/add the install line (VMT for virtuals), keep the
`// 0xADDR` comment. Overloaded names in SIGS (e.g. `CQuaternion::Set_Euler`) are disambiguation hints only — keep the real C++
overloads and use `RH_ScopedOverloadedInstall`.

### FPU precision note (settled 2026-10-09, do not re-raise in reports)
The exe's CRT sets x87 PC=53, and D3D9 `CreateDevice` (no FPU_PRESERVE) drops the MAIN thread to PC=24 for the rest of the game;
other threads (audio streaming) stay at PC=53. The "double for x87-stack values, float at spill points" rule stays: the standalone
build compiles game code with `/arch:IA32` (x87) and lets D3D9 set PC=24 exactly like the original (plan D7), so our `double`
intermediates are rounded per op the same way the exe's were. Reviewers: do not list "x87 precision unknown" as unverifiable.

### Lesson 2026-10-10: reciprocal constants and lerp forms (oracle-proven)
* Never write `x * (1.0f / N)` or `x / N.f` when the exe does `fmul [rdata float]`: MSVC x87 folds the inline reciprocal as an
  EXTENDED-precision constant and the result differs in the last bit (161 SCREEN_SCALE sites were off this way). Use the exact float
  constant the exe loads (`std::bit_cast<float>(0x…)` / a named constexpr with the hex value), and `fdiv` only where the exe divides.
* `lerp` has two forms in the exe: `(to-from)*t+from` (common.h `lerp`) and `from*(1-t)+to*t` (`lerpBlend`) — pick per site from the asm.
* `RadiansToDegrees` is `x * 57.29578f` (0x42652EE0); the exe never divides by π. Exact π family constants only.
* `(int)float` under MSVC /arch:IA32 = `__ftol2_sse` (saturating) ≠ the exe's `_ftol2` (low dword of `fistp qword`: NaN → 0, wrap).
  The standalone build overrides `_ftol2*` in `source/standalone/FtolExe.cpp`; in reviews, treat plain casts as exe-equivalent there,
  but DLL-build-only code paths still differ. Use `ExeRecip(N)` (`ExeRecip.h`) for every `x / N` / `x * (1/N)` the exe does as fmul.

### Lesson 2026-10-10: freed memory must stay intact in the run build
The port's `CPool::Delete` (and debug allocators) fill freed objects with 0xDD; the exe leaves memory intact, and the original game
RELIES on benign stale reads (idle-cam events, group intelligence double delete, …). In `NOTSA_STANDALONE_RUN` the pools must
behave exactly like the exe's `CPool` (flag byte + ref byte only, no fill, no "allocation failed" abort). A stale pointer is a port
bug only when the exe's own bookkeeping differs (e.g. 0x602A28 nulls `event.entity` after `CleanUpOldReference`).
* Transcendentals: use `x87::sin/cos/tan/atan2/asin/acos/sqrt/log10` (`Core/X87Intrinsics.h`) — never `std::` — where the exe
  inlines the instruction (327 functions, table `.notes/X87_INLINE_TABLE.md`) or calls `_CIasin/_CIacos` (UCRT differs in 73% of
  cases). pow/floor/ceil/modf/exp may stay `std::` (bit-identical to the old CRT).
* Destructors must restore the base vftable like the exe's do (MSVC 7 emitted the store; MSVC 14 /O2 ELIDES it for inline
  `= default`/empty dtors). The original relies on stale objects dispatching to the empty BASE virtuals. Define every destructor of
  a class with virtual functions OUT OF LINE in the .cpp (never `= default` inline) and verify the vptr store in the Release disasm.
* Build mutex hygiene: wrap every ninja/cmake in `timeout 1800 …`; a wine-msvc `cl.exe` can hang at 0% CPU and hold `build.lock`
  for everyone. If the lock is older than 20 min and no cl.exe/link.exe has CPU, kill the hung build and `rmdir` the lock.

## Build timeout on this Mac (2026-10-10)
There is no `timeout`/`gtimeout`. Wrap builds as `perl -e 'alarm shift; exec @ARGV' 2400 ninja -C build/X <target>`
(SIGALRM kills a hung wine cl.exe chain; then check `ps` for leftover wine-msvc.sh and kill them before releasing build.lock).

## Report format (mandatory)
Write the full report to `.notes/reports/<YOUR-ID>.md`. Your final message to the orchestrator: at most 15 lines —
commits, test/soak result, open items, path of the report file. No tables of evidence in the message.

## Do not suspend the game thread under Wine (2026-10-10)
SuspendThread/GetThreadContext on the main thread (old soak sampler, winedbg bt, stack.sh) corrupts the x87 register stack under
Wine/Rosetta -> random NaN, wanted level 5, "fell through the map", pool exhaustion. The sampler is opt-in now (a43d03b3). Any NaN seen
in a run that used one of these tools is not evidence of a port bug.

## Game runs must be SILENT (user, 2026-10-10)
Never let a test/soak/measurement run of either exe play audio on the host. Our exe: `NOTSA_STANDALONE_MUTE=1` (soak.sh sets it by
default). Do NOT disable the Wine audio driver: the game hangs before the menu without an audio device. For the ORIGINAL exe (no mute
switch) set its in-game SFX/radio volume to 0 in the private prefix's settings before running, or don't run it with audio at all.

## Builds run at background priority (2026-10-11)
The user plays/looks at the game on this Mac while we build; parallel cl.exe saturation causes the hitches they see (FPS report).
Every ninja/cmake build: `taskpolicy -b nice -n 15 perl -e 'alarm shift; exec @ARGV' 2400 ninja -j6 -C <dir> <target>`
(taskpolicy -b = background QoS, efficiency cores on Apple silicon). Game runs and measurements stay at normal priority.

## Private Wine prefixes must NOT share the user's Documents (2026-10-11)
A prefix cloned from ~/.wine-msvc (or a fresh one) has `drive_c/users/<user>/Documents -> /Users/Andrei.Mukhin/Documents`: game runs then
read/WRITE the user's real `GTA San Andreas User Files/gta_sa.set` (and saves). Right after creating a private prefix, replace that
symlink with a real empty directory. Never patch gta_sa.set outside your private prefix.

## Host: Windows (native x86-64 PC with GPU, from 2026-10-16; setup in `docs/WINDOWS_SETUP.md`)
The reference work (Win32 x86 MSVC 14.51.36231 build, exe-oracle tests, game soaks, debugging) runs on the Windows PC, in Git Bash started through
`tools/standalone/windows/gtash.cmd` (MSVC x86 environment). The Mac keeps the portability streams; everything above this section still applies unless it is Wine/macOS-specific:
* **Build mutex**: `C:/gta-scratch/build.lock` (same `mkdir`/`rmdir` lock). Wrap EVERY ninja/cmake/conan in `tools/standalone/build_locked.sh <cmd...>` (lock + `timeout` 2400 s, `TIMEOUT_S` to change; a timeout also kills leftover cl/link).
  The same script works on the Mac (lock in `$TMPDIR/gta-standalone-scratch`, `taskpolicy -b nice -n 15`, perl alarm), so new briefs may use it instead of the hand-written snippets. No `taskpolicy`/background priority on Windows.
* **No Wine rules apply**: no prefixes, no `WINEPREFIX`/`WINEDEBUG`, no wined3d start-up flake, no "do not suspend the game thread" (that was a Wine/Rosetta x87 problem; VS/WinDbg breaks and `NOTSA_STANDALONE_SAMPLER=1` are fine), no `Z:` paths
  (`RW_EXE_ORACLE` is a normal `C:\...` path). A debugger (Visual Studio, WinDbg) is allowed and expected for crash/oracle work: PDB + map are next to the exe (`build/<dir>/bin`).
* **Gameplay regression (before you declare a gameplay-affecting change done)**: run `tools/standalone/gameplay_regress.sh` (~20 min, silent, windowed, GPU lock per run; use `run_in_background`) after `run_all_tests.sh` and the soak - any change to tasks / AI / audio / pools / scripts / input / camera / render path / population. It replays the playtest findings (police engage, weapon drops, hit reaction, audio mixer, cheats vs the original, missions/busted, Return key, mode change, mouse capture, first-start window) and prints PASS/FAIL/KNOWN per check; FAIL = regression (or an open item not yet in `tools/standalone/gameplay_regress/known.tsv`). A new playtest finding gets a scenario (docs/STANDALONE.md "Gameplay regression suite").
* **Still silent**: every test/soak/measurement run sets `NOTSA_STANDALONE_MUTE=1` (soak.sh/census.py do by default; check the `[mute]` lines in `standalone.log`); the original exe has no mute switch - never run it with audio.
* **Scripts** detect Git Bash (`uname -s` MINGW*/MSYS*) via `tools/standalone/hostenv.sh`; paths are `C:/...` (mixed form), defaults: game data `C:/gta`, soak data dir `C:/gta-soak`, outputs `C:/gta-soak-out`, repo `C:/src/gta-reversed`.
  The game still uses the user's real `Documents\GTA San Andreas User Files` (no private prefix): do not edit/patch `gta_sa.set` there without telling the user; a missing/empty file => `Failed: RwEngineSetVideoMode(-1)` at start-up.
* Git: the fork is `origin` on the PC (`git push origin reverse/interior_c`), `fork` on the Mac. Commit trailer and report rules are unchanged. `.notes/` is mostly untracked: only force-added files travel through git.
