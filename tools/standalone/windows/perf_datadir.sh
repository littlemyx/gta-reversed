#!/bin/bash
# WIN-PERF: build/refresh a private game data dir (NTFS hard links / junctions of the install, like soak.sh) holding either OUR exe or the ORIGINAL oracle exe.
# usage: perf_datadir.sh <datadir> ours|orig [build-dir (default build/StandaloneRelease)]
# ours -> <datadir>/gta_reversed.exe (nothing else of ours since slice F3); orig -> <datadir>/gta_sa.exe = repo-root gta_sa_compact.exe (v1.0 compact; the Steam gta-sa.exe is v3.0, never used)
. "$(dirname "$0")/../hostenv.sh"
REPO="$(cd "$(dirname "$0")/../../.." && pwd)"
DATA="$1"; KIND="$2"; BDIR="${3:-$REPO/build/StandaloneRelease}"
[ -n "$DATA" ] && [ -n "$KIND" ] || { sed -n 2,4p "$0"; exit 2; }
GAME_SRC="${GAME_SRC:-$DEF_GAME_SRC}"
mkdir -p "$DATA"
for e in "$GAME_SRC"/*; do
  n="$(basename "$e")"
  case "$n" in gta_sa.exe|gta-sa.exe|gta_reversed.exe|logs|standalone*) continue;; esac
  [ -e "$DATA/$n" ] && continue
  if [ -d "$e" ]; then cmd //c mklink //J "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp -R "$e" "$DATA/$n"
  else cmd //c mklink //H "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp "$e" "$DATA/$n"; fi
done
rm -rf "$DATA/logs" "$DATA"/standalone*.log "$DATA"/frame_*.bmp "$DATA"/mark_*.bmp
if [ "$KIND" = ours ]; then
  rm -f "$DATA/gta_sa.exe"; rm -f "$DATA"/original_data.* "$DATA/data_pointers.bin"; cp "$BDIR/bin/gta_reversed.exe" "$DATA/"   # F3: the run exe needs no image files
else
  rm -f "$DATA/gta_reversed.exe" "$DATA"/original_data.* "$DATA/data_pointers.bin"; cp "$REPO/gta_sa_compact.exe" "$DATA/gta_sa.exe"
fi
echo "perf_datadir: $DATA ready ($KIND)"
