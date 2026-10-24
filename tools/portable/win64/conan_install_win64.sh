#!/usr/bin/env bash
# Conan dependencies for the PortableWin64 preset (Windows x64, MSVC, static CRT, SDL3 + OpenAL) -> build/PortableWin64/generators.
# Same scratch-copy approach as conan_install_mac.sh / _linux.sh (ConanPresets.json and source/libs/imgui of the repo are not rewritten, except a fresh checkout's source/libs/imgui is filled).
# Additionally patches openal-soft into the scratch conanfile (the Windows ASI/exe builds use DirectSound and do not need it).
#
# usage: tools/portable/win64/conan_install_win64.sh [Release|Debug]
#   env: CONAN, TMPDIR, CONAN_EXTRA_ARGS
#        PROFILE=wine  (default on Darwin/Linux) msvc-wine cross route: tools/portable/win64/conanprofile-wine.txt (needs wine on PATH, WINEPREFIX, ~/tools/msvc)
#        PROFILE=native (default on Windows) conan/profiles/windows-x64-msvc.txt (run from an x64 Native Tools shell, git-bash/msys)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
CONAN="${CONAN:-$(command -v conan || echo $HOME/.local/bin/conan)}"
BT="${1:-Release}"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) PROFILE="${PROFILE:-native}";; *) PROFILE="${PROFILE:-wine}";; esac
if [ "$PROFILE" = wine ]; then HP="$HERE/conanprofile-wine.txt"; else HP="$ROOT/conan/profiles/windows-x64-msvc.txt"; fi
DST="$ROOT/build/PortableWin64/generators"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/portable-conan.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
[ -f "$ROOT/ConanPresets.json" ] || echo '{ "version": 4, "vendor": { "conan": {} } }' > "$ROOT/ConanPresets.json"
python3 - "$ROOT/conanfile.py" "$WORK/conanfile.py" <<'PY'
import sys
s = open(sys.argv[1]).read()
anchor = '        # Required for vanilla build'
assert anchor in s
s = s.replace(anchor, '        self.requires("openal-soft/1.23.1")  # portable Windows build (tools/portable/win64)\n' + anchor, 1)
open(sys.argv[2], 'w').write(s)
PY
mkdir -p "$WORK/source"
( cd "$WORK" && "$CONAN" install . -pr:h "$HP" -pr:b default -s build_type="$BT" ${CONAN_EXTRA_ARGS:-} --build=missing -of "$WORK/out" )
GEN="$(find "$WORK/out" -name conan_toolchain.cmake -print -quit | xargs dirname)"
if [ ! -d "$ROOT/source/libs/imgui" ] && [ -d "$WORK/source/libs/imgui" ]; then mkdir -p "$ROOT/source/libs"; cp -R "$WORK/source/libs/imgui" "$ROOT/source/libs/imgui"; fi
rm -rf "$DST"; mkdir -p "$(dirname "$DST")"; cp -R "$GEN" "$DST"
if grep -rl "$WORK" "$DST" >/dev/null 2>&1; then echo "WARNING: generators reference the scratch dir $WORK" >&2; grep -rl "$WORK" "$DST" >&2; fi
echo "generators: $DST"
