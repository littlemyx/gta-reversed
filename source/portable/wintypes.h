#pragma once
/*
 * Windows scalar types and the few <windows.h> macros that are pure arithmetic, for NON-Windows builds (stream C-6).
 *
 * Included from app/platform/win/winincl.h when !_WIN32. On Windows this file is not used (and is a no-op if it is).
 *
 * Rules:
 *  - Fixed width, as on Win32 AND Win64: LONG/ULONG/DWORD/HRESULT are 32 bit (never `long`: LP64 makes `long` 64 bit);
 *    *_PTR / SIZE_T / WPARAM / LPARAM follow the pointer size (as on Win64).
 *  - NO Win32 API, no structs of the GDI/USER/DirectX kind: those are stream W/R (compiled out or reimplemented).
 *    Only GUID / LARGE_INTEGER / ULARGE_INTEGER are provided as they are plain data used outside the platform files.
 */
#ifndef _WIN32

#include <cstddef>
#include <cstdint>
#include <cstring>

// ---- calling conventions / MSVC keywords: only when the compiler does not take them natively (-fms-extensions) ----
#ifndef _MSC_EXTENSIONS
    #define __cdecl
    #define __stdcall
    #define __fastcall
    #define __thiscall
    #define __vectorcall
    #define __forceinline inline __attribute__((always_inline))
    #define __declspec(x)
    #define __int8 char
    #define __int16 short
    #define __int32 int
    #define __int64 long long
#endif
#define WINAPI
#define WINAPIV
#define CALLBACK
#define APIENTRY
#define NTAPI
#define CDECL
#define PASCAL
#define FAR
#define NEAR
#define CONST const
#define VOID void
#ifndef TRUE
    #define TRUE 1
#endif
#ifndef FALSE
    #define FALSE 0
#endif

// ---- scalar types ----
using BOOL    = int;
using BOOLEAN = unsigned char;
using BYTE    = unsigned char;
using byte    = unsigned char; // rpcndr.h: `typedef unsigned char byte;`
using UCHAR   = unsigned char;
using CHAR    = char;
using WORD    = uint16_t;
using USHORT  = uint16_t;
using SHORT   = int16_t;
using INT     = int;
using UINT    = unsigned int;
using LONG    = int32_t;  // 32 bit on every Windows target
using ULONG   = uint32_t;
using DWORD   = uint32_t;
using FLOAT   = float;
using INT8    = int8_t;
using UINT8   = uint8_t;
using INT16   = int16_t;
using UINT16  = uint16_t;
using INT32   = int32_t;
// libjpeg (jmorecfg.h) defines its own `long` INT32 unless Microsoft's basetsd.h claims it; this header plays that role (INT32 = int, as on Windows)
#ifndef _BASETSD_H_
    #define _BASETSD_H_
#endif
using UINT32  = uint32_t;
using INT64   = int64_t;
using UINT64  = uint64_t;
using LONG32  = int32_t;
using ULONG32 = uint32_t;
using LONG64  = int64_t;
using ULONG64 = uint64_t;
using DWORD32 = uint32_t;
using DWORD64 = uint64_t;
using LONGLONG  = int64_t;
using ULONGLONG = uint64_t;
using WCHAR   = wchar_t; // NOTE: 16 bit on Windows, 32 bit here: UTF-16 handling is stream W's job
using TCHAR   = char;    // the build is _MBCS

using HRESULT = int32_t;

using INT_PTR   = intptr_t;
using UINT_PTR  = uintptr_t;
using LONG_PTR  = intptr_t;
using ULONG_PTR = uintptr_t;
using DWORD_PTR = uintptr_t;
using SIZE_T    = size_t;
using SSIZE_T   = intptr_t;
using WPARAM    = UINT_PTR;
using LPARAM    = LONG_PTR;
using LRESULT   = LONG_PTR;

using PVOID   = void*;
using LPVOID  = void*;
using LPCVOID = const void*;
using HANDLE  = void*;
using PHANDLE = HANDLE*;
using LPSTR   = char*;
using PSTR    = char*;
using LPCSTR  = const char*;
using PCSTR   = const char*;
using LPWSTR  = wchar_t*;
using LPCWSTR = const wchar_t*;
using LPTSTR  = char*;
using LPCTSTR = const char*;
using PCHAR   = char*;
using PBYTE   = BYTE*;
using LPBYTE  = BYTE*;
using PBOOL   = BOOL*;
using LPBOOL  = BOOL*;
using PWORD   = WORD*;
using LPWORD  = WORD*;
using PDWORD  = DWORD*;
using LPDWORD = DWORD*;
using PLONG   = LONG*;
using LPLONG  = LONG*;
using PULONG  = ULONG*;
using PUINT   = UINT*;
using PINT    = INT*;
using PFLOAT  = FLOAT*;
using PINT8   = INT8*;
using PUINT8  = UINT8*;
using PINT16  = INT16*;
using PUINT16 = UINT16*;
using PINT32  = INT32*;
using PUINT32 = UINT32*;
using PINT64  = INT64*;
using PUINT64 = UINT64*;

// opaque handles (DECLARE_HANDLE): distinct pointer types, never dereferenced
#define NOTSA_DECLARE_HANDLE(name) struct name##__; using name = name##__*
NOTSA_DECLARE_HANDLE(HWND);
NOTSA_DECLARE_HANDLE(HINSTANCE);
NOTSA_DECLARE_HANDLE(HDC);
NOTSA_DECLARE_HANDLE(HICON);
NOTSA_DECLARE_HANDLE(HMENU);
NOTSA_DECLARE_HANDLE(HBRUSH);
NOTSA_DECLARE_HANDLE(HCURSOR);
NOTSA_DECLARE_HANDLE(HMONITOR);
NOTSA_DECLARE_HANDLE(HBITMAP);
NOTSA_DECLARE_HANDLE(HFONT);
NOTSA_DECLARE_HANDLE(HGLRC);
#undef NOTSA_DECLARE_HANDLE
using HMODULE = HINSTANCE;

// ---- plain data structs ----
struct GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
};
using IID   = GUID;
using CLSID = GUID;
using REFGUID  = const GUID&;
using REFIID   = const GUID&;
using REFCLSID = const GUID&;
inline bool operator==(const GUID& a, const GUID& b) {
    return a.Data1 == b.Data1 && a.Data2 == b.Data2 && a.Data3 == b.Data3
        && a.Data4[0] == b.Data4[0] && a.Data4[1] == b.Data4[1] && a.Data4[2] == b.Data4[2] && a.Data4[3] == b.Data4[3]
        && a.Data4[4] == b.Data4[4] && a.Data4[5] == b.Data4[5] && a.Data4[6] == b.Data4[6] && a.Data4[7] == b.Data4[7];
}
inline bool operator!=(const GUID& a, const GUID& b) { return !(a == b); }

union LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; };
    struct { DWORD LowPart; LONG HighPart; } u;
    LONGLONG QuadPart;
};
union ULARGE_INTEGER {
    struct { DWORD LowPart; DWORD HighPart; };
    struct { DWORD LowPart; DWORD HighPart; } u;
    ULONGLONG QuadPart;
};

// ---- constants / macros that are pure arithmetic ----
#define MAX_PATH 260
#define INFINITE 0xFFFFFFFFu
#define NOERROR   0
#define NO_ERROR  0
#define S_OK      ((HRESULT)0)
#define S_FALSE   ((HRESULT)1)
#define E_FAIL        ((HRESULT)0x80004005)
#define E_NOTIMPL     ((HRESULT)0x80004001)
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define E_POINTER     ((HRESULT)0x80004003)
#define E_ABORT       ((HRESULT)0x80004004)
#define E_UNEXPECTED  ((HRESULT)0x8000FFFF)
#define E_OUTOFMEMORY ((HRESULT)0x8007000E)
#define E_INVALIDARG  ((HRESULT)0x80070057)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr)    (((HRESULT)(hr)) < 0)

#define LOWORD(l)  ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l)  ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w)  ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w)  ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define UNREFERENCED_PARAMETER(P) ((void)(P))
#define ZeroMemory(d, l) memset((d), 0, (l))
#define FillMemory(d, l, f) memset((d), (f), (l))
#define CopyMemory(d, s, l) memcpy((d), (s), (l))
#define MoveMemory(d, s, l) memmove((d), (s), (l))

// compiler intrinsic of <intrin.h> that is used like an API
#ifndef __debugbreak
    #define __debugbreak() __builtin_trap()
#endif

#endif // !_WIN32
