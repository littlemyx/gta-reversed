#pragma once

#include "Base.h"

#include "List_c.h"
#include "ListItem_c.h"
#include "BoneNode.h"
#include "Ragdoll/BoneInfo.h"

class BoneNodeManager_c {
private:
    std::array<BoneNode_c, 128> m_Space;
    TList_c<BoneNode_c>         m_Bones;

    static inline NOTSA_GLOBAL(ms_boneInfos, 0x8D26D0, (std::array<BoneInfo_t, MAX_BONE_NUM>), { {
    { BONE_ROOT, BONE_UNKNOWN, CVector{ -0.2f, 0.4f, 87.3f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_PELVIS, BONE_ROOT, CVector{ -180.0f, -88.5f, 90.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_SPINE, BONE_PELVIS, CVector{ 2.1f, 0.0f, -0.8f }, 30.0f, 30.0f, 20.0f, 20.0f, 5.0f, 60.0f },
    { BONE_SPINE1, BONE_SPINE, CVector{ 2.6f, 0.4f, 6.1f }, 10.0f, 10.0f, 10.0f, 10.0f, 15.0f, 10.0f },
    { BONE_NECK, BONE_SPINE1, CVector{ 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_HEAD, BONE_NECK, CVector{ -1.8f, -0.5f, -7.3f }, 65.0f, 65.0f, 15.0f, 15.0f, 45.0f, 45.0f },
    { BONE_L_BROW, BONE_HEAD, CVector{ -1.4f, -9.0f, 81.5f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_R_BROW, BONE_HEAD, CVector{ 3.2f, 6.7f, 81.7f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_JAW, BONE_HEAD, CVector{ 0.0f, 0.0f, 111.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_R_CLAVICLE, BONE_NECK, CVector{ 22.0f, 80.6f, -166.7f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_R_UPPER_ARM, BONE_R_CLAVICLE, CVector{ -5.4f, -45.0f, -13.0f }, 0.0f, 0.0f, 0.0f, 115.0f, 90.0f, 45.0f },
    { BONE_R_FORE_ARM, BONE_R_UPPER_ARM, CVector{ 0.0f, 0.0f, -13.6f }, 0.0f, 0.0f, 0.0f, 0.0f, 135.0f, 13.0f },
    { BONE_R_HAND, BONE_R_FORE_ARM, CVector{ 113.4f, -4.0f, 6.5f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_R_FINGER, BONE_R_HAND, CVector{ 0.0f, 0.0f, 19.5f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_R_FINGER_01, BONE_R_FINGER, CVector{ 0.0f, 0.0f, 13.3f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_L_CLAVICLE, BONE_NECK, CVector{ -22.0f, -80.6f, -166.7f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_L_UPPER_ARM, BONE_L_CLAVICLE, CVector{ 5.4f, 45.6f, -13.0f }, 0.0f, 0.0f, 115.0f, 0.0f, 90.0f, 45.0f },
    { BONE_L_FORE_ARM, BONE_L_UPPER_ARM, CVector{ 0.0f, 0.0f, -13.6f }, 0.0f, 0.0f, 0.0f, 0.0f, 135.0f, 13.0f },
    { BONE_L_HAND, BONE_L_FORE_ARM, CVector{ -113.4f, 4.0f, 6.5f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_L_FINGER, BONE_L_HAND, CVector{ 0.0f, 0.0f, 19.5f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_L_FINGER_01, BONE_L_FINGER, CVector{ 0.0f, 0.0f, 13.3f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_L_THIGH, BONE_PELVIS, CVector{ 176.0f, -7.7f, 165.9f }, 0.0f, 0.0f, 20.0f, 5.0f, 30.0f, 0.0f },
    { BONE_L_CALF, BONE_L_THIGH, CVector{ 0.0f, 0.0f, -24.4f }, 0.0f, 0.0f, 0.0f, 0.0f, 110.0f, 0.0f },
    { BONE_L_FOOT, BONE_L_CALF, CVector{ -1.4f, -5.0f, 10.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_L_TOE_0, BONE_L_FOOT, CVector{ 0.0f, 0.0f, 90.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_R_THIGH, BONE_PELVIS, CVector{ -176.0f, 7.7f, 165.9f }, 0.0f, 0.0f, 5.0f, 20.0f, 30.0f, 0.0f },
    { BONE_R_CALF, BONE_R_THIGH, CVector{ 0.0f, 0.0f, -24.4f }, 0.0f, 0.0f, 0.0f, 0.0f, 110.0f, 0.0f },
    { BONE_R_FOOT, BONE_R_CALF, CVector{ 1.4f, 5.0f, 10.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_R_TOE_0, BONE_R_FOOT, CVector{ 0.0f, 0.0f, 90.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_L_BREAST, BONE_SPINE1, CVector{ 15.1f, -1.9f, 81.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_R_BREAST, BONE_SPINE1, CVector{ -6.0f, 2.0f, 84.3f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { BONE_BELLY, BONE_SPINE, CVector{ 0.0f, 0.0f, 88.1f }, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }
} });
#line 16

public:
    BoneNodeManager_c() = default;  // 0x617330
    ~BoneNodeManager_c() = default; // 0x617390

    bool Init();
    void Exit();
    void Reset();

    BoneNode_c* GetBoneNode();
    void ReturnBoneNode(BoneNode_c* boneNode);
    uint32 GetNumBoneNodesLeft() const;

    // notsa
    static const BoneInfo_t* GetBoneInfoFromTag(eBoneTag32 tag);

    friend class IKChain_c;
    friend class BoneNode_c;

private: // notsa section
    friend void InjectHooksMain();
    static void InjectHooks();

    BoneNodeManager_c* Constructor() {
        this->BoneNodeManager_c::BoneNodeManager_c();
        return this;
    }

    BoneNodeManager_c* Destructor() {
        this->BoneNodeManager_c::~BoneNodeManager_c();
        return this;
    }
};
VALIDATE_SIZE(BoneNodeManager_c, 0x4C0C);

static inline NOTSA_GLOBAL(g_boneNodeMan, 0xC10820, (BoneNodeManager_c), {});
