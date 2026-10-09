#include "StdInc.h"

#include "D3DIndexDataBuffer.h"

// 0x730330
void D3DIndexDataBuffer::Resize(uint32 newCapacity) {
    if (newCapacity == m_nCapcacity) {
        return;
    }

    auto* const newArray = (decltype(m_apIndexData))operator new(newCapacity * sizeof(void*));
    for (int32 i = 0; i < (int32)m_nSize && i < (int32)newCapacity; i++) {
        newArray[i] = m_apIndexData[i];
    }
    // Release the ones that don't fit. NOTE: `m_nSize` isn't updated (same in the original)
    for (int32 i = newCapacity; i < (int32)m_nSize; i++) {
        ((IDirect3DIndexBuffer9*)m_apIndexData[i])->Release();
    }
    operator delete(m_apIndexData);
    m_apIndexData = newArray;
    m_nCapcacity  = newCapacity;
}

// 0x730270
IDirect3DIndexBuffer9* D3DIndexDataBuffer::Pop(uint32 indexCount) {
    // Best fit search (smallest buffer that's big enough), from the back
    const uint32 neededSize = indexCount * 2;
    uint32 bestSize = 0x7FFFFFFF;
    int32  bestIdx  = -1;
    for (int32 i = (int32)m_nSize; i != 0;) {
        auto* const ib = m_apIndexData[--i];
        D3DINDEXBUFFER_DESC desc;
        ib->GetDesc(&desc);
        if (desc.Size < bestSize && desc.Size >= neededSize) {
            bestSize = desc.Size;
            bestIdx  = i;
        }
    }
    if (bestIdx == -1) {
        return nullptr;
    }

    auto* const ib = m_apIndexData[bestIdx];
    const int32 last = (int32)--m_nSize;
    if (last > 0 && last != bestIdx) {
        m_apIndexData[bestIdx] = m_apIndexData[last];
    }
    // NOTE: `m_nNumDatasInBuffer` isn't decremented (same in the original)
    // BUG: The original doesn't null the vacated slot (harmless, `m_nSize` guards it)
    return ib;
}

// 0x7303B0
uint32 D3DIndexDataBuffer::GetTotalDataSize() {
    uint32 total = 0;
    if (m_nFormat == 0) {
        // Buffer for buffers of any size => sum of all
        for (int32 i = 0; i < (int32)m_nSize; i++) {
            D3DINDEXBUFFER_DESC desc;
            m_apIndexData[i]->GetDesc(&desc);
            total += desc.Size;
        }
    } else if (m_nSize != 0) {
        // All buffers are of the same size => look at the first one
        D3DINDEXBUFFER_DESC desc;
        m_apIndexData[0]->GetDesc(&desc);
        total = m_nSize * desc.Size;
    }
    return total;
}
