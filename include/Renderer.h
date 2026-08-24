//
// Created by pedroleonardo on 19/08/2026.
//

#ifndef SUBWAY_RENDERER_H
#define SUBWAY_RENDERER_H

#include <SDL2/SDL.h>

#include <random>
#include <vector>

class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    int initSDL();
    void run();
    void render();

private:
    enum class EntityType {
        Obstacle,
        SlowCollectible,
        ScoreCollectible
    };

    struct Entity {
        EntityType type;
        int lane;
        float z;
    };

    struct Color {
        float r;
        float g;
        float b;
    };

    struct Transform {
        float x;
        float y;
        float z;
        float scaleX;
        float scaleY;
        float scaleZ;
    };

    bool setOpenGLAttributes() const;
    bool createWindow();
    bool createOpenGLContext();
    bool initializeGlad() const;
    bool initializeGraphics();
    bool createShaderProgram();
    bool createCubeMesh();

    void handleEvents();
    void update(float deltaTime);
    void updatePlayer(float deltaTime);
    void updateWorld(float deltaTime);
    void updateEntities(float deltaTime);
    void updateCollisions();
    void spawnEntity();
    void resetGame();
    void updateWindowTitle();
    void drawWorld() const;
    void drawCube(const Transform& transform, const Color& color) const;
    void shutdownGraphics();
    void shutdown();

    SDL_Window* window_;
    SDL_GLContext glContext_;
    bool sdlInitialized_;
    bool running_;
    bool gameOver_;

    unsigned int cubeVao_;
    unsigned int cubeVbo_;
    unsigned int shaderProgram_;
    int mvpUniform_;
    int colorUniform_;
    int drawableWidth_;
    int drawableHeight_;

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

    std::vector<Entity> entities_;
    std::mt19937 randomEngine_;
};


#endif //SUBWAY_RENDERER_H
