#include "StdInc.h"

#include "Gamma.h"
#ifdef NOTSA_PORTABLE
namespace rw::gl3 { extern float presentGamma; }
#endif

void CGamma::InjectHooks() {
    RH_ScopedClass(CGamma);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x747180);
    RH_ScopedInstall(SetGamma, 0x747200);
}

// 0x747180
void CGamma::Init() {
    m_IsEnabled    = true;
    m_CurrentLevel = 0.5f;
    m_TargetLevel  = -1000.0f;
    m_PrevLevel    = 1000.0f;

#ifndef NOTSA_PORTABLE
    auto* caps = (D3DCAPS9*)const_cast<void*>(RwD3D9GetCaps());
    auto* dev  = (IDirect3DDevice9*)RwD3D9GetCurrentD3DDevice();
    D3DCAPS9 devCaps{}; // TODO: what's the difference with `caps`?
    dev->GetDeviceCaps(&devCaps);

    if (caps->DeclTypes & D3DDTCAPS_DEC3N) {
        caps->DeclTypes &= ~D3DDTCAPS_DEC3N;
    }

    if (!(devCaps.Caps2 & D3DCAPS2_FULLSCREENGAMMA) && !(devCaps.Caps2 & D3DCAPS2_CANCALIBRATEGAMMA)) {
        m_IsEnabled = false; // No gamma support
    }
#else
    m_IsEnabled = true; // VIS1: no hardware gamma ramp under GL3; SetGamma sets the exponent of librw gl3's final present pass instead
#endif
}

// 0x747200
void CGamma::SetGamma(float gammaLevel, bool fade) {
    if (!m_IsEnabled) {
        return;
    }
    const auto level = gammaLevel * 0.7f;
    m_PrevLevel = level; // exe 0x74720F: stored unconditionally (before the compare with the target)
    if (level != m_TargetLevel) {
        m_TargetLevel = level;
    } else if (!fade) {
        return;
    }

#ifndef NOTSA_PORTABLE
    auto* dev = (IDirect3DDevice9*)RwD3D9GetCurrentD3DDevice();
    if (!dev) {
        return;
    }

    if (!gbGammaChanged) {
        gbGammaChanged = true;
        dev->GetGammaRamp(0, &ms_SavedGamma);
    }

    const auto power = 1.0f - level + 0.2f;
    // TODO: rngv::zip
    for (auto i = 0; i < 256; i++) {
        const auto gamma = static_cast<int16>(NOTSA_FTOL(int32, X87R(x87::pow(X87W((x87d)(float(i + 1) / 256.0f)), X87W((x87d)power)) * (x87d)65535.0))); // exe 0x7472AF: CRT pow 0x8220F0 (qword args) (the same kernel), x 65535.0 (qword 0x874B00) rounds, _ftol
        ms_GammaTable.red[i] = ms_GammaTable.green[i] = ms_GammaTable.blue[i] = gamma;
    }
    dev->SetGammaRamp(0, D3DSGR_CALIBRATE, &ms_GammaTable);
#else
    rw::gl3::presentGamma = 1.0f - level + 0.2f; // the D3D ramp is pow((i + 1) / 256, power), power = 1 - level + 0.2
#endif
}
