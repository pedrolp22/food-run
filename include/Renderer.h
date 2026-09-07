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

    struct CollectEffect {
        EntityType type;
        int lane;
        float z;
        float time = 0.0f;
    };

    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    int initSDL();
    void resizeViewport();
    void setWindowTitle(const std::string& title) const;
    void render(float playerX, float roadScroll, float gameTime, const std::vector<Entity>& entities,
                const std::vector<CollectEffect>& collectEffects);

private:
    enum class Face { FrontBack, Side, Top };

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
    bool createSkyShaderProgram();
    bool createSunShaderProgram();
    bool createCubeShaderProgram();
    bool createCubeMesh();
    bool createQuadMesh();
    bool createRoadShaderProgram();
    bool loadCollectibleTextures();

    void drawWorld(float playerX, float roadScroll, float gameTime, const std::vector<Entity>& entities,
                   const std::vector<CollectEffect>& collectEffects);
    void drawCube(const std::array<float, 16>& viewProj, const Transform& transform, const Color& color) const;
    void drawTexturedQuad(const std::array<float, 16>& viewProj, unsigned int texture,
                          const Transform& transform, int face, float texScaleU, float texScaleV,
                          float lightFactor) const;
    void drawColoredQuad(const std::array<float, 16>& viewProj, const Color& color,
                         const Transform& transform, int face, float lightFactor, float alpha = 1.0f) const;
    void drawSprite(const std::array<float, 16>& viewProj, unsigned int texture, const Transform& transform,
                    const float rippleColor[3], float spawnTime, float gameTime, float texAspect) const;
    void drawShadow(const std::array<float, 16>& viewProj, float centerX, float centerZ,
                    float width, float depth, float y, float alpha) const;
    void drawSunShadow(const std::array<float, 16>& viewProj, float centerX, float centerZ,
                       float height, float width, float depth, float y, float alpha) const;
    void drawSun(const std::array<float, 16>& viewProj) const;
    void drawCollectEffect(const std::array<float, 16>& viewProj, const CollectEffect& effect,
                           float gameTime) const;
    float computeMipLevel(float centerX, float centerY, float centerZ,
                          float worldU, float worldV, float texelU, float texelV) const;
    const SpriteTexture* findSpriteTexture(unsigned int id) const;
    void shutdownGraphics();
    void shutdown();

    unsigned int loadTexture(const char* path, int& outWidth, int& outHeight, bool repeat);
    unsigned int getCollectibleTexture(const Entity& entity, float& outAspect) const;

    SDL_Window* window_;
    SDL_GLContext glContext_;
    bool sdlInitialized_;

    unsigned int cubeVao_;
    unsigned int cubeVbo_;
    unsigned int shaderProgram_;
    int mvpUniform_;
    int colorUniform_;
    int alphaUniform_;

    unsigned int cubeShaderProgram_;
    int cubeMvpUniform_;
    int cubeColorUniform_;
    int cubeSunDirUniform_;
    int cubeAmbientUniform_;

    unsigned int spriteShaderProgram_;
    int spriteMvpUniform_;
    int spriteTextureUniform_;
    int spriteTimeUniform_;
    int spriteSpawnTimeUniform_;
    int spriteRippleColorUniform_;
    int spriteAspectUniform_;

    unsigned int skyShaderProgram_;
    int skyMvpUniform_;
    int skyTopColorUniform_;
    int skyBottomColorUniform_;

    unsigned int sunShaderProgram_;
    int sunMvpUniform_;
    int sunColorUniform_;
    int sunCoreUniform_;
    int sunHazeUniform_;

    unsigned int roadShaderProgram_;
    int roadMvpUniform_;
    int roadTextureUniform_;
    int roadTexScaleUniform_;
    int roadTexOffsetUniform_;
    int roadLodUniform_;
    int roadLightUniform_;

    unsigned int quadVao_;
    unsigned int quadVbo_;

    SpriteTexture roadTexture_;
    SpriteTexture skyscraperTexture_;
    SpriteTexture wallTexture_;
    SpriteTexture waterTexture_;
    SpriteTexture saladTexture_;
    SpriteTexture salmonTexture_;
    SpriteTexture sodaTexture_;
    SpriteTexture burgerTexture_;
    SpriteTexture friesTexture_;

    int drawableWidth_;
    int drawableHeight_;
};


#endif //SUBWAY_RENDERER_H
