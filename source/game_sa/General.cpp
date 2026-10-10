/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include <random>

static std::random_device randomDevice;
static std::mt19937 randomEngine(randomDevice());

// 0x407180 - 2-arg wrapper to allow hooking the original function while the template has an extra `inclusive` arg
static int32 GetRandomNumberInRange_int32(int32 min, int32 max) {
    return CGeneral::GetRandomNumberInRange(min, max);
}

void CGeneral::InjectHooks() {
    RH_ScopedNamespace(CGeneral);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(LimitAngle, 0x53CB00);
    RH_ScopedInstall(LimitRadianAngle, 0x53CB50);
    RH_ScopedOverloadedInstall(GetRadianAngleBetweenPoints, "", 0x53CBE0, float(*)(float, float, float, float));
    RH_ScopedInstall(GetATanOfXY, 0x53CC70);
    RH_ScopedInstall(GetNodeHeadingFromVector, 0x53CDC0);
    RH_ScopedInstall(SolveQuadratic, 0x53CE30);
    RH_ScopedInstall(GetAngleBetweenPoints, 0x53CEA0);
    RH_ScopedGlobalInstall(GetRandomNumberInRange_int32, 0x407180);
    RH_ScopedOverloadedInstall(GetRandomNumberInRange<float>, "", 0x41BD90, float (*)(float, float));
}

namespace {
// The float constants the original uses (.rdata), widened exactly
constexpr double K_HALF_PI     = (double)1.5707963705062866f; // 0x858FE4 (0x3FC90FDB), -K_HALF_PI at 0x859998
constexpr double K_PI          = (double)3.1415927410125732f; // 0x858CB8 (0x40490FDB), -K_PI at 0x858CC0
constexpr double K_TWO_PI      = (double)6.2831854820251465f; // 0x858CBC (0x40C90FDB)
constexpr double K_ONE_HALF_PI = (double)4.71238899230957f;   // 0x863AC4 (0x4096CBE4)

// `fpatan` with ST0 = 1: atan2(v, 1)
inline double AtanExt(double v) {
    return x87::atan2(v, 1.0);
}
}

// 0x53CB00
float CGeneral::LimitAngle(float angle) {
    return (float)LimitAngleExt(angle);
}

double CGeneral::LimitAngleExt(float angle) {
    double result = angle;
    if (!(result < 180.0f) && result == result) { // 0x85A994; FCOM + JNE on C0: NaN counts as "less" and skips
        do {
            result -= 360.0f; // 0x859E2C
        } while (!(result < 180.0f));
    }
    while (result < -180.0f) { // 0x863ABC
        result += 360.0f;
    }
    return result;
}

// 0x53CB50
float CGeneral::LimitRadianAngle(float angle) {
    return (float)LimitRadianAngleExt(angle);
}

double CGeneral::LimitRadianAngleExt(float angle) {
    double result = angle;
    if (-25.0f > result) { // 0x863AC0; FCOMP: NaN / equal / greater fall to the next check
        result = -25.0;
        do {
            result += K_TWO_PI; // do-while: the add happens before the first check
        } while (result < -K_PI);
        return result;
    }

    bool subtract = false;
    if (25.0f < result) { // 0x858FE8
        result   = 25.0;
        subtract = true; // The first subtraction happens unconditionally
    } else if (!(result < K_PI) && result == result) { // FCOM: NaN counts as "less"
        subtract = true;
    }
    if (subtract) {
        do {
            result -= K_TWO_PI;
        } while (!(result < K_PI));
    }

    if (result < -K_PI) {
        do {
            result += K_TWO_PI;
        } while (result < -K_PI);
    }
    return result;
}

// 0x53CBE0
float CGeneral::GetRadianAngleBetweenPoints(float x1, float y1, float x2, float y2) {
    return (float)GetRadianAngleBetweenPointsExt(x1, y1, x2, y2);
}

double CGeneral::GetRadianAngleBetweenPointsExt(float x1, float y1, float x2, float y2) {
    const double x = (double)x2 - (double)x1;
    double       y = (double)y2 - (double)y1;
    if (y == 0.0) { // FCOM + JP: NaN keeps its value
        y = (double)0.0001f; // 0x858FC4
    }
    const double at = AtanExt(x / y);
    if (x > 0.0) {
        return y > 0.0
            ? (K_HALF_PI - at) + K_HALF_PI
            : K_HALF_PI - (at + K_HALF_PI);
    }
    return y > 0.0
        ? -K_HALF_PI - (at + K_HALF_PI) // 0x859998
        : (K_HALF_PI - at) - K_HALF_PI;
}

// 0x53CC70
float CGeneral::GetATanOfXY(float x, float y) {
    return (float)GetATanOfXYExt(x, y);
}

double CGeneral::GetATanOfXYExt(float x, float y) {
    if (x == 0.0f && y == 0.0f) {
        return 0.0;
    }
    const float  xabs = x < 0.0f ? -x : x; // FCOMP + JP + FCHS: NaN stays NaN
    const float  yabs = y < 0.0f ? -y : y;
    const double dx = x, dy = y;
    if (xabs < yabs) {
        if (y > 0.0f) {
            return x > 0.0f
                ? K_HALF_PI - AtanExt(dx / dy)
                : AtanExt((-1.0 / dy) * dx) + K_HALF_PI; // 0x858C1C = -1.0
        }
        return x > 0.0f
            ? AtanExt((-1.0 / dy) * dx) + K_ONE_HALF_PI
            : K_ONE_HALF_PI - AtanExt(dx / dy);
    }
    if (y > 0.0f) {
        return x > 0.0f
            ? AtanExt(dy / dx)
            : K_PI - AtanExt((-1.0 / dx) * dy);
    }
    return x > 0.0f
        ? K_TWO_PI - AtanExt((-1.0 / dx) * dy)
        : AtanExt(dy / dx) + K_PI;
}

// 0x53CDC0
uint32 CGeneral::GetNodeHeadingFromVector(float x, float y) {
    double angle = GetRadianAngleBetweenPointsExt(x, y, 0.0f, 0.0f);
    if (angle < 0.0) {
        angle += K_TWO_PI;
    }
    angle = K_TWO_PI - angle + (double)0.39269909f; // 0x859F50 (22.5 deg)
    if (!(angle < K_TWO_PI) && angle == angle) { // FCOM + JNE on C0: NaN skips the subtraction
        angle -= K_TWO_PI;
    }
    // 0x8594F0 (1 / 2pi), 0x859000 (8.0); floor (0x8219F0) + _ftol2 (0x821B40): `fistp qword`, only the LOW dword is returned (NaN / inf / out of range => the
    // indefinite 0x8000000000000000 => 0, big values wrap), NOT the saturating 0x80000000 of a 32-bit conversion
    return (uint32)(uint64)(int64)floor(angle * (double)0.15915494f * 8.0);
}

/*!
* Solves: ax² + bx + c = 0
* @returns If there is a solution for the equation
* @addr 0x53CE30
*/
bool CGeneral::SolveQuadratic(float a, float b, float c, float& x1, float& x2) {
    // x12 = (-b ± √(b²-4ac)) / 2a, in the exe's x87 form (0x53CE30): disc = b*b - (a*c)*4, recip = 1 / a, x = ((±-b ± s) * recip) * 0.5
    const double discr = (double)b * b - ((double)a * c) * 4.0f;
    if (discr < 0.0f) { // `fcom; test ah, 5; jp`: NaN continues
        return false; // No solution
    }

    const double s     = std::sqrt(discr);
    const double recip = 1.0 / (double)a;
    x2 = (float)(((s - b) * recip) * 0.5f);
    x1 = (float)(((-(double)b - s) * recip) * 0.5f);
    return true;
}

// 0x53CEA0
float CGeneral::GetAngleBetweenPoints(float x1, float y1, float x2, float y2) {
    return (float)GetAngleBetweenPointsExt(x1, y1, x2, y2);
}

double CGeneral::GetAngleBetweenPointsExt(float x1, float y1, float x2, float y2) {
    return GetRadianAngleBetweenPointsExt(x1, y1, x2, y2) * (double)57.2957764f; // 0x859878
}

uint16 CGeneral::GetRandomNumber() { // Why does this return `uint16`? Should be `int32` (same as retval of `rand()`)
    static_assert(RAND_MAX == 0x7FFF, "PC-generated random numbers should not exceed 32767");
#ifdef BETTER_RNG
    static_assert(false, "PC-generated random numbers should not exceed 32767");
#else
    return rand();
#endif
}

float CGeneral::GetRadianAngleBetweenPoints(CVector2D a, CVector2D b) {
    return GetRadianAngleBetweenPoints(a.x, a.y, b.x, b.y);
}

bool CGeneral::RandomBool(float chanceOfTrue) {
    assert(chanceOfTrue <= 100.f);
    return CGeneral::GetRandomNumberInRange(0.f, 100.f) <= chanceOfTrue;
}

/*!
* @return true/false with 50/50 change
* @addr notsa
*/
bool CGeneral::DoCoinFlip() {
	return CGeneral::GetRandomNumber() >= RAND_MAX / 2;
}
