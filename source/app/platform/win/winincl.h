#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOGDICAPMASKS
#define NOSYSMETRICS
#define NOICONS
#define NOSYSCOMMANDS
#define NORASTEROPS
#define OEMRESOURCE
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NORPC
#define NOPROXYSTUB
#define NOIMAGE
#define NOTAPE
#define NOATOM
#define NOCLIPBOARD
#define NODRAWTEXT
#define NOKERNEL
#define NOMEMMGR
#define NOMETAFILE
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define STRICT

#define INC_OLE2 // for `V_VT` and `V_UNION` macros at some headers of Windows SDK

// #define NOMENUS -- `GetMenu` in `_rwD3D9CameraClear`
// #define NOCOLOR -- Used at ImGui Win32 impl.
// #define NOCTLMGR -- for LPCDLGTEMPLATE at some headers of Windows SDK
// #define NONLS -- for GetACP at common.h

#ifdef _WIN32
#include <Windows.h>

#ifdef USE_DSOUND
    #include <mmsystem.h> // for DirectSound types
#endif
#ifdef NOTSA_PORTABLE
#include "portable/audio_portable.h" // PortableWin64: MainImageRange()
#include "portable/app_win32_compat.h" // PortableWin64: notsa::portable::Unsupported() only (the Win32 API itself is real)
// D3DGAMMARAMP (d3d9types.h) without <d3d9.h>: librw's rwd3d.h defines the D3D vocabulary itself and rejects the real header
struct D3DGAMMARAMP { WORD red[256]; WORD green[256]; WORD blue[256]; };
#endif
#else
// Non-Windows (portable) build: the scalar types and the MSVC CRT names only; Win32 API calls are NOT shimmed (stream W)
#include "portable/wintypes.h"
#include "portable/crt.h"
#include "portable/winapi_compat.h"
#include "portable/app_win32_compat.h"
#endif

#undef GetCurrentTime
#undef MoveMemory
#undef PlaySound
#undef GetObject
