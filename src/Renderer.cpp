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
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
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
      spriteShaderProgram_(0),
      spriteMvpUniform_(-1),
      spriteTextureUniform_(-1),
      spriteTimeUniform_(-1),
      spriteSpawnTimeUniform_(-1),
      spriteRippleColorUniform_(-1),
      spriteGlowBorderUniform_(-1),
      quadVao_(0),
      quadVbo_(0),
      waterTexture_(),
      saladTexture_(),
      salmonTexture_(),
      sodaTexture_(),
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

void Renderer::render(float playerX, float roadScroll, float gameTime, const std::vector<Entity>& entities) {
    if(!window_ || shaderProgram_ == 0) { return; }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgram_);
    glBindVertexArray(cubeVao_);

    drawWorld(playerX, roadScroll, gameTime, entities);

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
    glClearColor(0.05f, 0.07f, 0.10f, 1.0f);

    return createShaderProgram() && createSpriteShaderProgram() && createCubeMesh() && createQuadMesh() && loadCollectibleTextures();
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

void main() { FragColor = vec4(uColor, 1.0); }
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
    if (mvpUniform_ < 0 || colorUniform_ < 0) {
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
uniform bool uHasGlowBorder;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    float alpha = step(0.1, texColor.a);

    float pixelGrid = 12.0;

    vec2 centered = vTexCoord - 0.5;
    float dist = length(centered);
    float pixelDist = floor(dist * pixelGrid + 0.5) / pixelGrid;

    float ripple = sin(pixelDist * 14.0 - uTime * 5.0 + uSpawnTime * 2.0) * 0.5 + 0.5;

    vec3 rippleContrib = uRippleColor * ripple;

    vec3 finalColor = texColor.rgb * 0.45 + rippleContrib * 0.55;
    finalColor *= alpha;

    if (uHasGlowBorder) {
        float edgeDist = min(min(vTexCoord.x, 1.0 - vTexCoord.x),
                             min(vTexCoord.y, 1.0 - vTexCoord.y));
        float glow = smoothstep(0.12, 0.0, edgeDist);
        float glowPulse = 0.6 + 0.4 * sin(uTime * 2.5 + uSpawnTime * 1.7);
        vec3 glowColor = vec3(0.1, 1.0, 0.3) * glow * glowPulse * 0.6;
        finalColor += glowColor * alpha;
    }

    FragColor = vec4(finalColor, alpha);
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
    spriteGlowBorderUniform_ = glGetUniformLocation(spriteShaderProgram_, "uHasGlowBorder");

    if (spriteMvpUniform_ < 0 || spriteTextureUniform_ < 0 || spriteTimeUniform_ < 0 ||
        spriteSpawnTimeUniform_ < 0 || spriteRippleColorUniform_ < 0 || spriteGlowBorderUniform_ < 0) {
        std::cerr << "Sprite shader program is missing required uniforms\n";
        glDeleteProgram(spriteShaderProgram_); spriteShaderProgram_ = 0; return false;
    }

    return true;
}

bool Renderer::createCubeMesh() {
    constexpr std::array<float, 108> cubeVertices = {
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,

        -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,

        -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,

         0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,

        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f,

        -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f
    };

    glGenVertexArrays(1, &cubeVao_);
    glGenBuffers(1, &cubeVbo_);

    if(cubeVao_ == 0 || cubeVbo_ == 0) { std::cerr << "Failed to create cube mesh buffers\n"; return false; }

    glBindVertexArray(cubeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(cubeVertices.size() * sizeof(float)),
                       cubeVertices.data(), GL_STATIC_DRAW );

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * static_cast<int>(sizeof(float)), nullptr);
    glEnableVertexAttribArray(0);

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

unsigned int Renderer::loadTexture(const char* path, int& outWidth, int& outHeight) {
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

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, outWidth, outHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    std::cout << "Loaded texture: " << path << " (" << outWidth << "x" << outHeight << ")\n";
    return texture;
}

bool Renderer::loadCollectibleTextures() {
    waterTexture_.id = loadTexture(ASSETS_DIR "/water.png", waterTexture_.width, waterTexture_.height);
    saladTexture_.id = loadTexture(ASSETS_DIR "/salad.png", saladTexture_.width, saladTexture_.height);
    salmonTexture_.id = loadTexture(ASSETS_DIR "/salmon.png", salmonTexture_.width, salmonTexture_.height);
    sodaTexture_.id = loadTexture(ASSETS_DIR "/soda.png", sodaTexture_.width, sodaTexture_.height);

    if (waterTexture_.id == 0 || saladTexture_.id == 0 || salmonTexture_.id == 0 || sodaTexture_.id == 0) {
        std::cerr << "Failed to load one or more collectible textures\n";
        return false;
    }
    return true;
}

unsigned int Renderer::getCollectibleTexture(const Entity& entity, float& outAspect) const {
    if (entity.type == EntityType::SlowCollectible) {
        outAspect = static_cast<float>(sodaTexture_.width) / static_cast<float>(sodaTexture_.height);
        return sodaTexture_.id;
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

void Renderer::drawWorld(float playerX, float roadScroll, float gameTime, const std::vector<Entity>& entities) {
    constexpr Color laneColor = {0.17f, 0.19f, 0.21f};
    constexpr Color alternateLaneColor = {0.13f, 0.15f, 0.17f};
    constexpr Color dividerColor = {0.95f, 0.85f, 0.22f};
    constexpr Color playerColor = {0.12f, 0.55f, 1.0f};
    constexpr Color obstacleColor = {0.95f, 0.08f, 0.06f};

    const float aspectRatio = static_cast<float>(std::max(1, drawableWidth_)) /
                              static_cast<float>(std::max(1, drawableHeight_));
    const Mat4 projection = perspective(radians(60.0f), aspectRatio, 0.1f, 200.0f);
    const Mat4 view = lookAt({0.0f, 5.0f, 9.0f}, {0.0f, 0.5f, -18.0f}, {0.0f, 1.0f, 0.0f});
    const Mat4 viewProj = multiply(projection, view);

    const float scrollOffset = std::fmod(roadScroll, kLaneSegmentLength);
    for (int segment = 0; segment < kLaneSegmentCount; ++segment) {
        const float z = kLaneStartZ + scrollOffset + static_cast<float>(segment) * kLaneSegmentLength;
        for (int lane = 0; lane < kLaneCount; ++lane) {
            const Color color = ((segment + lane) % 2 == 0) ? laneColor : alternateLaneColor;
            drawCube(viewProj.values, {laneToX(lane), -0.08f, z, 1.85f, 0.08f, kLaneSegmentLength - 0.15f}, color);
        }

        for (int lane = 0; lane < kLaneCount - 1; ++lane) {
            const float dividerX = (laneToX(lane) + laneToX(lane + 1)) * 0.5f;
            drawCube(viewProj.values, {dividerX, 0.01f, z, 0.08f, 0.06f, kLaneSegmentLength * 0.55f}, dividerColor);
        }

        constexpr float buildingSides[] = {-1.0f, 1.0f};
        constexpr float buildingBaseX[] = {4.5f, 6.5f, 9.0f, 11.5f};
        constexpr int kBuildingsPerSide = 4;

        for (float side : buildingSides) {
            for (int b = 0; b < kBuildingsPerSide; ++b) {
                const unsigned int seed = static_cast<unsigned int>(segment) * 100u + static_cast<unsigned int>(b);
                const float bx = side * buildingBaseX[b];
                const float bh = 1.5f + hashFloat(seed, 0) * 8.0f;
                const float bd = 2.0f + hashFloat(seed, 1) * 6.0f;
                const float bz = z + (hashFloat(seed, 2) - 0.5f) * 4.0f;
                const float g = 0.15f + hashFloat(seed, 3) * 0.25f;
                drawCube(viewProj.values, {bx, bh * 0.5f - 0.08f, bz, 2.5f, bh, bd}, {g, g, g + 0.02f});
            }
        }
    }

    drawCube(viewProj.values, {playerX, kPlayerScale * 0.5f, kPlayerZ, kPlayerScale, kPlayerScale, kPlayerScale}, playerColor);

    for(const Entity& entity : entities){
        if(entity.type == EntityType::Obstacle) {
            drawCube(viewProj.values, {laneToX(entity.lane), 0.48f, entity.z, 0.95f, 0.95f, 0.95f}, obstacleColor);
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
            drawSprite(viewProj.values, tex, {laneToX(entity.lane), 0.42f, entity.z, 1.5f, 1.5f, 1.5f},
                       rippleColor, false, entity.spawnTime, gameTime, aspect);
        } else if(entity.type == EntityType::ScoreCollectible) {
            constexpr float rippleColor[3] = {0.04f, 0.85f, 0.30f};
            float aspect = 1.0f;
            unsigned int tex = getCollectibleTexture(entity, aspect);
            drawSprite(viewProj.values, tex, {laneToX(entity.lane), 0.42f, entity.z, 1.5f, 1.5f, 1.5f},
                       rippleColor, true, entity.spawnTime, gameTime, aspect);
        }
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

    glUniformMatrix4fv(mvpUniform_, 1, GL_FALSE, mvp.values.data());
    glUniform3f(colorUniform_, color.r, color.g, color.b);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void Renderer::drawSprite(const std::array<float, 16>& viewProj, unsigned int texture, const Transform& transform,
                          const float rippleColor[3], bool hasGlowBorder,
                          float spawnTime, float gameTime, float texAspect) const {
    float bobY = transform.y + std::sin(gameTime * 3.0f + spawnTime) * 0.15f;

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
    glUniform1i(spriteGlowBorderUniform_, hasGlowBorder ? 1 : 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(spriteTextureUniform_, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Renderer::shutdownGraphics(){
    if(waterTexture_.id!=0){ glDeleteTextures(1, &waterTexture_.id); waterTexture_ = {}; }
    if(saladTexture_.id!=0){ glDeleteTextures(1, &saladTexture_.id); saladTexture_ = {}; }
    if(salmonTexture_.id!=0){ glDeleteTextures(1, &salmonTexture_.id); salmonTexture_ = {}; }
    if(sodaTexture_.id!=0){ glDeleteTextures(1, &sodaTexture_.id); sodaTexture_ = {}; }
    if(quadVbo_!=0) { glDeleteBuffers(1, &quadVbo_); quadVbo_ = 0; }
    if(quadVao_!=0){ glDeleteVertexArrays(1, &quadVao_); quadVao_ = 0; }
    if(spriteShaderProgram_!=0){ glDeleteProgram(spriteShaderProgram_); spriteShaderProgram_ = 0; }
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
