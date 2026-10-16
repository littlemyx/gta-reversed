# Tool edits by D-B07 / D-B08 (append; re-read before editing a tool; one small commit per tool change)

| who | tool | change | commit |
|---|---|---|---|
| D-B08 | codemod_globals.py `--check-converted` | aliases.json extents of kind "shrink" (VehicleGunOffset 168->156, col1 96->48) now REPLACE the table size (were max()-ed: the shrunk-away tail floats 0x8D3674/78 were reported as strays) | see git log |
