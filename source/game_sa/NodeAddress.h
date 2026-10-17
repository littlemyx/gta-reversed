/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

class CNodeAddress {
public:
    uint16 m_wAreaId{ (uint16)-1 };
    uint16 m_wNodeId{ (uint16)-1 };

    constexpr CNodeAddress() = default;
    constexpr CNodeAddress(uint16 areaId, uint16 nodeId) : m_wAreaId(areaId), m_wNodeId(nodeId) {}

    bool operator==(CNodeAddress const&) const = default;
    bool operator!=(CNodeAddress const&) const = default;

    void ResetAreaId() { m_wAreaId = UINT16_MAX; }
    void ResetNodeId() { m_wNodeId = UINT16_MAX; }

    [[nodiscard]] bool IsAreaValid() const { return m_wAreaId != (uint16)-1; }
    [[nodiscard]] bool IsValid() const { return IsAreaValid() && m_wNodeId != UINT16_MAX; }

    operator bool() const { return IsValid(); }
};
VALIDATE_SIZE(CNodeAddress, 0x4);

// The exe fills its global CNodeAddress arrays with static ctors that store ONLY the area id (0xFFFF); the node id stays 0 (ctors 0x84A040 / 0x84A080 / 0x84A640).
// The NSDMI above is {-1, -1}: the detached globals use this to reproduce the exe image byte for byte.
template<size_t N>
constexpr std::array<CNodeAddress, N> MakeExeNodeAddressArray() {
    std::array<CNodeAddress, N> a{};
    for (auto& e : a) { e = CNodeAddress{ (uint16)-1, 0 }; }
    return a;
}
