import sys,shutil,os,re
R="/Users/Andrei.Mukhin/github/recomp/gta-reversed/source/"
S=os.path.dirname(os.path.abspath(__file__))
files={"app/app_game.cpp":"../standalone/FrameProf.h","game_sa/Streaming.cpp":"../standalone/FrameProf.h","game_sa/CdStreamInfo.cpp":"../standalone/FrameProf.h","standalone/rw/camera.cpp":"../FrameProf.h","standalone/rw/raster.cpp":"../FrameProf.h","standalone/rw/texdict.cpp":"../FrameProf.h"}
if sys.argv[1]=="apply":
    shutil.copy(S+"/FrameProf.h",R+"standalone/FrameProf.h")
    for f,inc in files.items():
        shutil.copy(R+f,S+"/orig_"+f.replace("/","_"))
        t=open(R+f).read()
        # include after the first #include line
        i=t.index("#include"); j=t.index("\n",i)+1
        t=t[:j]+'#include "%s"\n'%inc+t[j:]
        def sub(old,new,count=1):
            global t
            assert old in t,(f,old)
            t=t.replace(old,new,count)
        if f=="app/app_game.cpp":
            sub("void Idle(void* param) {\n    ZoneScoped;\n","void Idle(void* param) {\n    ZoneScoped;\n    FPROF_IDLE();\n")
            sub("void FrontendIdle() {\n    ZoneScoped;\n","void FrontendIdle() {\n    ZoneScoped;\n    FPROF_IDLE();\n")
            sub("    CGame::Process();\n    AudioEngine.Service();\n","    { FPROF(P_CGAMEPROC); CGame::Process(); }\n    { FPROF(P_AUDIO); AudioEngine.Service(); }\n")
            sub("    AudioEngine.Service();\n    CameraSize(Scene.m_pRwCamera, nullptr, SCREEN_VIEW_WINDOW, SCREEN_ASPECT_RATIO);\n    CVisibilityPlugins::SetRenderWareCamera(Scene.m_pRwCamera);\n\n    if (g_FastLoaderConfig","    { FPROF(P_AUDIO); AudioEngine.Service(); }\n    CameraSize(Scene.m_pRwCamera, nullptr, SCREEN_VIEW_WINDOW, SCREEN_ASPECT_RATIO);\n    CVisibilityPlugins::SetRenderWareCamera(Scene.m_pRwCamera);\n\n    if (g_FastLoaderConfig")
        if f=="game_sa/Streaming.cpp":
            sub("void CStreaming::Update() {\n    ZoneScoped;\n","void CStreaming::Update() {\n    ZoneScoped;\n    FPROF(P_STREAM_UPD);\n")
            sub("void CStreaming::LoadAllRequestedModels(bool bOnlyPriorityRequests) {\n    ZoneScoped;\n","void CStreaming::LoadAllRequestedModels(bool bOnlyPriorityRequests) {\n    ZoneScoped;\n    FPROF(P_STREAM_LOADALL);\n")
            sub("bool CStreaming::ConvertBufferToObject(uint8* fileBuffer, int32 modelId) {\n","bool CStreaming::ConvertBufferToObject(uint8* fileBuffer, int32 modelId) {\n    FPROF(P_CONVERT);\n")
        if f=="game_sa/CdStreamInfo.cpp":
            sub("eCdStreamStatus CdStreamSync(int32 streamId) {\n","eCdStreamStatus CdStreamSync(int32 streamId) {\n    FPROF(P_CDSYNC);\n")
            sub("bool CdStreamRead(int32 streamId, void* lpBuffer, CdStreamPos pos, int32 sectorCount) {\n","bool CdStreamRead(int32 streamId, void* lpBuffer, CdStreamPos pos, int32 sectorCount) {\n    FPROF(P_CDREAD);\n")
        if f=="standalone/rw/camera.cpp":
            sub("    camera->showRaster(flags);\n    return camera;","    { FPROF(P_PRESENT); camera->showRaster(flags); }\n    return camera;")
        if f=="standalone/rw/raster.cpp":
            sub("RwRaster* RwRasterUnlock(RwRaster* raster) {\n","RwRaster* RwRasterUnlock(RwRaster* raster) {\n    FPROF(P_RASTER);\n")
        if f=="standalone/rw/texdict.cpp":
            sub("RwTexDictionary* RwTexDictionaryStreamRead(RwStream* stream) {\n","RwTexDictionary* RwTexDictionaryStreamRead(RwStream* stream) {\n    FPROF(P_TXD);\n")
        open(R+f,"w").write(t)
    print("applied")
else:
    for f in files:
        shutil.copy(S+"/orig_"+f.replace("/","_"),R+f)
    os.remove(R+"standalone/FrameProf.h"); print("restored")
