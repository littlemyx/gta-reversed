#include "StdInc.h"
#include "Curves.h"

void CCurves::InjectHooks() {
    RH_ScopedClass(CCurves);
    RH_ScopedCategoryGlobal();

    RH_ScopedGlobalInstall(TestCurves, 0x43C600, {.Locked =true});
    RH_ScopedGlobalInstall(DistForLineToCrossOtherLine, 0x43C610);
    RH_ScopedGlobalInstall(CalcSpeedVariationInBend, 0x43C660);
    RH_ScopedGlobalInstall(CalcSpeedScaleFactor, 0x43C710);
    RH_ScopedGlobalInstall(CalcCorrectedDist, 0x43C880);
    RH_ScopedGlobalInstall(CalcCurvePoint, 0x43C900);
}

// 0x43C610
float CCurves::DistForLineToCrossOtherLine(float lineBaseX, float lineBaseY, float lineDirX, float lineDirY, float otherLineBaseX, float otherLineBaseY, float otherLineDirX, float otherLineDirY) {
    // All intermediates stay on the x87 stack (extended precision) until the return value
    const double dir = (double)lineDirX * otherLineDirY - (double)lineDirY * otherLineDirX;

    if (dir == 0.0) {
        return -1.0f; // Lines are parallel, no intersection
    }

    const double dist = ((double)lineBaseX - otherLineBaseX) * otherLineDirY - ((double)lineBaseY - otherLineBaseY) * otherLineDirX;

    // 0x43C652: `fld -1.0; fdiv dir; fmulp` => dist * (-1 / dir), NOT -dist / dir
    return (float)(dist * (-1.0 / dir));
}

// 0x8594EC: the exe's `1/3` here is 0x3EAAAAAA (truncated, NOT the rounded 0x3EAAAAAB)
constexpr float CURVE_THIRD = std::bit_cast<float>(0x3EAAAAAAu);

// 0x43C660
float CCurves::CalcSpeedVariationInBend(
    const CVector& startCoors,
    const CVector& endCoors,
    float          startDirX,
    float          startDirY,
    float          endDirX,
    float          endDirY
) {
    float returnVal = 0.0f;
    float dotProduct = startDirX * endDirX + startDirY * endDirY;

    if (dotProduct <= 0.0f) {
        // If the dot product is <= 0, return a constant value (1/3)
        returnVal = CURVE_THIRD;
        return returnVal;
    }

    if (dotProduct > 0.7f) {
        // Calculate the distance from the start point to the mathematical line defined by the end point and direction
        float distToLine = CCollision::DistToMathematicalLine2D(endCoors.x, endCoors.y, endDirX, endDirY, startCoors.x, startCoors.y);

        // Calculate the straight-line distance between the start and end points
        float straightDist = (startCoors - endCoors).Magnitude2D();

        // Normalize the distance to the line by the straight-line distance
        returnVal = (distToLine / straightDist) * CURVE_THIRD;
        return returnVal;
    }

    // If the dot product is <= 0.7, interpolate the return value
    returnVal = (1.0f - (dotProduct * ExeRecip(0.7f))) * CURVE_THIRD;

    return returnVal;
}

// 0x43C710
float CCurves::CalcSpeedScaleFactor(
    const CVector& startCoors,
    const CVector& endCoors,
    float          startDirX,
    float          startDirY,
    float          endDirX,
    float          endDirY
) {
    float speedVariation = CalcSpeedVariationInBend(startCoors, endCoors, startDirX, startDirY, endDirX, endDirY);

    float distToPoint1 = DistForLineToCrossOtherLine(
        startCoors.x, startCoors.y, startDirX, startDirY, endCoors.x, endCoors.y, endDirX, endDirY
    );

    float distToPoint2 = DistForLineToCrossOtherLine(
        endCoors.x, endCoors.y, -endDirX, -endDirY, startCoors.x, startCoors.y, startDirX, startDirY
    );

    float straightDist2, straightDist1, bendDist, bendDist_Time, bendDistOneSegment, totalDist_Time;

    if (distToPoint1 > 0.0f && distToPoint2 > 0.0f) {
        bendDistOneSegment = std::min(distToPoint1, distToPoint2);
        bendDistOneSegment = std::min(5.0f, bendDistOneSegment);

        straightDist1 = distToPoint1 - bendDistOneSegment;
        straightDist2 = distToPoint2 - bendDistOneSegment;

        bendDist = 2.0f * bendDistOneSegment;

        totalDist_Time = bendDist;
    } else {
        bendDist = (startCoors - endCoors).Magnitude2D();
        bendDist_Time = 1.0f - speedVariation;

        straightDist1 = 0.0f;
        straightDist2 = 0.0f;

        totalDist_Time = bendDist / bendDist_Time;
    }

    // 0x43C834..0x43C83A: (2 * bendDistOneSegment + (distToPoint2 - bendDistOneSegment)) + (distToPoint1 - bendDistOneSegment): the second leg is added first
    return totalDist_Time + straightDist2 + straightDist1;
}

// 0x43C880
float CCurves::CalcCorrectedDist(float current, float total, float speedVariation, float* outT) {
    // Epsilon to avoid division by zero
    if (total < 0.00001f) {
        *outT = 0.5f;
        return 0.0f;
    }

    // 0x43C880: everything stays on the x87 stack (extended precision) except `*outT`; the constants are the exe's float literals
    // (0x40490FDB = pi, 0x40C90FDB = 2pi, 0x3E22F983 = the reciprocal of 2pi folded by the compiler: NOT `1.0f / TWO_PI` evaluated in extended)
    const double t = (double)current / (double)total;
    *outT = (float)(0.5 - std::cos((double)std::numbers::pi_v<float> * t) * 0.5);

    const double averageSpeed = std::sin(t * (double)std::bit_cast<float>(0x40C90FDBu));
    const double sv = speedVariation;
    const double corrected = averageSpeed * ((double)total * (double)std::bit_cast<float>(0x3E22F983u)) * sv
                           + ((1.0 - (sv + sv) + 1.0) * 0.5) * (double)current;
    return (float)corrected;
}

// 0x43C900
// Written from the asm: the x87 stack keeps several intermediates unrounded (modelled with double), the rest is spilled to float. Differences to the old port:
// NaN `time` -> 0, the `d <= 0` tests are `!(d > 0)`, `bendDist + 2m <= bdt` is `!(.. > bdt)`, sums add (2m + s2) + s1, the bend/end lerps are `from*(1-t) + to*t`
// with `(dir * f) * m` association, and the speed is `lerp(dir) * S / (ms * 0.001f)` with S the plain total length (NO (1 - time) factor).
void CCurves::CalcCurvePoint(const CVector& startCoors, const CVector& endCoors, const CVector& startDir, const CVector& endDir, float time, int32 traversalTimeInMS, CVector& resultCoor, CVector& resultSpeed) {
    // 0x43C907: `time > 0` ? (`time < 1` ? time : 1) : 0  (NaN -> 0)
    time = (time > 0.0f) ? ((time < 1.0f) ? time : 1.0f) : 0.0f;

    const float speedVariation = CalcSpeedVariationInBend(startCoors, endCoors, startDir.x, startDir.y, endDir.x, endDir.y);

    // Find where the ray from start position would intersect with end ray
    const float distToPoint1 = DistForLineToCrossOtherLine(
        startCoors.x, startCoors.y, startDir.x, startDir.y, endCoors.x, endCoors.y, endDir.x, endDir.y
    );

    // Find where the ray from end position would intersect with start ray (negative because direction is flipped)
    const float distToPoint2 = DistForLineToCrossOtherLine(
        endCoors.x, endCoors.y, -endDir.x, -endDir.y, startCoors.x, startCoors.y, startDir.x, startDir.y
    );

    double x87Total; // `X + Z + [0x50]` of the tail (0x43CDFB): the total length, unrounded

    if (!(distToPoint1 > 0.0f) || !(distToPoint2 > 0.0f)) { // 0x43CA10 / 0x43CA21 (NaN included)
        const float straightDist = (startCoors - endCoors).Magnitude2D();
        const float interpol     = (float)((double)straightDist / (1.0 - (double)speedVariation)); // fdivr, spilled

        float ourTime;
        const double corrected = CalcCorrectedDist((float)((double)interpol * time), interpol, speedVariation, &ourTime); // x87 return value, `current` spilled

        const CVector coorsOnLine1 = startCoors + startDir * (float)corrected;
        const CVector coorsOnLine2 = endCoors + endDir * (float)(corrected - straightDist);

        // from * (1 - t) + to * t
        const float nt = 1.0f - ourTime;
        resultCoor.x = coorsOnLine2.x * ourTime + coorsOnLine1.x * nt;
        resultCoor.y = coorsOnLine2.y * ourTime + coorsOnLine1.y * nt;
        resultCoor.z = coorsOnLine2.z * ourTime + coorsOnLine1.z * nt;

        x87Total = ((double)straightDist + 0.0) + 0.0;
    } else {
        // 0x43CA27..: m = min(d1, d2) (`d1 < d2 ? d1 : d2`), then min(5, m) (`5 < m ? 5 : m`)
        float m = (distToPoint1 < distToPoint2) ? distToPoint1 : distToPoint2;
        m = (5.0f < m) ? 5.0f : m;

        const float  s1 = distToPoint1 - m;            // [0x50], spilled
        const double s2 = (double)distToPoint2 - m;    // left on the x87 stack unrounded
        const float  twoM = m + m;                     // [0x48]

        const double totalExt = ((double)twoM + s2) + s1;
        const float  total    = (float)totalExt;       // [0x10]
        const float  bdt      = (float)(totalExt * time); // [0x4C]: the unrounded total times time

        if (s1 > bdt) { // fcomp [0x50] / jp
            resultCoor = startCoors + startDir * bdt;
        } else if (!((twoM + s1) > bdt)) { // 0x43CAE4: `jne` also takes the unordered case
            resultCoor = endCoors + endDir * (bdt - total);
        } else {
            const double bi = ((double)bdt - s1) / twoM; // unrounded

            // start side: P = startCoors + startDir * s1 (x spilled, y unrounded, z product spilled)
            const float  Px = (float)((double)startDir.x * s1 + startCoors.x);
            const double Py = (double)startDir.y * s1 + startCoors.y;
            const double Pz = (double)(float)(startDir.z * s1) + startCoors.z;
            // end side: Q = endCoors - endDir * s2 (all spilled to float)
            const float Qx = (float)(endCoors.x - s2 * endDir.x);
            const float Qy = (float)(endCoors.y - s2 * endDir.y);
            const float Qz = (float)(endCoors.z - (double)(float)(s2 * endDir.z));
            // bendStart = P + (startDir * bi) * m
            const float  rx = (float)(((double)startDir.x * bi) * m);
            const double ry = ((double)startDir.y * bi) * m;
            const float  rz = (float)(((double)(float)(startDir.z * bi)) * m);
            const float  bsx = (float)((double)rx + Px);
            const float  bsy = (float)(ry + Py);
            const double bsz = (double)rz + Pz;
            // bendEnd = Q - (endDir * (1 - bi)) * m
            const double nbi = 1.0 - bi;
            const double exm = (nbi * endDir.x) * m;
            const float  ey  = (float)(nbi * endDir.y);
            const float  ez  = (float)(nbi * endDir.z);
            const float  bex = (float)(Qx - exm);
            const float  bey = (float)(Qy - (double)ey * m);
            const float  bez = (float)(Qz - (double)(float)((double)ez * m));
            // Lerp(bendStart, bendEnd, bi) = bendStart * (1 - bi) + bendEnd * bi
            const float  ax = (float)(bsx * nbi), ay = (float)(bsy * nbi);
            const double az = bsz * nbi;
            const float  cz = (float)(bez * bi);
            resultCoor.x = (float)(bex * bi + ax);
            resultCoor.y = (float)(bey * bi + ay);
            resultCoor.z = (float)((double)cz + az);
        }

        x87Total = ((double)twoM + s2) + s1;
    }

    // 0x43CDFB: speed = lerp(startDir, endDir, time) * total / (ms * 0.001f)
    const double A = 1.0 - (double)time;
    const double M = (double)traversalTimeInMS * (double)std::bit_cast<float>(0x3A83126Fu);
    resultSpeed.x = (float)((((double)time * endDir.x + A * startDir.x) * x87Total) / M);
    resultSpeed.z = 0.0f;
    resultSpeed.y = (float)((((double)time * endDir.y + A * startDir.y) * x87Total) / M);
}

// unused
// 0x43C600
void CCurves::TestCurves() {
    // https://github.com/ifarbod/sa-curves-test

#define epsilon 0.01f
#define FLOAT_EQUAL(a, b) (fabs((a) - (b)) < (epsilon))

    auto DistForLineToCrossOtherLine_test = []
        {
            // Test case 1: Lines intersect
            {
                float result = DistForLineToCrossOtherLine(0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f);

                // 0.5
                assert(FLOAT_EQUAL(result, 0.5f));  // Expected distance to crossing
            }

            // Test case 2: Lines are parallel (no intersection)
            {
                float result = DistForLineToCrossOtherLine(0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);

                // -1
                assert(FLOAT_EQUAL(result, -1.0f));  // Expected -1 for parallel lines
            }

            // Test case 3: Lines intersect at a point
            {
                float result = DistForLineToCrossOtherLine(0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 4.0f, 2.0f, -2.0f);

                // 1.0
                assert(FLOAT_EQUAL(result, 1.0f));  // Expected distance to crossing
            }

            // Test case 4: Lines are coincident (infinite intersections)
            {
                float result = DistForLineToCrossOtherLine(0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f, 2.0f);

                // -1
                assert(FLOAT_EQUAL(result, -1.0f));  // Expected -1 for coincident lines
            }

            // Test case 5: Lines with large numbers
            {
                float result = DistForLineToCrossOtherLine(2500.5f, 1500.0f, 3.5f, 2.5f, 3000.0f, 2000.0f, -4.0f, 3.0f);

                // Should still give a reasonable intersection distance
                assert(FLOAT_EQUAL(result, 170.658539f));  // Expected distance to crossing
            }
        };

    auto CalcSpeedVariationInBend_test = []
        {
            // Test Case 1: Dot product <= 0 (opposite directions)
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 0.0f, 0.0f);
                float speedVariation =
                    CalcSpeedVariationInBend(startCoors, endCoors, 1.0f, 0.0f, -1.0f, 0.0f);  // Opposite directions
                assert(FLOAT_EQUAL(speedVariation, 1.0f / 3.0f) && "Test Case 1 Failed: Expected 0.33333.");
            }

            // Test Case 2: Dot product <= 0 (perpendicular directions)
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 1.0f, 0.0f);
                float speedVariation =
                    CalcSpeedVariationInBend(startCoors, endCoors, 1.0f, 0.0f, 0.0f, 1.0f);  // Perpendicular directions
                assert(FLOAT_EQUAL(speedVariation, 1.0f / 3.0f) && "Test Case 2 Failed: Expected 0.33333.");
            }

            // Test Case 3: Dot product <= 0.7 (small angle)
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 0.0f, 0.0f);
                float speedVariation = CalcSpeedVariationInBend(startCoors, endCoors, 1.0f, 0.0f, 0.9f, 0.1f);  // Small angle
                assert(FLOAT_EQUAL(speedVariation, 0.145296633f) &&
                    "Test Case 3 Failed: Expected value between 0 and 0.33333.");
            }

            // Test Case 4: Dot product > 0.7 (larger angle)
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 1.0f, 0.0f);
                float speedVariation =
                    CalcSpeedVariationInBend(startCoors, endCoors, 1.0f, 0.0f, 0.8f, 0.2f);  // Larger angle
                assert(FLOAT_EQUAL(speedVariation, 0.235702246f) &&
                    "Test Case 4 Failed: Expected value between 0 and 0.33333.");
            }

            // Test Case 5: Dot product = 0.7 (boundary case)
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 0.0f, 0.0f);
                float speedVariation =
                    CalcSpeedVariationInBend(startCoors, endCoors, 1.0f, 0.0f, 0.7f, 0.7141428f);  // Dot product = 0.7
                assert(FLOAT_EQUAL(speedVariation, 0.0f) && "Test Case 5 Failed: Expected 0.0.");
            }

            // Test Case 6: Dot product > 0.7 (sharp bend)
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 0.0f, 0.0f);
                float speedVariation = CalcSpeedVariationInBend(startCoors, endCoors, 1.0f, 0.0f, 0.6f, 0.8f);  // Sharp bend
                assert(
                    FLOAT_EQUAL(speedVariation, 0.047619f) && "Test Case 6 Failed: Expected value between 0 and 0.33333.");
            }

            // Test Case 7: Dot product = 1 (same direction)
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 0.0f, 0.0f);
                float speedVariation =
                    CalcSpeedVariationInBend(startCoors, endCoors, 1.0f, 0.0f, 1.0f, 0.0f);  // Same direction
                assert(speedVariation == 0.0f && "Test Case 7 Failed: Expected 0.0.");
            }

            // Test Case 8: Dot product > 0.7 (perpendicular distance calculation)
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 1.0f, 0.0f);
                float speedVariation =
                    CalcSpeedVariationInBend(startCoors, endCoors, 1.0f, 0.0f, 0.0f, 1.0f);  // Perpendicular directions
                assert(speedVariation > 0.0f && speedVariation <= (1.0f / 3.0f) &&
                    "Test Case 8 Failed: Expected value between 0 and 0.33333.");
            }

            // Test Case 9: Dot product > 0.7 (large perpendicular distance)
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(10.0f, 0.0f, 0.0f);
                float speedVariation =
                    CalcSpeedVariationInBend(startCoors, endCoors, 1.0f, 0.0f, 0.0f, 1.0f);  // Large perpendicular distance
                assert(speedVariation > 0.0f && speedVariation <= (1.0f / 3.0f) &&
                    "Test Case 9 Failed: Expected value between 0 and 0.33333.");
            }

            // Test Case 10: Dot product > 0.7 (small perpendicular distance)
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 0.1f, 0.0f);
                float speedVariation =
                    CalcSpeedVariationInBend(startCoors, endCoors, 1.0f, 0.0f, 0.0f, 1.0f);  // Small perpendicular distance
                assert(speedVariation > 0.0f && speedVariation <= (1.0f / 3.0f) &&
                    "Test Case 10 Failed: Expected value between 0 and 0.33333.");
            }
        };

    auto CalcSpeedScaleFactor_test = []
        {
            // Test Case 1: CalcSpeedScaleFactor - Simple Bend
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 1.0f, 0.0f);
                float speedScaleFactor = CalcSpeedScaleFactor(startCoors, endCoors, 1.0f, 0.0f, 0.0f, 1.0f);

                // 2.0
                assert(FLOAT_EQUAL(speedScaleFactor, 2.0f) && "Test Case Failed: Incorrect speed scale factor.");
            }

            // Test Case 2: CalcSpeedScaleFactor - Straight Line
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 0.0f, 0.0f);
                float speedScaleFactor = CalcSpeedScaleFactor(startCoors, endCoors, 1.0f, 0.0f, 1.0f, 0.0f);

                // 1.0
                assert(FLOAT_EQUAL(speedScaleFactor, 1.0f) && "Test Case Failed: Incorrect speed scale factor.");
            }

            // Test Case 3: CalcSpeedScaleFactor - Sharp Bend
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 0.0f, 0.0f);
                float speedScaleFactor = CalcSpeedScaleFactor(startCoors, endCoors, 1.0f, 0.0f, 0.0f, 1.0f);

                // 1.5
                assert(FLOAT_EQUAL(speedScaleFactor, 1.5f) &&
                    "Test Case Failed: Speed scale factor should be greater than 1.0.");
            }

            // Test Case 4: CalcSpeedScaleFactor - Parallel Lines
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 0.0f, 0.0f);
                float speedScaleFactor = CalcSpeedScaleFactor(startCoors, endCoors, 1.0f, 0.0f, 1.0f, 0.0f);

                // 1.0
                assert(FLOAT_EQUAL(speedScaleFactor, 1.0f) && "Test Case Failed: Incorrect speed scale factor.");
            }

            // Test Case 5: CalcSpeedScaleFactor - Coincident Lines
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 1.0f, 0.0f);
                float speedScaleFactor = CalcSpeedScaleFactor(startCoors, endCoors, 1.0f, 1.0f, 1.0f, 1.0f);

                // ~1.41
                assert(FLOAT_EQUAL(speedScaleFactor, 1.4142135f) && "Test Case Failed: Incorrect speed scale factor.");
            }

            // Test Case 6: CalcSpeedScaleFactor - Large Values
            {
                CVector startCoors(2500.0f, 1500.0f, 0.0f);
                CVector endCoors(3500.0f, 2000.0f, 0.0f);
                float speedScaleFactor = CalcSpeedScaleFactor(startCoors, endCoors, 2.0f, 1.0f, 3.0f, 2.0f);

                // Should be a reasonable scale factor despite large coordinates
                assert(FLOAT_EQUAL(speedScaleFactor, 1118.03394f) &&
                    "Test Case Failed: Speed scale factor out of reasonable range for large values.");
            }
        };

    auto CalcCurvePoint_test = []
        {
            // Test Case 1: CalcCurvePoint - Straight Line
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 0.0f, 0.0f);
                CVector startDir(1.0f, 0.0f, 0.0f);
                CVector endDir(1.0f, 0.0f, 0.0f);
                CVector resultCoor;
                CVector resultSpeed;
                CalcCurvePoint(startCoors, endCoors, startDir, endDir, 0.5f, 1000, resultCoor, resultSpeed);

                assert(FLOAT_EQUAL(resultCoor.x, 0.5f) && FLOAT_EQUAL(resultCoor.y, 0.0f) &&
                    FLOAT_EQUAL(resultCoor.z, 0.0f) && "Test Case 1 Failed: Incorrect curve point.");
            }

            // Test Case 2: CalcCurvePoint - Curve with Bend
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 1.0f, 0.0f);
                CVector startDir(1.0f, 0.0f, 0.0f);
                CVector endDir(0.0f, 1.0f, 0.0f);
                CVector resultCoor;
                CVector resultSpeed;
                CalcCurvePoint(startCoors, endCoors, startDir, endDir, 0.5f, 1000, resultCoor, resultSpeed);

                // Expected result depends on the interpolation logic
                assert(resultCoor.x > 0.0f && resultCoor.x < 1.0f && resultCoor.y > 0.0f && resultCoor.y < 1.0f &&
                    FLOAT_EQUAL(resultCoor.z, 0.0f) && "Test Case 2 Failed: Incorrect curve point.");
            }

            // Test Case 3: CalcCurvePoint - Large Values
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1000.0f, 1000.0f, 0.0f);
                CVector startDir(1.0f, 0.0f, 0.0f);
                CVector endDir(0.0f, 1.0f, 0.0f);
                CVector resultCoor;
                CVector resultSpeed;
                CalcCurvePoint(startCoors, endCoors, startDir, endDir, 0.5f, 1000, resultCoor, resultSpeed);

                // Expected result depends on the interpolation logic
                assert(resultCoor.x > 0.0f && resultCoor.x < 1000.0f && resultCoor.y > 0.0f && resultCoor.y < 1000.0f &&
                    FLOAT_EQUAL(resultCoor.z, 0.0f) && "Test Case 3 Failed: Incorrect curve point.");
            }

            // Test Case 4: CalcCurvePoint - time = 0.0 (Start Point)
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 1.0f, 0.0f);
                CVector startDir(1.0f, 0.0f, 0.0f);
                CVector endDir(0.0f, 1.0f, 0.0f);
                CVector resultCoor;
                CVector resultSpeed;
                CalcCurvePoint(startCoors, endCoors, startDir, endDir, 0.0f, 1000, resultCoor, resultSpeed);

                assert(FLOAT_EQUAL(resultCoor.x, 0.0f) && FLOAT_EQUAL(resultCoor.y, 0.0f) &&
                    FLOAT_EQUAL(resultCoor.z, 0.0f) && "Test Case 4 Failed: Incorrect curve point.");
            }

            // Test Case 5: CalcCurvePoint - time = 1.0 (End Point)
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 1.0f, 0.0f);
                CVector startDir(1.0f, 0.0f, 0.0f);
                CVector endDir(0.0f, 1.0f, 0.0f);
                CVector resultCoor;
                CVector resultSpeed;
                CalcCurvePoint(startCoors, endCoors, startDir, endDir, 1.0f, 1000, resultCoor, resultSpeed);

                assert(FLOAT_EQUAL(resultCoor.x, 1.0f) && FLOAT_EQUAL(resultCoor.y, 1.0f) &&
                    FLOAT_EQUAL(resultCoor.z, 0.0f) && "Test Case 5 Failed: Incorrect curve point.");
            }

            // Test Case 6: CalcCurvePoint - Z-Axis Movement
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(0.0f, 0.0f, 1.0f);
                CVector startDir(0.0f, 0.0f, 1.0f);
                CVector endDir(0.0f, 0.0f, 1.0f);
                CVector resultCoor;
                CVector resultSpeed;
                CalcCurvePoint(startCoors, endCoors, startDir, endDir, 0.5f, 1000, resultCoor, resultSpeed);

                assert(FLOAT_EQUAL(resultCoor.x, 0.0f) && FLOAT_EQUAL(resultCoor.y, 0.0f) &&
                    FLOAT_EQUAL(resultCoor.z, 0.5f) && "Test Case 6 Failed: Incorrect curve point.");
            }

            // Test Case 7: CalcCurvePoint - Sharp Bend
            {
                CVector startCoors(0.0f, 0.0f, 0.0f);
                CVector endCoors(1.0f, 0.0f, 0.0f);
                CVector startDir(1.0f, 0.0f, 0.0f);
                CVector endDir(-1.0f, 0.0f, 0.0f);
                CVector resultCoor;
                CVector resultSpeed;
                CalcCurvePoint(startCoors, endCoors, startDir, endDir, 0.5f, 1000, resultCoor, resultSpeed);

                // Expected result depends on the interpolation logic
                assert(FLOAT_EQUAL(resultCoor.x, 1.0f) && FLOAT_EQUAL(resultCoor.y, 0.0f) &&
                    FLOAT_EQUAL(resultCoor.z, 0.0f) && "Test Case 7 Failed: Incorrect curve point.");
            }
        };

    auto CalcCorrectedDist_test = []
        {
            // Test Case 1: CalcCorrectedDist - Simple Case
            {
                // interpol please don't arrest us, we have done nothing!
                float interpol = 0.0f;
                float correctedDist = CalcCorrectedDist(0.5f, 1.0f, 0.5f, &interpol);

                assert(FLOAT_EQUAL(correctedDist, 0.25f) && FLOAT_EQUAL(interpol, 0.5f) &&
                    "Test Case 1 Failed: Corrected distance out of range.");
            }
            // Test Case 2: CalcCorrectedDist - Start of Curve (time = 0.0)
            {
                float interpol = 0.0f;
                float correctedDist = CalcCorrectedDist(0.0f, 1.0f, 0.5f, &interpol);

                assert(FLOAT_EQUAL(correctedDist, 0.0f) && FLOAT_EQUAL(interpol, 0.0f) &&
                    "Test Case 2 Failed: Corrected distance should be 0.0 at the start.");
            }

            // Test Case 3: CalcCorrectedDist - End of Curve (time = 1.0)
            {
                float interpol = 0.0f;
                float correctedDist = CalcCorrectedDist(1.0f, 1.0f, 0.5f, &interpol);

                assert(FLOAT_EQUAL(correctedDist, 0.5f) && FLOAT_EQUAL(interpol, 1.0f) &&
                    "Test Case 3 Failed: Corrected distance should be 1.0 at the end.");
            }

            // Test Case 4: CalcCorrectedDist - No Speed Variation (SpeedVariation = 0.0)
            {
                float interpol = 0.0f;
                float correctedDist = CalcCorrectedDist(0.5f, 1.0f, 0.0f, &interpol);

                assert(FLOAT_EQUAL(correctedDist, 0.5f) && FLOAT_EQUAL(interpol, 0.5f) &&
                    "Test Case 4 Failed: Corrected distance should match input when SpeedVariation is 0.0.");
            }

            // Test Case 5: CalcCorrectedDist - Maximum Speed Variation (SpeedVariation = 1.0)
            {
                float interpol = 0.0f;
                float correctedDist = CalcCorrectedDist(0.5f, 1.0f, 1.0f, &interpol);

                assert(FLOAT_EQUAL(correctedDist, 0.0f) && FLOAT_EQUAL(interpol, 0.5f) &&
                    "Test Case 5 Failed: Corrected distance should be 0.0 when SpeedVariation is 1.0.");
            }

            // Test Case 6: CalcCorrectedDist - Large Total Distance
            {
                float interpol = 0.0f;
                float correctedDist = CalcCorrectedDist(500.0f, 1000.0f, 0.5f, &interpol);

                assert(FLOAT_EQUAL(correctedDist, 250.0f) && FLOAT_EQUAL(interpol, 0.5f) &&
                    "Test Case 6 Failed: Corrected distance should scale with large Total distance.");
            }

            // Test Case 7: CalcCorrectedDist - Small Total Distance
            {
                float interpol = 0.0f;
                float correctedDist = CalcCorrectedDist(0.1f, 0.2f, 0.5f, &interpol);

                assert(FLOAT_EQUAL(correctedDist, 0.05f) && FLOAT_EQUAL(interpol, 0.5f) &&
                    "Test Case 7 Failed: Corrected distance should scale with small Total distance.");
            }

            // Test Case 8: CalcCorrectedDist - Negative Current Distance
            {
                float interpol = 0.0f;
                float correctedDist = CalcCorrectedDist(-0.5f, 1.0f, 0.5f, &interpol);

                assert(FLOAT_EQUAL(correctedDist, -0.25f) && FLOAT_EQUAL(interpol, 0.5f) &&
                    "Test Case 8 Failed: Corrected distance should handle negative Current distance.");
            }

            // Test Case 9: CalcCorrectedDist - Negative Total Distance
            {
                float interpol = 0.0f;
                float correctedDist = CalcCorrectedDist(0.5f, -1.0f, 0.5f, &interpol);

                assert(FLOAT_EQUAL(correctedDist, 0.0f) && FLOAT_EQUAL(interpol, 0.5f) &&
                    "Test Case 9 Failed: Corrected distance should handle negative Total distance.");
            }

            // Test Case 10: CalcCorrectedDist - SpeedVariation > 1.0
            {
                float interpol = 0.0f;
                float correctedDist = CalcCorrectedDist(0.5f, 1.0f, 1.5f, &interpol);

                assert(FLOAT_EQUAL(correctedDist, -0.25f) && FLOAT_EQUAL(interpol, 0.5f) &&
                    "Test Case 10 Failed: Corrected distance should handle SpeedVariation > 1.0.");
            }
        };

    DistForLineToCrossOtherLine_test();
    CalcSpeedVariationInBend_test();
    CalcSpeedScaleFactor_test();
    CalcCurvePoint_test();
    CalcCorrectedDist_test();
}
