# RD-AB-8 - frame 42 (harness, not a port bug) + SetupSuspensionLines of the other vehicle classes

Agent RD-AB-8, rules v5, 2026-10-25 19:52 - ~20:45 WEST. Worktree C:/gta-wt/rdab8 (branch rdab8 on edcf5f53). Data C:/gta-scratch/rd-ab-8 (base, a1, a2, r6x, soak). Own proxy RD_PX=C:/gta-scratch/rd-ab-8/px.
Orig side: RD-AB-4 traces o1..o3 (orig-orig 1500/1500). Documents user-files signature identical (ee0a22fe...) before/after every run.

## Equal prefixes (orig-port, pin 1023, intro, 3 reps x 1500, port-only runs)
before (edcf5f53): 42 / 42 / 42, port-port 1500/1500 -> after the harness fix (audio RNG partition by return address, audio_rva.py): 73 / 73 / 73, port-port 1500/1500.

## Divergences (0-based traced frames)
| frame | layer | cause | fix |
|---|---|---|---|
| 42 | rng 81 (orig) vs 129 (port) | HARNESS, not CCarCtrl::ChooseModel: the port took the same branches as the exe. The first footstep sound of the run (CAEPedAudioEntity::HandleFootstepEvent, exe 0x4D9C17/0x4D9C56, 3 rand draws) happens in frame 42. The original's partition (pin 128) sends them to the audio stream; the port's partition only recognised rand() callers by NOTSA_LOCKSTEP_AUDIO_RVA, which NO script set (rd_port_run.sh), so the 2 port footstep draws (AEPedAudioEntity.cpp:451, AESound::NewVPSLEntry) came out of the gameplay stream, the first ChooseModel drew 0.11 instead of 0.49 (-> gang branch, 50 idx draws). Found with rcmp --seq + resolve_stack.py (+ a temporary Note of rnd/total/gangsControl in ChooseModel: all branch inputs were identical). | rd_port_run.sh derives NOTSA_LOCKSTEP_AUDIO_RVA from <exe>.map (audio_rva.py); audio_rva.py: inline comdats (e.g. CGeneral::GetRandomNumberInRange<int> at rva 0xb3274, which sat behind an audio object) END a range and are not audio (first attempt without that: prefix 4, because the general rand helper was classed as audio). Needs the .map next to the exe (build dir); a copied exe without map prints a WARNING. |
| 73 | rng 133 vs 42 calls | NOT analysed (budget). Lead below. | - |

## Lead for the next agent: gang ped type (visible in the D line, not in the F line)
D `pcnt` (ms_nNumGang[], exe 0xC0EC3C..) differs from traced frame 14 on: first gang ped is gang index 1 (Grove, 0xC0EC40) in the exe and gang index 8 (0xC0EC5C) in the port, rand counts equal. Candidates: CPopCycle::PickGangToCreateMembersOf (0x60F8D0; port PopCycle.cpp:530 looks like the asm), CPopulation::ChooseGangOccupation / the ped type read from the chosen model's CPedModelInfo (ped.dat type string -> id table), CPopulation::AddPed counting by type. Also frame 0 D line: CPopCycle::m_nCurrentZoneType is -1 in the exe, 0 in the port (zt=), harmless until the first PopCycle update, noted only.

## SetupSuspensionLines audit (sub-agent diff against the exe, exe bytes checked; Trailer re-checked by me)
| class | exe | verdict |
|---|---|---|
| CAutomobile | 0x6A65D0 | fixed in RD-AB-7 |
| CHeli, CPlane | slot 48 = 0x6A65D0 | inherit CAutomobile's: covered by the RD-AB-7 fix |
| CBike (+ctor marker 0x6BF73C) | 0x6B89B0 | OK (bbox min.z conditional store and radius max-if-greater are the exe's own shape) |
| CBmx | slot 48 = 0x6B89B0 | CBike's, OK |
| CQuadBike | 0x6CDCA0 | OK (field_9A8=1.0f then CAutomobile's) |
| CMonsterTruck | 0x6C7FB0 | OK (disks, bbox min.z = H - radius if smaller, radius if greater) |
| CBoat, CTrain | 0x6D6380 = bare ret | OK (port has no override) |
| CTrailer | 0x6CF1A0 | 3 deviations FIXED (Trailer.cpp): (1) 0x6CF4BB..0x6CF4DD ratio uses `|l0.y + l1.y| * 0.5` (fabs of the sum), the port used `(|l0.y| + |l1.y|) * 0.5`; (2) 0x6CF4F3..0x6CF505 height = `spring0*(1 - r/F) + (wheel/2 - l0.z)` (parenthesis summed first), the port summed left to right; the chain now runs in x87d like the exe (nothing spilled); (3) 0x6CF560..0x6CF57C the final loop writes m_wheelPosition[0..3] (4 iterations), the port wrote [0..1]. bbox min.z / sphere radius / lines 0-5 geometry already matched. |
Consumer audit (Trailer): m_fFrontHeightAboveRoad/m_fRearHeightAboveRoad (0x898/0x89C) feed the trailer suspension processing (CTrailer::ProcessControl/ProcessSuspension readers) and m_wheelPosition[2..3] used to keep the raw wheel z; no trailer is on the intro route, so no trace effect; no oracle row for SetupSuspensionLines.

## Gate (rules v5)
Release build ok (this tree incl. Trailer fix). Soak: see hand-back. Oracle targets: none touched. Not run: run_all_tests (orchestrator).
