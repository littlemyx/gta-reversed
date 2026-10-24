# MAC-A1b report: PortableMac on brew llvm, winshim, GL3 defines, config load

Host Apple Silicon Mac, branch portable/main. Commit **828725d2** (pushed to `fork`): CMakePresets.json, conan/profiles/macos-arm64.txt, source/CMakeLists.txt, source/fakerw/fakerw.h,
source/app/platform/win/WinMain.cpp, source/extensions/utility.hpp. MAC-P1's uncommitted files untouched.

## Changes
1. Preset PortableMac: CMAKE_C/CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang(++), CMAKE_OSX_SYSROOT=macosx (cmake -> `-isysroot .../MacOSX27.0.sdk`, verified in compile_commands),
   exe link `-L/opt/homebrew/opt/llvm/lib/c++ -Wl,-rpath,...` (brew libc++ at run time; untested until the first link). Apple-clang fallback documented in the preset description (free).
   Conan deps still built with apple-clang (profile comment updated). NOTE: after changing the compiler, delete build/PortableMac/CMakeCache.txt + CMakeFiles (a stale cache keeps
   `nlohmann_json_DIR-NOTFOUND`); keep `generators/`.
2. winshim: `target_include_directories(... PRIVATE portable/winshim)` under GTASA_PORTABLE (1109/1109 TUs have it).
3. RW backend: portable = librw `LIBRW_PLATFORM=GL3`, `LIBRW_GL3_GFXLIB=SDL3` (librw configures with conan SDL3 + system OpenGL framework); game target gets `NOTSA_RW_LIBRW` only
   (RW_GL3 + LIBRW_SDL3 come PUBLIC from librw); `RW_D3D9` and `USE_D3D9` only when not portable (the USE_D3D9 line moved into an `if` right after the same call, same order for MSVC);
   fakerw.h: `<d3d9.h>/WITH_D3D/RW_D3D9` under `#ifndef NOTSA_PORTABLE`. compdb: RW_GL3 x1171, RW_D3D9/RW_NULL/USE_D3D9/WITH_D3D x0. GL3 platform glue NOT implemented.
4. LoadConfigurations: called in the run-build WinMain (portable only, on the 3 padding lines after InjectHooksMain, line count unchanged); body (ini + FastLoader + Misc configs)
   defined at the end of WinMain.cpp under NOTSA_PORTABLE (dllmain.cpp is not in the portable target; a new .cpp under source/ would be globbed into the MSVC build, so not used).
5. utility.hpp: `try_ston<float>` uses `notsa::compat::from_chars` (include replaces a blank line).

## No-op proof (sonnet sub-agent, scratch /private/tmp/claude-501/a1b_ab, BEFORE = git archive HEAD, AFTER = + exactly the 5 files; msvc-wine)
* `cmake --preset StandaloneRelease`: 1284 / 1284 compile commands identical; whole build.ninja byte-identical after path normalisation (link statement identical).
* cl.exe under wine, same-path symlink swap, obj_code_sig (code + reloc names), 7/7 identical: WinMain 163018, CutsceneMgr 159474, HookFilter 59826, InjectHooksMain 140245,
  standalone/rw/engine 31586, standalone/rw/raster 28777, TaskComplexArrestPed 37120 bytes code.
* Only difference: `.debug$T` +13.5 KB per TU (CodeView) because utility.hpp now includes portable/stdcompat.h (<stacktrace>, <charconv>) on _WIN32 - no code effect.

## Census: see MAC-A1b-census.md. 1105 TUs, 845 clean (was 404), 3033 unique (was 3643); R 1315, W 915, P 159, F 208, C 15, A 0, unclassified 421. Contains MAC-P1's uncommitted codemod.

## Open items
* R: fakerw.h / standalone/rw/* / Pipelines are D3D9-specific (1315); the GL3 backend glue (W-1/R) is the next step. 
* PCH closure still 83 errors (fakerw.h vs GL3 rw types, ILP32 asserts, intrin headers).
* F: xmmintrin/emmintrin builtins (172): x86 SIMD on arm64 -> sse2neon or scalar.
* brew libc++ link flags untested (no link yet); `ninja -k 0` skipped (260 TUs fail).
