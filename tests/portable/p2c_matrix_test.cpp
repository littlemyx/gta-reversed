// P-2c host test (portable arm64, REAL engine headers via StdInc): CMatrix_File / CVehicleSaveStructure_File / tReplayCameraBlock matrix accessors.
//   tools/portable/run_p2c_test.sh   (golden vectors come from tools/portable/gen_x86_golden.py, an independent generator; they are generated into $OUT/p2c_golden.inc)
// The game is not linkable on the Mac, so the three out-of-line CMatrix members the headers need are stubbed here with the semantics of Matrix.cpp, and the
// CGenericGameStorage work buffer is an in-memory vector.
// Real headers Matrix.h / Vector.h / VehicleSaveStructure.h / ReplayBlockType.h / GenericGameStorage.h / FileRecord.hpp / FileRecords/MatrixFile.hpp on top of a tiny Base.h + rwplcore.h stub
// (tests/portable/p2c_stubs: the full StdInc cannot be linked on the Mac yet - other slices' known errors).
#include <Base.h>
#define MAX_PATH 260
#define NOTSA_GLOBAL(name, addr, type, ...) std::type_identity_t<NOTSA_UNPAREN type> name __VA_ARGS__
#include "Matrix.h"
#include "VehicleSaveStructure.h"
#include "ReplayBlockType.h"
#include "GenericGameStorage.h"
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include "p2c_golden.inc"

#ifndef NOTSA_PORTABLE
#error "this test is for the portable build"
#endif

// ---- stubs: Matrix.cpp semantics ------------------------------------------------------------------------------------------------------------------------
void CMatrix::CopyOnlyMatrix(const CMatrix& m) { std::memcpy((void*)this, (const void*)&m, sizeof(RwMatrix)); }
CMatrix::CMatrix(const CMatrix& m) { CopyOnlyMatrix(m); }
CMatrix::~CMatrix() {}
static std::vector<uint8> g_buf; static size_t g_pos = 0;
bool CGenericGameStorage::SaveDataToWorkBuffer(void* d, int32 n) { auto* p = (uint8*)d; g_buf.insert(g_buf.end(), p, p + n); return true; }
bool CGenericGameStorage::LoadDataFromWorkBuffer(void* d, int32 n) { if (g_pos + n > g_buf.size()) return false; std::memcpy(d, g_buf.data() + g_pos, n); g_pos += n; return true; }
static void reset() { g_buf.clear(); g_pos = 0; }

static int fails = 0;
#define CHECK(x) do { if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); fails++; } } while (0)

// ---- layout -----------------------------------------------------------------------------------------------------------------------------------------------
static_assert(notsa::HasFileRecord<CMatrix> && notsa::HasFileRecord<CVehicleSaveStructure>);
static_assert(notsa::FileSizeOf<CMatrix> == 0x48 && notsa::FileSizeOf<CVehicleSaveStructure> == 0x68);
static_assert(sizeof(CMatrix) == 0x50, "LP64: the attach pointer widens CMatrix (this is what the record hides)");
static_assert(sizeof(tReplayCameraBlock) == 88 && sizeof(tReplayCameraBlock) == 4 + sizeof(CMatrix_File) + 12, "== CReplay::FindSizeOfPacket(REPLAY_PACKET_GENERAL) (Replay.cpp:1383, VALIDATE_BLOCK)");
static_assert(offsetof(tReplayCameraBlock, isUsingRemoteVehicle) == 1 && offsetof(tReplayCameraBlock, matrix) == 4 && offsetof(tReplayCameraBlock, firstFocusPosn) == 0x4C);
static_assert(sizeof(kGolden_tReplayCameraBlock_basic) == sizeof(tReplayCameraBlock) && sizeof(kGolden_CVehicleSaveStructure_File_basic) == sizeof(CVehicleSaveStructure_File));

static CMatrix_File recFrom(const uint8_t* g) { CMatrix_File r; std::memcpy(&r, g, sizeof(r)); return r; }
static bool eq(const void* a, const void* b, size_t n) { return std::memcmp(a, b, n) == 0; }
static bool fEq(const CVector& v, float x, float y, float z) { return v.x == x && v.y == y && v.z == z; }

// first 0x40 bytes of the in-memory matrix (the RwMatrix part; CopyOnlyMatrix semantics)
static bool rwPartEq(const CMatrix& a, const CMatrix& b) { return eq(&a, &b, sizeof(RwMatrix)); }

int main() {
    // ---- golden CMatrix (identity + pos): file -> object fields -> file ----------------------------------------------------------------------------------
    {
        CMatrix m; const auto g = recFrom(kGolden_CMatrix_File_identity);
        notsa::FileRecord<CMatrix>::FromFile(m, g);
        CHECK(fEq(m.GetRight(), 1, 0, 0) && fEq(m.GetForward(), 0, 1, 0) && fEq(m.GetUp(), 0, 0, 1) && fEq(m.GetPosition(), 10.5f, -3.25f, 7.f));
        const auto out = notsa::FileRecord<CMatrix>::ToFile(m);
        CHECK(eq(&out, kGolden_CMatrix_File_identity, 0x48));
    }
    // ---- golden with non-zero flags and a rotation -------------------------------------------------------------------------------------------------------
    {
        CMatrix m; notsa::FileRecord<CMatrix>::FromFile(m, recFrom(kGolden_CMatrix_File_rotated));
        CHECK(fEq(m.GetRight(), 0, 1, 0) && fEq(m.GetForward(), -1, 0, 0) && fEq(m.GetUp(), 0, 0, 1) && fEq(m.GetPosition(), -2500.5f, 1000.25f, 12.125f));
        const auto out = notsa::FileRecord<CMatrix>::ToFile(m);
        CHECK(eq(&out, kGolden_CMatrix_File_rotated, 0x48));
    }
    // ---- built through the public ctor in zeroed storage (flags/pads 0) == the identity golden -----------------------------------------------------------
    {
        alignas(CMatrix) unsigned char raw[sizeof(CMatrix)] = {};
        auto* m = new (raw) CMatrix(CVector{10.5f, -3.25f, 7.f}, CVector{1, 0, 0}, CVector{0, 1, 0}, CVector{0, 0, 1});
        const auto out = notsa::FileRecord<CMatrix>::ToFile(*m);
        CHECK(eq(&out, kGolden_CMatrix_File_identity, 0x48));
    }
    // ---- attach slot and ownership flag are written as 0; FromFile never touches the attach state (CopyOnlyMatrix semantics) ----------------------------
    {
        CMatrix m; notsa::FileRecord<CMatrix>::FromFile(m, recFrom(kGolden_CMatrix_File_rotated));
        m.m_pAttachMatrix = reinterpret_cast<RwMatrix*>(uintptr_t(0x1234567890)); m.m_bOwnsAttachedMatrix = true;
        const auto out = notsa::FileRecord<CMatrix>::ToFile(m);
        CHECK(eq(&out, kGolden_CMatrix_File_rotated, 0x48) && out.attach == 0 && out.owns == 0 && out.pad4[0] == 0 && out.pad4[1] == 0 && out.pad4[2] == 0);
        auto in = recFrom(kGolden_CMatrix_File_identity); in.attach = 0xDEADBEEF; in.owns = 1;
        notsa::FileRecord<CMatrix>::FromFile(m, in);
        CHECK(m.m_pAttachMatrix == reinterpret_cast<RwMatrix*>(uintptr_t(0x1234567890)) && m.m_bOwnsAttachedMatrix == true);   // untouched
        m.m_pAttachMatrix = nullptr; m.m_bOwnsAttachedMatrix = false;
    }
    // ---- FromFile(ToFile(m)) == m for random RwMatrix bytes (incl. the flags / pad words), 2000 cases ----------------------------------------------------
    {
        std::mt19937 rng(0xC0FFEE); int bad = 0;
        for (int i = 0; i < 2000; ++i) {
            uint8_t bytes[sizeof(RwMatrix)]; for (auto& b : bytes) b = (uint8_t)rng();
            CMatrix a, b; std::memcpy((void*)&a, bytes, sizeof(bytes)); a.m_pAttachMatrix = nullptr; a.m_bOwnsAttachedMatrix = false;
            const auto rec = notsa::FileRecord<CMatrix>::ToFile(a);
            notsa::FileRecord<CMatrix>::FromFile(b, rec);
            if (!rwPartEq(a, b)) bad++;
            if (rec.attach != 0 || rec.owns != 0) bad++;
            if (!eq(&rec, bytes, 0x40)) bad++;             // record bytes 0..0x3F == the RwMatrix part (what the exe's memcpy wrote)
        }
        CHECK(bad == 0);
    }
    // ---- CVehicleSaveStructure -------------------------------------------------------------------------------------------------------------------------
    {
        CVehicleSaveStructure s;   // field_48 is uninitialised on purpose (the exe never sets it): ToFile must write 0
        notsa::FileRecord<CMatrix>::FromFile(s.m_matrix, recFrom(kGolden_CMatrix_File_rotated));
        s.field_48 = 0xAB; s.m_nPrimaryColor = 1; s.m_nSecondaryColor = 126; s.m_nTertiaryColor = 3; s.m_nQuaternaryColor = 255; s.m_nAlarmState = 0x1234; s.m_nMaxPassengers = 3;
        s.m_fSteerAngle = 0.5f; s.m_fGasPedal = 1.0f; s.m_fBrakePedal = -0.25f; s.m_nVehicleUpperFlags = 0x80000001; s.m_nVehicleLowerFlags = 0x00402010;
        const auto rec = notsa::FileRecord<CVehicleSaveStructure>::ToFile(s);
        CHECK(sizeof(rec) == 0x68 && eq(&rec, kGolden_CVehicleSaveStructure_File_basic, 0x68));       // pads and field_48 zero
        // typed Save: exactly the record on disk (0x68), not the 0x70 in-memory object
        reset(); CHECK(CGenericGameStorage::SaveDataToWorkBuffer(s)); CHECK(g_buf.size() == 0x68 && eq(g_buf.data(), kGolden_CVehicleSaveStructure_File_basic, 0x68));
        // typed Load from the golden bytes: every field
        reset(); g_buf.assign(kGolden_CVehicleSaveStructure_File_basic, kGolden_CVehicleSaveStructure_File_basic + 0x68);
        CVehicleSaveStructure l; CHECK(CGenericGameStorage::LoadDataFromWorkBuffer(l)); CHECK(g_pos == 0x68);
        CHECK(fEq(l.m_matrix.GetRight(), 0, 1, 0) && fEq(l.m_matrix.GetPosition(), -2500.5f, 1000.25f, 12.125f));
        CHECK(l.m_nPrimaryColor == 1 && l.m_nSecondaryColor == 126 && l.m_nTertiaryColor == 3 && l.m_nQuaternaryColor == 255 && l.m_nAlarmState == 0x1234 && l.m_nMaxPassengers == 3);
        CHECK(l.m_fSteerAngle == 0.5f && l.m_fGasPedal == 1.0f && l.m_fBrakePedal == -0.25f && l.m_nVehicleUpperFlags == 0x80000001 && l.m_nVehicleLowerFlags == 0x00402010 && l.field_48 == 0);
        CHECK(rwPartEq(l.m_matrix, s.m_matrix));
        // FromFile(ToFile(x)) twice == same bytes
        reset(); CGenericGameStorage::SaveDataToWorkBuffer(l); CHECK(g_buf.size() == 0x68 && eq(g_buf.data(), kGolden_CVehicleSaveStructure_File_basic, 0x68));
        // truncated stream fails (same contract as the raw path)
        reset(); g_buf.assign(kGolden_CVehicleSaveStructure_File_basic, kGolden_CVehicleSaveStructure_File_basic + 0x67); CHECK(!CGenericGameStorage::LoadDataFromWorkBuffer(l));
        // the on-disk stream of CVehicle::Save is [uint32 size][record]; the size is 0x68 (loader ignores it, reads the constant)
        CHECK((uint32)notsa::FileSizeOf<CVehicleSaveStructure> == 0x68);
    }
    // ---- tReplayCameraBlock: byte-exact block, accessors ------------------------------------------------------------------------------------------------
    {
        tReplayCameraBlock blk; std::memset((void*)&blk, 0, sizeof(blk));
        blk.type = REPLAY_PACKET_GENERAL; blk.isUsingRemoteVehicle = true; blk.firstFocusPosn = CVector{-2500.5f, 1000.25f, 12.125f};
        CMatrix m; notsa::FileRecord<CMatrix>::FromFile(m, recFrom(kGolden_CMatrix_File_rotated));
        blk.WriteMatrix(m);
        CHECK(eq(&blk, kGolden_tReplayCameraBlock_basic, sizeof(blk)));
        const CMatrix r = blk.ReadMatrix();
        CHECK(rwPartEq(r, m) && r.m_pAttachMatrix == nullptr && !r.m_bOwnsAttachedMatrix);
        // read from the golden bytes (as the playback does after buffer.Read)
        tReplayCameraBlock g; std::memcpy((void*)&g, kGolden_tReplayCameraBlock_basic, sizeof(g));
        const CMatrix r2 = g.ReadMatrix(); CHECK(fEq(r2.GetForward(), -1, 0, 0) && fEq(r2.GetPosition(), -2500.5f, 1000.25f, 12.125f) && fEq(g.firstFocusPosn, -2500.5f, 1000.25f, 12.125f));
    }
    // ---- source scans: the callers go through the record ------------------------------------------------------------------------------------------------
#define P2C_STR2(x) #x
#define P2C_STR(x) P2C_STR2(x)
    {
        auto slurp = [](const std::string& p) { std::ifstream in(p); std::stringstream ss; ss << in.rdbuf(); return ss.str(); };
        const std::string repo = P2C_STR(REPO_DIR);
        const auto veh = slurp(repo + "/source/game_sa/Entity/Vehicle/Vehicle.cpp");
        const auto rep = slurp(repo + "/source/game_sa/Replay.cpp");
        // CVehicle::Save/Load: portable branch uses FileSizeOf + the typed overloads; the raw sizeof(CVehicleSaveStructure) only in the #else branch
        const auto sPos = veh.find("bool CVehicle::Save()"), lPos = veh.find("bool CVehicle::Load()"), endPos = veh.find("void CVehicle::Shutdown()");
        CHECK(sPos != std::string::npos && lPos != std::string::npos && endPos != std::string::npos);
        const auto body = veh.substr(sPos, endPos - sPos);
        CHECK(body.find("notsa::FileSizeOf<CVehicleSaveStructure>") != std::string::npos);
        CHECK(body.find("SaveDataToWorkBuffer(data);") != std::string::npos && body.find("LoadDataFromWorkBuffer(data);") != std::string::npos);
        size_t pos = 0; int rawOutside = 0;
        while ((pos = body.find("sizeof(CVehicleSaveStructure)", pos)) != std::string::npos) {
            const auto elsePos = body.rfind("#else", pos), ifPos = body.rfind("#ifdef NOTSA_PORTABLE", pos);
            if (elsePos == std::string::npos || ifPos == std::string::npos || elsePos < ifPos) rawOutside++;
            pos++;
        }
        CHECK(rawOutside == 0);
        // Replay.cpp: the two camera-block sites use the record accessors in the portable branch; the reinterpret GetMatrix() only in the #else (x86) branch
        for (const char* raw : { "cameraPacket.GetMatrix()" }) {
            size_t p2 = 0; int outside = 0, n = 0;
            while ((p2 = rep.find(raw, p2)) != std::string::npos) {
                n++;
                const auto e = rep.rfind("#else", p2), i = rep.rfind("#ifdef NOTSA_PORTABLE", p2);
                if (e == std::string::npos || i == std::string::npos || e < i) outside++;
                p2++;
            }
            CHECK(n == 2 && outside == 0);
        }
        CHECK(rep.find("cameraPacket.WriteMatrix(") != std::string::npos && rep.find("cameraPacket.ReadMatrix()") != std::string::npos);
    }
    std::printf(fails ? "p2c FAILED (%d)\n" : "p2c host test: ALL OK (fails=%d)\n", fails);
    return fails ? 1 : 0;
}
