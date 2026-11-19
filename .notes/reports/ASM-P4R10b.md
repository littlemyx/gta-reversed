# ASM-P4R10b: signed-time sweep continued + int-vs-float
Review: f7a5f8581 CTaskComplexKillPedOnFootStealth::ControlSubTask (0x62B999 sub/cmp 0x1388/jle) CONFIRMED signed, port int32 cast correct.
Scan (C:/gta-scratch/p4r10b_all.sh + p4r10_sc2.py): every Install'ed function (~1040) in Entity/*, Entity/Vehicle/* (rest after ProcessBoatControl), Population, Garage(s), PedGroup*, Pickup(s), Gang*, Wanted, PlayerInfo, Stats, Darkel, Restart, CarGenerator, RoadBlocks, ScriptedBrain: timer load followed by a signed jcc. 15 functions hit; all either already signed in port (Darkel::Update, ManagePed, Heli::ProcessControl) or are `fild`+jge uint->float fixups / non-time compares. No new unsigned-vs-signed time bug.
Int-vs-float: spot-checked Stats (Get/Set/Inc/Dec/SetNewRecord), Wanted, PlayerInfo (AddHealth, stunts, taxi, remote explosion), Automobile bus doors / misc angle: match. One fix: CStats::GetStatValue (e5a0fa8f2) int stats addressed unchecked like the exe (ids 82..119).
NOT_REACHED: systematic fild/fistp diff of Ped.cpp / Vehicle.cpp bodies (no automated tool; only grep of static_cast<float> sites).
Build: StandaloneRelease links (ninja -j4).
