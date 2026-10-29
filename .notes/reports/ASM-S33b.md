# ASM-S33b (WaterLevel.cpp render code, 18 rows)

Result table: .notes/asm_sweep/S33b.result.tsv. 3 split-decision fixes in 3 commits, SplitWaterRectangleAlongXLine PARTIAL (t only), 14 NOT_REACHED (budget). Context: ASM-S33.md.

- RenderWaterRectangle 0x6EC5D0 (df0714e3): the camera range globals are C1F950 MaxY, C1F954 MinY, C1F958 MaxX, C1F95C MinX. Outside test equal. For X the exe tests `maxX > MaxX` first and splits at MaxX; only otherwise `minX < MinX` and splits at MinX. The port split at MinX whenever minX < MinX (different result only when both hold; same union of quads, different recursion). Y order (minY < MinY first) equal.
- RenderWaterTriangle 0x6EE240 (cc2886b4): same X split order fix.
- SplitWaterRectangleAlongYLine 0x6ED6D0 (6552b3a5): t = (split - Y1) / (Y2 - Y1) (fild / fidiv); the port used min/max of Y1, Y2, which differs when Y1 > Y2. SplitWaterRectangleAlongXLine t equal.
- Not compared: the (1-t) lerp blends and the recursive calls of the Split* functions, the flat / high detail copies inside RenderWaterRectangle, the triangle Split* family (the port is restructured), all vertex / UV / colour code.
