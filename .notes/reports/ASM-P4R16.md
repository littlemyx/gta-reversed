# ASM-P4R16: CGame::Process walk, level 4

Level-4 list: 19 functions (gameplay only, no verdict yet, >= ~0x30 bytes; C:/gta-scratch/p4r16/level4.tsv; built by extending the P4R2n port-source BFS by one level; 44 level-4 nodes before the verdict/size filters).
Review of 1aa7e5dc3 (CPedGroupMembership::Process): CONFIRMED against exe 0x5FBA60 (dz2+dy2+dx2 order, strict sep2 < dist2, NaN never removes, slots 0..6 only).
Compared 19: 18 MATCH, 1 INTENTIONAL (D3DResourceSystem::TidyUpD3DIndexBuffers, render layer), 0 fixes, 0 UNSURE. DoPathSearch and PickGangToCreateMembersOf: structure and all branch/float details checked; the per-gang repeated tail of PickGang was read for gangs 0-4 only (identical pattern).
Only delta seen: CCamera::CamShake rounds the uint32 ms diff to float before *0.001 (exe fild is exact); unobservable (value clamps to 0 after ~4.6 h anyway).
No source changed, no build needed.
