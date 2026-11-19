# ASM-RVZ4: review of latest HC fixes
Oracle: gta_sa_compact.exe 1.0. See RVZ4.result.tsv.
- WRONG: 2bdffac12 IsPedStopped (NaN in a vehicle = not stopped; fixed in 46d1f1de3). 9da7b7915 was already reverted by HC3e (5c7b187c0; planes[3].y verified).
- CONFIRMED: 387be1240, 48e900630 (FindMaxSteerAngle + steer clamp; CarAI.cpp:1336 float clamp is right, exe stores to float), c7dc7da60, 7b07f830c; 04adfa255 partial (entity distance + push-out).
- NOT_REACHED: 239e17beb CameraColDetAndReact.
