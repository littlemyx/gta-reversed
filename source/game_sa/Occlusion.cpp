#include "StdInc.h"

#include "Occlusion.h"
#include "Occluder.h"
#include "ActiveOccluder.h"

void COcclusion::InjectHooks() {
    RH_ScopedClass(COcclusion);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x71DCA0);
    RH_ScopedInstall(AddOne, 0x71DCD0);
    RH_ScopedInstall(IsPositionOccluded, 0x7200B0);
    RH_ScopedInstall(OccluderHidesBehind, 0x71E080);
    RH_ScopedInstall(ProcessBeforeRendering, 0x7201C0);
}

// 0x71DCA0
void COcclusion::Init() {
    ZoneScoped;

    NumOccludersOnMap         = 0;
    NumInteriorOccludersOnMap = 0;
    FarAwayList               = -1;
    NearbyList                = -1;
    ListWalkThroughFA         = -1;
    PreviousListWalkThroughFA = -1;
}

// 0x71DCD0
// Yes, rotation is z, y, x
void COcclusion::AddOne(float centerX, float centerY, float centerZ, float width, float length, float height, float rotZ, float rotY, float rotX, uint32 flags, bool isInterior) {
    // 0x71DCD0: `_ftol(fabs(x))` (an INTEGER; the exe's _ftol keeps the low dword for huge values), later `fild; fmul 4.0; _ftol`
    const int32 l = (int32)std::fabsf(length);
    const int32 w = (int32)std::fabsf(width);
    const int32 h = (int32)std::fabsf(height);
    // 0x71DD02..0x71DD1C: the exe counts the zero extents and rejects the occluder when MORE THAN ONE of them is zero (a flat occluder has exactly one)
    if ((int)(l == 0) + (int)(w == 0) + (int)(h == 0) > 1) {
        return;
    }

    // 0x71DE43 / 0x71DF65: the exe refuses to overflow the arrays (1000 map / 40 interior occluders)
    if (isInterior ? NumInteriorOccludersOnMap >= InteriorOccluders.size() : NumOccludersOnMap >= Occluders.size()) {
        return;
    }

    auto* const occl = isInterior
        ? &InteriorOccluders[NumInteriorOccludersOnMap++]
        : &Occluders[NumOccludersOnMap++];

    occl->m_Center   = CVector{ centerX, centerY, centerZ };
    const auto CompressExtent = [](int32 v) { return (int16)(int32)((double)v * (double)4.f); };
    occl->m_Length   = decltype(occl->m_Length){ CompressExtent(l) }; // pre-compressed
    occl->m_Width    = decltype(occl->m_Width){ CompressExtent(w) };
    occl->m_Height   = decltype(occl->m_Height){ CompressExtent(h) };
    // 0x71DD22..0x71DE37: each angle is brought into [0, 360] by repeated +-360 (no `+ 180`, no CGeneral::LimitAngle), multiplied by 0x8595EC (pi/180 as float) and stored as a float
    const auto AngleToRad = [](float deg) {
        double a = deg;
        if (a < 0.0) {
            do { a += (double)360.f; } while (a < 0.0);
        }
        if (a > (double)360.f) {
            do { a -= (double)360.f; } while (a > (double)360.f);
        }
        return (float)(a * (double)std::bit_cast<float>(0x3C8EFA35u));
    };
    // 0x71DE13..0x71DF0C: the angle arguments are stored in the order +0xC (m_RotZ) = 1st, +0xD (m_RotY) = 2nd, +0xE (m_RotX) = 3rd
    occl->m_RotZ     = AngleToRad(rotZ);
    occl->m_RotY     = AngleToRad(rotY);
    occl->m_RotX     = AngleToRad(rotX);

    if (!isInterior) {
        occl->m_DontStream = flags != 0;
        occl->m_NextIndex  = FarAwayList;
        FarAwayList        = NumOccludersOnMap - 1;
    }
}

// 0x71E080
bool COcclusion::OccluderHidesBehind(CActiveOccluder* first, CActiveOccluder* second) {
    if (!first->m_LinesUsed) {
        return second->m_LinesUsed == 0;
    }
    for (auto a = 0; a < first->m_LinesUsed; ++a) {
        const auto& lineA = first->m_Lines[a];
        for (auto b = 0; b < second->m_LinesUsed; ++b) {
            const auto& lineB = second->m_Lines[b];

            if (!IsPointInsideLine(lineB.Origin, lineB.Dir, lineA.Origin, 0.0f)) {
                return false;
            }
            if (!IsPointInsideLine(
                lineB.Origin,
                lineB.Dir,
                lineA.Origin + lineA.Dir * lineA.Length,
                0.0f
            )) {
                return false;
            }
        }
    }
    return true;
}

// 0x7200B0
bool COcclusion::IsPositionOccluded(CVector pos, float radius) {
    if (!NumActiveOccluders) {
        return false;
    }

    CVector scrPos;
    float   scaleX, scaleY;
    if (!CalcScreenCoors(pos, scrPos, scaleX, scaleY)) {
        return false;
    }

    const auto longEdge     = std::max(scaleX, scaleY);
    const auto screenRadius = radius * longEdge;
    const auto screenDepth  = scrPos.z - radius;
    return rng::any_of(GetActiveOccluders(), [=](auto&& o) {
        return o.GetDistToCam() <= screenDepth
            && o.IsPointWithinOcclusionArea(scrPos, screenRadius)
            && o.IsPointBehindOccluder(pos, radius);
    });
}

// 0x7201C0
void COcclusion::ProcessBeforeRendering() {
    NumActiveOccluders = 0;

    // Update nearby and far-away lists
    if (CGame::CanSeeOutSideFromCurrArea()) {
        // @ 0x72021A - Update far-away list
        const auto UpdateFarAwayList = [&] {
            for (int32 size{}; size < 16 && ListWalkThroughFA != -1; size++) { // We do at most 16 iterations!
                auto& occluder = Occluders[ListWalkThroughFA];

                // If the occluder is now near move it into the (nearby) list
                if (occluder.NearCamera()) { // 0x72022E
                    if (PreviousListWalkThroughFA == -1) {
                        FarAwayList = occluder.GetNext();
                    } else {
                        Occluders[PreviousListWalkThroughFA].SetNext(occluder.GetNext());
                    }
                    ListWalkThroughFA = occluder.SetNext(std::exchange(NearbyList, ListWalkThroughFA));
                } else {
                    PreviousListWalkThroughFA = std::exchange(ListWalkThroughFA, occluder.GetNext());
                }
            }
        };

        // @ 0x720307 - Update nearby list
        const auto UpdateNearbyList = [&] {
            for (int16 i{ NearbyList }, prev{ -1 }; i != -1;) {
                auto& occluder = Occluders[i];

                // 0x720323 - Process this occluder
                if (NumActiveOccluders < ActiveOccluders.size()) {
                    if (occluder.ProcessOneOccluder(&ActiveOccluders[NumActiveOccluders])) {
                        NumActiveOccluders++;
                    }
                }

                // If the occluder is now far away move it into the faraway list
                if (!occluder.NearCamera()) { // 0x72035A (inverted!)
                    if (prev == -1) {
                        NearbyList = occluder.GetNext();
                    } else {
                        Occluders[prev].SetNext(occluder.GetNext());
                    }
                    i = occluder.SetNext(std::exchange(FarAwayList, i));
                } else { // 0x7203D6
                    prev = std::exchange(i, occluder.GetNext());
                }
            }
        };

        if (ListWalkThroughFA == -1) {
            PreviousListWalkThroughFA = -1;
            ListWalkThroughFA         = FarAwayList;
            if (FarAwayList == -1) {
                UpdateNearbyList();
            } else {
                UpdateFarAwayList();
                UpdateNearbyList();
            }
        } else {
            UpdateNearbyList();
            UpdateFarAwayList();
        }
    }

    // 0x7203FC - Process interior occluders
    for (size_t i{}; i < NumInteriorOccludersOnMap; i++) {
        if (NumActiveOccluders > ActiveOccluders.size()) {
            break;
        }
        if (InteriorOccluders[i].ProcessOneOccluder(&ActiveOccluders[NumActiveOccluders])) {
            NumActiveOccluders++;
        }
    }

    // 0x72043A - Remove occluded occluders
    const auto [b, e] = rng::remove_if(GetActiveOccluders(), [](CActiveOccluder& o) {
        return rng::any_of(GetActiveOccluders(), [&](CActiveOccluder& i) {
            return &o != &i && i.m_DistToCam < o.m_DistToCam && OccluderHidesBehind(&o, &i);
        });
    });
    NumActiveOccluders -= (size_t)(std::distance(b, e));
}
