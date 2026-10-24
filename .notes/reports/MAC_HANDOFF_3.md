# MAC_HANDOFF_3 (2026-10-24)
Milestones 1-3 reached natively on macOS arm64 (PortableMac): links, boots to menu, New Game plays (walk/drive/shoot/police/cheats/save+load),
audio (dsound_al over OpenAL, radio), SDL3 input (kb/mouse/pad), vsync-safe, HiDPI, clean exit. RR harness (GTASA_STATEHASH) self-deterministic.
Running: MAC-RR3 (Win32 RR variant under wine + first native-vs-Win32 diff).
Next: R-8 buildings custom pipes, R-9 car env/specular, R-10 skin audit, R-11 effects, R-12 frame diff (REF-1 frames from Windows);
F stream for trace equality (F-2..F-7 per STREAM_F_PLAN, AudioLog10 rounding); RR-4..7; Linux x64 CI; Win x64.
User decision pending: portable.file_records warn step vs global STRICT (auto-mode refused the runner edit).
Windows asks open in PORTABLE_SYNC: merge portable/main, REF-1 frames, real save/replay fixtures, W1 (GenericSave padding), W2, CBike F5.
Build dirs to prune when space matters: build/PortableMacRR, build/PortableMacP1 (~800 MB each).

## 2026-10-24 update — goal change
User: portable fidelity bar = re3 (GTA III port) level; no Win32 trace equality. Stop FLOOP. Use re3 (~/github/re-GTA) as the model.
New queue: re3 digest (.notes/RE3_DIGEST.md) -> Linux x64 build + CI on the fork -> render completeness R-10..R-12 (skin audit,
effects/PostFX, D3D-like lighting) -> Win x64 -> stability soak on Mac (long play sessions, load race MAC-RACE1).
