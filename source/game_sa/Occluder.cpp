#include "StdInc.h"

#include "ActiveOccluder.h"
#include "Occluder.h"
#include "Occlusion.h"

void COccluder::InjectHooks() {
    RH_ScopedClass(COccluder);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(ProcessOneOccluder, 0x71E5D0);
    RH_ScopedInstall(ProcessLineSegment, 0x71E130);
    RH_ScopedInstall(NearCamera, 0x71F960);
}

// 0x71E5D0
// The asm is followed literally (verified against the exe with fixed_oracle_test): the visibility dot products have a different
// term order per face, the face offsets use the UNNORMALISED direction, the flat variant normalises its normal.
bool COccluder::ProcessOneOccluder(CActiveOccluder* out) {
    out->m_LinesUsed = 0;
    const auto center = CVector{m_Center};

    if (!CalcScreenCoors(center, CenterOnScreen)) {
        return false;
    }
    if (CenterOnScreen.z < -150.0F || CenterOnScreen.z > 300.0F) { // 0x71E684: `z < -150` / `z > 300` (NaN passes)
        return false;
    }
    const auto size = GetSize();

    // 0x71E6D5: sqrt((l^2 + w^2) + h^2) stays on the x87 stack, `fsubr` from the centre depth, then _ftol2; only the low word is stored
    {
        const x87d mag = x87::sqrt(((x87d)size.y * size.y + (x87d)size.x * size.x) + (x87d)size.z * size.z);
        out->m_DistToCam = (uint16)(int32)((x87d)CenterOnScreen.z - mag);
    }

    // 0x71E743..0x71E7FF: NOT SetRotate(x, y, z) (0x59B120): the exe builds X, Y, Z rotations (0x59B060 / 0x59B0A0 / 0x59B0E0) and multiplies (Y * X) * Z (0x59BE30)
    const auto rot = GetRotation();
    CMatrix rotX{}, rotY{}, rotZ{};
    rotX.SetRotateX(rot.x);
    rotY.SetRotateY(rot.y);
    rotZ.SetRotateZ(rot.z);
    const CMatrix transform = (rotY * rotX) * rotZ;

    MinXInOccluder = 999999.88F;
    MinYInOccluder = 999999.88F;
    MaxXInOccluder = -999999.88F;
    MaxYInOccluder = -999999.88F;

    if (size.x != 0 && size.y != 0.f && size.z != 0.f) {
        const auto right   = transform.TransformPoint(CVector(size.x / 2.0F, 0.0F, 0.0F));
        const auto up      = transform.TransformPoint(CVector(0.0F, size.y / 2.0F, 0.0F));
        const auto forward = transform.TransformPoint(CVector(0.0F, 0.0F, size.z / 2.0F));

        enum {
            DUP,
            DBOTTOM,

            DRIGHT,
            DLEFT,

            DBACK,
            DFRONT,
        };
        const std::array directions{
            up, -up,
            right, -right,
            forward, -forward
        };

        // Figure out if we see the front or back of a face: (center + dir - cam) . dir < 0
        // 0x71EAFF..0x71EDDD: the per-component sums are partly spilled to float, partly kept on the x87 stack, and the three products
        // are added in a different order per face (the compiler scheduled each inlined copy differently)
        const auto& cam = TheCamera.GetPosition();
        enum class Sum { XYZ, XZY, ZYX };
        const auto IsVertexOnScreen = [&](int32 i, Sum order, bool dxStack) -> bool {
            const auto& d = directions[i];
            const x87d dy = ((x87d)center.y + d.y) - cam.y;
            x87d dxD, dzD;
            if (!dxStack) { // 0x71EB30: x spilled as float (`fstp [esp+0x10]`), z summed to float then `fsub` stays on the stack
                dxD = (float)(((x87d)d.x + center.x) - cam.x);
                dzD = (x87d)(float)((x87d)center.z + d.z) - cam.z;
            } else { // 0x71ED97 (last face): x summed to float, z subtracted and spilled to float
                dzD = (float)(((x87d)center.z + d.z) - cam.z);
                dxD = (x87d)(float)((x87d)center.x + d.x) - cam.x;
            }
            x87d sum;
            switch (order) {
            case Sum::XYZ: sum = (dxD * d.x + dy * d.y) + dzD * d.z; break;
            case Sum::XZY: sum = (dxD * d.x + dzD * d.z) + dy * d.y; break;
            default:       sum = (dzD * d.z + dy * d.y) + dxD * d.x; break;
            }
            return sum < 0.0; // FCOMP + JP: NaN is false
        };
        const std::array onScreen{
            IsVertexOnScreen(DUP,     Sum::XYZ, false),
            IsVertexOnScreen(DBOTTOM, Sum::XZY, false),

            IsVertexOnScreen(DRIGHT,  Sum::XYZ, false),
            IsVertexOnScreen(DLEFT,   Sum::XZY, false),

            IsVertexOnScreen(DBACK,   Sum::ZYX, false),
            IsVertexOnScreen(DFRONT,  Sum::ZYX, true),
        };

        OccluderCoors[0] = center + directions[DUP]     + directions[DRIGHT] + directions[DBACK]; // back top right
        OccluderCoors[1] = center + directions[DBOTTOM] + directions[DRIGHT] + directions[DBACK]; // back bottom right
        OccluderCoors[2] = center + directions[DUP]     + directions[DLEFT]  + directions[DBACK]; // back top left
        OccluderCoors[3] = center + directions[DBOTTOM] + directions[DLEFT]  + directions[DBACK]; // back bottom left

        OccluderCoors[4] = center + directions[DUP]     + directions[DRIGHT] + directions[DFRONT]; // front top right
        OccluderCoors[5] = center + directions[DBOTTOM] + directions[DRIGHT] + directions[DFRONT]; // front bottom right
        OccluderCoors[6] = center + directions[DUP]     + directions[DLEFT]  + directions[DFRONT]; // front top left
        OccluderCoors[7] = center + directions[DBOTTOM] + directions[DLEFT]  + directions[DFRONT]; // front bottom left

        for (auto i = 0; i < 8; ++i) {
            OccluderCoorsValid[i] = CalcScreenCoors(OccluderCoors[i], OccluderCoorsOnScreen[i]);
        }

        // Between two differently facing sides we see an edge, so process those
        if ((onScreen[DUP] == onScreen[DRIGHT] || !ProcessLineSegment(0, 4, out))
         && (onScreen[DUP] == onScreen[DLEFT] || !ProcessLineSegment(2, 6, out))
         && (onScreen[DUP] == onScreen[DBACK] || !ProcessLineSegment(0, 2, out))
         && (onScreen[DUP] == onScreen[DFRONT] || !ProcessLineSegment(4, 6, out))
         && (onScreen[DBOTTOM] == onScreen[DRIGHT] || !ProcessLineSegment(1, 5, out))
         && (onScreen[DBOTTOM] == onScreen[DLEFT] || !ProcessLineSegment(3, 7, out))
         && (onScreen[DBOTTOM] == onScreen[DBACK] || !ProcessLineSegment(1, 3, out))
         && (onScreen[DBOTTOM] == onScreen[DFRONT] || !ProcessLineSegment(5, 7, out))
         && (onScreen[DRIGHT] == onScreen[DBACK] || !ProcessLineSegment(0, 1, out))
         && (onScreen[DLEFT] == onScreen[DBACK] || !ProcessLineSegment(2, 3, out))
         && (onScreen[DLEFT] == onScreen[DFRONT] || !ProcessLineSegment(6, 7, out))
         && (onScreen[DRIGHT] == onScreen[DFRONT] || !ProcessLineSegment(4, 5, out))
        ) {
            // 0x71F321: `fcompp` + `je`: fails only if W * 0.15 > dx (ordered), so NaN passes
            if (!(SCREEN_WIDTH * 0.15F > MaxXInOccluder - MinXInOccluder)
             && !(SCREEN_HEIGHT * 0.1F > MaxYInOccluder - MinYInOccluder)
            ) {
                out->m_NumFaces = 0;
                for (auto i = 0; i < 6; ++i) {
                    if (!onScreen[i]) {
                        continue;
                    }
                    // 0x71F38E: (dir + center) is built from the UNNORMALISED direction, `dir` itself is then normalised (0x59C910)
                    const auto& d = directions[i];
                    const float vx = d.x + center.x, vy = d.y + center.y, vz = d.z + center.z;
                    CVector dir = d;
                    dir.Normalise();

                    out->m_FaceNormals[(int8)out->m_NumFaces] = dir;
                    out->m_FaceOffsets[(int8)out->m_NumFaces] = (float)(((x87d)vx * dir.x + (x87d)vy * dir.y) + (x87d)vz * dir.z);

                    ++out->m_NumFaces;
                }
                return true;
            }
        }

        return false;
    }

    CVector right, up;
    if (size.y == 0.f) {
        right = CVector{size.x, 0.f, 0.f   };
        up    = CVector{0.f,   0.f, size.z };
    } else if (size.x == 0.f) {
        right = CVector{0.f, size.y, 0.f   };
        up    = CVector{0.f, 0.f,   size.z };
    } else if (size.z == 0.f) {
        right = CVector{0.f,    size.y, 0.f};
        up    = CVector{size.x , 0.f,   0.f};
    }
    right = transform.TransformPoint(right / 2.f);
    up    = transform.TransformPoint(up / 2.f);
    OccluderCoors[0] = center + right + up; // top right -- top right
    OccluderCoors[1] = center - right + up; // top left -- bottom right
    OccluderCoors[2] = center - right - up; // bottom left -- bottom left
    OccluderCoors[3] = center + right - up; // bottom right -- top left

    for (auto i = 0; i < 4; ++i) {
        OccluderCoorsValid[i] = CalcScreenCoors(OccluderCoors[i], OccluderCoorsOnScreen[i]);
    }

    if (ProcessLineSegment(0, 1, out)
     || ProcessLineSegment(1, 2, out)
     || ProcessLineSegment(2, 3, out)
     || ProcessLineSegment(3, 0, out)
    )  {
        return false;
    }
    // 0x71F7EB: fails only if W * 0.1 > dx / H * 0.07 > dy (ordered)
    if (SCREEN_WIDTH * 0.1F > MaxXInOccluder - MinXInOccluder
     || SCREEN_HEIGHT * 0.07F > MaxYInOccluder - MinYInOccluder
    ) {
        return false;
    }

    // 0x71F844: CrossProduct, 0x71F850: Normalise, offset = (n.x * c.x + n.y * c.y) + n.z * c.z
    auto normal = right.Cross(up);
    normal.Normalise();
    out->m_FaceNormals[0] = normal;
    out->m_FaceOffsets[0] = (float)(((x87d)normal.x * center.x + (x87d)normal.y * center.y) + (x87d)normal.z * center.z);
    out->m_NumFaces       = 1;

    return true;
}


// 0x71E130
// (static in spirit: `this` is unused, ECX is ignored)
bool COccluder::ProcessLineSegment(int32 idxFrom, int32 idxTo, CActiveOccluder* activeOccluder) {
    if (!OccluderCoorsValid[idxFrom] && !OccluderCoorsValid[idxTo]) {
        return true;
    }

    // Calcualte/get on-screen coordinates of the occluder
    // 0x71E184 / 0x71E28F: BOTH endpoints use the same formula (interpolate FROM -> TO to the plane z = 1.1 using the depths of both
    // endpoints, the weights are |zTo| / (|zFrom| + |zTo|) for FROM and the rest for TO), whichever of the two is behind the camera.
    // The upstream "fix" (swapping the index roles for the second endpoint) computes the same point mathematically, but rounds
    // differently, so it is NOT applied.
    const auto GetScreenCoorsOf = [&](int32 i) -> std::optional<CVector> {
        if (OccluderCoorsValid[i]) {
            return OccluderCoorsOnScreen[i];
        }

        const auto& from = OccluderCoors[idxFrom];
        const auto& to   = OccluderCoors[idxTo];

        const float  zFrom = (float)((x87d)TheCamera.m_mViewMatrix.TransformPoint(from).z - (x87d)1.1F); // spilled to float (`fstp [esp+0x70]`)
        const x87d zTo   = (x87d)TheCamera.m_mViewMatrix.TransformPoint(to).z - (x87d)1.1F;            // stays on the stack
        const x87d t     = std::fabs(zTo) / (std::fabs((x87d)zFrom) + std::fabs(zTo));
        const x87d u     = (x87d)1.0 - t;

        // 0x71E1E0: u * to is spilled for x / y, the z products stay on the stack
        const float ax = (float)(u * to.x);
        const float ay = (float)(u * to.y);
        const float bz = (float)(t * from.z);
        const CVector pt{
            (float)(t * from.x + ax),
            (float)(t * from.y + ay),
            (float)(bz + u * to.z)
        };

        if (CVector pos; CalcScreenCoors(pt, pos)) {
            return pos;
        }
        return std::nullopt;
    };
    const auto from = GetScreenCoorsOf(idxFrom);
    if (!from) {
        return true;
    }
    const auto to = GetScreenCoorsOf(idxTo);
    if (!to) {
        return true;
    }

    // 0x71E36A: FCOMP/FCOM + JP/JNP chains (not std::min/max: ties and NaN take the later operand)
    const auto Min2 = [](float cur, float a, float b) { const float r = cur < a ? cur : a; return r < b ? r : b; };
    const auto Max2 = [](float cur, float a, float b) { const float r = cur > a ? cur : a; return r > b ? r : b; };
    MinXInOccluder = Min2(MinXInOccluder, from->x, to->x);
    MaxXInOccluder = Max2(MaxXInOccluder, from->x, to->x);
    MinYInOccluder = Min2(MinYInOccluder, from->y, to->y);
    MaxYInOccluder = Max2(MaxYInOccluder, from->y, to->y);

    auto* const l = reinterpret_cast<CActiveOccluderLine*>(activeOccluder) + (int8)activeOccluder->m_LinesUsed; // `movsx`

    // 0x71E43A: calculate line origin and dir (the deltas stay on the x87 stack)
    x87d dx = (x87d)to->x - from->x;
    x87d dy = (x87d)to->y - from->y;
    float  ox = from->x, oy = from->y;
    // IsPointInsideLine(origin, dir, centre, 0): (cx - ox) * dy - (cy - oy) * dx >= 0, the line is flipped when it is < 0 (NaN: not flipped)
    if (((x87d)CenterOnScreen.x - from->x) * dy - ((x87d)CenterOnScreen.y - from->y) * dx < 0.0) {
        // the new origin is `from + delta`, which is not exactly `to`
        ox = (float)(dx + from->x);
        oy = (float)(from->y + dy);
        dx = -dx;
        dy = -dy;
    }
    // 0x71E4B6: Normalized(&length): fsqrt of the sum of squares, 1.0 / length (the float length), both components multiplied
    const float  len = (float)x87::sqrt(dy * dy + dx * dx);
    const x87d inv = (x87d)1.0 / (x87d)len;
    l->Length = len;
    l->Origin = { ox, oy };
    l->Dir    = { (float)(inv * dx), (float)(inv * dy) };

    if (!DoesInfiniteLineTouchScreen(l->Origin, l->Dir)) {
        // 0x71E52C: !IsPointInsideLine(origin, dir, screen centre, 0): (W/2 - ox) * dirY - (H/2 - oy) * dirX < 0
        const x87d w = (x87d)RsGlobal.maximumWidth  * (x87d)0.5;
        const x87d h = (x87d)RsGlobal.maximumHeight * (x87d)0.5;
        return (w - l->Origin.x) * l->Dir.y - (h - l->Origin.y) * l->Dir.x < 0.0;
    }

    ++activeOccluder->m_LinesUsed;
    return false;
}

// 0x71F960
bool COccluder::NearCamera() const {
    // 0x71F960: x87 throughout: the extents are the raw halves * 0.25 (exact), the distance is sqrt(dz^2 + dy^2 + dx^2) kept in extended precision, minus half the larger extent
    const x87d len = (float)m_Length, wid = (float)m_Width;
    const x87d ext = len > wid ? len : wid;
    const CVector c   = m_Center;
    const CVector cam = TheCamera.GetPosition();
    const x87d dx = (x87d)cam.x - (x87d)c.x, dy = (x87d)cam.y - (x87d)c.y, dz = (x87d)cam.z - (x87d)c.z;
    const x87d dist = x87::sqrt(dz * dz + dy * dy + dx * dx);
    return dist - ext * (x87d)0.5f < (double)250.f;
}

int16 COccluder::SetNext(int16 next) {
    const auto old = m_NextIndex;
    m_NextIndex    = next;
    return old;
}
