# ASM-S06c (CTaskComplexPartner finisher + S06b open item) - 2026-10-28
Result: 3 checked (3 MATCH, 0 fixed, 0 UNSURE). Table in S06c.result.tsv.
- CTaskComplexPartner::CreateNextSubTask (0x683AD0-0x6840BE): all 6 states, partner-task lookup, wander reset (default task type/WanderType check, +100000 ms scan time), strict/NaN tests for the 0.99/1.01 and 0.02 limits, GoToPoint ctor args, meeting-point/first-to-target flag stores: MATCH.
- CTaskComplexPartner::ControlSubTask (0x6840D0-0x68418D): abort condition (state<=1 with partner and not completed; partner task points back at ped), go-to-point frame limit 150, DropEntity, StreamRequiredAnims, AbortLookAt 500: MATCH.
- S06b open item: CTaskComplexPartnerGreet::GetPartnerSequence chat task. The exe allocates 0x34 bytes, calls CTaskSimpleRunTimedAnim::Constructor(0, 0xC, 4.0, -4.0, duration, 0x1A0, "Chat", 0) and sets vptr 0x86C78C, i.e. an inlined CTaskSimpleChat ctor (identical to 0x5F65A0, used for the extra chat). The port's CTaskSimpleChat(duration) has the same args and vptr; durations d2 (lead branch) and d4 (extra) match: MATCH.
No code changed.
