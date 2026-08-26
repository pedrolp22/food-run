//
// Created by pedroleonardo on 19/08/2026.
//

#ifndef SUBWAY_GAME_H
#define SUBWAY_GAME_H

#include "GameConfig.h"
#include "Renderer.h"

#include <random>
#include <vector>

class Game {
public:
    Game();

    int run();

private:
    void handleEvents();
    void update(float deltaTime);
    void updatePlayer(float deltaTime);
    void updateWorld(float deltaTime);
    void updateEntities(float deltaTime);
    void updateCollisions();
    void spawnEntity();
    void resetGame();
    void updateWindowTitle();

    float laneToX(int lane) const;

    Renderer renderer_;
    bool running_;
    bool gameOver_;

    int targetLane_;
    float playerX_;
    float speed_;
    float spawnTimer_;
    float roadScroll_;
    float titleUpdateTimer_;
    float scoreAccumulator_;
    int score_;
    int health_;
    int purpleStreak_;

    std::vector<Renderer::Entity> entities_;
    std::mt19937 randomEngine_;
    float gameTime_;
    float accumulator_;
};


#endif //SUBWAY_GAME_H
