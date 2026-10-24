// P4-S3 host test: the portable tRestoreEntry of source/game_sa/Models/VehicleModelInfo.h (struct text extracted from the real header by the runner -> $OUT/restore_entry.inc).
//   tools/portable/run_p4_restore_test.sh   (`mutate`: 4 byte restore of a texture slot, 8 byte over-read of a colour, no zero extension ... must FAIL this test)
// Models the three save shapes of SetEditableMaterialsCB / RemoveWindowAlphaCB / ResetEditableMaterials: a texture-pointer slot, the 4 RGBA bytes, the first float of the surface properties.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
using uint8 = uint8_t; using uint32 = uint32_t; using uint64 = uint64_t;
#define NOTSA_PORTABLE 1
#define VALIDATE_SIZE(struc, size) static_assert(true)
#include "restore_entry.inc"

static int g_fails = 0;
#define CHECK(c, ...) do { if (!(c)) { g_fails++; std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct RGBA { uint8 r, g, b, a; };
struct Surf { float ambient, specular, diffuse; };
struct Material { void* texture; RGBA color; Surf surf; }; // librw-like: a pointer slot (8 bytes on LP64), the colour, the surface properties

int main() {
    static_assert(sizeof(void*) == 8, "the test is about LP64");
    // exact-size heap objects so ASan flags any over-read of the 4 byte colour / float (the exe's `*(void**)colour` on LP64)
    RGBA* colour = (RGBA*)std::malloc(sizeof(RGBA)); *colour = { 10, 20, 30, 0 };
    float* ambient = (float*)std::malloc(sizeof(float)); *ambient = 0.5f;
    Material m{}; m.texture = (void*)0x123456789ABCull; m.color = { 255, 175, 0, 77 }; m.surf = { 1.f, 2.f, 3.f };

    tRestoreEntry list[8] = {}; tRestoreEntry* p = list;
    p->Save(&m.texture, sizeof(m.texture)); p++;               // texture slot
    p->Save(&m.color, 4); p++;                                 // colour (Material colour)
    p->Save(&m.surf, 4); p++;                                  // first float of the surface properties only (the exe restores just 4 bytes)
    p->Save(colour, 4); p++;                                   // heap colour, exactly 4 bytes allocated
    p->Save(ambient, 4); p++;
    p->m_pAddress = nullptr;                                   // terminator like SetEditableMaterials

    // the game overwrites everything
    m.texture = (void*)0xDEADBEEF; m.color = { 1, 2, 3, 4 }; m.surf = { 9.f, 8.f, 7.f }; *colour = { 0, 0, 0, 255 }; *ambient = 100.f;

    for (auto* e = list; e->m_pAddress; e++) e->Restore();      // ResetEditableMaterials

    CHECK(m.texture == (void*)0x123456789ABCull, "texture slot restored as 8 bytes: %p", m.texture);
    CHECK(m.color.r == 255 && m.color.g == 175 && m.color.b == 0 && m.color.a == 77, "colour %d %d %d %d", m.color.r, m.color.g, m.color.b, m.color.a);
    CHECK(m.surf.ambient == 1.f, "ambient restored");
    CHECK(m.surf.specular == 8.f && m.surf.diffuse == 7.f, "specular/diffuse are NOT restored (bug-compatible: the exe saves only 4 bytes): %f %f", m.surf.specular, m.surf.diffuse);
    CHECK(colour->r == 10 && colour->g == 20 && colour->b == 30 && colour->a == 0, "heap colour");
    CHECK(*ambient == 0.5f, "heap float");
    // the high half of a texture slot restored with a colour save must not leak: entries are independent and zero-extended
    tRestoreEntry e; e.Save(&m.color, 4); CHECK(e.m_nValue == 0x4DFF00AFull + 0 || e.m_nValue <= 0xFFFFFFFFull, "4 byte save is zero extended: %llx", (unsigned long long)e.m_nValue);
    CHECK(e.m_nSize == 4, "size recorded");
    std::free(colour); std::free(ambient);
    std::printf("p4 restore-entry test: %s (fails=%d)\n", g_fails ? "FAILED" : "ALL OK", g_fails);
    return g_fails != 0;
}
