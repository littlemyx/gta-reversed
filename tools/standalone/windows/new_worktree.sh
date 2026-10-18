#!/bin/bash
# Create a ready-to-build git worktree of the main checkout (Windows host): branch from HEAD, librw submodule, the (untracked) conan generators + imgui copy
# + ConanPresets.json copied from the main checkout, and the preset configured - optionally with the compile cache (docs/WINDOWS_SETUP.md "Compile cache").
# usage: tools/standalone/windows/new_worktree.sh <id> [--cache] [--preset StandaloneRelease|StandaloneRW|...] [--base <rev>]
#        -> C:/gta-wt/<id> (WT_ROOT overrides), branch <id>.  Build there with:  tools/standalone/build_locked.sh ninja -j4 -C build/<preset> gta_reversed.exe
#        --cache: -DGTASA_COMPILE_CACHE=ON (sccache from C:/gta-tools; tools/standalone/windows/get_sccache.sh installs it).  Remove the worktree when done:
#        git worktree remove --force C:/gta-wt/<id> && git branch -D <id>
# Needs the MSVC x86 environment in the shell (source tools/standalone/windows/msvcenv.sh).
set -eu
[ $# -ge 1 ] || { sed -n '2,8p' "$0"; exit 2; }
ID="$1"; shift; CACHE=0; PRESET=StandaloneRelease; BASE=HEAD
while [ $# -gt 0 ]; do case "$1" in --cache) CACHE=1;; --preset) PRESET="$2"; shift;; --base) BASE="$2"; shift;; *) echo "unknown option $1" >&2; exit 2;; esac; shift; done
HERE="$(cd "$(dirname "$0")" && pwd)"; MAIN="$(cd "$HERE/../../.." && git rev-parse --show-toplevel)"
MAIN="$(cd "$MAIN" && cd "$(git rev-parse --git-common-dir)/.." && pwd)"          # the main checkout even when run from another worktree
. "$MAIN/tools/standalone/hostenv.sh"
WT="${WT_ROOT:-C:/gta-wt}/$ID"
git -C "$MAIN" worktree add "$WT" -b "$ID" "$BASE"
cd "$WT"
git submodule update --init --reference "$MAIN/vendor/librw" vendor/librw 2>/dev/null || git submodule update --init vendor/librw
mkdir -p build/Debug build/Release source/libs
cp "$MAIN/ConanPresets.json" .
cp -r "$MAIN/build/Debug/generators" build/Debug/; cp -r "$MAIN/build/Release/generators" build/Release/
cp -r "$MAIN/source/libs/imgui" source/libs/
EXTRA=(); [ "$CACHE" = 1 ] && EXTRA+=(-DGTASA_COMPILE_CACHE=ON)
GP="$(sed -n "s/^GTASA_PYTHON:FILEPATH=//p" "$MAIN/build/StandaloneRelease/CMakeCache.txt" 2>/dev/null | head -1)"; [ -n "$GP" ] || GP="$(command -v "$PY")"
cmake --preset "$PRESET" -DGTASA_PYTHON="$GP" --compile-no-warning-as-error "${EXTRA[@]}" > "$WT/build/configure-$PRESET.log" 2>&1 || { tail -20 "$WT/build/configure-$PRESET.log"; exit 1; }
grep -h "Compile cache" "$WT/build/configure-$PRESET.log" || true
echo "worktree $WT ready (preset $PRESET, cache=$CACHE)"
