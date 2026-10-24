#pragma once
// MAC-W0: the one shell-folder query the game makes (SHGetFolderPathW(CSIDL_PERSONAL) = "My Documents"), for non-Windows builds.
#include "portable/wintypes.h"

#define CSIDL_PERSONAL 0x0005
#define SHGFP_TYPE_CURRENT 0
#define SHGFP_TYPE_DEFAULT 1

//! CSIDL_PERSONAL -> $HOME/Documents (the macOS Documents folder). Any other CSIDL: E_INVALIDARG. Defined in portable/oswrapper_portable.cpp.
HRESULT SHGetFolderPathW(HWND hwnd, int csidl, HANDLE token, DWORD flags, LPWSTR path);
