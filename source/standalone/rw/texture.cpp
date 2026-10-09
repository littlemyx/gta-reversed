// P2B-03b: RwTexture* on top of librw (rw::Texture, D3D9 backend) + the D3D9 native texture reader that SA TXDs need.
//   Game-called (rwapi.h): RwTextureCreate, RwTextureDestroy, RwTextureRead, RwTextureSetName, RwTextureSetRaster, RwTextureSetMipmapping,
//   RwTextureSetAutoMipmapping, RwTextureSetReadCallBack, RwTextureSetFindCallBack, RwTextureGetFindCallBack.
//   Declared in rwextra.h (03b block): RwTextureGetMipmapping, RwTextureGetAutoMipmapping, RwTextureGetReadCallBack, RwTextureSetMaskName,
//   RwTextureStreamRead / Write / GetSize.
//   Also here (shim-private, used by texdict.cpp): rwshim::ReadNativeTexture.
// Function classes (see .notes/P2B_SHIM_PLAN.md): D = direct, A = adapter, W = written here because librw has nothing equivalent.
//
// Checked against the exe (RW 3.6 rwcore texture module): Create 0x7F37C0, Destroy 0x7F3820, SetName 0x7F38A0, SetMaskName 0x7F3910, SetRaster 0x7F35D0,
// Read 0x7F3AC0, Set/GetFindCallBack 0x7F34D0 / 0x7F34F0, Set/GetReadCallBack 0x7F3500 / 0x7F3520, RwTexDictionaryStreamRead 0x804C30:
//   * Create: refCount 1, name/mask empty, filter NEAREST, address WRAP/WRAP (= librw). Destroy: --refCount, at <= 0 destroy plugins, unlink from the
//     dictionary, RwRasterDestroy(raster) (the texture always owns its raster), free (= librw).
//   * SetName / SetMaskName: strncpy to 32 bytes, then name[31] = 0 (the exe reports an error for names >= 32 chars, the shim silently truncates).
//   * SetRaster: the exe goes through the device's TEXTURESETRASTER standard function, which for D3D9 only stores the pointer (and returns the texture,
//     or NULL when a raster is given and the device refuses). The old raster is NOT destroyed.
//   * Read(name, mask): find callback -> hit: refCount++ and return it WITHOUT touching the current dictionary; miss: read callback; NULL = failure;
//     success: moved to the FRONT of the current dictionary (if one is set). librw's Texture::read appends instead, so Read is written out here.
//   * Find / read callbacks are plain global pointers; Set returns TRUE.
//
// D3D9 native texture reader (librw d3d9::readNativeTexture is NOT used): the exe's reader accepts PAL8 rasters (SA's outro.txd has one, 512x512,
// format 0x2600 = PAL8|888, d3dformat 0) and creates them as 32 bit textures on devices without P8; librw's reader would pass PAL8 to Raster::create,
// which asks the device for D3DFMT_P8 (unsupported on every current device / wined3d), the create fails and librw asserts.
//   Decision (documented in .notes/PHASE2_STATUS.md): a PAL8 / PAL4 texture is EXPANDED AT LOAD to a 32 bit raster. The raster reports the true
//   storage format (rwRASTERFORMAT8888 when the file says 8888, else 888 = X8R8G8B8), NOT rwRASTERFORMATPAL8; RwRasterLockPalette returns NULL for it
//   and RwRasterLock returns B,G,R,A texels. Nothing in the game needs the indices (the one game user of an 8 bit looking font, CCustomRoadsignMgr,
//   treats "roadsignfont" as RwRGBA pixels already; the palette entry order in the file is R,G,B,A and is swizzled to D3D's B,G,R,A here).
//   Level sizes in the file are w*h index bytes per level (PAL4 uses 8 bit indices as well in the D3D9 format and a 32 entry palette).
//   Everything else follows librw: compressed (flags & 8) textures are created through createTexture with the file's D3DFORMAT, uncompressed ones via
//   Raster::create(format | type); a lock whose pitch differs from the file's row size is filled row by row (librw copies the whole level blindly).
// Only needs fakerw + librw + the CRT (also built by the PCH-less unit test tests/standalone/rw_texture_test.cpp). Excluded from unity builds.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

// librw's D3D9 device internals (D3dRaster, createTexture)
#include <src/d3d/rwd3dimpl.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>

static_assert(sizeof(((RwTexture*)nullptr)->name) == 32 && sizeof(((RwTexture*)nullptr)->mask) == 32);

//--------------------------------------------------------------------------------------------------
// D3D9 native texture (chunk 0x15) reader
//--------------------------------------------------------------------------------------------------
namespace rwshim {
namespace {
using rw::d3d::D3dRaster;

// Reads one mip level of `size` bytes from the stream into the locked level of `raster`. `lw`/`lh`: level size in texels.
bool ReadLevel(rw::Stream* s, RwRaster* raster, int level, uint32_t size, int lw, int lh, bool compressed, const uint8_t* palette) {
    uint8_t* dst = RwRasterLock(raster, level, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
    if (!dst) {
        s->seek(size);
        return false;
    }
    const int      stride    = raster->stride; // D3D pitch, valid while locked
    const int      rows      = compressed ? std::max(1, (lh + 3) / 4) : lh;
    const uint32_t rowBytes  = rows ? size / rows : 0;
    std::vector<uint8_t> tmp(size);
    s->read8(tmp.data(), size);
    if (palette) {
        // index bytes -> B,G,R,A (palette in the file is R,G,B,A)
        for (int y = 0; y < lh; ++y) {
            const uint8_t* src = tmp.data() + (size_t)y * rowBytes;
            uint8_t*       out = dst + (size_t)y * stride;
            for (int x = 0; x < lw && (uint32_t)x < rowBytes; ++x) {
                const uint8_t* c = palette + src[x] * 4;
                out[x * 4 + 0] = c[2];
                out[x * 4 + 1] = c[1];
                out[x * 4 + 2] = c[0];
                out[x * 4 + 3] = c[3];
            }
        }
    } else if ((uint32_t)stride == rowBytes) {
        std::memcpy(dst, tmp.data(), size);
    } else {
        const uint32_t n = std::min<uint32_t>((uint32_t)stride, rowBytes);
        for (int y = 0; y < rows; ++y) {
            std::memcpy(dst + (size_t)y * stride, tmp.data() + (size_t)y * rowBytes, n);
        }
    }
    RwRasterUnlock(raster);
    return true;
}
} // namespace

// Stream positioned at the STRUCT chunk header of a TEXTURENATIVE chunk (see rw::Texture::streamReadNative); returns the texture or NULL.
RwTexture* ReadNativeTexture(rw::Stream* stream) {
    if (!rw::findChunk(stream, rw::ID_STRUCT, nullptr, nullptr)) {
        return nullptr;
    }
    const uint32_t platform = stream->readU32();
    if (platform != rw::PLATFORM_D3D9) {
        // other platforms: librw's own readers (they assert on anything but D3D9/GL3 we do not build; SA PC files are all platform 9)
        stream->seek(-16);
        return rw::Texture::streamReadNative(stream);
    }
    RwTexture* tex = rw::Texture::create(nullptr);
    if (!tex) {
        return nullptr;
    }
    // exe 0x4CD820: filter (bits 0-7) and U / V addressing (bits 8-15) are merged into the fresh texture's word, the upper 16 bits of the file are dropped
    tex->filterAddressing = stream->readU32() & 0xFFFFu;
    stream->read8(tex->name, 32);
    stream->read8(tex->mask, 32);
    tex->name[31] = '\0';
    tex->mask[31] = '\0';

    const int32_t format    = stream->readI32();
    const int32_t d3dformat = stream->readI32();
    const int32_t width     = stream->readU16();
    const int32_t height    = stream->readU16();
    const int32_t depth     = stream->readU8();
    const int32_t numLevels = stream->readU8();
    const int32_t type      = stream->readU8();
    const int32_t flags     = stream->readU8(); // 1 alpha, 2 cube, 4 auto mip generation, 8 compressed

    RwRaster* raster = nullptr;
    uint8_t   palette[256 * 4] = {};
    bool      paletted = false;

    if (flags & 8) {
        assert((flags & 2) == 0 && "cube maps");
        raster = rw::Raster::create(width, height, depth, format | type | rw::Raster::DONTALLOCATE, rw::PLATFORM_D3D9);
        if (raster) {
            D3dRaster* ext = GETD3DRASTEREXT(raster);
            ext->format   = d3dformat;
            ext->hasAlpha = flags & 1;
            ext->texture  = rw::d3d::createTexture(raster->width, raster->height, (raster->format & rw::Raster::MIPMAP) ? numLevels : 1, 0, ext->format);
            if (!ext->texture) {
                raster->destroy();
                raster = nullptr;
            } else {
                raster->flags &= ~rw::Raster::DONTALLOCATE;
                ext->customFormat = 1;
            }
        }
    } else if (flags & 2) {
        assert(0 && "cube maps");
    } else {
        int32_t fmt = format;
        int32_t dep = depth;
        if (fmt & (rw::Raster::PAL8 | rw::Raster::PAL4)) {
            // expand at load (see the header comment)
            paletted = true;
            stream->read8(palette, (fmt & rw::Raster::PAL4) ? 4 * 32 : 4 * 256);
            int32_t color = fmt & 0xF00;
            if (color != rw::Raster::C8888 && color != rw::Raster::C888) {
                color = (flags & 1) ? rw::Raster::C8888 : rw::Raster::C888;
            }
            fmt = (fmt & ~(rw::Raster::PAL8 | rw::Raster::PAL4 | 0xF00)) | color;
            dep = 32;
        }
        raster = rw::Raster::create(width, height, dep, fmt | type, rw::PLATFORM_D3D9);
    }
    if (!raster) {
        tex->destroy();
        return nullptr;
    }
    tex->raster = raster;

    const bool compressed = (flags & 8) != 0;
    const int  have       = raster->getNumLevels();
    for (int i = 0; i < numLevels; ++i) {
        const uint32_t size = stream->readU32();
        if (i < have) {
            const int lw = std::max(1, width >> i);
            const int lh = std::max(1, height >> i);
            ReadLevel(stream, raster, i, size, lw, lh, compressed, paletted ? palette : nullptr);
        } else {
            stream->seek(size);
        }
    }
    return tex;
}
} // namespace rwshim

//--------------------------------------------------------------------------------------------------
// RwTexture*
//--------------------------------------------------------------------------------------------------
namespace {
void CopyName(char* dst, const RwChar* src) {
    std::memset(dst, 0, 32);
    if (src) {
        std::strncpy(dst, src, 31);
    }
}
} // namespace

// D: Texture::create (refCount 1, empty names, NEAREST + WRAP/WRAP).
RwTexture* RwTextureCreate(RwRaster* raster) {
    return rw::Texture::create(raster);
}

// D: refCount--, destruction at <= 0 (plugins, dictionary unlink, raster, free).
RwBool RwTextureDestroy(RwTexture* texture) {
    texture->destroy();
    return TRUE;
}

// W (see header): Read through the find / read callbacks, new textures go to the FRONT of the current dictionary.
RwTexture* RwTextureRead(const RwChar* name, const RwChar* maskName) {
    if (RwTexture* tex = rw::Texture::findCB(name)) {
        tex->addRef();
        return tex;
    }
    RwTexture* tex = rw::Texture::readCB(name, maskName);
    if (!tex) {
        return nullptr;
    }
    if (RwTexDictionary* cur = rw::TexDictionary::getCurrent()) {
        cur->addFront(tex);
    }
    return tex;
}

RwTexture* RwTextureSetName(RwTexture* texture, const RwChar* name) {
    CopyName(texture->name, name);
    return texture;
}

RwTexture* RwTextureSetMaskName(RwTexture* texture, const RwChar* maskName) {
    CopyName(texture->mask, maskName);
    return texture;
}

// A: pointer store only (the old raster is not destroyed), like the D3D9 device function.
RwTexture* RwTextureSetRaster(RwTexture* texture, RwRaster* raster) {
    texture->raster = raster;
    return texture;
}

RwBool RwTextureSetMipmapping(RwBool enable) {
    rw::Texture::setMipmapping(enable);
    return TRUE;
}
RwBool RwTextureSetAutoMipmapping(RwBool enable) {
    rw::Texture::setAutoMipmapping(enable);
    return TRUE;
}
RwBool RwTextureGetMipmapping(void) { return rw::Texture::getMipmapping(); }
RwBool RwTextureGetAutoMipmapping(void) { return rw::Texture::getAutoMipmapping(); }

// The game installs CTxdStore::TxdStoreFindCB / TxdStoreLoadCB; both have librw's callback signatures.
RwBool RwTextureSetFindCallBack(RwTextureCallBackFind callBack) {
    rw::Texture::findCB = callBack;
    return TRUE;
}
RwTextureCallBackFind RwTextureGetFindCallBack(void) { return rw::Texture::findCB; }
RwBool RwTextureSetReadCallBack(RwTextureCallBackRead callBack) {
    rw::Texture::readCB = callBack;
    return TRUE;
}
RwTextureCallBackRead RwTextureGetReadCallBack(void) { return rw::Texture::readCB; }

// D: texture chunk (0x6: struct filter/address + 2 strings + plugins). StreamRead goes through the CURRENT find / read callbacks (Texture::read).
RwTexture* RwTextureStreamRead(RwStream* stream) { return rw::Texture::streamRead(stream); }
const RwTexture* RwTextureStreamWrite(const RwTexture* texture, RwStream* stream) {
    return const_cast<RwTexture*>(texture)->streamWrite(stream) ? texture : nullptr;
}
RwUInt32 RwTextureStreamGetSize(const RwTexture* texture) { return const_cast<RwTexture*>(texture)->streamGetSize(); }
#endif
