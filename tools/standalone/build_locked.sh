#!/bin/bash
# Run ONE build command (ninja/cmake/conan ...) under the shared build mutex, with a hard timeout and, on macOS, at background priority.
# This is the script form of the PORT_BRIEF snippet "until mkdir build.lock ...; timeout; taskpolicy"; it uses the same lock dir, so it
# interlocks with run_all_tests.sh and with agents that still use the hand-written snippet.
# usage: tools/standalone/build_locked.sh <command> [args...]        e.g.  tools/standalone/build_locked.sh ninja -j6 -C build/StandaloneRelease gta_reversed.exe
# env:   SCRATCH     dir that holds build.lock (default: Mac $TMPDIR/gta-standalone-scratch, Windows C:/gta-scratch)
#        TIMEOUT_S   hard timeout of the command in seconds (default 2400)
#        LOCK_WARN_MIN  warn (once) when the lock we wait for is older than this many minutes (default 20: hung build, see PORT_BRIEF)
# Mac:     taskpolicy -b nice -n 15 perl -e 'alarm shift; exec @ARGV' <secs> <command>     (no coreutils `timeout` there)
# Windows: GNU `timeout` from Git Bash; on a timeout cl.exe/link.exe left over by the killed ninja are killed too (we hold the lock, so they are ours).
#          Run it from the MSVC x86 shell (tools/standalone/windows/gtash.cmd, or `source tools/standalone/windows/msvcenv.sh`), so that cl is on PATH.
# Exit code: that of the command; 124 = timeout; 2 = usage.
set -u
. "$(dirname "$0")/hostenv.sh"
[ $# -ge 1 ] || { sed -n '2,13p' "$0"; exit 2; }
SCRATCH="$(hp "${SCRATCH:-$DEF_SCRATCH}")"; mkdir -p "$SCRATCH"
TIMEOUT_S="${TIMEOUT_S:-2400}"; LOCK_WARN_MIN="${LOCK_WARN_MIN:-20}"
LOCK="$SCRATCH/build.lock"; HELD=0; WARNED=0
release() { [ "$HELD" = 1 ] && { rm -f "$LOCK/pid"; rmdir "$LOCK" 2>/dev/null; }; HELD=0; }
trap release EXIT; trap 'exit 130' INT TERM
until mkdir "$LOCK" 2>/dev/null; do
  # A holder killed hard (e.g. an agent's tool timeout) never runs its EXIT trap: drop a lock whose recorded holder is gone.
  holder=$(cat "$LOCK/pid" 2>/dev/null)
  if [ -n "$holder" ] && ! kill -0 "$holder" 2>/dev/null; then
    echo "build_locked: holder pid $holder of $LOCK is gone - removing the stale lock" >&2
    rm -f "$LOCK/pid"; rmdir "$LOCK" 2>/dev/null; continue
  fi
  if [ "$WARNED" = 0 ] && [ -n "$(find "$LOCK" -maxdepth 0 -mmin +"$LOCK_WARN_MIN" 2>/dev/null)" ]; then
    echo "build_locked: $LOCK is older than $LOCK_WARN_MIN min - if no cl.exe/link.exe has CPU the holder hung: kill it and rmdir the lock" >&2; WARNED=1
  fi
  sleep 15
done
HELD=1; echo $$ > "$LOCK/pid"
if [ "$HOST" = windows ]; then
  timeout "$TIMEOUT_S" "$@"; rc=$?
  [ $rc = 124 ] && taskkill //F //IM cl.exe //IM link.exe >/dev/null 2>&1
elif [ "$HOST" = mac ]; then
  taskpolicy -b nice -n 15 perl -e 'alarm shift; exec @ARGV' "$TIMEOUT_S" "$@"; rc=$?
  [ $rc = 142 ] && echo "build_locked: timeout - check 'ps' for leftover wine-msvc.sh / cl.exe processes before the next build" >&2
else
  perl -e 'alarm shift; exec @ARGV' "$TIMEOUT_S" "$@"; rc=$?
fi
exit $rc
