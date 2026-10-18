# --- shared header for scenarios that need CJ free of the opening mission: header.inc + mount the tutorial BMX with Return (ends the opening mission: flag 1 -> 0; otherwise a move to Grove Street starts the next story mission (Big Smoke cutscene)) and dismount again ---
# Window 960x600 assumed (mouse coordinates). Scenarios that look at pixels use header_px.inc (Present on: the intro cutscene is frame-paced and has to be skipped).
until:menu:240000
wait:1500
mark:menu
mouse:472,248
wait:500
key:return
wait:3000
mouse:236,214
wait:500
key:return
wait:3500
key:return
mark:newgame_loading
wait:6000
until:control:150000
wait:3000
mark:control
wait:1000
key:return
until:invehicle:10000
wait:1500
key:return
wait:3000
cmd:snap:free_ready
