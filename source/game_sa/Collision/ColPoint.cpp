#include "StdInc.h"

#include "ColPoint.h"

#include "CustomBuildingDNPipeline.h"

// 0x59F0C0
float tColLighting::GetCurrentLighting(float fScale) const {
    // exe (every inlined site, oracle-proven at CTrain::FindPositionOnTrackFromCoors 0x6F6E95): the default scale folds into the constant 0.5 / 15 (0x858F10 = 0.033333335f, no
    // division at run time), and the day / night mix is the blend form `day * (1 - balance) + night * balance`
    const float k = fScale == 0.5F ? std::bit_cast<float>(0x3D088889u) : fScale / 15.0F;
    const float fDay = static_cast<float>(day) * k;
    const float fNight = static_cast<float>(night) * k;
    const float balance = CCustomBuildingDNPipeline::m_fDNBalanceParam;
    return fDay * (1.0F - balance) + fNight * balance;
}
