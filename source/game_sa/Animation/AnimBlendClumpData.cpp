#include "StdInc.h"

#include "AnimBlendClumpData.h"

void CAnimBlendClumpData::InjectHooks() {
    RH_ScopedClass(CAnimBlendClumpData);
    RH_ScopedCategory("Animation");

    RH_ScopedInstall(Constructor, 0x4CF0E0);
    RH_ScopedInstall(Destructor, 0x4CF100);
    RH_ScopedInstall(SetNumberOfBones, 0x4CF140);
    RH_ScopedInstall(ForAllFrames, 0x4CF190);
    RH_ScopedInstall(LoadFramesIntoSPR, 0x4CF1D0);
    RH_ScopedInstall(ForAllFramesInSPR, 0x4CF1E0);
}

// 0x4CF0E0
CAnimBlendClumpData::CAnimBlendClumpData() {
    m_NumFrameData   = 0;
    m_PedPosition = nullptr;
    m_FrameDatas      = nullptr;
}

// 0x4CF100
CAnimBlendClumpData::~CAnimBlendClumpData() {
    m_AnimList.Remove();
    if (m_FrameDatas) {
        CMemoryMgr::FreeAlign(m_FrameDatas);
    }
}

// 0x4CF140
void CAnimBlendClumpData::SetNumberOfBones(uint32 numBones) {
    if (m_FrameDatas) {
        CMemoryMgr::FreeAlign(m_FrameDatas);
    }
    m_NumFrameData = numBones;
    // 0x4CF140: the exe rounds the byte count up to a multiple of 64 (((n * 0x18 - 1) >> 6) + 1 << 6; 0 for n == 0) before MallocAlign(size, 64)
    const uint32 bytes = numBones * (uint32)sizeof(AnimBlendFrameData);
    m_FrameDatas = static_cast<AnimBlendFrameData*>(CMemoryMgr::MallocAlign((((bytes - 1u) >> 6) + 1u) << 6, 64));
}

// 0x4CF190
void CAnimBlendClumpData::ForAllFrames(void (*callback)(AnimBlendFrameData*, void*), void* data) {
    for (auto& frame : std::span{ m_FrameDatas, m_NumFrameData }) {
        callback(&frame, data);
    }
}

// 0x4CF1D0
void CAnimBlendClumpData::LoadFramesIntoSPR() {
    // NOP
}

// 0x4CF1E0
void CAnimBlendClumpData::ForAllFramesInSPR(void (*callback)(AnimBlendFrameData*, void*), void* data, uint32 a3) {
    // NOP
}
