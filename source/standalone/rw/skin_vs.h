// P2B-24a: the exe's D3D9 vertex-shader COMPOSER of the RenderWare skin pipeline ("vertexshader" module, 0x75EDD0..0x760CF0 + 0x75F0B0 front).
// See skin_vs.cpp for the addresses, the key layout and the deliberate differences.
#pragma once
#ifdef NOTSA_RW_LIBRW
#include <cstdint>
#include <string>
#include <vector>

struct IDirect3DVertexShader9;

namespace notsa::skinvs {

// The 4-byte shader key (the exe's "light descriptor" first dword; also the cache key). Every byte, every bit that influences the shader text:
//   b[0]  low nibble  number of group-A lights ("directional": c[5+2i] = direction, c[6+2i] = colour; 2 constants each)
//         high nibble number of group-B lights ("point": per chunk of 4 lights 1 packed constant (range / clamp), then position + colour per light)
//   b[1]  low nibble  number of group-C lights ("spot": per chunk 1 packed constant, then position, direction, cone (x = scale, y = offset), colour per light)
//         high nibble number of texture-coordinate sets copied / generated (v5.. of the vertex format)
//   b[2]  bit 0       morph target (second position/normal stream v14/v15, lerp factors in the constant lay.reg[3])
//         bits 1..3   N = number of bone weights per vertex (0 = not skinned; 1 = one bone; 2..4 = blended; v1 weights, v2 bone indices)
//         bit 4       prelit vertex colours (v4)
//         bit 5       vertex normals (v3); honoured only when the shader lights or generates texture coordinates from normals (see skin_vs.cpp)
//         bit 6       normalise the (skinned) normal
//         bit 7       multiply the lit colour by the material colour (constant lay.reg[0])
//   b[3]  bits 0..1   fog mode (0 none, 1 linear, 2 exp, 3 exp2; constants lay.reg[1])
//         bits 2..7   M = texture coordinate generation mode (0 = copy the sets, 1..11 = env / projection modes, 8 and 9 = tangent space bump, other = copy)
struct Key { std::uint8_t b[4]; };
using LightKey = Key;

// The 8 bytes the exe stores next to the shader in its cache ("key2"): the vertex-shader constant register of each block, 0xFF = absent.
//   reg[0] material colour (key.b[2] bit 7)       reg[1] fog parameters (fog mode != 0)      reg[2] texture-coordinate generation matrices (M != 0)
//   reg[3] morph interpolation factors (b[2] bit 0)    reg[4] first bone matrix (N != 0; 3 consecutive float4 per bone)     reg[5..7] unused (0xFF)
// Constants below the first light: c0..c3 projection, c4 ambient, c5.. lights (see Key), then the blocks above in that order.
struct Layout { std::uint8_t reg[8]; };
using FlagsKey = Layout;

// The shader model of the text header, chosen by the exe's init (0x760CF0) from the device caps: 0x0A = vs_1_1, 0x14 = vs_2_0, 0x19 = vs_2_x (0 = unsupported).
inline constexpr int kModelVS11 = 0x0A, kModelVS20 = 0x14, kModelVS2x = 0x19;

int  NumLightConstants(const Key&);                                              // 0x75EDD0 (number of vertex-shader constants taken by the lights + ambient base c0..c4, i.e. the first free register)
Layout ComputeLayout(const Key&, unsigned maxConstants);                         // the register allocation of 0x75F0B0 (also done by ComposeText)
// 0x75F240 section: the shader source exactly as the exe's buffer (header, declarations, body). Empty for an unsupported model. `lay` receives the register layout.
std::string ComposeText(const Key&, Layout& lay, int model, unsigned maxConstants);
std::string ComposeText(const Key&, Layout& lay);                                // uses the model / constants chosen by Init() / SetModel()

void SetModel(int model, unsigned maxConstants);                                 // what 0x760CF0 stores in 0xC94C00 / 0xC94C04
int  Model();
unsigned MaxConstants();
void SetFrameStamp(std::uint16_t stamp);                                         // [RwEngineInstance + 8]: LRU timestamp of the cache entries (the render frame counter)

// 0x75EED0 + 0x75F0B0: cached lookup (binary search over <= 256 entries ordered by key, least-recently-used replacement when full). Returns the vertex shader
// (null if the device / assembler refused it; the miss is cached like in the exe) and the register layout. Needs rw::d3d::d3ddevice.
IDirect3DVertexShader9* GetVertexShader(const Key&, Layout& lay);
IDirect3DVertexShader9* GetVertexShader(const Key&);

void Init();        // 0x760CF0: reset the cache, pick model / constant count from the device caps (D3DCAPS9 VertexShaderVersion, VS20Caps, MaxVertexShaderConst)
void Shutdown();    // 0x75EE60: release the cached shaders, reset the cache

// Test hooks
int  CacheCount();                                                               // [0xC94BF8]
bool CacheEntryAt(int idx, std::uint32_t& key, std::uint32_t& stamp, IDirect3DVertexShader9*& shader, std::uint8_t layout[8]);   // raw entry idx of the cache array
void SetAssembleHook(IDirect3DVertexShader9* (*hook)(const std::string&));      // replaces D3DAssemble + CreateVertexShader (nullptr = real)
bool AssembleToBytes(const std::string& text, std::vector<std::uint8_t>& out);  // the run-time assembler (D3DAssemble, the exe's flags) -> bytecode; false on failure

} // namespace notsa::skinvs
#endif
