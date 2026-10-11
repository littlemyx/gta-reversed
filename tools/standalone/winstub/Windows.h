// clang_census.py --mode native stub (see README.md): scalar types + MSVC keywords only
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef unsigned char BYTE; typedef unsigned short WORD; typedef uint32_t DWORD; typedef int BOOL; typedef long LONG; typedef unsigned long ULONG;
typedef unsigned int UINT; typedef int INT; typedef short SHORT; typedef unsigned short USHORT; typedef char CHAR; typedef wchar_t WCHAR; typedef float FLOAT;
typedef void* HANDLE; typedef void* LPVOID; typedef const void* LPCVOID; typedef void* PVOID; typedef char* LPSTR; typedef const char* LPCSTR;
typedef wchar_t* LPWSTR; typedef const wchar_t* LPCWSTR; typedef DWORD* LPDWORD; typedef BYTE* LPBYTE; typedef BOOL* LPBOOL;
typedef struct HWND__* HWND; typedef struct HINSTANCE__* HINSTANCE; typedef HINSTANCE HMODULE; typedef struct HDC__* HDC; typedef struct HICON__* HICON;
typedef struct HMENU__* HMENU; typedef struct HBRUSH__* HBRUSH; typedef struct HMONITOR__* HMONITOR; typedef struct HCURSOR__* HCURSOR; typedef long HRESULT;
typedef uintptr_t WPARAM; typedef intptr_t LPARAM; typedef intptr_t LRESULT; typedef uintptr_t ULONG_PTR; typedef intptr_t LONG_PTR; typedef uintptr_t UINT_PTR; typedef intptr_t INT_PTR; typedef uintptr_t DWORD_PTR;
typedef long long LONGLONG; typedef unsigned long long ULONGLONG; typedef unsigned char byte; typedef size_t SIZE_T; typedef intptr_t SSIZE_T; typedef uint64_t DWORD64; typedef int8_t INT8; typedef uint8_t UINT8; typedef int16_t INT16; typedef uint16_t UINT16; typedef int32_t INT32; typedef uint32_t UINT32; typedef int64_t INT64; typedef uint64_t UINT64; typedef unsigned char UCHAR; typedef unsigned char BOOLEAN; typedef signed char* PCHAR; typedef int8_t* PINT8;
#define __int64 long long
#define WINAPI
#define CALLBACK
#define APIENTRY
#define __stdcall
#define __cdecl
#define __fastcall
#define __thiscall
#define __forceinline inline
#define __declspec(x)
#define __int8 char
#define __int16 short
#define __int32 int
#define FAR
#define NEAR
#define CONST const
#define VOID void
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define INFINITE 0xFFFFFFFF
#define S_OK ((HRESULT)0L)
#define FAILED(h) (((HRESULT)(h)) < 0)
#define SUCCEEDED(h) (((HRESULT)(h)) >= 0)
#define LOWORD(l) ((WORD)(((uintptr_t)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((uintptr_t)(l)) >> 16) & 0xffff))
#define LOBYTE(w) ((BYTE)(((uintptr_t)(w)) & 0xff))
#define HIBYTE(w) ((BYTE)((((uintptr_t)(w)) >> 8) & 0xff))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(((uintptr_t)(a)) & 0xff)) | ((WORD)((BYTE)(((uintptr_t)(b)) & 0xff))) << 8))
#define UNREFERENCED_PARAMETER(P) (void)(P)
#define ZeroMemory(d, l) memset((d), 0, (l))
#define CopyMemory(d, s, l) memcpy((d), (s), (l))
#ifndef _MSC_VER
#define __debugbreak() __builtin_trap()
#endif
