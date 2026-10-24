// Stream R, slice R-3b: GL3 parts of standalone/rw/raster.cpp (portable build only; raster.cpp keeps the renderer neutral logic and calls these).
//   NotsaGl3DeviceClear      RwRasterClear / ClearRect on camera rasters (D3D: SetRenderTarget + Clear)
//   NotsaGl3DrawQuad         RwRasterRender / RenderScaled (Im2D quad)
//   NotsaGl3FlipLockedRows   top-down rows for locked camera rasters
//   NotsaGl3RasterSetFromImage  RwRasterSetFromImage on gl3 rasters
// Raster storage under gl3 (vendor/librw gl3raster.cpp): NORMAL / TEXTURE / CAMERATEXTURE are GL texture objects, CAMERA is the window's back buffer and
// ZBUFFER a depth+stencil texture. R-5: with rw::gl3::d3dTexelOrder (set by RwEngineOpen) the texel layout of NORMAL / TEXTURE rasters is the d3d9 one
// (8888 = B,G,R,A bytes, 888 = X8R8G8B8 4 bytes, 1555 / 565 / 555 / 4444 words, LUM8), so rwpixel.h, RwRGBAToPixel and the game's lock users work unchanged;
// no PAL (the TXD reader expands), DXT through the TXD reader. CAMERATEXTURE / CAMERA keep gl3's RGBA / RGB layout (R-11b).
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

#include <cstring>
#include <vector>

#include "raster_gl3.h" // + librw gl3 internals (rwgl3impl.h is unguarded: included once, here)
#include "standalone/rw/rwpixel.h"

bool NotsaGl3DeviceLive() {
    return rw::engine && rw::Engine::state == rw::Engine::Started && SDL_GL_GetCurrentContext() != nullptr;
}

void NotsaGl3FlipLockedRows(RwRaster* root) {
    if (!root || !root->pixels || root->height < 2 || root->stride <= 0) {
        return;
    }
    const size_t        stride = static_cast<size_t>(root->stride);
    std::vector<uint8_t> tmp(stride);
    for (int a = 0, b = root->height - 1; a < b; a++, b--) {
        uint8_t* ra = root->pixels + static_cast<size_t>(a) * stride;
        uint8_t* rb = root->pixels + static_cast<size_t>(b) * stride;
        std::memcpy(tmp.data(), ra, stride);
        std::memcpy(ra, rb, stride);
        std::memcpy(rb, tmp.data(), stride);
    }
}

bool NotsaGl3DeviceClear(RwRaster* ctx, const RwRect* rect, std::uint32_t argb) {
    if (!ctx || !NotsaGl3DeviceLive()) {
        return false;
    }
    RwRaster* root = (ctx->parent && ctx->parent != ctx) ? ctx->parent : ctx;
    const int type = root->type & rwRASTERTYPEMASK;
    GLuint    fbo  = 0;
    int       fbH  = 0;
    if (type == rwRASTERTYPECAMERA) {
        int w = 0;
        SDL_GetWindowSizeInPixels(rw::gl3::glGlobals.window, &w, &fbH);
    } else if (type == rwRASTERTYPECAMERATEXTURE) {
        fbo = GETGL3RASTEREXT(root)->fbo;
        fbH = root->height;
        if (!fbo) {
            return false; // never created as a render target
        }
    } else {
        return false;
    }
    GLint prevFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    rw::gl3::bindFramebuffer(fbo);
    const GLboolean scissorWas = glIsEnabled(GL_SCISSOR_TEST);
    GLint           prevScissor[4];
    if (rect) {
        glGetIntegerv(GL_SCISSOR_BOX, prevScissor);
        const int x = ctx->offsetX + rect->x, y = ctx->offsetY + rect->y; // top-down raster coordinates
        glScissor(x, fbH - (y + rect->h), rect->w, rect->h);
        glEnable(GL_SCISSOR_TEST);
    } else if (scissorWas) {
        glDisable(GL_SCISSOR_TEST);
    }
    GLboolean prevMask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, prevMask);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(static_cast<float>((argb >> 16) & 0xFF) / 255.0f, static_cast<float>((argb >> 8) & 0xFF) / 255.0f, static_cast<float>(argb & 0xFF) / 255.0f,
                 static_cast<float>((argb >> 24) & 0xFF) / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(prevMask[0], prevMask[1], prevMask[2], prevMask[3]);
    if (rect) {
        glScissor(prevScissor[0], prevScissor[1], prevScissor[2], prevScissor[3]);
        if (!scissorWas) {
            glDisable(GL_SCISSOR_TEST);
        }
    } else if (scissorWas) {
        glEnable(GL_SCISSOR_TEST);
    }
    rw::gl3::bindFramebuffer(static_cast<GLuint>(prevFbo));
    return true;
}

RwRaster* NotsaGl3DrawQuad(RwRaster* raster, float x, float y, float w, float h, bool filtered) {
    if (!raster || !rw::engine || !rw::engine->currentCamera || !NotsaGl3DeviceLive()) {
        return nullptr;
    }
    const RwRaster* root = (raster->parent && raster->parent != raster) ? raster->parent : raster;
    const int       t    = root->type & rwRASTERTYPEMASK;
    if ((t != rwRASTERTYPENORMAL && t != rwRASTERTYPETEXTURE && t != rwRASTERTYPECAMERATEXTURE) || !GETGL3RASTEREXT(const_cast<RwRaster*>(root))->texid) {
        return nullptr;
    }
    // sub raster: UV window inside the parent's texture
    const float u0 = float(raster->offsetX) / float(root->width), v0 = float(raster->offsetY) / float(root->height);
    const float u1 = float(raster->offsetX + raster->width) / float(root->width), v1 = float(raster->offsetY + raster->height) / float(root->height);

    const float z = rw::im2d::GetNearZ();
    rw::gl3::Im2DVertex q[4];
    const float xs[4] = { x, x + w, x, x + w }, ys[4] = { y, y, y + h, y + h };
    const float us[4] = { u0, u1, u0, u1 }, vs[4] = { v0, v0, v1, v1 };
    for (int i = 0; i < 4; ++i) {
        q[i].setScreenX(xs[i]);
        q[i].setScreenY(ys[i]);
        q[i].setScreenZ(z);
        q[i].setCameraZ(1.0f);
        q[i].setColor(255, 255, 255, 255);
        q[i].setU(us[i], 1.0f);
        q[i].setV(NotsaGl3TextureNeedsVFlip(root) ? 1.0f - vs[i] : vs[i], 1.0f); // R-4: shader samples 1 - v, game rasters are top-row first
    }
    RwRaster* tex = const_cast<RwRaster*>(root);
    const auto savedRaster = rw::GetRenderStatePtr(rw::TEXTURERASTER);
    uint32_t sav[5] = { rw::GetRenderState(rw::TEXTUREFILTER), rw::GetRenderState(rw::VERTEXALPHA), rw::GetRenderState(rw::ZTESTENABLE),
                        rw::GetRenderState(rw::ZWRITEENABLE), rw::GetRenderState(rw::SRCBLEND) };
    const uint32_t savDst = rw::GetRenderState(rw::DESTBLEND);
    rw::SetRenderStatePtr(rw::TEXTURERASTER, tex);
    rw::SetRenderState(rw::TEXTUREFILTER, filtered ? rw::Texture::LINEAR : rw::Texture::NEAREST);
    rw::SetRenderState(rw::VERTEXALPHA, rw::Raster::formatHasAlpha(tex->format));
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

RwRaster* NotsaGl3RasterSetFromImage(RwRaster* raster, RwImage* image) {
    if (!raster || !image || !image->pixels || (raster->parent && raster->parent != raster)) {
        return nullptr;
    }
    const int t = raster->type & rwRASTERTYPEMASK;
    if (t != rwRASTERTYPENORMAL && t != rwRASTERTYPETEXTURE) {
        return nullptr;
    }
    Gl3Raster* nr = GETGL3RASTEREXT(raster);
    if (!nr->texid || nr->isCompressed || image->width != raster->width || image->height != raster->height) {
        return nullptr;
    }
    // d3d9 texel words (librw gl3 d3dTexelOrder, set by RwEngineOpen): the same conversion as the D3D build (rwpixel.h)
    const int fmt = raster->format;
    const int bpp = nr->bpp;
    if (rwshim::FormatBytes(fmt) != bpp || (fmt & (rwRASTERFORMATPAL8 | rwRASTERFORMATPAL4))) {
        return nullptr;
    }
    uint32_t probe;
    if (!rwshim::EncodeTexel({ 0, 0, 0, 0 }, fmt, &probe)) {
        return nullptr;
    }
    uint8_t* base = raster->lock(0, rw::Raster::LOCKWRITE | rw::Raster::LOCKNOFETCH);
    if (!base) {
        return nullptr;
    }
    const int stride = raster->stride;
    for (int y = 0; y < image->height; ++y) {
        uint8_t* row = base + static_cast<size_t>(y) * static_cast<size_t>(stride);
        for (int x = 0; x < image->width; ++x) {
            uint32_t texel = 0;
            rwshim::EncodeTexel(rwshim::ImageGet(image, x, y), fmt, &texel);
            std::memcpy(row + x * bpp, &texel, bpp);
        }
    }
    raster->unlock(0);
    return raster;
}

#endif // NOTSA_RW_LIBRW
