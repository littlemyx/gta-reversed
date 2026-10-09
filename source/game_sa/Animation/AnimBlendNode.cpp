#include "StdInc.h"

#include "AnimBlendNode.h"

namespace {
// 0x4D00E0 - cdecl helper (NOT a CAnimBlendNode member): theta = acos(min(dot(a, b), 1)), invSinTheta = theta == 0 ? 0 : 1 / sin(theta)
void CalcThetaFromQuats(const CQuaternion* a, const CQuaternion* b, float* theta, float* invSinTheta) {
    CQuaternion::CalcThetaFromQuats(*a, *b, *theta, *invSinTheta); // the exe's x87 sequence, shared with 0x59C630
}
} // namespace

void CAnimBlendNode::InjectHooks() {
    RH_ScopedClass(CAnimBlendNode);
    RH_ScopedCategory("Animation");

    RH_ScopedInstall(Init, 0x4CFB70);
    RH_ScopedInstall(NextKeyFrameNoCalc, 0x4CFB90);
    RH_ScopedInstall(GetCurrentTranslation, 0x4CFC50);
    RH_ScopedInstall(GetCurrentTranslationCompressed, 0x4CFE60);
    RH_ScopedInstall(GetEndTranslation, 0x4CFD90);
    RH_ScopedInstall(GetEndTranslationCompressed, 0x4D0000);
    RH_ScopedGlobalInstall(CalcThetaFromQuats, 0x4D00E0); // NOTE: Is a cdecl helper (CQuaternion*, CQuaternion*, float*, float*), not a thiscall member
    RH_ScopedInstall(UpdateTime, 0x4D0160);
    RH_ScopedInstall(CalcDeltas, 0x4D0190);
    RH_ScopedInstall(FindKeyFrame, 0x4D0240);
    RH_ScopedInstall(CalcDeltasCompressed, 0x4D0350);
    RH_ScopedInstall(NextKeyFrame, 0x4D04A0);
    RH_ScopedInstall(NextKeyFrameCompressed, 0x4D0570);
    RH_ScopedInstall(SetupKeyFrameCompressed, 0x4D0650);
    RH_ScopedInstall(Update, 0x4D06C0);
    RH_ScopedInstall(UpdateCompressed, 0x4D08D0);
}

// 0x4CFB70
void CAnimBlendNode::Init() {
    // NOTE: The original does not touch m_Theta / m_InvSinTheta
    m_KFCurr          = -1;
    m_KFPrev          = -1;
    m_KFRemainingTime = 0.0f;
    m_Seq             = nullptr;
    m_BlendAssoc      = nullptr;
}

// 0x4D0240
bool CAnimBlendNode::FindKeyFrame(float time) {
    if (m_Seq->m_FramesNum < 1) {
        return false;
    }

    m_KFCurr = 0;
    m_KFPrev = 0;

    if (m_Seq->m_FramesNum == 1) {
        m_KFRemainingTime = 0.0f;
        CalcDeltas();
        return true;
    }

    // `time` lives on the x87 stack (unrounded) for the whole search
    double t = time;

    // Find kf that spans over the specified `time`
    m_KFCurr = 1;
    while (t > (double)m_Seq->GetUKeyFrame(m_KFCurr)->DeltaTime) { // `test ah, 0x41; jne` => leaves if !(t > dt) (NaN leaves)
        t -= (double)m_Seq->GetUKeyFrame(m_KFCurr)->DeltaTime;

        if (m_KFCurr + 1 >= m_Seq->m_FramesNum) {
            // reached end of animation
            if (!m_BlendAssoc->IsLooped()) {
                CalcDeltas();
                m_KFRemainingTime = 0.0f;
                return false;
            }
            m_KFCurr = 0;
        }

        m_KFPrev = m_KFCurr++;
    }

    // Now calculate how much we have remaining from this frame
    m_KFRemainingTime = (float)((double)m_Seq->GetUKeyFrame(m_KFCurr)->DeltaTime - t);

    CalcDeltas();
    return true;
}

// NOTSA: The exe has no such member, its callers inline this (0x4D00E0 is the cdecl helper `CalcThetaFromQuats` above, which does the dot product too)
void CAnimBlendNode::CalcTheta(const CQuaternion& a, const CQuaternion& b) {
    CalcThetaFromQuats(&a, &b, &m_Theta, &m_InvSinTheta);
}

// 0x4D0190
void CAnimBlendNode::CalcDeltas() {
    if (!m_Seq->m_bHasRotation) {
        return;
    }

    CalcTheta(
        m_Seq->GetUKeyFrame(m_KFCurr)->Rot,
        m_Seq->GetUKeyFrame(m_KFPrev)->Rot
    );
}

// 0x4D0350
void CAnimBlendNode::CalcDeltasCompressed() {
    if (!m_Seq->m_bHasRotation) {
        return;
    }

    KeyFrameCompressed* kfA = m_Seq->GetCKeyFrame(m_KFCurr);
    KeyFrameCompressed* kfB = m_Seq->GetCKeyFrame(m_KFPrev);

    CQuaternion rotB = kfB->Rot; // Decoded first
    CQuaternion rotA = kfA->Rot;

    // Dot is accumulated in extended precision in this order (w, z, y, x) and NOT rounded before the comparison
    const double dot = (((double)rotB.w * rotA.w + (double)rotB.z * rotA.z) + (double)rotB.y * rotA.y) + (double)rotB.x * rotA.x;
    if (dot < 0.0) {
        // Flip the CURRENT key frame (A) (not B), and write it back to the key-frame (re-quantised, truncating)
        rotA     = -rotA;
        kfA->Rot = rotA;
    }
    CalcTheta(rotA, rotB); // Uses the (float) negated values, not the re-quantised ones
}

// 0x4D0650
bool CAnimBlendNode::SetupKeyFrameCompressed() {
    if (m_Seq->m_FramesNum < 1) {
        return false;
    }

    m_KFCurr = 1;
    m_KFPrev = 0;

    if (m_Seq->m_FramesNum == 1) {
        m_KFCurr          = 0;
        m_KFRemainingTime = 0.0f;
    } else {
        // `fild; fmul [0x859044]; fstp` (NOT a division by 60)
        m_KFRemainingTime = (float)AnimBlendNodeX87::CompressedDeltaTime(m_Seq->GetCKeyFrame(m_KFCurr));
    }

    CalcDeltasCompressed();
    return true;
}

// 0x4D0160 - Unused
bool CAnimBlendNode::UpdateTime() {
    if (m_BlendAssoc->IsPlaying()) {
        m_KFRemainingTime -= m_BlendAssoc->m_TimeStep;
        if (m_KFRemainingTime <= 0.0f) {
            return NextKeyFrameNoCalc();
        }
    }
    return false;
}
