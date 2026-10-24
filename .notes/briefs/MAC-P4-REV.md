# MAC-P4-REV — line-by-line review of MAC-P4 (P-4a..g)

Host: Apple Silicon Mac. Rules `.notes/agent_rules/v4.md` (Windows-only parts do not apply). Repo
/Users/Andrei.Mukhin/github/recomp/gta-reversed, branch `portable/main` (remote `fork`). Main checkout; parallel agent MAC-P2k (tests,
CTest wiring in the portable CMake branch, FileRecord guard, Replay.h/WaterLevel.h) — never touch its files/uncommitted edits.
`.notes/` gitignored (`git add -f`). Wall-clock cap 80 min.

## What
`git log --oneline 98a8a26b~1..2be44cad` (84 files, +172/-156). Report .notes/reports/MAC-P4.md, plan .notes/P4_PLAN.md.
Check EVERY hunk:
1. MSVC semantics unchanged: same types on x86 (`size_t` == `unsigned int`, `uintptr_t`), explicit template args pick the same
   overload/type the implicit call picked on MSVC (watch int vs float vs uint8 promotions in min/max/GetRandomNumberInRange — a wrong
   explicit arg silently changes clamping), `NOTSA_PTR_ID32(p)` expands to the original expression on x86.
2. 64-bit semantics honest: where `NOTSA_PTR_ID32` truncates a pointer to 32 bits on arm64, is the id only compared/hashed (OK) or ever
   converted back to a pointer (BUG)? List each of the 58 sites with verdict. Same for `(void*)(uintptr_t)x` round trips.
3. No logic edits beyond the class recipe; line counts.
4. Re-run wine cl.exe A/B on a sample of 12 TUs not in MAC-P4's 71 list if any touched TU was missed; otherwise sample 12 of them.
Fix clear mistakes (one commit, `git commit -- <files>`, trailer exactly `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`, push;
never force; never push reverse/interior_c). Debatable: list.

## Report
`.notes/reports/MAC-P4-REV.md` (per-site table for NOTSA_PTR_ID32 and uintptr_t round trips; findings). Return ≤ 15 lines. Leave no
background tasks running.
