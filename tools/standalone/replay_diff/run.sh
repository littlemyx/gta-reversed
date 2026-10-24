#!/bin/bash
# REPLAY-DIFF runner (slice 1, .notes/REPLAY_DIFF_PLAN.md section 8): runs both legs of one scenario N times each and prints the frames-equal table
#   orig-orig / port-port (A-A: every mismatch is a harness bug) and orig-port (A-B: every mismatch is a port finding), via rd_diff.py.
# usage: run.sh [intro] [--reps 3] [--pin 31] [--frames 3000] [--port-exe <gta_reversed.exe>] [--out <dir>] [--secs-orig 45] [--secs-port 45] [--legs o,p] [--build-proxy] [--rlog N]
#   scenario "intro" = menu -> new game by real-time input (same route as gameplay_regress/orig/cheats.scn up to gstate=9), then NO input; the trace starts at the arm point (first frame with a player ped).
#   --pin     NOTSA_LOCKSTEP_PIN / RD_PIN bits: 1 time-derived srand seeds, 2 CTimer clock, 4 psTimer, 8 RNG re-seed at the arm point, 16 frame counter jump at the arm point (default 31 = all)
#            32 R log, 64 D probe, 128 audio RNG partition, 256 CdStream tick barrier, 512 audio off (= RR harness NOTSA_RR_NOAUDIO; rd_orig.c patches CAudioEngine::Service to ret); all = 1023
#   --rlog N  with pin bit 32 (add 32 to --pin): log every rand() call (R fc site) of the first N traced frames, for noise hunting
#   --frames  compare the first N traced frames; a run with fewer traced frames than N (focus lost, crash) is repeated (up to 2 more times)
# What it touches: gpu.lock around EVERY game run (rd_orig_run.sh / rd_port_run.sh acquire it per run; waits while another owner has it, USER-PLAY included); the ORIGINAL leg backs up
#   Documents\GTA San Andreas User Files and restores it after every run (hostenv.sh docs_backup/docs_restore, as gameplay_regress/orig_run.sh) - this script prints a before/after signature of that
#   directory and FAILS LOUDLY if it differs; the port leg uses a private NOTSA_STANDALONE_USERDIR (seeded with the same gta_sa.set bytes as the original leg) and the Steam data by links.
#   Never touches C:\gta-play, never uses the Steam gta-sa.exe; the original exe is a private copy of gta_sa_compact.exe in C:/gta-scratch/rdd/orig (+ the vorbisFile.dll tracer proxy).
# Output: <out>/report.md (table + first divergence), <out>/<tag>.trace per run.   Exit code: 0 = every compared pair fully equal, 1 = some pair differs, 2 = harness error / short run.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/../hostenv.sh"
SCN=intro; REPS=3; PIN=31; FRAMES=3000; EXE="$(cd "$HERE/../../.." && pwd)/build/StandaloneRelease/bin/gta_reversed.exe"; OUT=C:/gta-scratch/rdd/out-run; SO=45; SP=45; LEGS=o,p; BP=0; RLOG=0
while [ $# -gt 0 ]; do case "$1" in
  --reps) REPS="$2"; shift;; --pin) PIN="$2"; shift;; --frames) FRAMES="$2"; shift;; --port-exe) EXE="$2"; shift;; --out) OUT="$2"; shift;;
  --secs-orig) SO="$2"; shift;; --secs-port) SP="$2"; shift;; --legs) LEGS="$2"; shift;; --rlog) RLOG="$2"; shift;; --build-proxy) BP=1;;
  intro) SCN=intro;; *) echo "unknown argument $1"; sed -n 2,4p "$0"; exit 2;; esac; shift; done
OUT="$(hp "$OUT")"; EXE="$(hp "$EXE")"; mkdir -p "$OUT"
RDX="$(hp "${RD_PX:-C:/gta-scratch/rdd/px}")"
if [ "$BP" = 1 ] || [ ! -f "$RDX/vorbisFile.dll" ]; then   # the original-exe tracer proxy
  ( . "$HERE/../windows/msvcenv.sh" >/dev/null 2>&1; bash "$HERE/build_orig.sh" "$RDX" >/dev/null 2>&1 )
  [ -f "$RDX/vorbisFile.dll" ] || { echo "run.sh: cannot build the tracer proxy"; exit 2; }
fi
docs_sig() { ( cd "$DOCS_USERFILES" 2>/dev/null && { find . -type f -printf '%p %s %TY%Tm%Td%TH%TM%TS\n' | sort; find . -type f -exec md5sum {} + | sort -k2; } | md5sum ) ; }
SIG0="$(docs_sig)"; echo "docs signature before: $SIG0"
export RD_MAXF="$FRAMES" NOTSA_LOCKSTEP_MAXF="$FRAMES" RD_RLOG="$RLOG" NOTSA_LOCKSTEP_RLOG="$RLOG"
nfr() { local n; n="$(grep -c '^F ' "$OUT/$1.trace" 2>/dev/null)"; echo "${n:-0}"; }
run_leg() {   # $1 = o|p  $2 = rep
  local tag="$1$2" try=0 n=0
  while [ $try -lt 3 ]; do
    try=$((try+1))
    if [ $try -gt 1 ]; then mkdir -p "$OUT/failed"; for f in "$OUT/$tag".*; do [ -f "$f" ] && mv -f "$f" "$OUT/failed/try$((try-1))-$(basename "$f")"; done; fi   # keep the logs of a short try
    rm -f "$OUT/$tag".*
    if [ "$1" = o ]; then bash "$HERE/rd_orig_run.sh" "$tag" "$PIN" "$OUT" "$SO" > "$OUT/$tag.runner.log" 2>&1
    else bash "$HERE/rd_port_run.sh" "$tag" "$PIN" "$OUT" "$EXE" "$SP" > "$OUT/$tag.runner.log" 2>&1; fi
    n="$(nfr "$tag")"; echo "run $tag try $try: $n frames traced ($(date +%H:%M:%S))"
    [ "$n" -ge "$FRAMES" ] && return 0
  done
  return 1
}
FAIL=0
for r in $(seq 1 "$REPS"); do
  case ",$LEGS," in *,o,*)
    run_leg o "$r" || { echo "WARNING: o$r < $FRAMES frames"; FAIL=2; }
    S="$(docs_sig)"; [ "$S" = "$SIG0" ] || { echo "ERROR: Documents user files differ after o$r (signature $S) - restore them from $OUT/o$r.userfiles_backup"; exit 2; };;
  esac
  case ",$LEGS," in *,p,*) run_leg p "$r" || { echo "WARNING: p$r < $FRAMES frames"; FAIL=2; };; esac
done
SIG1="$(docs_sig)"; echo "docs signature after: $SIG1 (before: $SIG0)"
pair() {   # $1 $2 = tags; prints one table row, sets DIFFERS=1 when not fully equal
  [ -f "$OUT/$1.trace" ] && [ -f "$OUT/$2.trace" ] || return 0
  local s c p e f
  s="$("$PY" -I "$HERE/rd_diff.py" "$OUT/$1.trace" "$OUT/$2.trace" --max "$FRAMES" | grep '^SUMMARY')"
  c=$(sed 's/.*compared=\([0-9]*\).*/\1/' <<<"$s"); p=$(sed 's/.*prefix=\([0-9]*\).*/\1/' <<<"$s"); e=$(sed 's/.*equal=\([0-9]*\).*/\1/' <<<"$s"); f=${s##*first=}
  echo "| $1-$2 | $c | $p | $e | $f |"
  [ "$f" = none ] || echo 1 > "$OUT/.differs"
}
rm -f "$OUT/.differs"
{
  echo "# REPLAY-DIFF run: scenario $SCN, pin $PIN, first $FRAMES traced frames, $(date '+%Y-%m-%d %H:%M')"
  echo "port exe: $EXE ($(stat -c %s "$EXE" 2>/dev/null) bytes)   docs signature unchanged: $([ "$SIG1" = "$SIG0" ] && echo yes || echo NO)"
  echo; echo "| pair | compared | equal prefix | equal frames | first divergence |"; echo "|---|---|---|---|---|"
  for k in o p; do for i in $(seq 1 "$REPS"); do for j in $(seq $((i+1)) "$REPS"); do pair $k$i $k$j; done; done; done
  for i in $(seq 1 "$REPS"); do pair o$i p$i; done
  echo; echo "first divergence detail, o1-p1:"; echo '```'
  if [ -f "$OUT/o1.trace" ] && [ -f "$OUT/p1.trace" ]; then "$PY" -I "$HERE/rd_diff.py" "$OUT/o1.trace" "$OUT/p1.trace" --max "$FRAMES" | grep -v SUMMARY; fi
  echo '```'
} > "$OUT/report.md" 2>&1
cat "$OUT/report.md"
[ "$FAIL" = 0 ] && [ -f "$OUT/.differs" ] && FAIL=1
exit "$FAIL"
