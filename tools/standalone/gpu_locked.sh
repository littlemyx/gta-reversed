#!/bin/bash
# Run ONE command (a game run, a device test, a soak) under the shared GPU mutex C:/gta-scratch/gpu.lock, with dead-holder detection.
# The script form of the hand-written snippet "until mkdir gpu.lock ...; echo OWNER > gpu.lock/owner; <run>; rm -rf gpu.lock" (it uses the SAME lock dir and the
# same `owner` file, so it interlocks with play.cmd, the PT2-*/fps_run.sh snippets and agents that still use the hand-written form), mirroring build_locked.sh.
# usage: tools/standalone/gpu_locked.sh [-o OWNER] [-t SECONDS] <command> [args...]      e.g.  tools/standalone/gpu_locked.sh -o "PT2-AA boot" tools/standalone/soak.sh build/StandaloneRelease 7
#        tools/standalone/gpu_locked.sh --status        print who holds the lock and whether it looks alive (exit 0 free, 1 held alive, 3 held by USER-PLAY, 4 stale)
#        tools/standalone/gpu_locked.sh --reap          drop the lock if (and only if) it is stale/dead, never USER-PLAY; exit 0 if it is free afterwards
#        tools/standalone/gpu_locked.sh --acquire "<owner>" $$   /   --release $$     for a long-lived script that takes the lock around several runs (holder = that script's pid; call --release from its EXIT trap)
# env:   SCRATCH            dir that holds gpu.lock (default: Windows C:/gta-scratch, same as build_locked.sh)
#        GPU_OWNER          owner text (default "<script name of the caller> pid <pid>"; -o wins)
#        TIMEOUT_S          hard timeout of the held command in seconds (default 3600; 124 = timeout). Hold the lock for the run itself only (rules: <= 20 min of game time).
#        GPU_LOCK_GRACE_S   a lock dir with neither pid nor owner younger than this is "being created" (default 60); older = ownerless leftover -> removed
#        GPU_LEGACY_STALE_MIN  a hand-written lock (owner file, no pid) older than this AND with no gta_reversed/gta_sa/*_test process running is dropped (default 30)
#        GPU_LOCK_POLL_S    wait poll interval (default 5)
# Lock contents: pid (bash pid of the holder = this script), winpid, owner ("<OWNER> <date>"). A waiter drops a lock whose recorded pid is gone (the holder was killed hard,
# e.g. an agent's tool timeout, so its EXIT trap never ran) - the 40-minute ownerless gpu.lock of ORACLE-W2 can not recur. An owner starting with USER-PLAY (play.cmd: the user is
# playing) is NEVER removed automatically, whatever its age. Re-entrant: a command run under the lock sees GPU_LOCK_HELD=<pid>; a nested gpu_locked.sh of the same holder just runs.
# PRIORITY RESERVATION (GPULOCK-PRIO): a long-lived orchestrator script (windows/orch_gate.sh) calls `gpu_locked.sh --prio-reserve "<owner>" $$` at its start, then `export GPU_PRIO_OWNER=$$`,
# and `gpu_locked.sh --prio-release $$` from its EXIT trap. It writes $SCRATCH/gpu.prio (key=value lines: pid, winpid, epoch, owner). While a LIVE reservation exists, every OTHER waiter in
# wait_lock (gameplay_regress.sh --acquire, soak.sh, run_all_tests.sh, a plain `gpu_locked.sh cmd`) does NOT take the lock: a lock that is currently held is never preempted - the holder
# finishes its run, then the reserver gets the next turn instead of sweep agents queueing back to back. A process whose env GPU_PRIO_OWNER equals the reserved pid (the reserver's children
# inherit it) acquires normally. A reservation is stale and silently removed when its pid is dead or it is older than GPU_PRIO_MAX_S. Only one reservation at a time: --prio-reserve while
# a live foreign one exists fails (exit 1) and the caller just runs without priority. USER-PLAY keeps absolute precedence: play.cmd uses its own hand-written lock and ignores gpu.prio,
# a USER-PLAY holder is still waited for (never removed) by the reserver too. `--status` also prints the reservation.
#        GPU_PRIO_OWNER     pid of the reserving script (set by it, inherited by its children)
#        GPU_PRIO_MAX_S     reservation older than this many seconds is ignored/removed (default 21600 = 6 h)
# Exit code: that of the command; 124 = timeout; 2 = usage.
set -u
. "$(dirname "$0")/hostenv.sh"
SCRATCH="$(hp "${SCRATCH:-$DEF_SCRATCH}")"; mkdir -p "$SCRATCH"
LOCK="$SCRATCH/gpu.lock"
GRACE="${GPU_LOCK_GRACE_S:-60}"; LEGACY_MIN="${GPU_LEGACY_STALE_MIN:-30}"; POLL="${GPU_LOCK_POLL_S:-5}"

alive_pid() {   # $1 = msys pid, $2 = windows pid (may be empty)
  [ -n "$1" ] && kill -0 "$1" 2>/dev/null && return 0
  [ -n "${2:-}" ] && [ "$HOST" = windows ] && tasklist //FI "PID eq $2" //NH 2>/dev/null | grep -q "[[:space:]]$2[[:space:]]" && return 0
  return 1
}
game_running() { [ "$HOST" = windows ] && tasklist //FO CSV //NH 2>/dev/null | grep -qiE '^"(gta_reversed|gta_sa|gta_sa_compact|[^"]*_test)\.exe"'; }
# verdict of the current lock dir: FREE | ALIVE <info> | USERPLAY | STALE <why> | FORMING
lock_state() {
  [ -d "$LOCK" ] || { echo FREE; return; }
  local pid winpid owner
  pid=$(cat "$LOCK/pid" 2>/dev/null); winpid=$(cat "$LOCK/winpid" 2>/dev/null); owner=$(cat "$LOCK/owner" 2>/dev/null)
  case "$owner" in USER-PLAY*) echo "USERPLAY $owner"; return;; esac
  if [ -n "$pid" ]; then
    if alive_pid "$pid" "$winpid"; then echo "ALIVE pid $pid: $owner"; else echo "STALE holder pid $pid is gone ($owner)"; fi
    return
  fi
  if [ -z "$owner" ]; then   # neither pid nor owner: a holder in the middle of mkdir -> write, or an ownerless leftover
    if [ "$(age_s "$LOCK")" -gt "$GRACE" ]; then echo "STALE ownerless lock dir older than ${GRACE}s"; else echo "FORMING"; fi
    return
  fi
  # hand-written lock (owner file, no pid): no way to see the holder, judge by age and by running game processes
  if [ "$(age_s "$LOCK")" -gt $((LEGACY_MIN * 60)) ] && ! game_running; then
    echo "STALE hand-written lock '$owner' older than ${LEGACY_MIN} min and no game/test process is running"
  else
    echo "ALIVE (hand-written lock, no pid): $owner"
  fi
}
age_s() { local m; m=$(stat -c %Y "$1" 2>/dev/null) && echo $(( $(date +%s) - m )) || echo 0; }
drop_lock() { rm -f "$LOCK/pid" "$LOCK/winpid" "$LOCK/owner" 2>/dev/null; rm -rf "$LOCK" 2>/dev/null; }

PRIO="$SCRATCH/gpu.prio"; PRIO_MAX="${GPU_PRIO_MAX_S:-21600}"
prio_get() { sed -n "s/^$1=//p" "$PRIO" 2>/dev/null | head -1; }
# returns 0 and prints "pid owner" if a live reservation by ANOTHER pid blocks this process; 1 if none / own (GPU_PRIO_OWNER) / stale (stale file is removed)
prio_blocked() {
  [ -f "$PRIO" ] || return 1
  local rp rw re ro
  rp=$(prio_get pid); rw=$(prio_get winpid); re=$(prio_get epoch); ro=$(prio_get owner)
  [ -n "$rp" ] || { [ "$(age_s "$PRIO")" -gt "$GRACE" ] && rm -f "$PRIO"; return 1; }   # empty/garbled file: being written, or a leftover
  [ "${GPU_PRIO_OWNER:-}" = "$rp" ] && return 1                                          # the reserver itself / its children
  if ! alive_pid "$rp" "$rw" || [ $(( $(date +%s) - ${re:-0} )) -gt "$PRIO_MAX" ]; then
    [ "$(prio_get pid)" = "$rp" ] && { echo "gpu_locked: removing stale reservation $PRIO (pid $rp, $ro)" >&2; rm -f "$PRIO"; }
    return 1
  fi
  echo "$rp $ro"; return 0
}

HELD=0; WAITSHOWN=0
wait_lock() {   # loops until `mkdir` of the lock dir succeeds (and no other pid holds a priority reservation); drops stale locks on the way
  local p1 st st2 pb pshown=""
  while :; do
    pb=$(prio_blocked) || pb=""
    [ -z "$pb" ] && mkdir "$LOCK" 2>/dev/null && break
    [ -n "$pb" ] && [ "$pshown" != "$pb" ] && { echo "gpu_locked: $LOCK is reserved for pid ${pb} - waiting behind it" >&2; pshown="$pb"; }
    p1=$(cat "$LOCK/pid" 2>/dev/null); st=$(lock_state)
    case "$st" in
      FREE*) if [ -n "$pb" ]; then sleep "$POLL"; else sleep 0.2; fi; continue;;   # released between mkdir and the check (reserved: wait for the reserver)
      STALE*) st2=$(lock_state)   # re-check right before dropping: another waiter may have dropped it and a new holder taken it meanwhile
              case "$st2" in STALE*) if [ "$(cat "$LOCK/pid" 2>/dev/null)" = "$p1" ]; then echo "gpu_locked: removing stale $LOCK - ${st#STALE }" >&2; drop_lock; continue; fi;; esac;;
      USERPLAY*) [ "$WAITSHOWN" != USERPLAY ] && { echo "gpu_locked: the user is playing ($st) - waiting; do not start builds or heavy work meanwhile" >&2; WAITSHOWN=USERPLAY; };;
      *) [ "$WAITSHOWN" != "$st" ] && { echo "gpu_locked: waiting for $LOCK: ${st#* }" >&2; WAITSHOWN="$st"; };;
    esac
    sleep "$POLL"
  done
}
write_holder() {   # $1 = pid of the holder (liveness is judged by it), $2 = owner text
  local w=""; [ -r "/proc/$1/winpid" ] && w=$(cat "/proc/$1/winpid")
  echo "$1" > "$LOCK/pid"; [ -n "$w" ] && echo "$w" > "$LOCK/winpid"
  echo "$2 $(date)" > "$LOCK/owner"
}

case "${1:-}" in
  --prio-reserve) [ $# -ge 3 ] || { sed -n '2,33p' "$0"; exit 2; }
    if pb=$(prio_blocked) && [ "$(prio_get pid)" != "$3" ]; then echo "gpu_locked: a live reservation exists ($pb) - running without priority" >&2; exit 1; fi
    w=""; [ -r "/proc/$3/winpid" ] && w=$(cat "/proc/$3/winpid")
    ( set -o noclobber; printf 'pid=%s\nwinpid=%s\nepoch=%s\nowner=%s %s\n' "$3" "$w" "$(date +%s)" "$2" "$(date)" > "$PRIO" ) 2>/dev/null || exit 1
    exit 0;;
  --prio-release) [ $# -ge 2 ] || exit 2; [ "$(prio_get pid)" = "$2" ] && rm -f "$PRIO"; exit 0;;
  --status) st=$(lock_state); echo "gpu.lock: $st"; [ -f "$PRIO" ] && echo "gpu.prio: $(tr '\n' ' ' < "$PRIO")"; case "$st" in FREE*) exit 0;; USERPLAY*) exit 3;; STALE*) exit 4;; *) exit 1;; esac;;
  --reap)   st=$(lock_state); case "$st" in STALE*) echo "gpu_locked: removing $LOCK - ${st#STALE }" >&2; drop_lock;; esac; [ ! -d "$LOCK" ]; exit $?;;
  # for a long-lived script that takes the lock around SEVERAL runs: `gpu_locked.sh --acquire "<owner>" $$` waits, then records that script's pid as the holder (so a killed script is
  # detected as a dead holder); `gpu_locked.sh --release $$` drops the lock if that pid still holds it (call it from the script's EXIT trap too)
  --acquire) [ $# -ge 3 ] || { sed -n '2,33p' "$0"; exit 2; }; wait_lock; write_holder "$3" "$2"; exit 0;;
  --release) [ $# -ge 2 ] || exit 2; [ "$(cat "$LOCK/pid" 2>/dev/null)" = "$2" ] && drop_lock; exit 0;;
esac

OWNER="${GPU_OWNER:-}"; TIMEOUT_S="${TIMEOUT_S:-3600}"
while getopts "o:t:" opt; do case "$opt" in o) OWNER="$OPTARG";; t) TIMEOUT_S="$OPTARG";; *) exit 2;; esac; done
shift $((OPTIND - 1))
[ $# -ge 1 ] || { sed -n '2,33p' "$0"; exit 2; }
[ -n "$OWNER" ] || OWNER="gpu_locked caller pid $PPID"

# re-entrant: the outer gpu_locked.sh of this very lock is alive -> just run
if [ -n "${GPU_LOCK_HELD:-}" ] && [ "$(cat "$LOCK/pid" 2>/dev/null)" = "$GPU_LOCK_HELD" ] && kill -0 "$GPU_LOCK_HELD" 2>/dev/null; then exec "$@"; fi

release() { if [ "$HELD" = 1 ]; then [ "$(cat "$LOCK/pid" 2>/dev/null)" = "$$" ] && drop_lock; fi; HELD=0; }
trap release EXIT; trap 'exit 130' INT TERM
wait_lock
HELD=1
write_holder "$$" "$OWNER"
export GPU_LOCK_HELD=$$
timeout "$TIMEOUT_S" "$@"; rc=$?
[ $rc = 124 ] && echo "gpu_locked: timeout ${TIMEOUT_S}s - the command was killed (its own children may need a kill: check tasklist for gta_reversed.exe)" >&2
exit $rc
