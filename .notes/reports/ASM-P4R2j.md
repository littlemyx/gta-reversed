# ASM-P4R2j
Part A: enex EnterAngle (+0x10) / ExitAngle (+0x28) offsets match exe. Part B: Association (Start/SetBlend/SetCurrentTime/Sync/UpdateTime/UpdateBlend/UpdateTimeStep), Node::NextKeyFrame/Update, GetFirstAssociation x2 MATCH.
Leftover rows (SetTask, SetMaximumWantedLevel, TakeControl, CloseAllDoors, RemoveAllTrains, FindMaxNumberOfGroupMembers, RemoveBuildingsNotInArea) all MATCH. 0 fixes, no source change, no build.
Open: FrameUpdateCallBack* variants (NOT_REACHED), SetupKeyFrameCompressed/CalcDeltas UNSURE. See P4R2j.result.tsv.
