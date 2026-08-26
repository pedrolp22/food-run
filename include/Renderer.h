#ifndef SUBWAY_RENDERER_H
#define SUBWAY_RENDERER_H

#include "GameConfig.h"

#include <SDL2/SDL.h>

#include <array>
#include <string>
#include <vector>

class Renderer {
public:
    enum class EntityType {
        Obstacle,
        SlowCollectible,
        ScoreCollectible
    };

    struct Entity {
        EntityType type;
        int lane;
        float z;
        float spawnTime = 0.0f;
        int textureIndex = 0;
    };

    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    int initSDL();
    void resizeViewport();
    void setWindowTitle(const std::string& title) const;
    void render(float playerX, float roadScroll, float gameTime, const std::vector<Entity>& entities);

private:
    struct Color { float r; float g; float b; };

    struct Transform {
        float x; float y; float z;
        float scaleX; float scaleY; float scaleZ;
    };

    struct SpriteTexture {
        unsigned int id = 0;
        int width = 0;
        int height = 0;
    };

    bool setOpenGLAttributes() const;
    bool createWindow();
    bool createOpenGLContext();
    bool initializeGlad() const;
    bool initializeGraphics();
    bool createShaderProgram();
    bool createSpriteShaderProgram();
    bool createCubeMesh();
    bool createQuadMesh();
    bool loadCollectibleTextures();

    void drawWorld(float playerX, float roadScroll, float gameTime, const std::vector<Entity>& entities);
    void drawCube(const std::array<float, 16>& viewProj, const Transform& transform, const Color& color) const;
    void drawSprite(const std::array<float, 16>& viewProj, unsigned int texture, const Transform& transform,
                    const float rippleColor[3], bool hasGlowBorder, float spawnTime, float gameTime, float texAspect) const;
    void shutdownGraphics();
    void shutdown();

    unsigned int loadTexture(const char* path, int& outWidth, int& outHeight);
    unsigned int getCollectibleTexture(const Entity& entity, float& outAspect) const;

    SDL_Window* window_;
    SDL_GLContext glContext_;
    bool sdlInitialized_;

    unsigned int cubeVao_;
    unsigned int cubeVbo_;
    unsigned int shaderProgram_;
    int mvpUniform_;
    int colorUniform_;

    unsigned int spriteShaderProgram_;
    int spriteMvpUniform_;
    int spriteTextureUniform_;
    int spriteTimeUniform_;
    int spriteSpawnTimeUniform_;
    int spriteRippleColorUniform_;
    int spriteGlowBorderUniform_;

    unsigned int quadVao_;
    unsigned int quadVbo_;

    SpriteTexture waterTexture_;
    SpriteTexture saladTexture_;
    SpriteTexture salmonTexture_;
    SpriteTexture sodaTexture_;

    int drawableWidth_;
    int drawableHeight_;
};


#endif //SUBWAY_RENDERER_H
