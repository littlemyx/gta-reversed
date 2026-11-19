# ASM-P4R22b
Review 920f09f45 (CAutomobile::GetCarPitch): CONFIRMED against exe 0x6A6050.
Compared 6 functions (5 wind/blade/cop + review), all MATCH, 0 fixes. Zone narrowed mid-run (vehicle classes to P4R24): CBmx::LaunchBunnyHopCB handed over.
NOT_REACHED: CEntity::IsEntityOccluded, CPlayerPed::DrawTriangleForMouseRecruitPed (budget).
No source changed, no build needed. See P4R22b.result.tsv.
