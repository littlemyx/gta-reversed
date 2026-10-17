#pragma once
// Reads "before `CTaskSimpleFight::m_aComboData`" (PT-MISSION / U14).
//
// The exe indexes the combo table with `(m_nComboSet - 4) * sizeof(CMeleeInfo)` and NO clamp (0x624A40 `mov bl, [ecx + <m_aComboData + 0x84>]`, 0x61D6F0 `fld [... + <m_aComboData + 0x3C>]`),
// and `m_nComboSet` is -1 (ctor) / 0 / 1 for "no melee set picked" => the bytes it reads are whatever the linker put BELOW the array. A detached build gives `m_aComboData` its own storage,
// so those bytes have to be taken from the objects that sit there in the exe (run-time values, not the image's zeros), laid out by distance below the array:
//
//   exe address range      object                                   below (decimal, = array - address)
//   [ 0xC16DB8, +256 )     CGenericGameStorage::ms_SaveFileName          792
//   [ 0xC16EEC, +4   )     CGenericGameStorage::ms_WorkBufferPos         484
//   [ 0xC16F18, +260 )     C_PcSave::DefaultPCSaveFileName               440
//   [ 0xC17040, +60  )     RsGlobal (RsGlobalType)                       144
//   [ 0xC170CC, +4   )     a float computed by the exe's start-up code     4   (= 0x858630 / 0x86d500 in float; thunk 0x851F10 stores it; nothing writes it later)
//
// Between those objects there are other exe variables with no port counterpart (the 6-float tables at 0xC16F00 / 0xC1701C / 0xC17088, FadeTimer-adjacent padding ...): a read that
// touches them is NOT mapped (`Read` returns false) - for `m_nComboSet` in [-2, 16] (every value the game ever assigns, see TaskSimpleFight.cpp) no read of the two call forms (flags byte at +0x84,
// float at +0x3C) does. -2 reads ms_SaveFileName, -1 ms_SaveFileName, 0/1 DefaultPCSaveFileName / ms_WorkBufferPos, 2 RsGlobal.maximumWidth / DefaultPCSaveFileName, 3 the float / (the
// CPlayerRelationshipRecorder pointer at 0xC17084: heap pointer, not mapped).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace notsa::fight_prearray {

// The constant the exe's start-up thunk 0x851F10 stores into the last float below the array (float 0.0055555599, the low byte of it is what `m_nComboSet == 3` reads as flags)
inline constexpr uint32_t LastFloatBelowBits = 0x3BB60B6Au;

struct Segment {
    int32_t        below; // Distance (bytes) of the object's first byte below the start of `m_aComboData`
    const uint8_t* data;
    size_t         size;
};

// Reads `size` (<= 4) bytes, little endian, at byte offset `rel` relative to the start of `m_aComboData` (negative = below it; `comboSize` = the array's size).
// false when the bytes aren't completely inside the array or one mapped object
inline bool Read(std::span<const Segment> below, const uint8_t* combo, size_t comboSize, int32_t rel, size_t size, uint32_t& out) {
    out = 0;
    if (rel >= 0) {
        if ((size_t)rel + size > comboSize) {
            return false;
        }
        std::memcpy(&out, combo + rel, size);
        return true;
    }
    for (const auto& s : below) {
        const int32_t at = rel + s.below; // Offset inside this object
        if (at >= 0 && (size_t)at + size <= s.size) {
            std::memcpy(&out, s.data + at, size);
            return true;
        }
    }
    return false;
}

} // namespace notsa::fight_prearray
