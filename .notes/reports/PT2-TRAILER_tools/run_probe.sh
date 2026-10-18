#!/bin/bash
# usage: run_probe.sh <exe> <outdir> <route> [limit_s]   -- runs ours with the probe under the GPU lock, private data dir C:/gta-scratch/ptt_data
EXE="$1"; OUT="$2"; ROUTE="$3"; LIM="${4:-240}"
DATA=C:/gta-scratch/ptt_data
GAME_SRC="/c/Program Files (x86)/Steam/steamapps/common/Grand Theft Auto San Andreas"
mkdir -p "$DATA" "$OUT"
for e in "$GAME_SRC"/*; do n="$(basename "$e")"; case "$n" in gta_sa.exe|gta-sa.exe|gta_reversed.exe|logs|standalone*) continue;; esac
  [ -e "$DATA/$n" ] && continue
  if [ -d "$e" ]; then cmd //c mklink //J "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp -R "$e" "$DATA/$n"
  else cmd //c mklink //H "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp "$e" "$DATA/$n"; fi
done
DOCSET="$(cygpath -u "$USERPROFILE")/Documents/GTA San Andreas User Files/gta_sa.set"
[ -f "$DOCSET" ] && [ ! -e "$DOCSET.soak-bak" ] && cp -p "$DOCSET" "$DOCSET.soak-bak"
[ -z "${PTT_HAVE_LOCK:-}" ] && until mkdir C:/gta-scratch/gpu.lock 2>/dev/null; do sleep 0.2; done
[ -z "${PTT_HAVE_LOCK:-}" ] && echo "PT2-TRAILER $(date)" > C:/gta-scratch/gpu.lock/owner
cd "$DATA"
rm -f "$DOCSET" gta_reversed.exe gta_sa.exe standalone*.log frame_*.bmp mark_*.bmp; rm -rf logs
cp "$EXE" gta_reversed.exe
export NOTSA_STANDALONE_SKIP_VIDEOS=1 NOTSA_STANDALONE_MUTE=1 NOTSA_STANDALONE_WINDOWED=960x600 NOTSA_STANDALONE_PROBE=1
export NOTSA_STANDALONE_SCREENSHOT=100000 NOTSA_STANDALONE_SCREENSHOT_MAX=40 NOTSA_STANDALONE_INPUT="$ROUTE"
./gta_reversed.exe > "$OUT/run.out" 2>&1 &
WPID=$!
T0=$(date +%s)
while kill -0 $WPID 2>/dev/null; do sleep 3; [ $(( $(date +%s) - T0 )) -gt "$LIM" ] && { echo timeout; break; }; done
kill -9 $WPID 2>/dev/null
powershell.exe -NoProfile -Command 'Get-Process gta_reversed -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "C:\gta-scratch\ptt_data\*" } | Stop-Process -Force' >/dev/null 2>&1
sleep 1
cp -f standalone.log logs/log.log "$OUT/" 2>/dev/null
for b in mark_*.bmp frame_*.bmp; do [ -f "$b" ] && cp "$b" "$OUT/"; done
rm -f "$DOCSET"
[ -z "${PTT_HAVE_LOCK:-}" ] && rm -rf C:/gta-scratch/gpu.lock
echo done
