#pragma once
// Stream R / R-3b: GL3 parts of standalone/rw/raster.cpp (raster_gl3.cpp). Portable build only.
#include <cstdint>

#include "fakerw.h"
#include <src/gl/rwgl3impl.h>

using rw::gl3::Gl3Raster;

//! Started librw device with a current GL context.
bool NotsaGl3DeviceLive();

//! RwRasterClear / ClearRect on a CAMERA (window back buffer) or CAMERATEXTURE raster: clears the colour of `rect` (raster coordinates, null = all) to
//! `argb` (0xAARRGGBB, a D3DCOLOR). Framebuffer binding and scissor state are restored.
bool NotsaGl3DeviceClear(RwRaster* ctx, const RwRect* rect, std::uint32_t argb);

//! RwRasterRender / RenderScaled: textured Im2D quad at (x, y, w, h) (frame buffer pixels, top-down) onto the current camera. Null outside Begin/EndUpdate.
RwRaster* NotsaGl3DrawQuad(RwRaster* raster, float x, float y, float w, float h, bool filtered);

//! Reverses the row order of the pixels of a locked raster (rasterLock gave bottom-up rows; RW / D3D users expect the top row first).
void NotsaGl3FlipLockedRows(RwRaster* root);

//! RwRasterSetFromImage for gl3 rasters (8888 RGBA, 888 RGB, 1555 R5G5B5A1 in gl3's byte order); null on any mismatch.
RwRaster* NotsaGl3RasterSetFromImage(RwRaster* raster, RwImage* image);

//! Texture orientation (R-4). librw's gl3 fragment shaders sample at (u, 1 - v): they expect GL row order (first uploaded row = bottom), which is also what a
//! rendered camera texture (FBO) has. Game rasters hold D3D order (top row first; lock / unlock / the TXD reader upload it as is, DXT blocks included), so
//! immediate-mode vertices of a draw with such a texture bound must use v' = 1 - v for the shader flip to cancel. A CAMERATEXTURE raster needs no flip.
//! `tex` = the TEXTURERASTER bound for the draw (null = untextured: no flip needed, the value is irrelevant).
inline bool NotsaGl3TextureNeedsVFlip(const RwRaster* tex) {
    if (!tex) {
        return false;
    }
    const RwRaster* root = (tex->parent && tex->parent != tex) ? tex->parent : tex;
    return (root->type & rwRASTERTYPEMASK) != rwRASTERTYPECAMERATEXTURE;
}
