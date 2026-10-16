# WINPREP - Windows PC move preparation (2026-10-16)

Commits (pushed to `fork reverse/interior_c`, tip b23df719): bb29e20b scripts, ee3e4a48 presets + conan profile, b23df719 docs + PORT_BRIEF (force-added).

| Deliverable | File | State |
|---|---|---|
| Setup doc (prereqs with pinned MSVC 14.51.36231 / SDK 10.0.26100.0, clone, exe + data placement, conan, 4 presets, native run, env vars, debugger, first-session checklist) | docs/WINDOWS_SETUP.md | written, never executed on Windows |
| Host detection | tools/standalone/hostenv.sh (HOST, hp/wp path helpers, per-host defaults, PY) | new |
| soak.sh | Windows branch: no Wine/prefix/WINEDEBUG, hard links + junctions for the data dir, kill by pid + PowerShell path filter, BMP->PNG via System.Drawing | Mac branch unchanged |
| run_all_tests.sh | Windows branch: direct exe, taskkill, `cl` check, no Wine flags/GTASA_ML, `NO_EDIT_AND_CONTINUE=ON` | Mac branch unchanged |
| census.py | Windows default data dir, `which bash`, junction-safe cleanup | Mac unchanged |
| build_locked.sh | mutex + timeout (+ taskpolicy/perl alarm on Mac, `timeout` + cl kill on Windows) | new, Mac-tested |
| gtash.cmd | vswhere + `vcvarsall x86 -vcvars_ver=14.51` + Git Bash | new, untested (CRLF via .gitattributes) |
| Presets | `StandaloneRW` added; `GTASA_NO_EDIT_AND_CONTINUE=ON` in StandaloneRelease; conan/profiles/windows-msvc-ninja.txt | `cmake --list-presets` OK |
| Abs. Mac paths | only clang_census.py had one -> MSVC_WINE_ROOT; scripts use `$TOOLS_DIR` (~/tools) | done |
| PORT_BRIEF "Host: Windows" | .notes/PORT_BRIEF.md (was untracked; force-added) | done |

## Verification
* `bash -n` on soak.sh, run_all_tests.sh, build_locked.sh: OK; py_compile census.py/clang_census.py: OK.
* Mac `run_all_tests.sh build/StandaloneRW '^rw_engine_test$'` (SKIP_BUILD, WINE_BIN set): 94 checks, ok.
* Mac `build_locked.sh` (own SCRATCH): lock taken/released, perl-alarm timeout rc 142 path OK.
* Windows branches exercised on the Mac with stubs (fake uname=MINGW64, cygpath, cmd, powershell, taskkill, fake exe): soak.sh -> `RESULT: OK`, report built; run_all_tests.sh -> table row ok. Real Windows behaviour (mklink, PowerShell kill/convert, timeout) is untested.
* **Mac soak smoke did NOT reach the route**: `soak.sh build/StandaloneRelease 3 /tmp/d3w` (private prefix, fresh Documents) dies right after `GcurSelSS=0` with `Failed: RwEngineSetVideoMode(-1)` (RESULT: FAIL, no markers). The UNMODIFIED HEAD soak.sh (copied aside, own prefix) fails identically, also with a copy of the user's gta_sa.set (mode 1) in the prefix Documents, so it is not caused by this change (exe under test is the Oct 11 build; ~/Documents/GTA San Andreas User Files now holds only gta_sa.set.agents-bak-20261011). WinPs.cpp:569-584 path: vmDisplay==0/list[vmDisplay]==null and IsVMNotSelected false -> GcurSelVM stays -1. Someone with a known-good prefix should re-check (the Oct 10-11 FPS/SHADOW runs used patched settings files). Also note ~50% wined3d start flake (up to 5 retries seen with two soaks in parallel).

## Findings worth knowing
* Windows needs `GTASA_NO_EDIT_AND_CONTINUE=ON` (the Wine conan profile injected it; plain windows-msvc.txt does not) else /ZI + /O2 and no linker map.
* `.notes/` is untracked; only force-added files reach the PC. Not in git: decomp/asm, UNHOOKED*, DETACH_GLOBALS.tsv (census.py needs it). PORT_BRIEF.md is now tracked (git add -f).
* On Windows the game uses the user's real Documents (no prefix): gta_sa.set missing/invalid reproduces the -1 video mode failure above.
* Open Windows risks listed in the doc: hard-link/junction data dir, PowerShell kill filter, gtash.cmd vswhere detection, conan install with Ninja generator for msvc 195/x86 (CI uses the default generator), DirectSound mute only validated under Wine.
