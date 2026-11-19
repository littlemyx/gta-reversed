# ASM-Z8: script command handlers with no asm verdict, continuation of Z6

Compared 22 handlers/cases against the exe (see Z8.result.tsv): CREATE_CHAR and CREATE_CAR tails, PERFORM_SEQUENCE_TASK tail, SET_(L)VAR_TEXT_LABEL (8 and 16), START_NEW_SCRIPT, TERMINATE_THIS_SCRIPT, RETURN, REQUEST_MODEL, MARK_CAR_AS_NO_LONGER_NEEDED, five ped-flag setters, TASK_ENTER_CAR_AS_PASSENGER / LEAVE_CAR / LEAVE_ANY_CAR, SWITCH_CAR_GENERATOR, SET_SEQUENCE_TO_REPEAT, EVERYONE/POLICE_IGNORE_PLAYER.
Result: all MATCH, no code change. Notes: negative START_NEW_SCRIPT offset is UB in the exe (port asserts); SET_SEQUENCE_TO_REPEAT stores a raw int in the exe, the port a bool (scripts pass 0/1).
Not reached: ~170 remaining candidates (misc TASK_*, zone/group setters, roads-back-to-original, template var ops).
