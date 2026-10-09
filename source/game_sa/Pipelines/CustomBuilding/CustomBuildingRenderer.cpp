#include "StdInc.h"

#include "CustomBuildingRenderer.h"

#include "CustomBuildingDNPipeline.h"
#include "CustomBuildingPipeline.h"
#include "Clock.h"

constexpr auto CUSTOM_BUILDING_PIPELINE_ID = rpPDS_MAKEPIPEID(rwVENDORID_DEVELOPER, 0x9C); // see CustomBuildingPipeline.cpp

void CCustomBuildingRenderer::InjectHooks() {
    RH_ScopedClass(CCustomBuildingRenderer);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Initialise, 0x5D7EC0);
    RH_ScopedInstall(Shutdown, 0x5D7EE0);
    RH_ScopedInstall(PluginAttach, 0x5D7EF0);
    RH_ScopedInstall(AtomicSetup, 0x5D7F00);
    RH_ScopedInstall(IsCBPCPipelineAttached, 0x5D7F40);
    RH_ScopedInstall(UpdateDayNightBalanceParam, 0x5D7F80);
    RH_ScopedInstall(Update, 0x5D8050);
}

// 0x5D7EC0
bool CCustomBuildingRenderer::Initialise() {
    auto pipe = CCustomBuildingPipeline::CreatePipe();
    if (pipe)
        return CCustomBuildingDNPipeline::CreatePipe();

    return pipe;
}

// 0x5D7EE0
void CCustomBuildingRenderer::Shutdown() {
    CCustomBuildingPipeline::DestroyPipe();
    CCustomBuildingDNPipeline::DestroyPipe();
}

// 0x5D7EF0
bool CCustomBuildingRenderer::PluginAttach() {
    return CCustomBuildingDNPipeline::ExtraVertColourPluginAttach();
}

// 0x5D7F00
void CCustomBuildingRenderer::AtomicSetup(RpAtomic* atomic) {
    auto* const geometry = RpAtomicGetGeometry(atomic);
    if (CCustomBuildingDNPipeline::GetExtraVertColourPtr(geometry) && RwCompatGeometryPreLit(geometry)) {
        CCustomBuildingDNPipeline::CustomPipeAtomicSetup(atomic);
    } else {
        CCustomBuildingPipeline::CustomPipeAtomicSetup(atomic);
    }
}

// 0x5D7F40
bool CCustomBuildingRenderer::IsCBPCPipelineAttached(RpAtomic* atomic) {
    const auto pipeID   = GetPipelineID(atomic);
    auto* const geometry = RpAtomicGetGeometry(atomic);
    if (pipeID == CUSTOM_BUILDING_DN_PIPELINE_ID || pipeID == CUSTOM_BUILDING_PIPELINE_ID) {
        return true;
    }
    return CCustomBuildingDNPipeline::GetExtraVertColourPtr(geometry) && RwCompatGeometryPreLit(geometry);
}

// 0x5D7F80
void CCustomBuildingRenderer::UpdateDayNightBalanceParam() {
    const auto minutes = (float)(CClock::ms_nGameClockHours * 60u + CClock::ms_nGameClockMinutes) + (float)CClock::ms_nGameClockSeconds * (1.0f / 60.0f);

    if (minutes >= 360.0f) {
        if (minutes < 420.0f) {
            CCustomBuildingDNPipeline::m_fDNBalanceParam = (420.0f - minutes) * (1.0f / 60.0f);
            return;
        }
        if (minutes < 1200.0f) {
            CCustomBuildingDNPipeline::m_fDNBalanceParam = 0.0f;
            return;
        }
        if (minutes < 1260.0f) {
            CCustomBuildingDNPipeline::m_fDNBalanceParam = 1.0f - (1260.0f - minutes) * (1.0f / 60.0f);
            return;
        }
    }
    CCustomBuildingDNPipeline::m_fDNBalanceParam = 1.0f;
}

// 0x5D8050
void CCustomBuildingRenderer::Update() {
    ZoneScoped;

    UpdateDayNightBalanceParam();

    // 0x5D6830 [`sub_5D6830(0)`]: advance the per-frame atomic update slot (see s_Magic1/s_Magic2 in CustomBuildingDNPipeline.cpp)
    static auto& s_Magic1 = StaticRef<uint32>(0xC02C14);
    static auto& s_Magic2 = StaticRef<uint32>(0xC02C18);
    s_Magic1 = (s_Magic1 + 1) & 15;
    s_Magic2 = 0;
}
