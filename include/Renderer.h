//
// Created by pedroleonardo on 19/08/2026.
//

#ifndef SUBWAY_RENDERER_H
#define SUBWAY_RENDERER_H

#include "GameConfig.h"

#include <SDL2/SDL.h>

#include <string>
#include <vector>

class Renderer {
public:
    enum class EntityType {
        Obstacle,
        SlowCollectible,
        ScoreCollectible
    };

    struct Entity { EntityType type; int lane; float z; };

    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    int initSDL();
    void resizeViewport();
    void setWindowTitle(const std::string& title) const;
    void render(float playerX, float roadScroll, const std::vector<Entity>& entities);

private:
    struct Color { float r; float g; float b; };

    struct Transform {
        float x; float y; float z;
        float scaleX; float scaleY; float scaleZ;
    };

    bool setOpenGLAttributes() const;
    bool createWindow();
    bool createOpenGLContext();
    bool initializeGlad() const;
    bool initializeGraphics();
    bool createShaderProgram();
    bool createCubeMesh();

    void drawWorld(float playerX, float roadScroll, const std::vector<Entity>& entities) const;
    void drawCube(const Transform& transform, const Color& color) const;
    void shutdownGraphics();
    void shutdown();

    SDL_Window* window_;
    SDL_GLContext glContext_;
    bool sdlInitialized_;

    unsigned int cubeVao_;
    unsigned int cubeVbo_;
    unsigned int shaderProgram_;
    int mvpUniform_;
    int colorUniform_;
    int drawableWidth_;
    int drawableHeight_;
};


#endif //SUBWAY_RENDERER_H
