# ASM-S89 (phase 3 tier A): CAnimBlendAssocGroup, CAnimManager, CAEUserRadioTrackManager

46 rows: 28 MATCH, 5 FIXED functions (6 rows), 1 UNSURE, 9 NOT_REACHED (user-track platform file/thread/COM layer, ANPK body).
Details per row: `.notes/asm_sweep/S89.result.tsv`.

| Commit | Function | Fix |
|---|---|---|
| f1808dd9a | CAnimBlendAssocGroup::CreateAssociations(block, animNames, modelNames, stride) 0x4CE3B0 | name-table scan has no break: last duplicate wins; not-found log no longer reads past the table |
| 3b12b6a96 | CAnimManager::BlendAnimation (id 0x4D4610, hier 0x4D4410) | fade-out test is strict `bd < delta` (was `<=`); `blendAmount` NaN takes the `<= 0` arm; clamp is `-0.05 < bd ? -0.05 : bd` (not std::min) |
| 2f4a8e283 | CAnimManager::LoadAnimFile ANP2/ANP3 0x4D5172 | hierarchy compressed flag is set from the first sequence frame type (1/2 uncompressed, else compressed), overriding the ANP3 header flag |
| e327cd70b | CAEUserRadioTrackManager::SelectUserTrackIndex 0x4F3250, GetAudioFileType 0x4F31F0 | single track: rand(0,0) drawn, index 0 stored (port stored 1); ".mov" -> QuickTime added to extension table |

Open: CAnimManager::AddAnimAssocDefinition skip-over-occupied-slots (only matters on a second Initialise, overflows the 145-slot table in the exe); LoadAnimFile ANPK body not compared (vanilla data has no ANPK; ANP unknown header falls to ANPK in the exe, port asserts).

Tests: wave2_oracle_test (RW_EXE_ORACLE=compact exe) 1142 functions, 0 mismatches PC24/PC53; gameplay_regress combat+missions 25/25 PASS; v6 pre-commit grep clean; PortableWin64 cl.exe x64 syntax compile of the 3 touched files (PCH off, objects to scratch) rc 0.
