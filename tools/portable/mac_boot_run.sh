#!/bin/bash
# MAC-BOOT1: run the native PortableMac exe against the Steam copy WITHOUT writing into it. Usage:
#   tools/portable/mac_boot_run.sh <tag> <timeout_s> ["input script"]      (e.g. "wait:1500;key:up;wait:500;key:return;wait:1200;key:return;wait:1500;key:return;wait:1500;key:return" = New Game)
# - cwd = $OUT/gamedir: symlinks to the Steam data (read only use) + own writable logs/ and standalone.log; user files (gta_sa.set, saves) go to $OUT/userdir
#   (NOTSA_STANDALONE_USERDIR), nothing is written into the Steam copy;
# - NOTSA_STANDALONE_NOVSYNC=1 (default here; MAC-W2: export NOTSA_STANDALONE_NOVSYNC=0 to run with vsync): SDL3's Cocoa swap waits forever for a CVDisplayLink tick when the display is asleep / the window is not visible (agent runs);
# - NOTSA_HANG_DUMP_SECS=<timeout-2>: a hang is turned into a call stack of the main thread (lldb / sample cannot attach to this exe on the dev Mac);
# - MAC-W3 audio: NOTSA_AUDIO=loopback (default here: ALC_SOFT_loopback, nothing is audible while an agent runs the game) and NOTSA_AUDIO_WAV=$OUT/<tag>.wav (the first
#   seconds of the mixed output); set NOTSA_AUDIO=device to hear it, NOTSA_AUDIO=null for no OpenAL at all;
# - the exe is killed (process group) after <timeout_s>+3 s. Output: $OUT/<tag>.out
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-${TMPDIR:-/tmp}/mac_boot}"; mkdir -p "$OUT"
TAG="${1:?tag}"; TMO="${2:-45}"; SCRIPT="${3:-}"
EXE="${EXE:-$REPO/build/PortableMac/bin/gta_reversed}"
STEAM="${GTA_SA_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/grand theft auto - san andreas/Grand Theft Auto - San Andreas.app/Contents/Resources/transgaming/c_drive/Program Files/Rockstar Games/GTA San Andreas}"
G="$OUT/gamedir"
if [ ! -d "$G" ]; then
  mkdir -p "$G/logs" "$OUT/userdir"
  for e in Text anim audio autorun.inf data models movies stream.ini; do [ -e "$STEAM/$e" ] && ln -s "$STEAM/$e" "$G/$e"; done
fi
cd "$G" || exit 1
# MAC-NAN1: a scripted run ignores the REAL pointer (SDLWrapper), so the menu cursor stays where the game puts it: on "Options". Routes that start with "key:return" (no "key:up") relied on the
# parked real pointer to have "Start Game" selected: hover it explicitly (mouse:636,290 = the item, 1280x720 game pixels) in a per-run copy of the route.
if [ -f "$SCRIPT" ] && grep -q '^until:menu' "$SCRIPT"; then
  python3 - "$SCRIPT" "$OUT/route_$TAG.txt" <<'PY'
import sys
src, dst = sys.argv[1:3]
L = open(src).read().split("\n"); o = []; done = False
for i, l in enumerate(L):
    o.append(l)
    if not done and l.startswith("until:menu"):
        done = True
        if i + 2 < len(L) and L[i + 1].startswith("wait:") and L[i + 2] == "key:return":  # the "key:up" preambles select Start Game by keyboard
            o += ["wait:600", "mouse:636,290", "wait:400"]
open(dst, "w").write("\n".join(o))
PY
  SCRIPT="$OUT/route_$TAG.txt"
fi
export NOTSA_AUDIO="${NOTSA_AUDIO:-loopback}" NOTSA_AUDIO_WAV="${NOTSA_AUDIO_WAV:-$OUT/$TAG.wav}"
export NOTSA_STANDALONE_USERDIR="$OUT/userdir" NOTSA_STANDALONE_SKIP_VIDEOS=1 NOTSA_STANDALONE_NOVSYNC="${NOTSA_STANDALONE_NOVSYNC:-1}" NOTSA_HANG_DUMP_SECS=$((TMO-2)) NOTSA_STANDALONE_INPUT="$SCRIPT"
python3 - "$EXE" "$OUT/$TAG.out" "$TMO" <<'PY'
import subprocess, sys, os, signal, time
exe, out, tmo = sys.argv[1], sys.argv[2], int(sys.argv[3])
p = subprocess.Popen([exe], stdout=open(out, "w"), stderr=subprocess.STDOUT, start_new_session=True)
t = time.time()
try:
    rc = p.wait(timeout=tmo + 3); print("exit", rc, "after %.1fs" % (time.time() - t))
except subprocess.TimeoutExpired:
    os.killpg(p.pid, signal.SIGKILL); print("killed after %ds" % tmo)
PY
