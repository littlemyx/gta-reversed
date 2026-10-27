# Import the MSVC x86 build environment into the CURRENT Git Bash (SOURCE this file, do not run it):
#     source tools/standalone/windows/msvcenv.sh
# For agents / shells that were NOT started through tools\standalone\windows\gtash.cmd. It does what gtash.cmd does
#     vcvarsall.bat x86 -vcvars_ver=14.51
# and converts the result (PATH, INCLUDE, LIB, LIBPATH, WindowsSDK*, ...) into exports for this bash. It also puts python / cmake / ninja / conan
# on PATH when winget (or the python.org installer) put them somewhere the inherited PATH does not know yet (the registry User/Machine PATH is merged in,
# and a real python is placed in front of the Microsoft Store "python" alias).
# GTA_MSVC_ARCH=x64 source this file -> x64 environment (cl for x64) for the PortableWin64 build; do not mix x86 and x64 in one shell (the "already loaded" shortcut is keyed per arch).
# Env (same names as gtash.cmd):  GTA_MSVC_VER (default 14.51)   GTA_VSINSTALL (default: newest VS found by vswhere with the VC x86/x64 tools)
#                                 GTA_MSVCENV_SCRATCH (cache dir, default C:/gta-scratch; the vcvars dump is cached per VS path + toolset)
#                                 GTA_MSVCENV_FORCE=1 (ignore the cache / the "already loaded" shortcut)
# Idempotent: sourcing twice is a no-op (a few ms); the first source in a new VS/toolset costs one cmd + vcvarsall run (~2-5 s), later shells reuse the cached dump.
# Result check:  cl 2>&1 | head -1   ->  ...Version 19.51.x for x86 (VCToolsVersion=14.51.36231)     echo $WindowsSDKVersion  ->  10.0.26100.0\

# ---- 1. tools that winget/python.org installed but the inherited PATH may not list yet -------------------------------------------------------
_gta_need=0
for _gta_t in python cmake ninja conan; do command -v "$_gta_t" >/dev/null 2>&1 || _gta_need=1; done
case "$(command -v python 2>/dev/null)" in */WindowsApps/*) _gta_need=1;; esac
if [ "$_gta_need" = 1 ]; then
_gta_regpath="$(powershell.exe -NoProfile -NonInteractive -Command "[Environment]::GetEnvironmentVariable('Path','User') + ';' + [Environment]::GetEnvironmentVariable('Path','Machine')" 2>/dev/null | tr -d '\r')"
_gta_front=""   # registry PATH dirs that hold a real python (not WindowsApps) go first, other missing dirs go to the back
_gta_back=""
_gta_oldifs="$IFS"; IFS=';'
for _gta_d in $_gta_regpath; do
  [ -n "$_gta_d" ] || continue
  _gta_u="$(cygpath -u "$_gta_d" 2>/dev/null)" || continue
  [ -d "$_gta_u" ] || continue
  case ":$PATH:" in *":$_gta_u:"*) continue;; esac
  case "$_gta_u" in
    */WindowsApps|*/WindowsApps/) continue;;
  esac
  if [ -f "$_gta_u/python.exe" ] || [ -f "$_gta_u/conan.exe" ]; then _gta_front="$_gta_front:$_gta_u"; else _gta_back="$_gta_back:$_gta_u"; fi
done
IFS="$_gta_oldifs"
[ -n "$_gta_front" ] && PATH="${_gta_front#:}:$PATH"
[ -n "$_gta_back" ] && PATH="$PATH$_gta_back"
# the python.org per-user install may precede nothing in a stale PATH: make sure python.exe resolves to a real interpreter, not the Store stub
case "$(command -v python 2>/dev/null)" in
  */WindowsApps/*|"")
    for _gta_p in "$HOME"/AppData/Local/Programs/Python/Python3*/ /c/Python3*/ "/c/Program Files/Python3"*/; do
      [ -f "${_gta_p}python.exe" ] && { PATH="${_gta_p%/}:${_gta_p%/}/Scripts:$PATH"; break; }
    done;;
esac
fi
export PATH

# ---- 2. locate Visual Studio -----------------------------------------------------------------------------------------------------------------
_gta_ver="${GTA_MSVC_VER:-14.51}"
_gta_vs="${GTA_VSINSTALL:-}"
if [ -z "$_gta_vs" ]; then
  _gta_pf86="$(printenv 'ProgramFiles(x86)' 2>/dev/null)"; [ -n "$_gta_pf86" ] || _gta_pf86='C:\Program Files (x86)'
  _gta_vswhere="$(cygpath -u "$_gta_pf86")/Microsoft Visual Studio/Installer/vswhere.exe"
  if [ -x "$_gta_vswhere" ]; then
    _gta_vs="$("$_gta_vswhere" -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>/dev/null | tr -d '\r' | head -1)"
  fi
fi
if [ -z "$_gta_vs" ] || [ ! -f "$(cygpath -u "$_gta_vs")/VC/Auxiliary/Build/vcvarsall.bat" ]; then
  echo "msvcenv: no Visual Studio with the C++ x86/x64 tools found (set GTA_VSINSTALL) - see docs/WINDOWS_SETUP.md" >&2
  unset _gta_need _gta_t _gta_regpath _gta_front _gta_back _gta_oldifs _gta_d _gta_u _gta_p _gta_pf86 _gta_vswhere
  return 1 2>/dev/null || exit 1
fi
_gta_vsu="$(cygpath -u "$_gta_vs")"
_gta_arch="${GTA_MSVC_ARCH:-x86}"   # x86 (default; the Win32 gate) or x64 (the PortableWin64 gate)
if [ "$_gta_arch" = x86 ]; then _gta_key="$(printf '%s|%s|x86' "$_gta_vs" "$_gta_ver" | cksum | cut -d' ' -f1)"; else _gta_key="$(printf '%s|%s|%s' "$_gta_vs" "$_gta_ver" "$_gta_arch" | cksum | cut -d' ' -f1)"; fi

# ---- 3. import (or reuse) the vcvarsall environment ------------------------------------------------------------------------------------------
if [ "${GTA_MSVCENV_LOADED:-}" = "$_gta_key" ] && [ -z "${GTA_MSVCENV_FORCE:-}" ] && command -v cl >/dev/null 2>&1; then
  : # already loaded in this shell (or inherited from gtash.cmd-style parent that sourced us)
else
  _gta_scratch="${GTA_MSVCENV_SCRATCH:-C:/gta-scratch}"; _gta_scratch="$(cygpath -u "$_gta_scratch")"
  mkdir -p "$_gta_scratch" 2>/dev/null || _gta_scratch="${TMPDIR:-/tmp}"
  _gta_cache="$_gta_scratch/msvcenv-$_gta_key.sh"
  if [ -n "${GTA_MSVCENV_FORCE:-}" ] || [ ! -f "$_gta_cache" ] || ! grep -q "^# complete$" "$_gta_cache" 2>/dev/null; then
    _gta_bat="$_gta_scratch/msvcenv-$_gta_key.cmd"
    cat > "$_gta_bat" <<EOF
@echo off
rem a previously loaded (x86) VS environment must not leak into the dump: vcvars PREPENDS to these
set INCLUDE=
set LIB=
set LIBPATH=
set EXTERNAL_INCLUDE=
set > "$(cygpath -w "$_gta_scratch")\\msvcenv-$_gta_key.before"
call "$_gta_vs\\VC\\Auxiliary\\Build\\vcvarsall.bat" $_gta_arch -vcvars_ver=$_gta_ver >nul
if errorlevel 1 exit /b 1
set > "$(cygpath -w "$_gta_scratch")\\msvcenv-$_gta_key.after"
EOF
    if ! cmd.exe //c "$(cygpath -w "$_gta_bat")" >/dev/null 2>"$_gta_scratch/msvcenv-$_gta_key.err"; then
      echo "msvcenv: vcvarsall failed - is toolset $_gta_ver installed? (see $_gta_scratch/msvcenv-$_gta_key.err, docs/WINDOWS_SETUP.md)" >&2
      return 1 2>/dev/null || exit 1
    fi
    # diff before/after (case-insensitive names), emit exports; PATH additions are converted to POSIX and prepended
    _gta_py="$(command -v python 2>/dev/null)"
    [ -n "$_gta_py" ] || { echo "msvcenv: no python on PATH to convert the environment (see docs/WINDOWS_SETUP.md)" >&2; return 1 2>/dev/null || exit 1; }
    "$_gta_py" -I - "$_gta_scratch/msvcenv-$_gta_key.before" "$_gta_scratch/msvcenv-$_gta_key.after" > "$_gta_cache.tmp" <<'PY' || { echo "msvcenv: conversion failed" >&2; return 1 2>/dev/null || exit 1; }
import re, subprocess, sys
def load(p):
    d = {}
    for line in open(p, encoding="utf-8", errors="replace").read().splitlines():
        if not line or line.startswith("=") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        d[k.upper()] = (k, v)
    return d
before, after = load(sys.argv[1]), load(sys.argv[2])
def q(s):
    return "'" + s.replace("'", "'\\''") + "'"
out = []
for ku, (k, v) in sorted(after.items()):
    if ku == "PATH":
        old = [e.rstrip("\\").lower() for e in before.get("PATH", ("", ""))[1].split(";") if e]
        new = [e for e in v.split(";") if e and e.rstrip("\\").lower() not in old]
        if new:
            conv = subprocess.run(["cygpath", "-u", "-p", ";".join(new)], capture_output=True, text=True).stdout.strip()
            out.append('export PATH=%s:"$PATH"' % q(conv))
        continue
    if ku in before and before[ku][1] == v:
        continue
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", k):
        continue            # e.g. ProgramFiles(x86): cannot be a bash identifier, and the parent environment already has it
    out.append("export %s=%s" % (k, q(v)))
print("\n".join(out))
print("# complete")
PY
    mv "$_gta_cache.tmp" "$_gta_cache"
    rm -f "$_gta_bat" "$_gta_scratch/msvcenv-$_gta_key.before" "$_gta_scratch/msvcenv-$_gta_key.after"
  fi
  . "$_gta_cache"
  export GTA_MSVCENV_LOADED="$_gta_key"
fi
unset _gta_need _gta_t _gta_regpath _gta_front _gta_back _gta_oldifs _gta_d _gta_u _gta_p _gta_pf86 _gta_vswhere _gta_vs _gta_vsu _gta_key _gta_arch _gta_ver _gta_scratch _gta_cache _gta_bat _gta_py
