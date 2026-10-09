// P2B-03c: SA's RW patches for streaming TXDs (rwtexdict.h): RwTextureGtaStreamRead (internal), RwTexDictionaryGtaStreamRead / Read1 / Read2.
// Function class W (no librw counterpart): the exe's own patch of the RW 3.6 texture dictionary reader, reproduced from
// 0x730E60 (RwTextureGtaStreamRead), 0x730FC0 (RwTexDictionaryGtaStreamRead), 0x731070 (Read1), 0x731150 (Read2).
//
//   * RwTextureGtaStreamRead(stream): findChunk(0x15 TEXTURENATIVE) (no version check) -> D3D9 native reader (texture.cpp, PAL8 expansion) -> NO
//     plugin data read (the texture extension chunk inside the 0x15 chunk is skipped by the next findChunk) -> two SA patches:
//       - filter NEAREST -> LINEAR and MIPNEAREST -> MIPLINEAR (byte 0 of filterAddressing, set to 2 / 4: the game never shows point filtered textures);
//       - when the GPU does anisotropic filtering (anisotropySupportedByGFX 0xC87FFC) and the texture's max anisotropy is >= 1 and the FX quality is
//         >= HIGH (g_fx.GetFxQuality() >= 2): RpAnisotTextureSetMaxAnisotropy(tex, RpAnisotGetMaxSupportedMaxAnisotropy()) (librw: Texture::setMaxAnisotropy,
//         a no-op unless the anisotropy plugin is registered; rw::getMaxSupportedMaxAnisotropy = D3D caps).
//     (The exe also keeps a running average of the load time in 0xC87FEC / 0xC87FF0 when [0xC8D4C0] == 8, a profiler hook: not reproduced.)
//   * RwTexDictionaryGtaStreamRead: STRUCT {u16 n, u16 deviceId} (header size must be read in full, device id ignored) -> dictionary -> n x
//     (RwTextureGtaStreamRead + AddTexture at the FRONT); a failure destroys the textures read so far and the dictionary and returns NULL.
//     The dictionary's own plugin chunk is NOT read.
//   * Read1 (streaming thread half): same header, then reads n - n/2 textures, remembers the stream position (0xC87FE8, the memory stream's
//     `position`) and the number still to read (0x8D6088 = n/2); returns the dictionary. Read2 (main thread half): skips the stream to the remembered
//     position (RwStreamSkip(saved - current)), reads the remaining textures into `dict`, decrements the counter once more (it ends at -1) and
//     returns `dict`. A failure in either half destroys what `dict` holds so far. The statics are global like the exe's (a single streaming job at a time).
// Needs fakerw + librw (+ the game's Fx/WinPlatform globals unless NOTSA_RW_UNIT_TEST, where two plain globals stand in). Excluded from unity builds.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

#ifdef NOTSA_RW_UNIT_TEST
bool rwshim_AnisotropySupportedByGFX = false; // set by tests/standalone/rw_texture_test.cpp
int  rwshim_FxQuality                = 0;
static bool AnisotropyWanted() { return rwshim_AnisotropySupportedByGFX; }
static bool FxQualityAtLeastHigh() { return rwshim_FxQuality >= 2; }
#else
#include "Fx/Fx.h"
#include "platform/win/WinPlatform.h"
static bool AnisotropyWanted() { return anisotropySupportedByGFX; }
static bool FxQualityAtLeastHigh() { return g_fx.GetFxQuality() >= FX_QUALITY_HIGH; }
#endif

namespace rwshim { RwTexture* ReadNativeTexture(rw::Stream* stream); } // texture.cpp

namespace {
int32_t  s_remaining = 0; // 0x8D6088: textures Read2 still has to read
uint32_t s_savedPos  = 0; // 0xC87FE8: stream position after Read1's textures

RwTexture* TextureGtaStreamRead(RwStream* stream) {
    if (!rw::findChunk(stream, rw::ID_TEXTURENATIVE, nullptr, nullptr)) {
        return nullptr;
    }
    RwTexture* tex = rwshim::ReadNativeTexture(stream);
    if (!tex) {
        return nullptr;
    }
    switch (tex->filterAddressing & 0xFF) {
    case rw::Texture::NEAREST:    tex->filterAddressing = (tex->filterAddressing & ~0xFFu) | rw::Texture::LINEAR; break;
    case rw::Texture::MIPNEAREST: tex->filterAddressing = (tex->filterAddressing & ~0xFFu) | rw::Texture::MIPLINEAR; break;
    }
    if (AnisotropyWanted() && tex->getMaxAnisotropy() >= 1 && FxQualityAtLeastHigh()) {
        tex->setMaxAnisotropy(rw::getMaxSupportedMaxAnisotropy());
    }
    return tex;
}

// header: STRUCT chunk {u16 numTextures, u16 deviceId}; returns the number of textures or -1
int32_t ReadHeader(RwStream* stream) {
    if (!rw::findChunk(stream, rw::ID_STRUCT, nullptr, nullptr)) {
        return -1;
    }
    const int32_t n = stream->readU16();
    stream->readU16();
    return n;
}

RwTexDictionary* Fail(RwTexDictionary* dict) {
    RwTexDictionaryDestroy(dict); // librw's destroy also destroys the textures (exe: ForAllTextures(destroy) + Destroy)
    return nullptr;
}
} // namespace

RwTexDictionary* RwTexDictionaryGtaStreamRead(RwStream* stream) {
    int32_t n = ReadHeader(stream);
    if (n < 0) {
        return nullptr;
    }
    RwTexDictionary* dict = RwTexDictionaryCreate();
    if (!dict) {
        return nullptr;
    }
    for (; n > 0; --n) {
        RwTexture* tex = TextureGtaStreamRead(stream);
        if (!tex) {
            return Fail(dict);
        }
        RwTexDictionaryAddTexture(dict, tex);
    }
    return dict;
}

RwTexDictionary* RwTexDictionaryGtaStreamRead1(RwStream* stream) {
    s_remaining = 0;
    int32_t n = ReadHeader(stream);
    if (n < 0) {
        return nullptr;
    }
    RwTexDictionary* dict = RwTexDictionaryCreate();
    if (!dict) {
        return nullptr;
    }
    const int32_t half = n >> 1;
    s_remaining        = half;
    for (; n > half; --n) {
        RwTexture* tex = TextureGtaStreamRead(stream);
        if (!tex) {
            return Fail(dict);
        }
        RwTexDictionaryAddTexture(dict, tex);
    }
    s_savedPos  = stream->tell();
    s_remaining = n;
    return dict;
}

RwTexDictionary* RwTexDictionaryGtaStreamRead2(RwStream* stream, RwTexDictionary* dict) {
    stream->seek((int32_t)s_savedPos - (int32_t)stream->tell());
    while (s_remaining) {
        --s_remaining;
        RwTexture* tex = TextureGtaStreamRead(stream);
        if (!tex) {
            return Fail(dict);
        }
        RwTexDictionaryAddTexture(dict, tex);
    }
    --s_remaining;
    return dict;
}
#endif
