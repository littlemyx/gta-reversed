# ASM-S01 (40 Tasks functions: Interior*, SeekEntity*, ArrestPed, AttractorPartnerWait) - 2026-10-28
Result: 22 compared (11 FIXED in 4 commits, 11 MATCH, 0 UNSURE), 18 NOT REACHED (token budget; table in S01.result.tsv).
Fixes (exe address, one line each):
- 78ff50da BeInHouse::GetInfoForPedToUse (0x6750A0-0x67521D) draws the UNK_5 duration rand() even when no UNK_5 info is found; CreateNextSubTask (0x676200-0x67629E) was CreateFirstSubTask: the exe sets ped flag 0x474|=0x80 and passes bDoInstantly=false.
- 62064b17 BeInOffice::GetInfoForPedToUse (0x675300-0x6753D4) duration rolled per loop iteration, int rand*100<=20 test (port used the float RandomBool); CreateNextSubTask (0x676350-0x6763F4) sets the 0x80 ped flag and bDoInstantly=false (port ignored both).
- 2ca13c71 EntitySeekPosCalculatorRadiusAngleOffset (0x6946F0-0x6947C4): cos is stored to float before the radius multiply (sin stays extended).
- 2e8f5529 SeekEntityAnyMeans(XYOffset) (0x696030-0x696250): CreateFirstSubTask starts the scan timer (port never did, so ControlSubTask never re-aimed the drive); ControlSubTask restart unguarded; Clone registers the entity reference like ctor 0x695600.
MATCH: BeInHouse/BeInOffice CreateFirstSubTask, BeInShop::ScanForStuff, LieInBed::ProcessPed, ShopKeeper::CreateFirstSubTask, SitAtDesk x3, SitOnChair::ProcessPed, UseInfo MakeAbortable + CreateNextSubTask, SeekPosCalculator FixedPos + XYOffset, AttractorPartnerWait::CreateFirstSubTask.
SYSTEMIC_TODO timers: SitAtDesk 0x677A8F/0x677ADC and SitOnChair 0x676E47/0x676EC5 have the `>=0` test (Start is right); SitAtDesk anim timer 0x677780 is unguarded (port already is). SeekEntity Standard/RadiusAngleOffset timers NOT checked. LieInBed Clone reads an uninitialised byte in the exe (port: false without FIX_BUGS), not changed.
NOT REACHED: SeekEntityRadiusAngleOffset x8, SeekEntityStandard x8, ArrestPed x2.
Consumers: SeekEntityAnyMeans changes are local to the XYOffset instantiation (only user); BeInHouse/BeInOffice NextSubTask now match the exe's sit-after-first behaviour.
Gate: Release build OK; soak (3 min NOVSYNC, C:/gta-scratch/asm-s01/soak) RESULT: OK; wave2_oracle_test does NOT compile on this tree (tests/standalone/wave2_oracle_gen_func.inc(852) eBoneTag32 vs eBoneTag16 in CTaskSimpleIKChain, unrelated to these files; not run).
