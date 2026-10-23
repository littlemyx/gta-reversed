// P-2b compile-time layout checks on the REAL engine headers (portable arm64 build, StdInc). Syntax-only: if it compiles, it passes.
//   tools/portable/run_p2b_test.sh
#include "StdInc.h"
#include "Scripts/ScriptParam.h"
#include "SimpleVariablesSaveStructure.h"
#include "Pipelines/CustomBuilding/CustomBuildingDNPipeline.h"

#ifndef NOTSA_PORTABLE
#error "this test is for the portable build"
#endif

// tScriptParam: the unused void*/char* members are gone => 4 bytes (it is dumped raw inside CRunningScript)
static_assert(sizeof(tScriptParam) == 4);
static_assert(alignof(tScriptParam) == 4);
static_assert(sizeof(tScriptParam[34]) == 136);

// SaveSystemTime: exactly the 16 byte Win32 layout (8 x WORD)
static_assert(sizeof(SaveSystemTime) == 16);
static_assert(offsetof(SaveSystemTime, wYear) == 0 && offsetof(SaveSystemTime, wMonth) == 2 && offsetof(SaveSystemTime, wDayOfWeek) == 4 && offsetof(SaveSystemTime, wDay) == 6);
static_assert(offsetof(SaveSystemTime, wHour) == 8 && offsetof(SaveSystemTime, wMinute) == 10 && offsetof(SaveSystemTime, wSecond) == 12 && offsetof(SaveSystemTime, wMilliseconds) == 14);

// CSimpleVariablesSaveStructure: save block 0, 0x138 on disk
static_assert(sizeof(CSimpleVariablesSaveStructure) == 0x138);
// offsets of the members after the bool/char runs (checked against clang -target i686-pc-windows-msvc: tools/portable/run_p2b_test.sh compares them)
#define P2B_OFFSETS(X) \
    X(m_nVersionId) X(m_szSaveName) X(m_bMissionPackGame) X(m_nCurrLevel) X(m_vecCamPosn) X(m_nMsPerMinute) X(m_nLastClockTick) X(m_nGameMonth) X(m_nGameDayOfWeek) \
    X(m_bClockHasBeenStored) X(m_nPadMode) X(m_bHasPlayerCheated) X(m_nTimeInMilliseconds) X(m_fTimeScale) X(m_nFrameCounter) X(m_nOldWeatherType) X(m_nWeatherTypeInList) \
    X(m_nCameraCarZoom) X(m_nCurrArea) X(m_bInvertLook4Pad) X(m_nExtraColour) X(m_bExtraColourOn) X(m_fExtraColourInter) X(m_nWaterConfiguration) X(m_bLARiots) \
    X(m_nMaximumWantedLevel) X(m_nMaxChaos) X(m_bFrenchGame) X(_padEF) X(m_bCineyCamMessageDisplayed) X(m_systemTime) X(m_nTargetBlipIndex) \
    X(m_bHasDisplayedPlayerQuitEnterCarHelpText) X(m_bAllTaxisNitro) X(m_bProstitutesPayYou)
#ifdef P2B_PRINT_OFFSETS
#include <cstdio>
int main() {
  #define PR(m) std::printf("%s %zu\n", #m, offsetof(CSimpleVariablesSaveStructure, m));
    P2B_OFFSETS(PR)
    std::printf("sizeof %zu\n", sizeof(CSimpleVariablesSaveStructure));
}
#endif

// ExtraVertColour: in-memory plugin data (pointers) => reclassified to VALIDATE_SIZE; it is 24 bytes on LP64 and must NOT be used as a file record
static_assert(sizeof(ExtraVertColour) == 24);
