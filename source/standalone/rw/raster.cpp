// P2B-03a: RwRaster* on top of librw (rw::Raster, D3D9 backend).
//   Game-called (rwapi.h): RwRasterCreate, RwRasterDestroy, RwRasterLock, RwRasterUnlock, RwRasterUnlockPalette, RwRasterPushContext,
//   RwRasterPopContext, RwRasterRenderFast, RwRasterSetFromImage.
//   Declared in rwextra.h (03a block, not called by the game outside the stock RW headers): RwRasterLockPalette, RwRasterGetOffset,
//   RwRasterGetNumLevels, RwRasterSubRaster, RwRasterGetCurrentContext, RwRasterRender, RwRasterRenderScaled, RwRasterClear,
//   RwRasterClearRect, RwRasterShowRaster, RwRasterRead, RwRasterReadMaskedRaster, RwRasterSetFreeListCreateParams.
//   RwRGBAToPixel lives here too (JPegCompress.cpp).
// Function classes (see .notes/P2B_SHIM_PLAN.md): D = direct, A = adapter, W = written here because librw has nothing equivalent.
//
// Facts about librw's d3d9 raster that shape the adapters:
//   * Raster::create() ignores `depth` and derives it from the format (888 -> D3DFMT_X8R8G8B8, depth 32, 4 bytes per texel; PAL8 -> D3DFMT_P8).
//     Type/flags/format are split exactly like RW (type = flags & 7, flags = & 0xF8, format = & 0xFF00); PRIVATELOCK_* = rwRASTERPIXELLOCKED*.
//   * lock() returns NULL if already locked, rewrites width/height/stride/pixels for the duration of the lock (stride = D3D pitch), and
//     asserts on camera rasters when bit 4 (NOFETCH) is set; unlock(level) dereferences lockedSurf unconditionally. All three guarded here.
//   * lockPalette / unlockPalette / numLevels are `assert(0)` stubs for the null driver and the d3d9 driver registers no lockPalette at all
//     (D3dRaster::palette is the RGBA palette of a P8 texture; it is what RW's RwRasterLockPalette returned).
//   * Raster::subRaster() only copies the rectangle; it neither validates nor inherits type / format / texture.
//   * Raster::imageFindRasterFormat / setFromImage are the FindRasterFormat / SetFromImage of RW with two gaps (SetFromImage only converts to
//     8888 / 888 / 1555 / PAL8 and aborts via assert on a size mismatch): RwRasterSetFromImage below is written in full (every 16/24/32 bit
//     raster format, palettized images, NORMAL rasters, size check) on top of rwpixel.h.
// Only needs fakerw + librw + the CRT (also built by the PCH-less unit test tests/standalone/rw_raster_image_test.cpp). Excluded from unity builds.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

// librw's D3D9 device internals (d3ddevice, d3d9Globals, D3dRaster)
#include <src/d3d/rwd3dimpl.h>

#include "rwpixel.h"

#include <cassert>
#include <cstring>

static_assert(rwRASTERTYPENORMAL == rw::Raster::NORMAL && rwRASTERTYPEZBUFFER == rw::Raster::ZBUFFER && rwRASTERTYPECAMERA == rw::Raster::CAMERA &&
              rwRASTERTYPETEXTURE == rw::Raster::TEXTURE && rwRASTERTYPECAMERATEXTURE == rw::Raster::CAMERATEXTURE && rwRASTERDONTALLOCATE == rw::Raster::DONTALLOCATE);
static_assert(rwRASTERFORMAT1555 == rw::Raster::C1555 && rwRASTERFORMAT565 == rw::Raster::C565 && rwRASTERFORMAT4444 == rw::Raster::C4444 &&
              rwRASTERFORMATLUM8 == rw::Raster::LUM8 && rwRASTERFORMAT8888 == rw::Raster::C8888 && rwRASTERFORMAT888 == rw::Raster::C888 &&
              rwRASTERFORMAT555 == rw::Raster::C555 && rwRASTERFORMATAUTOMIPMAP == rw::Raster::AUTOMIPMAP && rwRASTERFORMATPAL8 == rw::Raster::PAL8 &&
              rwRASTERFORMATPAL4 == rw::Raster::PAL4 && rwRASTERFORMATMIPMAP == rw::Raster::MIPMAP);
static_assert(rwRASTERPIXELLOCKEDREAD == rw::Raster::PRIVATELOCK_READ && rwRASTERPIXELLOCKEDWRITE == rw::Raster::PRIVATELOCK_WRITE &&
              rwRASTERPALETTELOCKEDREAD == rw::Raster::PRIVATELOCK_READ_PALETTE && rwRASTERPALETTELOCKEDWRITE == rw::Raster::PRIVATELOCK_WRITE_PALETTE);

namespace {
using rw::d3d::D3dRaster;

constexpr int kPixelLockMask   = rw::Raster::PRIVATELOCK_READ | rw::Raster::PRIVATELOCK_WRITE;
constexpr int kPaletteLockMask = rw::Raster::PRIVATELOCK_READ_PALETTE | rw::Raster::PRIVATELOCK_WRITE_PALETTE;

inline D3dRaster* Ext(RwRaster* r) { return GETD3DRASTEREXT(r); }
inline bool IsSub(const RwRaster* r) { return r->parent && r->parent != r; }
inline bool IsPaletted(const RwRaster* r) { return (r->format & (rwRASTERFORMATPAL8 | rwRASTERFORMATPAL4)) != 0; }

// Create + the RW rules librw does not apply. AUTOMIPMAP means "generate the chain" in RW (RwTextureRead always sets MIPMAP | AUTOMIPMAP together),
// librw's rasterCreateTexture only auto-generates when both bits are present and otherwise silently makes a single level.
RwRaster* CreateRaster(RwInt32 width, RwInt32 height, RwInt32 depth, RwInt32 flags) {
    if (width < 0 || height < 0) {
        return nullptr;
    }
    if (flags & rwRASTERFORMATAUTOMIPMAP) {
        flags |= rwRASTERFORMATMIPMAP;
    }
    return rw::Raster::create(width, height, depth, flags);
}

// Real (parent) raster behind r; sub rasters share the parent's texture.
inline RwRaster* Root(RwRaster* r) { return IsSub(r) ? r->parent : r; }

bool HasLockableStorage(RwRaster* r) {
    RwRaster* root = Root(r);
    switch (root->type & rwRASTERTYPEMASK) {
    case rwRASTERTYPENORMAL:
    case rwRASTERTYPETEXTURE:
    case rwRASTERTYPECAMERATEXTURE:
        return Ext(root)->texture != nullptr;
    case rwRASTERTYPECAMERA:
        return rw::d3d::d3ddevice != nullptr;
    }
    return false; // z buffers are not lockable
}

// Read lock of a render target texture. librw's rasterLock does the same (copy the render target into a system memory surface and lock that) but
// never releases the IDirect3DSurface9 it gets from GetSurfaceLevel, so the texture could not be destroyed afterwards ("texture wasn't destroyed").
// Fills the same raster fields (pixels / stride / width / height / lockedSurf) so librw's rasterUnlock undoes it.
uint8_t* LockCameraTexture(RwRaster* r, int level, int lockMode) {
    IDirect3DDevice9* dev = rw::d3d::d3ddevice;
    D3dRaster*        nr  = Ext(r);
    auto*             tex = static_cast<IDirect3DTexture9*>(nr->texture);
    if (!dev || !tex) {
        return nullptr;
    }
    IDirect3DSurface9 *rt = nullptr, *sys = nullptr;
    if (FAILED(tex->GetSurfaceLevel(level, &rt))) {
        return nullptr;
    }
    D3DSURFACE_DESC desc{};
    rt->GetDesc(&desc);
    HRESULT hr = dev->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr);
    if (SUCCEEDED(hr)) {
        hr = dev->GetRenderTargetData(rt, sys);
    }
    rt->Release();
    D3DLOCKED_RECT lr{};
    if (FAILED(hr) || FAILED(sys->LockRect(&lr, nullptr, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY | D3DLOCK_NO_DIRTY_UPDATE))) {
        if (sys) {
            sys->Release();
        }
        return nullptr;
    }
    nr->lockedSurf = sys;
    r->pixels = static_cast<uint8_t*>(lr.pBits);
    r->width >>= level;
    r->height >>= level;
    if (r->width == 0) r->width = 1;
    if (r->height == 0) r->height = 1;
    r->stride = lr.Pitch;
    r->privateFlags |= (lockMode & rwRASTERLOCKREAD) ? rw::Raster::PRIVATELOCK_READ : 0;
    r->privateFlags |= (lockMode & rwRASTERLOCKWRITE) ? rw::Raster::PRIVATELOCK_WRITE : 0;
    return r->pixels;
}

//--------------------------------------------------------------------------------------------------
// Device clears (RwRasterClear / ClearRect on camera rasters). Direct device calls are fine here: they only touch the render target and
// the viewport, both restored, and not librw's render state cache.
//--------------------------------------------------------------------------------------------------
bool DeviceClear(RwRaster* ctx, const RwRect* rect, uint32_t argb) {
    IDirect3DDevice9* dev = rw::d3d::d3ddevice;
    if (!dev) {
        return false;
    }
    IDirect3DSurface9* target = nullptr;
    bool               ownTarget = false;
    if ((ctx->type & rwRASTERTYPEMASK) == rwRASTERTYPECAMERA) {
        target = rw::d3d::d3d9Globals.defaultRenderTarget;
    } else {
        auto* tex = static_cast<IDirect3DTexture9*>(Ext(Root(ctx))->texture);
        if (!tex || FAILED(tex->GetSurfaceLevel(0, &target))) {
            return false;
        }
        ownTarget = true;
    }
    IDirect3DSurface9* prev = nullptr;
    D3DVIEWPORT9       vp{};
    dev->GetRenderTarget(0, &prev);
    dev->GetViewport(&vp);
    bool ok = true;
    if (prev != target) {
        ok = SUCCEEDED(dev->SetRenderTarget(0, target));
    }
    if (ok) {
        D3DRECT r{};
        if (rect) {
            r.x1 = ctx->offsetX + rect->x;
            r.y1 = ctx->offsetY + rect->y;
            r.x2 = r.x1 + rect->w;
            r.y2 = r.y1 + rect->h;
        }
        ok = SUCCEEDED(dev->Clear(rect ? 1 : 0, rect ? &r : nullptr, D3DCLEAR_TARGET, argb, 1.0f, 0));
    }
    if (prev != target && prev) {
        dev->SetRenderTarget(0, prev);
        dev->SetViewport(&vp);
    }
    if (prev) {
        prev->Release();
    }
    if (ownTarget) {
        target->Release();
    }
    return ok;
}

// Textured quad through librw's Im2D onto whatever the current camera renders to (needs RwCameraBeginUpdate; the quad is in frame buffer pixels).
RwRaster* DrawQuad(RwRaster* raster, float x, float y, float w, float h, bool filtered) {
    if (!raster || !rw::engine || !rw::engine->currentCamera || !rw::d3d::d3ddevice || !Ext(Root(raster))->texture) {
        return nullptr;
    }
    const int t = Root(raster)->type & rwRASTERTYPEMASK;
    if (t != rwRASTERTYPENORMAL && t != rwRASTERTYPETEXTURE && t != rwRASTERTYPECAMERATEXTURE) {
        return nullptr;
    }
    // sub raster: UV window inside the parent's texture
    const RwRaster* root = Root(raster);
    const float u0 = float(raster->offsetX) / float(root->width), v0 = float(raster->offsetY) / float(root->height);
    const float u1 = float(raster->offsetX + raster->width) / float(root->width), v1 = float(raster->offsetY + raster->height) / float(root->height);

    const float z = rw::im2d::GetNearZ();
    rw::d3d::Im2DVertex q[4];
    const float xs[4] = { x, x + w, x, x + w }, ys[4] = { y, y, y + h, y + h };
    const float us[4] = { u0, u1, u0, u1 }, vs[4] = { v0, v0, v1, v1 };
    for (int i = 0; i < 4; ++i) {
        q[i].x = xs[i]; q[i].y = ys[i]; q[i].z = z; q[i].w = 1.0f;
        q[i].color = 0xFFFFFFFFu;
        q[i].u = us[i]; q[i].v = vs[i];
    }
    const auto savedRaster = rw::GetRenderStatePtr(rw::TEXTURERASTER);
    uint32_t sav[5] = { rw::GetRenderState(rw::TEXTUREFILTER), rw::GetRenderState(rw::VERTEXALPHA), rw::GetRenderState(rw::ZTESTENABLE),
                        rw::GetRenderState(rw::ZWRITEENABLE), rw::GetRenderState(rw::SRCBLEND) };
    const uint32_t savDst = rw::GetRenderState(rw::DESTBLEND);
    rw::SetRenderStatePtr(rw::TEXTURERASTER, Root(raster));
    rw::SetRenderState(rw::TEXTUREFILTER, filtered ? rw::Texture::LINEAR : rw::Texture::NEAREST);
    rw::SetRenderState(rw::VERTEXALPHA, rw::Raster::formatHasAlpha(Root(raster)->format));
    rw::SetRenderState(rw::SRCBLEND, rw::BLENDSRCALPHA);
    rw::SetRenderState(rw::DESTBLEND, rw::BLENDINVSRCALPHA);
    rw::SetRenderState(rw::ZTESTENABLE, 0);
    rw::SetRenderState(rw::ZWRITEENABLE, 0);
    rw::im2d::RenderPrimitive(rw::PRIMTYPETRISTRIP, q, 4);
    rw::SetRenderStatePtr(rw::TEXTURERASTER, savedRaster);
    rw::SetRenderState(rw::TEXTUREFILTER, sav[0]);
    rw::SetRenderState(rw::VERTEXALPHA, sav[1]);
    rw::SetRenderState(rw::ZTESTENABLE, sav[2]);
    rw::SetRenderState(rw::ZWRITEENABLE, sav[3]);
    rw::SetRenderState(rw::SRCBLEND, sav[4]);
    rw::SetRenderState(rw::DESTBLEND, savDst);
    return raster;
}
} // namespace

// D (+ RW AUTOMIPMAP rule): every combination the game uses is the plain librw path.
//   camera / z buffer:  rwRASTERTYPECAMERA, rwRASTERTYPEZBUFFER with depth 0 (Game.cpp, app_camera.cpp, WindowedMode.cpp); the format and
//                       depth come from the present parameters (back buffer / auto depth-stencil format), the size is whatever is asked.
//   camera texture:     rwRASTERTYPECAMERATEXTURE, depth 0 or 32 (Mirrors, ShadowCamera, RealTimeShadowManager): render target texture.
//   textures:           rwRASTERFORMAT8888 | rwRASTERTYPETEXTURE (grain), rwRASTERFORMAT888 | rwRASTERPIXELLOCKEDWRITE (== TYPETEXTURE, 0x04),
//                       and whatever RwImageFindRasterFormat returns (PlayerSkin).
//   rwRASTERDONTALLOCATE: the d3d9 raster is created without a texture (sub rasters, app_camera.cpp's camera raster).
// A failed create (D3D refused the texture) returns NULL like RW; librw drops its half-built raster in that case (a few hundred bytes leak).
RwRaster* RwRasterCreate(RwInt32 width, RwInt32 height, RwInt32 depth, RwInt32 flags) {
    RwRaster* r = CreateRaster(width, height, depth, flags);
    if (!r && (flags & rwRASTERFORMATAUTOMIPMAP)) {
        // driver without D3DUSAGE_AUTOGENMIPMAP: fall back to an explicit chain
        r = rw::Raster::create(width, height, depth, flags & ~rwRASTERFORMATAUTOMIPMAP);
    }
    return r;
}

// A: RW destroys a locked raster after dropping the locks; a sub raster never owns the texture (its native texture pointer stays NULL here), the
// parent is untouched.
RwBool RwRasterDestroy(RwRaster* raster) {
    if (!raster) {
        return FALSE;
    }
    if (raster->privateFlags & kPixelLockMask) {
        RwRasterUnlock(raster);
    }
    if (raster->privateFlags & kPaletteLockMask) {
        RwRasterUnlockPalette(raster);
    }
    raster->destroy();
    return TRUE;
}

// A: RW returns NULL when the raster is already locked, when the level does not exist or the raster has no storage. `lockMode` is
// rwRASTERLOCKREAD (2) / WRITE (1) / NOFETCH (4) / RAW (8), the same values as rw::Raster::LOCK*. Camera rasters can only be read (a copy of
// the render target is fetched into system memory) and need NOFETCH stripped (librw misreads bit 4 as "write" there).
// Sub rasters lock the parent and return the pointer to the sub rectangle (level 0 only); `stride` is the parent's pitch.
// After a successful lock RwRasterGetStride(raster) is the pitch of the locked surface.
RwUInt8* RwRasterLock(RwRaster* raster, RwUInt8 level, RwInt32 lockMode) {
    if (!raster || !HasLockableStorage(raster) || (raster->privateFlags & kPixelLockMask)) {
        return nullptr;
    }
    RwRaster* root = Root(raster);
    if (root->privateFlags & kPixelLockMask) {
        return nullptr;
    }
    const int rootType = root->type & rwRASTERTYPEMASK;
    const bool isCam = rootType == rwRASTERTYPECAMERA || rootType == rwRASTERTYPECAMERATEXTURE;
    if (isCam) {
        if (lockMode & rwRASTERLOCKWRITE) {
            return nullptr;
        }
        lockMode &= ~rwRASTERLOCKNOFETCH;
    }
    if (rootType != rwRASTERTYPECAMERA && level >= root->getNumLevels()) {
        return nullptr;
    }
    if (IsSub(raster) && level != 0) {
        return nullptr;
    }
    uint8_t* p = rootType == rwRASTERTYPECAMERATEXTURE ? LockCameraTexture(root, level, lockMode) : root->lock(level, lockMode);
    if (!p) {
        return nullptr;
    }
    if (!IsSub(raster)) {
        return p;
    }
    const D3dRaster* nr = Ext(root);
    raster->pixels = p + raster->offsetY * root->stride + raster->offsetX * (int)nr->bpp;
    raster->stride = root->stride;
    raster->privateFlags |= root->privateFlags & kPixelLockMask;
    return raster->pixels;
}

// A: unlocking a raster that is not locked is a no-op (librw would dereference its NULL lockedSurf). The locked level is remembered by the
// D3D surface, RW's signature has no level.
RwRaster* RwRasterUnlock(RwRaster* raster) {
    if (!raster) {
        return nullptr;
    }
    RwRaster* root = Root(raster);
    if (root->privateFlags & kPixelLockMask) {
        root->unlock(0);
    }
    if (root != raster) {
        raster->pixels = raster->originalPixels;
        raster->stride = raster->originalStride;
        raster->privateFlags &= ~kPixelLockMask;
    }
    return raster;
}

// W (declared in rwextra.h): the palette of a P8 raster is D3dRaster::palette, 4 bytes (R,G,B,A) per entry, 256 entries; NULL for non
// paletted rasters or when already locked.
RwUInt8* RwRasterLockPalette(RwRaster* raster, RwInt32 lockMode) {
    if (!raster || !IsPaletted(raster) || (raster->privateFlags & kPaletteLockMask)) {
        return nullptr;
    }
    auto* pal = static_cast<RwUInt8*>(Ext(Root(raster))->palette);
    if (!pal) {
        return nullptr;
    }
    if (lockMode & rwRASTERLOCKREAD) {
        raster->privateFlags |= rw::Raster::PRIVATELOCK_READ_PALETTE;
    }
    if (lockMode & rwRASTERLOCKWRITE) {
        raster->privateFlags |= rw::Raster::PRIVATELOCK_WRITE_PALETTE;
    }
    if (!(lockMode & (rwRASTERLOCKREAD | rwRASTERLOCKWRITE))) {
        return nullptr;
    }
    return pal;
}

// W: only clears the lock bits; D3D9 P8 textures take their palette from the device, which nothing in the shim sets (P8 is unsupported by
// current devices and the shim's FindRasterFormat/SetFromImage expand palettized images to 32 bit, see isP8supported).
RwRaster* RwRasterUnlockPalette(RwRaster* raster) {
    if (raster) {
        raster->privateFlags &= ~kPaletteLockMask;
    }
    return raster;
}

// W: offsets are stored by SubRaster, relative to the root raster.
RwRaster* RwRasterGetOffset(RwRaster* raster, RwInt16* xOffset, RwInt16* yOffset) {
    if (!raster) {
        return nullptr;
    }
    if (xOffset) *xOffset = (RwInt16)raster->offsetX;
    if (yOffset) *yOffset = (RwInt16)raster->offsetY;
    return raster;
}

// A: mip levels of the d3d9 texture (1 for everything that is not a texture). Returns 0 for a raster without storage (RW asserts).
RwInt32 RwRasterGetNumLevels(RwRaster* raster) {
    if (!raster) {
        return 0;
    }
    RwRaster* root = Root(raster);
    switch (root->type & rwRASTERTYPEMASK) {
    case rwRASTERTYPENORMAL:
    case rwRASTERTYPETEXTURE:
    case rwRASTERTYPECAMERATEXTURE:
        return Ext(root)->texture ? root->getNumLevels() : 0;
    }
    return 1;
}

// W: RW rules: `subRaster` must be a childless raster created with rwRASTERDONTALLOCATE, `rect` must lie inside `raster`; the sub raster inherits
// type / format / depth and shares the root's texture (its own native texture pointer stays NULL so destroying it never releases the parent's).
// Returns NULL on any violation.
RwRaster* RwRasterSubRaster(RwRaster* subRaster, RwRaster* raster, RwRect* rect) {
    if (!subRaster || !raster || !rect || subRaster == raster || !(subRaster->flags & rwRASTERDONTALLOCATE) || IsSub(subRaster)) {
        return nullptr;
    }
    if (rect->x < 0 || rect->y < 0 || rect->w <= 0 || rect->h <= 0 || rect->x + rect->w > raster->width || rect->y + rect->h > raster->height) {
        return nullptr;
    }
    subRaster->subRaster(raster, rect);                  // width/height, offsets (relative to the root), parent = root
    subRaster->type   = raster->type;
    subRaster->format = raster->format;
    subRaster->depth  = raster->depth;
    subRaster->stride = raster->stride;
    subRaster->originalWidth  = subRaster->width;
    subRaster->originalHeight = subRaster->height;
    subRaster->originalStride = raster->stride;
    subRaster->originalPixels = nullptr;
    subRaster->pixels = nullptr;
    D3dRaster* s = Ext(subRaster);
    const D3dRaster* p = Ext(Root(raster));
    s->format = p->format; s->bpp = p->bpp; s->hasAlpha = p->hasAlpha; s->customFormat = p->customFormat; s->autogenMipmap = false;
    return subRaster;
}

// D: stack of 32 in librw's raster module (RW: unbounded in practice, the game nests 1).
RwRaster* RwRasterPushContext(RwRaster* raster) {
    return rw::Raster::pushContext(raster);
}
RwRaster* RwRasterPopContext(void) {
    return rw::Raster::popContext();
}
RwRaster* RwRasterGetCurrentContext(void) {
    return rw::Raster::getCurrentContext();
}

// A: RW returns the raster on success and NULL when the combination is unsupported. librw implements exactly the one SA needs (the context raster is
// a camera texture, the source raster the camera's frame buffer: PostEffects' front-buffer copy) and dereferences the context unconditionally.
RwRaster* RwRasterRenderFast(RwRaster* raster, RwInt32 x, RwInt32 y) {
    RwRaster* ctx = rw::Raster::getCurrentContext();
    if (!raster || !ctx || !rw::d3d::d3ddevice || !Ext(Root(ctx))->texture) {
        return nullptr;
    }
    return raster->renderFast(x, y) ? raster : nullptr;
}

// W: textured quad on the current camera's target at (x, y), 1:1, point sampled, alpha blended when the raster has alpha. RW drew into the
// context raster; the shim supports the only case SA could hit (context = camera frame buffer being updated). NULL outside Begin/EndUpdate.
RwRaster* RwRasterRender(RwRaster* raster, RwInt32 x, RwInt32 y) {
    if (!raster) {
        return nullptr;
    }
    return DrawQuad(raster, float(x), float(y), float(raster->width), float(raster->height), false);
}

// W: as RwRasterRender but stretched to `rect` (bilinear).
RwRaster* RwRasterRenderScaled(RwRaster* raster, RwRect* rect) {
    if (!raster || !rect) {
        return nullptr;
    }
    return DrawQuad(raster, float(rect->x), float(rect->y), float(rect->w), float(rect->h), true);
}

// W: clears the context raster (RwRasterPushContext) to a pixel value in the raster's own format (RwRGBAToPixel). Texture rasters are filled
// through a lock, camera rasters through the device (back buffer / render target texture). Z buffers: FALSE (not supported by the d3d9 driver either).
RwBool RwRasterClear(RwInt32 pixelValue) {
    return RwRasterClearRect(nullptr, pixelValue);
}

RwBool RwRasterClearRect(RwRect* rect, RwInt32 pixelValue) {
    RwRaster* ctx = rw::Raster::getCurrentContext();
    if (!ctx) {
        return FALSE;
    }
    switch (ctx->type & rwRASTERTYPEMASK) {
    case rwRASTERTYPECAMERA:
    case rwRASTERTYPECAMERATEXTURE:
        return DeviceClear(ctx, rect, (uint32_t)pixelValue);
    case rwRASTERTYPENORMAL:
    case rwRASTERTYPETEXTURE: {
        const int bpp = Ext(Root(ctx))->bpp;
        if (bpp != 1 && bpp != 2 && bpp != 4) {
            return FALSE;
        }
        RwRect all{ 0, 0, ctx->width, ctx->height };
        if (!rect) {
            rect = &all;
        }
        const int x0 = rect->x < 0 ? 0 : rect->x, y0 = rect->y < 0 ? 0 : rect->y;
        const int x1 = rect->x + rect->w > ctx->width ? ctx->width : rect->x + rect->w;
        const int y1 = rect->y + rect->h > ctx->height ? ctx->height : rect->y + rect->h;
        uint8_t* base = RwRasterLock(ctx, 0, rwRASTERLOCKWRITE);
        if (!base) {
            return FALSE;
        }
        const int stride = ctx->stride;
        for (int y = y0; y < y1; ++y) {
            uint8_t* row = base + y * stride;
            for (int x = x0; x < x1; ++x) {
                std::memcpy(row + x * bpp, &pixelValue, bpp);   // little endian: low bytes of the pixel word
            }
        }
        RwRasterUnlock(ctx);
        return TRUE;
    }
    }
    return FALSE;
}

// A: `dev` is the window handle in RW and unused by librw (the swap chain belongs to the device); flags = rwRASTERFLIPWAITVSYNC or 0.
RwRaster* RwRasterShowRaster(RwRaster* raster, void* /*dev*/, RwUInt32 flags) {
    if (!raster || !rw::d3d::d3ddevice) {
        return nullptr;
    }
    raster->show(flags);
    return raster;
}

// W (full RW behaviour; see the header comment): a raster of type NORMAL / TEXTURE with a texture, the same size as the image. The image is
// converted to the raster format: 8888, 888 (X8R8G8B8), 1555, 555, 565, 4444, LUM8; PAL8 / PAL4 rasters take an image of depth 8 / 4 (indices +
// palette); palettized images go through their palette when the raster is true colour. Returns NULL for anything else.
RwRaster* RwRasterSetFromImage(RwRaster* raster, RwImage* image) {
    if (!raster || !image || !image->pixels || IsSub(raster)) {
        return nullptr;
    }
    const int t = raster->type & rwRASTERTYPEMASK;
    if (t != rwRASTERTYPENORMAL && t != rwRASTERTYPETEXTURE) {
        return nullptr;
    }
    D3dRaster* nr = Ext(raster);
    if (!nr->texture || nr->customFormat || image->width != raster->width || image->height != raster->height) {
        return nullptr;
    }
    const int fmt = raster->format;
    const bool pal = IsPaletted(raster);
    if (pal && (image->depth > 8 || !image->palette || !nr->palette)) {
        return nullptr;
    }
    if (!pal && rwshim::FormatBytes(fmt) != (int)nr->bpp) {
        return nullptr;
    }
    if (!pal) {
        uint32_t probe;
        if (!rwshim::EncodeTexel({ 0, 0, 0, 0 }, fmt, &probe)) {
            return nullptr;
        }
    }
    if (pal) {
        const int n = (fmt & rwRASTERFORMATPAL4) ? 16 : 256;
        const int have = 1 << image->depth;
        std::memset(nr->palette, 0, n * 4);
        std::memcpy(nr->palette, image->palette, (have < n ? have : n) * 4);
    }
    uint8_t* base = RwRasterLock(raster, 0, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
    if (!base) {
        return nullptr;
    }
    const int stride = raster->stride;
    const int bpp = (int)nr->bpp;
    for (int y = 0; y < image->height; ++y) {
        uint8_t* row = base + y * stride;
        if (pal) {
            std::memcpy(row, image->pixels + (size_t)y * image->stride, image->width);
            continue;
        }
        for (int x = 0; x < image->width; ++x) {
            uint32_t texel = 0;
            rwshim::EncodeTexel(rwshim::ImageGet(image, x, y), fmt, &texel);
            std::memcpy(row + x * bpp, &texel, bpp);
        }
    }
    RwRasterUnlock(raster);
    return raster;
}

// A (RwRGBAToPixel, exe 0x803740 -> D3D9 0x7FEE20): the texel word of a colour in a raster format (what JPegCompress writes into a locked 8888 / 888
// raster). Only `rasterFormat & 0xF00` counts (PAL / mipmap bits are ignored); 8888 keeps the alpha, 888 is X8R8G8B8 with 0xFF in the unused byte,
// 1555 keeps alpha bit 7 in bit 15, 555 / 565 / 4444 truncate the channels, LUM8 is (r*30+g*59+b*11)/100 * a / 255, and colour format 0 is encoded
// as 8888 (the exe calls itself with 0x500). Other formats give 0 (the exe raises an error and returns an uninitialised word).
RwUInt32 RwRGBAToPixel(RwRGBA* rgbIn, RwInt32 rasterFormat) {
    uint32_t px = 0;
    if (rgbIn) {
        if ((rasterFormat & 0x0F00) == 0) {
            rasterFormat = rwRASTERFORMAT8888;
        }
        rwshim::EncodeTexel({ rgbIn->red, rgbIn->green, rgbIn->blue, rgbIn->alpha }, rasterFormat, &px);
    }
    return px;
}

// A: RwRasterRead(name) = RwImageRead + FindRasterFormat + Create + SetFromImage; `filename` has no extension (RwImageRead tries the registered ones).
RwRaster* RwRasterRead(const RwChar* filename) {
    RwImage* image = RwImageRead(filename);
    if (!image) {
        return nullptr;
    }
    RwInt32 w, h, d, f;
    RwRaster* raster = nullptr;
    if (RwImageFindRasterFormat(image, rwRASTERTYPETEXTURE, &w, &h, &d, &f)) {
        raster = RwRasterCreate(w, h, d, f);
        if (raster && !RwRasterSetFromImage(raster, image)) {
            RwRasterDestroy(raster);
            raster = nullptr;
        }
    }
    RwImageDestroy(image);
    return raster;
}

RwRaster* RwRasterReadMaskedRaster(const RwChar* filename, const RwChar* maskname) {
    RwImage* image = RwImageReadMaskedImage(filename, maskname);
    if (!image) {
        return nullptr;
    }
    RwInt32 w, h, d, f;
    RwRaster* raster = nullptr;
    if (RwImageFindRasterFormat(image, rwRASTERTYPETEXTURE, &w, &h, &d, &f)) {
        raster = RwRasterCreate(w, h, d, f);
        if (raster && !RwRasterSetFromImage(raster, image)) {
            RwRasterDestroy(raster);
            raster = nullptr;
        }
    }
    RwImageDestroy(image);
    return raster;
}

// librw allocates rasters with rwMalloc: nothing to tune.
void RwRasterSetFreeListCreateParams(RwInt32 /*blockSize*/, RwInt32 /*numBlocksToPrealloc*/) {}
#endif
