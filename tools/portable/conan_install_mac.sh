#!/usr/bin/env bash
# Conan dependencies for the PortableMac preset (native macOS arm64, Apple clang) -> build/PortableMac/generators.
#
# Why not a plain `conan install .`: conanfile.py generate() rewrites ConanPresets.json (included by CMakePresets.json, used by the Windows
# presets) and deletes/re-copies source/libs/imgui. This script runs conan from a scratch copy of conanfile.py, so the repository tree is untouched,
# then keeps only the generators (relocatable: absolute paths into ~/.conan2/p).
#
# usage: tools/portable/conan_install_mac.sh [build_type: Release|Debug]     env: CONAN (default ~/.local/bin/conan), TMPDIR
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
CONAN="${CONAN:-$HOME/.local/bin/conan}"
BT="${1:-Release}"
DST="$ROOT/build/PortableMac/generators"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/portable-conan.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
cp "$ROOT/conanfile.py" "$WORK/"
mkdir -p "$WORK/source"
( cd "$WORK" && "$CONAN" install . -pr:h "$ROOT/conan/profiles/macos-arm64.txt" -pr:b default -s build_type="$BT" \
      --build=missing -of "$WORK/out" )
GEN="$(find "$WORK/out" -name conan_toolchain.cmake -print -quit | xargs dirname)"
rm -rf "$DST"; mkdir -p "$(dirname "$DST")"; cp -R "$GEN" "$DST"
# conan_toolchain.cmake / *-config.cmake refer to $WORK only through relative generators dir: verify nothing points into the scratch dir
if grep -rl "$WORK" "$DST" >/dev/null 2>&1; then echo "WARNING: generators reference the scratch dir $WORK" >&2; grep -rl "$WORK" "$DST" >&2; fi
echo "generators: $DST"
