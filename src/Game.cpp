#include "Game.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>

using namespace GameConfig;

Game::Game()
    : running_(false),
      gameOver_(false),
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
      randomEngine_(std::random_device{}()),
      gameTime_(0.0f),
      accumulator_(0.0f) {
}

int Game::run() {
    if(renderer_.initSDL() != 0) { return 1; }

    using Clock = std::chrono::steady_clock;

    resetGame();
    running_ = true;
    auto previousFrame = Clock::now();

    while(running_) {
        const auto currentFrame = Clock::now();
        const std::chrono::duration<float> elapsed = currentFrame - previousFrame;
        previousFrame = currentFrame;

        const float frameTime = std::min(elapsed.count(), 0.05f);
        accumulator_ += frameTime;

        handleEvents();

        constexpr float fixedStep = 1.0f / 60.0f;
        while(accumulator_ >= fixedStep) {
            update(fixedStep);
            accumulator_ -= fixedStep;
        }

        renderer_.render(playerX_, roadScroll_, gameTime_, entities_);
    }

    return 0;
}

void Game::handleEvents() {
    SDL_Event event;
    while(SDL_PollEvent(&event) != 0) {
        if(event.type == SDL_QUIT) { running_ = false; continue; }

        if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            renderer_.resizeViewport(); continue; }

        if(event.type != SDL_KEYDOWN || event.key.repeat != 0) { continue; }

        switch(event.key.keysym.sym) {
            case SDLK_ESCAPE: running_ = false; break;
            case SDLK_LEFT:
            case SDLK_a: if(!gameOver_) { targetLane_ = std::max(0, targetLane_ - 1); } break;
            case SDLK_RIGHT:
            case SDLK_d: if(!gameOver_) { targetLane_ = std::min(kLaneCount - 1, targetLane_ + 1); } break;
            case SDLK_r: resetGame(); break;
            default: break;
        }
    }
}

void Game::update(float deltaTime) {
    if(gameOver_) { return; }

    gameTime_ += deltaTime;

    updatePlayer(deltaTime);
    updateWorld(deltaTime);
    updateEntities(deltaTime);
    updateCollisions();

    scoreAccumulator_ += speed_ * deltaTime * 3.0f;
    const int distancePoints = static_cast<int>(scoreAccumulator_);
    if(distancePoints > 0) { score_ += distancePoints; scoreAccumulator_ -= static_cast<float>(distancePoints); }

    titleUpdateTimer_ -= deltaTime;
    if(titleUpdateTimer_ <= 0.0f) { updateWindowTitle(); titleUpdateTimer_ = 0.15f; }
}

void Game::updatePlayer(float deltaTime) {
    const float targetX = laneToX(targetLane_);
    const float blend = std::min(1.0f, kLaneChangeSpeed * deltaTime);
    playerX_ += (targetX - playerX_) * blend;
}

void Game::updateWorld(float deltaTime) {
    roadScroll_ = std::fmod(roadScroll_ + speed_ * deltaTime, kLaneSegmentLength);
    if(roadScroll_ < 0.0f) { roadScroll_ += kLaneSegmentLength; }

    spawnTimer_ -= deltaTime;
    if(spawnTimer_ <= 0.0f) {
        spawnEntity();

        std::uniform_real_distribution<float> delayDistribution(0.55f, 1.15f);
        spawnTimer_ = delayDistribution(randomEngine_);
    }
}

void Game::updateEntities(float deltaTime) {
    for(Renderer::Entity& entity : entities_) { entity.z += speed_ * deltaTime; }

    entities_.erase(
        std::remove_if(
            entities_.begin(),
            entities_.end(),
            [](const Renderer::Entity& entity) { return entity.z > kDespawnZ; }),
        entities_.end()
    );
}

void Game::updateCollisions() {
    for(auto entity = entities_.begin(); entity != entities_.end();) {
        const float entityX = laneToX(entity->lane);
        const bool touchesPlayer = std::abs(entity->z - kPlayerZ) <= kCollisionZRange &&
                                   std::abs(entityX - playerX_) <= kCollisionXRange;

        if(!touchesPlayer) { ++entity; continue; }

        switch(entity->type) {
            case Renderer::EntityType::Obstacle:
                health_ -= 1;
                speed_ = std::max(kMinSpeed, speed_ - 2.5f);
                if(health_ <= 0) { gameOver_ = true; }
                break;
            case Renderer::EntityType::SlowCollectible:
                purpleStreak_ += 1;
                speed_ = std::max(kMinSpeed, speed_ - 1.8f);
                if(purpleStreak_ >= 3) { gameOver_ = true; }
                break;
            case Renderer::EntityType::ScoreCollectible:
                score_ += 100;
                health_ = std::min(kMaxHealth, health_ + 1);
                purpleStreak_ = std::max(0, purpleStreak_ - 1);
                speed_ = std::min(kMaxSpeed, speed_ + 1.8f);
                break;
        }

        entity = entities_.erase(entity);
        updateWindowTitle();
    }
}

void Game::spawnEntity() {
    std::uniform_int_distribution<int> laneDistribution(0, kLaneCount - 1);
    std::uniform_real_distribution<float> typeDistribution(0.0f, 1.0f);

    const float typeRoll = typeDistribution(randomEngine_);
    Renderer::EntityType type = Renderer::EntityType::Obstacle;
    if(typeRoll > 0.78f) { type = Renderer::EntityType::ScoreCollectible; }
    else if(typeRoll > 0.56f) { type = Renderer::EntityType::SlowCollectible; }

    Renderer::Entity entity;
    entity.type = type;
    entity.lane = laneDistribution(randomEngine_);
    entity.z = kSpawnZ;
    entity.spawnTime = gameTime_;
    entity.textureIndex = (type == Renderer::EntityType::ScoreCollectible)
        ? std::uniform_int_distribution<int>(0, 2)(randomEngine_) : 0;
    entities_.push_back(entity);
}

void Game::resetGame() {
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
    gameTime_ = 0.0f;
    accumulator_ = 0.0f;
    entities_.clear();
    updateWindowTitle();
}

void Game::updateWindowTitle() {
    std::ostringstream title;
    title << kWindowTitle;

    if(gameOver_) { title << " | Game Over - Press R | Score " << score_; }
    else { title << " | Score " << score_ << " | Health " << health_ << " | Speed " << std::fixed
                 << std::setprecision(1) << speed_ << " | Purple " << purpleStreak_ << "/3"; }

    renderer_.setWindowTitle(title.str());
}

float Game::laneToX(int lane) const {
    const int clampedLane = std::clamp(lane, 0, kLaneCount - 1);
    return kLanePositions[static_cast<std::size_t>(clampedLane)];
}
