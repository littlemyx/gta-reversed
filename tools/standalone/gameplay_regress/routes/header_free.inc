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
until:invehicle:6000
# (REGRESS-SETUP: once in a full run (audio) the first Return did not mount the bike within 10 s and the dismount press below then mounted it: one retry press if CJ is still on foot after 6 s)
skipif:invehicle:hf_on
key:return
until:invehicle:8000
label:hf_on
# (CHEATS-ARMOUR: the opening mission ends when the script polls and sees CJ in the bike; under load (cdb, CPU burners, 5 of 11 cdb runs) the dismount below came first and the mission stayed on (SETUP mission=1): gate on the mission being over, then the usual settle time)
until:nomission:20000
wait:1500
# (REGRESS-SETUP: the dismount is gated, not timed - once in a full run the Return was not taken while the tutorial bike was still moving, CJ was still on it at free_ready (SETUP combat.free_start invehicle=1);
# same fix as routes/missions.txt (REGRESS-FLAKES): wait for onfoot, one retry press if he is still on the bike after 4 s)
skipif:onfoot:hf_off
key:return
until:onfoot:4000
skipif:onfoot:hf_off
key:return
until:onfoot:4000
label:hf_off
wait:3000
cmd:snap:free_ready
