#pragma once
// Stream R / R-3b: camera twin helpers of the portable build (camera_gl3.cpp). Portable build only.
#include <cstdint>
#include <vector>

//! Reads the default framebuffer's BACK buffer (what the next present shows) as tightly packed 8 bit RGB, rows top-down (D3D / BMP convention).
//! Pixel size = window size in pixels. False without a GL context. Leaves GL pack state alone except GL_PACK_ALIGNMENT (set to 1).
bool NotsaRwReadBackBuffer(std::vector<std::uint8_t>& rgb, int& width, int& height);

//! RwD3D9SetStencilClear's value (defined in standalone/rw/camera.cpp, portable view)
std::uint32_t NotsaRwGetStencilClear();
