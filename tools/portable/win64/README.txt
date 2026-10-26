PortableWin64 (MAC-WIN64): Windows x64 build of the portable game (MSVC cl.exe, static CRT, librw GL3 + SDL3, real Windows SDK headers).

A. Native Windows / windows-latest (x64 Native Tools shell or ilammy/msvc-dev-cmd arch: x64)
   bash tools/portable/win64/conan_install_win64.sh Release       (PROFILE=native; builds SDL3, spdlog, imgui, ... for Windows/x86_64)
   cmake --preset PortableWin64 -DGTASA_PYTHON=python
   cmake --build build/PortableWin64 -- -k 0                      -> build/PortableWin64/bin/gta_reversed.exe

B. Mac / Linux with msvc-wine (this is how MAC-WIN64 was built)
   1. msvc-wine installed in ~/tools/msvc (vsdownload.py --accept-license + install.sh), wine in PATH, WINEPREFIX=~/.wine-msvc, WINEDEBUG=-all.
      bin/x64/ wrappers exist after install.sh; bin/x64/cl already carries the /Zi -> /Z7 filter of contrib/msvc-wine/patch-wrappers.sh.
   2. patch -p0 bin/x64/wine-msvc.sh with wine-msvc-rsp.patch (x64 twin of the x86 patch: expands @response files to z: paths; the link line exceeds Wine's 32 KB
      CreateProcess limit, CMAKE_NINJA_FORCE_RESPONSE_FILE=ON).
   3. tools/portable/win64/conan_install_win64.sh Release     (PROFILE=wine: conanprofile-wine.txt + toolchain-msvc-wine-x64.cmake; ~20-40 min under wine, cached afterwards)
   4. cmake --preset PortableWin64 -DGTASA_PYTHON=<python>; ninja -C build/PortableWin64 -k 0
   Always wrap wine builds in a timeout; a cl.exe at 0% CPU for >15 min is hung (kill it and rmdir the ninja lock).
