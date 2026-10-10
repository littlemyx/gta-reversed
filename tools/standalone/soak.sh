#!/bin/bash
# Regression soak of the STANDALONE exe under Wine: boots to the menu, starts a new game, skips the cutscene, walks, spawns two cars (cheats), drives,
# idles (idle camera), quits through the pause menu; then prints a one-screen report. Exit code: 0 = clean, 1 = crash/trap/assert/abort/early death,
# 2 = could not start at all.
# usage: tools/standalone/soak.sh [build-dir (default build/StandaloneRelease)] [minutes (default 10)] [game-data-dir (default /tmp/d3s)]
#   minutes = total length of the run: the idle phase is stretched/shrunk to fit (the fixed part of tools/standalone/soak_route.txt is ~6 min; minimum idle 60 s)
# env: GAME_SRC=<install dir>   the original game install (default: the Steam macOS copy, see docs/STANDALONE.md "Run")
#      PREFIX=<dir>             private Wine prefix (default /tmp/d3s-prefix, cloned from $WINE_PREFIX_SRC = ~/.wine-msvc on first use)
#      WINE_BIN=<dir>           directory with the wine binary (default: Wine Staging.app in ~/tools)
#      SHOT_EVERY=<frames>      screenshot cadence (default 500 frames, NOTSA_STANDALONE_SCREENSHOT_MAX=40)
#      SHOT_MAX=<n>             screenshot cap (default 40)
#      OUT=<dir>                output dir (default /tmp/d3s-out/<timestamp>): logs, frame_*.png, report.txt
#      ROUTE=<file>             input route (default tools/standalone/soak_route.txt)
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
BDIR="${1:-build/StandaloneRelease}"; MIN="${2:-10}"; DATA="${3:-/tmp/d3s}"
case "$BDIR" in /*) ;; *) BDIR="$REPO/$BDIR";; esac
BIN="$BDIR/bin"
GAME_SRC="${GAME_SRC:-$HOME/Library/Application Support/Steam/steamapps/common/grand theft auto - san andreas/Grand Theft Auto - San Andreas.app/Contents/Resources/transgaming/c_drive/Program Files/Rockstar Games/GTA San Andreas}"
PREFIX="${PREFIX:-/tmp/d3s-prefix}"; WINE_PREFIX_SRC="${WINE_PREFIX_SRC:-$HOME/.wine-msvc}"
WINE_BIN="${WINE_BIN:-$HOME/tools/Wine Staging.app/Contents/Resources/wine/bin}"
OUT="${OUT:-/tmp/d3s-out/$(date +%Y%m%d-%H%M%S)}"
ROUTE="${ROUTE:-$REPO/tools/standalone/soak_route.txt}"
SHOT_EVERY="${SHOT_EVERY:-500}"
export PATH="$WINE_BIN:$HOME/.local/bin:/opt/homebrew/bin:$PATH" WINEPREFIX="$PREFIX" WINEDEBUG=-all
mkdir -p "$OUT"

for f in gta_reversed.exe data_pointers.bin original_data.bin original_data.json; do
  [ -f "$BIN/$f" ] || { echo "soak: missing $BIN/$f (build gta_reversed.exe first)"; exit 2; }
done
[ -d "$GAME_SRC" ] || { echo "soak: game install not found: $GAME_SRC (set GAME_SRC)"; exit 2; }

# game data dir: symlinks to the install (except the exe), refreshed copies of our exe + data image files
mkdir -p "$DATA"
for e in "$GAME_SRC"/*; do
  n="$(basename "$e")"
  case "$n" in gta_sa.exe|gta_reversed.exe) continue;; esac
  [ -e "$DATA/$n" ] || [ -L "$DATA/$n" ] || ln -s "$e" "$DATA/$n"
done
# private Wine prefix (so this run never clashes with another instance / wineserver)
if [ ! -d "$PREFIX" ]; then
  [ -d "$WINE_PREFIX_SRC" ] || { echo "soak: $WINE_PREFIX_SRC missing, cannot clone a prefix"; exit 2; }
  echo "soak: cloning Wine prefix $WINE_PREFIX_SRC -> $PREFIX"; cp -cR "$WINE_PREFIX_SRC" "$PREFIX"
fi

# route: stretch the idle phase to the requested length
FIXED_MS=$(sed 's/#.*//' "$ROUTE" | awk -F: '/^wait:[0-9]+/ {t+=$2} /^key:/ {t+=150} END {print t+0}')
IDLE_MS=$(( MIN * 60000 - FIXED_MS )); [ "$IDLE_MS" -lt 60000 ] && IDLE_MS=60000
sed "s/@IDLE_MS@/$IDLE_MS/" "$ROUTE" > "$OUT/route.txt"
ROUTE_MS=$(( FIXED_MS + IDLE_MS ))
LIMIT_S=$(( ROUTE_MS / 1000 + 90 ))   # route + slack for the quit sequence

export NOTSA_STANDALONE_SKIP_VIDEOS=1 NOTSA_STANDALONE_SCREENSHOT="$SHOT_EVERY" NOTSA_STANDALONE_SCREENSHOT_MAX="${SHOT_MAX:-40}"
export NOTSA_STANDALONE_MEMLOG=1 NOTSA_STANDALONE_SAMPLER=1 NOTSA_STANDALONE_INPUT="$OUT/route.txt"

kill_exe() { pkill -f "$(basename "$DATA").gta_reversed" 2>/dev/null; WINEPREFIX="$PREFIX" wineserver -k 2>/dev/null; sleep 1; }
started=0; STATUS_START=0
cd "$DATA" || exit 2
for a in 1 2 3 4 5 6; do
  kill_exe
  rm -f gta_reversed.exe data_pointers.bin original_data.* standalone*.log frame_*.bmp mark_*.bmp; rm -rf logs
  cp -c "$BIN/gta_reversed.exe" "$BIN/original_data.bin" "$BIN/original_data.json" "$BIN/data_pointers.bin" .
  wine gta_reversed.exe > "$OUT/wine.out" 2>&1 &
  WPID=$!
  for i in $(seq 1 15); do   # 30 s for the window / video mode
    sleep 2
    grep -q "GcurSelSS" logs/log.log 2>/dev/null && { started=1; break; }
    kill -0 $WPID 2>/dev/null || break
  done
  [ $started = 1 ] && break
  echo "soak: attempt $a: window did not appear within 30 s, retrying"
done
[ $started = 1 ] || { echo "soak: exe never started in 6 attempts (see $OUT/wine.out)"; cp -f logs/log.log standalone.log "$OUT/" 2>/dev/null; kill_exe; exit 2; }
echo "soak: started on attempt $a, route $((ROUTE_MS/1000)) s (idle $((IDLE_MS/1000)) s), outputs in $OUT"

# wait for the route to finish or the process to die
T0=$(date +%s)
while :; do
  sleep 5
  pgrep -f "$(basename "$DATA").gta_reversed" >/dev/null || break
  [ $(( $(date +%s) - T0 )) -gt "$LIMIT_S" ] && { echo "soak: time limit, killing"; TIMEOUT=1; break; }
done
kill_exe
cp -f standalone.log logs/log.log "$OUT/" 2>/dev/null; cp -f standalone_unknown_pointers.txt "$OUT/" 2>/dev/null
mkdir -p "$OUT/shots"
for b in frame_*.bmp mark_*.bmp; do [ -f "$b" ] && sips -s format png "$b" --out "$OUT/shots/${b%.bmp}.png" >/dev/null 2>&1; done
rm -f frame_*.bmp mark_*.bmp

python3 -I - "$OUT" "${TIMEOUT:-0}" 2>&1 <<'PY' | tee "$OUT/report.txt"
import sys, re, os, glob
out, timeout = sys.argv[1], sys.argv[2] == "1"
def rd(p):
    try: return open(os.path.join(out, p), errors="replace").read().splitlines()
    except OSError: return []
sl, gl = rd("standalone.log"), rd("log.log")
marks = []   # (name, script ms, tick)
for l in sl:
    m = re.search(r"\[mark\] (\S+) at (\d+) ms t=(\d+)", l)
    if m: marks.append((m.group(1), int(m.group(2)), int(m.group(3))))
mem = []     # (tick, frame, heap bytes, committed KB)
for l in sl:
    m = re.search(r"memlog t=(\d+) ms frame (\d+) .*?crt heap (\d+) bytes in \d+ blocks, committed (\d+) KB", l)
    if m: mem.append(tuple(int(x) for x in m.groups()))
bad_re = re.compile(r"FATAL|TRAP|[Aa]ssert|abort|[Ee]xception|Unhandled|pure virtual|invalid CRT|terminate")
errs = [l[:200] for l in sl if bad_re.search(l) and "atexit" not in l and "trapped" not in l and "exception handler" not in l]
errs += ["[log.log] " + l[:200] for l in gl if re.search(r"Exception Code|Exception Address|CALL STACK|gta_reversed!|\[critical\]", l)]
softerr = {}   # [error]-level game log lines (pool exhaustion, ...): grouped, listed separately, a non-zero count alone is not a failure
for l in gl:
    if "[error]" in l:
        k = re.sub(r"^\[error\]\[[0-9:.]+\]:?\s*", "", l)[:150]
        softerr[k] = softerr.get(k, 0) + 1
clean_exit = any("atexit handler ran" in l for l in sl)
last_mark = marks[-1][0] if marks else "(none, route never ran)"
order = [m[0] for m in marks]
print("=" * 78)
print("SOAK REPORT  ", out)
print("route markers reached: %d  last: %s" % (len(marks), last_mark))
print("  " + " > ".join(order))
print("quit sequence sent: %s   process exit via atexit: %s   timeout kill: %s" % ("mark:quit_sent" in " ".join("mark:" + m for m in order), clean_exit, timeout))
# fps per phase (frames between consecutive markers from memlog)
print("fps per phase (memlog every 100 frames, same GetTickCount clock as the marks):")
phases = list(zip(marks, marks[1:])) + ([(marks[-1], ("END", 0, mem[-1][0]))] if marks and mem else [])
for (n0, _, t0), (n1, _, t1) in phases:
    pts = [p for p in mem if t0 <= p[0] <= t1]
    if len(pts) >= 2 and pts[-1][0] > pts[0][0]:
        fps = (pts[-1][1] - pts[0][1]) * 1000.0 / (pts[-1][0] - pts[0][0])
        print("  %-20s %7.1f fps  (%d frames in %.0f s)" % (n0 + ">" + n1, fps, pts[-1][1] - pts[0][1], (pts[-1][0] - pts[0][0]) / 1000))
if mem:
    print("memory: peak committed %d MB, peak CRT heap %.1f MB, last committed %d MB (first %d MB), total frames %d" % (max(p[3] for p in mem) // 1024, max(p[2] for p in mem) / 1048576, mem[-1][3] // 1024, mem[0][3] // 1024, mem[-1][1]))
prof = [l for l in sl if "profile" in l and "samples" in l]
if prof: print("sampler: %d profile windows, last: %s" % (len(prof), prof[-1].strip()[:100]))
shots = sorted(glob.glob(os.path.join(out, "shots", "*.png")))
print("screenshots: %d in %s/shots" % (len(shots), out))
print("errors: %d" % len(errs))
for e in errs[:14]: print("  " + e)
print("game [error] log lines (grouped): %d kinds" % len(softerr))
for k, n in sorted(softerr.items(), key=lambda kv: -kv[1])[:6]: print("  %5dx %s" % (n, k))
crash = bool(errs) or timeout or (not clean_exit and not any(n == "quit_sent" for n in order)) or not marks
print("RESULT:", "FAIL" if crash else "OK")
print("=" * 78)
sys.exit(1 if crash else 0)
PY
exit ${PIPESTATUS[0]}
