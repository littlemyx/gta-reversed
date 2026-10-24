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
