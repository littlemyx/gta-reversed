# ASM-P4R7c

Review of cc5babbe9: CONFIRMED (exe 0x54CB5B: a boat jumps to 0x54CC5D, the train/wrecked/abandoned block at 0x54CBB2 is the non-boat path).

CPhysical::ProcessCollisionSectorList: rest of the body compared (0x54BD5D-0x54CF8D). One fix (59823aaa9): in the both-contacted branch a soft collision with piece type B 13..16 also records the damaged piece on the other entity (0x54BE21). Everything else matches.

CVehicle::ProcessWheel: whole function now compared, MATCH. CCarCtrl::WeaveForOtherCar: MATCH (callee 0x421A50 not reached).

Zone: 1112 rows (1095 not done before); 3 functions compared, 1 fix. Build: StandaloneRelease links (Physical.cpp). No game run / tests (brief forbids), regress left to the gate.
