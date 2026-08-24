#include "Renderer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <glad/glad.h>

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kOpenGLMajorVersion = 4;
constexpr int kOpenGLMinorVersion = 6;
constexpr int kVsyncEnabled = 1;
constexpr const char* kWindowTitle = "Subway Money";

constexpr int kLaneCount = 5;
constexpr std::array<float, kLaneCount> kLanePositions = {-2.2f, 0.0f, 2.2f};
constexpr float kPlayerZ = 3.0f;
constexpr float kPlayerScale = 0.9f;
constexpr float kLaneChangeSpeed = 12.0f;
constexpr float kInitialSpeed = 10.0f;
constexpr float kMinSpeed = 4.0f;
constexpr float kMaxSpeed = 18.0f;
constexpr int kMaxHealth = 3;
constexpr float kSpawnZ = -65.0f;
constexpr float kDespawnZ = 8.0f;
constexpr float kCollisionZRange = 0.95f;
constexpr float kCollisionXRange = 0.85f;
constexpr float kLaneSegmentLength = 12.0f;
constexpr float kLaneStartZ = -86.0f;
constexpr int kLaneSegmentCount = 10;
constexpr float kPi = 3.14159265358979323846f;

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Mat4 {
    std::array<float, 16> values{};
};

bool setGLAttribute(SDL_GLattr attribute, int value, const char* name) {
    if (SDL_GL_SetAttribute(attribute, value) == 0) {
        return true;
    }

    std::cerr << "Failed to set " << name << ": " << SDL_GetError() << '\n';
    return false;
}

float laneToX(int lane) {
    const int clampedLane = std::clamp(lane, 0, kLaneCount - 1);
    return kLanePositions[static_cast<std::size_t>(clampedLane)];
}

float radians(float degrees) {
    return degrees * kPi / 180.0f;
}

Vec3 operator-(const Vec3& left, const Vec3& right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

float dot(const Vec3& left, const Vec3& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vec3 cross(const Vec3& left, const Vec3& right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    };
}

Vec3 normalize(const Vec3& value) {
    const float length = std::sqrt(dot(value, value));
    if (length <= 0.0001f) {
        return {0.0f, 0.0f, 0.0f};
    }

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
    if (success != 0) {
        return shader;
    }

    std::array<char, 1024> infoLog{};
    glGetShaderInfoLog(shader, static_cast<int>(infoLog.size()), nullptr, infoLog.data());
    std::cerr << label << " shader compilation failed: " << infoLog.data() << '\n';
    glDeleteShader(shader);
    return 0;
}
}

Renderer::Renderer()
    : window_(nullptr),
      glContext_(nullptr),
      sdlInitialized_(false),
      running_(false),
      gameOver_(false),
      cubeVao_(0),
      cubeVbo_(0),
      shaderProgram_(0),
      mvpUniform_(-1),
      colorUniform_(-1),
      drawableWidth_(kWindowWidth),
      drawableHeight_(kWindowHeight),
      targetLane_(1),
      playerX_(laneToX(targetLane_)),
      speed_(kInitialSpeed),
      spawnTimer_(1.0f),
      roadScroll_(0.0f),
      titleUpdateTimer_(0.0f),
      scoreAccumulator_(0.0f),
      score_(0),
      health_(kMaxHealth),
      purpleStreak_(0),
      randomEngine_(std::random_device{}()) {
}

Renderer::~Renderer() {
    shutdown();
}

int Renderer::initSDL() {
    if (window_ && glContext_) {
        return 0;
    }

    if (window_ || glContext_ || sdlInitialized_) {
        shutdown();
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization error: " << SDL_GetError() << '\n';
        return -1;
    }
    sdlInitialized_ = true;

    if (!setOpenGLAttributes() || !createWindow() || !createOpenGLContext() || !initializeGlad() || !initializeGraphics()) {
        shutdown();
        return -1;
    }

    if (SDL_GL_SetSwapInterval(kVsyncEnabled) != 0) {
        std::cerr << "VSync setup warning: " << SDL_GetError() << '\n';
    }

    SDL_GL_GetDrawableSize(window_, &drawableWidth_, &drawableHeight_);
    glViewport(0, 0, drawableWidth_, drawableHeight_);

    resetGame();
    return 0;
}

void Renderer::run() {
    using Clock = std::chrono::steady_clock;

    running_ = true;
    auto previousFrame = Clock::now();

    while (running_) {
        const auto currentFrame = Clock::now();
        const std::chrono::duration<float> elapsed = currentFrame - previousFrame;
        previousFrame = currentFrame;

        const float deltaTime = std::min(elapsed.count(), 0.05f);
        handleEvents();
        update(deltaTime);
        render();
    }
}

void Renderer::render() {
    if (!window_ || shaderProgram_ == 0) {
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgram_);
    glBindVertexArray(cubeVao_);

    drawWorld();

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

    if (window_) {
        return true;
    }

    std::cerr << "Window creation error: " << SDL_GetError() << '\n';
    return false;
}

bool Renderer::createOpenGLContext() {
    glContext_ = SDL_GL_CreateContext(window_);
    if (!glContext_) {
        std::cerr << "OpenGL context error: " << SDL_GetError() << '\n';
        return false;
    }

    if (SDL_GL_MakeCurrent(window_, glContext_) != 0) {
        std::cerr << "Failed to make OpenGL context current: " << SDL_GetError() << '\n';
        return false;
    }

    return true;
}

bool Renderer::initializeGlad() const {
    if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)) != 0) {
        return true;
    }

    std::cerr << "Failed to initialize GLAD\n";
    return false;
}

bool Renderer::initializeGraphics() {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.05f, 0.07f, 0.10f, 1.0f);

    return createShaderProgram() && createCubeMesh();
}

bool Renderer::createShaderProgram() {
    constexpr const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPosition;

uniform mat4 uMvp;

void main() {
    gl_Position = uMvp * vec4(aPosition, 1.0);
}
)";

    constexpr const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

uniform vec3 uColor;

void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

    const unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource, "Vertex");
    if (vertexShader == 0) {
        return false;
    }

    const unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource, "Fragment");
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    shaderProgram_ = glCreateProgram();
    glAttachShader(shaderProgram_, vertexShader);
    glAttachShader(shaderProgram_, fragmentShader);
    glLinkProgram(shaderProgram_);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int success = 0;
    glGetProgramiv(shaderProgram_, GL_LINK_STATUS, &success);
    if (success == 0) {
        std::array<char, 1024> infoLog{};
        glGetProgramInfoLog(shaderProgram_, static_cast<int>(infoLog.size()), nullptr, infoLog.data());
        std::cerr << "Shader program linking failed: " << infoLog.data() << '\n';
        glDeleteProgram(shaderProgram_);
        shaderProgram_ = 0;
        return false;
    }

    mvpUniform_ = glGetUniformLocation(shaderProgram_, "uMvp");
    colorUniform_ = glGetUniformLocation(shaderProgram_, "uColor");
    if (mvpUniform_ < 0 || colorUniform_ < 0) {
        std::cerr << "Shader program is missing required uniforms\n";
        glDeleteProgram(shaderProgram_);
        shaderProgram_ = 0;
        return false;
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

    if (cubeVao_ == 0 || cubeVbo_ == 0) {
        std::cerr << "Failed to create cube mesh buffers\n";
        return false;
    }

    glBindVertexArray(cubeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(cubeVertices.size() * sizeof(float)),
        cubeVertices.data(),
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * static_cast<int>(sizeof(float)), nullptr);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

void Renderer::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            running_ = false;
            continue;
        }

        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            SDL_GL_GetDrawableSize(window_, &drawableWidth_, &drawableHeight_);
            glViewport(0, 0, drawableWidth_, drawableHeight_);
            continue;
        }

        if (event.type != SDL_KEYDOWN || event.key.repeat != 0) {
            continue;
        }

        switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                running_ = false;
                break;
            case SDLK_LEFT:
            case SDLK_a:
                if (!gameOver_) {
                    targetLane_ = std::max(0, targetLane_ - 1);
                }
                break;
            case SDLK_RIGHT:
            case SDLK_d:
                if (!gameOver_) {
                    targetLane_ = std::min(kLaneCount - 1, targetLane_ + 1);
                }
                break;
            case SDLK_r:
                resetGame();
                break;
            default:
                break;
        }
    }
}

void Renderer::update(float deltaTime) {
    if (gameOver_) {
        return;
    }

    updatePlayer(deltaTime);
    updateWorld(deltaTime);
    updateEntities(deltaTime);
    updateCollisions();

    scoreAccumulator_ += speed_ * deltaTime * 3.0f;
    const int distancePoints = static_cast<int>(scoreAccumulator_);
    if (distancePoints > 0) {
        score_ += distancePoints;
        scoreAccumulator_ -= static_cast<float>(distancePoints);
    }

    titleUpdateTimer_ -= deltaTime;
    if (titleUpdateTimer_ <= 0.0f) {
        updateWindowTitle();
        titleUpdateTimer_ = 0.15f;
    }
}

void Renderer::updatePlayer(float deltaTime) {
    const float targetX = laneToX(targetLane_);
    const float blend = std::min(1.0f, kLaneChangeSpeed * deltaTime);
    playerX_ += (targetX - playerX_) * blend;
}

void Renderer::updateWorld(float deltaTime) {
    roadScroll_ = std::fmod(roadScroll_ + speed_ * deltaTime, kLaneSegmentLength);
    if (roadScroll_ < 0.0f) {
        roadScroll_ += kLaneSegmentLength;
    }

    spawnTimer_ -= deltaTime;
    if (spawnTimer_ <= 0.0f) {
        spawnEntity();

        std::uniform_real_distribution<float> delayDistribution(0.55f, 1.15f);
        spawnTimer_ = delayDistribution(randomEngine_);
    }
}

void Renderer::updateEntities(float deltaTime) {
    for (Entity& entity : entities_) {
        entity.z += speed_ * deltaTime;
    }

    entities_.erase(
        std::remove_if(
            entities_.begin(),
            entities_.end(),
            [](const Entity& entity) {
                return entity.z > kDespawnZ;
            }
        ),
        entities_.end()
    );
}

void Renderer::updateCollisions() {
    for (auto entity = entities_.begin(); entity != entities_.end();) {
        const float entityX = laneToX(entity->lane);
        const bool touchesPlayer = std::abs(entity->z - kPlayerZ) <= kCollisionZRange &&
                                   std::abs(entityX - playerX_) <= kCollisionXRange;

        if (!touchesPlayer) {
            ++entity;
            continue;
        }

        switch (entity->type) {
            case EntityType::Obstacle:
                health_ -= 1;
                speed_ = std::max(kMinSpeed, speed_ - 2.5f);
                if (health_ <= 0) {
                    gameOver_ = true;
                }
                break;
            case EntityType::SlowCollectible:
                purpleStreak_ += 1;
                speed_ = std::max(kMinSpeed, speed_ - 1.8f);
                if (purpleStreak_ >= 3) {
                    gameOver_ = true;
                }
                break;
            case EntityType::ScoreCollectible:
                score_ += 100;
                health_ = std::min(kMaxHealth, health_ + 1);
                purpleStreak_ = 0;
                speed_ = std::min(kMaxSpeed, speed_ + 1.5f);
                break;
        }

        entity = entities_.erase(entity);
        updateWindowTitle();
    }
}

void Renderer::spawnEntity() {
    std::uniform_int_distribution<int> laneDistribution(0, kLaneCount - 1);
    std::uniform_real_distribution<float> typeDistribution(0.0f, 1.0f);

    const float typeRoll = typeDistribution(randomEngine_);
    EntityType type = EntityType::Obstacle;
    if (typeRoll > 0.78f) {
        type = EntityType::ScoreCollectible;
    } else if (typeRoll > 0.56f) {
        type = EntityType::SlowCollectible;
    }

    entities_.push_back({type, laneDistribution(randomEngine_), kSpawnZ});
}

void Renderer::resetGame() {
    gameOver_ = false;
    targetLane_ = 1;
    playerX_ = laneToX(targetLane_);
    speed_ = kInitialSpeed;
    spawnTimer_ = 1.0f;
    roadScroll_ = 0.0f;
    titleUpdateTimer_ = 0.0f;
    scoreAccumulator_ = 0.0f;
    score_ = 0;
    health_ = kMaxHealth;
    purpleStreak_ = 0;
    entities_.clear();
    updateWindowTitle();
}

void Renderer::updateWindowTitle() {
    if (!window_) {
        return;
    }

    std::ostringstream title;
    title << kWindowTitle;

    if (gameOver_) {
        title << " | Game Over - Press R | Score " << score_;
    } else {
        title << " | Score " << score_
              << " | Health " << health_
              << " | Speed " << std::fixed << std::setprecision(1) << speed_
              << " | Purple " << purpleStreak_ << "/3";
    }

    SDL_SetWindowTitle(window_, title.str().c_str());
}

void Renderer::drawWorld() const {
    constexpr Color laneColor = {0.17f, 0.19f, 0.21f};
    constexpr Color alternateLaneColor = {0.13f, 0.15f, 0.17f};
    constexpr Color dividerColor = {0.95f, 0.85f, 0.22f};
    constexpr Color playerColor = {0.12f, 0.55f, 1.0f};
    constexpr Color obstacleColor = {0.95f, 0.08f, 0.06f};
    constexpr Color slowCollectibleColor = {0.62f, 0.18f, 0.95f};
    constexpr Color scoreCollectibleColor = {0.04f, 0.85f, 0.30f};

    const float scrollOffset = std::fmod(roadScroll_, kLaneSegmentLength);
    for (int segment = 0; segment < kLaneSegmentCount; ++segment) {
        const float z = kLaneStartZ + scrollOffset + static_cast<float>(segment) * kLaneSegmentLength;
        for (int lane = 0; lane < kLaneCount; ++lane) {
            const Color color = ((segment + lane) % 2 == 0) ? laneColor : alternateLaneColor;
            drawCube({laneToX(lane), -0.08f, z, 1.85f, 0.08f, kLaneSegmentLength - 0.15f}, color);
        }

        for (int lane = 0; lane < kLaneCount - 1; ++lane) {
            const float dividerX = (laneToX(lane) + laneToX(lane + 1)) * 0.5f;
            drawCube({dividerX, 0.01f, z, 0.08f, 0.06f, kLaneSegmentLength * 0.55f}, dividerColor);
        }
    }

    drawCube({playerX_, kPlayerScale * 0.5f, kPlayerZ, kPlayerScale, kPlayerScale, kPlayerScale}, playerColor);

    for (const Entity& entity : entities_) {
        switch (entity.type) {
            case EntityType::Obstacle:
                drawCube({laneToX(entity.lane), 0.48f, entity.z, 0.95f, 0.95f, 0.95f}, obstacleColor);
                break;
            case EntityType::SlowCollectible:
                drawCube({laneToX(entity.lane), 0.42f, entity.z, 0.68f, 0.68f, 0.68f}, slowCollectibleColor);
                break;
            case EntityType::ScoreCollectible:
                drawCube({laneToX(entity.lane), 0.42f, entity.z, 0.68f, 0.68f, 0.68f}, scoreCollectibleColor);
                break;
        }
    }
}

void Renderer::drawCube(const Transform& transform, const Color& color) const {
    const float aspectRatio = static_cast<float>(std::max(1, drawableWidth_)) /
                              static_cast<float>(std::max(1, drawableHeight_));

    const Mat4 projection = perspective(radians(60.0f), aspectRatio, 0.1f, 200.0f);
    const Mat4 view = lookAt({0.0f, 6.0f, 11.0f}, {0.0f, 0.25f, -18.0f}, {0.0f, 1.0f, 0.0f});
    const Mat4 model = multiply(
        translation({transform.x, transform.y, transform.z}),
        scale({transform.scaleX, transform.scaleY, transform.scaleZ})
    );
    const Mat4 viewModel = multiply(view, model);
    const Mat4 mvp = multiply(projection, viewModel);

    glUniformMatrix4fv(mvpUniform_, 1, GL_FALSE, mvp.values.data());
    glUniform3f(colorUniform_, color.r, color.g, color.b);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void Renderer::shutdownGraphics() {
    if (cubeVbo_ != 0) {
        glDeleteBuffers(1, &cubeVbo_);
        cubeVbo_ = 0;
    }

    if (cubeVao_ != 0) {
        glDeleteVertexArrays(1, &cubeVao_);
        cubeVao_ = 0;
    }

    if (shaderProgram_ != 0) {
        glDeleteProgram(shaderProgram_);
        shaderProgram_ = 0;
    }
}

void Renderer::shutdown() {
    shutdownGraphics();

    if (glContext_) {
        SDL_GL_DeleteContext(glContext_);
        glContext_ = nullptr;
    }

    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    if (sdlInitialized_) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        sdlInitialized_ = false;
    }
}
