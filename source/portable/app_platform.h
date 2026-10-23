#pragma once
/*
 * MAC-W0a: portable replacements of small Win32 facilities used by app/platform/win/{WinMain,WinPs}.cpp. Implementations: app_platform.cpp.
 * Used only from `#ifdef NOTSA_PORTABLE` blocks.
 */
#ifdef NOTSA_PORTABLE
#include <cstdint>
#include <utility>

namespace notsa::portable {
//! true = this is the only running instance (advisory file lock held for the process lifetime)
bool AcquireSingleInstance();
//! `new[]`'d root of a mounted optical volume named GTA_SAN_ANDREAS (macOS: /Volumes/GTA_SAN_ANDREAS/), or nullptr
char* FindDvdGamePath();
//! Win32 PRIMARYLANGID values derived from the preferred locale(s): system, user, keyboard layout (all the same here)
struct LangIds { uint16_t sys, usr, kb; };
LangIds GetLangIds();
//! Physical memory in bytes (available = free + inactive on macOS)
void QueryPhysMemory(size_t& total, size_t& avail);
//! Desktop mode of the primary display {w, h}; {0, 0} if SDL has no display
std::pair<uint32_t, uint32_t> GetNativeResolution();
//! Same contract as WinPs.cpp GetBestRefreshRate: lowest rate >= 60 Hz among the primary display's modes of w x h; 0 if a matching mode reports 0; UINT_MAX if none
uint32_t GetBestRefreshRate(uint32_t w, uint32_t h);
//! W-A11: no read-back of the presented frame yet. A valid, fully black RGBA image of the screen size (never nullptr: JPegCompressScreen dereferences the result), logged once.
RwImage* GrabScreenBlank(RwCamera* camera);
//! Installs the fatal-signal handler that dumps registers-less backtrace through the game logger (replaces SetUnhandledExceptionFilter)
void InstallCrashHandler();
}
#endif
