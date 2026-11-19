# ASM-P4R3c
Fast pass over Ported/Group*.cpp story-path handlers, rows 95-151 of the P4R3b unverified list (hottest first) plus 159, 163, 164, 166, 189, 190, 202, 229, 240.

Result: 66 handlers compared against the exe case bodies, 66 MATCH, 0 fixes, no source change, no build needed.
Checked: param load kinds (fild vs fld), *0.0174 / *0.01667 constants, strict vs non-strict compares, -1/-2 handle/time handling, flag bit offsets and masks, CWorld::Remove/Add around flag writes, push orders and constant trailing args (SetCharCoordinates 0,1).
Not covered (budget): the 12 head-only rows of P4R3b (IS_CHAR_HOLDING_OBJECT, CREATE_CHAR, SET_CAR_WATERTIGHT, ADD_BLIP_FOR_COORD, DRAW_SPHERE, HAS_OBJECT_OF_TYPE_BEEN_SMASHED, CREATE_CAR, IS_CAR_TYRE_BURST, ..._DRUGS_BUYERS, PERFORM_SEQUENCE_TASK, GIVE_WEAPON_TO_CHAR, IS_GROUP_MEMBER), rows 152-158, 160-162, 165, 167-188, 191-201, 203-228, 230-239, 241+ (about 295 handlers left). Long handlers (SET_GROUP_MEMBER, APPLY_FORCE_TO_CAR, AddDecisionMakerEventResponse, ATTACH_OBJECT_TO_CAR) were compared only down to the main flow/arg order.
