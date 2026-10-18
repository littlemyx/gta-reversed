#!/bin/bash
# usage: loop.sh EXE ROUTE TAG N   -> runs N times, logs to /c/gta-scratch/skinflaky/out/TAG/runK
. /c/Users/littl/github/gta-reversed/tools/standalone/hostenv.sh
EXE="$1"; ROUTE="$2"; TAG="$3"; N="$4"
DATA=/c/gta-scratch/sf_data; OUT=/c/gta-scratch/skinflaky/out/$TAG; mkdir -p "$DATA" "$OUT"
GAME_SRC="$DEF_GAME_SRC"
for e in "$GAME_SRC"/*; do n="$(basename "$e")"; case "$n" in gta_sa.exe|gta-sa.exe|gta_reversed.exe|logs|standalone*) continue;; esac
  [ -e "$DATA/$n" ] && continue
  if [ -d "$e" ]; then cmd //c mklink //J "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp -R "$e" "$DATA/$n"
  else cmd //c mklink //H "$(cygpath -w "$DATA/$n")" "$(cygpath -w "$e")" >/dev/null 2>&1 || cp "$e" "$DATA/$n"; fi
done
cd "$DATA"
UD="$(userdir_new sf)"
crashes=0
for k in $(seq 1 $N); do
  /c/Users/littl/github/gta-reversed/tools/standalone/gpu_locked.sh -o "SKIN-FLAKY $TAG $k" bash -c '
    cd '"$DATA"'; . /c/Users/littl/github/gta-reversed/tools/standalone/hostenv.sh
    userdir_reset '"$UD"'; rm -f gta_reversed.exe standalone*.log; rm -rf logs; cp '"$EXE"' gta_reversed.exe
    export NOTSA_STANDALONE_SKIP_VIDEOS=1 NOTSA_STANDALONE_MUTE=1 NOTSA_STANDALONE_INPUT='"$ROUTE"' NOTSA_STANDALONE_USERDIR='"$UD"' NOTSA_STANDALONE_WINDOWED=960x600 NOTSA_STANDALONE_NOVSYNC=1 '"${EXTRA_ENV:-}"'
    ./gta_reversed.exe > run.out 2>&1 &  P=$!; for t in $(seq 1 '${LIMIT:-240}'); do sleep 1; kill -0 $P 2>/dev/null || break; done; kill -9 $P 2>/dev/null; wait $P
    echo "exit=$?" > status
  '
  R="$OUT/run$k"; mkdir -p "$R"; cp -f "$DATA/standalone.log" "$DATA/logs/log.log" "$DATA/status" "$DATA/run.out" "$R/" 2>/dev/null
  if grep -qE "UNHANDLED EXCEPTION|SKINBAD" "$R/log.log" "$R/run.out" 2>/dev/null; then crashes=$((crashes+1)); echo "run$k CRASH: $(grep -m1 -A0 'gta_reversed!' $R/log.log | cut -c60-140)"; else echo "run$k ok $(cat $R/status) $(grep -c '\[probe\]' $R/standalone.log) probes"; fi
done
echo "TAG $TAG: $crashes crashes / $N"
