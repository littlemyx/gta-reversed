/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
// by Github.com / jte
#pragma once

#include "Quaternion.h"
#include "AnimBlendSequence.h"
#include "AnimBlendAssociation.h"

#include <bit>


// Helpers that reproduce the x87 evaluation (extended precision intermediates / float spills) of the exe
namespace AnimBlendNodeX87 {
// Blend amount: `partial ? amount : amount * weight`, product kept unrounded (extended)
inline double BlendAmountExt(const CAnimBlendAssociation* assoc, float weight) {
    return assoc->IsPartial() ? (double)assoc->m_BlendAmount : (double)assoc->m_BlendAmount * (double)weight;
}

inline int16 CompressedDeltaTimeRaw(const KeyFrameCompressed* kf) {
    return std::bit_cast<int16>(kf->DeltaTime); // FixedFloat<int16> is a single int16
}

// `fild; fmul [0x859044]` - NOT a division by 60 (the constant is the float 1/60 = 0x3C888889)
inline double CompressedDeltaTime(const KeyFrameCompressed* kf) {
    return (double)CompressedDeltaTimeRaw(kf) * (double)std::bit_cast<float>(0x3C888889u);
}

// from + (to - from) * t, with the exe's spills:
// x: (to.x - from.x) in extended, `* t` rounded to float, + from.x
// y: (to.y - from.y) in extended, `* t` rounded to float only if `RoundY` (0x4CFC50), + from.y
// z: (to.z - from.z) rounded to float, `* t` in extended, + from.z
template<bool RoundY>
inline CVector LerpTranslation(const CVector& from, const CVector& to, double t) {
    const double dx = (double)to.x - (double)from.x;
    const double dy = (double)to.y - (double)from.y;
    const float  dz = (float)((double)to.z - (double)from.z);

    const float  dxt = (float)(dx * t);
    const double dyt = RoundY ? (double)(float)(dy * t) : dy * t;
    const double dzt = t * (double)dz;

    return {
        (float)((double)dxt + (double)from.x),
        (float)(dyt + (double)from.y),
        (float)(dzt + (double)from.z),
    };
}
} // namespace AnimBlendNodeX87

/*!
 * @brief Represents the animation of a single node (bone)
 *
 * @details An animation can move one or more nodes (bones).
 * @details Each node has a sequence (that contain key-frames)
 */
class CAnimBlendNode {
public:
    static void InjectHooks();

    void Init();

    //! NOTSA: wraps the cdecl helper at 0x4D00E0 (theta = acos(min(dot(a, b), 1)), invSinTheta)
    void CalcTheta(const CQuaternion& a, const CQuaternion& b);

    //! @addr 0x4D0190
    void CalcDeltas();

    //! @addr 0x4D0350
    void CalcDeltasCompressed();


    //! @addr 0x4CFC50
    void GetCurrentTranslation(CVector& trans, float weight) { return I_GetCurrentTranslation<false>(trans, weight); }

    //! @addr 0x4CFE60
    void GetCurrentTranslationCompressed(CVector& trans, float weight) { return I_GetCurrentTranslation<true>(trans, weight); }


    //! @addr 0x4CFD90 
    void GetEndTranslation(CVector& trans, float weight) { return I_GetEndTranslation<false>(trans, weight); }

    //! @addr 0x4D0000
    void GetEndTranslationCompressed(CVector& trans, float weight) { return I_GetEndTranslation<true>(trans, weight); }


    //! @addr 0x4D04A0
    bool NextKeyFrame() { return I_NextKeyFrame<false>(); }

    //! @addr 0x4D0570
    bool NextKeyFrameCompressed() { return I_NextKeyFrame<true>(); } 

    //! @addr 0x4CFB90
    bool NextKeyFrameNoCalc() { return I_NextKeyFrame<false, false>(); }


    //! @addr 0x4D06C0
    bool Update(CVector& trans, CQuaternion& rot, float weight) { return I_Update<false>(trans, rot, weight); }

    //! @addr 0x4D08D0
    bool UpdateCompressed(CVector& trans, CQuaternion& rot, float weight) { return I_Update<true>(trans, rot, weight); }

    bool UpdateTime(); // Unused
    bool SetupKeyFrameCompressed();
    bool FindKeyFrame(float time);
    auto GetAnimAssoc() const { return m_BlendAssoc; }
    bool IsValid() const { return !!m_Seq; }
    auto GetSeq() const { return m_Seq; }
    auto GetRootKF() const { return &m_Seq[0]; }

    //! @notsa
    //! @brief Inverse progress of current frame (Eg.: How much is left of this frame)
    //! @details The exe keeps this on the x87 stack (extended precision), callers that spill it round to float themselves.
    template<bool IsCompressed>
    NOTSA_FORCEINLINE double GetTimeRemainingProgress() const {
        const auto kf = m_Seq->GetKeyFrame<IsCompressed>(m_KFCurr);
        if constexpr (IsCompressed) {
            const auto dt = AnimBlendNodeX87::CompressedDeltaTime(kf);
            return AnimBlendNodeX87::CompressedDeltaTimeRaw(kf) == 0
                ? 0.0
                : (dt - (double)m_KFRemainingTime) / dt;
        } else {
            return kf->DeltaTime == 0.0f // NaN goes to the division (`test ah, 0x44; jnp`)
                ? 0.0
                : ((double)kf->DeltaTime - (double)m_KFRemainingTime) / (double)kf->DeltaTime;
        }
    }

    // 0x4CFC50 / 0x4CFE60
    template<bool IsCompressed>
    NOTSA_FORCEINLINE void I_GetCurrentTranslation(CVector& trans, float weight) {
        trans = CVector{0.0f, 0.0f, 0.0f};

        // Uncompressed: the blend amount stays on the x87 stack unrounded, compressed: only the copy that is multiplied in is rounded to float
        const double blend = AnimBlendNodeX87::BlendAmountExt(m_BlendAssoc, weight);
        if (!(blend > 0.0)) { // `test ah, 0x41; jne` => return if !(blend > 0) (NaN returns)
            return;
        }

        if (!m_Seq->m_bHasTranslation) {
            return;
        }

        const auto kfA = m_Seq->GetKeyFrame<IsCompressed>(m_KFCurr),
                   kfB = m_Seq->GetKeyFrame<IsCompressed>(m_KFPrev);
        if constexpr (IsCompressed) {
            const float t = (float)GetTimeRemainingProgress<true>(); // Spilled
            trans = AnimBlendNodeX87::LerpTranslation<false>(kfB->Trans, kfA->Trans, (double)t);
            trans *= (float)blend;
        } else {
            trans = AnimBlendNodeX87::LerpTranslation<true>(kfB->Trans, kfA->Trans, GetTimeRemainingProgress<false>());
            trans = CVector{
                (float)((double)trans.x * blend),
                (float)((double)trans.y * blend),
                (float)((double)trans.z * blend),
            };
        }
    }

    // 0x4CFD90 / 0x4D0000
    template<bool IsCompressed>
    NOTSA_FORCEINLINE void I_GetEndTranslation(CVector& trans, float weight) {
        trans = CVector{ 0.0f, 0.0f, 0.0f };

        const double blend = AnimBlendNodeX87::BlendAmountExt(m_BlendAssoc, weight);
        if (!(blend > 0.0)) { // `test ah, 0x41; jne` => return if !(blend > 0) (NaN returns)
            return;
        }

        if (!m_Seq->m_bHasTranslation) {
            return;
        }

        const auto kf = m_Seq->GetKeyFrame<IsCompressed>(m_Seq->m_FramesNum - 1);
        const CVector kfTrans = kf->Trans;
        if constexpr (IsCompressed) {
            trans = kfTrans;
            trans *= (float)blend;
        } else {
            trans = CVector{
                (float)(blend * kfTrans.x),
                (float)(blend * kfTrans.y),
                (float)(blend * kfTrans.z),
            };
        }
    }

    // 0x4D04A0 / 0x4D0570 (/ 0x4CFB90 if `!CalcDeltas`)
    template<bool IsCompressed, bool DoCalcDeltas = true>
    NOTSA_FORCEINLINE bool I_NextKeyFrame() {
        if (m_Seq->m_FramesNum <= 1) {
            return false;
        }

        // Store old KF
        m_KFPrev = m_KFCurr;

        // Find next frame, this should usually be the next one
        // Unless `m_BlendAssoc->m_TimeStep > NextKF->DeltaTime` (In which case `m_KFRemainingTime` becomes negative)
        bool looped = false;
        while (m_KFRemainingTime <= 0.0f) {
            m_KFCurr++;

            if (m_KFCurr >= m_Seq->m_FramesNum) {
                // reached end of animation
                if (!m_BlendAssoc->IsLooped()) {
                    m_KFCurr--;
                    m_KFRemainingTime = 0.0f;
                    return false;
                }
                looped   = true;
                m_KFCurr = 0;
            }

            const auto kf = m_Seq->GetKeyFrame<IsCompressed>(m_KFCurr);
            if constexpr (IsCompressed) {
                // `fild; fmul; fadd [remaining]; fstp` => single rounding
                m_KFRemainingTime = (float)(AnimBlendNodeX87::CompressedDeltaTime(kf) + (double)m_KFRemainingTime);
            } else {
                m_KFRemainingTime += kf->DeltaTime;
            }
        }

        m_KFPrev = m_KFCurr == 0
            ? m_Seq->m_FramesNum - 1 // Previous frame was the last in the sequence
            : m_KFCurr - 1;

        if constexpr (DoCalcDeltas) {
            if constexpr (IsCompressed) {
                CalcDeltasCompressed();
            } else {
                CalcDeltas();
            }
        }

        return looped;
    }

    // 0x4D06C0 / 0x4D08D0
    template<bool IsCompressed>
    NOTSA_FORCEINLINE bool I_Update(CVector& trans, CQuaternion& rot, float weight) {
        bool looped = false;

        trans = CVector{0.0f, 0.0f, 0.0f};
        rot   = CQuaternion{0.0f, 0.0f, 0.0f, 0.0f};

        if (m_BlendAssoc->IsPlaying()) {
            m_KFRemainingTime -= m_BlendAssoc->m_TimeStep;
            if (m_KFRemainingTime <= 0.0f) {
                looped = I_NextKeyFrame<IsCompressed>();
            }
        }

        // Uncompressed: spilled to float, then compared. Compressed: the unrounded product is compared, the rounded copy is used.
        const double blendExt = AnimBlendNodeX87::BlendAmountExt(m_BlendAssoc, weight);
        const float  blend    = (float)blendExt;
        if (IsCompressed ? blendExt > 0.0 : blend > 0.0f) {
            const auto kfA = m_Seq->GetKeyFrame<IsCompressed>(m_KFCurr);
            const auto kfB = m_Seq->GetKeyFrame<IsCompressed>(m_KFPrev);

            // Calculate interpolation t (spilled to float)
            const float t = (float)GetTimeRemainingProgress<IsCompressed>();

            // Sanity check
            assert(m_Seq->m_bHasTranslation || m_Seq->m_bHasRotation);

            // Update translation
            if (m_Seq->m_bHasTranslation) {
                trans = AnimBlendNodeX87::LerpTranslation<false>(kfB->Trans, kfA->Trans, (double)t);
                trans *= blend;
            }

            // Update rotation
            if (m_Seq->m_bHasRotation) {
                rot.Slerp(kfB->Rot, kfA->Rot, m_Theta, m_InvSinTheta, t);
                rot *= blend;
            }
        }

        return looped;
    }

public:
    float                  m_Theta{};           //!< Angle between current and previous key-frames
    float                  m_InvSinTheta{};     //!< 1 / sin(m_Theta), used in slerp calculation
    int16                  m_KFCurr{ -1 };      //!< Current key frame (A)
    int16                  m_KFPrev{ -1 };      //!< Previous key frame (B)
    float                  m_KFRemainingTime{}; //!< Time until frames have to advance (So, basically time left from current key-frame, so <frame progress> = m_KFRemainingTime / KeyFrameCurr->DeltaTime)
    CAnimBlendSequence*    m_Seq{};             //!< Node animation key-frames
    CAnimBlendAssociation* m_BlendAssoc{};      //!< Parent anim to which this node belongs to
};
VALIDATE_SIZE(CAnimBlendNode, 0x18);

inline CAnimBlendNode* CAnimBlendAssociation::GetNode(int32 nodeIndex) { return &GetNodes()[nodeIndex]; } // 0x4CEB60
