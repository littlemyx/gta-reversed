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
