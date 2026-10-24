# MAC-A1b — PortableMac preset follow-up: brew llvm toolchain, winshim, RW backend defines, config load

Host: Apple Silicon Mac. Rules `.notes/agent_rules/v4.md` (Windows-only parts do not apply). Repo
/Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main` (remote `fork`). Main checkout. Agent MAC-P1 works in parallel
with MANY uncommitted edits across source/game_sa (static_assert -> VALIDATE_* codemod) and source/Base.h — never touch/commit those.
`.notes/` is gitignored (`git add -f`). Wall-clock cap 90 min.

## Input
Read `.notes/reports/MAC-A1.md` and `.notes/reports/MAC-C6.md` (both short). Findings to act on:
1. MAC-C6: brew llvm 23.1.2 (/opt/homebrew/opt/llvm) libc++ has `views::enumerate` / `ranges::shift_right`; Apple clang 21 does not.
   brew clang needs `-isysroot $(xcrun --show-sdk-path)`. `source/portable/winshim` must be on the include path for the ~10 files that
   `#include <Windows.h>` directly (portable only).
2. MAC-A1: portable compile_commands define both `RW_NULL` and `RW_D3D9`, and `USE_D3D9` is hard-coded. The portable build uses librw
   (vendor/librw, branch notsa) with its **GL3** backend (`RW_GL3`; librw needs GLFW or SDL — the plan is SDL3, stream W-1). In this slice
   only make the defines coherent: portable = librw + RW_GL3, no D3D9 defines; D3D9-only game code then shows up honestly in the census
   as stream R. Do not implement the GL3 platform glue.
3. MAC-A1: `LoadConfigurations()` lived in dllmain.cpp and is not called in the portable build — call it from the portable entry path
   (where the standalone WinMain/entry calls it today; find it) under NOTSA_PORTABLE.
4. MAC-C6: `utility.hpp` `try_ston<float>` should use `notsa::compat::from_chars` (MSVC keeps std:: — prove identical).

## Task
- Switch PortableMac to brew llvm (+ sysroot via CMAKE_OSX_SYSROOT or toolchain file), add winshim include dir under GTASA_PORTABLE,
  fix 2–4. Keep the Apple-clang route documented as a fallback only if it costs nothing.
- Reconfigure, then run the FULL portable census (`clang_census.py run --mode native --compdb build/PortableMac/compile_commands.json`,
  low priority, background, timeout) and write the per-directory + per-stream table to `.notes/reports/MAC-A1b-census.md`. Note: MAC-P1's
  uncommitted edits will be in the tree — say so; that's fine.
- Try a real `ninja -k 0` build of the PortableMac target only if the census leaves < 50 failing TUs (it won't yet — then skip).
- No-op proof: StandaloneRelease compile commands + link line identical (A1's method), and wine MSVC A/B for utility.hpp users (3 TUs)
  and any other shared header touched.

## Zones
Yours: CMakeLists.txt, source/CMakeLists.txt, CMakePresets.json, cmake/**, tools/portable/**, conan/profiles/macos-arm64.txt,
tools/standalone/clang_census*.py, source/extensions/utility.hpp, the portable entry file for item 3, RW backend define plumbing (CMake
and fakerw config headers only, no game .cpp). NOT yours: everything MAC-P1 has uncommitted (check `git status` first), Base.h.
Commit only your files (`git commit -- <files>`, trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`), push after each.
Never push to reverse/interior_c, never force. ≤ 1 own sub-agent.

## Report
`.notes/reports/MAC-A1b.md`; return ≤ 15 lines: commits, census totals by stream before/after, no-op result, open items.
