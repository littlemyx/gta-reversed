#pragma once
// P-2h: on-disk (x86 exe) layout of CPathNode = one 0x1C record of data/paths/NODES%d.DAT. Standalone POD header (no StdInc): usable from tests/portable/p2h_*.cpp.
//
// CPathFind::LoadPathFindData reads `numNodes * sizeof(CPathNode)` bytes of the file straight into the in-memory array. On the exe CPathNode is 0x1C: the hash-chain links
// `CPathNode *m_next, *m_prev` take the first 8 bytes (in the shipped files m_prev is always 0 and m_next is a stale heap pointer of the tool that wrote them in 98% of the nodes - 66,601 of 68,237;
// never dereferenced: AddNodeToList always assigns both links before DoPathSearch's hash buckets are walked), then the 20-byte tail. On an LP64 host the
// two pointers are 16 bytes (CPathNode is 40 B, tail at +16), so the file stride must come from this record, not from sizeof(CPathNode).
#include <cstddef>
#include <cstdint>

struct CPathNode_File {
    uint32_t links[2];     // 0x00 m_next, m_prev : stale pointer / 0 in the data files (the exe stored them raw; runtime-only: AddNodeToList/RemoveNodeFromList set them before use). Written 0 by ToFile, dropped by FromFile
    int16_t  pos[3];       // 0x08 m_vPos (CompressedLargeVector: int16 x, y, z; /8 = world units)
    int16_t  totalDistFromOrigin; // 0x0E m_totalDistFromOrigin (SHRT_MAX - 1 = 0x7FFE in the data files)
    int16_t  baseLinkId;   // 0x10 m_wBaseLinkId : index of the first link in the area's m_pNodeLinks
    uint16_t areaId;       // 0x12 m_wAreaId
    uint16_t nodeId;       // 0x14 m_wNodeId
    uint8_t  pathWidth;    // 0x16 m_nPathWidth (fixed point /16)
    uint8_t  floodFill;    // 0x17 m_nFloodFill
    uint32_t flags;        // 0x18 the bit-field word, bit positions below (LSB first). Byte 3 (bits 24..31) has no member: always 0 in the data files, never read
};
static_assert(sizeof(CPathNode_File) == 0x1C && alignof(CPathNode_File) == 4, "Invalid structure size of CPathNode_File"); // == VALIDATE_SIZE_FILE (this header is StdInc-free)
static_assert(offsetof(CPathNode_File, pos) == 0x08 && offsetof(CPathNode_File, totalDistFromOrigin) == 0x0E && offsetof(CPathNode_File, baseLinkId) == 0x10
           && offsetof(CPathNode_File, areaId) == 0x12 && offsetof(CPathNode_File, nodeId) == 0x14 && offsetof(CPathNode_File, pathWidth) == 0x16
           && offsetof(CPathNode_File, floodFill) == 0x17 && offsetof(CPathNode_File, flags) == 0x18);

// Bit positions in CPathNode_File::flags (identical to the exe's uint32 bit-fields of CPathNode: MSVC and Itanium both allocate LSB first inside one uint32 unit; host-tested)
inline constexpr unsigned kCPathNodeNumLinksShift = 0,            kCPathNodeNumLinksMask = 0xF;
inline constexpr unsigned kCPathNodeOnDeadEndBit = 4,             kCPathNodeIsSwitchedOffBit = 5, kCPathNodeRoadBlocksBit = 6, kCPathNodeWaterNodeBit = 7;
inline constexpr unsigned kCPathNodeSwitchedOffOriginalBit = 8,   kCPathNodeUnk1Bit = 9,  kCPathNodeDontWanderBit = 10, kCPathNodeUnk2Bit = 11;
inline constexpr unsigned kCPathNodeNotHighwayBit = 12,           kCPathNodeHighwayBit = 13, kCPathNodeUnk3Bit = 14, kCPathNodeUnk4Bit = 15;
inline constexpr unsigned kCPathNodeSpawnProbabilityShift = 16,   kCPathNodeSpawnProbabilityMask = 0xF;
inline constexpr unsigned kCPathNodeBehaviourTypeShift = 20,      kCPathNodeBehaviourTypeMask = 0xF;
inline constexpr uint32_t kCPathNodeFlagsUsedMask = 0x00FFFFFFu; // bits 24..31 are padding
