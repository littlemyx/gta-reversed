# ASM-RVZ5: review of RVZ4 leftovers
Oracle gta_sa_compact.exe 1.0. See RVZ5.result.tsv.
- 239e17beb CameraColDetAndReact: PARTIAL -> fixed one deviation (radius z^2 = unrounded z diff * float z diff, e9868852b); rest CONFIRMED.
- 04adfa255 ComputeClearTarget: CONFIRMED whole function (step dot z,y,x, break on dot<0/NaN, maxIterations 14).
- 48e900630 SteerAICarWithPhysicsFollowPath: tail 0x435452-0x435799 CONFIRMED; 0x434900-0x435452 NOT_REACHED (budget).
- 550b29e6d SH6 debug logs: CLEAN (all under NOTSA_STATEHASH; log-off and non-STATEHASH paths unchanged).
