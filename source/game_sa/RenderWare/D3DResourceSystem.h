/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "D3DTextureBuffer.h"
#include "D3DIndexDataBuffer.h"
#ifdef _DX9_SDK_INSTALLED
#include "d3d9.h"
#endif

class D3DResourceSystem {
public:
    static constexpr int32 MAX_TEXTURE_BUFFERS    = 17;
    static constexpr int32 NUM_INDEX_DATA_BUFFERS = 16;

    static inline NOTSA_GLOBAL(UseD3DResourceBuffering, 0x8D6084, (bool), { true });
    static inline NOTSA_GLOBAL(NumTextureBuffers, 0xC87C60, (int32), {});
    static inline NOTSA_GLOBAL(TextureBuffers, 0xC87C68, (std::array<D3DTextureBuffer, MAX_TEXTURE_BUFFERS>), {}); // [0] is the buffer for small textures of any format
    static inline NOTSA_GLOBAL(IndexDataBuffers, 0xC87E48, (std::array<D3DIndexDataBuffer, NUM_INDEX_DATA_BUFFERS>), {});
    static inline NOTSA_GLOBAL(LargeIndexDataBuffer, 0xC87FC8, (D3DIndexDataBuffer), {}); // For buffers that don't fit into the ones above
    static inline NOTSA_GLOBAL(FreeTextureBufferIndex, 0xC87FE0, (int32), {});
    static inline NOTSA_GLOBAL(FreeIndexBufferIndex, 0xC87FE4, (int32), {});

    static void InjectHooks();

    static void CancelBuffering();
    static uint32 GetTotalIndexDataSize();
    static uint32 GetTotalPixelsSize();
    static void Init();
    static void SetUseD3DResourceBuffering(bool bUse);
    static void Shutdown();
    static void TidyUpD3DIndexBuffers(uint32 count);
    static void TidyUpD3DTextures(uint32 count);
    static int32 CreateIndexBuffer(uint32 numIndices, uint32 format, void** ppIndexBuffer);
    static int32 CreateTexture(int32 width, int32 height, int32 levels, uint32 format, void** ppTexture);
    static void DestroyIndexBuffer(void* pIndexBuffer);
    static void DestroyTexture(void* texture);
};
