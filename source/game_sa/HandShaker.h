#pragma once

static constexpr auto NUM_HANDSHAKERS = 6;
class CHandShaker {
public:
  CVector m_ang;
  CVector m_lim;              //! Angle (`m_ang`) limit [That is, `m_ang` is clamped to [-m_lim, m_lim])
  CVector m_motion;           
  CVector m_vel;              ///! Motion velocity
  CVector m_slow;             ///! Slow motion velocity
  CMatrix m_resultMat;        ///!
  float   m_scaleReactionMin;
  float   m_scaleReactionMax;
  int32   m_twitchFreq;
  float   m_twitchVel;

public:
    static void InjectHooks();

    CHandShaker() { SetDefaults(); } // 0x517740
#ifdef NOTSA_GLOBALS_DETACHED
    explicit CHandShaker(const CVector& vel) : CHandShaker() { m_vel = vel; } // detached boot value of `gHandShaker` (below); the default ctor above still draws (and so consumes) its 3 `rand()` values
#endif
    ~CHandShaker() = default;        // 0x509810

    void SetDefaults();
    void Reset();
    void Process(float degree);
};

VALIDATE_SIZE(CHandShaker, 0x94);

#ifdef NOTSA_GLOBALS_DETACHED
// The exe's static ctor (0x84D6D0 -> CHandShaker 0x517740, 18 `rand()` calls with the CRT in its seed-1 state, filling `m_vel` z, y, x) leaves these `m_vel` values in the data image.
// A detached run would draw different ones (own evaluation order, own init order), so the exe's values are committed (bit patterns, 6 x 3 floats).
inline std::array<CHandShaker, NUM_HANDSHAKERS> MakeExeHandShakers() {
    constexpr uint32 kVel[NUM_HANDSHAKERS][3] = {
        { 0x382227C5u, 0x38EC6283u, 0x34065A57u }, { 0x38C945F9u, 0x38F55EE6u, 0x38A99AECu }, { 0x392C8FE3u, 0x393BE59Du, 0x3812EC42u },
        { 0x3934222Du, 0x38120D6Eu, 0x389C930Au }, { 0x387F0266u, 0x38D76465u, 0x389500C1u }, { 0x3898DCBDu, 0x37995943u, 0x35C91EA7u },
    };
    const auto v = [&](size_t i) { return CVector{ std::bit_cast<float>(kVel[i][0]), std::bit_cast<float>(kVel[i][1]), std::bit_cast<float>(kVel[i][2]) }; };
    // elements are built in place (prvalues): a copy would run the CMatrix copy ctor on `m_resultMat`, the exe image holds it all-zero
    return [&]<size_t... I>(std::index_sequence<I...>) {
        return std::array<CHandShaker, NUM_HANDSHAKERS>{ CHandShaker{ v(I) }... };
    }(std::make_index_sequence<NUM_HANDSHAKERS>{});
}
#endif
NOTSA_GLOBAL_HDR(gHandShaker, 0xB6ECA0, (std::array<CHandShaker, NUM_HANDSHAKERS>), = MakeExeHandShakers()); static_assert(sizeof(gHandShaker) == 888);
