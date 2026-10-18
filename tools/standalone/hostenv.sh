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
  # the orchestrator session and its agents share one build.lock in the session scratchpad: use it when present
  _sess=/private/tmp/claude-501/-Users-Andrei-Mukhin-github-recomp-gta-reversed/aa4942c3-1b0c-4e82-a9c0-0c095cb1fe07/scratchpad
  if [ -d "$_sess" ]; then DEF_SCRATCH="$_sess"; else DEF_SCRATCH="${TMPDIR:-/tmp}/gta-standalone-scratch"; fi
fi

# python: an explicit GTASA_PYTHON wins; otherwise the first candidate that really runs (on Windows `python3` is often the Microsoft Store stub)
PY="${GTASA_PYTHON:-}"
if [ -z "$PY" ]; then
  for _c in python3 python py; do
    if command -v "$_c" >/dev/null 2>&1 && "$_c" -c 'import sys' >/dev/null 2>&1; then PY="$_c"; break; fi
  done
  PY="${PY:-python3}"
fi

# USERDIR: a private "user files" dir per run, so an automated run never reads/writes the user's Documents\GTA San Andreas User Files (gta_sa.set, saves, Gallery,
# User Tracks). The run exe (NOTSA_STANDALONE_USERDIR, FileMgr.cpp InitUserDirectories) uses it instead of Documents. Windows host only (the Wine hosts have their own prefix).
#   userdir_new <tag> [seed]   creates a FRESH $DEF_SCRATCH/userdir/<tag>-<timestamp>-<pid>, prints its mixed path. seed = "playtest1" copies C:/gta-scratch/playtest1/gta_sa.set (the user's
#                              settings of playtest 1) into it, a file path copies that file, "" / "none" leaves the dir empty (first start, like the scripts always did).
#   userdir_reset <dir> [seed] empties <dir> and re-seeds it (use before every start attempt of a run).
userdir_reset() {   # $1 dir (must be under $DEF_SCRATCH/userdir/), $2 seed
  case "$1" in "$DEF_SCRATCH"/userdir/?*) ;; *) echo "userdir_reset: refusing $1" >&2; return 1;; esac
  rm -rf "$1"; mkdir -p "$1"
  case "${2:-}" in ""|none) ;; playtest1) cp -p "$DEF_SCRATCH/playtest1/gta_sa.set" "$1/gta_sa.set";; *) cp -p "$2" "$1/gta_sa.set";; esac
}
userdir_new() {
  local d="$DEF_SCRATCH/userdir/$1-$(date +%Y%m%d-%H%M%S)-$$"
  userdir_reset "$d" "${2:-}" || return 1
  printf '%s' "$d"
}
# The ORIGINAL gta_sa.exe cannot be redirected (no env, always Documents\GTA San Andreas User Files): a run of it backs the whole dir up first and restores it afterwards
# (files, content and mtimes; the restore also deletes whatever the run created).   docs_backup <dest-dir>   /   docs_restore <dest-dir>   (Windows host)
DOCS_USERFILES="$( [ "$HOST" = windows ] && echo "$(cygpath -u "$USERPROFILE")/Documents/GTA San Andreas User Files" )"
docs_backup() { rm -rf "$1"; mkdir -p "$1"; [ -d "$DOCS_USERFILES" ] && cp -a "$DOCS_USERFILES/." "$1/"; return 0; }
docs_restore() { [ -d "$1" ] || return 0; mkdir -p "$DOCS_USERFILES"; find "$DOCS_USERFILES" -mindepth 1 -delete 2>/dev/null; cp -a "$1/." "$DOCS_USERFILES/"; }
