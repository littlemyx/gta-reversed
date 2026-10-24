# MAC-LNX1: first Linux x64 build of the portable game + CI (milestone 5) - 2026-10-24

Commits on fork portable/main: c5e95d6d->250cc5a7 (preset, fixes, tests), f49ff090 (workflows, case shim, helpers). Work was done in a separate git worktree (RACE1's dirty files untouched).

## Result
| item | state |
|---|---|
| Linux x86_64 compile + link of `gta_reversed` (19.8 MB ELF) | DONE locally: ubuntu 24.04 amd64 container (Docker Desktop, Rosetta, 10 cpus), FRESH case-sensitive tree: conan install + configure + `ninja -k 0 -j9` (1118 steps, 0 errors) + link + `ctest -L portable` 17/17. Cold compile+link 344 s. |
| GitHub run (fork) | NOT GREEN, blocked: `vendor/librw` = private repo littlemyx/librw, the job token cannot read it. Run 40082432522 (Portable Linux) fails at the step "librw submodule" with the explanatory error (same for Portable macOS 40082432554). Everything after that step was only tested in the container. |

## BLOCKER (needs the owner, 1 command)
Run once yourself: `tools/portable/ci_librw_access.sh` (ssh-keygen -> read-only deploy key on littlemyx/librw -> Actions secret LIBRW_DEPLOY_KEY on littlemyx/gta-reversed -> local key deleted). Or set a fine-grained PAT (Contents:read on librw) as secret LIBRW_TOKEN, or make librw public. I did not do it: it changes repo security settings / stores a credential. Then `gh workflow run portable-linux.yml -R littlemyx/gta-reversed --ref portable/main`. Note: the fork gta-reversed is PUBLIC (task said private): logs and artifacts of the runs are public.

## Preset / deps route
* `PortableLinux` (CMakePresets.json): Ninja, clang-23 + libc++ (`-stdlib=libc++`, lld), Release -O2, same GTASA_* cache as PortableMac, `CMAKE_POLICY_VERSION_MINIMUM=3.5` (Ubuntu's OpenALConfig.cmake predates CMake 4). Why clang 23 + libc++ and not gcc: the game uses std::views::enumerate / ranges::shift_right (129 + 2 sites); libstdc++ 13 (noble) has neither, gcc 15 is not on 24.04. apt.llvm.org has llvm-toolchain-noble-23 (clang 23.1.5, libc++-23-dev).
* `tools/portable/linux_deps.sh` (apt: clang-23, lld, libc++-23-dev, build-essential, ccache, GL/X11/ALSA/udev dev, libopenal-dev; pip: cmake>=4.2 (noble has 3.28), ninja, conan 2).
* `conan/profiles/linux-x64.txt` + `tools/portable/conan_install_linux.sh` (scratch copy of conanfile.py like the mac script; also copies the generated `source/libs/imgui` into a fresh checkout and writes a stub ConanPresets.json, both gitignored and required by the configure). Deps are built by conan with the SAME clang 23 + libc++ (ABI of spdlog/imgui/tracy). SDL3 from conan (x11 only: wayland/sndio/pulse/vulkan off; conan installs the xorg -dev packages via apt, `tools.system.package_manager:mode=install`). OpenAL: apt libopenal-dev via find_package(OpenAL CONFIG).

## Error classes fixed (the Linux-only list was short: 5 + the case shim)
| class | fix |
|---|---|
| Mach-O API in a shared header (`mach-o/dyld.h`, audio_portable.h MainImageRange) | ELF branch with the linker symbols `__executable_start` / `_end` (`<link.h>` is shadowed by game_sa/Core/link.h on the include path) |
| `funopen` (BSD) in path_resolve.cpp | `fopencookie` twin (64-bit offsets), `#ifdef __APPLE__` keeps the old code |
| glibc `gamma(double)` vs the game global `CGamma gamma` (Gamma.h; MenuManager.cpp, WinMain.cpp use it) | under `#ifdef NOTSA_PORTABLE`: `#include <cmath>` + `#define gamma notsa_gamma` after the libm declaration; w0a_guardlines TEXT OK; Mac syntax check of Gamma/MenuManager OK |
| `getcwd/chdir` redeclared without noexcept (crt.h vs unistd.h) | `#include <unistd.h>` instead |
| Itanium C1 ctors of abstract classes undefined at link (7 symbols, same as the Mac MAC-BUILD1 case) | link_aliases.cpp: ELF x86_64 branch (`jmp C2@PLT`) |
| case-sensitive includes (10: Audio/entities/, Audio/hardware/, Audio/managers/, Common.h, Extensions/ci_string.hpp, Pathfind.h, Platform.h, Timecycle.h, data/SoundAttenuationTable.h, windows.h) | NO source edit: `tools/portable/gen_case_shim.py` at configure time (CMake block in source/CMakeLists.txt, Linux+portable only) symlinks each such include to the real file in `build/PortableLinux/case_shim`, last -I dir. The local container build on the case-INSENSITIVE bind mount hid these; found by the fresh-tree run. |
Not hit: `-mms-bitfields` on x86_64 clang, `operator new` noexcept (not audited: open item), pthread/dl link, <cstdint> transitivity. MSVC-TU edits: only Gamma.h (guard lines, TEXT OK); everything else is portable-only (source/portable/*) or CMake. No wine A/B needed.

## Host tests on Linux (ctest -L portable = 17 tests, 23 s wall -j3)
Fixes in tools/portable: GNU `sed -i.orig` (was BSD `sed -i ''`) in p2h/p2i, p2b reference clang from $CLANG, `CLANG=${CMAKE_CXX_COMPILER}` in the ctest environment, p2j links with lld + `--unresolved-symbols=ignore-all` (no dynamic_lookup on ELF), rr2: no `-weak_framework` off Darwin, input: SDL3 headers/libs from the build's compdb/link line instead of Homebrew, p2g: real main.scm part skipped when `CI` is set and the file is absent. `portable.txd_gl` (display + data, mac-only script) is labelled `display_needed` on non-Apple (a `portable-x` label would match `-L portable`, regex). `-L mutation` not run on Linux.

## Workflows
* `.github/workflows/portable-linux.yml`: push to portable/main + workflow_dispatch only, ubuntu-24.04, concurrency-cancel; steps: checkout (no submodules) -> `ci_fetch_librw.sh` -> linux_deps -> conan cache -> ccache -> conan install -> configure -> `cmake --build -- -k 0` (log + wall time + error count) -> `ctest -L portable` -> ldd -> artifacts `portable-linux-logs` (always) and `gta_reversed-linux-x64`. actionlint clean. Windows workflows untouched (they already trigger on every push, including portable/main).
* `.github/workflows/portable-macos.yml` (macos-15 arm64, brew llvm/openal-soft, PortableMac, same ctest): written, NEVER RUN (same librw blocker). Conan profile pins apple-clang 21: the workflow passes `CONAN_EXTRA_ARGS=-s compiler.version=<runner clang major>` (new env hook in conan_install_mac.sh/linux.sh).
* CI timing: not measured on a runner. Container numbers: apt/pip toolchain 2:19, conan (builds SDL3, ogg, vorbis, jpeg, spdlog, tracy, imgui) ~8-10 min cold, compile+link 344 s with 9 jobs; a 4-core runner should be 15-25 min compile (estimate), ccache/conan caches should cut reruns.

## Open items
1. librw access (above). 2. The ccache + clang PCH combination is untested on the runner (sloppiness set; a miss only costs time). 3. Pool `operator new` noexcept audit + pool-exhaustion test under -O2 (RE3_DIGEST 1.2), ASan leg, GCC leg: not done. 4. The exe was not run on Linux (no data/display): runtime porting (SDL3/GL on X11, statehash/app_platform Linux branches, `ldd`: needs libc++.so.1, libopenal, libsndio) is next. 5. libstdc++ also gets linked (via libopenal). 6. `-w` hides all warnings in the portable build (inherited). 7. Container cleaned up; worktree branch `lnx1` merged (ff) into portable/main.
