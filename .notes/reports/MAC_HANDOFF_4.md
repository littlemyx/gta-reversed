# MAC_HANDOFF_4 (2026-10-24)
Goal (user): re3-level fidelity for portable builds. State:
- macOS arm64: plays (menu, new game, drive/shoot/police/cheats/save-load), audio OpenAL, SDL3 input/HiDPI/vsync-safe, GL3 render incl.
  TXD, stock+NVC+car/building env pipes, D3D-FF lighting, skin audit; load crash (IFP garbage + ftol saturation) fixed; 155 class-A ftol
  sites; LP64 audits P-2..P-5; walk RR traces bit-identical to Win32 (loop stopped by user goal change).
- Linux x64: links + ctest 17/17 in ubuntu-24.04 container; CI workflows blocked on private librw access (user to run
  tools/portable/ci_librw_access.sh). Fork is PUBLIC by user decision; CI artifacts logs-only.
- Win x64: links via msvc-wine; wine run stops at GL init (NotsaRwGetGlInfo should bail when glad fails).
Blocked on the user being at the Mac (display idle -> GL swaps hang): R-11a/b PostFX + reVC-style shadows, interiors, R9-2 night car.
Headless next: NotsaRwGetGlInfo guard, ftol class B, x64 FP-trap audit, windows-latest CI workflow, SDL3 polish items not needing GPU.
Quota: weekly ahead of linear pace -> 1 agent at a time.

## 2026-10-25 update
Landed since: MERGECHK (merge of 58 interior_c commits; portable build fixed), R11 (PostFX, stencil + real-time shadows, rain hang),
INT1 (interiors: librw depth-mask cache), GLSTATE (librw gl3 cache audit, address-mode typo). CI: all portable workflows manual-only;
Build disabled on gta-reversed and on librw forks (failure mails). Every commit in both repos carries [skip ci].
Open: heat haze / underwater / gamma unverified, GLSTATE-1/2, camera lag after cmd:interior, Linux/Windows CI need librw access
(user: tools/portable/ci_librw_access.sh), x64 FP-trap audit, ftol class B, Win x64 on real hardware (asked Windows host).
GPU work needs an active display (caffeinate -dimsu helps once the display is awake).
