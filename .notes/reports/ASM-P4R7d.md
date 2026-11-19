# ASM-P4R7d
Review 91fba7fb6: CONFIRMED (exe 0x54BE21-0x54BE3A matches the added call).
Zone: Entity/** + gameplay .cpp in game_sa/: 969 functions with a port body found, 657 without an exe-address comment (C:/gta-scratch/p4r7d/never.tsv; 298 after dropping bodies that mention exe/x87/AUDIT).
Compared 6 (see P4R7d.result.tsv); 1 fixed: CWeapon::DoBulletImpact (7cbe37ee1).
Not reached: the rest of never.tsv (Wanted, Automobile::AddMovingCollisionSpeed, World Find*OfType, Entity UpdateRwMatrix etc.).
