# ORACLE-W3-MISC report

Agent ORACLE-W3-MISC (rules v3), 2026-10-20 17:54 - 18:15 WEST, worktree C:/gta-wt/oracle-w3-misc (branch oracle-w3-misc, base a8b507e2). New target `wave3_misc_oracle_test` (W2 harness: deterministic arena, whole exe data image + a 8 KB fixture block compared after every case), rows in `tests/standalone/wave3_oracle_misc.inc`.
**32 functions, PC24 0 mismatches, run time ~1 min 55 s** (PC53 0 as well). Development switch `W3M_DIV=n` divides the case counts.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 4550 | **4581** |
| Localisation.cpp uncovered (A/B) | 17 (GermanGame was covered) | **0** |
| TagManager.cpp uncovered (A/B + IsTag) | 14 | **SetupAtomic (B), RenderTagForPC (C), Save, Load (S)** |
| port bugs fixed (one commit each) | | **4** |

## Fixtures
Localisation: the three bool globals 0xB9B7EC/ED/EE (0 / 1) and FrontEndMenuManager's language byte 0xBA67CC (0..6 the menu values, 7..255 as a damaged gta_sa.set could give), via the exe addresses on both sides; `Metric` is also checked for the language-byte layout (static_assert offsetof(CMenuManager, m_nPrefsLanguage) == 0xBA67CC - 0xBA6748).
TagManager: the tag array at 0xA9A8C0 (all 150 slots valid, `ms_numTags` 0..13 / 149 / 150, tag entities distinct or with repeats, alphas at the 227/228/229 boundary mixed with random), `ms_numTagged` consistent and inconsistent; 12 raw CEntity blobs (position in the placement or in a CMatrix reached through m_matrix, RpAtomic blob or none, model index 0 = a valid tag model info), the atomics with the three CVisibilityPlugins plugin offsets randomised (0x40..0x7C), the exe's `ms_modelInfoPtrs` entries 0..3 pointing to fake model infos whose vtable slots GetRwModelType / AsAtomicModelInfoPtr are host stubs (IsTag, a class C function, is covered this way), TheCamera wide-screen flag, area rects around the entity positions with entities exactly on the borders and game-range floats mixed with NaN / inf / denormal / huge. `CGarages::TriggerMessage` (0x447B80) is `oracle::Patch`ed to a recording stub; the port's symbol of the same name is defined in the test TU with the same body (compared: key string hash, arguments, call count). Entities not in the tag array are not used for the functions where the exe dereferences address 4 (GetAlpha/SetAlpha of an unknown atomic-less entity).

## Port bugs fixed (rows failed pre-fix; counts = failing PC24 cases of the first full run on the unfixed code, log C:/gta-scratch/w3misc_run1.log + w3misc_run.log)
| function | asm | bug | pre-fix | commit |
|---|---|---|---|---|
| CTagManager::ShutdownForRestart | 0x49CC60 | exe clears Alpha of the first ms_numTags entries; port cleared all 150 | 140/150 | fde4b475 |
| CTagManager::AddTag | 0x49CC90 | exe stores Entity + Alpha byte; port assigned a tTagDesc temporary (3 padding bytes of stack garbage written into the tag array image) | 300/300 | 13d50c4e |
| CTagManager::FindTagDesc (users GetAlpha 0x49CF90, SetAlpha 0x49CEC0) | 0x49CCB0 | exe scans last to first (the LAST entry of a repeated entity wins); port first to last | 37/1500 (+49/1500, 216/2500 in the users) | 68ed92a0 |
| CTagManager::SetAlphaInArea | 0x49CFE0 | exe sets the tag's Alpha for every tag in the area and guards only the atomic's SetUserValue; port skipped the whole tag (alpha and counters) when the entity had no atomic (streamed-out tag) | 1404/3000 | 327f1197 |

Rows that matched without a fix: all 18 Localisation functions, Init, GetTagPos, IsTag, SetAlpha/GetAlpha(RpAtomic), GetPercentageTagged (incl. 0 tags = NaN/inf -> 0x80000000), UpdateNumTagged, GetPercentageTaggedInArea (float and integer percentage agree on every case, also with 0 tags in the area).

## Skips
- CTagManager::SetupAtomic 0x49CE10 (B): reads RpGeometry / RpMaterial through the RenderWare layout; the port is compiled against the librw layout, the exe's fields are at other offsets (read by hand: pipeline store, flags |= 0x40, SetUserValue(0) - equal).
- CTagManager::RenderTagForPC 0x49CE40 (C): calls the atomic's render callback through the RW object.
- CTagManager::Save 0x5D3D60 / Load 0x5D3DA0 (S): CGenericGameStorage work buffer (file IO state); Load stores ms_numTags into the buffer instead of loading (kept, noted in the source).
- CTagManager::GetNearestTag was already covered (script_oracle_g19_test).
- Not reproducible by a fixture: IsTag's `movsx` of the model index (the exe sign-extends m_nModelIndex, the port reads it unsigned) matters only for index >= 0x8000, impossible with 20000 model slots.
