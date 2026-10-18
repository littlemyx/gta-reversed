# --- shared header with Present on (pixel scenarios): boot to the main menu, NEW GAME, skip the frame-paced intro cutscene (ESC), wait for control ---
# Window 960x600 assumed (mouse coordinates).
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
wait:68000
mark:cutscene_skip
key:escape
wait:3000
key:escape
wait:3000
key:return
wait:12000
until:control:60000
wait:500
mark:control
