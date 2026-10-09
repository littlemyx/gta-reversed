#include "StdInc.h"

#include "D3DTextureBuffer.h"

// 0x730020
void D3DTextureBuffer::Resize(uint32 newCapacity) {
    if (newCapacity == m_nCapcacity) {
        return;
    }

    auto* const newArray = (decltype(m_apTextures))operator new(newCapacity * sizeof(void*));
    for (int32 i = 0; i < (int32)m_nSize && i < (int32)newCapacity; i++) {
        newArray[i] = m_apTextures[i];
    }
    // Release the ones that don't fit. NOTE: `m_nSize` isn't updated (same in the original)
    for (int32 i = newCapacity; i < (int32)m_nSize; i++) {
        ((IDirect3DTexture9*)m_apTextures[i])->Release();
    }
    operator delete(m_apTextures);
    m_apTextures = newArray;
    m_nCapcacity = newCapacity;
}

// 0x72FE80
void D3DTextureBuffer::Setup(uint32 format, int32 width, int32 bOneLevel, uint32 capacity) {
    m_nFormat    = format;
    m_nWidth     = width;
    m_nLevels    = bOneLevel;
    m_apTextures = (decltype(m_apTextures))operator new(capacity * sizeof(void*));
    m_nCapcacity = capacity;
    m_nSize      = 0;
    m_nNumTexturesInBuffer = 0;
}

// 0x72FFF0
bool D3DTextureBuffer::Push(IDirect3DTexture9* texture) {
    m_nNumTexturesInBuffer++;
    if ((int32)m_nSize >= (int32)m_nCapcacity) {
        return false;
    }
    m_apTextures[m_nSize++] = texture;
    return true;
}

// 0x730AD0
bool D3DTextureBuffer::PushWithoutIncreasingCounter(IDirect3DTexture9* texture) {
    // NOTE: The original stores `m_nNumTexturesInBuffer` into itself here, a no-op
    if ((int32)m_nSize >= (int32)m_nCapcacity) {
        return false;
    }
    m_apTextures[m_nSize++] = texture;
    return true;
}

// 0x72FF60
IDirect3DTexture9* D3DTextureBuffer::Pop(uint32 format, int32 width, int32 height, int32 levels) {
    // Searches from the back for a texture with the exact format/size/level count
    for (int32 i = (int32)m_nSize; i != 0;) {
        auto* const tex = m_apTextures[--i];
        const int32 isOneLevel = tex->GetLevelCount() == 1;
        if (isOneLevel != levels) {
            continue;
        }
        D3DSURFACE_DESC desc;
        tex->GetLevelDesc(0, &desc);
        if (desc.Format != format || desc.Width != (uint32)width || desc.Height != (uint32)height) {
            continue;
        }

        // Found => remove it by moving the last texture into its slot
        // NOTE: `m_nNumTexturesInBuffer` isn't decremented (same in the original)
        const int32 last = (int32)--m_nSize;
        if (last > 0 && last != i) {
            m_apTextures[i] = m_apTextures[last];
        }
        // BUG: The original doesn't null the vacated slot (harmless, `m_nSize` guards it)
        return tex;
    }
    return nullptr;
}

// 0x7300A0
uint32 D3DTextureBuffer::GetTotalDataSize() {
    // NOTE: Sizes are in pixels (width * height), not bytes. `_ftol` results are emulated with truncating double math (extended precision in the original)
    uint32 total = 0;
    if (m_nFormat == 0) {
        // Buffer for small textures of any format => sum of all
        for (int32 i = 0; i < (int32)m_nSize; i++) {
            D3DSURFACE_DESC desc;
            m_apTextures[i]->GetLevelDesc(0, &desc);
            if (m_apTextures[i]->GetLevelCount() == 0) { // Full chain => +1/3, but practically never true (GetLevelCount() returns >= 1)
                const uint32 pixels = desc.Width * desc.Height; // `FILD` + 2^32 fixup => unsigned
                total = (uint32)(int64)((double)pixels * (double)1.33333337f + (double)(int32)total);
            } else {
                total += desc.Width * desc.Height;
            }
        }
    } else if (m_nSize != 0) {
        // All textures in the buffer are of the same size => look at the first one
        D3DSURFACE_DESC desc;
        m_apTextures[0]->GetLevelDesc(0, &desc);
        const uint32 pixels = m_nSize * desc.Width * desc.Height;
        if (m_apTextures[0]->GetLevelCount() == 0) {
            return (uint32)(int64)((double)(int32)pixels * (double)1.33333337f);
        }
        total = pixels;
    }
    return total;
}
