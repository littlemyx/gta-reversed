# POSTFX-RASTER — post-effect raster sizes: exe rounds to the next power of two strictly ABOVE w

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh postfx-raster.
## Evidence (.notes/reports/ORACLE-W3-RENDER.md, X87-TRIG note in .notes/reports/X87-TRIG.md)
CPostEffects::SetupBackBufferVertex and ImmediateModeFilterStuffInitialise (class D, no oracle row) size their rasters with
ftol(ln(w)/ln2)+1 in the exe → next power of two strictly above w (1024 → 2048); the port uses std::bit_ceil (1024 → 1024). Different
raster sizes change the UVs of the colour filter / heat haze / blur passes at power-of-two resolutions (1024x768 / 2048-wide modes).
## Task (two functions + any other bit_ceil/power-of-two site in PostEffects.cpp)
Port the exe arithmetic exactly (x87 ln/fdiv/ftol — a naked-asm or inline-x87 helper ExeRasterSize like the report plans, so the
rounding mode / precision matches). Add a small row for the helper over w = 1..8192 (compare with the exe's inline sequence executed as a
snippet, or with values hand-derived from the asm). Visual check: run the game windowed at 1024x768 with NOVSYNC, screenshot with
heat haze or colour filter active before/after (gpu_locked.sh). HARD budget under 200k tokens.
## Acceptance
Helper row 0 mismatches over the range; both functions use it; screenshots; report ≤15 lines with start/end time.
