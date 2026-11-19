# ASM-P4R3b
Fast pass over Ported/Group*.cpp story-path script handlers (hottest first by executed count, rows 1-94 of the unverified list; 459 were unverdicted before the pass).

Result: 94 handlers compared against the exe case bodies, 94 MATCH, 0 fixes, no source change, no build needed.
Checked: param load kinds (int vs float), *50 / deg-rad constants, strict vs non-strict compares (test ah,5 / 0x41 + jp/jnp), -1 handle handling, flag bit offsets, push orders, store-parameter shapes.
Partially read (tail of long case not read): IS_CHAR_HOLDING_OBJECT, CREATE_CHAR, SET_CAR_WATERTIGHT, ADD_BLIP_FOR_COORD, DRAW_SPHERE, HAS_OBJECT_OF_TYPE_BEEN_SMASHED, CREATE_CAR, IS_CAR_TYRE_BURST, GET_RANDOM_CHAR_IN_SPHERE_ONLY_DRUGS_BUYERS, PERFORM_SEQUENCE_TASK, GIVE_WEAPON_TO_CHAR, IS_GROUP_MEMBER.
Left: 365 handlers (executed <= ~130 times per all-missions run).
