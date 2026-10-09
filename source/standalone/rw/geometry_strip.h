// Port of the exe's RpBuildMesh tristrip generator (RpBuildMeshGenerateDefaultTriStrip 0x7591B0 -> 0x7591D0, helpers 0x759790 / 0x75A090 /
// 0x75A1B0 / 0x75A250 / 0x75A370 / 0x75A660), used by RpGeometryUnlock (0x74C800 -> _rpMeshOptimise 0x75D970, default strip method).
// Pure C++ (no RenderWare types) so that tests can drive it directly.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace RwShim {
// Strips ONE mesh: `tris` are the vertex index triples of the mesh's triangles in the order the exe's qsort (comparator 0x759640) left them.
// The strips are joined into a single index list (appended to `out`) exactly like the exe's 0x75A660 does.
//   tryAllStarts = the 3rd argument of 0x7591D0 (param_2 -> 0x759790 param_4): try all three start rotations of every strip start (0x7591B0 passes 0)
//   padParity    = 2nd argument of 0x7591D0 (param_3 -> 0x75A660 param_2): fix the winding parity of joins with extra duplicates (0x7591B0 passes 1)
// The exe's default method (0x7591B0) is TriStripMesh(tris, n, false, true, out).
void TriStripMesh(const uint16_t (*tris)[3], size_t numTris, bool tryAllStarts, bool padParity, std::vector<uint16_t>& out);
} // namespace RwShim
