#include "StdInc.h"

#include "Camera.h"
#include "ControllerConfigManager.h"
#include "EntryExitManager.h"
#include "FireManager.h"
#include "PedClothesDesc.h"
#include "PlaneBanners.h"
#include "RealTimeShadowManager.h"
#include "Replay.h"
#include "Skidmarks.h"

void CReplay::InjectHooks() {
    RH_ScopedClass(CReplay);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x45E220);
    RH_ScopedInstall(Update, 0x460500);
    RH_ScopedInstall(DisableReplays, 0x45B150);
    RH_ScopedInstall(EnableReplays, 0x45B160);
    RH_ScopedInstall(StorePedAnimation, 0x45B170);
    RH_ScopedInstall(StorePedUpdate, 0x45C940);
    RH_ScopedInstall(RetrievePedAnimation, 0x45B4D0);
    RH_ScopedInstall(ProcessReplayCamera, 0x45D060);
    RH_ScopedInstall(Display, 0x45C210);
    RH_ScopedInstall(MarkEverythingAsNew, 0x45D430);
    RH_ScopedInstall(EmptyReplayBuffer, 0x45EC90);
    RH_ScopedInstall(EmptyPedsAndVehiclePools_NoDestructors, 0x45D390);
    RH_ScopedInstall(GoToNextBlock, 0x45E2A0);
    RH_ScopedInstall(RecordVehicleDeleted, 0x45EBB0);
    RH_ScopedInstall(RecordPedDeleted, 0x45EC20);
    RH_ScopedInstall(SaveReplayToHD, 0x45C340);
    RH_ScopedInstall(PlayReplayFromHD, 0x460390);
    RH_ScopedInstall(ShouldStandardCameraBeProcessed, 0x45C440);
    RH_ScopedInstall(ProcessLookAroundCam, 0x45D760);
    RH_ScopedInstall(FindPoolIndexForPed, 0x45C450);
    RH_ScopedInstall(FindPoolIndexForVehicle, 0x45C460);
    RH_ScopedInstall(ProcessPedUpdate, 0x45CA70);
    RH_ScopedInstall(CanWeFindPoolIndexForPed, 0x45C470);
    RH_ScopedInstall(CanWeFindPoolIndexForVehicle, 0x45C490);
    RH_ScopedInstall(StorePlayerInfoVariables, 0x45F020);
    RH_ScopedInstall(RestorePlayerInfoVariables, 0x45E1F0);
    RH_ScopedInstall(StoreStuffInMem, 0x45F180);
    RH_ScopedInstall(RestoreStuffFromMem, 0x45ECD0);
    RH_ScopedInstall(FinishPlayback, 0x45F050);
    RH_ScopedInstall(RecordThisFrame, 0x45E300);
    RH_ScopedInstall(StoreClothesDesc, 0x45C750);
    RH_ScopedInstall(RestoreClothesDesc, 0x45C7D0);
    RH_ScopedInstall(DealWithNewPedPacket, 0x45CEA0);
    RH_ScopedInstall(PlayBackThisFrameInterpolation, 0x45F380);
    RH_ScopedInstall(FastForwardToTime, 0x460350);
    RH_ScopedInstall(PlayBackThisFrame, 0x4604A0);
    RH_ScopedInstall(FindSizeOfPacket, 0x45C850);
    RH_ScopedInstall(IsThisVehicleUsedInRecording, 0x45DE40);
    RH_ScopedInstall(IsThisPedUsedInRecording, 0x45DDE0);
    RH_ScopedInstall(InitialiseVehiclePoolConversionTable, 0x45EFA0);
    RH_ScopedInstall(InitialisePedPoolConversionTable, 0x45EF20);
    RH_ScopedInstall(InitialisePoolConversionTables, 0x45F370);
    RH_ScopedInstall(FindFirstFocusCoordinate, 0x45D6C0);
    RH_ScopedInstall(NumberFramesAvailableToPlay, 0x45D670);
    RH_ScopedInstall(StreamAllNecessaryCarsAndPeds, 0x45D4B0);
    RH_ScopedInstall(CreatePlayerPed, 0x45D540);
    RH_ScopedInstall(TriggerPlayback, 0x4600F0);
}

// 0x460625 thunk
// 0x45E220
void CReplay::Init() {
    Mode = MODE_RECORD;
    Playback.m_nOffset = 0u; // field by field like 0x45E220 (assigning {} also zeroed the padding bytes)
    Playback.m_pBase   = nullptr;
    Playback.m_bSlot   = 0u;
    EmptyReplayBuffer();
    pReferences = nullptr;
    pPickups = nullptr;
    pEmptyReferences = nullptr;
    pWorld1 = nullptr;
    pStoredCam = nullptr;
    pRadarBlips = nullptr;
    bPlayingBackFromFile = false;
    bDoLoadSceneWhenDone = false;
    FramesActiveLookAroundCam = 0;
    bReplayEnabled = true;
}

// 0x460500
void CReplay::Update() {
    ZoneScoped;

    if (CCutsceneMgr::IsCutsceneProcessing() || CPad::GetPad()->ArePlayerControlsDisabled()
        || FrontEndMenuManager.m_bMenuActive || CEntryExitManager::ms_exitEnterState) {
        Init();
        return;
    }

    if (CTimer::GetIsCodePaused() || CTimer::GetIsUserPaused())
        return;

    NOTSA_GLOBAL_LOCAL(s_FrameRecordCounter, 0x97FB40, (uint32), {});
    if (Mode == MODE_PLAYBACK) {
        PlayBackThisFrame();
    } else if (CTimer::GetTimeInMS() - s_FrameRecordCounter > 26) {
        s_FrameRecordCounter = CTimer::GetTimeInMS();
        RecordThisFrame();
    }

    if (CDraw::IsFading() || !bReplayEnabled)
        return;

    if (Mode == MODE_PLAYBACK) {
        if (ControlsManager.GetIsKeyboardKeyJustDown(rsF1) || ControlsManager.GetIsKeyboardKeyJustDown(rsF3)) {
            FinishPlayback();
        }
    } else {
        if (ControlsManager.GetIsKeyboardKeyJustDown(rsF1)) {
            // Play stored replay
            TriggerPlayback(REPLAY_CAM_MODE_AS_STORED, CVector{}, false);
        } else if (ControlsManager.GetIsKeyboardKeyJustDown(rsF2)) {
            // Save to hard disk
            SaveReplayToHD();
        } else if (ControlsManager.GetIsKeyboardKeyJustDown(rsF3)) {
            // Play from hard disk
            PlayReplayFromHD();
        }
    }
}

// 0x45B150
void CReplay::DisableReplays() {
    bReplayEnabled = false;
}

// 0x45B160
void CReplay::EnableReplays() {
    bReplayEnabled = true;
}

// 0x45B170
void CReplay::StorePedAnimation(CPed* ped, CStoredAnimationState& state) {
    float blendValue{};
    CAnimBlendAssociation* second{};
    const auto main = RpAnimBlendClumpGetMainAssociation(ped->GetRpClump(), &second, &blendValue);
    // 0x45B170 writes the first four fields of state[0] and leaves m_nGroupId2 alone (it is only used by the blend states 1 and 2)
    const auto StoreMain = [&state](const AnimationState& made) {
        auto& st       = state[0];
        st.m_nAnimId   = made.m_nAnimId;
        st.m_nTime     = made.m_nTime;
        st.m_nSpeed    = made.m_nSpeed;
        st.m_nGroupId1 = made.m_nGroupId1;
    };
    if (main) {
        StoreMain(AnimationState::Make(main->m_AnimId, main->m_CurrentTime, main->m_Speed, static_cast<uint8>(main->m_AnimGroupId)));
    } else {
        StoreMain(AnimationState::Make(ANIM_ID_IDLE, 0.0f, 1.0f, 0));
    }

    if (second) {
        state[1] = AnimationState::MakeBlend(second->m_AnimId, second->m_CurrentTime, second->m_Speed, static_cast<uint8>(second->m_AnimGroupId), blendValue);
    } else {
        state[1] = AnimationState::MakeBlend(ANIM_ID_WALK, 0.0f, 0.0f, 0, 0.0f);
    }

    const auto partial = RpAnimBlendClumpGetMainPartialAssociation(ped->GetRpClump());
    if (partial && partial->m_AnimId >= ANIM_ID_WALK) {
        state[2] = AnimationState::MakeBlend(partial->m_AnimId, partial->m_CurrentTime, partial->m_Speed, static_cast<uint8>(partial->m_AnimGroupId), partial->m_BlendAmount);
    } else {
        state[2] = AnimationState::MakeBlend(ANIM_ID_WALK, 0.0f, 0.0f, 0, 0.0f);
    }
}

// 0x45C940
void CReplay::StorePedUpdate(CPed* ped, uint8 index) {
    auto vehicleIdx = [ped]() {
        if (ped->IsInVehicle()) {
            return GetVehiclePool()->GetIndex(ped->GetVehicleIfInOne()) + 1;
        } else {
            return ptrdiff_t{0};
        }
    }();

    // 0x45C940 writes the packet in place, field by field, and never touches the padding (2 bytes before the matrix, 2 inside the compressed matrix, 2 at the end), the unused bits of the
    // flags byte and state[0].m_nGroupId2: whatever the buffer held there stays. A local packet copied in with Record.Write overwrote them (zeros).
    auto* const block = reinterpret_cast<tReplayPedUpdateBlock*>(reinterpret_cast<uint8*>(Record.m_pBase) + Record.m_nOffset);
    static_assert(sizeof(tReplayPedUpdateBlock) == 52);
    block->type                     = REPLAY_PACKET_PED_UPDATE;
    block->poolRef                  = index;
    block->heading                  = NOTSA_FTOL(int8, ped->m_fCurrentRotation * HEADING_COMPRESS_VALUE);
    block->matrix.CompressFromFullMatrix(ped->GetMatrix());
    block->weaponModel              = (int16)ped->m_nWeaponModelId;
    block->contactSurfaceBrightness = NOTSA_FTOL(uint8, ped->m_fContactSurfaceBrightness * 100.0f);
    // 0x45C99D: the three flag bits are the ENTITY flags (ped + 0x1C) bit 7 (visible), bit 30 (tunnel), bit 31 (tunnel transition) - not CPed bits; the visible bit is cleared for the player in first person (0x45C9ED)
    block->flags.isVisible          = ped->m_bIsVisible;
    block->flags.tunnel             = ped->m_bTunnel;
    block->flags.tunnelTransition   = ped->m_bTunnelTransition;
    if (ped == FindPlayerPed() && gbFirstPersonRunThisFrame) {
        block->flags.isVisible = false;
    }
    block->vehicleIndex             = (uint8)vehicleIdx;
    block->animGroup                = (uint16)ped->m_nAnimGroup;
    StorePedAnimation(ped, block->animState);
    Record.m_nOffset += sizeof(tReplayPedUpdateBlock);
}

// 0x45B4D0
void CReplay::RetrievePedAnimation(CPed* ped, const CStoredAnimationState& state) {
    // todo: refactor

    // The exe multiplies by the reciprocals from its data section (0x859C2C / 0x859C28 / 0x859C24), it does not divide by 63.75 / 85 / 127.5
    constexpr auto ANIM_TIME_DECOMPRESS_VALUE  = 0.015686275f;
    constexpr auto ANIM_SPEED_DECOMPRESS_VALUE = 0.011764706f;
    constexpr auto ANIM_BLEND_DECOMPRESS_VALUE = 0.007843138f;

    CAnimBlendAssociation* anim = nullptr;
    if (auto first = state[0]; first.m_nAnimId > 3u) {
        auto animBlock = CAnimManager::GetAssocGroups()[first.m_nGroupId1].m_AnimBlock;
        if (animBlock && animBlock->IsLoaded) {
            anim = CAnimManager::BlendAnimation(ped->GetRpClump(), (AssocGroupId)first.m_nGroupId1, (AnimationId)first.m_nAnimId, 100.0f);
        } else {
            anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_DEFAULT, ANIM_ID_WALK, 100.0f);
        }
    } else {
        anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ped->m_nAnimGroup, (AnimationId)first.m_nAnimId, 100.0f);
    }
    anim->SetCurrentTime(state[0].m_nTime * ANIM_TIME_DECOMPRESS_VALUE);
    anim->SetSpeed(state[0].m_nSpeed * ANIM_SPEED_DECOMPRESS_VALUE);
    anim->SetBlend(1.0f, 1.0f);
    anim->m_nCallbackType = ANIM_BLEND_CALLBACK_NONE;

    anim = nullptr;
    if (auto second = state[1]; second.m_nGroupId1 && second.m_nAnimId) {
        if (second.m_nAnimId > 3u) {
            if (CAnimManager::GetAssocGroups()[second.m_nGroupId2].m_NumAnims > 0) { // the exe checks the group only for ids > 3
                anim = CAnimManager::BlendAnimation(ped->GetRpClump(), (AssocGroupId)second.m_nGroupId2, (AnimationId)second.m_nAnimId, 100.0f);
            }
        } else {
            anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ped->m_nAnimGroup, (AnimationId)second.m_nAnimId, 100.0f);
        }

        if (anim) {
            anim->SetCurrentTime(second.m_nTime * ANIM_TIME_DECOMPRESS_VALUE);
            anim->SetSpeed(second.m_nSpeed * ANIM_SPEED_DECOMPRESS_VALUE);
            anim->SetBlend(second.m_nGroupId1 * ANIM_BLEND_DECOMPRESS_VALUE, 1.0f);
            anim->m_nCallbackType = ANIM_BLEND_CALLBACK_NONE;
        }
    }

    RpAnimBlendClumpRemoveAssociations(ped->GetRpClump(), ANIMATION_IS_PARTIAL);
    if (auto third = state[2]; third.m_nGroupId1 && third.m_nAnimId) {
        if (/*third.m_nGroupId1 >= 0 &&*/ third.m_nAnimId != 3u) {
            if (auto animBlock = CAnimManager::GetAssocGroups()[third.m_nGroupId2].m_AnimBlock; animBlock && animBlock->IsLoaded) {
                anim = CAnimManager::BlendAnimation(ped->GetRpClump(), (AssocGroupId)third.m_nGroupId2, (AnimationId)third.m_nAnimId, 1000.0f);

                anim->SetCurrentTime(third.m_nTime * ANIM_TIME_DECOMPRESS_VALUE);
                anim->SetSpeed(third.m_nSpeed * ANIM_SPEED_DECOMPRESS_VALUE);
                anim->SetBlend(third.m_nGroupId1 * ANIM_BLEND_DECOMPRESS_VALUE, 0.0f); // no callback type store here (the exe ends after the blend delta)
            }
        }
    }

}

// 0x45C210
void CReplay::Display() {
    if (ShouldStandardCameraBeProcessed())
        return;

#ifdef NOTSA_STANDALONE_RUN
    // NOTSA-AUDIT: the exe (0x45C21E) blinks the text with a FRAME counter (0xA43240, 16 bit): shown while bit 5 is set = 32 frames on / 32 frames off
    NOTSA_GLOBAL_LOCAL(s_FrameCounter, 0xA43240, (uint32), {});
    s_FrameCounter = (s_FrameCounter + 1) & 0xFFFF;
    if (!(s_FrameCounter & 0x20))
        return;
#else
#line 252
    // NOTSA: FPS-independent 'REPLAY' text counter.
    static uint32 s_ReplayTextCounter = CTimer::GetTimeInMS();
    static bool s_ShowReplayText = true;
    if (CTimer::GetTimeInMS() - s_ReplayTextCounter >= 1000) {
        s_ReplayTextCounter = CTimer::GetTimeInMS();
        s_ShowReplayText = !s_ShowReplayText;
    }

    if (!s_ShowReplayText)
        return;
#endif
#line 262

    CFont::SetScale(SCREEN_SCALE_X(1.5f), SCREEN_SCALE_Y(1.5f));
    CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
    CFont::SetBackground(false, false);
    CFont::SetCentreSize(float(RsGlobal.maximumWidth - 20));
    CFont::SetProportional(true);
    CFont::SetColor({ 255u, 255u, 200u, 200u });
    CFont::SetFontStyle(eFontStyle::FONT_SUBTITLES);
    CFont::PrintString(float(RsGlobal.maximumWidth / 10), float(RsGlobal.maximumHeight / 15), TheText.Get("REPLAY"));
}

// 0x45D430
void CReplay::MarkEverythingAsNew() {
    for (auto& ped : NOTSA_EXE_OR(GetPedPool()->GetAllValidReverse(), GetPedPool()->GetAllValid())) {
        ped.bHasAlreadyBeenRecorded = false;
    }

    for (auto& veh : NOTSA_EXE_OR(GetVehiclePool()->GetAllValidReverse(), GetVehiclePool()->GetAllValid())) {
        veh.vehicleFlags.bHasAlreadyBeenRecorded = false;
    }
}

// 0x45EC90
void CReplay::EmptyReplayBuffer() {
    if (Mode == MODE_PLAYBACK)
        return;

    rng::fill(BufferStatus, REPLAYBUFFER_NOT_AVAILABLE);
    BufferStatus[0] = REPLAYBUFFER_IN_USE;
    Buffers[0].Write<tReplayEndBlock>(0u);
    Record.m_nOffset = 0u; // field by field like 0x45EC90: assigning a temporary also copied its 3 padding bytes (stack garbage) into the image
    Record.m_pBase   = &Buffers[0];
    Record.m_bSlot   = 0u;
    MarkEverythingAsNew();
}

// 0x45D390
void CReplay::EmptyPedsAndVehiclePools_NoDestructors() {
    for (auto& veh : NOTSA_EXE_OR(GetVehiclePool()->GetAllValidReverse(), GetVehiclePool()->GetAllValid())) {
        CWorld::Remove(&veh);
    }

    for (auto& ped : NOTSA_EXE_OR(GetPedPool()->GetAllValidReverse(), GetPedPool()->GetAllValid())) {
        CWorld::Remove(&ped);

        if (auto* const shadow = ped.m_pShadowData) {
            g_realTimeShadowMan.ReturnRealTimeShadow(shadow);
        }
    }
}

// 0x45E2A0
void CReplay::GoToNextBlock() {
    Record.Write<tReplayEndBlock>();
    BufferStatus[Record.m_bSlot] = REPLAYBUFFER_FULL;

    Record.Next();
    Record.Write<tReplayEndBlock>();
    BufferStatus[Record.m_bSlot] = REPLAYBUFFER_IN_USE;

    MarkEverythingAsNew();
}

// 0x45EBB0
void CReplay::RecordVehicleDeleted(CVehicle* vehicle) {
    if (Mode != MODE_RECORD)
        return;

    if (Record.m_nOffset + FindSizeOfPacket(REPLAY_PACKET_DELETED_VEH) > 99'984) {
        GoToNextBlock();
    }

    Record.Write<tReplayDeletedVehicleBlock>({.poolRef = (int16)GetVehiclePool()->GetIndex(vehicle)});
    Record.Write<tReplayEndBlock>();
}

// 0x45EC20
void CReplay::RecordPedDeleted(CPed* ped) {
    if (Mode != MODE_RECORD)
        return;

    if (Record.m_nOffset + FindSizeOfPacket(REPLAY_PACKET_DELETED_PED) > 99'984) {
        GoToNextBlock();
    }

    Record.Write<tReplayDeletedPedBlock>({.poolRef = (int16)GetPedPool()->GetIndex(ped)});
    Record.Write<tReplayEndBlock>();
}

// 0x45EFA0
void CReplay::InitialiseVehiclePoolConversionTable() {
    rng::fill(m_VehiclePoolConversion, -1);
    auto convIdx = 0u;
    for (auto poolIdx = 0u; poolIdx < GetVehiclePool()->GetSize(); poolIdx++) {
        if (GetVehiclePool()->GetAt(poolIdx))
            continue;

        if (!IsThisVehicleUsedInRecording(convIdx)) {
            while (++convIdx <= std::size(m_VehiclePoolConversion) && !IsThisVehicleUsedInRecording(convIdx))
                ;

            if (convIdx >= std::size(m_VehiclePoolConversion))
                break;
        }

        m_VehiclePoolConversion[convIdx++] = static_cast<int32>(poolIdx);
    }
}

// 0x45EF20
void CReplay::InitialisePedPoolConversionTable() {
    rng::fill(m_PedPoolConversion, -1);
    auto convIdx = 0u;
    for (auto poolIdx = 0u; poolIdx < GetPedPool()->GetSize(); poolIdx++) {
        if (GetPedPool()->GetAt(poolIdx))
            continue;

        if (!IsThisPedUsedInRecording(convIdx)) {
            while (++convIdx <= std::size(m_PedPoolConversion) && !IsThisPedUsedInRecording(convIdx))
                ;

            if (convIdx >= std::size(m_PedPoolConversion))
                break;
        }

        m_PedPoolConversion[convIdx++] = static_cast<int32>(poolIdx);
    }
}

// 0x45F370
void CReplay::InitialisePoolConversionTables() {
    InitialiseVehiclePoolConversionTable();
    InitialisePedPoolConversionTable();
}

// 0x45C340
void CReplay::SaveReplayToHD() {
    CFileMgr::SetDirMyDocuments();

    if (auto file = CFileMgr::OpenFileForWriting("replay.rep")) {
        CFileMgr::Write(file, "GtaSA29", 8u);

        // TODO: Refactor
        const auto NextSlot = [](uint8 slot) { return (slot + 1) % NUM_REPLAY_BUFFERS; };

        auto inUse = std::distance(BufferStatus.begin(), rng::find(BufferStatus, REPLAYBUFFER_IN_USE));
        auto slot = NextSlot(inUse);

        for (auto status = BufferStatus[slot]; status != REPLAYBUFFER_IN_USE; status = BufferStatus[slot]) {
            if (status == REPLAYBUFFER_FULL)
                break;
            slot = NextSlot(slot);
        }
        assert(0 <= slot && slot <= std::size(Buffers));
        CFileMgr::Write(file, Buffers[slot].buffer.data(), sizeof(tReplayBuffer));

        while (BufferStatus[slot] != REPLAYBUFFER_IN_USE) {
            slot = NextSlot(slot);
            CFileMgr::Write(file, Buffers[slot].buffer.data(), sizeof(tReplayBuffer));
        }
        CFileMgr::CloseFile(file);
    }
    CFileMgr::SetDir("");
}

// 0x460390
void CReplay::PlayReplayFromHD() {
    CFileMgr::SetDirMyDocuments();
    if (auto file = CFileMgr::OpenFile("replay.rep", "rb")) {
        CFileMgr::Read(file, gString, 8u);

        if (strncmp(gString, "GtaSA29", 8u) != 0) {
            NOTSA_LOG_DEBUG("Invalid replay file data, header unmatch (='{}')", std::string_view{gString, 8u});
        } else {
            auto bufferIdx = 0u;
            for (; bufferIdx < NUM_REPLAY_BUFFERS && CFileMgr::Read(file, Buffers[bufferIdx].buffer.data(), sizeof(tReplayBuffer)); bufferIdx++) {
                BufferStatus[bufferIdx] = REPLAYBUFFER_FULL;
            }
            assert(bufferIdx >= 1);
            BufferStatus[bufferIdx - 1] = REPLAYBUFFER_IN_USE; // Mark last used buffer as in-use.

            for (auto i = bufferIdx; i < NUM_REPLAY_BUFFERS; i++) { // Mark unfilled buffer as n/a.
                BufferStatus[i] = REPLAYBUFFER_NOT_AVAILABLE;
            }

            CFileMgr::CloseFile(file); // exe 0x460456: the file is closed BEFORE the dir reset / TriggerPlayback / streaming (the port closed it after them)
            file = nullptr;
            CFileMgr::SetDir(""); // FileMgr dir should be resetted to root before the TriggerPlayback call.
            TriggerPlayback(REPLAY_CAM_MODE_AS_STORED, CVector{}, false);
            bPlayingBackFromFile = true;
            bAllowLookAroundCam = true;
            StreamAllNecessaryCarsAndPeds();
        }
        if (file) {
            CFileMgr::CloseFile(file);
        }
    }
    CFileMgr::SetDir("");
}

// 0x45C440
bool CReplay::ShouldStandardCameraBeProcessed() {
    return Mode != MODE_PLAYBACK;
}

// inlined
// 0x45C450
int32 CReplay::FindPoolIndexForPed(int32 index) {
    return m_PedPoolConversion[index];
}

// inlined
// 0x45C460
int32 CReplay::FindPoolIndexForVehicle(int32 index) {
    return m_VehiclePoolConversion[index];
}

// 0x45CA70
void CReplay::ProcessPedUpdate(CPed* ped, float interpValue, CAddressInReplayBuffer& address) {
    assert(address.Read<tReplayBlockBase>().type == REPLAY_PACKET_PED_UPDATE);
    const auto& packet = address.Read<tReplayPedUpdateBlock>();

    address.m_nOffset += FindSizeOfPacket(REPLAY_PACKET_PED_UPDATE); // count packet read beforehand.
    if (!ped)
        return;

    // 0x45CABF: multiplied by the exe constant 0x859C44 (= 0.02453125f), not divided by HEADING_COMPRESS_VALUE (1/40.764328f is a different float)
    constexpr float HEADING_DECOMPRESS_VALUE = 0.02453125f;
    ped->m_fCurrentRotation = (float)packet.heading * HEADING_DECOMPRESS_VALUE;
    ped->m_fAimingRotation  = (float)packet.heading * HEADING_DECOMPRESS_VALUE;

    ped->GetMatrix() = Lerp(ped->GetMatrix(), CCompressedMatrixNotAligned::Decompress(packet.matrix), interpValue);

    if (const auto vehIdx = packet.vehicleIndex) {
        auto& vehicle = ped->m_pVehicle;
        CEntity::SafeCleanUpRef(ped->m_pVehicle);
        vehicle = nullptr;

        if (auto poolRef = FindPoolIndexForVehicle(vehIdx - 1); !GetVehiclePool()->IsFreeSlotAtIndex(poolRef)) {
            vehicle = GetVehiclePool()->GetAt(poolRef);
            vehicle->RegisterReference(vehicle);
            ped->bInVehicle = true;
        }
    } else {
        CEntity::SafeCleanUpRef(ped->m_pVehicle);
        ped->m_pVehicle = nullptr;
        ped->bInVehicle = false;
    }
    if (const auto ag = (int32)(int16)packet.animGroup; ag != ped->m_nAnimGroup) { // 0x45CBE0: movsx word
        ped->m_nAnimGroup = static_cast<AssocGroupId>(ag);
        if (ped == FindPlayerPed()) {
            ped->AsPlayer()->ReApplyMoveAnims();
        }
    }

    // 0x45CC07: entity flags bit 7 / 30 / 31 (see StorePedUpdate)
    ped->m_bIsVisible        = packet.flags.isVisible;
    ped->m_bTunnel           = packet.flags.tunnel;
    ped->m_bTunnelTransition = packet.flags.tunnelTransition;
    ped->m_fContactSurfaceBrightness = static_cast<float>(packet.contactSurfaceBrightness) * ExeRecip(100.0f);
    RetrievePedAnimation(ped, packet.animState);

    ped->RemoveWeaponModel();
    if (const auto wm = packet.weaponModel; wm != -1) {
        if (CStreaming::ms_aInfoForModel[wm].IsLoaded()) {
            ped->AddWeaponModel(wm);
        } else {
            CStreaming::RequestModel(static_cast<eModelID>(wm), STREAMING_DEFAULT);
        }
    }

    ped->SetAreaCode(static_cast<eAreaCodes>(CGame::currArea));
    CWorld::Remove(ped);
    CWorld::Add(ped);
}

// 0x45D060
void CReplay::ProcessReplayCamera() {
    assert(TheCamera.m_pRwCamera);

    auto modelling = TheCamera.GetRwMatrix();
    switch (CameraMode) {
    case REPLAY_CAM_MODE_TOPDOWN:
        TheCamera.GetRight()    = CVector{1.0f, 0.0f,  0.0f};
        TheCamera.GetUp()       = CVector{0.0f, 1.0f,  0.0f};
        TheCamera.GetForward()  = CVector{0.0f, 0.0f, -1.0f};
        TheCamera.GetPosition() = CVector{CameraFocus.x, CameraFocus.y, CameraFocus.z + 15.0f};

        // 0x45D1ED / 0x45D2D9: the RW frame gets the camera matrix (position included), not the focus point
        modelling->pos   = TheCamera.GetPosition();
        modelling->at    = TheCamera.GetForward();
        modelling->up    = TheCamera.GetUp();
        modelling->right = TheCamera.GetRight();
        break;
    case REPLAY_CAM_MODE_FIXED: {
        // 0x45D085-0x45D1B5: forward = normalise(focus - fixed); right = normalise(Z x forward) (note the operand order: Z x forward, 0x59C730 is out = 2nd x 3rd);
        // up = normalise(forward x right). All three go through CVector::Normalise (0x59C910), not NormaliseAndMag
        CVector direction = CameraFocus - CameraFixed;
        direction.Normalise();
        CVector right = CrossProduct(CVector{0.0f, 0.0f, 1.0f}, direction);
        right.Normalise();
        CVector up = CrossProduct(direction, right);
        up.Normalise();

        TheCamera.GetPosition() = CameraFixed;
        TheCamera.GetForward()  = direction;
        TheCamera.GetUp()       = up;
        TheCamera.GetRight()    = right;

        modelling->pos   = TheCamera.GetPosition();
        modelling->at    = TheCamera.GetForward();
        modelling->up    = TheCamera.GetUp();
        modelling->right = TheCamera.GetRight();
        break; }
    default:
        break;
    }
    TheCamera.m_vecGameCamPos = TheCamera.GetPosition();
    TheCamera.CalculateDerivedValues(false, true);
    RwMatrixUpdate(modelling);
    RwFrameUpdateObjects(RwCameraGetFrame(TheCamera.m_pRwCamera));
    RwFrameOrthoNormalize(RwCameraGetFrame(TheCamera.m_pRwCamera));
}

// 0x45D760
void CReplay::ProcessLookAroundCam() {
    if (bAllowLookAroundCam) {
        NOTSA_GLOBAL_LOCAL(playerCameraDistance, 0x97FAD4, (float), {});
        NOTSA_GLOBAL_LOCAL(playerCameraDirAngle, 0x97FADC, (float), {});
        NOTSA_GLOBAL_LOCAL(viewAngle, 0x97FAD8, (float), {});

        const auto& pad = CPad::GetPad();
        auto steer = pad->NewMouseControllerState.m_AmountMoved * ExeRecip(200.0f);
        // 0x45D7A4-0x45D7EC: the look-around cam is (re)armed when EITHER mouse axis moved by more than 0.01 (fabs: fchs when < 0), the port tested only y > 0.01
        // 0x45D7F2-0x45D8E1: the clamp [3, 15] is `!(d < 15) ? 15 : (d > 3 ? d : 3)` (jp after the 15.0 compare: NaN and 15 itself give 15), std::clamp let a NaN through
        constexpr auto ClampCamDistance = [](float d) { return !(d < 15.0f) ? 15.0f : (d > 3.0f ? d : 3.0f); };

        if (steer.x > 0.01f || steer.x < -0.01f || steer.y > 0.01f || steer.y < -0.01f) {
            if (!FramesActiveLookAroundCam) {
                const auto& camPos    = TheCamera.GetPosition();
                const auto  playerPos = FindPlayerCoors();
                // 0x45D81F-0x45D842: sqrt((dz*dz + dy*dy) + dx*dx)
                const x87d dx = playerPos.x - camPos.x, dy = playerPos.y - camPos.y, dz = playerPos.z - camPos.z;
                playerCameraDistance = ClampCamDistance((float)std::sqrt(dz * dz + dy * dy + dx * dx));
                playerCameraDirAngle = x87::atan2(camPos.x - FindPlayerCoors().x, camPos.y - FindPlayerCoors().y);
            }
            FramesActiveLookAroundCam = 60;
        }

        static NOTSA_GLOBAL_ALIAS(s_FrameActiveLookAroundCamReset, 0x97f66d, (bool), CReplay::bIsUsingRemoteCar);
        if (s_FrameActiveLookAroundCamReset) {
            FramesActiveLookAroundCam = 0;
        } else if (FramesActiveLookAroundCam) {
            playerCameraDirAngle += steer.x;
            FramesActiveLookAroundCam--;

            if (pad->NewMouseControllerState.isMouseLeftButtonPressed && pad->NewMouseControllerState.isMouseRightButtonPressed) {
                playerCameraDistance = ClampCamDistance(playerCameraDistance + 2.0f * steer.y);
            } else {
                viewAngle = std::clamp(viewAngle + steer.y, 0.1f, 1.5f); // probably some kind of cheap clamping between [0, pi/2].
            }

            auto target = CVector{
                    (float)(X87R(x87::sin(playerCameraDirAngle) * x87::cos(viewAngle)) * playerCameraDistance),
                    (float)(X87R(x87::cos(playerCameraDirAngle) * x87::cos(viewAngle)) * playerCameraDistance),
                    (float)(X87R(x87::sin(viewAngle) * playerCameraDistance))
            } + CameraFocus;

            CColPoint colPoint;
            CEntity* outEntity{};
            if (CWorld::ProcessLineOfSight(CameraFocus, target, colPoint, outEntity, true, false, false, false, false, true, true, false)) {
                target = colPoint.m_vecPoint;
                // 0x45DB39-0x45DB7A: all three components of the (CVector::Normalise'd) focus direction are scaled by 0.25 - the port left z unscaled
                CVector focusCollisionDir = CVector{ CameraFocus } - colPoint.m_vecPoint;
                focusCollisionDir.Normalise();
                target.x += focusCollisionDir.x * 0.25f;
                target.y += focusCollisionDir.y * 0.25f;
                target.z += focusCollisionDir.z * 0.25f;
            }

            // 0x45DB7E-0x45DC5E: forward, right and up are all CVector::Normalise'd (0x59C910); the port normalised only forward (and via NormaliseAndMag)
            CVector forward = CVector{ CameraFocus } - target;
            forward.Normalise();
            CVector right = CrossProduct(CVector{0.0f, 0.0f, 1.0f}, forward);
            right.Normalise();
            CVector up = CrossProduct(forward, right);
            up.Normalise();

            TheCamera.GetForward()  = forward;
            TheCamera.GetUp()       = up;
            TheCamera.GetRight()    = right;
            TheCamera.GetPosition() = target;

            auto modelling = TheCamera.GetRwMatrix();
            modelling->pos   = target;
            modelling->at    = forward;
            modelling->up    = up;
            modelling->right = right;
            TheCamera.CalculateDerivedValues(false, true);
            RwMatrixUpdate(modelling);
            RwFrameUpdateObjects(RwCameraGetFrame(TheCamera.m_pRwCamera));
            RwFrameOrthoNormalize(RwCameraGetFrame(TheCamera.m_pRwCamera));
        }
    }

    RwCameraSetNearClipPlane(Scene.m_pRwCamera, 0.5f);
    if (FramesActiveLookAroundCam) {
        const auto MakeVisible = [](auto* entity) {
            if (entity) {
                entity->SetIsVisible(true);
            }
        };

        MakeVisible(FindPlayerPed());
        MakeVisible(FindPlayerVehicle());
    }
}

// inlined
// 0x45C470
bool CReplay::CanWeFindPoolIndexForPed(int32 index) {
    return FindPoolIndexForPed(index) >= 0;
}

// inlined
// 0x45C490
bool CReplay::CanWeFindPoolIndexForVehicle(int32 index) {
    return FindPoolIndexForVehicle(index) >= 0;
}

// 0x45F020
void CReplay::StorePlayerInfoVariables() {
    auto& playerInfo = FindPlayerInfo(PED_TYPE_PLAYER1);

    PlayerInfo = playerInfo;
    playerInfo.m_PlayerData.m_pWanted = nullptr;
    playerInfo.m_PlayerData.m_pPedClothesDesc = nullptr;
    playerInfo.m_PlayerData.m_pArrestingCop = nullptr;
    playerInfo.m_PlayerData.m_nMeleeWeaponAnimReferenced = 0;
    playerInfo.m_PlayerData.m_nMeleeWeaponAnimReferencedExtra = 0;
    playerInfo.m_pSkinTexture = nullptr;
}

// 0x45E1F0
void CReplay::RestorePlayerInfoVariables() {
    FindPlayerInfo(PED_TYPE_PLAYER1) = PlayerInfo;
    PlayerInfo.m_PlayerData.m_pWanted = nullptr;
    PlayerInfo.m_PlayerData.m_pPedClothesDesc = nullptr;
    PlayerInfo.m_PlayerData.m_pArrestingCop = nullptr;
    PlayerInfo.m_pSkinTexture = nullptr;
}

// 0x45F180
void CReplay::StoreStuffInMem() {
    pStoredCam = new CCamera;
    memcpy(pStoredCam, &TheCamera, sizeof(CCamera));

    StorePlayerInfoVariables();
    Time1 = CTimer::GetTimeInMS();
    Time2 = CTimer::GetTimeInMSNonClipped();
    Time3 = CTimer::GetPreviousTimeInMS();
    Time4 = CTimer::m_snPPreviousTimeInMilliseconds;
    Time5 = CTimer::m_snPPPreviousTimeInMilliseconds;
    Time6 = CTimer::m_snPPPPreviousTimeInMilliseconds;
    Time7 = CTimer::GetTimeInMSPauseMode();
    Frame = CTimer::GetFrameCounter();
    TimeScale = CTimer::GetTimeScale();
    TimeStep = CTimer::GetTimeStep();
    TimeStepNonClipped = CTimer::GetTimeStepNonClipped();

    ClockHours = CClock::GetGameClockHours();
    ClockMinutes = CClock::GetGameClockMinutes();

    OldWeatherType = CWeather::OldWeatherType;
    NewWeatherType = CWeather::NewWeatherType;
    WeatherInterpolationValue = CWeather::InterpolationValue;

    ms_nNumCivMale_Stored = CPopulation::ms_nNumCivMale;
    ms_nNumCivFemale_Stored = CPopulation::ms_nNumCivFemale;
    ms_nNumCop_Stored = CPopulation::ms_nNumCop;
    ms_nNumEmergency_Stored = CPopulation::ms_nNumEmergency;
    ms_nNumDealers_Stored = CPopulation::ms_nNumDealers;
    ms_nTotalCarPassengerPeds_Stored = CPopulation::ms_nTotalCarPassengerPeds;
    ms_nTotalCivPeds_Stored = CPopulation::ms_nTotalCivPeds;
    ms_nTotalGangPeds_Stored = CPopulation::ms_nTotalGangPeds;
    ms_nTotalPeds_Stored = CPopulation::ms_nTotalPeds;
    ms_nTotalMissionPeds_Stored = CPopulation::ms_nTotalMissionPeds;
    rng::copy(CPopulation::ms_nNumGang, ms_nNumGang_Stored.begin());
}

// 0x45ECD0
void CReplay::RestoreStuffFromMem() {
    for (auto& ped : GetPedPool()->GetAllValid()) {
        if (ped.bUsedForReplay) {
            if (auto playerData = ped.GetPlayerData()) {
                playerData->DeAllocateData();
            }
            CWorld::Remove(&ped);
            delete &ped;
        } else { // if not, restore it
            CWorld::Add(&ped);
        }
    }

    for (auto& veh : GetVehiclePool()->GetAllValid()) {
        if (veh.vehicleFlags.bUsedForReplay) {
            CWorld::Remove(&veh);
            delete &veh;
        } else { // if not, restore it
            CWorld::Add(&veh);
        }
    }

    memcpy(&TheCamera, pStoredCam, sizeof(TheCamera));
    delete pStoredCam;
    pStoredCam = nullptr;

    CTimer::SetTimeInMS(Time1);
    CTimer::SetTimeInMSNonClipped(Time2);
    CTimer::SetPreviousTimeInMS(Time3);
    CTimer::m_snPreviousTimeInMilliseconds = Time3;
    CTimer::m_snPPreviousTimeInMilliseconds = Time4;
    CTimer::m_snPPPreviousTimeInMilliseconds = Time5;
    CTimer::m_snPPPPreviousTimeInMilliseconds = Time6;
    CTimer::SetTimeInMSPauseMode(Time7);
    CTimer::SetTimeStep(TimeStep);
    CTimer::SetTimeStepNonClipped(TimeStepNonClipped);
    CTimer::SetTimeScale(TimeScale);
    CTimer::SetFrameCounter(Frame);
    CClock::SetGameClock(ClockHours, ++ClockMinutes, 0);

    RestorePlayerInfoVariables();
    CWeather::OldWeatherType = OldWeatherType;
    CWeather::NewWeatherType = NewWeatherType;
    CWeather::InterpolationValue = WeatherInterpolationValue;

    CGame::currArea = static_cast<eAreaCodes>(CurrArea);
    CPopulation::ms_nNumCivMale = ms_nNumCivMale_Stored;
    CPopulation::ms_nNumCivFemale = ms_nNumCivFemale_Stored;
    CPopulation::ms_nNumCop = ms_nNumCop_Stored;
    CPopulation::ms_nNumEmergency = ms_nNumEmergency_Stored;
    CPopulation::ms_nTotalCarPassengerPeds = ms_nTotalCarPassengerPeds_Stored;
    CPopulation::ms_nTotalCivPeds = ms_nTotalCivPeds_Stored;
    CPopulation::ms_nTotalGangPeds = ms_nTotalGangPeds_Stored;
    CPopulation::ms_nTotalPeds = ms_nTotalPeds_Stored;
    CPopulation::ms_nTotalMissionPeds = ms_nTotalMissionPeds_Stored;

#ifdef FIX_BUGS
    // FIX_BUGS: ms_nNumGang not restored (0x45ECD0: the exe copies back only the scalar counters, never 0x97FA80 -> 0xC0EC3C)
    rng::copy(ms_nNumGang_Stored, CPopulation::ms_nNumGang.begin());
#endif
}

// 0x45F050
void CReplay::FinishPlayback() {
    if (ShouldStandardCameraBeProcessed())
        return;

    RestoreStuffFromMem();
    Mode = MODE_RECORD;

    if (bDoLoadSceneWhenDone) {
        CGame::currLevel = CTheZones::GetLevelFromPosition(LoadScene);
        CCollision::SortOutCollisionAfterLoad();
        CStreaming::LoadScene(LoadScene);
    }
    bDoLoadSceneWhenDone = false;

    if (bPlayingBackFromFile) {
        Init();
        MarkEverythingAsNew();
    }

    TheCamera.SetFadeColour(0, 0, 0);
    TheCamera.Fade(0.0f, eFadeFlag::FADE_IN);
    TheCamera.ProcessFade();
    TheCamera.Fade(1.0f, eFadeFlag::FADE_OUT);

    gFireManager.CreateAllFxSystems();
    AudioEngine.SetEffectsMasterVolume(FrontEndMenuManager.m_nSfxVolume);
    AudioEngine.SetMusicMasterVolume(FrontEndMenuManager.m_nRadioVolume);

    if (auto vehicle = FindPlayerVehicle()) {
        vehicle->m_vehicleAudio.TurnOnRadioForVehicle();
        AudioEngine.RetuneRadio(OldRadioStation);
    }
}

// 0x45E300
void CReplay::RecordThisFrame() {
    // Calculate the frame size beforehand.
    auto framePacketSize = 116u;
    for (const auto& veh : GetVehiclePool()->GetAllValid()) {
        if (veh.GetRwObject()) {
            switch (veh.m_nVehicleSubType) {
            case VEHICLE_TYPE_AUTOMOBILE:
            case VEHICLE_TYPE_MTRUCK:
            case VEHICLE_TYPE_QUAD:
            case VEHICLE_TYPE_BOAT:
            case VEHICLE_TYPE_TRAILER:
                framePacketSize += FindSizeOfPacket(REPLAY_PACKET_VEHICLE);
                break;
            case VEHICLE_TYPE_HELI:
            case VEHICLE_TYPE_BIKE:
            case VEHICLE_TYPE_BMX:
                framePacketSize += 56; // REPLAY_PACKET_BIKE/BMX/HELI
                break;
            case VEHICLE_TYPE_PLANE:
                framePacketSize += FindSizeOfPacket(REPLAY_PACKET_PLANE);
                break;
            case VEHICLE_TYPE_TRAIN:
                framePacketSize += FindSizeOfPacket(REPLAY_PACKET_TRAIN);
                break;
            default:
                break;
            }
        }
    }

    for (const auto& ped : GetPedPool()->GetAllValid()) {
        if (ped.GetRwObject()) {
            if (!ped.bHasAlreadyBeenRecorded) {
                // New ped!
                framePacketSize += FindSizeOfPacket(REPLAY_PACKET_PED_HEADER);
                if (ped.IsPlayer()) {
                    framePacketSize += FindSizeOfPacket(REPLAY_PACKET_CLOTHES);
                }
            }
            framePacketSize += FindSizeOfPacket(REPLAY_PACKET_PED_UPDATE);
        }
    }

    for (auto& trace : CBulletTraces::aTraces) {
        if (trace.m_bExists) {
            framePacketSize += FindSizeOfPacket(REPLAY_PACKET_BULLET_TRACES);
        }
    }

    if (Record.m_nOffset + framePacketSize + 20 > 99'984) {
        // writing the frame will overflow the current buffer, switch to next.
        GoToNextBlock();
    }

    auto cameraPacket = tReplayCameraBlock{
        .isUsingRemoteVehicle = FindPlayerInfo().m_pRemoteVehicle != nullptr,
        .matrix               = 0 /* to be filled */,
        .firstFocusPosn       = FindPlayerCoors()
    };
#ifdef NOTSA_PORTABLE
    cameraPacket.WriteMatrix(TheCamera.GetMatrix());
#else
    cameraPacket.GetMatrix() = TheCamera.GetMatrix();
#endif
    Record.Write(cameraPacket);

    Record.Write<tReplayClockBlock>({
        .currentHour = CClock::GetGameClockHours(), .currentMinute = CClock::GetGameClockMinutes()
    });

    Record.Write<tReplayWeatherBlock>({
        .oldWeather  = (uint8)CWeather::OldWeatherType,
        .newWeather  = (uint8)CWeather::NewWeatherType,
        .interpValue = CWeather::InterpolationValue
    });

    Record.Write<tReplayTimerBlock>({.timeInMS = CTimer::GetTimeInMS()});

    for (auto&& [i, veh] : GetVehiclePool()->GetAllValidWithIndex()) {
        if (veh.GetRwObject()) {
            switch (veh.m_nVehicleSubType) {
            case VEHICLE_TYPE_AUTOMOBILE:
            case VEHICLE_TYPE_MTRUCK:
            case VEHICLE_TYPE_QUAD:
            case VEHICLE_TYPE_BOAT:
            case VEHICLE_TYPE_TRAILER:
                Record.Write(tReplayVehicleBlock::MakeVehicleUpdateData(veh, i));
                break;
            case VEHICLE_TYPE_HELI: {
                tReplayHeliBlock packet{};
                *packet.As<tReplayVehicleBlock>() = tReplayVehicleBlock::MakeVehicleUpdateData(veh, i);

                packet.type = REPLAY_PACKET_HELI;
                packet.rotorSpeed = veh.AsHeli()->m_fHeliRotorSpeed;
                Record.Write(packet);
                break;
            }
            case VEHICLE_TYPE_PLANE: {
                tReplayPlaneBlock packet{};
                *packet.As<tReplayVehicleBlock>() = tReplayVehicleBlock::MakeVehicleUpdateData(veh, i);

                packet.type = REPLAY_PACKET_PLANE;
                packet.field_9C8 = veh.AsPlane()->field_9C8;
                packet.propSpeed = veh.AsPlane()->m_fPropSpeed;
                Record.Write(packet);
                break;
            }
            case VEHICLE_TYPE_TRAIN:
                Record.Write(tReplayTrainBlock::MakeTrainUpdateData(*veh.AsTrain(), i));
                break;

            case VEHICLE_TYPE_BIKE:
                Record.Write(tReplayBikeBlock::MakeBikeUpdateData(*veh.AsBike(), i));
                break;

            case VEHICLE_TYPE_BMX:
                Record.Write(tReplayBmxBlock::MakeBmxUpdateData(*veh.AsBmx(), i));
                break;
            default:
                break;
            }
        }
    }

    for (auto&& [i, ped] : GetPedPool()->GetAllValidWithIndex()) {
        if (ped.GetRwObject()) {
            if (!ped.bHasAlreadyBeenRecorded) {
                // New ped!
                const auto modelId = ped.m_nModelIndex;

                Record.Write<tReplayPedHeaderBlock>({
                    .poolRef = (uint8)i,
                    .modelId = (int16)((modelId >= MODEL_SPECIAL01 && modelId <= MODEL_CUTOBJ01) ? MODEL_MALE01 : modelId),
                    .pedType = (uint8)ped.m_nPedType
                });

                if (ped.IsPlayer()) {
                    tReplayClothesBlock clothesData{};
                    StoreClothesDesc(*ped.AsPlayer()->GetClothesDesc(), clothesData);
                    Record.Write(clothesData);
                }

                ped.bHasAlreadyBeenRecorded = true;
            }
            StorePedUpdate(&ped, i);
        }
    }

    for (auto&& [i, trace] : rngv::enumerate(CBulletTraces::aTraces)) {
        if (!trace.m_bExists)
            continue;

        tReplayBulletTraceBlock packet{};
        packet.type = REPLAY_PACKET_BULLET_TRACES;
        packet.index = i;
        packet.start = trace.m_vecStart;
        packet.end = trace.m_vecEnd;

        Record.Write(packet);
    }

    Record.Write<tReplayMiscBlock>({
        .camShakeStart = TheCamera.m_nCamShakeStart,
        .camShakeForce = TheCamera.m_fCamShakeForce,
        .currArea      = (uint8)CGame::currArea,
        .camConfig     = {
            .videoCam = CSpecialFX::bVideoCam,
            .liftCam = CSpecialFX::bLiftCam
        }
    });

    Record.Write<tReplayEOFBlock>();
    Record.Write<tReplayEndBlock>();
}

// 0x45C750
void CReplay::StoreClothesDesc(const CPedClothesDesc& desc, tReplayClothesBlock& packet) {
    packet.type = REPLAY_PACKET_CLOTHES;
    rng::copy(desc.m_anModelKeys, packet.modelKeys.begin());
    rng::copy(desc.m_anTextureKeys, packet.textureKeys.begin());
    packet.fatStat = NOTSA_FTOL(int16, desc.m_fFatStat);
    packet.muscleStat = NOTSA_FTOL(int16, desc.m_fMuscleStat);
}

// 0x45C7D0
void CReplay::RestoreClothesDesc(CPedClothesDesc& desc, tReplayClothesBlock& packet) {
    assert(packet.type == REPLAY_PACKET_CLOTHES);
    rng::copy(packet.modelKeys, desc.m_anModelKeys.begin());
    rng::copy(packet.textureKeys, desc.m_anTextureKeys.begin());
    desc.m_fFatStat = (float)(int16)packet.fatStat; // 0x45C7D0 sign-extends (movsx)
    desc.m_fMuscleStat = (float)(int16)packet.muscleStat;
}

// 0x45CEA0
CPed* CReplay::DealWithNewPedPacket(const tReplayPedHeaderBlock& pedPacket, bool loadModel, tReplayClothesBlock* clothesPacket) {
    assert(pedPacket.type == REPLAY_PACKET_PED_HEADER);

    if (GetPedPool()->GetAt(FindPoolIndexForPed(pedPacket.poolRef)))
        return nullptr;

    if (pedPacket.pedType == PED_TYPE_PLAYER1 && loadModel) {
        CStreaming::RequestModel(pedPacket.modelId, STREAMING_DEFAULT);
        CStreaming::LoadAllRequestedModels(false);
    }

    if (CStreaming::GetInfo(pedPacket.modelId).IsLoaded()) {
        CPed* ped = nullptr;
        if (pedPacket.pedType == PED_TYPE_PLAYER1) {
            assert(clothesPacket && clothesPacket->type == REPLAY_PACKET_CLOTHES);

            ped = new (FindPoolIndexForPed(pedPacket.poolRef) << 8) CPlayerPed(PED_TYPE_PLAYER1, true);
            RestoreClothesDesc(*ped->GetClothesDesc(), *clothesPacket);
            CClothes::RebuildPlayer(static_cast<CPlayerPed*>(ped), true);
        } else if (!loadModel) {
            ped = new (FindPoolIndexForPed(pedPacket.poolRef) << 8) CCivilianPed((ePedType)pedPacket.pedType, pedPacket.modelId);
        } else {
            return nullptr;
        }

        ped->SetStatus(STATUS_PLAYER_PLAYBACK_FROM_BUFFER);
        ped->bUsedForReplay = true;
        ped->GetMatrix().SetUnity();
        ped->SetCharCreatedBy(PED_GAME_MISSION);
        CWorld::Remove(ped);
        CWorld::Add(ped);
        return ped;
    } else {
        CStreaming::RequestModel(pedPacket.modelId, STREAMING_DEFAULT);
    }
    return nullptr;
}

// Returns true if the playback has ended.
// 0x45F380
bool CReplay::PlayBackThisFrameInterpolation(CAddressInReplayBuffer& buffer, float interpolation, uint32* outTimer) {
    const auto SetupVehicle = [](const tReplayVehicleBlock& packet, CVehicle* vehicle) {
        vehicle->SetStatus(STATUS_PLAYER_PLAYBACK_FROM_BUFFER);
        vehicle->vehicleFlags.bUsedForReplay = true;
        vehicle->SetUsesCollision(false);
        packet.matrix.DecompressIntoFullMatrix(vehicle->GetMatrix());
        vehicle->m_nPrimaryColor = packet.primaryColor;
        vehicle->m_nSecondaryColor = packet.secondaryColor;
        if (vehicle->IsSubBoat()) {
            vehicle->AsBoat()->m_nBoatFlags.bLockedToXY = false;
        }

        CWorld::Add(vehicle);
    };

    CBulletTraces::Init();

    while (buffer.Read<tReplayBlockBase>().type != REPLAY_PACKET_END_OF_FRAME) {
        auto& packet = buffer.Read<tReplayBlockBase>();

        switch (packet.type) {
        case REPLAY_PACKET_END:
            if (buffer.GetStatus() == REPLAYBUFFER_IN_USE) {
                FinishPlayback();
                return true;
            }
            buffer.Next();
            continue; // We do not increment when we read the end packet.
        case REPLAY_PACKET_VEHICLE: {
            auto vehiclePacket = buffer.Read<tReplayVehicleBlock>();
            const auto modelId = vehiclePacket.modelId;
            const auto poolIdx = FindPoolIndexForVehicle(vehiclePacket.poolRef);

            if (!GetVehiclePool()->GetAt(poolIdx)) {
                if (CStreaming::IsModelLoaded(modelId)) {
                    auto created = [&]() -> CVehicle* {
                        switch (vehiclePacket.vehicleSubType) {
                        case VEHICLE_TYPE_AUTOMOBILE:
                            return new (poolIdx << 8) CAutomobile(modelId, MISSION_VEHICLE, true);
                        case VEHICLE_TYPE_MTRUCK:
                            return new (poolIdx << 8) CMonsterTruck(modelId, MISSION_VEHICLE);
                        case VEHICLE_TYPE_QUAD:
                            return new (poolIdx << 8) CQuadBike(modelId, MISSION_VEHICLE);
                        case VEHICLE_TYPE_BOAT:
                            return new (poolIdx << 8) CBoat(modelId, MISSION_VEHICLE);
                        case VEHICLE_TYPE_TRAIN:
                            return new (poolIdx << 8) CTrain(modelId, MISSION_VEHICLE);
                        case VEHICLE_TYPE_TRAILER:
                            return new (poolIdx << 8) CTrailer(modelId, MISSION_VEHICLE);
                        default:
                            NOTSA_UNREACHABLE_RET(nullptr, "Unknown vehicle subtype ={}", (int)vehiclePacket.vehicleSubType);
                        }
                    }();

                    SetupVehicle(vehiclePacket, created);
                } else {
                    CStreaming::RequestModel(modelId, STREAMING_DEFAULT);
                }
            }
            // 0x45F5F0 (all six vehicle packets): the slot is looked up AGAIN after the create / request step, so a just created vehicle gets the update (with `interpolation`) in the same call
            if (auto* const vehicle = GetVehiclePool()->GetAt(poolIdx)) {
                vehiclePacket.ExtractVehicleUpdateData(*vehicle, interpolation);
            }
            break;
        }
        case REPLAY_PACKET_BIKE: {
            auto bikePacket = buffer.Read<tReplayBikeBlock>();
            const auto modelId = bikePacket.modelId;
            const auto poolIdx = FindPoolIndexForVehicle(bikePacket.poolRef);

            if (!GetVehiclePool()->GetAt(poolIdx)) {
                if (CStreaming::IsModelLoaded(modelId)) {
                    SetupVehicle(bikePacket, new (poolIdx << 8) CBike(modelId, MISSION_VEHICLE));
                } else {
                    CStreaming::RequestModel(modelId, STREAMING_DEFAULT);
                }
            }
            if (auto* const vehicle = GetVehiclePool()->GetAt(poolIdx)) {
                bikePacket.ExtractBikeUpdateData(*vehicle->AsBike(), interpolation);
            }
            break;
        }
        case REPLAY_PACKET_PED_HEADER:
            DealWithNewPedPacket(buffer.Read<tReplayPedHeaderBlock>(), false, nullptr);
            break;
        case REPLAY_PACKET_PED_UPDATE: {
            const auto poolIdx = FindPoolIndexForPed(buffer.Read<tReplayPedUpdateBlock>().poolRef);
            ProcessPedUpdate(GetPedPool()->GetAt(poolIdx), interpolation, buffer);
            continue; // ProcessPedUpdate increments the offset by itself already.
        }
        case REPLAY_PACKET_GENERAL: {
            auto cameraPacket = buffer.Read<tReplayCameraBlock>();

#ifdef NOTSA_PORTABLE
            TheCamera.GetMatrix() = Lerp(TheCamera.GetMatrix(), cameraPacket.ReadMatrix(), interpolation);
#else
            TheCamera.GetMatrix() = Lerp(TheCamera.GetMatrix(), cameraPacket.GetMatrix(), interpolation);
#endif
            auto modelling = TheCamera.GetRwMatrix();
            modelling->pos = TheCamera.GetMatrix().GetPosition();
            modelling->at = TheCamera.GetMatrix().GetForward();
            modelling->up = TheCamera.GetMatrix().GetUp();
            modelling->right = TheCamera.GetMatrix().GetRight();

            CameraFocus = cameraPacket.firstFocusPosn * interpolation + CVector{ CameraFocus } * (1.0f - interpolation);
            bIsUsingRemoteCar = cameraPacket.isUsingRemoteVehicle;
            break;
        }
        case REPLAY_PACKET_CLOCK: {
            auto clockPacket = buffer.Read<tReplayClockBlock>();
            CClock::SetGameClock(clockPacket.currentHour, clockPacket.currentMinute, 0);
            break;
        }
        case REPLAY_PACKET_WEATHER: {
            auto weatherPacket = buffer.Read<tReplayWeatherBlock>();
            CWeather::OldWeatherType = (eWeatherType)weatherPacket.oldWeather;
            CWeather::NewWeatherType = (eWeatherType)weatherPacket.newWeather;
            CWeather::InterpolationValue = weatherPacket.interpValue;
            break;
        }
        case REPLAY_PACKET_END_OF_FRAME:
            break;
        case REPLAY_PACKET_TIMER: {
            auto timeInMS = buffer.Read<tReplayTimerBlock>().timeInMS;
            if (outTimer) {
                *outTimer = timeInMS;
            }

            CTimer::SetTimeInMS(timeInMS);
            break;
        }
        case REPLAY_PACKET_BULLET_TRACES: {
            auto bulletTracePacket = buffer.Read<tReplayBulletTraceBlock>();

            auto& trace = CBulletTraces::aTraces[bulletTracePacket.index];
            trace.m_bExists = true;
            trace.m_vecStart = bulletTracePacket.start;
            trace.m_vecEnd = bulletTracePacket.end;
            break;
        }
        case REPLAY_PACKET_MISC: {
            auto miscPacket = buffer.Read<tReplayMiscBlock>();

            TheCamera.m_nCamShakeStart = miscPacket.camShakeStart;
            TheCamera.m_fCamShakeForce = miscPacket.camShakeForce;
            CSpecialFX::bVideoCam = miscPacket.camConfig.videoCam;
            CSpecialFX::bLiftCam = miscPacket.camConfig.liftCam;
            CGame::currArea = static_cast<eAreaCodes>(miscPacket.currArea);
            break;
        }
        case REPLAY_PACKET_DELETED_VEH: {
            auto deletedVehPacket = buffer.Read<tReplayDeletedVehicleBlock>();
            if (!CanWeFindPoolIndexForVehicle(deletedVehPacket.poolRef))
                break;

            const auto idx = FindPoolIndexForVehicle(deletedVehPacket.poolRef);
            if (auto vehicle = GetVehiclePool()->GetAt(idx)) {
                CWorld::Remove(vehicle);
                delete vehicle;
            }
            break;
        }
        case REPLAY_PACKET_DELETED_PED: {
            auto deletedPedPacket = buffer.Read<tReplayDeletedPedBlock>();
            if (!CanWeFindPoolIndexForPed(deletedPedPacket.poolRef))
                break;

            const auto idx = FindPoolIndexForPed(deletedPedPacket.poolRef);
            if (auto ped = GetPedPool()->GetAt(idx)) {
                CWorld::Remove(ped);
                delete ped;
            }
            break;
        }
        case REPLAY_PACKET_BMX: {
            auto bmxPacket = buffer.Read<tReplayBmxBlock>();
            const auto modelId = bmxPacket.modelId;
            const auto poolIdx = FindPoolIndexForVehicle(bmxPacket.poolRef);

            if (!GetVehiclePool()->GetAt(poolIdx)) {
                if (CStreaming::IsModelLoaded(modelId)) {
                    SetupVehicle(bmxPacket, new (poolIdx << 8) CBmx(modelId, MISSION_VEHICLE));
                } else {
                    CStreaming::RequestModel(modelId, STREAMING_DEFAULT);
                }
            }
            if (auto* const vehicle = GetVehiclePool()->GetAt(poolIdx)) {
                bmxPacket.As<tReplayBmxBlock>()->ExtractBmxUpdateData(*vehicle->AsBmx(), interpolation);
            }
            break;
        }
        case REPLAY_PACKET_HELI: {
            auto heliPacket = buffer.Read<tReplayHeliBlock>();
            const auto modelId = heliPacket.modelId;
            const auto poolIdx = FindPoolIndexForVehicle(heliPacket.poolRef);

            if (!GetVehiclePool()->GetAt(poolIdx)) {
                if (CStreaming::IsModelLoaded(modelId)) {
                    SetupVehicle(heliPacket, new (poolIdx << 8) CHeli(modelId, MISSION_VEHICLE));
                } else {
                    CStreaming::RequestModel(modelId, STREAMING_DEFAULT);
                }
            }
            if (auto* const vehicle = GetVehiclePool()->GetAt(poolIdx)) {
                heliPacket.ExtractVehicleUpdateData(*vehicle, interpolation);
                vehicle->AsHeli()->m_fHeliRotorSpeed = heliPacket.rotorSpeed;
            }
            break;
        }
        case REPLAY_PACKET_PLANE: {
            auto planePacket = buffer.Read<tReplayPlaneBlock>();
            const auto modelId = planePacket.modelId;
            const auto poolIdx = FindPoolIndexForVehicle(planePacket.poolRef);

            if (!GetVehiclePool()->GetAt(poolIdx)) {
                if (CStreaming::IsModelLoaded(modelId)) {
                    SetupVehicle(planePacket, new (poolIdx << 8) CPlane(modelId, MISSION_VEHICLE));
                } else {
                    CStreaming::RequestModel(modelId, STREAMING_DEFAULT);
                }
            }
            if (auto* const vehicle = GetVehiclePool()->GetAt(poolIdx)) {
                planePacket.ExtractVehicleUpdateData(*vehicle, interpolation);
                vehicle->AsPlane()->field_9C8 = planePacket.field_9C8;
                vehicle->AsPlane()->m_fPropSpeed = planePacket.propSpeed;
            }
            break;
        }
        case REPLAY_PACKET_TRAIN: {
            auto trainPacket = buffer.Read<tReplayTrainBlock>();
            const auto modelId = trainPacket.modelId;
            const auto poolIdx = FindPoolIndexForVehicle(trainPacket.poolRef);

            if (!GetVehiclePool()->GetAt(poolIdx)) {
                if (CStreaming::IsModelLoaded(modelId)) {
                    SetupVehicle(trainPacket, new (poolIdx << 8) CTrain(modelId, MISSION_VEHICLE));
                } else {
                    CStreaming::RequestModel(modelId, STREAMING_DEFAULT);
                }
            }
            if (auto* const vehicle = GetVehiclePool()->GetAt(poolIdx)) {
                trainPacket.ExtractTrainUpdateData(*vehicle->AsTrain(), interpolation);
            }
            break;
        }
        case REPLAY_PACKET_CLOTHES:
            break;
        default:
            NOTSA_UNREACHABLE("Invalid packet read (type ID={})", (int32)packet.type);
        }

        buffer.m_nOffset += FindSizeOfPacket(packet.type);
    }
    buffer.m_nOffset += 4u;
    ProcessReplayCamera();

    return false;
}

// 0x460350
bool CReplay::FastForwardToTime(uint32 start) {
    if (!start)
        return true;

    uint32 timer = 0;
    while (!PlayBackThisFrameInterpolation(Playback, 1.0f, &timer)) {
        if (timer >= start) {
            return true;
        }
    }

    return false;
}

// 0x4604A0
void CReplay::PlayBackThisFrame() {
    if (PlayBackThisFrameInterpolation(Playback, 1.0f, nullptr)) {
        AudioEngine.SetEffectsMasterVolume(FrontEndMenuManager.m_nSfxVolume);
        AudioEngine.SetMusicMasterVolume(FrontEndMenuManager.m_nRadioVolume);
    } else {
        ProcessLookAroundCam();
        AudioEngine.SetEffectsMasterVolume(0);
        AudioEngine.SetMusicMasterVolume(0);
    }
}

// 0x45C850
constexpr uint32 CReplay::FindSizeOfPacket(eReplayPacket type) {
    switch (type) {
    case REPLAY_PACKET_END:          // <- actually 1 but we shouldn't call this func with this anyways so it's no problem.
    case REPLAY_PACKET_END_OF_FRAME:
    case REPLAY_PACKET_CLOCK:
    case REPLAY_PACKET_DELETED_VEH:
    case REPLAY_PACKET_DELETED_PED:
        return 4;
    case REPLAY_PACKET_VEHICLE:
    case REPLAY_PACKET_PED_UPDATE:
        return 52;
    case REPLAY_PACKET_BIKE:
    case REPLAY_PACKET_BMX:
    case REPLAY_PACKET_HELI:
        return 56;
    case REPLAY_PACKET_PED_HEADER:
    case REPLAY_PACKET_WEATHER:
    case REPLAY_PACKET_TIMER:
        return 8;
    case REPLAY_PACKET_GENERAL:
        return 88;
    case REPLAY_PACKET_BULLET_TRACES:
        return 28;
    case REPLAY_PACKET_PARTICLE:
        return 20;
    case REPLAY_PACKET_MISC:
        return 16;
    case REPLAY_PACKET_PLANE:
        return 60;
    case REPLAY_PACKET_TRAIN:
        return 76;
    case REPLAY_PACKET_CLOTHES:
        return 120;
    default:
        return 0;
    }
}

// 0x45DE40
bool CReplay::IsThisVehicleUsedInRecording(int32 index) {
    for (auto& buffer : GetAllActiveBuffers()) {
        for (auto& packet : buffer) {
            switch (packet.type) {
            case REPLAY_PACKET_VEHICLE:
            case REPLAY_PACKET_BIKE:
            case REPLAY_PACKET_BMX:
            case REPLAY_PACKET_HELI:
            case REPLAY_PACKET_PLANE:
            case REPLAY_PACKET_TRAIN:
                if (packet.As<tReplayVehicleBlock>()->poolRef == index) {
                    return true;
                }
                break;
            case REPLAY_PACKET_PED_UPDATE:
                if (auto pedIdx = packet.As<tReplayPedUpdateBlock>()->vehicleIndex; pedIdx && pedIdx - 1 == index) {
                    return true;
                }
                break;
            default:
                break;
            }
        }
    }
    return false;
}

// 0x45DDE0
bool CReplay::IsThisPedUsedInRecording(int32 index) {
    for (auto& buffer : GetAllActiveBuffers()) {
        const auto packet = rng::find_if(buffer, [index](auto&& p) {
            return p.type == REPLAY_PACKET_PED_HEADER && p.As<tReplayPedHeaderBlock>()->poolRef == index;
        });

        if (packet != buffer.end()) {
            return true;
        }
    }
    return false;
}

// 0x45D6C0
void CReplay::FindFirstFocusCoordinate(CVector& outPos) {
    outPos = CVector{}; // 0x45D6C0 clears the result first
    for (auto& buffer : GetAllActiveBuffers()) {
        const auto packet = rng::find_if(buffer, [](auto&& p) { return p.type == REPLAY_PACKET_GENERAL; });
        if (packet != buffer.end()) {
            outPos = packet->As<tReplayCameraBlock>()->firstFocusPosn;
            break;
        }
    }
}

// 0x45D670
uint32 CReplay::NumberFramesAvailableToPlay() {
    auto frames = 0u;
    for (auto& buffer : GetAllActiveBuffers()) {
        for (auto& packet : buffer) { // 0x45D670: one count per end-of-frame packet
            if (packet.type == REPLAY_PACKET_END_OF_FRAME)
                frames++;
        }
    }
    return frames;
}

// 0x45D4B0
void CReplay::StreamAllNecessaryCarsAndPeds() {
    for (auto& buffer : GetAllActiveBuffers()) {
        for (auto& packet : buffer) {
            switch (packet.type) {
            case REPLAY_PACKET_VEHICLE:
            case REPLAY_PACKET_BIKE:
                CStreaming::RequestModel(packet.As<tReplayVehicleBlock>()->modelId, STREAMING_DEFAULT);
                break;
            case REPLAY_PACKET_PED_HEADER:
                CStreaming::RequestModel(packet.As<tReplayPedHeaderBlock>()->modelId, STREAMING_DEFAULT);
                break;
            default:
                break;
            }
        }
    }
    CStreaming::LoadAllRequestedModels(false);
}

// 0x45D540
CPlayerPed* CReplay::CreatePlayerPed() {
    CPlayerPed* player = nullptr;

    auto i = 0;
    for (auto slot = Playback.GetNextSlot(i); i < NUM_REPLAY_BUFFERS && BufferStatus[slot]; slot = Playback.GetNextSlot(++i)) {
        auto& buffer = Buffers[slot];
        if (buffer.Read<tReplayBlockBase>(0).type == REPLAY_PACKET_END)
            break;

        for (auto& packet : buffer) {
            const auto offset = (size_t)((uint8*)&packet - (uint8*)&buffer);

            switch (packet.type) {
            case REPLAY_PACKET_PED_HEADER:
                if (!player) {
                    // get the next packet
                    auto next = buffer.Read<tReplayClothesBlock>(offset + FindSizeOfPacket(REPLAY_PACKET_PED_HEADER));
                    assert(next.type == REPLAY_PACKET_CLOTHES);

                    player = static_cast<CPlayerPed*>(DealWithNewPedPacket(*packet.As<tReplayPedHeaderBlock>(), true, &next));
                }
                break;
            case REPLAY_PACKET_PED_UPDATE:
                if (player && player == GetPedPool()->GetAt(FindPoolIndexForPed(packet.As<tReplayPedUpdateBlock>()->poolRef))) {
                    CAddressInReplayBuffer address(buffer, i, offset); // m_bSlot definition (i) is NOTSA
                    ProcessPedUpdate(player, 1.0f, address);
                    return player;
                }
            }
        }
    }
    return nullptr;
}

// 0x4600F0
void CReplay::TriggerPlayback(eReplayCamMode mode, CVector fixedCamPos, bool loadScene) {
    if (!ShouldStandardCameraBeProcessed())
        return;

    CSpecialFX::ReplayStarted();
    CameraMode = mode;
    Mode = MODE_PLAYBACK;
    CameraFixed = fixedCamPos;

    bPlayingBackFromFile = false;
    bAllowLookAroundCam = true;
    FramesActiveLookAroundCam = 0;

    OldRadioStation = [&]() -> eRadioID {
        if (FindPlayerVehicle()) {
            // exe 0x46015C/0x460171: the station is read BEFORE StopRadio (the port stopped the radio first)
            const auto station = AERadioTrackManager.GetCurrentRadioStationID();
            AudioEngine.StopRadio(nullptr, false);
            return station;
        }

        return RADIO_EMERGENCY_AA; // Possibly not intended.
    }();

    CurrArea = CGame::currArea;

    // TODO: refactor
    auto idx = 7;
    for (auto&& [i, status] : rngv::enumerate(BufferStatus)) {
        if (status == REPLAYBUFFER_IN_USE) {
            idx = i;
            break;
        }
    }

    // TODO: refactor
    auto slot = (idx + 1) % 8;
    for (auto j = BufferStatus[slot]; j != REPLAYBUFFER_IN_USE && j != REPLAYBUFFER_FULL; j = BufferStatus[slot]) {
        slot = (slot + 1) % 8;
    }
    slot %= 8;
    assert(slot >= 0);
    Playback = CAddressInReplayBuffer(Buffers[slot], slot);

    CObject::DeleteAllTempObjectsInArea(CVector{0.0f}, 1'000'000.0f);
    StoreStuffInMem();
    InitialisePoolConversionTables();
    EmptyPedsAndVehiclePools_NoDestructors();
    CSkidmarks::Clear();
    StreamAllNecessaryCarsAndPeds();

    CWorld::Players[0].m_pPed = CreatePlayerPed();

    bDoLoadSceneWhenDone = [&] {
        if (!loadScene) {
            LoadScene = TheCamera.GetPosition();
            CVector point{};
            FindFirstFocusCoordinate(point);
            CGame::currLevel = CTheZones::GetLevelFromPosition(point);
            CCollision::SortOutCollisionAfterLoad();
            CStreaming::LoadScene(point);

            return true;
        }
        return false;
    }();

    CWeaponEffects::ClearCrossHairs();
    CExplosion::ClearAllExplosions();
    CPlaneBanners::Init();
    gFireManager.DestroyAllFxSystems();
    TheCamera.Restore();
    CDraw::SetFOV(70.0f);
    TheCamera.SetFadeColour(0, 0, 0);
    TheCamera.Fade(0.0f, eFadeFlag::FADE_IN);
    TheCamera.ProcessFade();
    TheCamera.Fade(1.5f, eFadeFlag::FADE_OUT);
}

CReplay::tReplayBuffer::Iterator& CReplay::tReplayBuffer::Iterator::operator++() {
#ifdef NOTSA_DEBUG
    if (m_offset >= REPLAY_BUFFER_SIZE || !m_buffer) {
        NOTSA_UNREACHABLE("Increment after end()!");
    }
#endif

    const auto type = m_buffer->Read<tReplayBlockBase>(m_offset).type;
    m_offset = (type != REPLAY_PACKET_END) ? m_offset + CReplay::FindSizeOfPacket(type) : REPLAY_BUFFER_SIZE;
    return *this;
}

#define VALIDATE_BLOCK(block) static_assert(sizeof(block) == CReplay::FindSizeOfPacket(block::Type), "Block size mismatch!")

VALIDATE_BLOCK(tReplayVehicleBlock);
VALIDATE_BLOCK(tReplayBikeBlock);
VALIDATE_BLOCK(tReplayPedHeaderBlock);
VALIDATE_BLOCK(tReplayPedUpdateBlock);
VALIDATE_BLOCK(tReplayCameraBlock);
VALIDATE_BLOCK(tReplayClockBlock);
VALIDATE_BLOCK(tReplayWeatherBlock);
VALIDATE_BLOCK(tReplayEOFBlock);
VALIDATE_BLOCK(tReplayTimerBlock);
VALIDATE_BLOCK(tReplayTimerBlock);
VALIDATE_BLOCK(tReplayBulletTraceBlock);
VALIDATE_BLOCK(tReplayParticleBlock);
VALIDATE_BLOCK(tReplayMiscBlock);
VALIDATE_BLOCK(tReplayDeletedVehicleBlock);
VALIDATE_BLOCK(tReplayDeletedPedBlock);
VALIDATE_BLOCK(tReplayBmxBlock);
VALIDATE_BLOCK(tReplayHeliBlock);
VALIDATE_BLOCK(tReplayPlaneBlock);
VALIDATE_BLOCK(tReplayTrainBlock);
VALIDATE_BLOCK(tReplayClothesBlock);
#undef VALIDATE_BLOCK
