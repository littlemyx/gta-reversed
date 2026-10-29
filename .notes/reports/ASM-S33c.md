# ASM-S33c (WaterLevel.cpp render code, 15 rows)

Result table: .notes/asm_sweep/S33c.result.tsv. 2 fixes (2 commits), SplitWaterRectangleAlongXLine MATCH, 12 NOT_REACHED (budget).
Method: the exe functions were run in a unicorn x86 emulator (C:/gta-scratch/asm-s33c/emu.py, model.py, modelx.py, modelrx.py) with the calls to
RenderWaterTriangle / RenderWaterRectangle / the lerp helper 0x6E5280 hooked, and compared against a Python model of the port logic on 300-400 random inputs each.

- SplitWaterTriangleAlongXLine 0x6ECF00 (924c1524): tests only `X1 == X3`. For X1 == X3 the exe blends 3->1 and 3->2 with t = (XS-X1)/(X2-X1) (vertex 3 on the (1-t) side) and uses them for the bottom triangle, the rect (P1,P12,P31,P32) and the right triangle; the port used 1->3 / 2->3. For X2 == X3 the rect is (P12,P2,P13,P23) (port had P12 as 4th); exe call order bottom, rect, left.
- SplitWaterTriangleAlongYLine 0x6EE5A0 (e575d97a): not an X split. Four layouts (Y1<Y3 x X1==X3), t = (split-Y1)/(Y3-Y1), XS = X2 - dX or X1 + dX with the integer dX = (X2-X1)*(split-Y1)/(Y3-Y1); Y1>Y3 rects take (YS, Y1) as the Y range; the Y1>Y3, X2==X3 rect passes the 1->3 blend for both top corners (exe quirk, reproduced).
- SplitWaterRectangleAlongXLine 0x6E73A0: MATCH.
- RenderFlatWaterTriangle / Rectangle only glanced at (bSplitBigPolys test, `cmp 0xa8` size test, X split at the midpoint); not verified. Open: all RenderFlat*, RenderHighDetail*, RenderWater, RenderWaterFog, RenderBoatWakes, RenderWakeSegment. The emulator harness can drive them with hooks on AddWaterLevelQuad / AddWaterLevelTriangle.
