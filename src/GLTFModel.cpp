#include "GLTFModel.h"

#include <glad/glad.h>
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

struct JsonValue {
    enum class Type { Null, Bool, String, Number, Array, Object } type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<std::string> keys;
    std::vector<JsonValue> items;

    bool isNull() const { return type == Type::Null; }
    int size() const { return static_cast<int>(items.size()); }
    const JsonValue* at(int index) const { return (index >= 0 && index < size()) ? &items[index] : nullptr; }
    const JsonValue* obj(const std::string& key) const {
        if (type != Type::Object) { return nullptr; }
        for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
            if (keys[static_cast<std::size_t>(i)] == key) { return &items[static_cast<std::size_t>(i)]; }
        }
        return nullptr;
    }
    int asInt(int fallback) const { return type == Type::Number ? static_cast<int>(number) : fallback; }
    float asFloat(float fallback) const { return type == Type::Number ? static_cast<float>(number) : fallback; }
};

class JsonParser {
public:
    explicit JsonParser(const std::string& input) : input_(input) {}

    bool parse(JsonValue& out) {
        skipWhitespace();
        if (!parseValue(out)) { return false; }
        skipWhitespace();
        return pos_ == input_.size();
    }

private:
    const std::string& input_;
    std::size_t pos_ = 0;

    char peek() const { return pos_ < input_.size() ? input_[pos_] : '\0'; }
    char next() { return pos_ < input_.size() ? input_[pos_++] : '\0'; }
    void skipWhitespace() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) { ++pos_; }
    }

    bool parseValue(JsonValue& out) {
        skipWhitespace();
        const char c = peek();
        if (c == '{') { return parseObject(out); }
        if (c == '[') { return parseArray(out); }
        if (c == '"') {
            out.type = JsonValue::Type::Null;
            if (!parseString(out.string)) { return false; }
            out.type = JsonValue::Type::String;
            return true;
        }
        if (c == 't' || c == 'f') {
            out.type = JsonValue::Type::Bool;
            std::string literal;
            while (pos_ < input_.size() && std::isalpha(static_cast<unsigned char>(input_[pos_]))) {
                literal += next();
            }
            if (literal == "true") { out.boolean = true; return true; }
            if (literal == "false") { out.boolean = false; return true; }
            return false;
        }
        if (c == 'n') {
            std::string literal;
            while (pos_ < input_.size() && std::isalpha(static_cast<unsigned char>(input_[pos_]))) {
                literal += next();
            }
            if (literal == "null") { out.type = JsonValue::Type::Null; return true; }
            return false;
        }
        return parseNumber(out);
    }

    bool parseObject(JsonValue& out) {
        out.type = JsonValue::Type::Object;
        next();
        skipWhitespace();
        if (peek() == '}') { next(); return true; }
        for (;;) {
            skipWhitespace();
            if (peek() != '"') { return false; }
            std::string key;
            if (!parseString(key)) { return false; }
            skipWhitespace();
            if (next() != ':') { return false; }
            JsonValue value;
            if (!parseValue(value)) { return false; }
            out.keys.push_back(key);
            out.items.push_back(value);
            skipWhitespace();
            const char c = next();
            if (c == ',') { continue; }
            if (c == '}') { return true; }
            return false;
        }
    }

    bool parseArray(JsonValue& out) {
        out.type = JsonValue::Type::Array;
        next();
        skipWhitespace();
        if (peek() == ']') { next(); return true; }
        for (;;) {
            JsonValue value;
            if (!parseValue(value)) { return false; }
            out.items.push_back(value);
            skipWhitespace();
            const char c = next();
            if (c == ',') { continue; }
            if (c == ']') { return true; }
            return false;
        }
    }

    bool parseString(std::string& out) {
        if (next() != '"') { return false; }
        out.clear();
        for (;;) {
            const char c = next();
            if (c == '\0') { return false; }
            if (c == '"') { return true; }
            if (c == '\\') {
                const char e = next();
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        unsigned int code = 0;
                        for (int i = 0; i < 4; ++i) {
                            const char h = next();
                            code <<= 4;
                            if (h >= '0' && h <= '9') { code += static_cast<unsigned int>(h - '0'); }
                            else if (h >= 'a' && h <= 'f') { code += static_cast<unsigned int>(h - 'a' + 10); }
                            else if (h >= 'A' && h <= 'F') { code += static_cast<unsigned int>(h - 'A' + 10); }
                            else { return false; }
                        }
                        if (code < 0x80) { out += static_cast<char>(code); }
                        else if (code < 0x800) {
                            out += static_cast<char>(0xC0 | (code >> 6));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (code >> 12));
                            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: return false;
                }
                continue;
            }
            out += c;
        }
    }

    bool parseNumber(JsonValue& out) {
        std::string token;
        const std::size_t start = pos_;
        while (pos_ < input_.size()) {
            const char c = input_[pos_];
            if (c == ',' || c == ']' || c == '}' || std::isspace(static_cast<unsigned char>(c))) { break; }
            token += next();
        }
        if (token.empty() || pos_ == start) { return false; }
        out.number = std::strtod(token.c_str(), nullptr);
        out.type = JsonValue::Type::Number;
        return true;
    }
};

constexpr int kCompUbyte = 5121;
constexpr int kCompUshort = 5123;
constexpr int kCompUint = 5125;
constexpr int kCompFloat = 5126;

struct Vec3 { float x; float y; float z; };

struct Mat4 { std::array<float, 16> v{}; };

Mat4 identity4() {
    Mat4 m;
    m.v[0] = 1.0f; m.v[5] = 1.0f; m.v[10] = 1.0f; m.v[15] = 1.0f;
    return m;
}

Mat4 multiply4(const Mat4& left, const Mat4& right) {
    Mat4 result;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float value = 0.0f;
            for (int index = 0; index < 4; ++index) {
                value += left.v[static_cast<std::size_t>(index * 4 + row)] *
                         right.v[static_cast<std::size_t>(column * 4 + index)];
            }
            result.v[static_cast<std::size_t>(column * 4 + row)] = value;
        }
    }
    return result;
}

Vec3 transformPoint(const Mat4& m, const Vec3& p) {
    return {
        m.v[0] * p.x + m.v[4] * p.y + m.v[8] * p.z + m.v[12],
        m.v[1] * p.x + m.v[5] * p.y + m.v[9] * p.z + m.v[13],
        m.v[2] * p.x + m.v[6] * p.y + m.v[10] * p.z + m.v[14]
    };
}

Vec3 transformNormal(const Mat4& m, const Vec3& n) {
    const float x = m.v[0] * n.x + m.v[4] * n.y + m.v[8] * n.z;
    const float y = m.v[1] * n.x + m.v[5] * n.y + m.v[9] * n.z;
    const float z = m.v[2] * n.x + m.v[6] * n.y + m.v[10] * n.z;
    const float len = std::sqrt(x * x + y * y + z * z);
    if (len <= 0.0001f) { return {0.0f, 1.0f, 0.0f}; }
    return {x / len, y / len, z / len};
}

std::string parentDirectory(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) { return "."; }
    return path.substr(0, slash);
}

bool readFileToString(const std::string& path, std::string& out) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) { return false; }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    out.resize(static_cast<std::size_t>(size));
    const std::size_t read = size > 0 ? std::fread(out.data(), 1, static_cast<std::size_t>(size), file) : 0;
    std::fclose(file);
    return read == static_cast<std::size_t>(size);
}

unsigned int readElementAsUint(const unsigned char* data, int componentType) {
    if (componentType == kCompUbyte) { return static_cast<unsigned int>(*data); }
    if (componentType == kCompUshort) {
        return static_cast<unsigned int>(data[0]) | (static_cast<unsigned int>(data[1]) << 8);
    }
    if (componentType == kCompUint) {
        unsigned int value = 0;
        std::memcpy(&value, data, sizeof(value));
        return value;
    }
    if (componentType == kCompFloat) {
        float value = 0.0f;
        std::memcpy(&value, data, sizeof(value));
        return static_cast<unsigned int>(value);
    }
    return 0;
}

float readElementAsFloat(const unsigned char* data, int componentType) {
    if (componentType == kCompFloat) {
        float value = 0.0f;
        std::memcpy(&value, data, sizeof(value));
        return value;
    }
    return static_cast<float>(readElementAsUint(data, componentType));
}

int componentByteSize(int componentType) {
    return componentType == kCompFloat || componentType == kCompUint ? 4
         : componentType == kCompUshort ? 2 : 1;
}

struct AccessorInfo {
    int componentType = 0;
    int components = 1;
    int count = 0;
    std::size_t byteOffset = 0;
    int byteStride = 0;
    const unsigned char* data = nullptr;
};

bool readAccessor(const JsonValue& gltf, const unsigned char* binary,
                  int accessorIndex, AccessorInfo& out) {
    const JsonValue* accessors = gltf.obj("accessors");
    const JsonValue* bufferViews = gltf.obj("bufferViews");
    if (!accessors || !bufferViews) { return false; }
    const JsonValue* accessor = accessors->at(accessorIndex);
    if (!accessor) { return false; }

    out.componentType = accessor->obj("componentType")->asInt(0);
    out.count = accessor->obj("count")->asInt(0);
    const JsonValue* typeNode = accessor->obj("type");
    if (typeNode && typeNode->type == JsonValue::Type::String) {
        if (typeNode->string == "SCALAR") { out.components = 1; }
        else if (typeNode->string == "VEC2") { out.components = 2; }
        else if (typeNode->string == "VEC3") { out.components = 3; }
        else if (typeNode->string == "VEC4") { out.components = 4; }
    }
    out.byteOffset = accessor->obj("byteOffset") ? accessor->obj("byteOffset")->asInt(0) : 0;

    const JsonValue* view = bufferViews->at(accessor->obj("bufferView")->asInt(0));
    if (!view) { return false; }
    out.byteStride = view->obj("byteStride") ? view->obj("byteStride")->asInt(0) : 0;
    if (out.byteStride == 0) {
        out.byteStride = componentByteSize(out.componentType) * out.components;
    }
    const std::size_t bufferOffset = view->obj("byteOffset") ? view->obj("byteOffset")->asInt(0) : 0;
    out.data = binary + bufferOffset + out.byteOffset;
    return out.componentType != 0;
}

unsigned int loadTextureFromImage(const std::string& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load model texture: " << path << " - " << stbi_failure_reason() << '\n';
        return 0;
    }

    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    return texture;
}

} // namespace

GLTFModel::~GLTFModel() {
    release();
}

void GLTFModel::release() {
    for (Primitive& primitive : primitives_) {
        if (primitive.vbo != 0) { glDeleteBuffers(1, &primitive.vbo); primitive.vbo = 0; }
        if (primitive.ebo != 0) { glDeleteBuffers(1, &primitive.ebo); primitive.ebo = 0; }
        if (primitive.vao != 0) { glDeleteVertexArrays(1, &primitive.vao); primitive.vao = 0; }
        if (primitive.material.textureId != 0) {
            glDeleteTextures(1, &primitive.material.textureId);
            primitive.material.textureId = 0;
        }
        primitive.indexCount = 0;
    }
    primitives_.clear();
}

bool GLTFModel::load(const std::string& gltfPath, float targetHeight) {
    release();

    std::string jsonText;
    if (!readFileToString(gltfPath, jsonText)) {
        std::cerr << "Failed to read GLTF file: " << gltfPath << '\n';
        return false;
    }

    JsonValue root;
    JsonParser parser(jsonText);
    if (!parser.parse(root)) {
        std::cerr << "Failed to parse GLTF JSON: " << gltfPath << '\n';
        return false;
    }

    const std::string baseDir = parentDirectory(gltfPath);
    const JsonValue* buffers = root.obj("buffers");
    const JsonValue* nodes = root.obj("nodes");
    const JsonValue* scenes = root.obj("scenes");
    const JsonValue* meshes = root.obj("meshes");
    const JsonValue* materials = root.obj("materials");
    const JsonValue* images = root.obj("images");
    if (!buffers || !nodes || !scenes || !meshes || buffers->size() < 1) {
        std::cerr << "GLTF file missing required sections: " << gltfPath << '\n';
        return false;
    }

    const JsonValue* bufferNode = buffers->at(0);
    const JsonValue* uriNode = bufferNode->obj("uri");
    std::vector<unsigned char> binary;
    if (uriNode && uriNode->type == JsonValue::Type::String) {
        std::string binaryText;
        if (!readFileToString(baseDir + "/" + uriNode->string, binaryText)) {
            std::cerr << "Failed to read GLTF buffer: " << baseDir << "/" << uriNode->string << '\n';
            return false;
        }
        binary.assign(binaryText.begin(), binaryText.end());
    } else {
        std::cerr << "GLTF buffer must reference an external binary file: " << gltfPath << '\n';
        return false;
    }

    std::vector<std::pair<int, Mat4>> meshNodes;
    {
        std::vector<Mat4> worlds(static_cast<std::size_t>(nodes->size()));

        const auto computeNodeMatrix = [](const JsonValue& node) -> Mat4 {
            const JsonValue* matrixNode = node.obj("matrix");
            if (matrixNode && matrixNode->size() == 16) {
                Mat4 m;
                for (int i = 0; i < 16; ++i) { m.v[static_cast<std::size_t>(i)] = matrixNode->at(i)->asFloat(0.0f); }
                return m;
            }
            const float tx = node.obj("translation") ? node.obj("translation")->at(0)->asFloat(0.0f) : 0.0f;
            const float ty = node.obj("translation") ? node.obj("translation")->at(1)->asFloat(0.0f) : 0.0f;
            const float tz = node.obj("translation") ? node.obj("translation")->at(2)->asFloat(0.0f) : 0.0f;
            const float sx = node.obj("scale") ? node.obj("scale")->at(0)->asFloat(1.0f) : 1.0f;
            const float sy = node.obj("scale") ? node.obj("scale")->at(1)->asFloat(1.0f) : 1.0f;
            const float sz = node.obj("scale") ? node.obj("scale")->at(2)->asFloat(1.0f) : 1.0f;
            Mat4 R = identity4();
            const JsonValue* rotationNode = node.obj("rotation");
            if (rotationNode && rotationNode->size() == 4) {
                const float x = rotationNode->at(0)->asFloat(0.0f);
                const float y = rotationNode->at(1)->asFloat(0.0f);
                const float z = rotationNode->at(2)->asFloat(0.0f);
                const float w = rotationNode->at(3)->asFloat(1.0f);
                R.v[0] = 1.0f - 2.0f * (y * y + z * z);
                R.v[1] = 2.0f * (x * y + z * w);
                R.v[2] = 2.0f * (x * z - y * w);
                R.v[4] = 2.0f * (x * y - z * w);
                R.v[5] = 1.0f - 2.0f * (x * x + z * z);
                R.v[6] = 2.0f * (y * z + x * w);
                R.v[8] = 2.0f * (x * z + y * w);
                R.v[9] = 2.0f * (y * z - x * w);
                R.v[10] = 1.0f - 2.0f * (x * x + y * y);
            }
            Mat4 S = identity4();
            S.v[0] = sx; S.v[5] = sy; S.v[10] = sz;
            Mat4 T = identity4();
            T.v[12] = tx; T.v[13] = ty; T.v[14] = tz;
            return multiply4(multiply4(T, R), S);
        };

        std::vector<int> sceneRoots;
        for (int sceneIndex = 0; sceneIndex < scenes->size(); ++sceneIndex) {
            const JsonValue* sceneNodes = scenes->at(sceneIndex)->obj("nodes");
            if (!sceneNodes) { continue; }
            for (int i = 0; i < sceneNodes->size(); ++i) {
                sceneRoots.push_back(sceneNodes->at(i)->asInt(0));
            }
        }
        if (sceneRoots.empty()) {
            std::cerr << "GLTF scene has no root nodes: " << gltfPath << '\n';
            return false;
        }

        const auto visit = [&](const auto& self, int nodeIndex, const Mat4& parentWorld) -> void {
            const JsonValue* node = nodes->at(nodeIndex);
            if (!node) { return; }
            const Mat4 local = computeNodeMatrix(*node);
            const Mat4 world = multiply4(parentWorld, local);
            worlds[static_cast<std::size_t>(nodeIndex)] = world;
            const JsonValue* children = node->obj("children");
            if (children) {
                for (int i = 0; i < children->size(); ++i) {
                    self(self, children->at(i)->asInt(0), world);
                }
            }
        };
        for (const int root : sceneRoots) {
            visit(visit, root, identity4());
        }

        for (int i = 0; i < nodes->size(); ++i) {
            const JsonValue* node = nodes->at(i);
            const JsonValue* meshNode = node->obj("mesh");
            if (!meshNode) { continue; }
            meshNodes.emplace_back(meshNode->asInt(0), worlds[static_cast<std::size_t>(i)]);
        }
        if (meshNodes.empty()) {
            std::cerr << "GLTF file has no meshes: " << gltfPath << '\n';
            return false;
        }
    }

    struct PrimitiveData {
        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        GLTFModel::Material material;
    };
    std::vector<PrimitiveData> primitives;
    primitives.reserve(static_cast<std::size_t>(meshNodes.size()));

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();

    for (const auto& entry : meshNodes) {
        const int meshIndex = entry.first;
        const Mat4& world = entry.second;
        const JsonValue* mesh = meshes->at(meshIndex);
        if (!mesh) { continue; }
        const JsonValue* primitivesJson = mesh->obj("primitives");
        if (!primitivesJson) { continue; }

        for (int primitiveIndex = 0; primitiveIndex < primitivesJson->size(); ++primitiveIndex) {
            const JsonValue* primitive = primitivesJson->at(primitiveIndex);
            if (!primitive) { continue; }

            const JsonValue* attributes = primitive->obj("attributes");
            if (!attributes) { continue; }
            const JsonValue* positionNode = attributes->obj("POSITION");
            if (!positionNode) { continue; }

            AccessorInfo posInfo;
            AccessorInfo normalInfo;
            AccessorInfo uvInfo;
            if (!readAccessor(root, binary.data(), positionNode->asInt(0), posInfo)) { continue; }
            bool hasNormals = false;
            bool hasUvs = false;
            const JsonValue* normalNode = attributes->obj("NORMAL");
            if (normalNode && readAccessor(root, binary.data(), normalNode->asInt(0), normalInfo)) { hasNormals = true; }
            const JsonValue* uvNode = attributes->obj("TEXCOORD_0");
            if (uvNode && readAccessor(root, binary.data(), uvNode->asInt(0), uvInfo)) { hasUvs = true; }

            AccessorInfo indexInfo;
            const JsonValue* indicesNode = primitive->obj("indices");
            if (!indicesNode || !readAccessor(root, binary.data(), indicesNode->asInt(0), indexInfo)) { continue; }

            PrimitiveData data;
            data.vertices.reserve(static_cast<std::size_t>(posInfo.count) * 8);
            data.indices.reserve(static_cast<std::size_t>(indexInfo.count));

            for (int vertex = 0; vertex < posInfo.count; ++vertex) {
                const int posElem = componentByteSize(posInfo.componentType);
                const unsigned char* pos = posInfo.data + static_cast<std::size_t>(vertex) * posInfo.byteStride;
                const Vec3 p = transformPoint(world, {
                    readElementAsFloat(pos, posInfo.componentType),
                    readElementAsFloat(pos + posElem, posInfo.componentType),
                    readElementAsFloat(pos + posElem * 2, posInfo.componentType)
                });
                Vec3 n = {0.0f, 1.0f, 0.0f};
                if (hasNormals) {
                    const int normElem = componentByteSize(normalInfo.componentType);
                    const unsigned char* norm = normalInfo.data + static_cast<std::size_t>(vertex) * normalInfo.byteStride;
                    n = transformNormal(world, {
                        readElementAsFloat(norm, normalInfo.componentType),
                        readElementAsFloat(norm + normElem, normalInfo.componentType),
                        readElementAsFloat(norm + normElem * 2, normalInfo.componentType)
                    });
                }
                float u = 0.0f;
                float v = 0.0f;
                if (hasUvs) {
                    const unsigned char* uv = uvInfo.data + static_cast<std::size_t>(vertex) * uvInfo.byteStride;
                    u = readElementAsFloat(uv, uvInfo.componentType);
                    v = readElementAsFloat(uv + componentByteSize(uvInfo.componentType), uvInfo.componentType);
                }
                data.vertices.push_back(p.x);
                data.vertices.push_back(p.y);
                data.vertices.push_back(p.z);
                data.vertices.push_back(n.x);
                data.vertices.push_back(n.y);
                data.vertices.push_back(n.z);
                data.vertices.push_back(u);
                data.vertices.push_back(v);

                minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
                minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
                minZ = std::min(minZ, p.z); maxZ = std::max(maxZ, p.z);
            }

            for (int index = 0; index < indexInfo.count; ++index) {
                const unsigned char* idx = indexInfo.data + static_cast<std::size_t>(index) * indexInfo.byteStride;
                data.indices.push_back(readElementAsUint(idx, indexInfo.componentType));
            }

            if (const JsonValue* materialNode = primitive->obj("material")) {
                const JsonValue* material = materials ? materials->at(materialNode->asInt(0)) : nullptr;
                if (material) {
                    const JsonValue* pbr = material->obj("pbrMetallicRoughness");
                    if (pbr) {
                        const JsonValue* colorFactor = pbr->obj("baseColorFactor");
                        if (colorFactor && colorFactor->size() == 4) {
                            data.material.baseColor[0] = colorFactor->at(0)->asFloat(1.0f);
                            data.material.baseColor[1] = colorFactor->at(1)->asFloat(1.0f);
                            data.material.baseColor[2] = colorFactor->at(2)->asFloat(1.0f);
                            data.material.baseColor[3] = colorFactor->at(3)->asFloat(1.0f);
                        }
                        const JsonValue* baseTexture = pbr->obj("baseColorTexture");
                        if (baseTexture) {
                            const int textureIndex = baseTexture->obj("index")->asInt(0);
                            const JsonValue* textures = root.obj("textures");
                            const JsonValue* texture = textures ? textures->at(textureIndex) : nullptr;
                            const JsonValue* source = texture ? texture->obj("source") : nullptr;
                            const int imageIndex = source ? source->asInt(0) : 0;
                            const JsonValue* image = images ? images->at(imageIndex) : nullptr;
                            const JsonValue* imageUri = image ? image->obj("uri") : nullptr;
                            if (imageUri && imageUri->type == JsonValue::Type::String) {
                                const unsigned int textureId = loadTextureFromImage(baseDir + "/" + imageUri->string);
                                if (textureId != 0) {
                                    data.material.textureId = textureId;
                                    data.material.hasTexture = true;
                                }
                            }
                        }
                    }
                }
            }

            primitives.push_back(std::move(data));
        }
    }

    if (primitives.empty()) {
        std::cerr << "GLTF file produced no renderable primitives: " << gltfPath << '\n';
        return false;
    }

    const float height = maxY - minY;
    if (height <= 0.0001f) {
        std::cerr << "GLTF model has no vertical extent: " << gltfPath << '\n';
        return false;
    }
    const float scale = targetHeight / height;
    const float centerX = (minX + maxX) * 0.5f;
    const float centerZ = (minZ + maxZ) * 0.5f;

    for (PrimitiveData& data : primitives) {
        for (std::size_t vertex = 0; vertex < data.vertices.size(); vertex += 8) {
            data.vertices[vertex] = (data.vertices[vertex] - centerX) * scale;
            data.vertices[vertex + 1] = (data.vertices[vertex + 1] - minY) * scale;
            data.vertices[vertex + 2] = (data.vertices[vertex + 2] - centerZ) * scale;
        }

        GLTFModel::Primitive primitive;
        primitive.indexCount = static_cast<int>(data.indices.size());
        primitive.material = data.material;

        glGenVertexArrays(1, &primitive.vao);
        glGenBuffers(1, &primitive.vbo);
        glGenBuffers(1, &primitive.ebo);
        if (primitive.vao == 0 || primitive.vbo == 0 || primitive.ebo == 0) {
            std::cerr << "Failed to create GLTF mesh buffers: " << gltfPath << '\n';
            glDeleteVertexArrays(1, &primitive.vao);
            glDeleteBuffers(1, &primitive.vbo);
            glDeleteBuffers(1, &primitive.ebo);
            if (primitive.material.textureId != 0) { glDeleteTextures(1, &primitive.material.textureId); }
            return false;
        }

        glBindVertexArray(primitive.vao);
        glBindBuffer(GL_ARRAY_BUFFER, primitive.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.vertices.size() * sizeof(float)),
                     data.vertices.data(), GL_STATIC_DRAW);

        constexpr int stride = 8 * static_cast<int>(sizeof(float));
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, primitive.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.indices.size() * sizeof(unsigned int)),
                     data.indices.data(), GL_STATIC_DRAW);
        glBindVertexArray(0);

        primitives_.push_back(primitive);
    }

    return true;
}