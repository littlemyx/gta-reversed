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

    static inline auto& UseD3DResourceBuffering = StaticRef<bool>(0x8D6084);
    static inline auto& NumTextureBuffers       = StaticRef<int32>(0xC87C60);
    static inline auto& TextureBuffers          = StaticRef<std::array<D3DTextureBuffer, MAX_TEXTURE_BUFFERS>>(0xC87C68); // [0] is the buffer for small textures of any format
    static inline auto& IndexDataBuffers        = StaticRef<std::array<D3DIndexDataBuffer, NUM_INDEX_DATA_BUFFERS>>(0xC87E48);
    static inline auto& LargeIndexDataBuffer    = StaticRef<D3DIndexDataBuffer>(0xC87FC8); // For buffers that don't fit into the ones above
    static inline auto& FreeTextureBufferIndex  = StaticRef<int32>(0xC87FE0);
    static inline auto& FreeIndexBufferIndex    = StaticRef<int32>(0xC87FE4);

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
    static int32 CreateTexture(int32 width, int32 height, uint32 format, void** ppTexture);
    static void DestroyIndexBuffer(void* pIndexBuffer);
    static void DestroyTexture(void* texture);
};
