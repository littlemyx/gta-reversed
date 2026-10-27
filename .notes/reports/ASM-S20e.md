# ASM-S20e - last 4 big Ped.cpp functions (2 reached)

Rules v6; worktree C:/gta-wt/asm-s20e (removed at the end). Result table: `.notes/asm_sweep/S20e.result.tsv`. Stopped at the token budget.

| verdict | count |
|---|---|
| MATCH | 1 (ProcessEntityCollision 0x5E2530, compared to ~0x35F3; the 0x35F3..0x3610 epilogue not read) |
| FIXED | 1 (PreRenderAfterTest) |
| UNSURE | 0 |
| NOT_REACHED | 2 (ProcessControl, Constructor) |

## Fix (645cc4ff, Ped.cpp, whole function 0x5E65A0..0x5E7676)
- Shadows: real-time test compared sqrt distance with the squared limit (exe is squared); low-quality dummy shadow no longer tests m_pShadowData (exe only after DoShadowThisFrame); dead ped takes bIsDucking; bInVehicle + null vehicle gives false.
- Wobble bones 0x12E/0x12D are rotated for NON-players (port had it inverted); earthquake swing is multiplied by 0.0025 (missing, 400x too large).
- Rain splash: pos = center + 2 randoms (x,y) and z + radius*0.75 (port drew 3 randoms, RNG desync); wetness drips ((r*0.6+p)-0.3 xy, (r+p)-0.8 z; alpha=(wet*alpha)*0.01).
- Open-top vehicle speed = signed move . forward (z,y,x order), not the magnitude; torso key slot 0 only; wetness is a signed byte > 0.
- Not changed: FX quality HIGH test uses IsPlayer() (FIX_BUGS comment; exe is pedType==0 only).

## Gate
Release build OK. No oracle target covers these functions. soak.sh 7 min RESULT: OK (0 errors, 10600 frames).
