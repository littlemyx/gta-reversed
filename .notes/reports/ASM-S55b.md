# ASM-S55b (CPickups::RenderPickUpText, CPickups::DoPickUpEffects) - 2026-10-29
Result: 2 rows: 1 FIXED, 1 MATCH. Table: S55b.result.tsv.
Fix: e923f5996 RenderPickUpText (0x455000): a message with a text but without the number flag (bit 2, property pickups) is printed from message.text itself - the port always printed the local buffer (empty or the previous message's text); the scale cap is the INTEGER maximumWidth/640 (idiv) - the port used the float SCREEN_STRETCH_X(1.0f).
MATCH: DoPickUpEffects (0x455720-0x455E14): camera corona (hours<5 || vision FX, alpha/G/B, size, 100.0 far clip, shared stack-slot id = documented NOTSA deviation), model->weapon table (all 12 model globals mapped), message distance sqrt(dy2+dx2)<14 strict, 16-message cap, colour/alpha/price/flags/text arms, bbox scale (max of dims, 1.2/max, floor 1, 0.6 blend, model 0x16A -> 1.2), matrix stores.
Tests: same run as ASM-S56 (oracle 0/0, combat regress PASS=14, PortableWin64 compile clean).
