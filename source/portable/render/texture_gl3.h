#pragma once
// Stream R / R-5: D3D9 native texture (TXD chunk 0x15) -> librw gl3 raster. Portable build only; called from standalone/rw/texture.cpp (rwshim::ReadNativeTexture).
#include <cstdint>

#include "fakerw.h"

//! Reads the rest of a platform-9 (D3D9) TEXTURENATIVE struct (the stream is positioned right after the platform word) into a gl3 TEXTURE raster.
//! Uploaded as is, top row first, DXT blocks untouched (see raster_gl3.h NotsaGl3TextureNeedsVFlip). Formats: DXT1 / DXT3 / DXT5 (S3TC), 8888, 888
//! (X8R8G8B8; R8G8B8 files are widened), 1555, 565, 555, 4444, LUM8, PAL8 / PAL4 (expanded to 8888 / 888 at load, as the D3D build does). Returns NULL
//! and logs the reason on failure (unsupported FOURCC, cube map, level size mismatch, no S3TC, ...).
RwTexture* NotsaGl3ReadD3D9Texture(rw::Stream* stream);

//! Counters of the last NotsaGl3ReadD3D9Texture calls, for the TXD test (reset with NotsaGl3TextureStatsReset).
struct NotsaGl3TextureStats {
    unsigned loaded;
    unsigned failed;
    unsigned levelSizeMismatch; //!< a file level whose byte count differs from the raster's level size (data copied up to the smaller one)
    unsigned zeroLevels;        //!< DXT levels stored with size 0 (2x2 / 1x1), filled from the previous level
    unsigned glErrors;          //!< glGetError() != 0 after an upload
    unsigned npot;              //!< textures with a non power of two side
};
NotsaGl3TextureStats& NotsaGl3TextureStatsGet();
void                  NotsaGl3TextureStatsReset();
