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
