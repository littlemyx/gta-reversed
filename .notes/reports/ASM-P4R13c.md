# ASM-P4R13c
- ManagePed fix e1f3bc48b CONFIRMED against exe 0x6120F2-0x61214F: fmul order (mult*cam)*const, strict `<`, NaN behaviour, constants 65/54.5/25. Precision: x87d is double on the Win32 reference build (hardware runs PC24, so every op rounds to float) and float on PORTABLE; no change needed.
- SlowCarDownForPedsSectorList fully re-read (0x425440-0x425B28): MATCH, no deviations.
- CRoadBlocks: ClearSpaceForRoadBlockObject, RegisterScriptRoadBlock, ClearScriptRoadBlocks MATCH; GenerateRoadBlocks / CreateRoadBlockBetween2Points / GenerateRoadBlockPedsForCar keep S74 verdicts (not re-read).
- No source changes, no build needed.
