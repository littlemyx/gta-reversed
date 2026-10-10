/*
    P2B-01: RwStream* of the RW C API on top of librw, RwOsGetFileInterface.

    RwStream is rw::Stream (abstract). Streams created through the RW C API are ShimStream objects (file / memory); streams librw hands to
    plugin callbacks may be any rw::Stream, the functions below only need read8/write8/seek for those.
    Semantics checked against the exe (RW 3.6 stream module, rwplcore): RwStreamOpen 0x7ECEF0, _rwStreamInitialize 0x7EC810, RwStreamRead 0x7EC9D0,
    RwStreamSkip 0x7ECD00, RwStreamClose 0x7ECE20, RwStreamFindChunk 0x7ED2D0, RwStreamReadChunkHeaderInfo 0x7ED590 (+ _rwStreamReadChunkHeader 0x7ED0F0):
      * rwSTREAMFILENAME opens through the file interface with "rb"/"wb"/"ab"; rwSTREAMFILE wraps an open FILE* (closed by RwStreamClose);
        rwSTREAMMEMORY takes an RwMemory{start,length}: READ uses that buffer in place, APPEND copies it, WRITE starts empty; write/append streams grow an
        engine-allocated buffer that RwStreamClose hands back through its pData (RwMemory*, release with RwFree). rwSTREAMCUSTOM is not supported
        (the game has no custom streams).
      * RwStreamRead returns the number of bytes read (a short read is reported, not fatal); a memory stream clamps at its end.
      * RwStreamSkip(0) is a no-op; a memory stream skipping past its end stops at the end and returns NULL; returns the stream otherwise.
      * RwStreamFindChunk: 12-byte headers, skips non-matching chunks by length; only accepts a matching chunk whose library version is within
        [0x34000, 0x36003] (RW 3.4 .. 3.6.0.3), anything else fails like the original; length/version outputs may be NULL.
      * RwStreamReadChunkHeaderInfo fills {type, length, version (unpacked), buildNum, isComplex} and returns NULL if the 12 bytes cannot be read.
      * _rwStreamInitialize: the caller storage (the game's `gRwStream`) is NOT used as the object (rw::Stream is polymorphic, the exe's RwStream is a plain
        struct), but the exe's identity semantics are kept: every storage address owns ONE persistent slot, the returned stream lives at a fixed address and is
        re-initialised in place by the next _rwStreamInitialize of the same storage; RwStreamClose finishes the stream but never frees the slot (the exe's
        close does not free the struct, and CStreaming::ConvertBufferToObject closes a stream pointer it already closed and re-initialised: the exe closes the
        same static struct twice, 0x7ECE20 on a closed stream is harmless). A close on a dead slot returns TRUE.
*/
#ifdef NOTSA_RW_UNIT_TEST
#include <rwcore.h>
#else
#include "StdInc.h"
#endif
#include <cstdio>
#include <cstring>
#include <new>
#include <unordered_map>

#ifdef NOTSA_RW_LIBRW

//--------------------------------------------------------------------------------------------------
// File interface (RwOsGetFileInterface): the CRT, with the argument types RW used
//--------------------------------------------------------------------------------------------------
static RwBool  __cdecl FsExist(const RwChar* path) { FILE* f = fopen(path, "rb"); if (f) { fclose(f); return TRUE; } return FALSE; }
static void*   __cdecl FsOpen(const RwChar* path, const RwChar* mode) { return fopen(path, mode); }
static int     __cdecl FsClose(void* fp) { return fclose(static_cast<FILE*>(fp)); }
static size_t  __cdecl FsRead(void* p, size_t size, size_t n, void* fp) { return fread(p, size, n, static_cast<FILE*>(fp)); }
static size_t  __cdecl FsWrite(const void* p, size_t size, size_t n, void* fp) { return fwrite(p, size, n, static_cast<FILE*>(fp)); }
static RwChar* __cdecl FsGets(RwChar* buf, int n, void* fp) { return fgets(buf, n, static_cast<FILE*>(fp)); }
static int     __cdecl FsPuts(const RwChar* s, void* fp) { return fputs(s, static_cast<FILE*>(fp)); }
static int     __cdecl FsEof(void* fp) { return feof(static_cast<FILE*>(fp)); }
static int     __cdecl FsSeek(void* fp, long off, int origin) { return fseek(static_cast<FILE*>(fp), off, origin); }
static int     __cdecl FsFlush(void* fp) { return fflush(static_cast<FILE*>(fp)); }
static long    __cdecl FsTell(void* fp) { return ftell(static_cast<FILE*>(fp)); }

RwFileFunctions* RwOsGetFileInterface() {
    static RwFileFunctions s_fi = { FsExist, FsOpen, FsClose, FsRead, FsWrite, FsGets, FsPuts, FsEof, FsSeek, FsFlush, FsTell };
    return &s_fi;
}

//--------------------------------------------------------------------------------------------------
// Stream objects
//--------------------------------------------------------------------------------------------------
namespace {
class ShimStream : public rw::Stream {
public:
    // RwStreamSkip: false = failed (the original returns NULL)
    virtual bool skip(uint32_t n) = 0;
    // RwStreamClose: release the resources, `out` = pData (may be NULL). TRUE on success
    virtual RwBool finish(RwMemory* out) = 0;
};

class FileStream final : public ShimStream {
    void* m_file;
public:
    explicit FileStream(void* file) : m_file(file) {}
    ~FileStream() override { close(); }
    void close() override {
        if (m_file) {
            RwOsGetFileInterface()->rwfclose(m_file);
            m_file = nullptr;
        }
    }
    uint32_t write8(const void* data, uint32_t length) override { return (uint32_t)RwOsGetFileInterface()->rwfwrite(data, 1, length, m_file); }
    uint32_t read8(void* data, uint32_t length) override { return (uint32_t)RwOsGetFileInterface()->rwfread(data, 1, length, m_file); }
    void     seek(int32_t offset, int32_t whence) override { RwOsGetFileInterface()->rwfseek(m_file, offset, whence); } // 0 SET, 1 CUR, 2 END
    uint32_t tell() override { return (uint32_t)RwOsGetFileInterface()->rwftell(m_file); }
    bool     eof() override { return RwOsGetFileInterface()->rwfeof(m_file) != 0; }
    bool     skip(uint32_t n) override { return n == 0 || RwOsGetFileInterface()->rwfseek(m_file, (long)n, SEEK_CUR) == 0; }
    RwBool   finish(RwMemory*) override {
        const bool ok = m_file && RwOsGetFileInterface()->rwfclose(m_file) == 0;
        m_file = nullptr;
        return ok;
    }
};

class MemoryStream final : public ShimStream {
    uint8_t* m_data = nullptr;
    uint32_t m_pos = 0, m_len = 0, m_cap = 0;
    bool     m_owned = false;      // m_data is an engine allocation that we grow / hand back / free
    bool     m_writable = false;
    bool     m_hitEnd = false;     // a read ran past the end (rw::Stream::eof)

    bool Reserve(uint32_t cap) {
        if (cap <= m_cap) {
            return true;
        }
        uint32_t ncap = m_cap ? m_cap : 1024;
        while (ncap < cap) {
            ncap *= 2;
        }
        auto* p = static_cast<uint8_t*>(m_data ? rwRealloc(m_data, ncap, rw::MEMDUR_EVENT | rw::ID_NAOBJECT) : rwMalloc(ncap, rw::MEMDUR_EVENT | rw::ID_NAOBJECT));
        if (!p) {
            return false;
        }
        m_data = p;
        m_cap  = ncap;
        return true;
    }
public:
    MemoryStream(RwStreamAccessType access, const RwMemory* mem) {
        m_writable = access != rwSTREAMREAD;
        switch (access) {
        case rwSTREAMREAD: // reads the caller's buffer in place
            m_data = mem->start; m_len = m_cap = mem->length;
            break;
        case rwSTREAMAPPEND: // continue behind the given contents (copied: the buffer must be growable)
            m_owned = true;
            if (mem && mem->start && mem->length && Reserve(mem->length)) {
                std::memcpy(m_data, mem->start, mem->length);
                m_len = m_pos = mem->length;
            }
            break;
        default: // rwSTREAMWRITE: starts empty
            m_owned = true;
            break;
        }
    }
    ~MemoryStream() override {
        if (m_owned && m_data) {
            rwFree(m_data);
        }
    }
    uint32_t read8(void* data, uint32_t length) override {
        const uint32_t rem = m_len - m_pos;
        const uint32_t n   = length < rem ? length : rem;
        if (n) {
            std::memcpy(data, m_data + m_pos, n);
        }
        m_pos += n;
        if (n != length) {
            m_hitEnd = true;
        }
        return n;
    }
    uint32_t write8(const void* data, uint32_t length) override {
        if (!m_writable || !m_owned || !Reserve(m_pos + length)) {
            return 0;
        }
        std::memcpy(m_data + m_pos, data, length);
        m_pos += length;
        if (m_pos > m_len) {
            m_len = m_pos;
        }
        return length;
    }
    void seek(int32_t offset, int32_t whence) override {
        const int64_t p = whence == 0 ? (int64_t)offset : whence == 1 ? (int64_t)m_pos + offset : (int64_t)m_len - offset;
        m_pos = p < 0 ? 0 : p > (int64_t)m_len ? m_len : (uint32_t)p;
    }
    uint32_t tell() override { return m_pos; }
    bool     eof() override { return m_hitEnd; }
    bool     skip(uint32_t n) override {
        if (n > m_len - m_pos) {
            m_pos = m_len;
            return false;
        }
        m_pos += n;
        return true;
    }
    RwBool finish(RwMemory* out) override {
        if (m_writable && m_owned && out) { // hand the buffer over; the caller releases it with RwFree
            out->start  = m_data;
            out->length = m_pos;
            m_data = nullptr;
        }
        return TRUE;
    }
};

ShimStream* CreateStream(RwStreamType type, RwStreamAccessType access, const void* pData, void* at = nullptr) {
#define NEW_STREAM(T, ...) (at ? new (at) T(__VA_ARGS__) : new T(__VA_ARGS__))
    switch (type) {
    case rwSTREAMFILE: {
        void* const file = const_cast<void*>(pData);
        if (!file || RwOsGetFileInterface()->rwftell(file) == -1L) {
            return nullptr;
        }
        return NEW_STREAM(FileStream, file);
    }
    case rwSTREAMFILENAME: {
        const char* mode = access == rwSTREAMREAD ? "rb" : access == rwSTREAMWRITE ? "wb" : access == rwSTREAMAPPEND ? "ab" : nullptr;
        if (!mode || !pData) {
            return nullptr;
        }
        void* const file = RwOsGetFileInterface()->rwfopen(static_cast<const char*>(pData), mode);
        return file ? NEW_STREAM(FileStream, file) : nullptr;
    }
    case rwSTREAMMEMORY:
        if (access < rwSTREAMREAD || access > rwSTREAMAPPEND || (!pData && access != rwSTREAMWRITE)) {
            return nullptr;
        }
        return NEW_STREAM(MemoryStream, access, static_cast<const RwMemory*>(pData));
    default: // rwSTREAMCUSTOM, rwNASTREAM
        return nullptr;
    }
#undef NEW_STREAM
}

// One persistent object slot per _rwStreamInitialize storage address (see the file header)
struct PersistentSlot {
    alignas(16) unsigned char mem[(sizeof(FileStream) > sizeof(MemoryStream) ? sizeof(FileStream) : sizeof(MemoryStream))];
    bool live = false;
    ShimStream* obj() { return reinterpret_cast<ShimStream*>(mem); }
};
std::unordered_map<const void*, PersistentSlot*>& SlotsByStorage() { static std::unordered_map<const void*, PersistentSlot*> m; return m; }
std::unordered_map<const void*, PersistentSlot*>& SlotsByObject() { static std::unordered_map<const void*, PersistentSlot*> m; return m; }

// 12-byte chunk header {type, length, libraryID}; version = libraryIDUnpackVersion, build = libraryIDUnpackBuild
bool ReadChunkHeader(RwStream* stream, RwUInt32& type, RwUInt32& length, RwUInt32& version, RwUInt32& build) {
    RwUInt32 raw[3];
    if (stream->read8(raw, sizeof(raw)) != sizeof(raw)) {
        return false;
    }
    type    = raw[0];
    length  = raw[1];
    version = rw::libraryIDUnpackVersion(raw[2]);
    build   = rw::libraryIDUnpackBuild(raw[2]);
    return true;
}

// chunks that contain further chunks (exe jump table at 0x7ED1FC): Camera, Texture, Material, MaterialList, AtomicSection, PlaneSection,
// World, FrameList, Geometry, Clump, Light, Atomic, GeometryList
RwBool IsComplexChunk(RwUInt32 type) {
    switch (type) {
    case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 14: case 15: case 16: case 18: case 20: case 26:
        return TRUE;
    default:
        return FALSE;
    }
}
} // namespace

RwStream* RwStreamOpen(RwStreamType type, RwStreamAccessType accessType, const void* pData) {
    return CreateStream(type, accessType, pData);
}

RwStream* _rwStreamInitialize(RwStream* stream, RwBool /*rwOwned*/, RwStreamType type, RwStreamAccessType accessType, const void* pData) {
    if (!stream) {
        return nullptr;
    }
    auto*& slot = SlotsByStorage()[stream];
    if (!slot) {
        slot = new PersistentSlot();
    }
    if (slot->live) { // re-initialised over a stream that is still open (the exe just overwrites the struct)
        slot->obj()->~ShimStream();
        slot->live = false;
    }
    ShimStream* const s = CreateStream(type, accessType, pData, slot->mem);
    if (!s) {
        return nullptr;
    }
    slot->live = true;
    SlotsByObject()[s] = slot;
    return s;
}

RwBool RwStreamClose(RwStream* stream, void* pData) {
    if (!stream) {
        return FALSE;
    }
    if (const auto it = SlotsByObject().find(stream); it != SlotsByObject().end()) {
        PersistentSlot* const slot = it->second;
        if (!slot->live) {
            return TRUE; // closing a closed static stream
        }
        const RwBool ok = slot->obj()->finish(static_cast<RwMemory*>(pData));
        slot->obj()->~ShimStream();
        slot->live = false;
        return ok;
    }
    if (auto* const s = dynamic_cast<ShimStream*>(stream)) {
        const RwBool ok = s->finish(static_cast<RwMemory*>(pData));
        delete s;
        return ok;
    }
    stream->close(); // a stream librw created: its owner releases it
    return TRUE;
}

RwUInt32 RwStreamRead(RwStream* stream, void* buffer, RwUInt32 length) {
    return stream->read8(buffer, length);
}

RwStream* RwStreamWrite(RwStream* stream, const void* buffer, RwUInt32 length) {
    return stream->write8(buffer, length) == length ? stream : nullptr;
}

RwStream* RwStreamSkip(RwStream* stream, RwUInt32 offset) {
    if (offset == 0) {
        return stream;
    }
    if (auto* const s = dynamic_cast<ShimStream*>(stream)) {
        return s->skip(offset) ? stream : nullptr;
    }
    stream->seek((int32_t)offset, 1);
    return stream;
}

RwStream* RwStreamReadChunkHeaderInfo(RwStream* stream, RwChunkHeaderInfo* chunkHeaderInfo) {
    RwUInt32 type, length, version, build;
    if (!ReadChunkHeader(stream, type, length, version, build)) {
        return nullptr;
    }
    chunkHeaderInfo->type      = type;
    chunkHeaderInfo->length    = length;
    chunkHeaderInfo->version   = version;
    chunkHeaderInfo->buildNum  = build;
    chunkHeaderInfo->isComplex = IsComplexChunk(type);
    return stream;
}

RwBool RwStreamFindChunk(RwStream* stream, RwUInt32 type, RwUInt32* lengthOut, RwUInt32* versionOut) {
    RwUInt32 t, length, version, build;
    if (!ReadChunkHeader(stream, t, length, version, build)) {
        return FALSE;
    }
    while (t != type) {
        if (!RwStreamSkip(stream, length) || !ReadChunkHeader(stream, t, length, version, build)) {
            return FALSE;
        }
    }
    if (version < 0x34000 || version > 0x36003) { // RW 3.6 refuses chunks of other library versions
        return FALSE;
    }
    if (lengthOut) {
        *lengthOut = length;
    }
    if (versionOut) {
        *versionOut = version;
    }
    return TRUE;
}

#endif // NOTSA_RW_LIBRW
