# MAC-STAB4: fire-audio use-after-free (2026-10-25, portable/main)

Build dirs build/PortableMacAsan (ASan) and build/PortableMacStab2 (release), harness stab_run.sh (own NOTSA_LOCK_FILE), NOTSA_MATRIX_CHECK=1, caffeinate -dimsu held (UserIsActive 1), one GL process at a time. Scratch /private/tmp/claude-501/stab4/. Note: a fresh OUT dir needs gta_sa.set copied from an older OUT/userdir, otherwise the first-run menu eats the 4 Returns and no new game starts (no "Loading the Game").

## Root cause (proven by the ASan stacks, 13/13 identical)
* Reader: `CAESoundManager::Service -> CAESound::UpdateParameters -> CAEFireAudioEntity::UpdateParameters:126 -> FxSystem_c::GetCompositeMatrix` multiplies by `m_ParentMatrix`.
* Freed block: `rw::Frame::destroy <- RwFrameDestroy <- CEntity::DeleteRwObject (Entity.cpp:350) <- CStreaming::DeleteAllRwObjects <- LoadScene` (roadveh / tpg command). Allocated by `CAtomicModelInfo::CreateInstance`: the RwFrame of a building.
* Ownership: `CEntity::CreateEffects` (0x533790) -> `Fx_c::CreateEntityFx(entity, name, pos, GetModellingMatrix())`: the 2dfx particle system (fire, smoke_flare, ... = systems with a fire audio entity) uses the building's frame matrix as parent matrix. The fire audio entity is NOT separate: `m_FireAE` is embedded in FxSystem_c (`m_FxSystem` = the owner), so its lifetime = the fx system's.
* The exe order is the same: `DeleteRwObject` destroys the frame first and only then `DestroyEffects -> Fx_c::DestroyEntityFx -> FxSystem_c::Kill` (just `Stop` + FX_KILLED). A killed system survives two `FxManager::Update` calls (FX_KILLED -> FX_3 -> destroyed in `DestroyFxSystem -> Exit -> m_FireAE.Terminate -> CancelSoundsOwnedByAudioEntity`). The fire sound (AE_FRONTEND_HIGHLIGHT fade-out) is serviced in that window and reads the freed frame. Ported functions (DeleteRwObject, DestroyEffects, DestroyEntityFx, Kill, FxSystem::Update, UpdateParameters, Terminate) match the exe semantics: NO port deviation. The exe has the same dangling read; it is harmless there (the freed heap block is still mapped and the sound position is only wrong for 1-2 frames), under ASan / a recycling allocator it is a UAF (and a garbage position if the block is reused by the streaming that follows LoadScene).

## Fix (`#ifdef NOTSA_PORTABLE`, Entity.cpp, guardlines TEXT OK, Win32 view byte-identical, no wine A/B needed)
At the top of `CEntity::DeleteRwObject`, when the model has 2dfx: for every `g_fx.m_FxEntities` item of this entity whose system has a parent matrix it does not own, `FxSystem_c::CopyParentMatrix()` (0x4AA890, the exe's own idiom in Weapon / Ped / TaskSimpleSwim / JetPack / Fx.cpp). The dying system then owns a snapshot of the matrix (freed in `~FxSystem_c` via m_allocatedParentMat) and nothing reads the freed frame. Consumer audit: readers of `m_ParentMatrix` = GetCompositeMatrix, Update, FxEmitterBP (particle spawn), ~FxSystem_c (frees an owned copy); all see a valid matrix with the same values as before the frame died. `SetMatrix` / `AttachToBone` (which would leak an owned copy) are never called on entity-fx systems. No layout change. Cost: one list walk per streamed-out entity that has 2dfx (list is short).
PORTABLE_SYNC.md entry added: Win32 has the same latent dangling read; the 6-line fix may be applied unguarded (optional, gameplay-neutral).

## Acceptance
| run | binary | route | result |
|---|---|---|---|
| b1 | ASan, HEAD before fix | stab3_car | 13 heap-use-after-free (UpdateParameters <- DeleteRwObject <- LoadScene) + 3 known GetModelInfo global-buffer-overflow |
| a1 | ASan, fix | stab3_car (190 s) | exit 0, 0 UAF, 0 checker failures, 0 guard; only the 3 known startup GetModelInfo reads (ProcessLoadingChannel:1904, ConvertBufferToObject:421, FinishLoadingLargeFile:1481) |
| a2 | ASan, fix | stab_combat (242 s) | exit 0, same: only the 3 known reads |
| s1 | release (Stab2 dir), fix | stab3_soak2 (10.2 min script, 40 roadveh, 12 combat, 56 tp) | exit 0 after 649 s, 0 [matrix] CHECK FAILED, 0 guard firings, 0 [pool] FATAL, 0 crashes |
All runs: "Loading the Game" present (139 lines), clean quit.
