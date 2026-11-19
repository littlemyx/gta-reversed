# ASM-P4R7k: never-audited pop/traffic/path-area functions

Regenerated with tools/standalone/asm/noverdict.py (min-size 0x30), filtered to the brief's classes: 33 candidates
(CModelInfo, CDecisionMakerTypes*, CCarAI::FindSwitchDistanceFar). No CPopulation/CPopCycle/CCarCtrl/CAutoPilot/CPathFind/
CPedType/CPedStats/CLoadedCarGroup/CCarGenerator function without a verdict remains at size >= 0x30.

Result: 1 FIXED, ~28 MATCH, 3 skipped (already carry exe-address comments).
- FIXED CModelInfo::ReInit2dEffects (aa886f9cd): exe tests the model pointer for null before Init2dEffects; port dereferenced it
  (20000 slots, most null).
- All CModelInfo::Is*Model compare type==6 and the exe vehicle-type constant (0..11); Add*Model = store count++ + virtual Init + SetModelInfo.
- DecisionMakerTypes Remove/Flush/AddEventResponse/HasAnyEventResponse structurally identical (strides 0x29 x 0x3c, defaults at 10..19).
