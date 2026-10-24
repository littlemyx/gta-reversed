# RD-NOISE-2 - port self-identity with the RR pins, then the first A-B divergences

Agent RD-NOISE-2, rules v5, 2026-10-24 18:21 - ~19:55 WEST. Worktree C:/gta-wt/rd-noise-2 (branch rd-noise-2 on e4cd76e9). Data C:/gta-scratch/rdd/n2a..n2f.

## Result
New pin bit **512 = audio off** (mirror of the RR harness `NOTSA_RR_NOAUDIO`: port `CAudioEngine::Service()` returns, AudioEngine.cpp; original `rd_orig.c` patches 0x507750 to `ret`). With pin 1023 (= all bits 1..512; 128 = audio RNG partition, measured now):

| pair (intro, 3000 frames, 3 reps, pin 1023) | equal prefix |
|---|---|
| port p1-p2 / p1-p3 / p2-p3 | 2999 / 2985 / 2985 (before: 7-9) |
| orig o1-o2 / o1-o3 / o2-o3 | 1186 / 1186 / 2734 (before: 1520 / 2177 / 1520) |
| orig-port o1-p1 / o2-p2 / o3-p3 | 1 / 1 / 1 before the Radar fix (scr hash at frame 1); after the fix o1-p1 = 3 (scr equal to frame 2695; rng/ncalls at frame 3) |
Earlier port-only sets (n2a/b/c/d): prefixes 2617..3000, always a single late flake (3-5 sets of 3 reps: 1-2 of 3 runs reach 3000). Acceptance (>= 3000 on 3 reps) NOT reached; the residual is one-off (see below).

## Port frame 8/19 extra RandomBool = audio
The extra `CGeneral::RandomBool` at frame 7-9 was the audio state (RR finding F3: `CAEWeatherAudioEntity::UpdateParameters` draws RandomBool only in a certain DirectSound playing state). Pin 512 removes the cause, pin 128 (measured: it works with 512) isolates the rest. No caller to fix in the port.
Residual port flake: in the failing run one frame ends early (the R log of frames 2972/2973 shows only the Process head, Weather::Update + Camera::Process, and no render-pass rand calls: CWaterLevel::RenderWater, PlantBuffer, Coronas) => that frame's render pass did not run; the render pass draws from the one RNG, so the sim diverges. New diagnostics `X fc rendered|skip:..` lines (Idle) show no explicit skip path was taken (param/Horizon/menu notes absent), i.e. the frame was not an Idle at all. Cause not found (suspect: a FrontendIdle/other CTimer::Update call or a focus/menu event); next step: log the caller of CTimer::Update in the failing frame.

## A-B (orig vs port), pin 1023, frames 0-based
* frame 1 (scr hash) **FIXED**: script-space dump (`NOTSA_LOCKSTEP_DUMPSCR` / `RD_DUMPSCR`) differed in 44 dwords, orig `0x0002xxxx` vs port `0x0001xxxx` = **radar blip handles** `(tRadarTrace.m_nCounter << 16) | slot` (the D probe line `bc=` shows the counters: orig 1 at frame 0 in every slot, port 0; pickup reference indices `pk=` were equal and a red herring). Cause: exe `CRadar::Initialise` 0x587FB0 sets `m_nCounter = 1` of every trace (0x587FF2 `mov [eax-8], cx` in the init loop; `ClearActualBlip` 0x587C10 leaves it alone), the port left it 0. Fix: Radar.cpp CRadar::Initialise. Effect on the same A-B run: scr hash equal for frames 0..2695 (was: differs at frame 1), equal prefix 1 -> 3 (n2i). Consumer audit: every reader/writer of m_nCounter is GetNewUniqueBlipIndex / GetActualBlipArrayIndex / save-load of traces (counter now starts at 1 as in the exe; blip handles issued by scripts are 1 higher than before, scripts only store/compare them). Oracle row: not possible without building the Radar oracle target (not run).
* frame 3 (rng, ncalls 133 vs 120): `tools/standalone/replay_diff/rcmp.py` (new; orig return address -> registry function, port site/caller). Orig frame 3: 70 CEntity ctor, 40 GenerateCarCreationCoors2, 14 GeneratePlateText, 2 SetModelIndex, 2 ChooseModel, 2 PickRandomCar, Weather, Camera, **1 FxSystem_c::Play (ret 0x4AA337) between the 3rd and 4th object ctor - missing in the port**, and the orig's first GenerateOneRandomCar does a long GenerateCarCreationCoors2 walk (22 rand calls) where the port's returns after 1-3 calls (CarCtrl.cpp:2476/2480: start node invalid). Both point at state that differs before frame 3: an effect started (Play) in the orig only, and path nodes near the player found in the orig only (path streaming / node areas at frame 3). Not fixed.
* rng equal through frame 2 (the frame-1 CarCtrl fix of RD-NOISE holds: 73 = 73, 262 = 262 calls).

## Tooling added (all inert without NOTSA_LOCKSTEP_OUT / RD_OUT)
pin 512 (above); `NOTSA_LOCKSTEP_DUMPSCR=<prefix>` / `RD_DUMPSCR=<prefix>` raw script space of traced frames 1..4; `X fc rendered|skip:*` lines from Idle(); `rcmp.py <repo> <exe> <orig.trace> <port.trace> <fA> <fB> [--seq]` per-frame rand site tables for both sides. Rebuild the original proxy with build_orig.sh for pin 512 / DUMPSCR (RD_PX).

## Gate (rules v5)
Release build (StandaloneRelease, this tree): ok. Oracle targets touched: none (no oracle test covers AudioEngine.cpp / app_game.cpp changes; both edits are inert by default). Soak (3 min route, no NOVSYNC): first run OK before the Radar fix (RESULT OK, errors 0); second run after the fix: see hand-back. Not run: full run_all_tests (orchestrator).
Note: do not edit run.sh while a run is in progress (I did once; the 6 traces were complete, only the report phase broke - table computed by hand with rd_diff.py).
