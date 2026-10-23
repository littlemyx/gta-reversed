# CRASH-HUNT - anim null-slot crash (NOT captured) and stencil-shadow crash (root cause + fix)

Rules v4. Start 20:30, end ~22:05 (2026-10-23). Worktree C:/gta-wt/crash-hunt (removed at hand-back). Scratch: C:/gta-scratch/crash-hunt/.

## Tooling added
* cdb x86 (C:/gta-scratch/dbg/cdb.exe) wired into gameplay_regress.sh through `GR_WRAP` (prefix of the game command, e.g. `GR_WRAP=/c/gta-scratch/crash-hunt/cdbrun.sh`)
  and `GR_NORETRY=1` (no second attempt after a set-up failure - halves the time of a hunt loop). The wrapper `cdbrun.sh` runs the game under cdb with
  `sxe -c "$$<av.txt" av`; av.txt dumps `.exr -1; r; kv 40; ub/u @eip; dd esp` and quits when the first-chance AV is inside gta_reversed.exe (tested on a null-deref
  test program; benign AVs in other modules continue). Breakpoint scripts (`bu ...Process`) work the same way (cdbrun_tc.sh / cdbrun_poke.sh).
* Run cost: 2.7 min per cheats run without GPU contention, 4.3-8.5 min with other agents' soaks / the gate holding gpu.lock.

## 1. Anim null-slot AV (UncompressAnimation <- CopyAnimation <- BlendAnimation <- CTaskSimpleFall::ProcessPed:74): NOT captured
* 13 cheats runs under cdb in 50 min (2 on the 16:25 build, 11 on HEAD 665a3ab0; 8 of them under 8 BelowNormal CPU burners + other agents' builds), 0 AVs.
  Together with ANIM-CRASH (1 of ~19 under load) the rate is below ~1 in 20 runs, and the GPU lock shared with the gate/soaks limits a hunt to ~10 runs per 50 min.
* Nothing new about (group, id). What the registers of the 3 earlier sightings still allow (EBX=-1, EDX=0x3c0=40*24, EDI=0):
  - CAnimBlendStaticAssociation() = default gives an unfilled slot m_AnimId=-1 (ANIM_ID_UNDEFINED), m_AnimGroupId=-1, m_BlendHier=null; EBX=-1 is that slot's id/group,
    i.e. BlendAnimation was handed a slot that CreateAssociations(2/3) never filled (the vehicle groups: 40 slots, 4-6 anims) or index 40 of a short group.
  - HIT_BEHIND is anim 40 of the default group and PED.IFP of the Steam install contains "HIT_behind" (so the "default group index 40" reading is the less likely one).
* Next step for a continuation: run the same loop (cdbrun.sh) on a quiet machine / overnight; the first hit prints kv with BlendAnimation's args (group, id) and the
  CTaskSimpleFall `this` in the ProcessPed frame. Everything is in place: `GR_WRAP=/c/gta-scratch/crash-hunt/cdbrun.sh GR_NORETRY=1 tools/standalone/gameplay_regress.sh -x <exe> cheats`.

## 2. Stencil-shadow AV (CCollisionData::GetShadTrianglePoint:205 <- BuildShadowVolume:124 <- CStencilShadows::Process:478, read at 0x3)
* Crash registers (ANIM-CRASH m-3): ECX=3, EDX=0, EAX=0, EDI=3; GetShadTrianglePoint +0xd is `movsx eax, word ptr [ecx+edx*2]` with ecx = colData->m_pShadowVertices (+0x28).
  So colData is NOT null, its m_pShadowVertices is the garbage value 3 (and m_nNumShadowTriangles matched m_NumShadowFaces, otherwise RenderForObject returns early).
* Root cause: CColModel::AllocateData(int32 size) (the single allocation used by AllocateData(spheres, boxes, lines, verts, tris, disks), 0x40F870) mallocs the
  CCollisionData without running its constructor. The exe's 0x40F870 fills only +0x00..+0x1C (counts, sphere/box/line/vertex/triangle pointers, bUsesDisks); the flag byte
  (+0x07: bHasFaceGroups, bHasShadowInfo) and the shadow fields +0x20..+0x2F stay as the heap left them. Users: CTempColModels (flying car door/bumper/panel/bonnet/boot/wheel,
  body parts, ped colmodels) and CPedModelInfo. Measured under cdb at frame 900 of the cheats route (debug heap fill): all of them have m_nNumShadowTriangles =
  m_nNumShadowVertices = m_pShadowVertices = m_pShadowTriangles = 0xBAADF00D and bHasFaceGroups = 1 (garbage) - C:/gta-scratch/crash-hunt/logs/tc-213111.log.
  Without the debugger the contents are whatever the recycled CRT block held. The exe builds these at start-up from a heap that was not recycled yet (zeros); the port builds them
  after much more heap churn. A temp colmodel with bHasShadowInfo=1 and stale numbers is registered as a stencil shadow OBJECT as soon as a CObject carrying it exists in the
  repeat sectors (cheat 56 OFVIAC, "blow up all cars", spawns exactly such flying components - the m-3 crash fell on cheat_56) and RenderForObject reads m_pShadowVertices = 3.
* Fix (ColModel.cpp CColModel::AllocateData(int32)): memset the CCollisionData header to zero after the Malloc = the state the exe sees. Not a mask: the exe has no extra check, the fields are
  simply deterministic. Consumer audit: every user of AllocateData(int32) is a path that then sets all counts/pointers it needs; the zeroed fields are exactly the ones the exe leaves unset
  (shadow counts/pointers, bHasFaceGroups/bHasShadowInfo); FileLoader's raw Mallocs construct CCollisionData with placement new and set bHasShadowInfo explicitly (unchanged).
* Oracle row: not possible (the fields are indeterminate in the exe as well).
* Mechanism proof on the pre-fix exe: see "Verification".

## Verification
* Pre-fix mechanism proof: the cheats route under cdb with a breakpoint on CStencilShadows::Process that, at the 900th frame, writes the stale-heap values into the temp colmodels
  (m_nNumShadowTriangles = m_nNumShadowVertices = m_pShadowVertices = m_pShadowTriangles = 3, bHasShadowInfo = 1; cdbrun_poke.sh / poke.txt). On cheat 56 the game dies with the
  recorded crash: read at 0x3, ECX=3 EDI=3 EDX=0, `GetShadTrianglePoint+0xd movsx eax, [ecx+edx*2]` <- BuildShadowVolume+0x1f3 <- CStencilShadows::Process+0x10a <- CGame::Process
  (C:/gta-scratch/crash-hunt/logs/poke-215145.log). The first poke attempt (poke-213424) had a script error and proves nothing.
* Post-fix state: the same dump on the fixed exe (cdb, frame 900): bHasFaceGroups = 0, bHasShadowInfo = 0, shadow counts 0, shadow pointers null for every temp colmodel
  (logs/tc-215609.log); pre-fix the same fields were 0xBAADF00D (logs/tc-213111.log).
* Soak (fixed exe, NOVSYNC, new game, 6.5 min route): RESULT OK, 0 errors, 21 markers (C:/gta-scratch/crash-hunt/soak-fixed/report.txt).
* Crash rate: the shadow AV was seen once in ~30 cheats runs (ANIM-CRASH + this task); a 15-run acceptance cannot show 0 for a 1-in-30 event, so the proof above is by mechanism
  (reproduced with the poke, absent after the fix), not by rate. gameplay_regress cheats on the fixed exe under cdb: set-up failure (free_start / header, load dependent as on HEAD), no crash.
* run_all_tests: NOT run by this agent (a fresh worktree has no StandaloneRW build; the 100 min cap does not leave room for the ~1200-TU test build). The change is one memset in
  CColModel::AllocateData(int32); the orchestrator gate covers it on landing.

## Side findings
* HEAD (665a3ab0) cheats runs failed the set-up gate (`free_start mission=1`: the first Return did not mount the tutorial bike within 6 s, the second did, so the opening mission
  stayed on) in 3 of 5 cdb runs, vs 0 of 2 on the 16:25 build. Load-dependent mounting (cdb + CPU burners) is the likely cause, not proven a regression; check the gate's own cheats result.
* gpu.lock is shared with the gate and with soaks: a hunt loop gets ~10 runs per 50 min.
