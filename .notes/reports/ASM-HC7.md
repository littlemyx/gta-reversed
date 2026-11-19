# ASM-HC7: second pass over hot MATCH rows (Entity/Tasks/Events/Collision/Animation/Scripts)
Zone: 737 MATCH rows with exe address in the zones (355 without a re-review line). Compared: 19 functions (highest call count, float / rand heavy first). RECONFIRMED 18, FIXED 1, NOT_REACHED: rest (token budget).
Fixed: CAutomobile::PlayCarHorn (exe 0x6A3770): signed char % 8 vs exe `and eax,7`. The earlier S16 MATCH ("one rand, timer & 7 branches") was wrong.
Stale: S26 UNSURE on CPhysical::CheckCollision sector index no longer applies (GetSectorX already x*0.02f+60 floor/ftol).
Not compared (from the NEW list, by calls): CPhysical::RemoveAndAdd, CPed::UpdatePosition, S48 scanners, SphereCastVsCaches, CObject::*, CPhysical::ApplyCollision.
