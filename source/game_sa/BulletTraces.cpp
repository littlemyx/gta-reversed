#include "StdInc.h"
#include "BulletTraces.h"

void CBulletTraces::InjectHooks()
{
    RH_ScopedClass(CBulletTraces);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x721D50);
    RH_ScopedOverloadedInstall(AddTrace, "", 0x723750, void(*)(const CVector&, const CVector&, float, uint32, uint8));
    RH_ScopedOverloadedInstall(AddTrace, "Wrapper", 0x726AF0, void(*)(const CVector&, const CVector&, eWeaponType, CEntity*));
    RH_ScopedInstall(Render, 0x723C10);
    RH_ScopedInstall(Update, 0x723FB0);
}

// 0x721D50
void CBulletTraces::Init()
{
    for (CBulletTrace& trace : aTraces) {
        trace.m_bExists = false;
    }
}

// 0x723FB0
void CBulletTraces::Update()
{
    for (CBulletTrace& trace : aTraces) {
        trace.Update();
    }
}

CBulletTrace* CBulletTraces::GetFree() {
    for (CBulletTrace& trace : aTraces) {
        if (!trace.m_bExists) {
            return &trace;
        }
    }
    return nullptr;
}

// 0x723750 (inlined tail, 0x7238F7-0x723BFE): the whoosh of a bullet passing the camera.
// The exe works on TheCamera.m_mCameraMatrix (0xB6F99C) and TheCamera.GetPosition(), every dot product has its own term order, and the
// events are picked from the sign of pointOnLineX: x >= 0 plays the 0x1B/0x1C pair (the AE_FRONTEND_BULLET_PASS_LEFT_* enumerators), x < 0 the 0x1D/0x1E pair.
void PlayFrontEndSoundForTrace(CVector fromWorldSpace, CVector toWorldSpace) {
    const CMatrix& camMat = TheCamera.m_mCameraMatrix;
    const CVector& camPos = TheCamera.GetPosition();
    const CVector& R = camMat.GetRight();
    const CVector& F = camMat.GetForward();
    const CVector& U = camMat.GetUp();

    const float fdx = fromWorldSpace.x - camPos.x, fdy = fromWorldSpace.y - camPos.y, fdz = fromWorldSpace.z - camPos.z;
    const float tdx = toWorldSpace.x - camPos.x,   tdy = toWorldSpace.y - camPos.y,   tdz = toWorldSpace.z - camPos.z;

    // 0x72390A-0x723A79: "from" terms are z,y,x (right), x,z,y (up and forward); "to" terms are z,y,x (right), z,y,x (up and forward)
    const float fromCSX = (float)((x87d)fdz * R.z + (x87d)fdy * R.y + (x87d)fdx * R.x);
    const float fromCSZ = (float)((x87d)fdx * U.x + (x87d)fdz * U.z + (x87d)fdy * U.y);
    const float fromCSY = (float)((x87d)fdx * F.x + (x87d)fdz * F.z + (x87d)fdy * F.y);
    const float toCSX   = (float)((x87d)tdz * R.z + (x87d)tdy * R.y + (x87d)tdx * R.x);
    const float toCSZ   = (float)((x87d)tdz * U.z + (x87d)tdy * U.y + (x87d)tdx * U.x);
    const float toCSY   = (float)((x87d)tdz * F.z + (x87d)tdy * F.y + (x87d)tdx * F.x);

    if (!(toCSY * fromCSY < 0.0f)) { // 0x723A79: unordered (NaN) returns too
        return; // Both points are either in front or behind us
    }

    // Interpolation on line (0x723A90-0x723ACD)
    const float absFrom = fromCSY < 0.0f ? -fromCSY : fromCSY;
    const float absTo   = toCSY   < 0.0f ? -toCSY   : toCSY;
    const x87d  t       = (x87d)absFrom / ((x87d)absTo + absFrom);

    const float dx = (float)((x87d)toCSX - fromCSX);
    const float dz = (float)((x87d)toCSZ - fromCSZ);
    const float tz = (float)((x87d)dz * t); // 0x723AE1-0x723AED: both are spilled to floats
    const x87d  pointOnLineX = (x87d)dx * t + fromCSX;
    const x87d  pointOnLineZ = (x87d)tz + fromCSZ;

    // 0x723B09: fsqrt of z*z + x*x, stored as a float; `dist < 2.0` is false for NaN
    const float camToLineDist = (float)x87::sqrt(pointOnLineZ * pointOnLineZ + pointOnLineX * pointOnLineX);
    if (!(camToLineDist < 2.0f)) {
        return; // Point too far from camera
    }

    const x87d volDistFactor = (x87d)1.0 - (x87d)camToLineDist * (x87d)0.5; // 0x723B2A-0x723B32
    // 0x723B3B: == 0 gives -100.0f, otherwise fldlg2/fyl2x then x 20.0 (decibels, 0x858BA4)
    const float volumeChange = volDistFactor == 0.0 ? -100.0f : (float)X87R(x87::log10(X87W(volDistFactor)) * (x87d)20.0);

    const bool isComingFromBehind = !(fromCSY > 0.0f); // 0x723B66: fcomp 0.0, test ah,0x41
    if (pointOnLineX >= 0.0 || pointOnLineX != pointOnLineX) {
        AudioEngine.ReportFrontendAudioEvent(isComingFromBehind ? AE_FRONTEND_BULLET_PASS_LEFT_REAR : AE_FRONTEND_BULLET_PASS_LEFT_FRONT, volumeChange, 1.0f);
    } else {
        AudioEngine.ReportFrontendAudioEvent(isComingFromBehind ? AE_FRONTEND_BULLET_PASS_RIGHT_REAR : AE_FRONTEND_BULLET_PASS_RIGHT_FRONT, volumeChange, 1.0f);
    }
}

// 0x723750
void CBulletTraces::AddTrace(const CVector& from, const CVector& to, float radius, uint32 disappearTime, uint8 alpha)
{
    // 0x723750-0x7237B6: the lifetime depends on how many traces exist right now (not on the index of the free slot)
    const auto numTraces = (size_t)rng::count_if(aTraces, [](const CBulletTrace& t) { return t.m_bExists; });
    uint32 lifeTime;
    if (numTraces >= 10) {
        lifeTime = NOTSA_FTOL(uint32, (float)disappearTime * 0.25f);
    } else if (numTraces >= 5) {
        lifeTime = NOTSA_FTOL(uint32, (float)disappearTime * 0.5f);
    } else {
        lifeTime = disappearTime;
    }

    if (CBulletTrace* pTrace = GetFree()) {
        pTrace->m_vecStart = from;
        pTrace->m_vecEnd = to;
        pTrace->m_nCreationTime = CTimer::GetTimeInMS();
        pTrace->m_nTransparency = alpha;
        pTrace->m_bExists = true;
        pTrace->m_fRadius = radius;
        pTrace->m_nLifeTime = lifeTime;
    }
    PlayFrontEndSoundForTrace(from, to);
}

// 0x726AF0
void CBulletTraces::AddTrace(const CVector& posMuzzle, const CVector& posBulletHit, eWeaponType weaponType, CEntity* fromEntity)
{
    if (FindPlayerPed() == fromEntity || FindPlayerVehicle() && FindPlayerVehicle() == fromEntity) {
        switch (CCamera::GetActiveCamera().m_nMode) {
        case MODE_M16_1STPERSON:
        case MODE_SNIPER:
        case MODE_CAMERA:
        case MODE_ROCKETLAUNCHER:
        case MODE_ROCKETLAUNCHER_HS:
        case MODE_M16_1STPERSON_RUNABOUT:
        case MODE_SNIPER_RUNABOUT:
        case MODE_ROCKETLAUNCHER_RUNABOUT:
        case MODE_ROCKETLAUNCHER_RUNABOUT_HS:
        case MODE_HELICANNON_1STPERSON: {
            const CVector& speed = FindPlayerEntity()->AsPhysical()->m_vecMoveSpeed; // 0x726BA6-0x726BE6: z,y,x sum
            if ((float)x87::sqrt((x87d)speed.z * speed.z + (x87d)speed.y * speed.y + (x87d)speed.x * speed.x) < 0.05f) {
                return;
            }
        }
        }
    }

    // 0x726BEC-0x726CF5: z,y,x length; the exe's min() macro evaluates GetRandomNumberInRange(2, 5) a second time (a second rand() call) when it is the smaller operand
    CVector dir = posBulletHit - posMuzzle;
    const float traceLengthOriginal = (float)x87::sqrt((x87d)dir.z * dir.z + (x87d)dir.y * dir.y + (x87d)dir.x * dir.x);
    dir.Normalise();

    const float traceLengthNew = CGeneral::GetRandomNumberInRange(0.0f, traceLengthOriginal);
    const float remaining      = traceLengthOriginal - traceLengthNew;
    const float fRadius        = CGeneral::GetRandomNumberInRange(2.0f, 5.0f) < remaining ? CGeneral::GetRandomNumberInRange(2.0f, 5.0f) : remaining;

    CVector from = posMuzzle + dir * traceLengthNew;
    CVector to = from + dir * fRadius;

    AddTrace(
        from,
        to,
        0.01f,
        300,
        70
    );
}

// 0x723C10
void CBulletTraces::Render() {
    ZoneScoped;

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(RwBlendFunction::rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(RwBlendFunction::rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATECULLMODE,          RWRSTATE(RwCullMode::rwCULLMODECULLNONE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(NULL));

    RxObjSpace3DVertex verts[6];
    for (CBulletTrace& trace : aTraces) {
        if (!trace.m_bExists) {
            continue;
        }

        // Time progress 1 => 0 as time passes
        const float t = 1.0f - (float)trace.GetRemainingLifetime() / (float)trace.m_nLifeTime;

        // Set vertex positions
        {
            // Direction from camera to trace's origin (aka `start`)
            const auto camToOriginDir = (trace.m_vecStart - TheCamera.GetPosition()).Normalized();

            // Upward direction relative to the camera's position
            const auto up = camToOriginDir.Cross(trace.GetDirection().Normalized()).Normalized();

            // Size vector
            const auto sizeVec = up * (trace.m_fRadius * t);

            // Current position on the trace's line segment (Inverted because `t` is going 1 => 0 not 0 => 1)
            const CVector currPosOnTrace = trace.m_vecEnd - trace.GetDirection() * t;

            // Finally, calculate the position for each vertex
            const CVector vertPositions[std::size(verts)] = {
                currPosOnTrace,
                currPosOnTrace + sizeVec,
                currPosOnTrace - sizeVec,

                trace.m_vecEnd,
                trace.m_vecEnd + sizeVec,
                trace.m_vecEnd - sizeVec,
            };

            for (const auto& [idx, pos] : rngv::enumerate(vertPositions)) {
                RwV3dAssign(RwIm3DVertexGetPos(&verts[idx]), &pos);
            }
        }

        // Set colors
        for (auto& vert : verts) {
            RwIm3DVertexSetRGBA(&vert, 255, 255, 128, 0);
        }
        RwIm3DVertexSetRGBA(&verts[3], 255, 255, 128, NOTSA_FTOL(int8, (float)trace.m_nTransparency * t)); // Only vertex 3 has non-zero alpha

        if (RwIm3DTransform(verts, std::size(verts), nullptr, rwIM3D_VERTEXRGBA)) {
            RwImVertexIndex indices[] = {
                // Each row represents a triangle
                4, 1, 3,
                1, 0, 3,
                0, 2, 3,
                3, 2, 5
            };
            RwIm3DRenderIndexedPrimitive(RwPrimitiveType::rwPRIMTYPETRILIST, indices, std::size(indices));
            RwIm3DEnd();
        }
    }

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,     RWRSTATE(RwBlendFunction::rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,    RWRSTATE(RwBlendFunction::rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATECULLMODE,     RWRSTATE(RwCullMode::rwCULLMODECULLBACK));
}
