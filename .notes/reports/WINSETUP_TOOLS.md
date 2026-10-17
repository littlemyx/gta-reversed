# WINSETUP_TOOLS - Windows host toolchain (verified by command, 2026-10-16, agent WIN-TEST)

Host: Windows 11 Home 10.0.26200, AMD Ryzen 7 7700X (16 threads), 31 GB RAM, C: 931 GB (230 GB free), Git Bash. GPUs: NVIDIA GeForce RTX 4080 (drv 32.0.16.1714), AMD Radeon(TM) Graphics iGPU (32.0.11024.2), Meta Virtual Monitor.
All commands below were run in a Git Bash after `source tools/standalone/windows/msvcenv.sh` (the agents are not started from gtash.cmd).

| Tool | Required (docs/WINDOWS_SETUP.md section 0/1) | Installed / verified | Command |
|---|---|---|---|
| Python | 3.12+ x64, with unicorn + capstone (+ conan) | 3.12.10, 64-bit (`sys.maxsize > 2**32`), `%LOCALAPPDATA%\Programs\Python\Python312\python.exe` (winget Python.Python.3.12); unicorn 2.1.4, capstone 5.0.9 (pip; `capstone.__version__` reports the core 5.0.7), conan 2.33.0, pip 25.0.1 | `python --version`, `python -c "import unicorn, capstone"`, `python -m pip list` |
| CMake | >= 4.2 | 4.4.4 (winget Kitware.CMake, `...\WinGet\Packages\Kitware.CMake_...\cmake-4.4.4-windows-x86_64\bin`) | `cmake --version` |
| Ninja | any recent | 1.13.2 (winget Ninja-build.Ninja) | `ninja --version` |
| conan | 2.x | 2.33.0 (pip, `Python312\Scripts\conan`), same as the Mac; profile `conan/profiles/windows-msvc-ninja.txt` = x86, msvc 195, static runtime, Ninja | `conan --version` |
| Visual Studio | VS 2026 / v145 Build Tools | Visual Studio Build Tools 2026, installationVersion 18.11.12306.390, path `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools` | `vswhere -all -products '*'` |
| MSVC toolset | **14.51.36231** | only toolset dir installed: `VC\Tools\MSVC\14.51.36231`; `$VCToolsVersion` = 14.51.36231 | `echo $VCToolsVersion` |
| cl | (banner = servicing level of VS 18.11) | `Microsoft (R) C/C++ Optimizing Compiler Version 19.51.36260 for x86`; `cl.exe` (Hostx64\x86) FileVersion 19.51.36260.0; the build uses Hostx86\x86\cl.exe (seen in the process list) | `cl 2>&1 | head -1` |
| Windows SDK | **10.0.26100.0** | `$WindowsSDKVersion` = 10.0.26100.0; `Windows Kits\10\Include` contains only 10.0.26100.0 | `echo $WindowsSDKVersion; ls "/c/Program Files (x86)/Windows Kits/10/Include"` |
| Git | for Windows, `core.autocrlf=false`, `core.longpaths=true` | git 2.54.0.windows.1; **global** `core.autocrlf=false`, `core.longpaths=true` (`~/.gitconfig`); the system config (`C:/Program Files/Git/etc/gitconfig`) still has `autocrlf=true` but is overridden by the global value | `git config --show-origin --get-all core.autocrlf` |
| Submodule | vendor/librw 6f80bf3 | `6f80bf3d11a759ff4be8525622eda071c66923b0 vendor/librw (heads/notsa)` | `git submodule status` |
| Oracle exe | `gta_sa_compact.exe`, sha256 72ae59e4...cecc9b | match (repo root, untracked) | `sha256sum gta_sa_compact.exe` |

Extra tools on PATH (not needed by the port): ffmpeg 9.0.1 (winget Gyan.FFmpeg), jq, Node, Docker, WSL.

## Contract check (cl banner)
`cl` prints 19.51.36260 while the toolset folder is 14.51.36231. WINDOWS_SETUP.md section 4/10 documents exactly this pair (VS 18.11 Build Tools: banner 19.51.36260 inside the 14.51.36231 folder); the toolset folder is the contract, not the banner. No other MSVC toolset is installed, so `-vcvars_ver=14.51` in msvcenv.sh cannot select a wrong one.

## Things that had to be fixed / pitfalls found on this host
| Problem | Cause | Fix / rule |
|---|---|---|
| Shells not started from gtash.cmd had no `cl` | agents inherit the plain PATH | `tools/standalone/windows/msvcenv.sh` (cc24627c): imports the vcvarsall x86 environment, adds python/cmake/ninja/conan to PATH; `source` it in every Bash call that builds or runs tests |
| Configure/build stopped at C4018 (`WinPs.cpp:473`) and C4715 (`WinMain.cpp:240`) | native cl reports warnings the Wine flow's `/WX-` hid | configure with `--compile-no-warning-as-error` (caec4700) |
| `cl.exe` 0xC0000142 (STATUS_DLL_INIT_FAILED) at >8 parallel jobs | desktop-heap / spawn limit on this host | build with `-j8` at most (session 2) |
| **Desktop froze while `run_all_tests.sh` built the test exes** | the script ran `ninja` without `-j` = 18 jobs (16 threads + 2), on top of another agent's build | coordinator rule: never more than `-j4` on this host; `run_all_tests.sh` now passes `-j${GTA_JOBS:-4}` and starts ninja at BelowNormal priority via `cmd /c start /low /b /wait` (commit 9ee7bc21) |
| `python3` on PATH is the Microsoft Store stub (`...\WindowsApps\python3.exe`) | App-execution alias | `hostenv.sh` picks the first of python3/python that really runs; a bare Bash without `msvcenv.sh` may even resolve `python` to a stub (a heredoc into it silently did nothing) - always `source msvcenv.sh` first |
| `audio_fade_oracle_test` reported "crash/no result (exit 0)" | `run_all_tests.sh` had no parser for its summary line (test added after the Mac's 41-test table) | parser added (e042b797); see WIN-TEST.md |
| Hard-coded `/c/gta` game dir missing | game data only in the Steam folder | scripts fall back to the Steam folder (hostenv.sh `DEF_GAME_SRC`); `GAME_DIR` must be set for the asset extraction in `run_all_tests.sh` (`GAME_DIR="/c/Program Files (x86)/Steam/steamapps/common/Grand Theft Auto San Andreas"`) |
| Documents folder `C:\Users\littl\Documents\GTA San Andreas User Files` | does not exist yet (nothing to back up); the game creates it on the first run, later runs read the same `gta_sa.set` | none |
