# --- shared header (no Present: NOTSA_STANDALONE_NOPRESENT=1): boot to the main menu, NEW GAME, wait for control. Everything after this is relative to the moment CJ can move (opening mission, alley at 2241,-1261). ---
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
