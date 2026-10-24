#!/bin/bash
# stopper.sh <exe-name-pattern> <start_s> <hold_s> <period_s>
sleep 3
for i in $(seq 1 40); do pid=$(pgrep -x "$1" | head -1); [ -n "$pid" ] && break; sleep 0.5; done
sleep $2
while kill -0 $pid 2>/dev/null; do kill -STOP $pid; sleep $3; kill -CONT $pid; sleep $4; done
