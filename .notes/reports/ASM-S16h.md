# ASM-S16h (Automobile.cpp, last 3 functions) - 2026-10-28
Result: 2 FIXED (2 commits), 1 PARTIAL (PreRender: 0x6AAB50-0x6AB154 MATCH, the rest NOT REACHED, token budget); 0 UNSURE. Table: S16h.result.tsv.
Fixes:
- ProcessBuoyancy (0x6A8C00-0x6A9678), 6a5101db: float constant multiplies as in the exe: force.z/((ts*mass)*0.008f), (U/buoy)*f*1.05f, 1-f*0.05f (port divided by 125 / 20), mass*0.0064f and buoy-mass*8.0000009e-6f (port 8/1250, 0.01/1250), pow result rounded once.
- VehicleDamage (0x6A7650-0x6A84B5), d812c1f9:
  - upside down: remote controlled OR submerged returns, otherwise health loss unless wrecked (port: only a remote+submerged return);
  - damageIntensity != 0 branch keeps min 25 and collForceMult 0.333 (port set 1.0); the inside-view vehicle damage is *0.333 (port /3);
  - front bumper: left wing needs localDir < -0.7 / < -0.5 (port had > signs, so nearly always left wing); bonnet part (and its rand() & 3, port % 3) only when the bumper is not ok; component order bonnet, LF, RF, windscreen, rear: boot, LR, RR (statuses read before the damage);
  - ReactToVehicleDamage(event owner's driver) after the collision event (port omitted it, it uses RNG); crash Say probability 0.66f and the player/mission test (port: other != player veh || other mission, exe: this != player veh || !other mission);
  - law enforcer compares magnitudes, burn: stores the damager in m_pExplosionVictim (+0x928, the BlowUpCar credit) not m_pLastDamageEntity, prevHealth test is int16 signed, dot sum orders z,y,x / y,z,x, 0.4f not 0.4.
Consumer audit: no signature or layout change; m_pExplosionVictim readers (BlowUpCar burn paths in Automobile.cpp, Vehicle.cpp) now see the damager as in the exe.
Gate: Release build OK (0 errors); no oracle target covers Automobile.cpp; soak (NOVSYNC, 3 min) RESULT: OK. Scratch: C:/gta-scratch/asm-s16h (asm dumps, pr.txt, edit scripts, build and soak logs).
