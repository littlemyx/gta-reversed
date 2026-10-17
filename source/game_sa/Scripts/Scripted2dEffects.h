#pragma once

struct C2dEffect;

class CScriptedEffectPair {
public:
    enum class eMode : int32 {
        NONE = -1,
        USE_PARTNER_ONCE = 0x0,
        LOOK_FOR_ANOTHER_PARTNER = 0x1,
    };

public:
    struct {
        int32 Effect{ -1 };                  //!< Index of the effect (See `CScripted2dEffects::GetEffect`)
        int32 WaitingTask{ -1 };             //!< Task sequence index (Used in `CTaskComplexAttractorPartnerWait`)
        int32 PartnerTask{ -1 };             //!< Task sequence index (Used in `CTaskComplexUseAttractorPartner`)
        eMode PartnerUseMode{ eMode::NONE }; //!< Use mode
    } Effects[2];
    bool UsePartnerImmediately{ false };
};

class CScriptedEffectPairs {
public:
    enum {
        MAX_NUM_EFFECT_PAIRS = 0x4,
    };

    int32 NumPairs{};
    CScriptedEffectPair Pairs[MAX_NUM_EFFECT_PAIRS]{};
};
VALIDATE_SIZE(CScriptedEffectPairs, 148);

struct tUserList_Child {
    int32 m_Id;      // eModelID
    int32 m_PedType; // ePedType
};
VALIDATE_SIZE(tUserList_Child, 0x8);

struct tUserList {
    std::array<int32, 4> m_UserTypes{ -1, -1, -1, -1 };
    std::array<int32, 4> m_UserTypesByPedType{ -1, -1, -1, -1 };
    bool  m_bUseList{};

    bool IsPedTypeInList(int32 pedType) const; // 0x5FE960 - true if the list is unused, or if it contains `pedType` (an entry with `m_UserTypes[i] == -2` matches by `m_UserTypesByPedType[i]`)
};
VALIDATE_SIZE(tUserList, 0x24);

constexpr auto NUM_SCRIPTED_2D_EFFECTS = 64;

class CScripted2dEffects {
public:
    static inline NOTSA_GLOBAL(ms_effects, 0xC3AB00, (std::array<C2dEffect, NUM_SCRIPTED_2D_EFFECTS>), {}); // actual type is `C2dEffectPedAttractor`
    static inline NOTSA_GLOBAL(ms_effectPairs, 0xC3BB00, (std::array<CScriptedEffectPairs, NUM_SCRIPTED_2D_EFFECTS>), = [] { std::array<CScriptedEffectPairs, NUM_SCRIPTED_2D_EFFECTS> a{}; for (auto& ps : a) { for (auto& p : ps.Pairs) { p.Effects[1].PartnerTask = 0; } } return a; }()); static_assert(sizeof(ms_effectPairs) == 9472); // static ctor 0x6FA790 / 0x6FA770 stores -1 everywhere EXCEPT Effects[1].PartnerTask (the same slip as `ClearEffectPairs`, see Scripted2dEffects.cpp): the exe image holds 0 there
    static inline NOTSA_GLOBAL(ms_userLists, 0xC3A200, (std::array<tUserList, NUM_SCRIPTED_2D_EFFECTS>), {}); static_assert(sizeof(ms_userLists) == 2304); // class maybe
    static inline NOTSA_GLOBAL(ms_activated, 0xC3A1A0, (std::array<bool, NUM_SCRIPTED_2D_EFFECTS>), {});
    static inline NOTSA_GLOBAL(ScriptReferenceIndex, 0xC3A120, (std::array<uint16, NUM_SCRIPTED_2D_EFFECTS>), {});
    static inline NOTSA_GLOBAL(ms_effectSequenceTaskIDs, 0xC3A020, (std::array<int32, NUM_SCRIPTED_2D_EFFECTS>), {});
    static inline NOTSA_GLOBAL(ms_useAgainFlags, 0xC39FE0, (std::array<bool, NUM_SCRIPTED_2D_EFFECTS>), {});
    static inline NOTSA_GLOBAL(ms_radii, 0xC39EE0, (std::array<float, NUM_SCRIPTED_2D_EFFECTS>), {});

public:
    static void InjectHooks();

    static void Init();

    static CScriptedEffectPairs* GetEffectPairs(const C2dEffectPedAttractor* effect);
    static int32 GetIndex(const C2dEffectPedAttractor* effect);

    static int32 AddScripted2DEffect(float radius);
    static void ReturnScripted2DEffect(int32 index);

    /// Index of the effect if it's from `ms_effects` `nullopt` otherwise.
    static auto IndexOfEffect(const C2dEffectPedAttractor* effect) ->std::optional<size_t>;

    static auto GetEffect(int32 index) -> C2dEffectPedAttractor*;

    static void Load() { } // NOP
    static void Save() { } // NOP

    // NOTSA
    static int32 FindFreeSlot();
};
