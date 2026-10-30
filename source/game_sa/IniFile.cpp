#include "StdInc.h"

#include "IniFile.h"
#include "CarCtrl.h"

void CIniFile::InjectHooks() {
    RH_ScopedClass(CIniFile);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(LoadIniFile, 0x56D070);
}

// 0x56D070
void CIniFile::LoadIniFile() {
    ZoneScoped;

    CFileMgr::SetDir("");
    auto file = CFileMgr::OpenFile("gta3.ini", "r");
    if (file) {
        CFileMgr::ReadLine(file, gString, 200);
        VERIFY(sscanf_s(gString, "%f", &PedNumberMultiplier) == 1);
        // exe 0x56D0BB: `!(x < 0.5)` (fcomp + test ah,0x41), then `x > 3.0` (jp skips unordered): a NaN stays a NaN
        if (!(PedNumberMultiplier < 0.5f)) {
            if (PedNumberMultiplier > 3.0f)
                PedNumberMultiplier = 3.0f;
        } else {
            PedNumberMultiplier = 0.5f;
        }

        CFileMgr::ReadLine(file, gString, 200);
        VERIFY(sscanf_s(gString, "%f", &CarNumberMultiplier) == 1);
        // exe 0x56D0BB: `!(x < 0.5)` (fcomp + test ah,0x41), then `x > 3.0` (jp skips unordered): a NaN stays a NaN
        if (!(CarNumberMultiplier < 0.5f)) {
            if (CarNumberMultiplier > 3.0f)
                CarNumberMultiplier = 3.0f;
        } else {
            CarNumberMultiplier = 0.5f;
        }
        CFileMgr::CloseFile(file);
    }
    CPopulation::MaxNumberOfPedsInUse = 25;
    CCarCtrl::MaxNumberOfCarsInUse = 12;
}
