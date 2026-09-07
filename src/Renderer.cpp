#include "Renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <string>

#include <glad/glad.h>
#include <stb_image.h>

using namespace GameConfig;

namespace {
constexpr int kWindowWidth = 720;
constexpr int kWindowHeight = 1280;
constexpr int kOpenGLMajorVersion = 4;
constexpr int kOpenGLMinorVersion = 6;
constexpr int kVsyncEnabled = 1;
constexpr float kPi = 3.14159265358979323846f;

struct Vec3 { float x; float y; float z; };

struct Mat4 { std::array<float, 16> values{}; };

bool setGLAttribute(SDL_GLattr attribute, int value, const char* name) {
    if (SDL_GL_SetAttribute(attribute, value) == 0) { return true; }
    std::cerr << "Failed to set " << name << ": " << SDL_GetError() << '\n';
    return false;
}

float laneToX(int lane) { const int clampedLane = std::clamp(lane, 0, kLaneCount - 1); return kLanePositions[static_cast<std::size_t>(clampedLane)]; }

float radians(float degrees) { return degrees * kPi / 180.0f;}

Vec3 operator-(const Vec3& left, const Vec3& right) { return {left.x - right.x, left.y - right.y, left.z - right.z}; }
    float dot(const Vec3& left, const Vec3& right) { return left.x * right.x + left.y * right.y + left.z * right.z; }

Vec3 cross(const Vec3& left, const Vec3& right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    };
}

Vec3 normalize(const Vec3& value) {
    const float length = std::sqrt(dot(value, value));
    if (length <= 0.0001f) { return {0.0f, 0.0f, 0.0f}; }

    return {value.x / length, value.y / length, value.z / length};
}

constexpr float kSunAmbient = 0.35f;
const Vec3 kSunDir = normalize({-0.7f, 1.4f, 0.9f});

float faceLightForNormal(const Vec3& n) {
    const float d = std::max(0.0f, dot(n, kSunDir));
    return kSunAmbient + (1.0f - kSunAmbient) * d;
}

Mat4 identity() {
    Mat4 result;
    result.values[0] = 1.0f;
    result.values[5] = 1.0f;
    result.values[10] = 1.0f;
    result.values[15] = 1.0f;
    return result;
}

Mat4 multiply(const Mat4& left, const Mat4& right) {
    Mat4 result;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0f;
            for (int index = 0; index < 4; ++index) {
                value += left.values[index * 4 + row] * right.values[column * 4 + index];
            }
            result.values[column * 4 + row] = value;
        }
    }
    return result;
}

Mat4 translation(const Vec3& position) {
    Mat4 result = identity();
    result.values[12] = position.x;
    result.values[13] = position.y;
    result.values[14] = position.z;
    return result;
}

Mat4 scale(const Vec3& size) {
    Mat4 result = identity();
    result.values[0] = size.x;
    result.values[5] = size.y;
    result.values[10] = size.z;
    return result;
}

Mat4 rotateX(float angleRadians) {
    const float cosine = std::cos(angleRadians);
    const float sine = std::sin(angleRadians);

    Mat4 result = identity();
    result.values[5] = cosine;
    result.values[6] = sine;
    result.values[9] = -sine;
    result.values[10] = cosine;
    return result;
}

float worldCurvatureEase(float z) {
    constexpr float anchorZ = -5.0f;
    constexpr float rampDistance = 24.0f;
    const float dz = anchorZ - z;
    if (dz <= 0.0f) { return 0.0f; }
    const float ramp = std::min(dz / rampDistance, 1.0f);
    return ramp * ramp * (3.0f - 2.0f * ramp);
}

float worldCurvatureDrop(float z) {
    constexpr float anchorZ = -5.0f;
    const float dz = anchorZ - z;
    if (dz <= 0.0f) { return 0.0f; }
    return GameConfig::kWorldCurvature * dz * dz * worldCurvatureEase(z);
}

float worldLateralOffset(float z) {
    constexpr float anchorZ = -5.0f;
    const float dz = anchorZ - z;
    if (dz <= 0.0f) { return 0.0f; }
    const float dzToFar = std::max(dz, anchorZ - kLaneStartZ);
    const float arc = (dz / dzToFar) * (dz / dzToFar);
    return GameConfig::kRoadSway * arc * worldCurvatureEase(z);
}

Mat4 rotateY(float angleRadians) {
    const float cosine = std::cos(angleRadians);
    const float sine = std::sin(angleRadians);

    Mat4 result = identity();
    result.values[0] = cosine;
    result.values[2] = -sine;
    result.values[8] = sine;
    result.values[10] = cosine;
    return result;
}

Mat4 perspective(float fovRadians, float aspectRatio, float nearPlane, float farPlane) {
    Mat4 result;
    const float tanHalfFov = std::tan(fovRadians / 2.0f);

    result.values[0] = 1.0f / (aspectRatio * tanHalfFov);
    result.values[5] = 1.0f / tanHalfFov;
    result.values[10] = -(farPlane + nearPlane) / (farPlane - nearPlane);
    result.values[11] = -1.0f;
    result.values[14] = -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane);

    return result;
}

Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    const Vec3 forward = normalize(center - eye);
    const Vec3 side = normalize(cross(forward, up));
    const Vec3 cameraUp = cross(side, forward);

    Mat4 result = identity();
    result.values[0] = side.x;
    result.values[1] = cameraUp.x;
    result.values[2] = -forward.x;

    result.values[4] = side.y;
    result.values[5] = cameraUp.y;
    result.values[6] = -forward.y;

    result.values[8] = side.z;
    result.values[9] = cameraUp.z;
    result.values[10] = -forward.z;

    result.values[12] = -dot(side, eye);
    result.values[13] = -dot(cameraUp, eye);
    result.values[14] = dot(forward, eye);

    return result;
}

unsigned int compileShader(unsigned int shaderType, const char* source, const char* label) {
    const unsigned int shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success != 0) { return shader; }

    std::array<char, 1024> infoLog{};
    glGetShaderInfoLog(shader, static_cast<int>(infoLog.size()), nullptr, infoLog.data());
    std::cerr << label << " shader compilation failed: " << infoLog.data() << '\n';
    glDeleteShader(shader);
    return 0;
}

unsigned int hashSeed(unsigned int seed) {
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

float hashFloat(unsigned int seed, unsigned int index) {
    return static_cast<float>(hashSeed(seed * 137u + index * 7919u) & 0xFFFFu) / 65535.0f;
}
}

Renderer::Renderer()
    : window_(nullptr),
      glContext_(nullptr),
      sdlInitialized_(false),
      cubeVao_(0),
      cubeVbo_(0),
      shaderProgram_(0),
      mvpUniform_(-1),
      colorUniform_(-1),
      alphaUniform_(-1),
      cubeShaderProgram_(0),
      cubeMvpUniform_(-1),
      cubeColorUniform_(-1),
      cubeSunDirUniform_(-1),
      cubeAmbientUniform_(-1),
      spriteShaderProgram_(0),
      spriteMvpUniform_(-1),
      spriteTextureUniform_(-1),
      spriteTimeUniform_(-1),
      spriteSpawnTimeUniform_(-1),
      spriteRippleColorUniform_(-1),
      spriteAspectUniform_(-1),
      skyShaderProgram_(0),
      skyMvpUniform_(-1),
      skyTopColorUniform_(-1),
      skyBottomColorUniform_(-1),
      sunShaderProgram_(0),
      sunMvpUniform_(-1),
      sunColorUniform_(-1),
      sunCoreUniform_(-1),
      sunHazeUniform_(-1),
      roadShaderProgram_(0),
      roadMvpUniform_(-1),
      roadTextureUniform_(-1),
      roadTexScaleUniform_(-1),
      roadTexOffsetUniform_(-1),
      roadLodUniform_(-1),
      roadLightUniform_(-1),
      quadVao_(0),
      quadVbo_(0),
      roadTexture_(),
      skyscraperTexture_(),
      wallTexture_(),
      waterTexture_(),
      saladTexture_(),
      salmonTexture_(),
      sodaTexture_(),
      burgerTexture_(),
      friesTexture_(),
      drawableWidth_(kWindowWidth),
      drawableHeight_(kWindowHeight){
}

Renderer::~Renderer() {
    shutdown();
}

int Renderer::initSDL() {
    if(window_ && glContext_) { return 0; }
    if (window_ || glContext_ || sdlInitialized_) { shutdown(); }

    if(SDL_Init(SDL_INIT_VIDEO) < 0) { std::cerr << "SDL initialization error: " << SDL_GetError() << '\n'; return -1; }
    sdlInitialized_ = true;

    if (!setOpenGLAttributes() || !createWindow() || !createOpenGLContext() || !initializeGlad() || !initializeGraphics()) {
        shutdown();
        return -1;
    }

    if (SDL_GL_SetSwapInterval(kVsyncEnabled) != 0) { std::cerr << "VSync setup warning: " << SDL_GetError() << '\n'; }

    resizeViewport();
    return 0;
}

void Renderer::resizeViewport() {
    if(!window_){ return; }

    SDL_GL_GetDrawableSize(window_, &drawableWidth_, &drawableHeight_);
    glViewport(0, 0, drawableWidth_, drawableHeight_);
}

void Renderer::setWindowTitle(const std::string& title) const {
    if(!window_){ return; }

    SDL_SetWindowTitle(window_, title.c_str());
}

void Renderer::render(float playerX, float roadScroll, float gameTime, const std::vector<Entity>& entities,
                      const std::vector<CollectEffect>& collectEffects) {
    if(!window_ || shaderProgram_ == 0) { return; }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgram_);
    glBindVertexArray(cubeVao_);

    drawWorld(playerX, roadScroll, gameTime, entities, collectEffects);

    glBindVertexArray(0);
    glUseProgram(0);
    SDL_GL_SwapWindow(window_);
}

bool Renderer::setOpenGLAttributes() const {
    return setGLAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, kOpenGLMajorVersion, "OpenGL major version") &&
           setGLAttribute(SDL_GL_CONTEXT_MINOR_VERSION, kOpenGLMinorVersion, "OpenGL minor version") &&
           setGLAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE, "OpenGL profile") &&
           setGLAttribute(SDL_GL_RED_SIZE, 8, "red channel size") &&
           setGLAttribute(SDL_GL_GREEN_SIZE, 8, "green channel size") &&
           setGLAttribute(SDL_GL_BLUE_SIZE, 8, "blue channel size") &&
           setGLAttribute(SDL_GL_DEPTH_SIZE, 24, "depth buffer size") &&
           setGLAttribute(SDL_GL_DOUBLEBUFFER, 1, "double buffering");
}

bool Renderer::createWindow() {
    window_ = SDL_CreateWindow(
        kWindowTitle,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        kWindowWidth,
        kWindowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if(window_) { return true; }

    std::cerr << "Window creation error: " << SDL_GetError() << '\n';
    return false;
}

bool Renderer::createOpenGLContext() {
    glContext_ = SDL_GL_CreateContext(window_);
    if (!glContext_) { std::cerr << "OpenGL context error: " << SDL_GetError() << '\n'; return false; }
    if (SDL_GL_MakeCurrent(window_, glContext_) != 0) {
        std::cerr << "Failed to make OpenGL context current: " << SDL_GetError() << '\n'; return false; }

    return true;
}

bool Renderer::initializeGlad() const {
    if(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)) != 0) { return true; }

    std::cerr << "Failed to initialize GLAD\n";
    return false;
}

bool Renderer::initializeGraphics() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.74f, 0.86f, 0.97f, 1.0f);

    return createShaderProgram() && createSpriteShaderProgram() && createSkyShaderProgram() && createSunShaderProgram() &&
           createRoadShaderProgram() && createCubeShaderProgram() && createCubeMesh() && createQuadMesh() &&
           loadCollectibleTextures();
}

bool Renderer::createShaderProgram() {
    constexpr const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPosition;

uniform mat4 uMvp;

void main() { gl_Position = uMvp * vec4(aPosition, 1.0); }
)";

    constexpr const char* fragmentShaderSource = R"(
#version 330 core

out vec4 FragColor;
uniform vec3 uColor;
uniform float uAlpha;

void main() { FragColor = vec4(uColor, uAlpha); }
)";

    const unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource, "Vertex");
    if(vertexShader == 0) { return false; }

    const unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource, "Fragment");
    if(fragmentShader == 0) { glDeleteShader(vertexShader); return false; }

    shaderProgram_ = glCreateProgram();
    glAttachShader(shaderProgram_, vertexShader);
    glAttachShader(shaderProgram_, fragmentShader);
    glLinkProgram(shaderProgram_);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int success = 0;
    glGetProgramiv(shaderProgram_, GL_LINK_STATUS, &success);
    if(success == 0) {
        std::array<char, 1024> infoLog{};
        glGetProgramInfoLog(shaderProgram_, static_cast<int>(infoLog.size()), nullptr, infoLog.data());
        std::cerr << "Shader program linking failed: " << infoLog.data() << '\n';
        glDeleteProgram(shaderProgram_); shaderProgram_ = 0; return false;
    }

    mvpUniform_ = glGetUniformLocation(shaderProgram_, "uMvp");
    colorUniform_ = glGetUniformLocation(shaderProgram_, "uColor");
    alphaUniform_ = glGetUniformLocation(shaderProgram_, "uAlpha");
    if (mvpUniform_ < 0 || colorUniform_ < 0 || alphaUniform_ < 0) {
        std::cerr << "Shader program is missing required uniforms\n";
        glDeleteProgram(shaderProgram_); shaderProgram_ = 0; return false;
    }

    return true;
}

bool Renderer::createSpriteShaderProgram() {
    constexpr const char* vertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 uMvp;

out vec2 vTexCoord;

void main() {
    gl_Position = uMvp * vec4(aPosition, 1.0);
    vTexCoord = aTexCoord;
}
)";

    constexpr const char* fragmentSource = R"(
#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uTime;
uniform float uSpawnTime;
uniform vec3 uRippleColor;
uniform float uAspect;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    float alpha = step(0.1, texColor.a);
    if (alpha < 0.5) { discard; }

    vec2 texelSize = 1.0 / vec2(textureSize(uTexture, 0));

    float transparentNeighbors =
        (1.0 - step(0.1, texture(uTexture, vTexCoord + vec2(0.0, texelSize.y)).a)) +
        (1.0 - step(0.1, texture(uTexture, vTexCoord - vec2(0.0, texelSize.y)).a)) +
        (1.0 - step(0.1, texture(uTexture, vTexCoord + vec2(texelSize.x, 0.0)).a)) +
        (1.0 - step(0.1, texture(uTexture, vTexCoord - vec2(texelSize.x, 0.0)).a));
    float outline = step(0.5, transparentNeighbors);

    float pixelGrid = 12.0;

    vec2 centered = vTexCoord - 0.5;
    float dist = length(vec2(centered.x * uAspect, centered.y));
    float pixelDist = floor(dist * pixelGrid + 0.5) / pixelGrid;

    float ripple = sin(pixelDist * 14.0 - uTime * 5.0 + uSpawnTime * 2.0) * 0.5 + 0.5;

    vec3 outlineColor = uRippleColor + vec3(0.30);
    vec3 finalColor = texColor.rgb * 1.35 + outlineColor * outline * (0.55 + 1.35 * ripple);

    FragColor = vec4(finalColor, 1.0);
}
)";

    const unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, "Sprite Vertex");
    if(vertexShader == 0) { return false; }

    const unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "Sprite Fragment");
    if(fragmentShader == 0) { glDeleteShader(vertexShader); return false; }

    spriteShaderProgram_ = glCreateProgram();
    glAttachShader(spriteShaderProgram_, vertexShader);
    glAttachShader(spriteShaderProgram_, fragmentShader);
    glLinkProgram(spriteShaderProgram_);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int success = 0;
    glGetProgramiv(spriteShaderProgram_, GL_LINK_STATUS, &success);
    if(success == 0) {
        std::array<char, 1024> infoLog{};
        glGetProgramInfoLog(spriteShaderProgram_, static_cast<int>(infoLog.size()), nullptr, infoLog.data());
        std::cerr << "Sprite shader program linking failed: " << infoLog.data() << '\n';
        glDeleteProgram(spriteShaderProgram_); spriteShaderProgram_ = 0; return false;
    }

    spriteMvpUniform_ = glGetUniformLocation(spriteShaderProgram_, "uMvp");
    spriteTextureUniform_ = glGetUniformLocation(spriteShaderProgram_, "uTexture");
    spriteTimeUniform_ = glGetUniformLocation(spriteShaderProgram_, "uTime");
    spriteSpawnTimeUniform_ = glGetUniformLocation(spriteShaderProgram_, "uSpawnTime");
    spriteRippleColorUniform_ = glGetUniformLocation(spriteShaderProgram_, "uRippleColor");
    spriteAspectUniform_ = glGetUniformLocation(spriteShaderProgram_, "uAspect");

    if (spriteMvpUniform_ < 0 || spriteTextureUniform_ < 0 || spriteTimeUniform_ < 0 ||
        spriteSpawnTimeUniform_ < 0 || spriteRippleColorUniform_ < 0 || spriteAspectUniform_ < 0) {
        std::cerr << "Sprite shader program is missing required uniforms\n";
        glDeleteProgram(spriteShaderProgram_); spriteShaderProgram_ = 0; return false;
    }

    return true;
}

bool Renderer::createSkyShaderProgram() {
    constexpr const char* vertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 uMvp;

out vec2 vUv;

void main() {
    gl_Position = uMvp * vec4(aPosition, 1.0);
    vUv = aTexCoord;
}
)";

    constexpr const char* fragmentSource = R"(
#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform vec3 uTopColor;
uniform vec3 uBottomColor;

void main() {
    FragColor = vec4(mix(uBottomColor, uTopColor, vUv.y), 1.0);
}
)";

    const unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, "Sky Vertex");
    if(vertexShader == 0) { return false; }

    const unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "Sky Fragment");
    if(fragmentShader == 0) { glDeleteShader(vertexShader); return false; }

    skyShaderProgram_ = glCreateProgram();
    glAttachShader(skyShaderProgram_, vertexShader);
    glAttachShader(skyShaderProgram_, fragmentShader);
    glLinkProgram(skyShaderProgram_);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int success = 0;
    glGetProgramiv(skyShaderProgram_, GL_LINK_STATUS, &success);
    if(success == 0) {
        std::array<char, 1024> infoLog{};
        glGetProgramInfoLog(skyShaderProgram_, static_cast<int>(infoLog.size()), nullptr, infoLog.data());
        std::cerr << "Sky shader program linking failed: " << infoLog.data() << '\n';
        glDeleteProgram(skyShaderProgram_); skyShaderProgram_ = 0; return false;
    }

    skyMvpUniform_ = glGetUniformLocation(skyShaderProgram_, "uMvp");
    skyTopColorUniform_ = glGetUniformLocation(skyShaderProgram_, "uTopColor");
    skyBottomColorUniform_ = glGetUniformLocation(skyShaderProgram_, "uBottomColor");
    if (skyMvpUniform_ < 0 || skyTopColorUniform_ < 0 || skyBottomColorUniform_ < 0) {
        std::cerr << "Sky shader program is missing required uniforms\n";
        glDeleteProgram(skyShaderProgram_); skyShaderProgram_ = 0; return false;
    }

    return true;
}

bool Renderer::createSunShaderProgram() {
    constexpr const char* vertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 uMvp;

out vec2 vUv;

void main() {
    gl_Position = uMvp * vec4(aPosition, 1.0);
    vUv = aTexCoord;
}
)";

    constexpr const char* fragmentSource = R"(
#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform vec3 uColor;
uniform float uCore;
uniform float uHaze;

void main() {
    vec2 centered = (vUv - vec2(0.5)) * 2.0;
    float dist = length(centered);
    float core = exp(-dist * dist * uCore);
    float haze = exp(-dist * uHaze * 2.0) * 0.5;
    float brightness = core + haze;
    FragColor = vec4(uColor * brightness, 1.0);
}
)";

    const unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, "Sun Vertex");
    if(vertexShader == 0) { return false; }

    const unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "Sun Fragment");
    if(fragmentShader == 0) { glDeleteShader(vertexShader); return false; }

    sunShaderProgram_ = glCreateProgram();
    glAttachShader(sunShaderProgram_, vertexShader);
    glAttachShader(sunShaderProgram_, fragmentShader);
    glLinkProgram(sunShaderProgram_);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int success = 0;
    glGetProgramiv(sunShaderProgram_, GL_LINK_STATUS, &success);
    if(success == 0) {
        std::array<char, 1024> infoLog{};
        glGetProgramInfoLog(sunShaderProgram_, static_cast<int>(infoLog.size()), nullptr, infoLog.data());
        std::cerr << "Sun shader program linking failed: " << infoLog.data() << '\n';
        glDeleteProgram(sunShaderProgram_); sunShaderProgram_ = 0; return false;
    }

    sunMvpUniform_ = glGetUniformLocation(sunShaderProgram_, "uMvp");
    sunColorUniform_ = glGetUniformLocation(sunShaderProgram_, "uColor");
    sunCoreUniform_ = glGetUniformLocation(sunShaderProgram_, "uCore");
    sunHazeUniform_ = glGetUniformLocation(sunShaderProgram_, "uHaze");
    if (sunMvpUniform_ < 0 || sunColorUniform_ < 0 || sunCoreUniform_ < 0 || sunHazeUniform_ < 0) {
        std::cerr << "Sun shader program is missing required uniforms\n";
        glDeleteProgram(sunShaderProgram_); sunShaderProgram_ = 0; return false;
    }

    return true;
}

bool Renderer::createRoadShaderProgram() {
    constexpr const char* vertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 uMvp;
uniform vec2 uTexScale;
uniform vec2 uTexOffset;

out vec2 vTexCoord;

void main() {
    gl_Position = uMvp * vec4(aPosition, 1.0);
    vTexCoord = aTexCoord * uTexScale + uTexOffset;
}
)";

    constexpr const char* fragmentSource = R"(
#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uLod;
uniform float uLight;

void main() {
    FragColor = vec4(textureLod(uTexture, vTexCoord, uLod).rgb * uLight, 1.0);
}
)";

    const unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, "Road Vertex");
    if(vertexShader == 0) { return false; }

    const unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "Road Fragment");
    if(fragmentShader == 0) { glDeleteShader(vertexShader); return false; }

    roadShaderProgram_ = glCreateProgram();
    glAttachShader(roadShaderProgram_, vertexShader);
    glAttachShader(roadShaderProgram_, fragmentShader);
    glLinkProgram(roadShaderProgram_);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int success = 0;
    glGetProgramiv(roadShaderProgram_, GL_LINK_STATUS, &success);
    if(success == 0) {
        std::array<char, 1024> infoLog{};
        glGetProgramInfoLog(roadShaderProgram_, static_cast<int>(infoLog.size()), nullptr, infoLog.data());
        std::cerr << "Road shader program linking failed: " << infoLog.data() << '\n';
        glDeleteProgram(roadShaderProgram_); roadShaderProgram_ = 0; return false;
    }

    roadMvpUniform_ = glGetUniformLocation(roadShaderProgram_, "uMvp");
    roadTextureUniform_ = glGetUniformLocation(roadShaderProgram_, "uTexture");
    roadTexScaleUniform_ = glGetUniformLocation(roadShaderProgram_, "uTexScale");
    roadTexOffsetUniform_ = glGetUniformLocation(roadShaderProgram_, "uTexOffset");
    roadLodUniform_ = glGetUniformLocation(roadShaderProgram_, "uLod");
    roadLightUniform_ = glGetUniformLocation(roadShaderProgram_, "uLight");

    if (roadMvpUniform_ < 0 || roadTextureUniform_ < 0 || roadTexScaleUniform_ < 0 ||
        roadTexOffsetUniform_ < 0 || roadLodUniform_ < 0 || roadLightUniform_ < 0) {
        std::cerr << "Road shader program is missing required uniforms\n";
        glDeleteProgram(roadShaderProgram_); roadShaderProgram_ = 0; return false;
    }

    return true;
}

bool Renderer::createCubeShaderProgram() {
    constexpr const char* vertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;

uniform mat4 uMvp;

out vec3 vNormal;

void main() {
    gl_Position = uMvp * vec4(aPosition, 1.0);
    vNormal = aNormal;
}
)";

    constexpr const char* fragmentSource = R"(
#version 330 core

in vec3 vNormal;
out vec4 FragColor;

uniform vec3 uColor;
uniform vec3 uSunDir;
uniform float uAmbient;

void main() {
    float diffuse = max(dot(normalize(vNormal), normalize(uSunDir)), 0.0);
    float light = uAmbient + (1.0 - uAmbient) * diffuse;
    FragColor = vec4(uColor * light, 1.0);
}
)";

    const unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, "Cube Vertex");
    if(vertexShader == 0) { return false; }

    const unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "Cube Fragment");
    if(fragmentShader == 0) { glDeleteShader(vertexShader); return false; }

    cubeShaderProgram_ = glCreateProgram();
    glAttachShader(cubeShaderProgram_, vertexShader);
    glAttachShader(cubeShaderProgram_, fragmentShader);
    glLinkProgram(cubeShaderProgram_);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int success = 0;
    glGetProgramiv(cubeShaderProgram_, GL_LINK_STATUS, &success);
    if(success == 0) {
        std::array<char, 1024> infoLog{};
        glGetProgramInfoLog(cubeShaderProgram_, static_cast<int>(infoLog.size()), nullptr, infoLog.data());
        std::cerr << "Cube shader program linking failed: " << infoLog.data() << '\n';
        glDeleteProgram(cubeShaderProgram_); cubeShaderProgram_ = 0; return false;
    }

    cubeMvpUniform_ = glGetUniformLocation(cubeShaderProgram_, "uMvp");
    cubeColorUniform_ = glGetUniformLocation(cubeShaderProgram_, "uColor");
    cubeSunDirUniform_ = glGetUniformLocation(cubeShaderProgram_, "uSunDir");
    cubeAmbientUniform_ = glGetUniformLocation(cubeShaderProgram_, "uAmbient");

    if (cubeMvpUniform_ < 0 || cubeColorUniform_ < 0 || cubeSunDirUniform_ < 0 || cubeAmbientUniform_ < 0) {
        std::cerr << "Cube shader program is missing required uniforms\n";
        glDeleteProgram(cubeShaderProgram_); cubeShaderProgram_ = 0; return false;
    }

    return true;
}

bool Renderer::createCubeMesh() {
    constexpr std::array<float, 216> cubeVertices = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f, -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f, -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f, -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };

    glGenVertexArrays(1, &cubeVao_);
    glGenBuffers(1, &cubeVbo_);

    if(cubeVao_ == 0 || cubeVbo_ == 0) { std::cerr << "Failed to create cube mesh buffers\n"; return false; }

    glBindVertexArray(cubeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(cubeVertices.size() * sizeof(float)),
                       cubeVertices.data(), GL_STATIC_DRAW );

    constexpr int stride = 6 * static_cast<int>(sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

bool Renderer::createQuadMesh() {
    constexpr std::array<float, 30> quadVertices = {
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,

        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
    };

    glGenVertexArrays(1, &quadVao_);
    glGenBuffers(1, &quadVbo_);

    if(quadVao_ == 0 || quadVbo_ == 0) { std::cerr << "Failed to create quad mesh buffers\n"; return false; }

    glBindVertexArray(quadVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(quadVertices.size() * sizeof(float)),
                       quadVertices.data(), GL_STATIC_DRAW );

    constexpr int stride = 5 * static_cast<int>(sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

unsigned int Renderer::loadTexture(const char* path, int& outWidth, int& outHeight, bool repeat) {
    int channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &outWidth, &outHeight, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << " - " << stbi_failure_reason() << '\n';
        return 0;
    }

    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    const int wrapMode = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                       GameConfig::kTriPointFiltering ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, outWidth, outHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    std::cout << "Loaded texture: " << path << " (" << outWidth << "x" << outHeight << ")\n";
    return texture;
}

bool Renderer::loadCollectibleTextures() {
    roadTexture_.id = loadTexture(ASSETS_DIR "/road.png", roadTexture_.width, roadTexture_.height, true);
    skyscraperTexture_.id = loadTexture(ASSETS_DIR "/skyscraper.png", skyscraperTexture_.width, skyscraperTexture_.height, true);
    wallTexture_.id = loadTexture(ASSETS_DIR "/wall_brick.png", wallTexture_.width, wallTexture_.height, true);
    waterTexture_.id = loadTexture(ASSETS_DIR "/water.png", waterTexture_.width, waterTexture_.height, false);
    saladTexture_.id = loadTexture(ASSETS_DIR "/salad.png", saladTexture_.width, saladTexture_.height, false);
    salmonTexture_.id = loadTexture(ASSETS_DIR "/salmon.png", salmonTexture_.width, salmonTexture_.height, false);
    sodaTexture_.id = loadTexture(ASSETS_DIR "/soda.png", sodaTexture_.width, sodaTexture_.height, false);
    burgerTexture_.id = loadTexture(ASSETS_DIR "/burger.png", burgerTexture_.width, burgerTexture_.height, false);
    friesTexture_.id = loadTexture(ASSETS_DIR "/fries.png", friesTexture_.width, friesTexture_.height, false);

    if (roadTexture_.id == 0 || skyscraperTexture_.id == 0 || wallTexture_.id == 0 ||
        waterTexture_.id == 0 || saladTexture_.id == 0 || salmonTexture_.id == 0 || sodaTexture_.id == 0 ||
        burgerTexture_.id == 0 || friesTexture_.id == 0) {
        std::cerr << "Failed to load one or more textures\n";
        return false;
    }
    return true;
}

unsigned int Renderer::getCollectibleTexture(const Entity& entity, float& outAspect) const {
    if (entity.type == EntityType::SlowCollectible) {
        const SpriteTexture* tex = nullptr;
        switch (entity.textureIndex % 3) {
            case 0: tex = &sodaTexture_; break;
            case 1: tex = &burgerTexture_; break;
            default: tex = &friesTexture_; break;
        }
        outAspect = static_cast<float>(tex->width) / static_cast<float>(tex->height);
        return tex->id;
    }

    const SpriteTexture* tex = nullptr;
    switch (entity.textureIndex % 3) {
        case 0: tex = &waterTexture_; break;
        case 1: tex = &saladTexture_; break;
        default: tex = &salmonTexture_; break;
    }
    outAspect = static_cast<float>(tex->width) / static_cast<float>(tex->height);
    return tex->id;
}

void Renderer::drawWorld(float playerX, float roadScroll, float gameTime, const std::vector<Entity>& entities,
                         const std::vector<CollectEffect>& collectEffects) {
    constexpr Color playerColor = {0.12f, 0.55f, 1.0f};
    constexpr Color obstacleColor = {0.95f, 0.08f, 0.06f};

    const float aspectRatio = static_cast<float>(std::max(1, drawableWidth_)) /
                              static_cast<float>(std::max(1, drawableHeight_));
    const Mat4 projection = perspective(radians(60.0f), aspectRatio, 0.1f, 200.0f);
    const Mat4 view = lookAt({0.0f, 5.0f, 9.0f}, {0.0f, 0.5f, -18.0f}, {0.0f, 1.0f, 0.0f});
    const Mat4 viewProj = multiply(projection, view);

    const float scrollOffset = std::fmod(roadScroll, kLaneSegmentLength);

    glDepthMask(GL_FALSE);
    glUseProgram(skyShaderProgram_);
    glBindVertexArray(quadVao_);
    {
        const Mat4 skyScale = scale({2.0f, 2.0f, 1.0f});
        glUniformMatrix4fv(skyMvpUniform_, 1, GL_FALSE, skyScale.values.data());
        glUniform3f(skyTopColorUniform_, 0.30f, 0.60f, 0.93f);
        glUniform3f(skyBottomColorUniform_, 0.74f, 0.86f, 0.97f);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glUseProgram(shaderProgram_);
    glBindVertexArray(quadVao_);
    drawSun(viewProj.values);

    glDepthMask(GL_TRUE);

    constexpr Color groundColor = {0.36f, 0.35f, 0.33f};
    constexpr float groundY = -0.06f;
    constexpr float groundDepth = 11.0f;
    constexpr float groundSigns[] = {-1.0f, 1.0f};
    constexpr int kGroundSubdivisions = 6;
    const float laneWidth = 1.85f;
    const float roadWidth = (kLanePositions[kLaneCount - 1] - kLanePositions[0]) + laneWidth;
    const float roadHalfEdge = roadWidth * 0.5f;
    const float sidewalkHalfWidth = roadHalfEdge + 0.75f;
    const float ks = kLaneStartZ + scrollOffset;

    glUseProgram(shaderProgram_);
    glBindVertexArray(quadVao_);

    for (float sign : groundSigns) {
        const float centerX = sign * (sidewalkHalfWidth + groundDepth * 0.5f);
        const float groundLight = faceLightForNormal({0.0f, 1.0f, 0.0f});
        for (int segment = 0; segment < kLaneSegmentCount; ++segment) {
            const float segmentZ = ks + static_cast<float>(segment) * kLaneSegmentLength;
            for (int sub = 0; sub < kGroundSubdivisions; ++sub) {
                const float centerZ = segmentZ + (static_cast<float>(sub) + 0.5f) *
                                                 (kLaneSegmentLength / static_cast<float>(kGroundSubdivisions));
                drawColoredQuad(viewProj.values, groundColor,
                                {centerX + worldLateralOffset(centerZ), groundY - worldCurvatureDrop(centerZ),
                                 centerZ, groundDepth, kLaneSegmentLength / static_cast<float>(kGroundSubdivisions),
                                 1.0f},
                                static_cast<int>(Face::Top), groundLight);
            }
        }
    }

    glUseProgram(roadShaderProgram_);
    glBindVertexArray(quadVao_);

    constexpr float roadY = -0.04f;
    constexpr int kRoadSubdivisions = 6;
    const float subLength = kLaneSegmentLength / static_cast<float>(kRoadSubdivisions);
    const Mat4 rs = multiply(rotateX(kPi * 0.5f), scale({roadWidth, subLength, 1.0f}));
    const float roadLight = faceLightForNormal({0.0f, 1.0f, 0.0f});

    for (int segment = 0; segment < kLaneSegmentCount; ++segment) {
        const float segmentZ = kLaneStartZ + scrollOffset + static_cast<float>(segment) * kLaneSegmentLength;

        for (int sub = 0; sub < kRoadSubdivisions; ++sub) {
            const float centerZ = segmentZ + (static_cast<float>(sub) + 0.5f) * subLength;
            const float centerY = roadY - worldCurvatureDrop(centerZ);
            const Mat4 model = multiply(translation({worldLateralOffset(centerZ), centerY, centerZ}), rs);
            const Mat4 mvp = multiply(viewProj, model);

            const float lod = computeMipLevel(worldLateralOffset(centerZ), centerY, centerZ,
                                              roadWidth, subLength, 256.0f, 256.0f / kRoadSubdivisions);

            glUniformMatrix4fv(roadMvpUniform_, 1, GL_FALSE, mvp.values.data());
            glUniform1i(roadTextureUniform_, 0);
            glUniform2f(roadTexScaleUniform_, 1.0f, 1.0f / static_cast<float>(kRoadSubdivisions));
            glUniform2f(roadTexOffsetUniform_, 0.0f,
                        static_cast<float>(segment) + static_cast<float>(sub) / static_cast<float>(kRoadSubdivisions));
            glUniform1f(roadLodUniform_, lod);
            glUniform1f(roadLightUniform_, roadLight);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, roadTexture_.id);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(quadVao_);

    constexpr int kBuildingsPerSide = 4;
    constexpr float buildingSides[] = {-1.0f, 1.0f};
    constexpr float kSkyscraperMinHeight = 6.0f;
    constexpr Color roofColor = {0.45f, 0.45f, 0.48f};
    constexpr float kCellWidth = 2.5f;
    constexpr float kCellStartX = 3.7f;

    struct Building {
        float x;
        float cy;
        float z;
        float w;
        float h;
        float d;
        unsigned int texture;
    };

    std::vector<Building> buildings;
    buildings.reserve(static_cast<std::size_t>(kLaneSegmentCount * 2 * kBuildingsPerSide));

    constexpr float kBuildingSpacing = 12.0f;
    constexpr float kBuildingNearZ = -100.0f;
    constexpr float kBuildingFarZ = 12.0f;

    for (float side : buildingSides) {
        const unsigned int sideIndex = side < 0.0f ? 0u : 1u;

        for (int b = 0; b < kBuildingsPerSide; ++b) {
            const int firstI = static_cast<int>(std::floor((roadScroll - kBuildingFarZ) / kBuildingSpacing));
            const int lastI = static_cast<int>(std::ceil((roadScroll - kBuildingNearZ) / kBuildingSpacing));

            for (int i = firstI; i <= lastI; ++i) {
                const unsigned int seed = static_cast<unsigned int>(i) * 97u +
                                          sideIndex * 131u + static_cast<unsigned int>(b) * 353u;

                const float w = 1.6f + hashFloat(seed, 0) * 0.7f;
                const float h = 1.5f + hashFloat(seed, 1) * 8.0f;
                const float d = 2.5f + hashFloat(seed, 2) * 4.5f;

                const float cellCenterX = kCellStartX + (static_cast<float>(b) + 0.5f) * kCellWidth;
                const float xJitterMax = std::max(0.05f, (kCellWidth - w) * 0.5f - 0.1f);
                const float zJitterMax = std::max(0.0f, 5.5f - d * 0.5f);
                const float bz = roadScroll - (static_cast<float>(i) * kBuildingSpacing +
                                               (hashFloat(seed, 4) - 0.5f) * 2.0f * zJitterMax);
                const float bx = side * (cellCenterX + (hashFloat(seed, 3) - 0.5f) * 2.0f * xJitterMax) +
                                 worldLateralOffset(bz);

                buildings.push_back({bx, h * 0.5f - 0.08f - worldCurvatureDrop(bz), bz, w, h, d,
                                     h >= kSkyscraperMinHeight ? skyscraperTexture_.id : wallTexture_.id});
            }
        }
    }

    const float frontLight = faceLightForNormal({0.0f, 0.0f, 1.0f});
    const float backLight = faceLightForNormal({0.0f, 0.0f, -1.0f});
    const float rightLight = faceLightForNormal({1.0f, 0.0f, 0.0f});
    const float leftLight = faceLightForNormal({-1.0f, 0.0f, 0.0f});
    const float roofLight = faceLightForNormal({0.0f, 1.0f, 0.0f});

    for (const Building& building : buildings) {
        drawTexturedQuad(viewProj.values, building.texture,
                         {building.x, building.cy, building.z + building.d * 0.5f, building.w, building.h, 1.0f},
                         static_cast<int>(Face::FrontBack), 1.0f, 1.0f, frontLight);
        drawTexturedQuad(viewProj.values, building.texture,
                         {building.x, building.cy, building.z - building.d * 0.5f, building.w, building.h, 1.0f},
                         static_cast<int>(Face::FrontBack), 1.0f, 1.0f, backLight);
        drawTexturedQuad(viewProj.values, building.texture,
                         {building.x + building.w * 0.5f, building.cy, building.z, building.d, building.h, 1.0f},
                         static_cast<int>(Face::Side), 1.0f, 1.0f, rightLight);
        drawTexturedQuad(viewProj.values, building.texture,
                         {building.x - building.w * 0.5f, building.cy, building.z, building.d, building.h, 1.0f},
                         static_cast<int>(Face::Side), 1.0f, 1.0f, leftLight);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(shaderProgram_);

    for (const Building& building : buildings) {
        drawColoredQuad(viewProj.values, roofColor,
                        {building.x, building.h - 0.08f - worldCurvatureDrop(building.z), building.z,
                         building.w, building.d, 1.0f},
                        static_cast<int>(Face::Top), roofLight);
    }

    constexpr Color sidewalkColor = {0.42f, 0.42f, 0.45f};
    constexpr float sidewalkY = -0.03f;
    constexpr float sidewalkWidth = 0.75f;
    constexpr float sidewalkSigns[] = {-1.0f, 1.0f};

    for (int segment = 0; segment < kLaneSegmentCount; ++segment) {
        const float segmentZ = kLaneStartZ + scrollOffset + static_cast<float>(segment) * kLaneSegmentLength;

        for (int sub = 0; sub < kRoadSubdivisions; ++sub) {
            const float centerZ = segmentZ + (static_cast<float>(sub) + 0.5f) * subLength;

            for (float sideSign : sidewalkSigns) {
                const float centerX = sideSign * (roadHalfEdge + sidewalkHalfWidth) * 0.5f +
                                      worldLateralOffset(centerZ);
                drawColoredQuad(viewProj.values, sidewalkColor,
                                {centerX, sidewalkY - worldCurvatureDrop(centerZ), centerZ,
                                 sidewalkWidth, subLength, 1.0f},
                                static_cast<int>(Face::Top), roadLight);
            }
        }
    }

    glDepthMask(GL_FALSE);

    for (const Building& building : buildings) {
        drawShadow(viewProj.values, building.x, building.z,
                   building.w * 1.05f, building.d, -0.02f, 0.30f);
        drawSunShadow(viewProj.values, building.x, building.z,
                      building.h, building.w, building.d, -0.02f, 0.26f);
    }

    drawShadow(viewProj.values, playerX + worldLateralOffset(kPlayerZ), kPlayerZ,
               kPlayerScale * 1.1f, 0.55f, -0.035f, 0.30f);
    drawSunShadow(viewProj.values, playerX + worldLateralOffset(kPlayerZ), kPlayerZ,
                  kPlayerScale, kPlayerScale * 0.9f, kPlayerScale * 0.9f, -0.035f, 0.22f);

    for(const Entity& entity : entities){
        if(entity.type == EntityType::Obstacle) {
            drawShadow(viewProj.values, laneToX(entity.lane) + worldLateralOffset(entity.z), entity.z,
                       1.0f, 0.55f, -0.035f, 0.30f);
            drawSunShadow(viewProj.values, laneToX(entity.lane) + worldLateralOffset(entity.z), entity.z,
                          0.95f, 0.9f, 0.9f, -0.035f, 0.22f);
        } else {
            drawShadow(viewProj.values, laneToX(entity.lane) + worldLateralOffset(entity.z), entity.z,
                       1.1f, 0.6f, -0.035f, 0.30f);
            drawSunShadow(viewProj.values, laneToX(entity.lane) + worldLateralOffset(entity.z), entity.z,
                          0.8f, 1.0f, 1.0f, -0.035f, 0.18f);
        }
    }

    glDepthMask(GL_TRUE);

    glBindVertexArray(cubeVao_);
    glUseProgram(cubeShaderProgram_);

    drawCube(viewProj.values, {playerX + worldLateralOffset(kPlayerZ), kPlayerScale * 0.5f, kPlayerZ,
                               kPlayerScale, kPlayerScale, kPlayerScale}, playerColor);

    for(const Entity& entity : entities){
        if(entity.type == EntityType::Obstacle) {
            drawCube(viewProj.values, {laneToX(entity.lane) + worldLateralOffset(entity.z),
                                       0.48f - worldCurvatureDrop(entity.z),
                                       entity.z, 0.95f, 0.95f, 0.95f}, obstacleColor);
        }
    }

    glDepthMask(GL_FALSE);
    glUseProgram(spriteShaderProgram_);
    glBindVertexArray(quadVao_);

    for(const Entity& entity : entities){
        if(entity.type == EntityType::SlowCollectible) {
            constexpr float rippleColor[3] = {0.9f, 0.15f, 0.15f};
            float aspect = 1.0f;
            unsigned int tex = getCollectibleTexture(entity, aspect);
            drawSprite(viewProj.values, tex, {laneToX(entity.lane) + worldLateralOffset(entity.z),
                                              0.82f - worldCurvatureDrop(entity.z),
                                              entity.z, 1.5f, 1.5f, 1.5f},
                       rippleColor, entity.spawnTime, gameTime, aspect);
        } else if(entity.type == EntityType::ScoreCollectible) {
            constexpr float rippleColor[3] = {0.04f, 0.85f, 0.30f};
            float aspect = 1.0f;
            unsigned int tex = getCollectibleTexture(entity, aspect);
            drawSprite(viewProj.values, tex, {laneToX(entity.lane) + worldLateralOffset(entity.z),
                                              0.82f - worldCurvatureDrop(entity.z),
                                              entity.z, 1.5f, 1.5f, 1.5f},
                       rippleColor, entity.spawnTime, gameTime, aspect);
        }
    }

    glUseProgram(shaderProgram_);
    glBindVertexArray(quadVao_);
    for (const CollectEffect& effect : collectEffects) {
        drawCollectEffect(viewProj.values, effect, gameTime);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glUseProgram(shaderProgram_);
    glBindVertexArray(cubeVao_);
}

void Renderer::drawCube(const std::array<float, 16>& viewProj, const Transform& transform, const Color& color) const {
    const Mat4 model = multiply(
        translation({transform.x, transform.y, transform.z}),
        scale({transform.scaleX, transform.scaleY, transform.scaleZ})
    );
    Mat4 vp;
    vp.values = viewProj;
    const Mat4 mvp = multiply(vp, model);

    glUniformMatrix4fv(cubeMvpUniform_, 1, GL_FALSE, mvp.values.data());
    glUniform3f(cubeColorUniform_, color.r, color.g, color.b);
    glUniform3f(cubeSunDirUniform_, kSunDir.x, kSunDir.y, kSunDir.z);
    glUniform1f(cubeAmbientUniform_, kSunAmbient);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void Renderer::drawTexturedQuad(const std::array<float, 16>& viewProj, unsigned int texture, const Transform& transform,
                                int face, float texScaleU, float texScaleV, float lightFactor) const {
    Mat4 rotation = identity();
    if (face == static_cast<int>(Face::Side)) { rotation = rotateY(kPi * 0.5f); }
    else if (face == static_cast<int>(Face::Top)) { rotation = rotateX(kPi * 0.5f); }

    const Mat4 model = multiply(
        translation({transform.x, transform.y, transform.z}),
        multiply(rotation, scale({transform.scaleX, transform.scaleY, transform.scaleZ}))
    );
    Mat4 vp;
    vp.values = viewProj;
    const Mat4 mvp = multiply(vp, model);

    const SpriteTexture* tex = findSpriteTexture(texture);
    const float texelU = static_cast<float>(tex->width) * texScaleU;
    const float texelV = static_cast<float>(tex->height) * texScaleV;
    const float lod = computeMipLevel(transform.x, transform.y, transform.z,
                                      transform.scaleX, transform.scaleY, texelU, texelV);

    glUniformMatrix4fv(roadMvpUniform_, 1, GL_FALSE, mvp.values.data());
    glUniform1i(roadTextureUniform_, 0);
    glUniform2f(roadTexScaleUniform_, texScaleU, texScaleV);
    glUniform2f(roadTexOffsetUniform_, 0.0f, 0.0f);
    glUniform1f(roadLodUniform_, lod);
    glUniform1f(roadLightUniform_, lightFactor);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

float Renderer::computeMipLevel(float centerX, float centerY, float centerZ,
                               float worldU, float worldV, float texelU, float texelV) const {
    const float dX = centerX - 0.0f;
    const float dY = centerY - 5.0f;
    const float dZ = centerZ - 9.0f;
    const float distance = std::sqrt(dX * dX + dY * dY + dZ * dZ);

    const float viewHalfHeight = distance * 0.5773503f;
    const float worldPerTexel = std::max(worldU / texelU, worldV / texelV);
    const float pixelsPerTexel = worldPerTexel * static_cast<float>(drawableHeight_) /
                                 (viewHalfHeight * 2.0f);

    const float lod = std::ceil(std::log2(1.0f / pixelsPerTexel));
    return std::max(0.0f, std::min(8.0f, lod));
}

const Renderer::SpriteTexture* Renderer::findSpriteTexture(unsigned int id) const {
    const std::array<const SpriteTexture*, 6> allTextures = {
        &roadTexture_, &skyscraperTexture_, &wallTexture_,
        &waterTexture_, &saladTexture_, &salmonTexture_
    };
    for (const SpriteTexture* tex : allTextures) {
        if (tex->id == id) { return tex; }
    }
    static const SpriteTexture fallback{0, 128, 256};
    return &fallback;
}

void Renderer::drawColoredQuad(const std::array<float, 16>& viewProj, const Color& color, const Transform& transform,
                               int face, float lightFactor, float alpha) const {
    Mat4 rotation = identity();
    if (face == static_cast<int>(Face::Side)) { rotation = rotateY(kPi * 0.5f); }
    else if (face == static_cast<int>(Face::Top)) { rotation = rotateX(kPi * 0.5f); }

    const Mat4 model = multiply(
        translation({transform.x, transform.y, transform.z}),
        multiply(rotation, scale({transform.scaleX, transform.scaleY, transform.scaleZ}))
    );
    Mat4 vp;
    vp.values = viewProj;
    const Mat4 mvp = multiply(vp, model);

    glUniformMatrix4fv(mvpUniform_, 1, GL_FALSE, mvp.values.data());
    glUniform3f(colorUniform_, color.r * lightFactor, color.g * lightFactor, color.b * lightFactor);
    glUniform1f(alphaUniform_, alpha);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::drawShadow(const std::array<float, 16>& viewProj, float centerX, float centerZ,
                          float width, float depth, float y, float alpha) const {
    constexpr Color shadowColor = {0.02f, 0.02f, 0.03f};
    const float drop = worldCurvatureDrop(centerZ);

    struct Shell { float scale; float alphaScale; };
    constexpr Shell shells[] = {
        {1.00f, 1.00f},
        {1.18f, 0.60f},
        {1.42f, 0.30f},
        {1.72f, 0.14f},
    };
    for (const Shell& shell : shells) {
        drawColoredQuad(viewProj, shadowColor,
                        {centerX, y - drop, centerZ, width * shell.scale, depth * shell.scale, 1.0f},
                        static_cast<int>(Face::Top), 1.0f, alpha * shell.alphaScale);
    }
}

void Renderer::drawSunShadow(const std::array<float, 16>& viewProj, float centerX, float centerZ,
                             float height, float width, float depth, float y, float alpha) const {
    const Vec3 lightDir = {-kSunDir.x, -kSunDir.y, -kSunDir.z};
    const float horiz = std::sqrt(lightDir.x * lightDir.x + lightDir.z * lightDir.z);
    if (horiz < 1e-4f) { return; }
    const float lx = lightDir.x / horiz;
    const float lz = lightDir.z / horiz;
    const float perpX = -lz;
    const float perpZ = lx;
    const float len = height * std::abs(horiz / lightDir.y);

    const float wAlong = (std::abs(width * lx) + std::abs(depth * lz)) * 0.5f;
    const float wPerp = (std::abs(width * perpX) + std::abs(depth * perpZ)) * 0.5f;
    const float maxLen = 2.0f * wAlong + len;

    constexpr Color shadowColor = {0.02f, 0.02f, 0.03f};

    struct Layer { float start; float end; float alphaScale; };
    constexpr Layer layers[] = {
        {0.00f, 1.00f, 0.80f},
        {0.22f, 1.00f, 0.60f},
        {0.44f, 1.00f, 0.42f},
        {0.66f, 1.00f, 0.26f},
        {0.84f, 1.00f, 0.14f},
    };
    Mat4 vp;
    vp.values = viewProj;

    for (const Layer& layer : layers) {
        const float s0 = layer.start * maxLen;
        const float s1 = layer.end * maxLen;
        const float segLen = s1 - s0;
        const float mid = (s0 + s1) * 0.5f;

        const float px = centerX + lx * (mid - wAlong);
        const float pz = centerZ + lz * (mid - wAlong);
        const float drop = worldCurvatureDrop(pz);

        const float penumbra = 1.0f + (s0 / maxLen) * 0.60f;

        struct Feather { float widthScale; float alphaScale; };
        constexpr Feather feathers[] = {
            {1.00f, 1.00f},
            {1.14f, 0.30f},
            {1.30f, 0.10f},
        };
        for (const Feather& feather : feathers) {
            const float shadowWidth = 2.0f * wPerp * penumbra * feather.widthScale;

            Mat4 model{};
            model.values[0] = lx * segLen;
            model.values[2] = lz * segLen;
            model.values[4] = perpX * shadowWidth;
            model.values[6] = perpZ * shadowWidth;
            model.values[8] = 0.0f;
            model.values[9] = 1.0f;
            model.values[10] = 0.0f;
            model.values[12] = px;
            model.values[13] = y - drop;
            model.values[14] = pz;
            model.values[15] = 1.0f;

            const Mat4 mvp = multiply(vp, model);

            glUniformMatrix4fv(mvpUniform_, 1, GL_FALSE, mvp.values.data());
            glUniform3f(colorUniform_, shadowColor.r, shadowColor.g, shadowColor.b);
            glUniform1f(alphaUniform_, alpha * layer.alphaScale * feather.alphaScale);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }
}

void Renderer::drawSun(const std::array<float, 16>& viewProj) const {
    constexpr float sunX = -8.0f, sunY = 17.0f, sunZ = -36.0f;
    constexpr Color sunColor = {1.0f, 1.0f, 1.0f};

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glUseProgram(sunShaderProgram_);
    glBindVertexArray(quadVao_);

    Mat4 vp;
    vp.values = viewProj;
    const Mat4 model = multiply(
        translation({sunX, sunY, sunZ}),
        scale({6.0f, 6.0f, 1.0f})
    );
    const Mat4 mvp = multiply(vp, model);

    glUniformMatrix4fv(sunMvpUniform_, 1, GL_FALSE, mvp.values.data());
    glUniform3f(sunColorUniform_, sunColor.r, sunColor.g, sunColor.b);
    glUniform1f(sunCoreUniform_, 6.5f);
    glUniform1f(sunHazeUniform_, 2.2f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::drawCollectEffect(const std::array<float, 16>& viewProj, const CollectEffect& effect,
                                 float gameTime) const {
    const float age = gameTime - effect.time;
    constexpr float kEffectDuration = 0.7f;
    if (age < 0.0f || age > kEffectDuration) { return; }

    const float t = age / kEffectDuration;
    const float x = laneToX(effect.lane) + worldLateralOffset(effect.z);
    const float z = effect.z;
    const float drop = worldCurvatureDrop(z);

    Color color = {0.04f, 0.85f, 0.30f};
    if (effect.type == EntityType::SlowCollectible) {
        color = {0.9f, 0.15f, 0.15f};
    }
    constexpr Color white = {1.0f, 1.0f, 1.0f};
    const Color bright = {std::min(color.r * 1.5f + 0.2f, 1.0f),
                          std::min(color.g * 1.5f + 0.2f, 1.0f),
                          std::min(color.b * 1.5f + 0.2f, 1.0f)};

    const float life = std::max(0.0f, 1.0f - t);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    constexpr float kShards = 18.0f;
    const int shardCount = static_cast<int>(kShards);
    const float radius = 0.12f + t * t * 2.0f;

    for (int i = 0; i < shardCount; ++i) {
        const float angle = static_cast<float>(i) / kShards * 2.0f * kPi;
        const float shardX = x + std::cos(angle) * radius;
        const float shardZ = z + std::sin(angle) * radius * 0.55f;
        const float shardSize = std::max(0.05f, 0.24f - t * 0.13f);
        drawColoredQuad(viewProj, bright,
                        {shardX, -0.02f + t * 0.4f - drop, shardZ, shardSize, shardSize, 1.0f},
                        static_cast<int>(Face::Top), 1.0f, life * 0.95f);
    }

    constexpr float kSparks = 14.0f;
    const int sparkCount = static_cast<int>(kSparks);
    for (int i = 0; i < sparkCount; ++i) {
        const float angle = (static_cast<float>(i) / kSparks + 0.5f / kSparks) * 2.0f * kPi;
        const float sparkRadius = radius * (0.85f + t * 0.35f);
        const float sparkX = x + std::cos(angle) * sparkRadius * 1.25f;
        const float sparkZ = z + std::sin(angle) * sparkRadius * 0.6f;
        const float sparkSize = std::max(0.04f, 0.11f - t * 0.06f);
        drawColoredQuad(viewProj, white,
                        {sparkX, -0.01f + t * 0.6f - drop, sparkZ, sparkSize, sparkSize, 1.0f},
                        static_cast<int>(Face::Top), 1.0f, life * 0.8f);
    }

    constexpr float kRingRays = 16.0f;
    for (int i = 0; i < static_cast<int>(kRingRays); ++i) {
        const float angle = static_cast<float>(i) / kRingRays * 2.0f * kPi;
        const float rayX = x + std::cos(angle) * (radius * 0.7f);
        const float rayZ = z + std::sin(angle) * (radius * 0.55f);
        const float raySize = std::max(0.05f, 0.30f * (1.0f - t * 0.5f));
        drawColoredQuad(viewProj, bright,
                        {rayX, 0.35f - drop, rayZ, raySize, raySize, 1.0f},
                        static_cast<int>(Face::FrontBack), 1.0f, life * 0.6f);
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::drawSprite(const std::array<float, 16>& viewProj, unsigned int texture, const Transform& transform,
                          const float rippleColor[3], float spawnTime, float gameTime, float texAspect) const {
    float bobY = transform.y + std::sin(gameTime * 3.0f + spawnTime) * 0.08f;

    float scaleX = transform.scaleX;
    float scaleY = transform.scaleY;
    if (texAspect >= 1.0f) {
        scaleY = scaleY / texAspect;
    } else {
        scaleX = scaleX * texAspect;
    }

    const Mat4 model = multiply(
        translation({transform.x, bobY, transform.z}),
        scale({scaleX, scaleY, transform.scaleZ})
    );
    Mat4 vp;
    vp.values = viewProj;
    const Mat4 mvp = multiply(vp, model);

    glUniformMatrix4fv(spriteMvpUniform_, 1, GL_FALSE, mvp.values.data());
    glUniform1f(spriteTimeUniform_, gameTime);
    glUniform1f(spriteSpawnTimeUniform_, spawnTime);
    glUniform3f(spriteRippleColorUniform_, rippleColor[0], rippleColor[1], rippleColor[2]);
    glUniform1f(spriteAspectUniform_, scaleX / scaleY);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(spriteTextureUniform_, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::shutdownGraphics(){
    if(roadTexture_.id!=0){ glDeleteTextures(1, &roadTexture_.id); roadTexture_ = {}; }
    if(skyscraperTexture_.id!=0){ glDeleteTextures(1, &skyscraperTexture_.id); skyscraperTexture_ = {}; }
    if(wallTexture_.id!=0){ glDeleteTextures(1, &wallTexture_.id); wallTexture_ = {}; }
    if(waterTexture_.id!=0){ glDeleteTextures(1, &waterTexture_.id); waterTexture_ = {}; }
    if(saladTexture_.id!=0){ glDeleteTextures(1, &saladTexture_.id); saladTexture_ = {}; }
    if(salmonTexture_.id!=0){ glDeleteTextures(1, &salmonTexture_.id); salmonTexture_ = {}; }
    if(sodaTexture_.id!=0){ glDeleteTextures(1, &sodaTexture_.id); sodaTexture_ = {}; }
    if(burgerTexture_.id!=0){ glDeleteTextures(1, &burgerTexture_.id); burgerTexture_ = {}; }
    if(friesTexture_.id!=0){ glDeleteTextures(1, &friesTexture_.id); friesTexture_ = {}; }
    if(quadVbo_!=0) { glDeleteBuffers(1, &quadVbo_); quadVbo_ = 0; }
    if(quadVao_!=0){ glDeleteVertexArrays(1, &quadVao_); quadVao_ = 0; }
    if(roadShaderProgram_!=0){ glDeleteProgram(roadShaderProgram_); roadShaderProgram_ = 0; }
    if(spriteShaderProgram_!=0){ glDeleteProgram(spriteShaderProgram_); spriteShaderProgram_ = 0; }
    if(skyShaderProgram_!=0){ glDeleteProgram(skyShaderProgram_); skyShaderProgram_ = 0; }
    if(sunShaderProgram_!=0){ glDeleteProgram(sunShaderProgram_); sunShaderProgram_ = 0; }
    if(cubeShaderProgram_!=0){ glDeleteProgram(cubeShaderProgram_); cubeShaderProgram_ = 0; }
    if(cubeVbo_!=0) { glDeleteBuffers(1, &cubeVbo_); cubeVbo_ = 0; }
    if(cubeVao_!=0){ glDeleteVertexArrays(1, &cubeVao_); cubeVao_ = 0; }
    if(shaderProgram_!=0){ glDeleteProgram(shaderProgram_); shaderProgram_ = 0; }
}

void Renderer::shutdown() {
    shutdownGraphics();

    if(glContext_){ SDL_GL_DeleteContext(glContext_); glContext_ = nullptr; }
    if(window_){ SDL_DestroyWindow(window_); window_ = nullptr; }
    if(sdlInitialized_){ SDL_QuitSubSystem(SDL_INIT_VIDEO); sdlInitialized_ = false; }
}
