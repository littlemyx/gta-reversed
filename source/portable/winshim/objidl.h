#pragma once
// MAC-W0: COM base interfaces as TYPES for non-Windows builds (IUnknown, IStream). Nothing here creates a COM object: the only COM-style class that is
// instantiated in the portable build is CAEDataStream (a plain C++ implementation of IStream), and its only consumers (Media Foundation / WMA decoders)
// are replaced by logging failure stubs (portable/audio_decoders_portable.cpp).
#include "portable/wintypes.h"

#ifndef STDMETHODCALLTYPE
    #define STDMETHODCALLTYPE
#endif
#ifndef STDMETHOD
    #define STDMETHOD(m) virtual HRESULT STDMETHODCALLTYPE m
#endif

inline constexpr GUID IID_IUnknown        = {0x00000000, 0x0000, 0x0000, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
inline constexpr GUID IID_ISequentialStream = {0x0c733a30, 0x2a1c, 0x11ce, {0xad, 0xe5, 0x00, 0xaa, 0x00, 0x44, 0x77, 0x3d}};
inline constexpr GUID IID_IStream         = {0x0000000c, 0x0000, 0x0000, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
inline constexpr GUID GUID_NULL           = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};

struct IUnknown {
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG STDMETHODCALLTYPE   AddRef()                                       = 0;
    virtual ULONG STDMETHODCALLTYPE   Release()                                      = 0;
};

enum STGTY { STGTY_STORAGE = 1, STGTY_STREAM = 2, STGTY_LOCKBYTES = 3, STGTY_PROPERTY = 4 };
enum STATFLAG { STATFLAG_DEFAULT = 0, STATFLAG_NONAME = 1, STATFLAG_NOOPEN = 2 };
struct FILETIME { DWORD dwLowDateTime, dwHighDateTime; };
struct STATSTG {
    LPWSTR         pwcsName;
    DWORD          type;
    ULARGE_INTEGER cbSize;
    FILETIME       mtime, ctime, atime;
    DWORD          grfMode, grfLocksSupported;
    CLSID          clsid;
    DWORD          grfStateBits, reserved;
};

struct ISequentialStream : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead)         = 0;
    virtual HRESULT STDMETHODCALLTYPE Write(const void* pv, ULONG cb, ULONG* pcbWritten) = 0;
};
struct IStream : ISequentialStream {
    virtual HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition)                      = 0;
    virtual HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize)                                                                 = 0;
    virtual HRESULT STDMETHODCALLTYPE CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten)       = 0;
    virtual HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags)                                                                       = 0;
    virtual HRESULT STDMETHODCALLTYPE Revert()                                                                                           = 0;
    virtual HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)                          = 0;
    virtual HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)                        = 0;
    virtual HRESULT STDMETHODCALLTYPE Stat(STATSTG* pstatstg, DWORD grfStatFlag)                                                         = 0;
    virtual HRESULT STDMETHODCALLTYPE Clone(IStream** ppstm)                                                                             = 0;
};

