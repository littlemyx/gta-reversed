# ORACLE-W3-UI report

Agent ORACLE-W3-UI (rules v3), start 03:46, end ~04:50 (2026-10-20), worktree C:/gta-wt/oracle-w3-ui (branch oracle-w3-ui, base ce9cd899). New target `wave3_ui_oracle_test` (W2 harness: deterministic arena, whole exe data image + a 128 KB scratch block compared after every case), rows in `tests/standalone/wave3_oracle_ui*.inc` (a Messages, b Hud, c Radar, d Font, e Text / Sprite2d / MenuManager, f recorded debug-draw calls), skip list `tests/standalone/tools/w3_ui_skip.tsv`.
**77 functions, PC24 0 mismatches** (PC53, CRT default precision, informational: 652, all in ShowRadarMarker). Development switches: `W3U_DIV=n` divides the case counts, `W3U_SCALE=n` multiplies them.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 4057 | **4134** |
| game_sa root files covered | 1122 | **1196** |
| uncovered A / B in the root files | 310 / 436 | **288 / 394** |
| slice files (Messages / Hud / Radar / Font / Sprite / Sprite2d / MenuManager / Text): uncovered A / B | 27 / 46 | **3 / 4** (each with a reason, below) |
| port bugs fixed (one commit each) | | **10 commits** (9 behaviour fixes + 1 oracle-parity change) |

## Harness
- Globals under test live at their exe addresses on both sides (NOTSA_ORACLE_ADDRESS_GLOBALS); each case rebuilds the fixture from the seed: a pool of 32 GXT-like strings in the scratch block (incl. `~1~` / `~a~` markers and the same text at different addresses), message arrays with a random number of live entries, the 175-slot blip table with valid / stale / -1 handles, HUD timers and states, CFont state + `gFontData`, pads for the front-end direction checks.
- Dead data is normalised after the call (a message whose Text is null, padding bytes, bytes after a terminator): the exe only clears the text pointer, the port wipes whole entries, nothing reads them.
- `CTheScripts::ScriptDebugLine3D` (0x485DE0, called by `CRadar::ShowRadarMarker`; the exe body is a bare `ret`, the vectors are passed by value) is recorded by a host stub on the exe side and by a port-side definition (TheScripts.cpp is not linked) with the same body; the call log is part of the compared scratch block.
- Port-side definitions for TUs that are not linked: `CGame::CanSeeOutSideFromCurrArea`, `CEntryExitManager::GetPositionRelativeToOutsideWorld` (the fixture keeps the interior stack empty).

## Port bugs fixed (rows failed pre-fix; counts = failing PC24 cases of the first run on the unfixed code, logs C:/gta-scratch/w3ui/run*.log)
| function | asm | bug | pre-fix failing |
|---|---|---|---|
| CMessages::Init | 0x69EE00 | ClearMessages(true) skipped the two mission-title big-message styles; the exe clears every style | 98/100 |
| CMessages::AddToPreviousBriefArray | 0x69DD50 | `std::shift_right(begin, begin + i)` keeps the elements inside the range: the entry at i-1 was dropped on every insertion (the previous-brief list lost a message) | 1241/1500 |
| CMessages::ClearThisPrint | 0x69EA30 | the new front brief went to the previous briefs without testing its PreviousBrief flag (+0x2C) | 47/1500 |
| CMessages::Process | 0x69EE60 | queue shifted with `std::shift_right` (towards the END: the following messages never reached the front); previous-brief add on every call regardless of the flag. The exe copies the live messages one slot down, stamps CreatedAtMS and adds only after an expiry | 1484/1500 |
| CMessages::InsertStringInString | 0x69E040 | looked for `~1~` (the number marker); the exe replaces `~a~`; also the 16 bit output bound and the zero fill to 400 bytes | 452/1500 |
| CRadar::ChangeBlipBrightness | 0x583C70 | brightness == 1 sets the bit in the port, the exe clears it | 9/700 |
| CRadar::SetCoordBlip (SetShortRangeCoordBlip through it) | 0x583820 / 0x583920 | the exe stores flag byte +0x25 = 3 (all other flags of the reused slot cleared); the port only set the two bits, so a new blip inherited short-range / friendly / remain / fade / appearance of the previous user of the slot | 642/1000, 633/1000 |
| CFont::SetDropColor | 0x719510 | drop-shadow alpha = colour alpha * font alpha * (1/255) (0x859A3C); the port omitted the 1/255 | 235/500 |
| CMenuManager::CheckHover | 0x57C4F0 | integer bounds (the callers `_ftol` the stretched coordinates); the port took floats. Header signature + the two slider call sites changed | 98/2000 |
| CFont::FilterOutTokensFromString | 0x719240 | not a behaviour bug: the std::string copy allocated through the debug-STL proxy, so the allocation count differed in every case; now a 252-byte stack buffer like the exe (over-long input truncated instead of overflowing) | 200/200 (allocation count only) |

Header changes: Font.h (`GetScriptLetterSize`, `GetLetterIdPropValue` were file-`static` declarations in the header, unreachable from a test; now external, only Font.cpp defines and uses them), MenuManager.h (CheckHover signature).

## Rows that pass without a change
Messages: ClearMessages, ClearSmallMessagesOnly, ClearPreviousBriefArray, ClearThisBigPrint, ClearThisPrintBigNow, ClearAllMessagesDisplayedByGame, AddMessageQ / Jump / WithStringQ / JumpQWithString / WithNumberQ / JumpQWithNumber, AddBigMessageQ / WithNumber / WithNumberQ (after the previous-brief fix).
Hud: SetHelpMessageStatUpdate, SetHelpMessageWithNumber, SetVehicleName, SetZoneName, DrawFadeState (3000 cases, all four elements, force-fade-in, fractional time steps), GetRidOfAllHudMessages.
Radar: ChangeBlipColour, ChangeBlipDisplay, SetBlipFade, SetBlipFriendly, SetBlipSprite, ClearBlip, ClearActualBlip, ClearBlipForEntity, GetNewUniqueBlipIndex (counter wrap at 0xFFFE), DisplayThisBlip (all sprite classes, priorities, interior areas), InitFrontEndMap, AddBlipToLegendList, SetMapCentreToPlayerCoords, ShowRadarMarker.
Font: SetScale, SetScaleForCurrentLanguage, SetSlantRefPoint, SetSlant, SetFontStyle, SetWrapx, SetCentreSize, SetRightJustifyWrap, SetAlphaFade, SetDropShadowPosition, SetEdge, SetProportional, SetBackground, SetBackgroundColor, SetJustify, SetOrientation (0..2), GetLetterIdPropValue, GetScriptLetterSize.
Text / Sprite2d / MenuManager: CKeyArray::BinarySearch, CText::GetNameOfLoadedMissionText, CSprite2d::Constructor / SetRecipNearClip / SetAddressing / SetAddressingUV (fake texture), SetHelperText, ResetHelperText, GetNumberOfMenuOptions (cache + SCREEN_MAP / SCREEN_BRIEF), CheckFrontEnd{Up,Down,Left,Right}Input (key state, analogue stick with the function-local "previous stick" statics at their exe addresses, d-pad).

## Observed, not changed
- CFont::SetOrientation with an out-of-range alignment (>= 3): the exe leaves both flags alone, the port clears them; no caller passes one (rows use 0..2). GetLetterIdPropValue with a letter id >= 208 (OOB read in the exe, assert in the port): rows use 0..207. CKeyArray::BinarySearch with a negative first index (signed midpoint in the exe, `uint16` index in the port): unreachable, the loop returns first for first > last.
- The exe returns CMenuManager::GetNumberOfMenuOptions in `al` only (rows compare the low byte).
- Denormal operands in ShowRadarMarker differ between x87 (extended exponent range) and the port's float math; no game values, the row avoids them.
- CFont::FilterOutTokensFromString: the port stops at an unterminated token (the exe walks off its buffer); rows use terminated tokens.
- DisplayThisBlip (918 of 6000 cases) and AddBlipToLegendList (170 of 4000) have cases with a null player ped / out-of-table index that fault identically on both sides and are skipped by the harness; the rest is compared.

## Not covered (tests/standalone/tools/w3_ui_skip.tsv)
CMessages::CutString (no caller in the exe, the port asserts on purpose), CText::Constructor (the port runs Unload; identical on the zeroed global), CSprite::Initialise (bare ret), CSprite::InitSpriteBuffer / CSprite2d::InitPerFrame / CFont::InitPerFrame (read `RwEngineInstance`; the port's pointer is a separate variable; needs an RW engine fixture), CRadar::SetupAirstripBlips (deliberate FIX_BUGS deviation: frame-independent timer in a function-local static). The class C / S / D draw functions (Radar Draw*, Hud Draw*, Font Render* / Print*, Sprite2d Draw*, MenuManager Draw* / Print*, Messages::Display, Text Load) need recorded RW / draw stubs for CSprite2d / CFont on both sides: not written.

## Gates
`wave3_ui_oracle_test` 77 functions PC24 0 mismatches, exit 0 (final tree, logs C:/gta-scratch/w3ui/full1.log and full2.log). Targets linking the changed TUs, built and run in this worktree on the same tree: world_oracle_test (117 functions, PC24 0), x87_oracle_test (67 rows, PC24 0), review_oracle_test (42, PC24 0), gameplay_oracle_test (313, PC24 0 excluding NaN payloads, exit 0). The orchestrator runs the full run_all_tests.
