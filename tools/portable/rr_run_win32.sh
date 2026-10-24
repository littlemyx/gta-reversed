#!/bin/bash
# RR-3 (MAC-RR3): one deterministic record run of the WIN32 reference exe (preset StandaloneRelease + -DGTASA_STATEHASH=ON, build/StandaloneReleaseRR) under Wine on the Mac.
# Same pins, seed, step and injector script as tools/portable/rr_run.sh (the native run). Usage:
#   tools/portable/rr_run_win32.sh <tag> <timeout_s> [route]       -> $OUT/<tag>.trace (+ $OUT/<tag>.out wine stdout, $OUT/<tag>.standalone.log, $OUT/<tag>.log.log)
# env: EXE (default build/StandaloneReleaseRR/bin/gta_reversed.exe), OUT (default /tmp/rr3), PREFIX (private Wine prefix, default /tmp/rr3-prefix, cloned from ~/.wine-msvc on first use),
#      RR_SEED (12345), RR_STEP_MS (20), RR_DETAIL (A-B), RR_MAXFRAMES, RR_TAG, GTA_SA_DIR (the Steam copy, read-only: only symlinked), TOOLS_DIR (Wine Staging.app lives in $TOOLS_DIR, default ~/tools).
# The game runs in $OUT/gamedir-<tag>: symlinks to the Steam data + a copy of the exe + its own logs; user files (gta_sa.set, saves) go to $OUT/userdir-<tag>
# (NOTSA_STANDALONE_USERDIR): nothing is written into the Steam copy. Never suspends the game thread (it drifts the x87 state under Wine); the run is bounded by <timeout_s>.
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
TAG="${1:?tag}"; TMO="${2:-300}"; ROUTE="${3:-$REPO/tools/portable/rr_route_walk.txt}"
export OUT="${OUT:-/tmp/rr3}"; mkdir -p "$OUT"
EXE="${EXE:-$REPO/build/StandaloneReleaseRR/bin/gta_reversed.exe}"
TOOLS_DIR="${TOOLS_DIR:-$HOME/tools}"
PREFIX="${PREFIX:-/tmp/rr3-prefix}"
STEAM="${GTA_SA_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/grand theft auto - san andreas/Grand Theft Auto - San Andreas.app/Contents/Resources/transgaming/c_drive/Program Files/Rockstar Games/GTA San Andreas}"
export PATH="$TOOLS_DIR/Wine Staging.app/Contents/Resources/wine/bin:$HOME/.local/bin:/opt/homebrew/bin:$PATH" WINEPREFIX="$PREFIX" WINEDEBUG="${WINEDEBUG:--all}"
[ -f "$EXE" ] || { echo "rr_run_win32: missing $EXE"; exit 2; }
if [ ! -d "$PREFIX" ]; then
  echo "rr_run_win32: cloning Wine prefix ~/.wine-msvc -> $PREFIX"; cp -cR "$HOME/.wine-msvc" "$PREFIX" || exit 2
fi
# a cloned prefix has Documents -> the user's real Documents: replace by an empty directory (the game must never read/write the real user files)
for d in "$PREFIX"/drive_c/users/*/Documents; do [ -L "$d" ] && { rm "$d" && mkdir "$d"; }; done

G="$OUT/gamedir-$TAG"; U="$OUT/userdir-$TAG"
rm -rf "${G:?}" "${U:?}"; mkdir -p "$G/logs" "$U"
for e in "$STEAM"/*; do
  n="$(basename "$e")"
  case "$n" in gta_sa.exe|gta_reversed.exe|logs|standalone*) continue;; esac
  ln -s "$e" "$G/$n"
done
cp -c "$EXE" "$G/gta_reversed.exe"
cd "$G" || exit 1

export NOTSA_RR="${RR:-1}" NOTSA_RR_SEED="${RR_SEED:-12345}" NOTSA_RR_STEP_MS="${RR_STEP_MS:-20}" NOTSA_RR_TRACE="rr.trace" NOTSA_RR_TAG="${RR_TAG:-win32-$(git -C "$REPO" rev-parse --short HEAD)}"
export NOTSA_RR_NOAUDIO="${NOTSA_RR_NOAUDIO:-1}"
[ -n "${RR_DETAIL:-}" ] && export NOTSA_RR_DETAIL="$RR_DETAIL"
[ -n "${RR_MAXFRAMES:-}" ] && export NOTSA_RR_MAXFRAMES="$RR_MAXFRAMES" NOTSA_RR_EXIT=1
export NOTSA_STANDALONE_NOVSYNC=1 NOTSA_STANDALONE_SKIP_VIDEOS=1 NOTSA_STANDALONE_MUTE=1 NOTSA_STANDALONE_USERDIR="Z:${U//\//\\}" NOTSA_STANDALONE_INPUT="$ROUTE"

# wined3d flakes at start-up (~1 in 3 under Wine on this Mac: an access violation in ddraw/wined3d inside psInitialize, nothing of the game has run): such a run is retried
for attempt in 1 2 3 4 5 6; do
  wineserver -k 2>/dev/null; sleep 1
  rm -rf logs/* rr.trace standalone.log
  python3 - "$OUT/$TAG.out" "$TMO" <<'PY'
import subprocess, sys, os, signal, time
out, tmo = sys.argv[1], int(sys.argv[2])
p = subprocess.Popen(["wine", "gta_reversed.exe"], stdout=open(out, "w"), stderr=subprocess.STDOUT, start_new_session=True)
t = time.time()
while p.poll() is None and time.time() - t < tmo:
    time.sleep(1)
    # the second start-up flake: the process hangs after "CPU vendor" before the video modes are listed (never seen after 'rw video modes')
    if time.time() - t > 40:
        try: started = "rw video modes" in open("standalone.log", errors="replace").read()
        except OSError: started = False
        if not started:
            os.killpg(p.pid, signal.SIGKILL); print("start-up hang, killed"); sys.exit(0)
if p.poll() is None:
    os.killpg(p.pid, signal.SIGKILL); print("killed after %ds" % tmo)
else:
    print("exit", p.returncode, "after %.1fs" % (time.time() - t))
PY
  if grep -q "rw video modes" standalone.log 2>/dev/null; then :; else echo "rr_run_win32: attempt $attempt: start-up hang/flake, retrying"; continue; fi
  if [ ! -s rr.trace ] && grep -q "Exception Code" logs/log.log 2>/dev/null && grep -q "wined3d\|ddraw" logs/log.log; then echo "rr_run_win32: attempt $attempt: wined3d start-up flake, retrying"; continue; fi
  break
done
wineserver -k 2>/dev/null; sleep 1
pkill -f "$G/gta_reversed.exe" 2>/dev/null
cp -f rr.trace "$OUT/$TAG.trace" 2>/dev/null; cp -f standalone.log "$OUT/$TAG.standalone.log" 2>/dev/null; cp -f logs/log.log "$OUT/$TAG.log.log" 2>/dev/null
echo "trace: $OUT/$TAG.trace ($(grep -c '^F ' "$OUT/$TAG.trace" 2>/dev/null || echo 0) frames)"
