# ASM-P4R10c: review of d5745f101 + int-vs-float (fild/fld) diff of Ped / Vehicle / Physical hooks

Review d5745f101 (CStats::GetStatValue, exe 0x558E40): CONFIRMED. The exe does `cmp ax,0x52; jae` then `fild [id*4 + 0xB78E20]` with no
range check; the int array base 0xB78E20 is &StatTypesInt[0] - 120*4, so ids 82..119 read below the array. IsStatFloat (0x558E30,
`cmp word [esp+4],0x52; setb`) agrees with the port. `IntStatCell` reproduces this. No fix needed.

Scan: tools/standalone/asm/fildscan.py (new). Exe side via exedis.py for all 772 hooked functions of Entity/Ped/*.cpp, Entity/Vehicle/*.cpp,
Physical.cpp; `this` register tracked through ecx copies; offsets mapped to port fields via `cl /d1reportSingleClassLayout<Class>` plus header
member declarations (VALIDATE_OFFSET is too sparse in these headers); stack args via esp-delta tracking vs the port definition's parameter types.
Results: 1813 this-field/param x87 sites, 408 integer-operand sites (fild/fidiv/fimul/fisub/fiadd) of which 347 are [esp+N] spills/params,
0 real mismatches. Only 1 fild reads a this-field ([esi+0x9d0] in Plane::ProcessFlyingCarStuff, int32 field, port converts via uint32 as the
exe's jge/fadd 2^32 idiom); 5 fild read CTimer/ int globals (0xB7CB84, 0x8D3448): all uint32 in the port. 8 scanner hits (bool/uint32 "float" params)
were hand-checked: stack local slots, false positives of the esp tracker.
Not covered: int-vs-float of [esp+N] locals (not params), `mov`-based integer fields vs float ports, non-hooked functions.
No source changes, no build needed beyond the script.
