# MAC-HL1 - GL loader guard + portable-windows.yml CI (2026-10-24)

## 1. GL loader guard
| item | state |
|---|---|
| librw (fork littlemyx/librw notsa, dbb5c84, pushed) | `missingGLEntryPoint()` in src/gl/gl3device.cpp: after gladLoad*Loader succeeds, 18 core entry points (+8 for GL>=3.0: VAO, FBO, UBO bind) must be non-null, else RWERROR "GL entry point missing: <name>" and startSDL2/SDL3/GLFW clean up and return 0. Whole file is `#ifdef RW_OPENGL` (x86 StandaloneRelease = D3D9, empty TU there) |
| game (source/portable/render/engine_gl3.cpp, 67fc5a9c) | `RwEngineStart` now fails (RsRwInitialize false -> normal exit path) with `FATAL: OpenGL 3.3 core is not available: <reason>` in standalone.log + SDL message box (skipped when env NOTSA_NO_MESSAGEBOX is set, for headless runs). Reasons: no glcontext, gl3Caps.glversion<33 / ES, null GL info entry point (named), compat/<3.3 profile. `NotsaRwGetGlInfo` no longer calls through null pointers |
| submodule bump | 95a41ffa (separate commit) |
| PortableMac | incremental + earlier full rebuild (1107 steps): rc=0 |
| gl_smoke | tools/portable/run_gl_smoke.sh compile+link OK (build/gl_smoke/gl_smoke); NOT run (display idle, per instructions) |
| PortableWin64 (wine) | ninja relink rc=0, 0 errors |
| wine run, 8 s | exe now exits at RwEngineStart with `FATAL: OpenGL 3.3 core is not available: The driver only provided an OpenGL 2.1 context.` instead of the null call crash. Root cause of the old crash: wine's macOS GL gives only a 2.1 context, librw fell back to profile 2.1, where glBindFramebuffer (3.0) is null. Exit code of the process is 0 (WinMain error path, not touched: MSVC x86 file) |
| Win32 x86 StandaloneRelease | unchanged: touched = portable-only engine_gl3.cpp (not in the x86 build) + librw gl3device.cpp (RW_OPENGL only; x86 compile command has -DRW_D3D9, no RW_OPENGL) + new CI/tool files. No MSVC x86 TU touched, so no w0a/cl A/B needed |

## 2. .github/workflows/portable-windows.yml (3 files: workflow, tools/portable/win64/conanprofile-native.txt, conan_install_win64.sh)
* windows-latest, triggers: push to portable/main + workflow_dispatch only; concurrency group; permissions contents: read; timeout 330 min.
* librw via tools/portable/ci_fetch_librw.sh with secrets LIBRW_DEPLOY_KEY / LIBRW_TOKEN (same step as Linux).
* ilammy/msvc-dev-cmd@v1 arch x64 vsversion 2022; `rm /usr/bin/link.exe` (git-bash link shadows MSVC); pip conan+cmake+ninja; conan profile detect; conan cache ~/.conan2/p.
* conan: `CONAN_EXTRA_ARGS="-s compiler.version=194" conan_install_win64.sh Release`; native profile = windows-x64-msvc.txt + `tools.cmake.cmaketoolchain:generator=Ninja` + /WX- /wd4005 (like the wine one). No new CMake preset needed: the wine-only parts are all in the conan profile, PortableWin64 preset is used as is (wine route untouched; the script's PROFILE=wine default on Mac/Linux unchanged).
* build `cmake --build build/PortableWin64 -- -k 0 | tee build.log`; sanity step = ls + dumpbin headers/dependents -> sanity.log (binary not uploaded).
* artifacts: explicit list conan.log configure.log build.log sanity.log (no globs, no build/ paths, no exe/pdb/map).
* actionlint: clean (the SC2086 info on `exit $rc` fixed; portable-linux.yml has the same info, not touched).
* Not run (no triggers, nothing pushed to Actions by me except the normal git push). Unverifiable here and the likely first-run breakages: MSVC 19.44 (VS 2022) vs the 19.51 used so far (C++23 front-end differences), conan msvc 194 on the runner, git-bash path conversion in conan_install_win64.sh (mktemp under /tmp), cmake 4.2 requirement (pip cmake must be >= 4.2), ctest not run (no portable tests on Windows).
