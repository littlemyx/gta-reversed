# BUG-REFS: AV in `CEntity::~CEntity` (+0x17d) during `CPools::ShutDown` (Windows host, 2026-10-17)

Commits: 3f468b99 (pool shutdown, the fix), 523afe99 (reference-list fidelity + logic oracle test). Worktree C:/gta-scratch/wtref (StandaloneDetached + StandaloneRW), harness/logs C:/gta-scratch/refs (`run.sh`, `out/*`), 7 of the 8 allowed game runs.

## Verdict
**Root cause: not the reference lists and not 0fa3ade8. The port runs the destructor of every entity left in the pools at pool shutdown, the exe never does.**
`CPools::ShutDown` does `delete pool` -> `~CPool` -> `Flush` -> `Clear` -> `std::destroy_at` on every valid element, because `notsa::bugfixes::CPool_DestructOnClear` ("destroy objects before marking their memory as free", Pirulax) is **enabled by default and was also enabled in the standalone run build**. The exe's `CPools::ShutDown` (0x5519F0) is 17 x { `free(storage)`, `free(slot states)`, `operator delete(pool)` } and contains no call to any destructor. In the port the leftover entities are destroyed after the object pool (and the earlier pools) were freed, so `~CEntity::ResolveReferences` reads `*ref->m_ppEntity` through a reference whose holder lives in freed memory. Whether that read faults depends on whether the heap has unmapped the freed block: a lottery (1 of 4 F2 census runs; the other runs read stale but mapped memory and compare unequal, so nothing is written).

## 1. Static comparison with the exe (asm via capstone, `C:/gta-scratch/refs/dis.py`)
| function | exe | port (before) | verdict |
|---|---|---|---|
| `CReferences::Init` 0x5719B0 | 3000 nodes chained, `[0xBA1760]` (= aRefs[2999].next) = 0, pEmptyList = aRefs | same (loop + last = null) | equal; oracle-tested |
| `ListSize` 0x5719E0 | counts via `next` | same | equal; oracle-tested |
| `CleanUpOldReference` 0x571A00 | unlink the node whose `ppEntity == arg`, push on free list, `ppEntity = 0` | same | equal |
| `ResolveReferences` 0x571A40 | `if (*ppEntity == this) *ppEntity = 0` for every node (no null check of `ppEntity`), clear `ppEntity` of all nodes, splice the whole list onto the free list | extra `ref->m_ppEntity &&` | removed (523afe99), otherwise equal |
| `PruneReferences` 0x571A90 | drop nodes with `*ppEntity != this` | same | equal |
| `RegisterReference` 0x571B70 | building filter (`type==building && !(tempBuilding\|procObject) && iplIndex==0` returns), duplicate check, on an empty free list prune peds, then vehicles, then objects **from the last slot down** until a node is free, silent drop if none | forward slot order | fixed (523afe99); prune scan runs only when the free list is empty (all 3000 nodes in use) |
| `PruneAllReferencesInWorld` 0x571CC0 | peds, vehicles, objects, **each from its last slot down** | forward | fixed (523afe99): the free list came out in a different node order (found by the new oracle test) |
| `RemoveReferencesToPlayer` 0x571AD0 | `FindPlayerVehicle(-1,false)->ResolveReferences`, `FindPlayerPed(-1)->ResolveReferences`, group `SetLeader`, `Process` | same | equal (not oracle-tested: needs the player/group world) |
| `~CEntity` 0x535E90 | `if (m_pLod) --m_pLod->numLodChildren; DeleteRwObject; ResolveReferences; ~CPlaceable` | `RemoveLodChildren()` is that decrement | equal |
| `CIplStore::RemoveIpl` 0x404B20 (post 0fa3ade8) | inclusive `first..last` for buildings and dummies, objects `0..size-1` with `CWorld::Add(dummyObject)` first, `CWorld::Remove` + deleting dtor, then `RemoveCarGenerators` | same | equal; **0fa3ade8 is not involved** (the objects that hold the dangling references have nothing to do with IPL leftovers) |
| **`CPools::ShutDown` 0x5519F0** | 17 pools: `free(objects)`, `free(flags)`, `delete pool`; **no destructor** | `delete pool` runs every element destructor (CPool_DestructOnClear) | **DEVIATION = root cause**, fixed (3f468b99) |
| `~CObject` 0x59F660 | **no `CleanUpOldReference` for `m_pDummyObject`** (no call to 0x571A00 anywhere in it) | same | equal: the exe has the same latent stale reference, see 4 |

Pool exhaustion: the exe's `Init` loop links all 3000 nodes to the next one (the last to `&aRefs[3000]`, which is the `pEmptyList` global) and then stores `[0xBA1760] = 0` (= aRefs[2999].next), so the list ends in null exactly like the port's; the pool cannot run into the global.

## 2. Dynamic reproduction
Private build (not committed): diagnostics in `CPools::ShutDown` (per pool: entities left, references, where each `ppEntity` points, LOD pointers) and `NOTSA_BR_PROTECT=1` = after each pool is freed its whole pages become `PAGE_NOACCESS`, plus an env switch for old/new pool-shutdown behaviour. Route: `c56_route` (new game, walk, car cheat + drive, Rhino cheat, idle 20 s, quit via the menu), 960x600 window, silent.

| run | pool dtor mode | protect | result |
|---|---|---|---|
| A1 | old (destructors) | off | clean quit (atexit). At `CPools::ShutDown` the ped/vehicle/building/object pools hold **0** valid entities, the dummy pool holds **24** dummies, each with **1 reference whose holder lies in the object pool storage** (the object pool is freed before the dummy pool) |
| B1 | old | on | **AV 0xC0000005 at CEntity::~CEntity+0x179** (inlined ResolveReferences, same function and ~offset as the census_p1 crash) <- `CDummyObject::scalar deleting destructor` <- `CPool<CDummy>::Flush:141` <- `CPools::ShutDown` <- `CGame::Shutdown`; 50 dummies, 50 references into the object pool |
| B2 | old | on (corrected: only pages fully inside the pool) | same AV, same address 0x00EDFCE9, fault address 0x144A1944 inside the protected object pool [0x14497000, 0x144B9000); 30 dummies |
| C1 | new | on | harness bug of mine (first/last protected page overlapped the neighbouring ColModel pool: `memset` in its Flush faulted); not evidence, corrected in B2/C2 |
| C2 | new (`PoolDestructOnClear()==false`) | on | **clean quit** (atexit handler ran, rc 0), 31 dummies with 31 references into the freed object pool: nobody reads them |

Why the references exist: `CObject::m_pDummyObject` is registered with the dummy (`RegisterReference`, Object.cpp:666); `CWorld::ShutDown` deletes the objects (the exe's `~CObject` does not clean the reference up) but the dummies that were swapped for objects stay in the dummy pool, detached from the sectors, with the stale node in their list.

## 3. Fix
`Core/Pool.h`: `notsa::detail::PoolDestructOnClear()` is `false` under `NOTSA_STANDALONE_RUN` (the NOTSA fix stays on for the DLL build), used by `Flush()` and `Clear()`. `Pools.cpp`: comment (the claim "freed without running the destructors" is now true). No null checks, no skipped deletes: the run build now does what the exe does. The same switch removes the other destructor-at-shutdown hazards of the same class (all other pools: tasks, events, route/ptr-node pools, `ms_octTreePool`; the `m_pLod` decrement of `~CEntity` writing into the freed building pool, a possible source of the other quit-time heap corruptions).

## 4. What the exe does differently in memory (the latent bug is the exe's too)
`~CObject` leaves a stale node (`ppEntity` = the dead object's `m_pDummyObject`) in the dummy's list, in the exe as in the port. It is harmless there because (a) the object pool storage is never unmapped while the process runs and its slots are reused: a later `ResolveReferences` of the dummy compares `*ppEntity == this` and finds either null, another dummy or garbage that is not `this`, so it writes nothing, and (b) nothing ever runs a dummy destructor after the pools are freed. `RegisterReference`'s prune pass (`PruneReferences`) reclaims such nodes only when the 3000-node pool runs dry. The port had (b) violated; with the destructors gone the port matches the exe in both.

## 5. Tests
* `tests/standalone/logic_oracle_refs.inc` (included by `logic_oracle_test`, group `TestRefs`): `CReferences::Init`, `ListSize`, and 300 random sequences of 90 operations (Register / duplicate Register / CleanUpOldReference / ResolveReferences / PruneReferences / PruneAllReferencesInWorld / silent holder overwrite) on fake ped/vehicle/object pools, buildings with random filter flags and loose entities, port and exe on the SAME statics (snapshot -> port -> snapshot -> restore -> exe -> compare every node, the free list, all entity storage, all 3,000+ holders); 1/3 of the cases start with an almost used up node pool (prune-all-pools path: 47 of 300 cases reach it, plus the silent drop). Result: StandaloneRW (Debug, the official gate config) PC24 **0** mismatches in these 3 rows, whole `logic_oracle_test` 227 functions PC24 0; StandaloneDetached (Release) also 0 for these rows (its 567 mismatches are the pre-existing `CStreaming::GetDefaultCopCarModel` row, identical with the main tree's old 17:55 binary).
  Found by it: `PruneAllReferencesInWorld` free-list order (237 of 300 sequences differed before the fix). Mutation tests (each detected): forward pool scan (23), `tail->next` of the Resolve splice (280), `CleanUpOldReference` free-list push (298), building filter without `m_bIsProcObject` (93).
* `tests/standalone/pool_oracle_test.cpp` `TestShutdown`: the exe's `CPools::ShutDown` machine code runs on 17 fake pools (host free/delete/print patched in): exactly 34 frees + 17 deletes in order, element storage untouched; the port's `CPool` runs 0 element destructors at `Clear()`/`~CPool` (mutation `return true`: 9 destructors, FAIL). pool_oracle_test PASS (1.9 M checks, Debug and Release).

## 6. Verification runs of the final code (build of 523afe99 in the worktree, no diagnostics, plain quit via the menu)
| run | result |
|---|---|
| F1 | clean quit, rc 0, `atexit handler ran`, 0 exceptions in log.log, 9,100+ frames |
| F2 | clean quit, rc 0, `atexit handler ran`, 0 exceptions |
Builds: StandaloneDetached exe 1185 steps, 0 errors; StandaloneRW `logic_oracle_test` + `pool_oracle_test`; the whole-tree run_all_tests was not repeated (only the two touched tests). A 1-in-4 crash cannot be disproved by 2 clean runs, which is why the proof is the deterministic NOACCESS reproduction (B2 vs C2) and the exe asm, not the run count.

## 7. Open / notes
* The bugfix switch `ReversibleBugFix::Enabled` is on for all 38 `notsa::bugfixes::` uses in the standalone run build; each is a deviation from the exe by construction. Only `CPool_DestructOnClear` was proven harmful here; the others were not audited.
* `Pool.h::Flush` still memsets the freed storage with 0xDD in the run build (exe: no fill); harmless now, kept (CInterestingEvents reset in `ShutDown` depends on nothing else).
* The Mac-era CInterestingEvents dangling entity (87cac5eb) is another symptom of the same freed-pool-memory class; the reset in `CPools::ShutDown` stays.
