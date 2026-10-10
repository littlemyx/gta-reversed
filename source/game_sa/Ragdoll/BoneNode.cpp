#include "StdInc.h"

#include "BoneNode.h"
#include "BoneNodeManager.h"

#include "rtslerp.h"

void BoneNode_c::InjectHooks() {
    RH_ScopedClass(BoneNode_c);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x616B30);
    RH_ScopedInstall(Destructor, 0x616B80);

    RH_ScopedInstall(Init, 0x6177B0);
    RH_ScopedInstall(InitLimits, 0x617490);
    RH_ScopedGlobalInstall(EulerToQuat, 0x6171F0);
    RH_ScopedGlobalInstall(QuatToEuler, 0x617080);
    RH_ScopedGlobalInstall(GetIdFromBoneTag, 0x617050);
    RH_ScopedInstall(ClampLimitsCurrent, 0x6175D0);
    RH_ScopedInstall(ClampLimitsDefault, 0x617530);
    RH_ScopedInstall(Limit, 0x617650);
    RH_ScopedInstall(BlendKeyframe, 0x616E30);
    RH_ScopedInstall(GetSpeed, 0x616CB0);
    RH_ScopedInstall(SetSpeed, 0x616CC0);
    RH_ScopedInstall(SetLimits, 0x616C50);
    RH_ScopedInstall(GetLimits, 0x616BF0);
    RH_ScopedInstall(AddChild, 0x616BD0);
    RH_ScopedInstall(CalcWldMat, 0x616CD0);
}

// 0x6177B0
bool BoneNode_c::Init(eBoneTag32 boneTag, RpHAnimBlendInterpFrame* interpFrame) {
    m_BoneTag     = boneTag;
    m_InterpFrame = interpFrame;
    m_Orientation = interpFrame->q;
    m_Pos         = interpFrame->t;
    m_Parent      = nullptr;

    m_Childs.RemoveAll();
    InitLimits();

    m_Speed = 1.0f;

    return true;
}

// 0x617490
void BoneNode_c::InitLimits() {
    const auto& bone = BoneNodeManager_c::ms_boneInfos[GetIdFromBoneTag(m_BoneTag)];
    m_LimitMin = bone.PoseRots - CVector{
        bone.MinX,
        bone.MinY,
        bone.MinZ
    };
    m_LimitMax = bone.PoseRots + CVector{
        bone.MaxX,
        bone.MaxY,
        bone.MaxZ
    };
}

// degrees → radians → half-angles → quaternion
// Tait-Bryan XYZ convention
// 0x6171F0
void BoneNode_c::EulerToQuat(const CVector& angles, RtQuat& outQuat) {
    // Exact x87 form of the exe (0x6171F0): half angles are `deg * PI * (1/180) * 0.5` (0x858CB8, 0x85F0AC, 0x858B8C), NOT DegreesToRadians(deg) / 2;
    // intermediates stay in extended precision (doubles) except where the exe spills to floats. The normalisation multiplies by `1 / (|q|^2)`
    // (no square root, an oddity of the original).
    constexpr double kPi      = (double)std::numbers::pi_v<float>;                // 0x858CB8
    constexpr double kInv180  = (double)std::bit_cast<float>(0x3BB60B61u);        // 0x85F0AC
    const auto       HalfRad  = [&](float deg) { return (double)deg * kPi * kInv180 * (double)0.5f; };

    const double ex = HalfRad(angles.x);
    const double ey = HalfRad(angles.y);
    const float  ez = (float)HalfRad(angles.z); // spilled
    const float  cx = (float)x87::cos(ex);
    const float  cy = (float)x87::cos(ey);
    const float  cz = (float)x87::cos((double)ez);
    const float  sx = (float)x87::sin(ex);
    const double sy = x87::sin(ey);
    const double sz = x87::sin((double)ez);

    const float  cc = (float)((double)cz * (double)cx);
    const double cs = (double)cx * sz;
    const double sc = (double)sx * (double)cz;
    const float  ss = (float)(sz * (double)sx);

    const float  A = (float)((double)cy * sc - cs * sy);
    const float  B = (float)((double)cc * sy + (double)ss * (double)cy);
    const double C = cs * (double)cy - sc * sy;
    const double Dx = (double)ss * sy + (double)cc * (double)cy;
    const float  D = (float)Dx;

    const double sumSq = ((Dx * (double)D + C * C) + (double)B * (double)B) + (double)A * (double)A;
    const float  r     = (float)(1.0 / sumSq);

    outQuat.imag.x = (float)((double)A * (double)r);
    outQuat.imag.y = (float)((double)B * (double)r);
    outQuat.imag.z = (float)((double)r * C);
    outQuat.real   = (float)((double)r * (double)D);
}

// atan2(y, x) * 180 * (1/pi) on the x87 stack: `fpatan` and both multiplications round at the CURRENT precision control (PC=24 in game); a CRT atan2 returning a double
// followed by separate multiplications double-rounds differently
#if defined(_MSC_VER) && !defined(__clang__) && defined(_M_IX86)
static float AtanDeg(double y, double x) {
    const float c180 = 180.0f;                              // 0x85A994
    const float cInvPi = std::bit_cast<float>(0x3EA2F983u); // 0x86D2B4
    float r;
    __asm {
        fld qword ptr [y]
        fld qword ptr [x]
        fpatan
        fmul dword ptr [c180]
        fmul dword ptr [cInvPi]
        fstp dword ptr [r]
    }
    return r;
}
#else
static float AtanDeg(double y, double x) { return (float)(x87::atan2(y, x) * 180.0 * (double)std::bit_cast<float>(0x3EA2F983u)); }
#endif

// 0x617080
void BoneNode_c::QuatToEuler(const RtQuat& quat, CVector& outAngles) {
    // Exact x87 form of the exe (0x617080): intermediates stay in extended precision (doubles here), only A..D, V and the results are spilled to floats.
    // Degrees = atan2(..) * 180 * (1/PI as float 0x86D2B4) (two separate multiplications), NOT common.h's RadiansToDegrees.
    // atan2(y, x) * 180 * (1/pi) on the x87 stack: `fpatan` and both multiplications round at the CURRENT precision control (PC=24 in game); a CRT atan2 returning a double
    // followed by separate multiplications double-rounds differently

    const double x = quat.imag.x;
    const double y = quat.imag.y;
    const double z = quat.imag.z;
    const double w = quat.real;

    const float  A = (float)((1.0 - 2.0 * (y * y)) - 2.0 * (z * z));
    const float  B = (float)(2.0 * (z * w) + 2.0 * (x * y));
    const float  C = (float)(2.0 * (z * y) + 2.0 * (w * x));
    const float  D = (float)((1.0 - 2.0 * (x * x)) - 2.0 * (y * y));
    const double s = -(2.0 * (z * x) - 2.0 * (w * y));
    const float  V = (float)std::sqrt(1.0 - s * s);

    outAngles.y = AtanDeg(s, (double)V);
    if (s == 1.0 || s == -1.0) { // Gimbal lock case: Yaw and Roll collapse into a single degree of freedom
        const double E = (1.0 - 2.0 * (x * x)) - 2.0 * (z * z);
        const double G = -(2.0 * (z * y) - 2.0 * (w * x));
        outAngles.x = AtanDeg(G, E);
        outAngles.z = AtanDeg(0.0, 1.0);
    } else { // General case
        const double r = 1.0 / (double)V;
        outAngles.x = AtanDeg((double)C * r, (double)D * r);
        outAngles.z = AtanDeg((double)B * r, (double)A * r);
    }
}

// inline
// 0x617050
inline int32 BoneNode_c::GetIdFromBoneTag(eBoneTag32 tag) {
    for (auto&& [i, info] : rngv::enumerate(BoneNodeManager_c::ms_boneInfos)) {
        if (info.BoneTag == tag) {
            return (eBoneTag)(i);
        }
    }
    return BONE_UNKNOWN;
}

// Empty in Android
// 0x6175D0
void BoneNode_c::ClampLimitsCurrent(bool limitX, bool limitY, bool limitZ) {
    static auto& s_TestSkipClampCurrent = StaticRef<bool>(0x8D2BD1); // true

    if (!s_TestSkipClampCurrent) {
        CVector current;
        QuatToEuler(m_Orientation, current);
        if (limitX) {
            m_LimitMin.x = m_LimitMax.x = current.x;
        }
        if (limitY) {
            m_LimitMin.y = m_LimitMax.y = current.y;
        }
        if (limitZ) {
            m_LimitMin.z = m_LimitMax.z = current.z;
        }
    }
}

// Empty in Android
// 0x617530
void BoneNode_c::ClampLimitsDefault(bool limitX, bool limitY, bool limitZ) {
    static auto& s_TestSkipClampDefault = StaticRef<bool>(0x8D2BD0); // true

    if (!s_TestSkipClampDefault) {
        if (const auto* info = GetBoneInfo()) { // BUGFIX: Check if info is available
            if (limitX) {
                m_LimitMin.x = m_LimitMax.x = info->PoseRots.x;
            }
            if (limitY) {
                m_LimitMin.y = m_LimitMax.y = info->PoseRots.y;
            }
            if (limitZ) {
                m_LimitMin.z = m_LimitMax.z = info->PoseRots.z;
            }
        }
    }
}

// argument (float blend) - ignored
// 0x617650
void BoneNode_c::Limit(float blend) {
    CVector angles{};
    QuatToEuler(m_Orientation, angles);

    angles.x = std::clamp(angles.x, m_LimitMin.x, m_LimitMax.x);
    angles.y = std::clamp(angles.y, m_LimitMin.y, m_LimitMax.y);

    float minZ = m_LimitMin.z;
    float maxZ = m_LimitMax.z;

    if (m_BoneTag == eBoneTag::BONE_HEAD) {
        if (const auto* info = GetBoneInfo()) { // BUGFIX: Check if info is available
            float pose = info->PoseRots.z;

            // Scale the Z range toward boneOffset as angleX approaches 45°
            const float factor = std::max(std::abs(angles.x) / -45.0f + 1.0f, 0.0f);
            minZ = (minZ - pose) * factor + pose;
            maxZ = (maxZ - pose) * factor + pose;
        }
    }

    angles.z = std::clamp(angles.z, minZ, maxZ);

    EulerToQuat(angles, m_Orientation);
}

// 0x616E30
void BoneNode_c::BlendKeyframe(float blend) {
    auto src = m_InterpFrame->q;
    auto dst = m_Orientation;
    RtQuatSlerpCache cache;
    RtQuatSetupSlerpCache(&src, &dst, &cache);
    RtQuatSlerp(&m_InterpFrame->q, &src, &dst, blend, &cache);
}

// notsa
const BoneInfo_t* BoneNode_c::GetBoneInfo() {
    return BoneNodeManager_c::GetBoneInfoFromTag(m_BoneTag);
}

// 0x616CB0
float BoneNode_c::GetSpeed() const {
    return m_Speed;
}

// 0x616CC0
void BoneNode_c::SetSpeed(float speed) {
    m_Speed = speed;
}

// 0x616C50
void BoneNode_c::SetLimits(eRotationAxis axis, float min, float max) {
    m_LimitMin[+axis] = min;
    m_LimitMax[+axis] = max;
}

// 0x616BF0
void BoneNode_c::GetLimits(eRotationAxis axis, float& outMin, float& outMax) const {
    outMin = m_LimitMin[+axis];
    outMax = m_LimitMax[+axis];
}

// 0x616BD0
void BoneNode_c::AddChild(BoneNode_c* children) {
    children->m_Parent = this;
    m_Childs.AddItem(children);
}

// 0x616CD0
void BoneNode_c::CalcWldMat(const RwMatrix* boneMatrix) {
    RwMatrix rotMatrix = [this] {
        CMatrix mat{};
        mat.SetRotate(CQuaternion{ m_Orientation });
        mat.GetPosition() = m_Pos;
        return mat.ToRwMatrix();
    }();

    rwMatrixSetFlags(&rotMatrix, rwMATRIXTYPEORTHONORMAL);
    RwMatrixMultiply(&m_WorldMat, &rotMatrix, boneMatrix);

    for (auto bone = m_Childs.GetHead(); bone; bone = m_Childs.GetNext(bone)) {
        bone->CalcWldMat(&m_WorldMat);
    }
}
