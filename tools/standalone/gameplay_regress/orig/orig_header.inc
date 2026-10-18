# original exe (inputdrv.exe): boot to the menu, NEW GAME, skip the intro, wait for control (800x600 exclusive: mouse coordinates are desktop pixels)
cap:60
pressuntil:return:7
until:menu:240000
wait:3000
mark:menu
mouse:400,242
key:return
wait:2500
key:return
wait:3000
key:return
wait:6000
until:gstate=9:400000
wait:5000
key:escape
wait:3000
key:escape
wait:3000
skipintro:120000
wait:1000
# mount + dismount the tutorial BMX: ends the opening mission (same as routes/header_free.inc); orig_gr_poll.py (started by orig_run.sh) then teleports CJ to Idlewood (2100,-1790) 2 s after he stepped off (the original has no injector)
slow:return
until:invehicle:10000
wait:1500
slow:return
wait:6000
