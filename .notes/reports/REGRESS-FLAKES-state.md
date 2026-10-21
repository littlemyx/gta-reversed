# REGRESS-FLAKES state (stopped at the 100-min cap, 12:30)
CONTINUE: REGRESS-FLAKES-state.md
Landed (one commit): gr_check.py (cops_* only inside the interior, settled snapshots, orig_reference row), baseline.json why, routes/missions.txt (gated dismounts, pre_exit snaps), orig/interior.scn + orig_run.sh + orig_gr_poll.py + gameplay_regress.sh (interior_lspd has a live original leg),
SDLWrapper.cpp (`until:onfoot`), GameplayRegress.cpp (vspd). Needs a Release rebuild of gta_reversed.exe (the route uses `until:onfoot`; an old exe ignores it and just does not gate).
Todo: (a) 3x consecutive full `GR_SKIP=render_firststart gameplay_regress.sh` on a build with these sources (not run); (b) decide the +13 dm at 4 s / the original having no cop-type peds in the LSPD (see report 1-2);
(c) worktree build note: new_worktree.sh --cache has no PCH and RListBuilder.cpp then fails to link (boost::throw_exception unresolved; exceptions off) - I linked with -DCMAKE_EXE_LINKER_FLAGS=/FORCE:UNRESOLVED locally.
