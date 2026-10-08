#include "StdInc.h"

#include "AttractorScanner.h"

void CAttractorScanner::InjectHooks() {
    RH_ScopedClass(CAttractorScanner);
    RH_ScopedCategory("Scanners");

    RH_ScopedInstall(Clear, 0x5FFF90);
}

// 0x5FFF90
void CAttractorScanner::Clear() {
    for (auto i = 0; i < 10; i++) {
        field_18[i] = 0;
        field_40[i] = 0;
        field_68[i] = 15.0f * 15.0f; // 0x86C52C = 15.0f
        if (i != 4 && i != 7) {
            field_68[i] = 25.0f;
        }
    }
}
