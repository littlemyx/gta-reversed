# ASM-HC1b: continuation of HC1, zone source/game_sa/Entity/**

Zone rows left (exe_addr != "-", verdict NONE after excluding every address in .notes/asm_sweep/*result*.tsv): 0.
Re-checked ryder1.tsv (558 unverified rows overall) by .obj name against all Entity/** .cpp files and by class name
(CEntity/CPhysical/CVehicle*/CPed*/CObject/...): no row in the zone. Nearby rows are outside it (PedGeometryAnalyser, PedGroups*,
PedScriptedTaskRecord, IKChain*, Radar, VisibilityPlugins) and belong to other slices. Nothing compared, no source changed.
