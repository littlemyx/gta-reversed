# RD-AB-3 - orig-vs-port divergences at frame 3 and 4 (+ the two self-identity residues)

Agent RD-AB-3, rules v5, 2026-10-24 22:35 - 2026-10-25 00:15 WEST. Worktree C:/gta-wt/rd-ab-3 (branch rd-ab-3 on b193a685). Data C:/gta-scratch/rdd/ab3 (r1..r14). Own tracer proxy: RD_PX=C:/gta-scratch/rdd/px_ab3.

## Divergences (0-based traced frames, pin 1023, intro scenario)
| frame | layer | cause | fix |
|---|---|---|---|
| 3 | rng (133 vs 120 calls) | `CStreaming::RemoveBuildingsNotInArea` (exe 0x4094B0) deleted the RwObject of every IN-area dummy and object (loops tested `IsInArea` instead of `!IsInArea`; exe 0x409545/0x409549, 0x4095A2/0x4095A6: skip when area == arg or area == 13). Called by CGameLogic::RestorePlayerStuffDuringResurrection at the start of a new game. Consequence: at frame 3 the 89 dummies had no RwObject, CPopulation::ConvertToRealObject -> CObject(CDummyObject*) took the no-RwObject path, so `CEntity::AttachToRwObject -> CreateEffects -> Fx_c::CreateEntityFx -> FxSystem_c::Play` (1 rand per particle 2dfx, model 1265 trash bags) never ran; the missing draw shifted the RNG and the first GenerateCarCreationCoors2 (finding (b) of the brief) hit the other `rand()&0xF` branch (start node via FindNodeClosestToCoors, none within 50 m) and returned early - (b) was a consequence of (a), path areas are identical (probe `pn=`: areas 14,15,22,23 loaded on both sides). | Streaming.cpp RemoveBuildingsNotInArea: `!IsInArea` in the object and dummy loops (asm 0x409545.., 0x4095A2..) |
| 4 | rng (122 vs 81 calls) | `CAutomobile::ProcessBuoyancy` (0x6A8C00) calls the CRT rand() at 0x6A8C36 and drops the result; the port had it in a "Unused code" comment. Every later draw of the frame was shifted by one. | Automobile.cpp: `(void)CGeneral::GetRandomNumber();` (CPed::ProcessBuoyancy already has it; CBike 0x6B5FB0 and cBuoyancy::ProcessBuoyancy have no rand) |
| 10 | rng (306 vs 184 calls), peds 4 vs 3 | not analysed (out of time): population spawn timing, the next lead | - |

## Equal prefixes (orig-port, pin 1023, 3 reps, 3000 frames)
before (RD-NOISE-2 tree, after its Radar fix): 3 / 3 / 3 -> after the RemoveBuildingsNotInArea fix 4 / 4 / 4 -> after the ProcessBuoyancy rand 10 / 10 / 10.
Self-identity (3 reps, final binaries, own proxy): orig-orig 2865 / 2885 / 2865, port-port 2844 / 2854 / 2844 (RD-NOISE-2: orig 1186 / 1186 / 2734, port 2999 / 2985 / 2985). The orig-orig "regression" is not pin 512: the same build run.sh WITHOUT `RD_PX` uses the shared proxy C:/gta-scratch/rdd/px and gave orig-orig 1509 / 14 / 14 (r9) - there an audio ResolveProbability rand (0x4D9C97) at the end of frame 1 is NOT partitioned (that proxy lacks pin 128 / is older). With `export RD_PX=<own dir>` + `--build-proxy` (rd_orig.c rebuilt) the orig is stable to ~2865. Whoever reruns this must rebuild/point RD_PX; the shared px dir may be stale.
Residues: orig-orig and port-port both flake once around frame 2850-2890 (not looked into; the port-port render-pass flake of RD-NOISE-2 is the same class: one frame runs without the render-pass rand calls). Not reached: >= 3000 on 3 reps.

## New diagnostics (inert without the env vars)
* D line: `pn=<hi><lo>` loaded path-node areas 0..63 and `rw=<buildings>,<dummies>,<objects>` count of entities that have an RwObject (both sides). This is how the frame-3 cause showed: `rw=81,89,0` on both sides at frames 0-2, then orig `81,80,9`, port `81,0,0`.
* rd_orig.c `RD_RSTACK=<lo>-<hi>` (hex return-address range; with pin bit 32 + RD_RLOG): `T fc ra: <call-site candidates from the stack>` and `U fc ents: ptr/m<model>/t<type>` lines for every rand() call whose caller is in the range (who called Play, with the entity's model) - this identified FxSystem_c::Play <- CEntity::CreateEffects <- AttachToRwObject <- CObject(CDummyObject*) <- ConvertToRealObject and model 1265.
* `RD_DUMPSTR=<prefix>` / `NOTSA_LOCKSTEP_DUMPSTR=<prefix>`: per model id (load state, flags) of CStreamingInfo at traced frames 1..6 (A-B streaming hunt; ruled out streaming for frame 3: DFF states equal, only TXD ids 20014.. differ in pairs - NOT investigated, may matter later).
* Lockstep.cpp D line reads ThePaths / pools only with pin 64.

## Gate (rules v5)
Release build (StandaloneRelease, this tree) ok. Oracle targets touched: none built - no oracle row exists for CStreaming::RemoveBuildingsNotInArea (script_oracle_test only stubs it) or CAutomobile::ProcessBuoyancy; both fixes are asm-proven (disassembly above) and A-B-proven (prefix 3 -> 4 -> 10). Soak: see hand-back. Not run: run_all_tests (orchestrator).
Consumer audit: IsInArea has no other user; the RwObject of in-area dummies/objects is only read by the renderer and ConvertToRealObject (both now see what the exe sees). The extra rand draw in ProcessBuoyancy changes the RNG sequence of every game (as in the exe).
