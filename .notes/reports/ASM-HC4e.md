# ASM-HC4e
Zone rows (verdict NONE, mine): ~45 in ryder1.tsv; most already had MATCH rows (stale tsv). Compared here: 14 rows. Skipped-render: CStreaming::RenderEntity, CPlantMgr::Render, CVisibilityPlugins::GetModelInfo, DrawScriptSprites.
Reviews: 4a6dd6ed8 and 2e53966f0 CONFIRMED against the exe.
Full walk of SphereCastVersusVsPoly (1.7M calls): MATCH.
Fixes: 387be1240 AddToListOfSpecialAnimGroupsAttachedToCharModels (last free slot), 2bdffac12 IsPedStopped (NaN speed in vehicle).
Rows: .notes/asm_sweep/HC4e.result.tsv. Not reached: CRenderer::ShouldModelBeStreamed (root, HC3).
