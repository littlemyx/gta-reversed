# ASM-P4R6d
Review of 60de888ca (RequirePistolWhip >= 1.0): CONFIRMED (fcomp/test ah,5/jp; constant 0x8D2E68 = 1.0f).
Compared 9 functions (+1 review): 2 FIXED (865f8d57c IsPedInLeaderFiringLine 2D-normalised dir; 8a71b17ed Wander::UpdateDir node copies), 7 MATCH.
Not reached: KillPedOnFootArmed::ControlSubTask / LineOfSightClearForAttack / Stealth::ControlSubTask (previously reviewed in S05e/S05d/S06 without verdict rows, no fresh pass), the rest of the zone (about 770 rows >= 0x30 without a verdict, many already carry ASM-* comments).
Tip: tools list for the no-ASM-comment rows: C:/gta-scratch/p4r6d/w.py (138 functions with no ASM/exe comment in the body).
