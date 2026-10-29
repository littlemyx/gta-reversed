# ASM-S33e (WaterLevel.cpp high-detail render code, 7 rows)

Result table: .notes/asm_sweep/S33e.result.tsv. 4 fixes (4 commits), 2 MATCH, RenderBoatWakes NOT_REACHED (budget; its body is CBoat::RenderWakePoints).
Method: C:/gta-scratch/asm-s33e (S33d unicorn harness + h2.py with thiscall/cdecl hook callbacks, t_hdr.py, t_htri.py, t_hrect.py): the exe functions run in the emulator (PC=24), the OneLayer vertex buffer read back (positions, colour, UV, indices) or the Fog / OneLayer / Split / lerp / Rect / IsSphereVisible calls hooked, against Python models that round to float after every op. Zero mismatches (for OneLayer the model matched the exe; the port's bare-double UV is the deviation).

- RenderHighDetailWaterRectangle_OneLayer 0x6E91D0 (f884e8fb) / RenderHighDetailWaterTriangle_OneLayer 0x6E8780 (d5645e98): UV = offset*scale + base was computed with bare double casts; the exe rounds each op to float (1 ulp off in ~23% of vertices). Now x87d. Positions, z, fade, colour, indices, gradients are equal.
- RenderHighDetailWaterRectangle 0x6EB810 / Triangle 0x6EDDC0: MATCH (the Y split re-lerps P24 then P13 per rect call: same values as the port).
- RenderWaterFog 0x6E7760 (6039f032): inside-fog timer test: `fcomp 0; test ah,0x41; jp` skips on timer > 0 and NaN, the port's `!(t > 0)` entered for NaN. Now `t <= 0`.
- RenderWater 0x6EF650 (81b94c20): the end of the exe inlines the buffer flush and returns early (no RenderBoatWakes, no DefinedState) when the buffer is empty or RwIm3DTransform fails; the port always called both. Out-of-world sea bed blocks use an uninitialised `al` (low byte of the camera position pointer) as the strip-X flag: treated as set, as before.
- Open: RenderBoatWakes 0x6ED9A0 (CBoat::RenderWakePoints GetWidth / GetAlpha / corner math not compared).
