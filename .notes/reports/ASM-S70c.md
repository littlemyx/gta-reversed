# ASM-S70c (BreakObject_c x7, BreakManager_c::Add, CBirds x3, CEscalator::AddThisOne, CGlass::Render + 3 poly helpers)

All 16 rows compared against the exe. Verdicts: 8 FIXED, 8 MATCH (table: S70c.result.tsv).

Fixes (rebased hashes: Init 4339a806c, DoCollisionResponse c13765223, Update a76d99179, Render 9099fe6ce, Birds Create ae141b9ab, Update 2b612de17, Render ef85e0318, Escalator 345f173f9)
- BreakObject_c::Init 0x59E750: collision-box centre is (max - min) * 0.5 + min (port used (max + min) / 2); origin z is (pos.z + min.z) + 0.25 (port pos.z + (min.z + 0.25)).
- BreakObject_c::DoCollisionResponse 0x59DE40: the exe calls RW's table-based RwV3dNormalize (x2) / RwV3dLength (x2: speed and sparks amount), not CVector::Normalise / Magnitude; the smoke particle position is (rand * (1/32767) + pos) - 0.5 (port: pos + lerp(-0.5, 0.5)); a NaN speed does not set bStoppedMoving (jp on `speed < 0.05`).
- BreakObject_c::Update 0x59E220: the settling axis uses RwV3dNormalize.
- BreakObject_c::Render 0x59E480: with no break groups the exe returns without RenderEnd.
- CBirds::CreateNumberOfBirds 0x711EF0: normal-biome body/wing grey is (rand & 0x7F) + 0x80 (port took the top bits via the int range helper); the exe stores the check position over its by-value `vecTargetPos`, so bird n+1 aims at bird n's check point.
- CBirds::Update 0x712330: desert count is min(n, 3 - birds) (port min(n, birds)); water biome only when distance < 30.0 (30.0 and NaN are land); camera-relative spawn angle offset is a signed int [-128, 127] (port wrapped through uint8); far-bird removal compares squared 2D distance with max^2 (port sqrt); the per-bird update needs time > updateAfter (port >=).
- CBirds::Render 0x712810: far fade is 1 - (dist - 0.7*max) / (0.3*max) (port invLerp divides by max - cutoff); second wing phase is time % period (port added the period first).
- CEscalator::AddThisOne 0x717970: bounding radius sums z,y,x; plane count is ftol(len * 2.5 + 1.0).

MATCH: BreakManager_c::Add, BreakObject_c::CalcGroupCenter / Exit / DoCollision, CGlass::Render, RenderReflectionPolys, RenderShatteredPolys, RenderHiLightPolys (buffer bases 1024/1536 and index bases 2048/3072, state order, flushes and resets identical to the exe).

Notes: the exe passes bJustFaces/bSmash as a full int, the port keeps bool in BreakManager_c::Add / m_JustFaces (byte store of the low byte): only differs for values other than 0/1, no caller passes them. Biome > 2 (never used) skips colours in the exe but is treated as NORMAL in the port.

Tests: wave2_oracle_test (RW_EXE_ORACLE=GTA_EXE=gta_sa_compact.exe): 1142 functions, 0 mismatches PC24 / PC53 (the target does not contain the Birds / BreakObject / Escalator functions, it only confirms nothing else moved); combat regress PASS=14 (StandaloneRelease of the worktree, built after the last edit; the link needed one retry after a transient LNK1318 PDB error); v6 pre-commit grep clean; Birds.cpp, BreakObject_c.cpp and Escalator.cpp compile for PortableWin64 (x64 cl, flags from the main PortableWin64 compile_commands with the include root swapped to the worktree, PCH dropped).
