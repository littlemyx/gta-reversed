# MAC_HANDOFF_5 (2026-10-25)
portable/main = interior_c merged up to 2026-10-25 + Mac work. Goal: re3-level fidelity (user).
Done since HANDOFF_4: MERGECHK x2, R11 (PostFX, stencil + real-time shadows), INT1 (interiors), GLSTATE (librw gl3 cache audit),
VIS1 (gamma, visual tour), VIS2 (colours = Win32 under wine), STAB1-4 (LP64 heap bugs: GetWheelPosn, vehicle pool slot vs CPlane,
TaskAllocator slot, FxMemoryPool realloc; matrix-list ownerless link = Win32 fidelity bug in ClearSpaceForMissionEntity; fire-audio UAF).
Stability: soaks 3x10 min (combat, 30-40 car spawns, 40-56 far teleports) clean; ASan clean except 3 known startup reads.
Open (small): rain/fog haze 25-40 m vs Win32 (needs a real Windows frame), ftol class B (926 sites), x64 FP-trap audit,
CI needs librw access (user: tools/portable/ci_librw_access.sh), Win x64 on real hardware (asked Windows host).
Rules learned: [skip ci] on every commit in both repos; Build disabled on both forks; caffeinate -dimsu for GL runs;
one GL process at a time; fresh OUT dir needs gta_sa.set (first-run menu eats Returns).
Note: a caffeinate -d -i -m from CrowdStrike's installer (pid 67094) is not ours.
