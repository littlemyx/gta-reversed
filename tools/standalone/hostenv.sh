# Host detection for tools/standalone/*.sh (SOURCE this file, do not run it).
#   HOST=mac|linux     the exe runs under Wine (msvc-wine toolchain, private prefixes, taskpolicy for builds)  -- the original flow, unchanged
#   HOST=windows       Git Bash / MSYS2 on a native Windows PC: the exe runs directly (no Wine, no prefix, no WINEDEBUG, no taskpolicy);
#                      Windows-facing paths are normalised to the "mixed" form (C:/dir) which both bash and native programs understand
# Defaults per host (all overridable by the env vars the individual scripts document):
#   TOOLS_DIR      dir holding the Mac-only tools (Wine Staging.app, msvc-wine install `msvc/`); default $HOME/tools
#   DEF_GAME_SRC   original game install the data dir is linked to
#   DEF_DATA       default game-data (run) dir  -- keep it SHORT (the game copies the cwd into a 128-char buffer)
#   DEF_OUTROOT    default root for soak outputs
#   DEF_SCRATCH    default dir for the shared build mutex (build.lock)
#   PY             python used by the scripts ($GTASA_PYTHON if set, else the first of python3/python that runs)
# Functions: hp <path> (host path: mixed form on Windows, unchanged elsewhere), wp <path> (path as the NATIVE exe wants it: C:\dir on Windows)

case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) HOST=windows ;;
  Darwin)               HOST=mac ;;
  *)                    HOST=linux ;;
esac

TOOLS_DIR="${TOOLS_DIR:-$HOME/tools}"

if [ "$HOST" = windows ]; then
  hp() { case "$1" in [A-Za-z]:*|/*) cygpath -m "$1" ;; *) printf '%s' "$1" ;; esac; }
  wp() { cygpath -w "$1"; }
  DEF_GAME_SRC="/c/gta"
  if [ ! -d "$DEF_GAME_SRC" ]; then
    for _d in "/c/Program Files (x86)/Steam/steamapps/common/Grand Theft Auto San Andreas" "/c/Program Files/Steam/steamapps/common/Grand Theft Auto San Andreas"; do
      [ -d "$_d" ] && { DEF_GAME_SRC="$_d"; break; }
    done
  fi
  DEF_DATA="/c/gta-soak"
  DEF_OUTROOT="/c/gta-soak-out"
  DEF_SCRATCH="/c/gta-scratch"
else
  hp() { printf '%s' "$1"; }
  wp() { printf 'Z:%s' "${1//\//\\}"; }     # Wine: the host root is drive Z:
  DEF_GAME_SRC="$HOME/Library/Application Support/Steam/steamapps/common/grand theft auto - san andreas/Grand Theft Auto - San Andreas.app/Contents/Resources/transgaming/c_drive/Program Files/Rockstar Games/GTA San Andreas"
  DEF_DATA="/tmp/d3s"
  DEF_OUTROOT="/tmp/d3s-out"
  DEF_SCRATCH="${TMPDIR:-/tmp}/gta-standalone-scratch"
fi

# python: an explicit GTASA_PYTHON wins; otherwise the first candidate that really runs (on Windows `python3` is often the Microsoft Store stub)
PY="${GTASA_PYTHON:-}"
if [ -z "$PY" ]; then
  for _c in python3 python; do
    if command -v "$_c" >/dev/null 2>&1 && "$_c" -c 'import sys' >/dev/null 2>&1; then PY="$_c"; break; fi
  done
  PY="${PY:-python3}"
fi
