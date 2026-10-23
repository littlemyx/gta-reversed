#pragma once
// P-2c: on-disk (x86 exe) layouts of CMatrix and CVehicleSaveStructure. Standalone POD header (no StdInc): usable from tests/portable/p2c_*.cpp.
//
// CMatrix is dumped raw by the exe: nested in CVehicleSaveStructure (Vehicle.cpp Save/Load) and as the `uint8 matrix[sizeof(CMatrix)]` of tReplayCameraBlock (replay ring buffer and
// replay.rep). The only pointer is `RwMatrix* m_pAttachMatrix` (@0x40): 4 bytes on the exe, 8 on an LP64 host, which makes CMatrix 0x50 there (0x48 on file).
// The record keeps the exe's 0x48 bytes with explicit padding so that every byte is defined (the exe wrote stack garbage into the pad slots).
#include <cstddef>
#include <cstdint>

struct CMatrix_File {
    float    right[3];   // 0x00
    uint32_t flags;      // 0x0C  RwMatrix::flags (CMatrix::flags)
    float    top[3];     // 0x10  (CMatrix::m_forward)
    uint32_t pad1;       // 0x1C
    float    at[3];      // 0x20  (CMatrix::m_up)
    uint32_t pad2;       // 0x2C
    float    pos[3];     // 0x30
    uint32_t pad3;       // 0x3C
    uint32_t attach;     // 0x40  RwMatrix* m_pAttachMatrix: always 0 (the saved temporary is never attached; the slot is meaningless in a file)
    uint8_t  owns;       // 0x44  m_bOwnsAttachedMatrix: always 0 on disk
    uint8_t  pad4[3];    // 0x45
};
static_assert(sizeof(CMatrix_File) == 0x48 && alignof(CMatrix_File) == 4, "Invalid structure size of CMatrix_File"); // == VALIDATE_SIZE_FILE (this header is StdInc-free)

// CVehicleSaveStructure (Vehicle.cpp Save: uint32 size, then the record; the loader ignores the size and reads the constant)
struct CVehicleSaveStructure_File {
    CMatrix_File matrix;        // 0x00
    uint8_t      field_48;      // 0x48  never initialised by Construct(): the exe wrote garbage, nothing reads it (0 here)
    uint8_t      primaryColor;  // 0x49
    uint8_t      secondaryColor;// 0x4A
    uint8_t      tertiaryColor; // 0x4B
    uint8_t      quaternaryColor;// 0x4C
    uint8_t      pad4D;         // 0x4D
    uint16_t     alarmState;    // 0x4E
    uint8_t      maxPassengers; // 0x50
    uint8_t      pad51[3];      // 0x51
    float        steerAngle;    // 0x54
    float        gasPedal;      // 0x58
    float        brakePedal;    // 0x5C
    uint32_t     upperFlags;    // 0x60
    uint32_t     lowerFlags;    // 0x64
};
static_assert(sizeof(CVehicleSaveStructure_File) == 0x68 && alignof(CVehicleSaveStructure_File) == 4, "Invalid structure size of CVehicleSaveStructure_File");
// x86 offsets of the exe (VALIDATE_OFFSET numbers / i686 layout dump; also tools/portable/gen_x86_golden.py)
static_assert(offsetof(CMatrix_File, top) == 0x10 && offsetof(CMatrix_File, at) == 0x20 && offsetof(CMatrix_File, pos) == 0x30 && offsetof(CMatrix_File, attach) == 0x40 && offsetof(CMatrix_File, owns) == 0x44);
static_assert(offsetof(CVehicleSaveStructure_File, field_48) == 0x48 && offsetof(CVehicleSaveStructure_File, alarmState) == 0x4E && offsetof(CVehicleSaveStructure_File, maxPassengers) == 0x50
           && offsetof(CVehicleSaveStructure_File, steerAngle) == 0x54 && offsetof(CVehicleSaveStructure_File, upperFlags) == 0x60 && offsetof(CVehicleSaveStructure_File, lowerFlags) == 0x64);
