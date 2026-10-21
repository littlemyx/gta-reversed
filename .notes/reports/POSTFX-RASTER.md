# POSTFX-RASTER - front-buffer raster size = exe's next power of two strictly above w (2026-10-21, 03:09 - 03:26)

## Result
* Code done and gated (commit b7036680 on branch postfx-raster, rebased on 272acc53): `CPostEffects::ExeRasterSize(w)` (PostEffects.cpp/.h) = `pow(2.0, ftol(ln(w)/ln(2)) + 1)`, the ln/fdiv/ftol part is a naked x87 helper
  (`fldln2; fld1; fadd st,st; fyl2x` = ln(2) at the current precision control, `fldln2; fild w; fyl2x; fdiv st(0),st(1)`, truncating `fistp qword`). Used in
  `SetupBackBufferVertex` (0x7043D0: width / height of the front-buffer raster) and `ImmediateModeFilterStuffInitialize` (0x703CC0: fFrontBufferU2 = SCREEN_WIDTH / size_w, V2 = SCREEN_HEIGHT / size_h;
  the exe divides by the width size for U2 and the height size for V2, traced from the stack slots at 0x703F3A..0x703F73). No other bit_ceil / power-of-two site exists in PostEffects.cpp.
* Row `CPostEffects::ExeRasterSize w=1..8192` in tests/standalone/review_oracle_test.cpp: the port against TWO transcriptions of the exe sequences (SetupBackBufferVertex spills ln(2) to a qword, the
  Initialize function keeps it in st(1)), both calling the exe's own `_ftol` 0x821B40 and `pow` 0x8220F0 from the mapped image: **0 mismatches at PC24 and PC53** (8192 widths each, 13 powers of two > 1 hit).
  Pre-fix (std::bit_ceil): 14 of 8192 widths differ at PC24 = every power of two (1 2 4 .. 4096 8192; 1024 -> exe 2048, port 1024; w=1 -> exe 2, port 1). The old code agreed for all non-powers (1000, 960, 600 -> 1024).
  review_oracle_test: 43 rows ok (PC53 total 1502 is pre-existing in other rows, mine 0). Release build (StandaloneRelease) OK.
* Effect at 1024x768 (computed, not seen): exe front buffer 2048x1024 (U2 = 0.5, V2 = 0.75, cc_vertices UVs divided by 2048/1024), port had 1024x1024 (U2 = 1.0, V2 = 0.75).
  The same screen area is sampled, but the heat-haze / blur patch positions (`GetRandomNumber() * (raster width - scan)`) and the half-texel offsets use the raster size, so the haze pattern differs.

## NOT done: before / after screenshots - HEAD 272acc53 crashes when a new game starts
Both exes (my baseline build of 272acc53 and the main checkout's build/StandaloneRelease exe of 02:56) crash 3/3 on soak.sh's new-game route (NOVSYNC and vsync, private and default data dir, empty userdir):
`menu > newgame_loading > (cutscene_skip)`, EXCEPTION_ACCESS_VIOLATION read of address 0 at a function entry (ECX = 0, EIP at the first instruction), right after the log line
`CTxdStore::AddTxdSlot("script") -> 3602` (before it: AnimManager `Reading block (DEALER)`), 2 s after the key:return that ends the intro skip or during loading. Last good soak: 2026-10-18 21:27 (/c/gta-soak-out/20261018-212754).
Logs: C:/gta-scratch/postfx_raster/out_before{,2,3,4}/{log.log,standalone.log,report.txt}, out_mainref/ (main exe), maps in C:/gta-scratch/postfx_raster/{before,mainref}/bin.
Crash location guess (stack-word fit with the ASLR base, ambiguous): `notsa::script::detail::AddCommandHandler<... DisplayText(CVector2D, const char*) ...>` wrapper (offset 0x5567d0 of .text) - a script command with a null
string argument; not verified. Route used: C:/gta-scratch/postfx_raster/route.txt (header_px + `cmd:time:10:40`, `vmode:1024x768`, 4 marks). Baseline and fixed exes are ready: C:/gta-scratch/postfx_raster/{before,after}/bin.

CONTINUE: once the new-game crash is fixed (separate brief), run
`NOTSA_STANDALONE_NOVSYNC=1 SHOT_EVERY=100000 ROUTE=C:/gta-scratch/postfx_raster/route.txt OUT=<dir> tools/standalone/soak.sh C:/gta-scratch/postfx_raster/{before,after} 3`
for before and after (shots/mark_r_a..d.png at 1024x768 after `vmode:1024x768`, hour 10:40 = heat haze active) and compare; the code part needs no rework.
