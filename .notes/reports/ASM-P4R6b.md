# ASM-P4R6b
Review 44d18755a (CTaskSimpleInAir::MakeAbortable): CONFIRMED. 0x85A6E8 = 625.0f; CTaskComplexKillCriminal::MakeAbortable row settled MATCH.
Compared 6 (5 + 1 UNSURE), 3 fixes: LeaveCar::CreateNextSubTask e235a8799 (in-air JUMP_OUT must not store m_nDieAnimID; S06b misread 0x641A9E), GoToCarDoorAndStandStill::ControlSubTask 363eb1059 (locals, door starts 0, seat path), UseGun::RequirePistolWhip 5c3e67b4e (sqrt > 1).
Build: changed .cpp + StandaloneRelease link OK. Not run: tests/regress (left to gate). Zone open rows (>=0x30): ~1080 (1086 before).
