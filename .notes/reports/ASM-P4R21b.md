# ASM-P4R21b: core math continued (cBuoyancy first)
Compared 8 of 32 rows: cBuoyancy PreCalcSetup / SimpleSumBuoyancyData / FindWaterLevelNorm / SimpleCalcBuoyancy / ProcessBuoyancyBoat MATCH; CMatrix operator+ MATCH, operator* (9 of 12 cells) MATCH; AddSplashParticles FIXED (6ec7eaba0): magnitudes summed z,y,x with float rounding, speed clamp x*(1/s)*0.1 (two roundings), velocity modifier z = -fMoveSpeed*120 (scalar, not speed.z), position = from + (to-from)*t (port used Lerp (1-t)*a+t*b), NaN speed returns.
Not reached (budget): CQuaternion, Reorthogonalise, CPlaceable, CEntity bound/transform, CVector Difference/FromMultiply3x3, CPathNode::GetPosition, the 4 x87 re-compares. See P4R21b.result.tsv.
Build: StandaloneRelease links (Buoyancy.cpp). No test/game runs.
