#ifndef SUBWAY_GLTFMODEL_H
#define SUBWAY_GLTFMODEL_H

#include <array>
#include <string>
#include <vector>

class GLTFModel {
public:
    struct Material {
        std::array<float, 4> baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
        unsigned int textureId = 0;
        bool hasTexture = false;
    };

    struct Primitive {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        int indexCount = 0;
        Material material{};
    };

    GLTFModel() = default;
    ~GLTFModel();
    GLTFModel(const GLTFModel&) = delete;
    GLTFModel& operator=(const GLTFModel&) = delete;

    bool load(const std::string& gltfPath, float targetHeight);
    void release();

    const std::vector<Primitive>& primitives() const { return primitives_; }

private:
    std::vector<Primitive> primitives_;
};

#endif //SUBWAY_GLTFMODEL_H