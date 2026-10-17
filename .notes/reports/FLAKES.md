# FLAKES report: two one-off crashes of the agent soaks (Windows host, 2026-10-17)

Worktree C:/gta-scratch/wtfl (StandaloneRelease, removed at the end), tools/logs in C:/gta-scratch/flakes, run output C:/gta-soak-out/flakes/*. 6 game runs of the 10 allowed.

## Crash 1: null `new CDummyObject()` in `CFileLoader::LoadObjectInstance` (D-B12)
**Reproduced** (2 of 3 runs that stressed IPL loading; `oldB`, `newC`), same stack/registers as D-B12 (`LoadObjectInstance:1225 <- LoadIpl <- ConvertBufferToObject <- ProcessLoadingChannel`, EAX = 0).

* **The exe has no null check either.** 0x538090 (dummy branch): `push 0x38; call 0x5326E0 (CDummyObject::operator new); cmp eax,esi(0); je skip-ctor; ...; mov edx,[esi]; call [edx+0x18]` -> `esi` is 0 -> AV. Port == exe here; nothing to fix in that function, and the port's CPool matches (`pool_oracle_test`: New 0x5E45E0 scan, EDX = 2500 is the wrap check of a full pass).
* **The dummy pool really fills.** New diagnostic `poollog` (building/dummy/object pool use + loaded IPLs, every 100 frames in standalone.log). Pool = 2500 (exe `CPools::Initialise` too). Walking Grove Street: 1778 dummies with 16 IPLs loaded (71 %); other parts of LS 2075; peak seen 2356 / 2476 in the stress runs. In my runs the crash came from `CStreaming::LoadScene` (a debug teleport that loads the destination's IPLs while the old site's IPLs are still resident). In the real route the same transient can come from any synchronous flush of the request queue (`CStreaming::LoadAllRequestedModels`, e.g. the car/Rhino cheats, `CIplStore::EnsureIplsAreInMemory`) when streaming is slower than usual (host load) and stale IPL requests are queued: the exe would crash identically, it is just less likely there (faster disk, shallower queue).
* **Real port bug found on the way (fixed, 0fa3ade8):** `CIplStore::RemoveIpl` looped `[first, last)` instead of the exe's inclusive `first..last` (0x404B71, 0x404C1B: `jle` back-edge), and `IncludeEntity` compared a building index against `firstBuilding` instead of `lastBuilding` (exe 0x404CF4 uses def+0x24), so `lastBuilding` was the last-loaded index, not the maximum. Result: every IPL unload left its highest-index dummy and 1-3 buildings alive. Measured with an unload check (`FLAKES RemoveIpl ...`): old build 375 unloads, 0 clean; fixed build 398 unloads, 0 leftovers. The leftovers are cleaned by a later unload of the same slot (bounded, ~1 dummy + 2 buildings per IPL), so this is NOT what fills the pool by itself, but it raised the resident count and left entities with dangling `m_pLod` (LOD pointers into IPLs that were gone) in the world; the stress run with the old code showed up to 2x the dummies of the fixed code at the same site (e.g. hop 36: 1661 vs 595).
* Not fixed on purpose: a null guard in LoadObjectInstance (the exe crashes there; the run build follows the exe). If the standalone should survive pool exhaustion instead, the guard is: `if (!newEntity) return nullptr` plus a null check of `FinishLoadObjInst`'s argument in `LoadIpl`/`LoadIplBoundingBox` (the exe would skip nothing, so this is a deliberate deviation, say so in standalone_deviations.tsv).

## Crash 2: AV at 0x016a5f26 after the car cheat (D-B18)
**Located, not reproduced.** The `b18_stash_b18b` exe exists (no map); I located the instruction by byte pattern in `b18_stash_b19b` (map kept): the AV is in **`BuildShadowVolume`** (`StencilShadows.cpp`, common part of `CStencilShadows::RenderForVehicle/RenderForObject`, 0x70FAE0/0x710310), at `mov eax,[colData+0x2C]; movzx ecx, word ptr [eax+t*8]` = `colData->m_pShadowTriangles[t]`: the pointer is bad (null/garbage) while `colData->m_nNumShadowTriangles == obj->m_NumShadowFaces` passed the gate. The exe has the identical gate and loop (0x70FB3F.., 0x70FD34), so the port is a faithful copy; the state that leads to it is the open question. Candidates, none proven: (a) an entity left alive after its IPL/COL went away (the RemoveIpl leak above is one source: leaked buildings keep a `CColModel` whose `m_pColData` was freed/re-created by COL streaming; they get shadow objects through `RegisterStencilShadows`' sector scan), (b) a `CCollisionData` with `m_nNumShadowTriangles` set but no pointer (`LoadCollisionModelVer3` sets the count even when `bHasShadowInfo` is false; harmless today because `CreateShadowObjectFor` requires `bHasShadowInfo`). No recurrence in 6 runs incl. a car + Rhino route. Recommendation: if it comes back, the exception handler prints the address; map it with the matching `.map` (keep exe+pdb+map of every census/soak run).

## Relation to the Mac-era intermittents (PHASE2_STATUS 2026-10-10)
CInterestingEvents dangling entity (fixed 87cac5eb), CAESound freed owner (fixed b402c68c/6a553e8c), CEntryExit pool (no leak, static 376+24 entries): none touches the IPL/dummy path; no relation found. The RemoveIpl leak could only contribute indirectly (stale LOD links) to unexplained one-offs.

## Changes
* 0fa3ade8: `IplStore.cpp` (RemoveIpl inclusive ranges, IncludeEntity lastBuilding), `standalone/Fixups.cpp` (`poollog` line next to `memlog`).
* Test-only tooling (not committed): teleport "hops" and unload check were env-gated diagnostics in the worktree.

## Runs
| tag | build | what | result |
|---|---|---|---|
| oldA | old RemoveIpl | full route (car + Rhino) | OK, dummy flat 1778, no IPL cycling |
| oldB | old | 7 min, teleport hop every ~12 s | crash = crash 1 (hop 37, dummy 2356 peak) |
| newB | fixed | same | no crash, dummy peak 1964-2172, per-site counts converge |
| oldC | old + unload check | 6 min hops | every unload leaves >=1 dummy + 1-3 buildings |
| newC | fixed + unload check | 6 min hops | 398 unloads, 0 leftovers; crash 1 again at hop (dummy 2476): pool overflow by LoadScene overlap, exe-identical |
| finalA | final commit 6c289fa5 (= 0fa3ade8) | standard soak route 7 min | RESULT OK, 21 markers, 0 errors |
