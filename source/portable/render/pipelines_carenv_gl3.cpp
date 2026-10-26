// Stream R, slice R-9 / R-8: see pipelines_carenv_gl3.h. Portable build only.
//
// CPU math -> per-mesh NotsaSaEnvMesh (game side); GL side (this file):
//   vertex shader  = librw default.vert lighting (ambient + RpWorld lights, prelit, material colour; R6-3) +
//                    * v_env: CAR_CAM  (Nv.x*sx + Nv.z*u, Nv.y*sy + Nv.z*v, Nv.z) with Nv the camera space normal (D3D view space == librw view space: x flipped view matrix),
//                                      the fragment divides by z (D3DTTFF_PROJECTED), so s = u + sx*nx/nz, t = v + sy*ny/nz (texture matrix row 3 = {u, v, 1} acts on nz);
//                             CAR_WAVE (tex1.u + offU, (1 - stored v) + offV, 1) (the instance callback stores 1-v: undone to D3D uv space);
//                             BLD_CAM* (nx, ny, 1) (texture transform disabled in the exe: raw normal as uv, WRAP)
//                    * v_spec: per-vertex D3D FF specular of g_GameLight, non-local viewer (view vector (0,0,-1) in view space): H = norm(L + V), (N.H)^power * spec * lightSpec
//                      when N.L > 0 (Gouraud, added after the texture stages, i.e. after the env stage)
//   fragment shader = tex0 * colour, then the env stage (additive / blend), + specular, fog, alpha test.
// The v flip: rasters hold the top row first, so a D3D (s,t) is sampled as GL (s,t) with no flip for the env texture; tex0 follows the shared (u, 1 - stored v) rule.
#ifdef NOTSA_RW_LIBRW
#include "standalone/Fixups.h"
#include "fakerw.h"
#include <src/gl/rwgl3impl.h>
#include <src/gl/rwgl3plg.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "pipelines_carenv_gl3.h"

namespace {
using namespace rw::gl3;
using rw::Atomic;
using rw::Material;
using rw::Texture;
using rw::RGBA;
using rw::int32;
using rw::uint32;

void LogF(const char* fmt, ...) {
    char    buf[512];
    va_list a;
    va_start(a, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);
#ifdef NOTSA_STANDALONE_RUN
    notsa::standalone::Fixups::Log("%s", buf);
#else
    std::fprintf(stderr, "%s\n", buf);
#endif
}

const char* kVertSrc = R"GLSL(
// the three parameter vectors ride on librw's matfx uniforms (registered at engine init, i.e. known to EVERY gl3 shader; a uniform registered later would make
// librw's flushUniforms print "uniform doesn't exist" for each older shader on every draw)
uniform vec4 u_fxparams;   // x: mode (0..4), y: factor, z: spec (0 = off), w: power
uniform vec4 u_envColor;   // sx, sy, u, v  (CAR_CAM)  /  offU, offV (CAR_WAVE)
uniform vec4 u_colorClamp; // xyz: world direction towards the light, w: light specular
#define u_saFx u_fxparams
#define u_saEnv u_envColor
#define u_saLight u_colorClamp

VSIN(ATTRIB_POS)	vec3 in_pos;

VSOUT vec4 v_color;
VSOUT vec2 v_tex0;
VSOUT vec3 v_env;
VSOUT vec3 v_spec;
VSOUT float v_fog;

void
main(void)
{
	vec4 Vertex = u_world * vec4(in_pos, 1.0);
	gl_Position = u_proj * u_view * Vertex;
	vec3 Normal = mat3(u_world) * in_normal;
	bool d3d = u_ambLight.a > 0.5;	// R-12 lighting model (librw gl3: ambLight.alpha), 0 = librw's original (NOTSA_GL3_OLD_LIGHTING)
	if(d3d)
		Normal = NormalizeNormal(Normal);
	vec3 Nv = mat3(u_view) * Normal;	// camera (view) space normal

	v_tex0 = in_tex0;

	// D3D9 FF order (R-12): (prelit + ambient*ka + lights*kd) * material colour, saturated at the end
	vec3 lit = in_color.rgb;
	lit += u_ambLight.rgb*surfAmbient;
	lit += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse;
	vec3 addAfter = vec3(0.0, 0.0, 0.0);
	if(u_saFx.z > 0.0){
		// g_GameLight is also an enabled D3D light: diffuse 0.25 and ambient 0.75 (CCustomCarEnvMapPipeline::PreRenderUpdate).
		// Its ambient term multiplies the AMBIENT material source: prelit + modulate (u_surfProps.w = 1): the vertex colour (not scaled by the material
		// colour), otherwise the material ambient (ambient light * ka, scaled by the material colour like the rest).
		if(d3d && u_surfProps.w > 0.5)
			addAfter = in_color.rgb*0.75;
		else
			lit += u_ambLight.rgb*surfAmbient*0.75;
		lit += vec3(0.25*max(dot(normalize(Normal), normalize(u_saLight.xyz)), 0.0))*surfDiffuse;
	}
	if(d3d){
		v_color.rgb = clamp(lit*u_matColor.rgb + addAfter, 0.0, 1.0);
		v_color.a = clamp(in_color.a, 0.0, 1.0)*u_matColor.a;
	}else
		v_color = clamp(vec4(lit, in_color.a), 0.0, 1.0)*u_matColor;

	int mode = int(u_saFx.x + 0.5);
	v_env = vec3(0.0, 0.0, 1.0);
	if(mode == 1)
		v_env = vec3(Nv.x*u_saEnv.x + Nv.z*u_saEnv.z, Nv.y*u_saEnv.y + Nv.z*u_saEnv.w, Nv.z);
	else if(mode == 2)
		v_env = vec3(in_tex1.x + u_saEnv.x, (1.0 - in_tex1.y) + u_saEnv.y, 1.0);
	else if(mode >= 3)
		v_env = vec3(Nv.x, Nv.y, 1.0);

	v_spec = vec3(0.0, 0.0, 0.0);
	if(u_saFx.z > 0.0){
		vec3 N = normalize(Nv);
		vec3 L = normalize(mat3(u_view) * u_saLight.xyz);
		if(dot(N, L) > 0.0){
			vec3 H = normalize(L + vec3(0.0, 0.0, -1.0));
			float s = pow(max(dot(N, H), 0.0), u_saFx.w);
			v_spec = vec3(s*u_saFx.z*u_saLight.w);
		}
	}

	v_fog = DoFog(gl_Position.w);
}
)GLSL";

const char* kFragSrc = R"GLSL(
uniform sampler2D tex0;
uniform sampler2D tex1;
uniform vec4 u_fxparams;
#define u_saFx u_fxparams

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN vec3 v_env;
FSIN vec3 v_spec;
FSIN float v_fog;

void
main(void)
{
	vec4 color = v_color*texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	int mode = int(u_saFx.x + 0.5);
	if(mode == 1){
		color.rgb += texture(tex1, v_env.xy / v_env.z).rgb * u_saFx.y;
	}else if(mode == 2){
		color.rgb = mix(color.rgb, texture(tex1, v_env.xy).rgb, u_saFx.y);
	}else if(mode == 3){
		color.rgb += texture(tex1, v_env.xy).rgb * u_saFx.y;
	}else if(mode == 4){
		color.rgb += texture(tex1, v_env.xy).rgb * v_color.rgb;
	}
	color.rgb = clamp(color.rgb, 0.0, 1.0);
	color.rgb = clamp(color.rgb + v_spec, 0.0, 1.0);
	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);
	DoAlphaTest(color.a);
	FRAGCOLOR(color);
}
)GLSL";

Shader* g_shader[4]; // [fullLight*2 + noAT]
int32   u_saFx = -1, u_saEnv = -1, u_saLight = -1;
bool    g_shaderFailed = false;
uint32_t g_draws = 0;

bool EnsureShaders() {
    if (g_shader[0]) {
        return true;
    }
    if (g_shaderFailed) {
        return false;
    }
    u_saFx    = findUniform("u_fxparams");
    u_saEnv   = findUniform("u_envColor");
    u_saLight = findUniform("u_colorClamp");
    if (u_saFx < 0 || u_saEnv < 0 || u_saLight < 0) {
        LogF("R-9: librw's matfx uniforms are not registered (MatFX plugin missing?), no SA env shader");
        g_shaderFailed = true;
        return false;
    }
    const char* lights = "#define DIRECTIONALS\n#define POINTLIGHTS\n#define SPOTLIGHTS\n";
    const char* vs[]            = {shaderDecl, header_vert_src, kVertSrc, nullptr};
    const char* vs_fullLight[]  = {shaderDecl, lights, header_vert_src, kVertSrc, nullptr};
    const char* fs[]            = {shaderDecl, header_frag_src, kFragSrc, nullptr};
    const char* fs_noAT[]       = {shaderDecl, "#define NO_ALPHATEST\n", header_frag_src, kFragSrc, nullptr};
    g_shader[0] = Shader::create(vs, fs);
    g_shader[1] = Shader::create(vs, fs_noAT);
    g_shader[2] = Shader::create(vs_fullLight, fs);
    g_shader[3] = Shader::create(vs_fullLight, fs_noAT);
    if (!g_shader[0] || !g_shader[1] || !g_shader[2] || !g_shader[3]) {
        LogF("R-9: SA env shader failed to build, env / specular meshes fall back to the default pipeline");
        NotsaGl3SaEnvShutdown();
        g_shaderFailed = true;
        return false;
    }
    LogF("R-9: SA env/specular shader built (uniforms %d %d %d of %d)", u_saFx, u_saEnv, u_saLight, uniformRegistry.numUniforms);
    return true;
}

// librw's ObjPipeline has no user pointer; one pipeline object per (prepare, plugin id) pair is enough (car, building, building DN) -> a tiny table
struct PipeEntry {
    rw::ObjPipeline* pipe;
    NotsaSaEnvPrepareFn prepare;
};
PipeEntry g_pipes[8];
int       g_numPipes = 0;

NotsaSaEnvPrepareFn FindPrepare(const rw::ObjPipeline* p) {
    for (int i = 0; i < g_numPipes; i++) {
        if (g_pipes[i].pipe == p) {
            return g_pipes[i].prepare;
        }
    }
    return nullptr;
}

// R-12 diagnostics: NOTSA_R12_LIGHTLOG=1 logs the lights an SA env atomic is drawn with (every 300th mesh, at most 60 lines)
void LogLightsOnce(Atomic* atomic, Material* m, uint32 flags) {
    static const bool on = std::getenv("NOTSA_R12_LIGHTLOG") != nullptr;
    static int        counter = 0, lines = 0;
    if (!on || lines >= 60 || (counter++ % 300) != 0) {
        return;
    }
    rw::WorldLights ld;
    rw::Light*      dirs[8];
    rw::Light*      locs[8];
    ld.directionals    = dirs;
    ld.numDirectionals = 8;
    ld.locals          = locs;
    ld.numLocals       = 8;
    ((rw::World*)rw::engine->currentWorld)->enumerateLights(atomic, &ld);
    std::string d;
    char        b[160];
    for (int i = 0; i < ld.numDirectionals; i++) {
        std::snprintf(b, sizeof(b), " dir%d=(%.2f,%.2f,%.2f)", i, dirs[i]->color.red, dirs[i]->color.green, dirs[i]->color.blue);
        d += b;
    }
    lines++;
    LogF("R-12 light: flags=0x%x amb=(%.2f,%.2f,%.2f) n_amb=%d%s | mat=(%d,%d,%d,%d) ka=%.2f kd=%.2f", flags, ld.ambient.red, ld.ambient.green, ld.ambient.blue, ld.numAmbients, d.c_str(), m->color.red,
         m->color.green, m->color.blue, m->color.alpha, m->surfaceProps.ambient, m->surfaceProps.diffuse);
}

// INT1 diagnostics: NOTSA_INT1_LOG=1 logs every mesh drawn of the geometry with that many triangles (interior shell: 2221), <= 120 lines
void LogShellMesh(Atomic* atomic, Material* m, InstanceData* inst, int32 vsBits, uint32 flags) {
    static const char* on    = std::getenv("NOTSA_INT1_LOG"); // = triangle count of the geometry to log
    static int         lines = 0;
    if (!on || lines >= 120 || atomic->geometry->numTriangles != std::atoi(on)) {
        return;
    }
    lines++;
    {
        rw::WorldLights ld;
        rw::Light*      dirs[8];
        rw::Light*      locs[8];
        ld.directionals    = dirs;
        ld.numDirectionals = 8;
        ld.locals          = locs;
        ld.numLocals       = 8;
        ((rw::World*)rw::engine->currentWorld)->enumerateLights(atomic, &ld);
        LogF("INT1 lights: ambient=(%.3f,%.3f,%.3f) numAmbients %d numDirectionals %d numLocals %d", ld.ambient.red, ld.ambient.green, ld.ambient.blue, ld.numAmbients, ld.numDirectionals, ld.numLocals);
    }
    rw::Geometry* g = atomic->geometry;
    long sum = 0;
    for (int i = 0; i < g->numVertices && g->colors; i++) {
        sum += g->colors[i].red + g->colors[i].green + g->colors[i].blue;
    }
    LogF("INT1 mesh: geo flags 0x%x tris %d vsBits 0x%x curLightBits 0x%x mat col (%d,%d,%d,%d) tex %s raster %dx%d alpha %d | inst idx %u vtxAlpha %d | atTest %d | prelit sum/vert %.1f", flags,
         g->numTriangles, vsBits, rw::gl3::curLightBits, m->color.red, m->color.green, m->color.blue,
         m->color.alpha, m->texture ? m->texture->name : "(none)", m->texture && m->texture->raster ? m->texture->raster->width : 0, m->texture && m->texture->raster ? m->texture->raster->height : 0,
         m->texture && m->texture->raster ? 1 : 0, (unsigned)inst->numIndex, (int)inst->vertexAlpha, (int)getAlphaTest(), g->numVertices ? (double)sum / g->numVertices : 0.0);
}

void SaEnvRenderCB(Atomic* atomic, InstanceDataHeader* header) {
    NotsaSaEnvPrepareFn prepare = FindPrepare(atomic->pipeline);
    const uint32 flags = atomic->geometry->flags;
    setWorldMatrix(atomic->getFrame()->getLTM());
    const int32 vsBits = lightingCB(atomic);

    setupVertexInput(header);
    InstanceData* inst = header->inst;
    int32 n = header->numMeshes;
    while (n--) {
        Material* m = inst->material;
        NotsaSaEnvMesh mesh;
        if (prepare) {
            prepare(atomic, m, mesh);
        }
        const bool useFx = (mesh.mode != NOTSA_SAENV_NONE && mesh.envTex && mesh.envTex->raster) || mesh.spec > 0.f;
        const bool haveShader = useFx && EnsureShaders();

        LogLightsOnce(atomic, m, flags);
        LogShellMesh(atomic, m, inst, vsBits, flags);
        RGBA col = m->color;
        if (mesh.blackColor) {
            col.red = col.green = col.blue = 0;
        }
        // exe 0x5DA7A9: the material colour only modulates when the geometry has MODULATE and the colour is not white; with prelit colours the
        // ambient term of the extra D3D light (g_GameLight) then multiplies the vertex colour instead of the material ambient (u_surfProps.w)
        const bool white      = col.red == 255 && col.green == 255 && col.blue == 255 && col.alpha == 255;
        const bool prelitMod  = (flags & rw::Geometry::PRELIT) && (flags & rw::Geometry::MODULATE) && !white;
        setMaterial(flags, col, m->surfaceProps, prelitMod ? 1.0f : 0.0f);
        setTexture(0, m->texture);
        rw::SetRenderState(rw::VERTEXALPHA, inst->vertexAlpha || m->color.alpha != 0xFF);

        const bool fullLight = (vsBits & VSLIGHT_MASK) != 0;
        if (haveShader) {
            float fx[4]    = {0.f, mesh.factor, mesh.spec, mesh.power};
            float env[4]   = {mesh.env[0], mesh.env[1], mesh.env[2], mesh.env[3]};
            float light[4] = {mesh.lightDir[0], mesh.lightDir[1], mesh.lightDir[2], mesh.lightSpec};
            if (mesh.mode != NOTSA_SAENV_NONE && mesh.envTex && mesh.envTex->raster) {
                fx[0] = (float)mesh.mode;
                mesh.envTex->setAddressU(Texture::WRAP);
                mesh.envTex->setAddressV(Texture::WRAP);
                setTexture(1, mesh.envTex);
            }
            setUniform(u_saFx, fx);
            setUniform(u_saEnv, env);
            setUniform(u_saLight, light);
            g_shader[(fullLight ? 2 : 0) + (getAlphaTest() ? 0 : 1)]->use();
            g_draws++;
        } else if (!fullLight) {
            (getAlphaTest() ? defaultShader : defaultShader_noAT)->use();
        } else {
            (getAlphaTest() ? defaultShader_fullLight : defaultShader_fullLight_noAT)->use();
        }
        {
            static const char* q = std::getenv("NOTSA_INT1_LOG");
            static int         qlines = 0;
            GLint vpq[4] = {};
            if (q) glGetIntegerv(GL_VIEWPORT, vpq);
            if (q && atomic->geometry->numTriangles == std::atoi(q) && qlines < 60) { // INT1: samples that pass depth test + the colour at the centre of the screen after the draw
                {
                    float dz[5] = {};
                    const int px[5][2] = { {640, 360}, {320, 180}, {960, 540}, {100, 600}, {1200, 100} };
                    for (int k = 0; k < 5; k++) glReadPixels(px[k][0], px[k][1], 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &dz[k]);
                    GLint fb = 0;
                    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb);
                    LogF("INT1 depth before draw: centre %.6f (320,180) %.6f (960,540) %.6f (100,600) %.6f (1200,100) %.6f; fbo %d", dz[0], dz[1], dz[2], dz[3], dz[4], fb);
                }
                GLuint id = 0;
                glGenQueries(1, &id);
                glBeginQuery(GL_SAMPLES_PASSED, id);
                drawInst(header, inst);
                glEndQuery(GL_SAMPLES_PASSED);
                GLuint n = 0;
                glGetQueryObjectuiv(id, GL_QUERY_RESULT, &n);
                glDeleteQueries(1, &id);
                GLint vp[4];
                glGetIntegerv(GL_VIEWPORT, vp);
                GLboolean cm[4];
                glGetBooleanv(GL_COLOR_WRITEMASK, cm);
                GLboolean zt = glIsEnabled(GL_DEPTH_TEST), bl = glIsEnabled(GL_BLEND), cf = glIsEnabled(GL_CULL_FACE);
                GLint cfm = 0, zf = 0;
                glGetIntegerv(GL_CULL_FACE_MODE, &cfm);
                glGetIntegerv(GL_DEPTH_FUNC, &zf);
                qlines++;
                {
                    const rw::Matrix* ltm = atomic->getFrame()->getLTM();
                    const float* bs = &atomic->geometry->morphTargets[0].boundingSphere.center.x;
                    LogF("INT1 xform: LTM pos (%.1f,%.1f,%.1f) right (%.2f,%.2f,%.2f) up (%.2f,%.2f,%.2f) at (%.2f,%.2f,%.2f) flags 0x%x; bsphere centre (%.1f,%.1f,%.1f) r %.1f; atomic flags 0x%x", ltm->pos.x, ltm->pos.y, ltm->pos.z, ltm->right.x, ltm->right.y, ltm->right.z, ltm->up.x, ltm->up.y, ltm->up.z, ltm->at.x, ltm->at.y, ltm->at.z, (unsigned)ltm->flags, bs[0], bs[1], bs[2], atomic->geometry->morphTargets[0].boundingSphere.radius, (unsigned)atomic->object.object.flags);
                    rw::Camera* cam = rw::engine->currentCamera;
                    const rw::Matrix* cl = cam->getFrame()->getLTM();
                    LogF("INT1 camera: pos (%.1f,%.1f,%.1f) at (%.2f,%.2f,%.2f) near %.3f far %.1f", cl->pos.x, cl->pos.y, cl->pos.z, cl->at.x, cl->at.y, cl->at.z, cam->nearPlane, cam->farPlane);
                }
                LogF("INT1 draw: samples passed %u viewport (%d,%d,%d,%d) colorMask %d%d%d%d depthTest %d blend %d cull %d mode 0x%x depthFunc 0x%x", n, vp[0], vp[1], vp[2], vp[3], cm[0], cm[1], cm[2], cm[3], (int)zt, (int)bl, (int)cf, cfm, zf);
                inst++;
                continue;
            }
        }
        drawInst(header, inst);
        inst++;
    }
    teardownVertexInput(header);
}
} // namespace

rw::ObjPipeline* NotsaGl3CreateSaEnvPipeline(NotsaSaEnvPrepareFn prepare, uint32_t pluginId) {
    if (g_numPipes >= (int)(sizeof(g_pipes) / sizeof(g_pipes[0]))) {
        LogF("R-9: too many SA env pipelines");
        return nullptr;
    }
    rw::gl3::ObjPipeline* pipe = rw::gl3::ObjPipeline::create();
    pipe->instanceCB   = defaultInstanceCB;
    pipe->uninstanceCB = defaultUninstanceCB;
    pipe->renderCB     = SaEnvRenderCB;
    pipe->pluginID     = pluginId;
    pipe->pluginData   = pluginId;
    g_pipes[g_numPipes++] = {pipe, prepare};
    return pipe;
}

void NotsaGl3SaEnvShutdown() {
    for (auto& s : g_shader) {
        if (s) {
            s->destroy();
            s = nullptr;
        }
    }
}

uint32_t NotsaGl3SaEnvDrawCount() { return g_draws; }
#endif
