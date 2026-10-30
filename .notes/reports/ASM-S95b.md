# ASM-S95b (CHud draw/state functions, phase 3 tier B) - 2026-10-30
Result: 15 rows compared against the exe (logic and timing in full, draw-call argument lists skimmed), 15 MATCH, 0 deviations, 0 code changes. Table: S95b.result.tsv.
Checked: all three CHud jump tables (help text states 0x58C050/0x58C064, odd-job 0x58D224, vehicle name 0x58B16C), every timer/fade constant (600/3000/1000/120/1500/5000 ms, 0.4/0.3/0.2/1-6 factors, 40/30 clamps), message-style slot indices (0xBAACC0 + 0x80*n), static posY guards (0xBAB224, 0xBAB21C), widescreen/cutscene/pause/subtitle gating in Draw, DrawAfterFade, DrawHelpText, DrawSubtitles, DrawVehicleName, camera-mode and weapon-type sets in DrawCrossHairs.
Not re-read: CRect literals in DrawCrossHairs/DrawRadar (already fixed by SYSTEMIC-7 200a8d848).
Tests: no code changed, so no build / oracle / combat regress was run (they would exercise the unmodified origin).
