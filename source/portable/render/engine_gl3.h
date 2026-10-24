#pragma once
// Stream R / R-3a: GL context facts of the portable engine twin (engine_gl3.cpp). Portable build only.

struct NotsaGlInfo {
    char  version[128], renderer[128], vendor[128], glsl[64];
    int   major, minor;
    bool  core;
    int   redBits, alphaBits, depthBits, stencilBits;    // default framebuffer as the driver gave it
    int   sdlDepthBits, sdlStencilBits;                  // what SDL_GL_GetAttribute reports
    bool  s3tc;
    float maxAnisotropy;
    int   maxSamples;
};

//! Fills `out` from the current GL context; false when there is none.
bool NotsaRwGetGlInfo(NotsaGlInfo* out);
