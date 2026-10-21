#!/bin/bash
# Agent watchdog: exits (and so wakes the orchestrator) on the first anomaly.
REPO=/c/Users/littl/github/gta-reversed
LOCKMAX=${LOCKMAX:-2700}   # 45 min
STALLMAX=${STALLMAX:-2400} # 40 min
orphan=0
cd "$REPO" || exit 2
last_head=$(git rev-parse HEAD); last_change=$(date +%s)
while :; do
  now=$(date +%s)
  for l in gpu build; do
    d=/c/gta-scratch/$l.lock
    if [ -d "$d" ]; then
      age=$(( now - $(stat -c %Y "$d") ))
      if [ $age -gt $LOCKMAX ]; then echo "ANOMALY $l.lock held ${age}s owner=$(cat $d/owner 2>/dev/null)"; exit 1; fi
    fi
  done
  if tasklist 2>/dev/null | grep -qi gta_reversed && [ ! -d /c/gta-scratch/gpu.lock ]; then
    orphan=$((orphan+1)); [ $orphan -ge 3 ] && { echo "ANOMALY gta_reversed.exe running without gpu.lock for 3 checks"; exit 1; }
  else orphan=0; fi
  head=$(git rev-parse HEAD)
  newest=$(git status --short 2>/dev/null | awk '{print $2}' | xargs -I{} stat -c %Y {} 2>/dev/null | sort -n | tail -1)
  [ "$head" != "$last_head" ] && { last_head=$head; last_change=$now; }
  [ -n "$newest" ] && [ "$newest" -gt "$last_change" ] && last_change=$newest
  for l in gpu build; do [ -d /c/gta-scratch/$l.lock ] && { m=$(stat -c %Y /c/gta-scratch/$l.lock); [ $m -gt $last_change ] && last_change=$m; }; done
  # A running gate writes per-test logs into C:/gta-scratch/run_all_tests.<pid>/: that is activity too.
  g=$(ls -td /c/gta-scratch/run_all_tests.* 2>/dev/null | head -1)
  [ -n "$g" ] && { m=$(stat -c %Y "$g"/* 2>/dev/null | sort -n | tail -1); [ -n "$m" ] && [ "$m" -gt "$last_change" ] && last_change=$m; }
  # Agents work in worktrees under C:/gta-wt: their source edits, commits and ninja logs count as activity too.
  for w in /c/gta-wt/*/; do
    [ -d "$w" ] || continue
    for f in "$w.git" "$w"build/*/.ninja_log; do [ -e "$f" ] && { m=$(stat -c %Y "$f"); [ $m -gt $last_change ] && last_change=$m; }; done
    m=$(git -C "$w" status --short 2>/dev/null | awk '{print $2}' | (cd "$w" && xargs -I{} stat -c %Y {} 2>/dev/null) | sort -n | tail -1)
    [ -n "$m" ] && [ "$m" -gt "$last_change" ] && last_change=$m
  done
  if [ $(( now - last_change )) -gt $STALLMAX ]; then echo "ANOMALY no commit/edit/lock activity for $(( now - last_change ))s (HEAD $head)"; exit 1; fi
  sleep 60
done
