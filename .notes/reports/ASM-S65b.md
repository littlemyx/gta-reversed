# ASM-S65b report

Base daa957222 (origin/reverse/interior_c). Result table: `.notes/asm_sweep/S65b.result.tsv`.

| row | verdict | commit |
|---|---|---|
| CWeaponEffects::Render (0x742CF0-0x743C1E) | FIXED (NaN-only min/max form) | b9b01950a |
| CWeaponInfo::LoadWeaponData rest (0x5BE670-0x5BEDB5) | FIXED (loop head), field mapping MATCH | 707031435 |
| CTheZones::GetZoneWasVisited 0x572130 (extra) | FIXED | f453a240d |
| CTheZones::SetCurrentZoneAsUnlocked 0x572800 (extra) | MATCH | - |
| CTheZones::Save/Load (extra) | FIXED (unsigned counters) | 6d8c4a1b9 |

## Notes

- LoadWeaponData scanf slot decoding (E0 = esp at the line dispatch): the gun branch's 30 pointers map in port order (type name, fire type, 2 floats, model ids, slot, anim group, u16 ammo/damage, offset xyz, skill, req stat, accuracy, move speed, 2x(start,end,fire) ints, breakout int, flags %x, speed, radius, lifespan, spread). Stores into CWeaponInfo (+0 fire ... +0x68 spread, +0x6c aim offset index) agree with the port header offsets; the `%` branch fills 0xC8A8A8 + (group-11)*0x18 with 4 floats + 4 u16; the melee branch stores numCombos at +0x6f and baseCombo at +0x6e.
- Not changed (valid-data equivalent): the port VERIFYs the scanf counts (exe does not), and looks anim groups up case-insensitively and asserts on a miss (exe compares case-sensitively and keeps the old group / writes out of range on a miss).
- Render: rotation/ring/triangle geometry, render-state order (incl. ZWRITE/ZTEST resets, the cull-none state set once per triangle pair), the x/y role of w/h, 0.8/1.8/0.95/1.05 scales, off-screen arrow association and the 2-player block (shooter selection, dt/4+70 clamp, fade 3 - dt/512) all agree with the exe.
