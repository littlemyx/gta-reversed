# ASM-Z6: script command handlers used by story missions (no verdict yet)

Method: T3_SCM_USAGE opcodes -> exe case address (jump tables via gen_argcounts logic), minus cases ported from the exe (Commands/Ported) and minus existing verdict rows; 232 hand-written handlers remained. Compared ~30 (most used first); see Z6.result.tsv.

- FIXED: COMMAND_SCRIPT_NAME (0x3A4, exe 0x4846A1): the exe lowercases the label; the port built a lowered buffer but stored the original name (m_szName compared with "intro1" in PedDamageResponseCalculator).
- MATCH: variable set/add/sub/mul/div and int/float compare opcodes (NaN false), SET_CHAR_HEADING, SET_PLAYER_CONTROL, SET_CHAR_COORDINATES, CLEAR_AREA, PRINT_BIG/HELP, MARK_MODEL_AS_NO_LONGER_NEEDED, decision maker load/set, TASK_KILL_CHAR_ON_FOOT, TASK_TURN_CHAR_TO_FACE_CHAR, DELETE_OBJECT, SWITCH_ROADS_*, others.
- CCamera::TakeControlAttachToEntity: tail (0x50CA81-0x50CAD7) MATCH, whole function now MATCH.
- NOT_REACHED: remaining ~200 candidates (CREATE_CAR/CHAR, camera setters, sequence tasks, text labels, START_NEW_SCRIPT, LOAD_SCENE).
