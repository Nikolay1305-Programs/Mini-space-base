#define SDL_MAIN_HANDLED

#include <GL/glew.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdint>

#include "Model.h"


static float Clamp(float v, float lo, float hi) { return std::clamp(v, lo, hi); }

static GLuint CompileShader(GLenum type, const char* src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        std::string log((size_t)len, '\0');
        glGetShaderInfoLog(sh, len, nullptr, log.data());
        std::cout << "Shader compile error:\n" << log << "\n";
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static GLuint CreateProgram(const char* vsSrc, const char* fsSrc)
{
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log((size_t)len, '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cout << "Program link error:\n" << log << "\n";
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

struct Camera {
    glm::vec3 pos{ 0.0f, 1.8f, 6.0f };
    float yaw = -90.0f;
    float pitch = 0.0f;

    glm::vec3 Forward() const {
        float cy = std::cos(glm::radians(yaw));
        float sy = std::sin(glm::radians(yaw));
        float cp = std::cos(glm::radians(pitch));
        float sp = std::sin(glm::radians(pitch));
        return glm::normalize(glm::vec3(cy * cp, sp, sy * cp));
    }
    glm::vec3 Right() const {
        return glm::normalize(glm::cross(Forward(), glm::vec3(0, 1, 0)));
    }
    glm::mat4 View() const {
        return glm::lookAt(pos, pos + Forward(), glm::vec3(0, 1, 0));
    }
};

static bool RayAABB(const glm::vec3& ro, const glm::vec3& rd,
    const glm::vec3& bmin, const glm::vec3& bmax, float& outT)
{
    float tmin = 0.0f;
    float tmax = 100000.0f;

    for (int i = 0; i < 3; ++i) {
        float origin = ro[i];
        float dir = rd[i];
        float invD = (std::abs(dir) > 1e-6f) ? (1.0f / dir) : 1e30f;

        float t0 = (bmin[i] - origin) * invD;
        float t1 = (bmax[i] - origin) * invD;
        if (t0 > t1) std::swap(t0, t1);

        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmax < tmin) return false;
    }

    outT = tmin;
    return true;
}

struct TextTexture {
    GLuint tex = 0;
    int w = 0, h = 0;
    void Destroy() { if (tex) glDeleteTextures(1, &tex); tex = 0; w = 0; h = 0; }
};

static TextTexture MakeTextTexture(TTF_Font* font, const std::string& text)
{
    TextTexture out{};
    SDL_Color white{ 255,255,255,255 };

    SDL_Surface* s0 = TTF_RenderUTF8_Blended(font, text.c_str(), white);
    if (!s0) { std::cout << "TTF_Render failed: " << TTF_GetError() << "\n"; return out; }

    SDL_Surface* s = SDL_ConvertSurfaceFormat(s0, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(s0);
    if (!s) { std::cout << "ConvertSurface failed: " << SDL_GetError() << "\n"; return out; }

    out.w = s->w; out.h = s->h;

    glGenTextures(1, &out.tex);
    glBindTexture(GL_TEXTURE_2D, out.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLint oldUnpack = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &oldUnpack);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, out.w, out.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, s->pixels);

    glPixelStorei(GL_UNPACK_ALIGNMENT, oldUnpack);
    glBindTexture(GL_TEXTURE_2D, 0);
    SDL_FreeSurface(s);
    return out;
}

struct MeshGL {
    GLuint vao = 0, vbo = 0;
    GLsizei count = 0;
    void Destroy() {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        vao = 0; vbo = 0; count = 0;
    }
};

static MeshGL MakeUIDynamicQuad()
{
    MeshGL m{};
    m.count = 6;

    float empty[6 * 4] = {};
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);

    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(empty), empty, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
    return m;
}

static void UpdateUIQuad(MeshGL& ui, int winW, int winH, int texW, int texH, int topMarginPx)
{
    if (winW <= 0 || winH <= 0) return;

    float x0 = (winW - texW) * 0.5f;
    float y0 = (float)topMarginPx;
    float x1 = x0 + (float)texW;
    float y1 = y0 + (float)texH;

    auto toNDCx = [&](float px) { return (px / (float)winW) * 2.0f - 1.0f; };
    auto toNDCy = [&](float py) { return 1.0f - (py / (float)winH) * 2.0f; };

    float l = toNDCx(x0), r = toNDCx(x1);
    float t = toNDCy(y0), b = toNDCy(y1);

    float v[] = {
        l,t, 0,0,  l,b, 0,1,  r,b, 1,1,
        l,t, 0,0,  r,b, 1,1,  r,t, 1,0
    };

    glBindBuffer(GL_ARRAY_BUFFER, ui.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static MeshGL MakeStars(int count)
{
    std::vector<glm::vec3> pts;
    pts.reserve(count);

    uint32_t seed = 1234567u;
    auto rnd = [&]() {
        seed ^= (seed << 13);
        seed ^= (seed >> 17);
        seed ^= (seed << 5);
        return seed;
        };
    auto fr01 = [&]() { return rnd() / (float)UINT32_MAX; };

    const float starRadius = 260.0f;

    for (int i = 0; i < count; ++i) {
        float u = fr01();
        float v = fr01();
        float theta = 2.0f * 3.1415926f * u;
        float z = 2.0f * v - 1.0f;
        float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
        float x = r * std::cos(theta);
        float y = r * std::sin(theta);
        pts.push_back(glm::vec3(x, y, z) * starRadius);
    }

    MeshGL m{};
    m.count = (GLsizei)pts.size();

    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);

    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(pts.size() * sizeof(glm::vec3)), pts.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    glBindVertexArray(0);
    return m;
}

static void AddTriPN(std::vector<float>& out, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& n)
{
    float t[] = {
        a.x,a.y,a.z, n.x,n.y,n.z,
        b.x,b.y,b.z, n.x,n.y,n.z,
        c.x,c.y,c.z, n.x,n.y,n.z
    };
    out.insert(out.end(), std::begin(t), std::end(t));
}

static void AddQuadPN(std::vector<float>& out, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& n)
{
    AddTriPN(out, a, b, c, n);
    AddTriPN(out, a, c, d, n);
}

static MeshGL MakeLitMesh(const std::vector<float>& posNormal)
{
    MeshGL m{};
    m.count = (GLsizei)(posNormal.size() / 6);

    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);

    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(posNormal.size() * sizeof(float)), posNormal.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
    return m;
}

static std::vector<float> BuildHangar()
{
    const float xmin = -12.0f, xmax = 12.0f;
    const float ymin = 0.0f, ymax = 10.0f;
    const float zback = 10.0f, zfront = -30.0f;

    const float oxmin = -9.0f, oxmax = 9.0f;
    const float oymin = 1.0f, oymax = 8.6f;
    const float oz = zfront;

    std::vector<float> out;
    out.reserve(3000);

    AddQuadPN(out,
        glm::vec3(xmin, ymin, zback), glm::vec3(xmax, ymin, zback), glm::vec3(xmax, ymin, zfront), glm::vec3(xmin, ymin, zfront),
        glm::vec3(0, 1, 0));

    AddQuadPN(out,
        glm::vec3(xmin, ymin, zfront), glm::vec3(xmin, ymin, zback), glm::vec3(xmin, ymax, zback), glm::vec3(xmin, ymax, zfront),
        glm::vec3(1, 0, 0));

    AddQuadPN(out,
        glm::vec3(xmax, ymin, zback), glm::vec3(xmax, ymin, zfront), glm::vec3(xmax, ymax, zfront), glm::vec3(xmax, ymax, zback),
        glm::vec3(-1, 0, 0));

    AddQuadPN(out,
        glm::vec3(xmin, ymin, zback), glm::vec3(xmax, ymin, zback), glm::vec3(xmax, ymax, zback), glm::vec3(xmin, ymax, zback),
        glm::vec3(0, 0, -1));

    AddQuadPN(out,
        glm::vec3(xmin, ymin, oz), glm::vec3(xmax, ymin, oz), glm::vec3(xmax, oymin, oz), glm::vec3(xmin, oymin, oz),
        glm::vec3(0, 0, 1));

    AddQuadPN(out,
        glm::vec3(xmin, oymax, oz), glm::vec3(xmax, oymax, oz), glm::vec3(xmax, ymax, oz), glm::vec3(xmin, ymax, oz),
        glm::vec3(0, 0, 1));

    AddQuadPN(out,
        glm::vec3(xmin, oymin, oz), glm::vec3(oxmin, oymin, oz), glm::vec3(oxmin, oymax, oz), glm::vec3(xmin, oymax, oz),
        glm::vec3(0, 0, 1));

    AddQuadPN(out,
        glm::vec3(oxmax, oymin, oz), glm::vec3(xmax, oymin, oz), glm::vec3(xmax, oymax, oz), glm::vec3(oxmax, oymax, oz),
        glm::vec3(0, 0, 1));

    return out;
}

static std::vector<float> BuildDoor(float x0, float x1, float y0, float y1, float z)
{
    std::vector<float> out;
    out.reserve(36);

    glm::vec3 a(x0, y0, z), b(x1, y0, z), c(x1, y1, z), d(x0, y1, z);
    AddQuadPN(out, a, b, c, d, glm::vec3(0, 0, 1));
    return out;
}

static void AddBoxPN(std::vector<float>& out, const glm::vec3& mn, const glm::vec3& mx)
{
    glm::vec3 p000(mn.x, mn.y, mn.z), p100(mx.x, mn.y, mn.z), p110(mx.x, mx.y, mn.z), p010(mn.x, mx.y, mn.z);
    glm::vec3 p001(mn.x, mn.y, mx.z), p101(mx.x, mn.y, mx.z), p111(mx.x, mx.y, mx.z), p011(mn.x, mx.y, mx.z);

    AddQuadPN(out, p000, p100, p110, p010, glm::vec3(0, 0, -1));
    AddQuadPN(out, p101, p001, p011, p111, glm::vec3(0, 0, 1));
    AddQuadPN(out, p001, p000, p010, p011, glm::vec3(-1, 0, 0));
    AddQuadPN(out, p100, p101, p111, p110, glm::vec3(1, 0, 0));
    AddQuadPN(out, p001, p101, p100, p000, glm::vec3(0, -1, 0));
    AddQuadPN(out, p010, p110, p111, p011, glm::vec3(0, 1, 0));
}

static std::vector<float> BuildMetalFrame(float w, float d, float h, float t)
{
    std::vector<float> out;
    out.reserve(6000);

    float hx = w * 0.5f;
    float hz = d * 0.5f;

    float baseW = w + 0.35f;
    float baseD = d + 0.35f;
    AddBoxPN(out,
        glm::vec3(-baseW * 0.5f, 0.00f, -baseD * 0.5f),
        glm::vec3(baseW * 0.5f, t, baseD * 0.5f));

    float px = hx - t * 0.5f;
    float pz = hz - t * 0.5f;

    AddBoxPN(out, glm::vec3(-px - t * 0.5f, 0.0f, -pz - t * 0.5f), glm::vec3(-px + t * 0.5f, h, -pz + t * 0.5f));
    AddBoxPN(out, glm::vec3(px - t * 0.5f, 0.0f, -pz - t * 0.5f), glm::vec3(px + t * 0.5f, h, -pz + t * 0.5f));
    AddBoxPN(out, glm::vec3(-px - t * 0.5f, 0.0f, pz - t * 0.5f), glm::vec3(-px + t * 0.5f, h, pz + t * 0.5f));
    AddBoxPN(out, glm::vec3(px - t * 0.5f, 0.0f, pz - t * 0.5f), glm::vec3(px + t * 0.5f, h, pz + t * 0.5f));

    float y0 = t, y1 = t + t;
    AddBoxPN(out, glm::vec3(-hx, y0, -hz - t * 0.5f), glm::vec3(hx, y1, -hz + t * 0.5f));
    AddBoxPN(out, glm::vec3(-hx, y0, hz - t * 0.5f), glm::vec3(hx, y1, hz + t * 0.5f));
    AddBoxPN(out, glm::vec3(-hx - t * 0.5f, y0, -hz), glm::vec3(-hx + t * 0.5f, y1, hz));
    AddBoxPN(out, glm::vec3(hx - t * 0.5f, y0, -hz), glm::vec3(hx + t * 0.5f, y1, hz));

    float yt0 = h - t, yt1 = h;
    AddBoxPN(out, glm::vec3(-hx, yt0, -hz - t * 0.5f), glm::vec3(hx, yt1, -hz + t * 0.5f));
    AddBoxPN(out, glm::vec3(-hx, yt0, hz - t * 0.5f), glm::vec3(hx, yt1, hz + t * 0.5f));
    AddBoxPN(out, glm::vec3(-hx - t * 0.5f, yt0, -hz), glm::vec3(-hx + t * 0.5f, yt1, hz));
    AddBoxPN(out, glm::vec3(hx - t * 0.5f, yt0, -hz), glm::vec3(hx + t * 0.5f, yt1, hz));

    return out;
}

enum class LaunchState { Idle, Countdown, Launched };

struct LaunchController {
    LaunchState state = LaunchState::Idle;
    float timer = 0.0f;
    int lastShown = -1;
    float launchedTime = 0.0f;

    void Start() {
        state = LaunchState::Countdown;
        timer = 5.0f;
        lastShown = -1;
        launchedTime = 0.0f;
    }

    std::string Update(float dt) {
        if (state == LaunchState::Idle) return "";

        if (state == LaunchState::Countdown) {
            timer -= dt;
            if (timer <= 0.0f) { state = LaunchState::Launched; launchedTime = 0.0f; return "LAUNCH!"; }
            int sec = (int)std::ceil(timer);
            if (sec != lastShown) { lastShown = sec; return "T-" + std::to_string(sec); }
        }
        else {
            launchedTime += dt;
        }
        return "";
    }
};

enum class DroidState { ToCrates, WaitAtCrates, ToConsole, WaitAtConsole };

struct DroidPatrol {
    DroidState st = DroidState::ToCrates;
    float wait = 0.0f;
    glm::vec3 pos{ 0,0,0 };
    float yawDeg = 0.0f;

    float speed = 2.1f;
    float waitSeconds = 3.0f;

    void Init(const glm::vec3& startPos) {
        pos = startPos;
        st = DroidState::ToCrates;
        wait = 0.0f;
        yawDeg = 0.0f;
    }

    void Update(float dt, const glm::vec3& consoleWP, const glm::vec3& cratesWP) {
        auto faceDir = [&](const glm::vec3& dir) {
            if (glm::length(dir) < 1e-4f) return;
            yawDeg = glm::degrees(std::atan2(dir.x, -dir.z));
            };

        if (st == DroidState::WaitAtCrates || st == DroidState::WaitAtConsole) {
            wait -= dt;
            if (wait <= 0.0f) st = (st == DroidState::WaitAtCrates) ? DroidState::ToConsole : DroidState::ToCrates;
            return;
        }

        glm::vec3 target = (st == DroidState::ToCrates) ? cratesWP : consoleWP;
        glm::vec3 to = target - pos;
        to.y = 0.0f;

        float dist = glm::length(to);
        if (dist < 0.08f) {
            pos = target;
            wait = waitSeconds;
            st = (st == DroidState::ToCrates) ? DroidState::WaitAtCrates : DroidState::WaitAtConsole;
            return;
        }

        glm::vec3 dir = to / dist;
        faceDir(dir);

        float step = speed * dt;
        pos = (step >= dist) ? target : (pos + dir * step);
    }
};

static const char* LIT_VS = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uM; uniform mat4 uV; uniform mat4 uP;
out vec3 vN;
void main(){
  vN = mat3(transpose(inverse(uM))) * aNormal;
  gl_Position = uP*uV*uM*vec4(aPos,1.0);
})";

static const char* LIT_FS = R"(#version 330 core
in vec3 vN; out vec4 FragColor;
uniform vec3 uColor; uniform vec3 uLightDir;
void main(){
  vec3 N=normalize(vN); vec3 L=normalize(-uLightDir);
  float diff = abs(dot(N,L));
  float amb=0.38;
  vec3 col = uColor*(amb + diff*0.80);
  FragColor = vec4(col,1);
})";

static const char* TEX_VS = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
uniform mat4 uM; uniform mat4 uV; uniform mat4 uP;
out vec3 vN;
out vec2 vUV;
void main(){
  vN = mat3(transpose(inverse(uM))) * aNormal;
  vUV = aUV;
  gl_Position = uP*uV*uM*vec4(aPos,1.0);
})";

static const char* TEX_FS = R"(#version 330 core
in vec3 vN; in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uDiffuse;
uniform vec3 uTint;
uniform vec3 uLightDir;
void main(){
  vec3 N=normalize(vN); vec3 L=normalize(-uLightDir);
  float diff = abs(dot(N,L));
  float amb = 0.28;
  vec3 tex = texture(uDiffuse, vUV).rgb;
  vec3 col = tex * uTint * (amb + diff*0.90);
  FragColor = vec4(col, 1.0);
})";

static const char* STAR_VS = R"(#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uV; uniform mat4 uP;
void main(){ gl_Position = uP*uV*vec4(aPos,1); gl_PointSize=2.0; }
)";

static const char* STAR_FS = R"(#version 330 core
out vec4 FragColor; void main(){ FragColor=vec4(1); }
)";

static const char* UI_VS = R"(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 vUV;
void main(){ vUV=aUV; gl_Position=vec4(aPos,0,1); }
)";

static const char* UI_FS = R"(#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uTex;
void main(){ FragColor=texture(uTex, vUV); }
)";

static void UseVP(GLuint prog, const glm::mat4& V, const glm::mat4& P, const glm::vec3& lightDir)
{
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog, "uV"), 1, GL_FALSE, &V[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(prog, "uP"), 1, GL_FALSE, &P[0][0]);
    glUniform3fv(glGetUniformLocation(prog, "uLightDir"), 1, &lightDir[0]);
}

static void DrawLitMesh(GLuint prog, const MeshGL& mesh, const glm::mat4& M, const glm::vec3& color)
{
    glUniformMatrix4fv(glGetUniformLocation(prog, "uM"), 1, GL_FALSE, &M[0][0]);
    glUniform3fv(glGetUniformLocation(prog, "uColor"), 1, &color[0]);
    glBindVertexArray(mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh.count);
    glBindVertexArray(0);
}

static void DrawModelTex(GLuint prog, Model& model, const glm::mat4& M, const glm::vec3& tint)
{
    glUniformMatrix4fv(glGetUniformLocation(prog, "uM"), 1, GL_FALSE, &M[0][0]);
    glUniform3fv(glGetUniformLocation(prog, "uTint"), 1, &tint[0]);
    model.Draw(prog);
}

int main()
{
    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::cout << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    if (IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) == 0) {
        std::cout << "IMG_Init failed: " << IMG_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    if (TTF_Init() != 0) {
        std::cout << "TTF_Init failed: " << TTF_GetError() << "\n";
        IMG_Quit(); SDL_Quit();
        return 1;
    }

    bool audioOK = true;
    Mix_Init(0);

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cout << "Mix_OpenAudio failed: " << Mix_GetError() << "\n";
        audioOK = false;
    }
    else {
        Mix_AllocateChannels(16);
    }

    Mix_Music* musAmbience = nullptr;
    Mix_Chunk* sfxDroid = nullptr;
    Mix_Chunk* sfxCountdown = nullptr;
    Mix_Chunk* sfxLaunch = nullptr;

    if (audioOK) {
        musAmbience = Mix_LoadMUS("assets/audio/space_station.wav");
        if (!musAmbience) std::cout << "[AUDIO] ambience: " << Mix_GetError() << "\n";

        sfxDroid = Mix_LoadWAV("assets/audio/druid.wav");
        if (!sfxDroid) std::cout << "[AUDIO] droid: " << Mix_GetError() << "\n";

        sfxCountdown = Mix_LoadWAV("assets/audio/countdown.wav");
        if (!sfxCountdown) std::cout << "[AUDIO] countdown: " << Mix_GetError() << "\n";

        sfxLaunch = Mix_LoadWAV("assets/audio/launch.wav");
        if (!sfxLaunch) std::cout << "[AUDIO] launch: " << Mix_GetError() << "\n";

        Mix_VolumeMusic(48);
        if (sfxDroid) Mix_VolumeChunk(sfxDroid, 70);
        if (sfxCountdown) Mix_VolumeChunk(sfxCountdown, 90);
        if (sfxLaunch) Mix_VolumeChunk(sfxLaunch, 110);

        if (musAmbience) Mix_PlayMusic(musAmbience, -1);
    }

    float droidSfxTimer = 0.0f;
    const float droidSfxEvery = 5.0f;
    bool countdownSfxPlayed = false;
    bool launchSfxPlayed = false;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "Weltraumbasis | Models Step",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1100, 700,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cout << "CreateWindow failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_GLContext glCtx = SDL_GL_CreateContext(window);
    if (!glCtx) {
        std::cout << "CreateContext failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_GL_SetSwapInterval(1);

    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    glGetError();
    if (glewErr != GLEW_OK) {
        std::cout << "glewInit failed\n";
        return 1;
    }

    int winW = 0, winH = 0;
    SDL_GetWindowSize(window, &winW, &winH);
    glViewport(0, 0, winW, winH);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_CULL_FACE);

    GLuint litProg = CreateProgram(LIT_VS, LIT_FS);
    GLuint texProg = CreateProgram(TEX_VS, TEX_FS);
    GLuint starProg = CreateProgram(STAR_VS, STAR_FS);
    GLuint uiProg = CreateProgram(UI_VS, UI_FS);

    if (!litProg || !texProg || !starProg || !uiProg) {
        std::cout << "Shader creation failed.\n";
        return 1;
    }

    MeshGL hangarMesh = MakeLitMesh(BuildHangar());
    MeshGL starMesh = MakeStars(2200);
    MeshGL uiQuad = MakeUIDynamicQuad();

    const float doorX0 = -9.0f, doorX1 = 9.0f;
    const float doorY0 = 1.0f, doorY1 = 8.6f;
    const float doorZ = -30.0f + 0.03f;
    MeshGL doorMesh = MakeLitMesh(BuildDoor(doorX0, doorX1, doorY0, doorY1, doorZ));

    float doorT = 0.0f;
    const float doorOpenSeconds = 1.8f;
    const float doorOpenDistance = (doorY1 - doorY0) + 0.30f;

    TTF_Font* font = TTF_OpenFont("assets/fonts/arial.ttf", 64);
    if (!font) font = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", 64);
    TextTexture uiText{};

    const float floorY = 0.0f;

    const float wallMargin = 1.30f;
    const float consoleX = -12.0f + wallMargin;
    const float cratesX = 12.0f - wallMargin;
    const float propsZ = -11.0f * 0.30f;

    const glm::vec3 consolePos(consoleX, floorY, propsZ);
    const glm::vec3 cratesPos(cratesX, floorY, propsZ);
    const glm::vec3 shipStart(0.0f, floorY, -14.8f);

    const float shipScale = 0.75f, shipLift = 0.80f;
    const float consoleScale = 2.00f, consoleLift = 3.10f;
    const float crateScale = 0.01f, crateLift = 0.67f, crateYStep = 1.70f;
    const float droidScale = 1.30f, droidLift = 1.32f;
    const float jupiterScale = 30.0f, jupiterSpin = 0.08f;

    const glm::vec3 mandoPos = consolePos + glm::vec3(1.0f, floorY, 6.7f);
    const float mandoScale = 3.00f, mandoLift = 0.00f, mandoYawDeg = 90.0f;

    const float frameW = 1.35f * mandoScale;
    const float frameD = 1.05f * mandoScale;
    const float frameH = 2.25f * mandoScale;
    const float frameT = 0.06f * mandoScale;
    MeshGL mandoFrameMesh = MakeLitMesh(BuildMetalFrame(frameW, frameD, frameH, frameT));

    glm::vec3 consoleAABBMin = consolePos + glm::vec3(-1.2f, 0.2f, -1.2f);
    glm::vec3 consoleAABBMax = consolePos + glm::vec3(1.2f, 2.4f, 1.2f);

    Model shipModel, consoleModel, crateModel, droidModel, jupiterModel, mandoModel;

    if (!shipModel.Load("assets/models/ship/ship.obj"))                 std::cout << "[WARN] ship.obj not loaded\n";
    if (!consoleModel.Load("assets/models/props/console/console.obj"))  std::cout << "[WARN] console.obj not loaded\n";
    if (!crateModel.Load("assets/models/props/crate/crate.obj"))        std::cout << "[WARN] crate.obj not loaded\n";
    if (!droidModel.Load("assets/models/druid/druid.obj"))              std::cout << "[WARN] druid.obj not loaded\n";
    if (!jupiterModel.Load("assets/models/Jupiter/jupiter.obj"))        std::cout << "[WARN] jupiter.obj not loaded\n";
    if (!mandoModel.Load("assets/models/mandalorian/mandalorian.obj"))  std::cout << "[WARN] mandalorian.obj not loaded\n";

    Camera cam;
    LaunchController launch;
    DroidPatrol patrol;
    patrol.Init(consolePos + glm::vec3(1.5f, floorY, 0.2f));

    SDL_SetRelativeMouseMode(SDL_TRUE);

    Uint64 last = SDL_GetPerformanceCounter();
    float timeSec = 0.0f;
    bool running = true;

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        double freq = (double)SDL_GetPerformanceFrequency();
        float dt = (float)((now - last) / freq);
        last = now;
        dt = Clamp(dt, 0.0f, 0.05f);
        timeSec += dt;

        SDL_Event e;
        bool click = false;
        int mdx = 0, mdy = 0;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;

            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                winW = e.window.data1; winH = e.window.data2;
                glViewport(0, 0, winW, winH);
            }

            if (e.type == SDL_MOUSEMOTION) { mdx += e.motion.xrel; mdy += e.motion.yrel; }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) click = true;

            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                SDL_bool rel = SDL_GetRelativeMouseMode();
                SDL_SetRelativeMouseMode(rel ? SDL_FALSE : SDL_TRUE);
            }
        }

        const float sens = 0.10f;
        cam.yaw += mdx * sens;
        cam.pitch -= mdy * sens;
        cam.pitch = Clamp(cam.pitch, -89.0f, 89.0f);

        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        glm::vec3 move(0.0f);

        if (ks[SDL_SCANCODE_W]) move += cam.Forward();
        if (ks[SDL_SCANCODE_S]) move -= cam.Forward();
        if (ks[SDL_SCANCODE_D]) move += cam.Right();
        if (ks[SDL_SCANCODE_A]) move -= cam.Right();
        if (ks[SDL_SCANCODE_SPACE]) move.y += 1.0f;
        if (ks[SDL_SCANCODE_LCTRL]) move.y -= 1.0f;

        if (glm::length(move) > 0.001f) move = glm::normalize(move);
        cam.pos += move * 5.0f * dt;
        cam.pos.y = std::max(cam.pos.y, 0.2f);

        if (audioOK && sfxDroid) {
            droidSfxTimer += dt;
            if (droidSfxTimer >= droidSfxEvery) {
                droidSfxTimer -= droidSfxEvery;
                Mix_PlayChannel(-1, sfxDroid, 0);
            }
        }

        float hitT = 0.0f;
        bool lookingAtConsole = RayAABB(cam.pos, cam.Forward(), consoleAABBMin, consoleAABBMax, hitT) && hitT < 10.0f;

        if (lookingAtConsole && click && launch.state == LaunchState::Idle) {
            launch.Start();
            doorT = 0.0f;
            countdownSfxPlayed = false;
            launchSfxPlayed = false;
        }

        std::string msg = launch.Update(dt);

        if (audioOK && sfxCountdown && !countdownSfxPlayed && msg == "T-4") {
            Mix_PlayChannel(-1, sfxCountdown, 0);
            countdownSfxPlayed = true;
        }
        if (audioOK && sfxLaunch && !launchSfxPlayed && msg == "LAUNCH!") {
            Mix_PlayChannel(-1, sfxLaunch, 0);
            launchSfxPlayed = true;
        }

        if (!msg.empty() && font) {
            uiText.Destroy();
            uiText = MakeTextTexture(font, msg);
        }

        if (launch.state != LaunchState::Idle) {
            doorT = Clamp(doorT + dt / doorOpenSeconds, 0.0f, 1.0f);
        }

        patrol.Update(dt,
            consolePos + glm::vec3(1.5f, floorY, 0.2f),
            cratesPos + glm::vec3(-1.5f, floorY, 0.2f)
        );

        float aspect = (winH > 0) ? (float)winW / (float)winH : 1.0f;
        glm::mat4 V = cam.View();
        glm::mat4 P = glm::perspective(glm::radians(75.0f), aspect, 0.05f, 800.0f);
        glm::mat4 VnoTrans = glm::mat4(glm::mat3(V));
        glm::vec3 lightDir = glm::normalize(glm::vec3(0.3f, -1.0f, 0.2f));

        glm::vec3 jupCenter(0.0f, 18.0f, -220.0f);
        glm::vec3 shipPos = shipStart;

        if (launch.state == LaunchState::Launched) {
            glm::vec3 target = jupCenter + glm::vec3(0, -6.0f, 0);
            float t = Clamp(launch.launchedTime / 8.0f, 0.0f, 1.0f);
            shipPos = glm::mix(shipStart, target, t);
        }

        glClearColor(0.01f, 0.01f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(starProg);
        glUniformMatrix4fv(glGetUniformLocation(starProg, "uV"), 1, GL_FALSE, &VnoTrans[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(starProg, "uP"), 1, GL_FALSE, &P[0][0]);
        glDepthMask(GL_FALSE);
        glBindVertexArray(starMesh.vao);
        glDrawArrays(GL_POINTS, 0, starMesh.count);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);

        if (jupiterModel.IsLoaded()) {
            UseVP(texProg, V, P, lightDir);
            glm::mat4 M(1.0f);
            M = glm::translate(M, jupCenter);
            M = glm::rotate(M, timeSec * jupiterSpin, glm::vec3(0, 1, 0));
            M = glm::scale(M, glm::vec3(jupiterScale));
            DrawModelTex(texProg, jupiterModel, M, glm::vec3(1, 1, 1));
        }

        UseVP(litProg, V, P, lightDir);
        DrawLitMesh(litProg, hangarMesh, glm::mat4(1.0f), glm::vec3(0.30f, 0.33f, 0.40f));

        {
            glm::mat4 Md(1.0f);
            Md = glm::translate(Md, glm::vec3(0.0f, doorT * doorOpenDistance, 0.0f));
            DrawLitMesh(litProg, doorMesh, Md, glm::vec3(0.22f, 0.24f, 0.28f));
        }

        {
            glm::mat4 Mf(1.0f);
            Mf = glm::translate(Mf, mandoPos + glm::vec3(0.0f, 0.01f, 0.0f));
            DrawLitMesh(litProg, mandoFrameMesh, Mf, glm::vec3(0.55f, 0.58f, 0.62f));
        }

        UseVP(texProg, V, P, lightDir);

        if (shipModel.IsLoaded()) {
            glm::mat4 M(1.0f);
            M = glm::translate(M, shipPos + glm::vec3(0.0f, shipLift, 0.0f));
            M = glm::rotate(M, glm::radians(180.0f), glm::vec3(0, 1, 0));
            M = glm::scale(M, glm::vec3(shipScale));
            DrawModelTex(texProg, shipModel, M, glm::vec3(1, 1, 1));
        }

        if (consoleModel.IsLoaded()) {
            glm::mat4 M(1.0f);
            M = glm::translate(M, consolePos + glm::vec3(0.0f, consoleLift, 0.0f));
            M = glm::rotate(M, glm::radians(-90.0f), glm::vec3(0, 1, 0));
            M = glm::scale(M, glm::vec3(consoleScale));
            DrawModelTex(texProg, consoleModel, M, glm::vec3(1, 1, 1));
        }

        if (mandoModel.IsLoaded()) {
            glm::mat4 M(1.0f);
            M = glm::translate(M, mandoPos + glm::vec3(0.0f, mandoLift, 0.0f));
            M = glm::rotate(M, glm::radians(mandoYawDeg), glm::vec3(0, 1, 0));
            M = glm::scale(M, glm::vec3(mandoScale));
            DrawModelTex(texProg, mandoModel, M, glm::vec3(1, 1, 1));
        }

        if (crateModel.IsLoaded()) {
            const int crateCount = 4;
            const int stacks = 3;
            const float stackSpacingZ = 5.20f;

            for (int s = 0; s < stacks; ++s) {
                float zOff = (s - 1) * stackSpacingZ;
                for (int i = 0; i < crateCount; ++i) {
                    glm::vec3 p = cratesPos + glm::vec3(0.0f, crateLift + i * crateYStep, zOff);
                    glm::mat4 M(1.0f);
                    M = glm::translate(M, p);
                    M = glm::scale(M, glm::vec3(crateScale));
                    DrawModelTex(texProg, crateModel, M, glm::vec3(1, 1, 1));
                }
            }
        }

        if (droidModel.IsLoaded()) {
            glm::mat4 M(1.0f);
            M = glm::translate(M, patrol.pos + glm::vec3(0.0f, droidLift, 0.0f));
            M = glm::rotate(M, glm::radians(patrol.yawDeg), glm::vec3(0, 1, 0));
            M = glm::scale(M, glm::vec3(droidScale));
            DrawModelTex(texProg, droidModel, M, glm::vec3(1, 1, 1));
        }

        if (uiText.tex) {
            UpdateUIQuad(uiQuad, winW, winH, uiText.w, uiText.h, 18);
            glDisable(GL_DEPTH_TEST);

            glUseProgram(uiProg);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, uiText.tex);
            glUniform1i(glGetUniformLocation(uiProg, "uTex"), 0);

            glBindVertexArray(uiQuad.vao);
            glDrawArrays(GL_TRIANGLES, 0, uiQuad.count);
            glBindVertexArray(0);

            glBindTexture(GL_TEXTURE_2D, 0);
            glEnable(GL_DEPTH_TEST);
        }

        SDL_GL_SwapWindow(window);
    }

    if (font) TTF_CloseFont(font);
    uiText.Destroy();

    mandoFrameMesh.Destroy();
    doorMesh.Destroy();
    hangarMesh.Destroy();
    starMesh.Destroy();
    uiQuad.Destroy();

    glDeleteProgram(litProg);
    glDeleteProgram(texProg);
    glDeleteProgram(starProg);
    glDeleteProgram(uiProg);

    if (audioOK) {
        Mix_HaltMusic();
        Mix_HaltChannel(-1);
        if (sfxLaunch)    Mix_FreeChunk(sfxLaunch);
        if (sfxCountdown) Mix_FreeChunk(sfxCountdown);
        if (sfxDroid)     Mix_FreeChunk(sfxDroid);
        if (musAmbience)  Mix_FreeMusic(musAmbience);
        Mix_CloseAudio();
    }
    Mix_Quit();

    SDL_GL_DeleteContext(glCtx);
    SDL_DestroyWindow(window);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}
