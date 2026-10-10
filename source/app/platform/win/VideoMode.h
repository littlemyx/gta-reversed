#pragma once

#include "Platform.h"
#include <array>
#include <rwplcore.h>

//
// Sub Systems
//

//! Maximum number of subsystems we can deal with
//! 1 SubSystem / Display (And maybe / GPU ?)
constexpr auto MAX_SUBSYSTEMS = 16;

//! Subsystem infos (Populated using `RwEngineGetSubSystemInfo`)
NOTSA_GLOBAL_HDR(GsubSysInfo, 0xC8CFC0, (std::array<RwSubSystemInfo, MAX_SUBSYSTEMS>), {});

//! Number of subsystems
NOTSA_GLOBAL_HDR(GnumSubSystems, 0xC920F0, (RwInt32), {});

//! Currently selected subsystem
NOTSA_GLOBAL_HDR(GcurSelSS, 0xC920F4, (RwInt32), {});

//! Whenever there are multiple subsystems available
NOTSA_GLOBAL_HDR(MultipleSubSystems, 0xC92118, (RwBool), {});

//
// Video Mode
//

//! Currently selected videomode
NOTSA_GLOBAL_HDR(GcurSelVM, 0x8D6220, (RwInt32), { -1 }); // VM = Video Mode

//! Whenever to use the default videomode (Instead of the user selecting it)
NOTSA_GLOBAL_HDR(UseDefaultVM, 0xC920FC, (RwBool), {});

//! Unused shit
NOTSA_GLOBAL_HDR(DefaultVM, 0x8D2E34, (RwBool), { 1 });

//! Whenever FrontEndMemnuManager videomode stuff was **NOT** yet set (See WinMain)
NOTSA_GLOBAL_HDR(IsVMNotSelected, 0x8D6218, (RwBool), { 1 });

/*
* Dynamic array of video modes with format "width x height x depth"
*/
NOTSA_GLOBAL_HDR(gVideoModes, 0xC920D0, (char**), {});
NOTSA_GLOBAL_HDR(gCurrentGpu, 0x8D6248, (uint32), { 0xFFFFFFFFU });
static inline NOTSA_GLOBAL_ALIAS(gCurrentVideoMode, 0x8D6220, (int32), GcurSelVM);

void VideoModeInjectHooks();

extern char** GetVideoModeList();
extern bool FreeVideoModeList();
extern void SetVideoMode(int32 mode);
extern bool IsVideoModeExclusive();
