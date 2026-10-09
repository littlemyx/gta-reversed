/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"

// 0x730900
void D3DResourceSystem::CancelBuffering() {
    // NOTE: The arrays are freed and their pointers nulled
    for (int32 i = 0; i < NumTextureBuffers; i++) {
        auto& buf = TextureBuffers[i];
        for (int32 j = 0; j < (int32)buf.m_nSize; j++) {
            ((IDirect3DTexture9*)buf.m_apTextures[j])->Release();
        }
        buf.m_nSize                = 0;
        buf.m_nNumTexturesInBuffer = 0;
        if (buf.m_apTextures) {
            operator delete(buf.m_apTextures);
            buf.m_apTextures = nullptr;
        }
    }
    NumTextureBuffers = 0;

    for (auto& buf : IndexDataBuffers) {
        for (int32 j = 0; j < (int32)buf.m_nSize; j++) {
            ((IDirect3DIndexBuffer9*)buf.m_apIndexData[j])->Release();
        }
        buf.m_nSize             = 0;
        buf.m_nNumDatasInBuffer = 0;
        if (buf.m_apIndexData) {
            operator delete(buf.m_apIndexData);
            buf.m_apIndexData = nullptr;
        }
    }

    auto& large = LargeIndexDataBuffer;
    for (int32 j = 0; j < (int32)large.m_nSize; j++) {
        ((IDirect3DIndexBuffer9*)large.m_apIndexData[j])->Release();
    }
    large.m_nSize             = 0;
    large.m_nNumDatasInBuffer = 0;
    if (large.m_apIndexData) {
        operator delete(large.m_apIndexData);
        large.m_apIndexData = nullptr;
    }

    UseD3DResourceBuffering = false;
}

// 0x7307F0
uint32 D3DResourceSystem::GetTotalIndexDataSize() {
    return plugin::CallAndReturn<uint32, 0x7307F0>();
}

// 0x730660
uint32 D3DResourceSystem::GetTotalPixelsSize() {
    return plugin::CallAndReturn<uint32, 0x730660>();
}

// 0x730830
void D3DResourceSystem::Init() {
    NumTextureBuffers       = 1;
    UseD3DResourceBuffering = true;

    auto& tex0       = TextureBuffers[0];
    tex0.m_nFormat   = 0;
    tex0.m_nWidth    = 0;
    tex0.m_nLevels   = -1;
    tex0.m_apTextures = (decltype(tex0.m_apTextures))operator new(16 * sizeof(void*));
    tex0.m_nCapcacity = 16;
    tex0.m_nSize      = 0;
    tex0.m_nNumTexturesInBuffer = 0;

    int32 field4 = 100;
    for (auto& buf : IndexDataBuffers) {
        buf.m_nFormat = 0x65;
        buf.field_4   = field4;
        buf.m_apIndexData       = (decltype(buf.m_apIndexData))operator new(16 * sizeof(void*));
        buf.m_nCapcacity        = 16;
        buf.m_nSize             = 0;
        buf.m_nNumDatasInBuffer = 0;
        field4 += 100;
    }

    auto& large               = LargeIndexDataBuffer;
    large.m_nFormat           = 0x65;
    large.field_4             = 0;
    large.m_apIndexData       = (decltype(large.m_apIndexData))operator new(16 * sizeof(void*));
    large.m_nCapcacity        = 16;
    large.m_nSize             = 0;
    large.m_nNumDatasInBuffer = 0;
}

// 0x730AC0
void D3DResourceSystem::SetUseD3DResourceBuffering(bool bUse) {
    ZoneScoped;

    UseD3DResourceBuffering = bUse;
}

// 0x730A00
void D3DResourceSystem::Shutdown() {
    // Same as CancelBuffering, except that the arrays are not freed and buffering isn't turned off
    for (int32 i = 0; i < NumTextureBuffers; i++) {
        auto& buf = TextureBuffers[i];
        for (int32 j = 0; j < (int32)buf.m_nSize; j++) {
            ((IDirect3DTexture9*)buf.m_apTextures[j])->Release();
        }
        buf.m_nSize                = 0;
        buf.m_nNumTexturesInBuffer = 0;
    }
    NumTextureBuffers = 0;

    for (auto& buf : IndexDataBuffers) {
        for (int32 j = 0; j < (int32)buf.m_nSize; j++) {
            ((IDirect3DIndexBuffer9*)buf.m_apIndexData[j])->Release();
        }
        buf.m_nSize             = 0;
        buf.m_nNumDatasInBuffer = 0;
    }

    auto& large = LargeIndexDataBuffer;
    for (int32 j = 0; j < (int32)large.m_nSize; j++) {
        ((IDirect3DIndexBuffer9*)large.m_apIndexData[j])->Release();
    }
    large.m_nSize             = 0;
    large.m_nNumDatasInBuffer = 0;
}

// 0x730740
void D3DResourceSystem::TidyUpD3DIndexBuffers(uint32 count) {
    if (count == 0) {
        return;
    }

    // First, free the buffers from the large index data buffer
    auto& large = LargeIndexDataBuffer;
    while (large.m_nSize != 0) {
        large.m_nNumDatasInBuffer--;
        large.m_nSize--;
        auto* const ib = (IDirect3DIndexBuffer9*)large.m_apIndexData[large.m_nSize];
        if (!ib) {
            break;
        }
        ib->Release();
        if (--count == 0) {
            return;
        }
    }

    // Then go over the other buffers round-robin, until `count` are freed, or 16 buffers in a row had nothing to free
    int32 idx    = FreeIndexBufferIndex;
    int32 misses = 0;
    while (count != 0 && misses <= 15) {
        auto& buf = IndexDataBuffers[idx];
        if (buf.m_nSize == 0) {
            misses++;
        } else {
            FreeIndexBufferIndex = idx;
            buf.m_nNumDatasInBuffer--;
            buf.m_nSize--;
            if (auto* const ib = (IDirect3DIndexBuffer9*)buf.m_apIndexData[buf.m_nSize]) {
                ib->Release();
                misses = 0;
                count--;
                idx = FreeIndexBufferIndex;
            } else {
                misses++;
            }
        }
        idx = (idx + 1) % NUM_INDEX_DATA_BUFFERS;
    }
    FreeIndexBufferIndex = idx;
}

// 0x7305E0
void D3DResourceSystem::TidyUpD3DTextures(uint32 count) {
    int32 misses = 0;
    int32 idx    = FreeTextureBufferIndex;
    int32 num    = NumTextureBuffers;
    while (count != 0 && misses < num) {
        auto& buf = TextureBuffers[idx];
        if (buf.m_nSize == 0) {
            misses++;
        } else {
            FreeTextureBufferIndex = idx;
            buf.m_nNumTexturesInBuffer--;
            buf.m_nSize--;
            if (auto* const tex = (IDirect3DTexture9*)buf.m_apTextures[buf.m_nSize]) {
                tex->Release();
                misses = 0;
                count--;
                idx    = FreeTextureBufferIndex;
                num    = NumTextureBuffers;
            } else {
                misses++;
            }
        }
        idx = (idx + 1) % num; // `num` can't be 0 here: the loop condition above exits first
    }
    FreeTextureBufferIndex = idx;
}

// 0x7306A0
int32 D3DResourceSystem::CreateIndexBuffer(uint32 numIndices, uint32 format, void** ppIndexBuffer) {
    return plugin::CallAndReturn<int32, 0x7306A0, uint32, uint32, void**>(numIndices, format, ppIndexBuffer);
}

// 0x730510
int32 D3DResourceSystem::CreateTexture(int32 width, int32 height, uint32 format, void** ppTexture) {
    return plugin::CallAndReturn<int32, 0x730510, int32, int32, uint32, void**>(width, height, format, ppTexture);
}

// 0x730D30
void D3DResourceSystem::DestroyIndexBuffer(void* pIndexBuffer) {
    auto* const ib = (IDirect3DIndexBuffer9*)pIndexBuffer;

    if (!UseD3DResourceBuffering) {
        ib->Release();
        return;
    }

    D3DINDEXBUFFER_DESC desc;
    ib->GetDesc(&desc);

    // Buffers are bucketed by their size (in indices, 100 per bucket)
    const uint32 bucket = ((desc.Size >> 1) - 1) / 100;
    auto& buf = (int32)bucket < NUM_INDEX_DATA_BUFFERS ? IndexDataBuffers[bucket] : LargeIndexDataBuffer;

    buf.m_nNumDatasInBuffer++;
    if ((int32)buf.m_nSize >= (int32)buf.m_nCapcacity) {
        buf.Resize(buf.m_nCapcacity * 2);
        if ((int32)buf.m_nSize >= (int32)buf.m_nCapcacity) { // Still full => leaked, same in the original
            return;
        }
    }
    buf.m_apIndexData[buf.m_nSize] = ib;
    buf.m_nSize++;
}

// 0x730B70
void D3DResourceSystem::DestroyTexture(void* texture) {
    auto* const tex = (IDirect3DTexture9*)texture;

    if (!UseD3DResourceBuffering) {
        tex->Release();
        return;
    }

    const uint32 numLevels  = tex->GetLevelCount();
    const int32  bOneLevel  = numLevels > 1 ? 0 : 1;

    D3DSURFACE_DESC desc;
    tex->GetLevelDesc(0, &desc);

    // Look for an existing buffer for square textures of this format, size and level count
    if (desc.Width == desc.Height && NumTextureBuffers > 1) {
        float widthF = (float)(int32)desc.Width;
        if ((int32)desc.Width < 0) {
            widthF += 4294967296.0f; // 0x858C54
        }
        for (int32 i = 1; i < NumTextureBuffers; i++) {
            auto& buf = TextureBuffers[i];
            // NOTE: `FILD` of the buffer's width (extended precision) compared to the float
            if ((double)(int32)buf.m_nWidth == (double)widthF && desc.Format == buf.m_nFormat && bOneLevel == buf.m_nLevels) {
                buf.m_nNumTexturesInBuffer++;
                if ((int32)buf.m_nSize >= (int32)buf.m_nCapcacity) {
                    buf.Resize(buf.m_nCapcacity * 2);
                    buf.PushWithoutIncreasingCounter(tex); // BUG: If it's still full the texture is leaked (same in the original)
                    return;
                }
                buf.m_apTextures[buf.m_nSize] = tex;
                buf.m_nSize++;
                return;
            }
        }
    }

    if (NumTextureBuffers < MAX_TEXTURE_BUFFERS && desc.Width == desc.Height && desc.Width >= 32) {
        if (desc.Width > 256) { // Too big to buffer
            tex->Release();
            return;
        }

        // Make a new buffer for this kind of texture
        TextureBuffers[NumTextureBuffers].Setup(desc.Format, desc.Width, bOneLevel, 16);
        const auto idx = NumTextureBuffers;
        TextureBuffers[idx].Push(tex);
        NumTextureBuffers = idx + 1;
        return;
    }

    if (desc.Width <= 256 && desc.Height <= 256) {
        // Put it into the buffer for the small textures
        auto& buf = TextureBuffers[0];
        buf.m_nNumTexturesInBuffer++;
        if ((int32)buf.m_nSize >= (int32)buf.m_nCapcacity) {
            buf.Resize(buf.m_nCapcacity * 2);
            if ((int32)buf.m_nSize >= (int32)buf.m_nCapcacity) { // BUG: Leaked, same in the original
                return;
            }
        }
        buf.m_apTextures[buf.m_nSize] = tex;
        buf.m_nSize++;
        return;
    }

    tex->Release();
}

void D3DResourceSystem::InjectHooks() {
    RH_ScopedClass(D3DResourceSystem);
    RH_ScopedCategory("RenderWare");

    RH_ScopedInstall(CancelBuffering, 0x730900);
    RH_ScopedInstall(Init, 0x730830);
    RH_ScopedInstall(SetUseD3DResourceBuffering, 0x730AC0);
    RH_ScopedInstall(Shutdown, 0x730A00);
    RH_ScopedInstall(TidyUpD3DIndexBuffers, 0x730740);
    RH_ScopedInstall(TidyUpD3DTextures, 0x7305E0);
    RH_ScopedInstall(DestroyIndexBuffer, 0x730D30);
    RH_ScopedInstall(DestroyTexture, 0x730B70);

    {
        RH_ScopedClass(D3DTextureBuffer);
        RH_ScopedCategory("RenderWare");
        RH_ScopedInstall(Resize, 0x730020);
        RH_ScopedInstall(Setup, 0x72FE80);
        RH_ScopedInstall(Push, 0x72FFF0);
        RH_ScopedInstall(PushWithoutIncreasingCounter, 0x730AD0);
    }
    {
        RH_ScopedClass(D3DIndexDataBuffer);
        RH_ScopedCategory("RenderWare");
        RH_ScopedInstall(Resize, 0x730330);
    }
}
