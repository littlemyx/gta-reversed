# ASM-S29 - Object / CutsceneObject / HandObject asm sweep (slice S29, partial)
Method: exedis vs source, rules v5. 39 functions: 24 MATCH, 10 FIXED, 5 NOT_REACHED (token budget): CHandObject::Render (401-line cap), CObject::ProcessControl (cap), SpecialEntityCalcCollisionSteps, ProcessSamSiteBehaviour, ObjectDamage. Details: S29.result.tsv.

| function | exe addr | fix |
|---|---|---|
| CObject::SpecialEntityPreCollisionStuff | 0x59FFDD, 0x5A008A..0x5A00C3, 0x5A01B1..0x5A0292 | fallen lamp post: a ped returns, a vehicle becomes the ignored entity (port swapped), other types fall through; vehicle branch gated by bbox.max < 1.0 on all axes; grenade below a ped disables the collision |
| CObject::Init | 0x59F887, 0x59F8BC->0x59F8DC | bCanBeAttachedToMagnet always set (reader: Rope.cpp:790); sway-in-wind centre of mass also for models without collision volumes |
| CObject::Explode | 0x5A13E9..0x5A1431 | speed kick ((rand&0xFF)-128)*0.0002f, port used float-range random +-0.0256 |
| CObject::ObjectFireDamage | 0x5A1620, 0x5A16F0 | burn damage 1-health*0.0005f; breaking sets only the static bit (not the virtual SetIsStatic, which also clears bDoorHitEndStop) |
| CObject::TryToFreeUpTempObjects | 0x5A1868 | stops after numObjects freed (port freed every invisible temp object) |
| CObject::DoBurnEffect | 0x59FBC3 | count scale *0.05f instead of /20 |
| CObject::ProcessGarageDoorBehaviour | 0x44A5E0 | plain door z *0.9090909f instead of /1.1f |
| CObject::ProcessTrainCrossingBehaviour | 0x5A0CE7 | angle step *0.005f instead of /200 |
| CCutsceneObject::ProcessControl | 0x5B1C03, 0x5B1C5E | attachment object: UpdateRpHAnim first, clear bWasPostponed afterwards |
| CCutsceneObject::PreRender | 0x5B2021 | blend factor *0.1f instead of /10 |

Consumer audit: Init now sets objectFlags bit 0x40000 as the exe (readers: Rope.cpp:790 magnet pick-up check, script WINCH_CAN_PICK_OBJECT_UP writer) - no layout changes. The other fixes change computation only. Benign UNSURE: Init zeroes m_nCarColor[2..3], the exe leaves them (pool memory).
Gate: Release build of the worktree OK; no oracle target covers these files; soak.sh 3 min NOVSYNC RESULT: OK (0 errors, 44300 frames).
