# MAC_HANDOFF_2 (2026-10-24)
portable/main state: GTASA_PORTABLE + PortableMac (brew llvm 23, librw GL3 + SDL3), shims (source/portable), platform layer (W 0),
P-1 VALIDATE split, P-2a..j landed (FileRecord pattern, real-data tests: gta3.img DFFs, main.scm, 64 NODES*.DAT), R-1..R-3b landed
(R census 6; tools/portable/run_gl_smoke.sh = first native GL run, 86 checks OK on GL 4.1 core).
Portable native census: 308 unique left (P width/overload ~225, F inline asm 36, C 14, imgui 4) — .notes/w0/r3b_after.tsv.
Running: MAC-P4 (width triage + mechanical fixes + .notes/P4_PLAN.md), MAC-P2k (P-2 verification, ctest wiring, strict guard).
Next: P-4 design slices; F inline asm (STREAM_F_PLAN) for the compile gate; first full ninja build + link of PortableMac;
R-4..R-7 (Im2D/Im3D, TXD->GL3 incl. BGRA swizzle, stock pipes, menu boot); W-3/W-4 OpenAL audio; record/replay hash harness.
Windows asks outstanding (PORTABLE_SYNC): merge portable/main into reverse/interior_c; REF-1 reference frames; a real .b save.
Methods: tools/portable/snap_census.sh (census on a snapshot), wine MSVC A/B per slice (__LINE__-only = neutral), .git/info/attributes
union merge for PHASE2_STATUS/metrics/PORTABLE_SYNC (local only).
