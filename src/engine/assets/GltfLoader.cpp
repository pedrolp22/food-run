#include "engine/assets/GltfLoader.h"
#include "Json.h"
#include "engine/math/Math.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace engine::assets {
using namespace engine::math;
using detail::JsonValue;
namespace {
constexpr int kCompUbyte = 5121;
constexpr int kCompUshort = 5123;
constexpr int kCompUint = 5125;
constexpr int kCompFloat = 5126;

bool readFileToString(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    out = contents.str();
    return !file.bad();
}

int integer(const JsonValue& node, const char* name, int fallback = -1) {
    const auto* value = node.obj(name);
    return value ? value->asInt(fallback) : fallback;
}

float materialFactor(const JsonValue& node, const char* name, float fallback) {
    const auto* value = node.obj(name);
    if (!value) {
        return fallback;
    }
    if (value->type != JsonValue::Type::Number || !std::isfinite(value->number) || value->number < 0 ||
        value->number > 1) {
        throw std::runtime_error(std::string("Invalid glTF material factor: ") + name);
    }
    return static_cast<float>(value->number);
}

std::filesystem::path materialTexturePath(const JsonValue& root, const JsonValue* info, const std::string& baseDir) {
    if (!info) {
        return {};
    }
    if (integer(*info, "texCoord", 0) != 0) {
        throw std::runtime_error("Only glTF material TEXCOORD_0 is supported");
    }
    const auto* textures = root.obj("textures");
    const auto* texture = textures ? textures->at(integer(*info, "index")) : nullptr;
    const auto* images = root.obj("images");
    const auto* image = texture && images ? images->at(integer(*texture, "source")) : nullptr;
    const auto* uri = image ? image->obj("uri") : nullptr;
    if (!uri || uri->type != JsonValue::Type::String || uri->string.empty() || uri->string.starts_with("data:")) {
        throw std::runtime_error("glTF material textures require an external image URI");
    }
    return std::filesystem::path(baseDir) / uri->string;
}

unsigned int readElementAsUint(const unsigned char* data, int componentType) {
    if (componentType == kCompUbyte) {
        return static_cast<unsigned int>(*data);
    }
    if (componentType == kCompUshort) {
        return static_cast<unsigned int>(data[0]) | (static_cast<unsigned int>(data[1]) << 8);
    }
    if (componentType == kCompUint) {
        unsigned int value = 0;
        std::memcpy(&value, data, sizeof(value));
        return value;
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
    return componentType == kCompFloat || componentType == kCompUint ? 4 : componentType == kCompUshort ? 2 : 1;
}

struct AccessorInfo {
    int componentType = 0;
    int components = 1;
    int count = 0;
    int byteStride = 0;
    const unsigned char* data = nullptr;
    bool normalized = false;
};

bool readAccessor(const JsonValue& gltf, std::span<const unsigned char> binary, int accessorIndex, AccessorInfo& out) {
    const auto fail = [] { throw std::runtime_error("Invalid or unsupported glTF accessor"); };
    const auto* accessors = gltf.obj("accessors");
    const auto* views = gltf.obj("bufferViews");
    const auto* accessor = accessors ? accessors->at(accessorIndex) : nullptr;
    if (!accessor || !views || accessor->obj("sparse")) {
        fail();
    }
    out.componentType = integer(*accessor, "componentType");
    out.count = integer(*accessor, "count");
    const auto* normalized = accessor->obj("normalized");
    out.normalized = normalized && normalized->type == JsonValue::Type::Bool && normalized->boolean;
    if (out.count <= 0 || (out.componentType != kCompUbyte && out.componentType != kCompUshort &&
                           out.componentType != kCompUint && out.componentType != kCompFloat)) {
        fail();
    }
    const auto* type = accessor->obj("type");
    if (!type) {
        fail();
    }
    if (type->string == "SCALAR") {
        out.components = 1;
    } else if (type->string == "VEC2") {
        out.components = 2;
    } else if (type->string == "VEC3") {
        out.components = 3;
    } else if (type->string == "VEC4") {
        out.components = 4;
    } else if (type->string == "MAT4") {
        out.components = 16;
    } else {
        fail();
    }
    const auto* view = views->at(integer(*accessor, "bufferView"));
    if (!view || integer(*view, "buffer") != 0) {
        fail();
    }
    const int offset = integer(*accessor, "byteOffset", 0);
    const int start = integer(*view, "byteOffset", 0);
    const int length = integer(*view, "byteLength");
    const int elementSize = componentByteSize(out.componentType) * out.components;
    out.byteStride = integer(*view, "byteStride", elementSize);
    if (offset < 0 || start < 0 || length < 0 || out.byteStride < elementSize) {
        fail();
    }
    const auto byteStart = static_cast<std::size_t>(start);
    const auto byteLength = static_cast<std::size_t>(length);
    if (byteStart > binary.size() || byteLength > binary.size() - byteStart ||
        static_cast<std::size_t>(offset) > byteLength) {
        fail();
    }
    const auto available = byteLength - static_cast<std::size_t>(offset);
    if (available < static_cast<std::size_t>(elementSize) ||
        static_cast<std::size_t>(out.count - 1) > (available - elementSize) / out.byteStride) {
        fail();
    }
    out.data = binary.data() + byteStart + offset;
    return true;
}

Mat4 computeNodeMatrix(const JsonValue& node) {
    for (const auto& [key, size] : {std::pair{"matrix", 16}, {"translation", 3}, {"scale", 3}, {"rotation", 4}}) {
        if (const auto* value = node.obj(key); value && value->size() != size) {
            throw std::runtime_error("Invalid glTF node transform");
        }
    }
    const JsonValue* matrixNode = node.obj("matrix");
    if (matrixNode && matrixNode->size() == 16) {
        Mat4 m;
        for (int i = 0; i < 16; ++i) {
            m.values[static_cast<std::size_t>(i)] = matrixNode->at(i)->asFloat(0.0f);
        }
        return m;
    }
    const float tx = node.obj("translation") ? node.obj("translation")->at(0)->asFloat(0.0f) : 0.0f;
    const float ty = node.obj("translation") ? node.obj("translation")->at(1)->asFloat(0.0f) : 0.0f;
    const float tz = node.obj("translation") ? node.obj("translation")->at(2)->asFloat(0.0f) : 0.0f;
    const float sx = node.obj("scale") ? node.obj("scale")->at(0)->asFloat(1.0f) : 1.0f;
    const float sy = node.obj("scale") ? node.obj("scale")->at(1)->asFloat(1.0f) : 1.0f;
    const float sz = node.obj("scale") ? node.obj("scale")->at(2)->asFloat(1.0f) : 1.0f;
    Mat4 R = identity();
    const JsonValue* rotationNode = node.obj("rotation");
    if (rotationNode && rotationNode->size() == 4) {
        const float x = rotationNode->at(0)->asFloat(0.0f);
        const float y = rotationNode->at(1)->asFloat(0.0f);
        const float z = rotationNode->at(2)->asFloat(0.0f);
        const float w = rotationNode->at(3)->asFloat(1.0f);
        R.values[0] = 1.0f - 2.0f * (y * y + z * z);
        R.values[1] = 2.0f * (x * y + z * w);
        R.values[2] = 2.0f * (x * z - y * w);
        R.values[4] = 2.0f * (x * y - z * w);
        R.values[5] = 1.0f - 2.0f * (x * x + z * z);
        R.values[6] = 2.0f * (y * z + x * w);
        R.values[8] = 2.0f * (x * z + y * w);
        R.values[9] = 2.0f * (y * z - x * w);
        R.values[10] = 1.0f - 2.0f * (x * x + y * y);
    }
    Mat4 S = identity();
    S.values[0] = sx;
    S.values[5] = sy;
    S.values[10] = sz;
    Mat4 T = identity();
    T.values[12] = tx;
    T.values[13] = ty;
    T.values[14] = tz;
    return multiply(multiply(T, R), S);
}

struct MeshNode {
    int mesh, node, skin;
    Mat4 world;
};
std::vector<MeshNode> collectMeshNodes(const JsonValue& root, ModelData& data) {
    const auto* nodes = root.obj("nodes");
    const auto* scenes = root.obj("scenes");
    std::vector<MeshNode> meshNodes;
    const bool rigged = root.obj("skins") && root.obj("skins")->size() > 0;
    if (rigged)
        data.nodes.resize(nodes->size());
    {
        std::vector<Mat4> worlds(static_cast<std::size_t>(nodes->size()));

        std::vector<int> sceneRoots;
        const auto* scene = scenes->at(integer(root, "scene", 0));
        const auto* sceneNodes = scene ? scene->obj("nodes") : nullptr;
        if (sceneNodes) {
            for (int i = 0; i < sceneNodes->size(); ++i) {
                sceneRoots.push_back(sceneNodes->at(i)->asInt(-1));
            }
        }
        if (sceneRoots.empty()) {

            throw std::runtime_error("glTF scene contains no mesh nodes");
        }

        std::vector<int> visited(static_cast<std::size_t>(nodes->size()), 0);
        const auto visit = [&](const auto& self, int nodeIndex, const Mat4& parentWorld, int parent,
                               int depth) -> void {
            if (depth > 256) {
                throw std::runtime_error("glTF node hierarchy is too deep");
            }
            const JsonValue* node = nodes->at(nodeIndex);
            if (!node || visited[static_cast<std::size_t>(nodeIndex)] != 0) {
                throw std::runtime_error("Invalid, cyclic or multiply-parented glTF node hierarchy");
            }
            visited[static_cast<std::size_t>(nodeIndex)] = 1;
            const Mat4 local = computeNodeMatrix(*node);
            const Mat4 world = multiply(parentWorld, local);
            worlds[static_cast<std::size_t>(nodeIndex)] = world;
            if (rigged) {
                const auto* name = node->obj("name");
                data.nodes[nodeIndex] = {name ? name->string : "", parent, local, world};
                data.nodeOrder.push_back(nodeIndex);
            }
            const JsonValue* children = node->obj("children");
            if (children) {
                for (int i = 0; i < children->size(); ++i) {
                    self(self, children->at(i)->asInt(-1), world, nodeIndex, depth + 1);
                }
            }
        };
        for (const int root : sceneRoots) {
            visit(visit, root, identity(), -1, 0);
        }

        for (int i = 0; i < nodes->size(); ++i) {
            const JsonValue* node = nodes->at(i);
            const JsonValue* meshNode = node->obj("mesh");
            if (!meshNode || !visited[static_cast<std::size_t>(i)]) {
                continue;
            }
            meshNodes.push_back({meshNode->asInt(-1), i, integer(*node, "skin"), worlds[static_cast<std::size_t>(i)]});
        }
        if (meshNodes.empty()) {

            throw std::runtime_error("glTF scene contains no mesh nodes");
        }
    }

    return meshNodes;
}

} // namespace

namespace {
ModelData loadModel(const std::filesystem::path& path, float targetHeight) {
    if (!(targetHeight > 0) || !std::isfinite(targetHeight)) {
        throw std::invalid_argument("Invalid model height");
    }
    const std::string gltfPath = path.string();

    std::string jsonText;
    if (!readFileToString(gltfPath, jsonText)) {

        throw std::runtime_error("Could not import glTF: " + gltfPath);
    }

    const JsonValue root = detail::parseJson(jsonText);

    const std::string baseDir = path.has_parent_path() ? path.parent_path().string() : ".";
    const JsonValue* buffers = root.obj("buffers");
    const JsonValue* nodes = root.obj("nodes");
    const JsonValue* scenes = root.obj("scenes");
    const JsonValue* meshes = root.obj("meshes");
    const JsonValue* materials = root.obj("materials");
    if (!buffers || !nodes || !scenes || !meshes || buffers->size() != 1) {

        throw std::runtime_error("Could not import glTF: " + gltfPath);
    }

    const JsonValue* bufferNode = buffers->at(0);
    const JsonValue* uriNode = bufferNode->obj("uri");
    std::vector<unsigned char> binary;
    if (uriNode && uriNode->type == JsonValue::Type::String) {
        std::string binaryText;
        if (!readFileToString(baseDir + "/" + uriNode->string, binaryText)) {
            throw std::runtime_error("Cannot open glTF buffer: " +
                                     (std::filesystem::path(baseDir) / uriNode->string).string());
        }
        binary.assign(binaryText.begin(), binaryText.end());
        const int length = integer(*bufferNode, "byteLength");
        if (length < 1 || binary.size() < static_cast<std::size_t>(length)) {
            throw std::runtime_error("Missing or truncated glTF buffer data");
        }
        binary.resize(length);
    } else {

        throw std::runtime_error("Could not import glTF: " + gltfPath);
    }

    ModelData result;
    const auto meshNodes = collectMeshNodes(root, result);
    if (const auto* skins = root.obj("skins")) {
        for (int i = 0; i < skins->size(); ++i) {
            const auto* skin = skins->at(i);
            const auto* joints = skin->obj("joints");
            if (!joints || joints->size() < 1 || joints->size() > static_cast<int>(kMaxSkinJoints)) {
                throw std::runtime_error("Invalid glTF skin joint count (maximum " + std::to_string(kMaxSkinJoints) + ")");
            }
            SkinData imported;
            for (int j = 0; j < joints->size(); ++j) {
                const int node = joints->at(j)->asInt(-1);
                if (node < 0 || node >= static_cast<int>(result.nodes.size()) ||
                    std::find(result.nodeOrder.begin(), result.nodeOrder.end(), node) == result.nodeOrder.end() ||
                    std::find(imported.joints.begin(), imported.joints.end(), node) != imported.joints.end()) {
                    throw std::runtime_error("Invalid glTF skin joint reference");
                }
                imported.joints.push_back(node);
            }
            imported.inverseBindMatrices.resize(imported.joints.size(), identity());
            if (const auto* bind = skin->obj("inverseBindMatrices")) {
                AccessorInfo info;
                readAccessor(root, binary, bind->asInt(-1), info);
                if (info.componentType != kCompFloat || info.components != 16 || info.count < joints->size()) {
                    throw std::runtime_error("Invalid glTF inverse bind matrices");
                }
                for (int j = 0; j < joints->size(); ++j) {
                    std::memcpy(imported.inverseBindMatrices[j].values.data(), info.data + j * info.byteStride,
                                sizeof(Mat4));
                    for (float value : imported.inverseBindMatrices[j].values) {
                        if (!std::isfinite(value))
                            throw std::runtime_error("Non-finite glTF inverse bind matrix");
                    }
                }
            }
            result.skins.push_back(std::move(imported));
        }
    }

    std::vector<PrimitiveData> primitives;
    primitives.reserve(static_cast<std::size_t>(meshNodes.size()));

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();

    for (const auto& entry : meshNodes) {
        const int meshIndex = entry.mesh;
        if (entry.skin >= static_cast<int>(result.skins.size()))
            throw std::runtime_error("Invalid glTF mesh skin");
        // glTF skinned positions live in bind space; the mesh node's transform is ignored for skinning.
        const Mat4 world = entry.skin >= 0 ? identity() : entry.world;
        const JsonValue* mesh = meshes->at(meshIndex);
        if (!mesh) {
            throw std::runtime_error("Invalid glTF mesh reference");
        }
        const JsonValue* primitivesJson = mesh->obj("primitives");
        if (!primitivesJson) {
            throw std::runtime_error("Missing glTF primitives");
        }

        for (int primitiveIndex = 0; primitiveIndex < primitivesJson->size(); ++primitiveIndex) {
            const JsonValue* primitive = primitivesJson->at(primitiveIndex);
            if (!primitive) {
                continue;
            }

            if (integer(*primitive, "mode", 4) != 4) {
                throw std::runtime_error("Only glTF triangle meshes are supported");
            }
            const JsonValue* attributes = primitive->obj("attributes");
            if (!attributes) {
                throw std::runtime_error("Missing glTF attributes");
            }
            const JsonValue* positionNode = attributes->obj("POSITION");
            if (!positionNode) {
                throw std::runtime_error("Missing glTF positions");
            }

            AccessorInfo posInfo;
            AccessorInfo normalInfo;
            AccessorInfo uvInfo;
            readAccessor(root, binary, positionNode->asInt(-1), posInfo);
            bool hasNormals = false;
            bool hasUvs = false;
            const JsonValue* normalNode = attributes->obj("NORMAL");
            if (normalNode && readAccessor(root, binary, normalNode->asInt(-1), normalInfo)) {
                hasNormals = true;
            }
            const JsonValue* uvNode = attributes->obj("TEXCOORD_0");
            if (uvNode && readAccessor(root, binary, uvNode->asInt(-1), uvInfo)) {
                hasUvs = true;
            }

            AccessorInfo indexInfo;
            const JsonValue* indicesNode = primitive->obj("indices");
            if (!indicesNode) {
                throw std::runtime_error("Non-indexed glTF meshes are not supported");
            }
            readAccessor(root, binary, indicesNode->asInt(-1), indexInfo);

            if (posInfo.componentType != kCompFloat || posInfo.components != 3 ||
                (hasNormals && (normalInfo.componentType != kCompFloat || normalInfo.components != 3 ||
                                normalInfo.count != posInfo.count)) ||
                (hasUvs &&
                 (uvInfo.componentType != kCompFloat || uvInfo.components != 2 || uvInfo.count != posInfo.count)) ||
                indexInfo.componentType == kCompFloat || indexInfo.components != 1 || indexInfo.count % 3 != 0) {
                throw std::runtime_error("Unsupported glTF vertex or index layout");
            }
            PrimitiveData data;
            data.node = result.nodes.empty() ? -1 : entry.node;
            data.skin = entry.skin;
            if (entry.skin >= 0) {
                AccessorInfo jointInfo, weightInfo;
                readAccessor(root, binary, integer(*attributes, "JOINTS_0"), jointInfo);
                readAccessor(root, binary, integer(*attributes, "WEIGHTS_0"), weightInfo);
                if (jointInfo.components != 4 || weightInfo.components != 4 || jointInfo.count != posInfo.count ||
                    weightInfo.count != posInfo.count || jointInfo.normalized ||
                    (jointInfo.componentType != kCompUbyte && jointInfo.componentType != kCompUshort) ||
                    (weightInfo.componentType == kCompFloat && weightInfo.normalized) ||
                    (weightInfo.componentType != kCompFloat &&
                     !((weightInfo.componentType == kCompUbyte || weightInfo.componentType == kCompUshort) &&
                       weightInfo.normalized)) ||
                    attributes->obj("JOINTS_1") || attributes->obj("WEIGHTS_1")) {
                    throw std::runtime_error("Unsupported glTF skin vertex layout");
                }
                data.influences.resize(posInfo.count);
                for (int v = 0; v < posInfo.count; ++v) {
                    auto& influence = data.influences[v];
                    float sum = 0;
                    for (int j = 0; j < 4; ++j) {
                        influence.joints[j] = readElementAsUint(jointInfo.data + v * jointInfo.byteStride +
                                                                    j * componentByteSize(jointInfo.componentType),
                                                                jointInfo.componentType);
                        float weight = readElementAsFloat(weightInfo.data + v * weightInfo.byteStride +
                                                              j * componentByteSize(weightInfo.componentType),
                                                          weightInfo.componentType);
                        if (weightInfo.normalized)
                            weight /= weightInfo.componentType == kCompUbyte ? 255.f : 65535.f;
                        if (influence.joints[j] >= result.skins[entry.skin].joints.size() || !std::isfinite(weight) ||
                            weight < 0) {
                            throw std::runtime_error("Invalid glTF joint index or weight");
                        }
                        influence.weights[j] = weight;
                        sum += weight;
                    }
                    if (!std::isfinite(sum) || sum <= 0)
                        throw std::runtime_error("Unweighted glTF skin vertex");
                    for (auto& weight : influence.weights)
                        weight /= sum;
                }
            }
            data.vertices.reserve(static_cast<std::size_t>(posInfo.count) * kModelVertexStride);
            data.indices.reserve(static_cast<std::size_t>(indexInfo.count));

            for (int vertex = 0; vertex < posInfo.count; ++vertex) {
                const int posElem = componentByteSize(posInfo.componentType);
                const unsigned char* pos = posInfo.data + static_cast<std::size_t>(vertex) * posInfo.byteStride;
                const Vec3 p = transformPoint(world, {readElementAsFloat(pos, posInfo.componentType),
                                                      readElementAsFloat(pos + posElem, posInfo.componentType),
                                                      readElementAsFloat(pos + posElem * 2, posInfo.componentType)});
                Vec3 n = {0.0f, 1.0f, 0.0f};
                if (hasNormals) {
                    const int normElem = componentByteSize(normalInfo.componentType);
                    const unsigned char* norm =
                        normalInfo.data + static_cast<std::size_t>(vertex) * normalInfo.byteStride;
                    n = transformNormal(world, {readElementAsFloat(norm, normalInfo.componentType),
                                                readElementAsFloat(norm + normElem, normalInfo.componentType),
                                                readElementAsFloat(norm + normElem * 2, normalInfo.componentType)});
                }
                float u = 0.0f;
                float v = 0.0f;
                if (hasUvs) {
                    const unsigned char* uv = uvInfo.data + static_cast<std::size_t>(vertex) * uvInfo.byteStride;
                    u = readElementAsFloat(uv, uvInfo.componentType);
                    v = readElementAsFloat(uv + componentByteSize(uvInfo.componentType), uvInfo.componentType);
                }
                for (float value : {p.x, p.y, p.z, n.x, n.y, n.z, u, v}) {
                    if (!std::isfinite(value)) {
                        throw std::runtime_error("Non-finite glTF vertex");
                    }
                }
                data.vertices.push_back(p.x);
                data.vertices.push_back(p.y);
                data.vertices.push_back(p.z);
                data.vertices.push_back(n.x);
                data.vertices.push_back(n.y);
                data.vertices.push_back(n.z);
                data.vertices.push_back(u);
                data.vertices.push_back(v);

                Vec3 bound = p;
                if (entry.skin >= 0) {
                    bound = {};
                    const auto& influence = data.influences[vertex];
                    const auto& skin = result.skins[entry.skin];
                    for (int j = 0; j < 4; ++j) {
                        const auto posed =
                            transformPoint(multiply(result.nodes[skin.joints[influence.joints[j]]].worldBind,
                                                    skin.inverseBindMatrices[influence.joints[j]]),
                                           p);
                        bound.x += posed.x * influence.weights[j];
                        bound.y += posed.y * influence.weights[j];
                        bound.z += posed.z * influence.weights[j];
                    }
                }
                minX = std::min(minX, bound.x);
                maxX = std::max(maxX, bound.x);
                minY = std::min(minY, bound.y);
                maxY = std::max(maxY, bound.y);
                minZ = std::min(minZ, bound.z);
                maxZ = std::max(maxZ, bound.z);
            }

            for (int index = 0; index < indexInfo.count; ++index) {
                const unsigned char* idx = indexInfo.data + static_cast<std::size_t>(index) * indexInfo.byteStride;
                const auto indexValue = readElementAsUint(idx, indexInfo.componentType);
                if (indexValue >= static_cast<unsigned int>(posInfo.count)) {
                    throw std::runtime_error("glTF index outside vertex array");
                }
                data.indices.push_back(indexValue);
            }

            if (!hasNormals) {
                std::vector<Vec3> normals(static_cast<std::size_t>(posInfo.count));
                const auto point = [&](unsigned int vertex) {
                    const auto offset = static_cast<std::size_t>(vertex) * kModelVertexStride;
                    return Vec3{data.vertices[offset], data.vertices[offset + 1], data.vertices[offset + 2]};
                };
                for (std::size_t i = 0; i < data.indices.size(); i += 3) {
                    const auto a = data.indices[i], b = data.indices[i + 1], c = data.indices[i + 2];
                    const auto normal = cross(point(b) - point(a), point(c) - point(a));
                    for (auto vertex : {a, b, c}) {
                        normals[vertex].x += normal.x;
                        normals[vertex].y += normal.y;
                        normals[vertex].z += normal.z;
                    }
                }
                for (std::size_t i = 0; i < normals.size(); ++i) {
                    const auto normal = normalize(normals[i]);
                    data.vertices[i * kModelVertexStride + 3] = normal.x;
                    data.vertices[i * kModelVertexStride + 4] = normal.y;
                    data.vertices[i * kModelVertexStride + 5] = normal.z;
                }
            }

            if (const JsonValue* materialNode = primitive->obj("material")) {
                const JsonValue* material = materials ? materials->at(materialNode->asInt(0)) : nullptr;
                if (material) {
                    if (const auto* emission = material->obj("emissiveFactor")) {
                        if (emission->type != JsonValue::Type::Array || emission->size() != 3) {
                            throw std::runtime_error("Invalid glTF emissive factor");
                        }
                        for (int i = 0; i < 3; ++i) {
                            const auto* value = emission->at(i);
                            if (value->type != JsonValue::Type::Number || !std::isfinite(value->number) ||
                                value->number < 0 || value->number > 1) {
                                throw std::runtime_error("Invalid glTF emissive factor");
                            }
                            data.material.emissiveFactor[i] = static_cast<float>(value->number);
                        }
                    }
                    data.material.emissiveTexture =
                        materialTexturePath(root, material->obj("emissiveTexture"), baseDir);
                    if (const auto* extensions = material->obj("extensions")) {
                        if (const auto* emission = extensions->obj("KHR_materials_emissive_strength")) {
                            if (const auto* strength = emission->obj("emissiveStrength")) {
                                if (strength->type != JsonValue::Type::Number || !std::isfinite(strength->number) ||
                                    strength->number < 0 || strength->number > std::numeric_limits<float>::max()) {
                                    throw std::runtime_error("Invalid glTF emissive strength");
                                }
                                data.material.emissiveStrength = static_cast<float>(strength->number);
                            }
                        }
                    }
                    if (const auto* mode = material->obj("alphaMode"); mode && mode->string == "MASK") {
                        data.material.alphaCutoff = materialFactor(*material, "alphaCutoff", .5f);
                    }
                    if (const auto* sided = material->obj("doubleSided")) {
                        data.material.doubleSided = sided->type == JsonValue::Type::Bool && sided->boolean;
                    }
                    const JsonValue* pbr = material->obj("pbrMetallicRoughness");
                    if (pbr) {
                        data.material.roughness = materialFactor(*pbr, "roughnessFactor", 1);
                        data.material.metallic = materialFactor(*pbr, "metallicFactor", 1);
                        const JsonValue* colorFactor = pbr->obj("baseColorFactor");
                        if (colorFactor && colorFactor->size() == 4) {
                            data.material.baseColor[0] = colorFactor->at(0)->asFloat(1.0f);
                            data.material.baseColor[1] = colorFactor->at(1)->asFloat(1.0f);
                            data.material.baseColor[2] = colorFactor->at(2)->asFloat(1.0f);
                            data.material.baseColor[3] = colorFactor->at(3)->asFloat(1.0f);
                        }
                        data.material.baseTexture = materialTexturePath(root, pbr->obj("baseColorTexture"), baseDir);
                        if (const auto* info = pbr->obj("baseColorTexture")) {
                            const auto* textures = root.obj("textures");
                            const auto* texture = textures ? textures->at(integer(*info, "index")) : nullptr;
                            const auto* samplers = root.obj("samplers");
                            const auto* sampler =
                                texture && samplers ? samplers->at(integer(*texture, "sampler")) : nullptr;
                            data.material.baseTextureNearest = sampler && integer(*sampler, "magFilter") == 9728 &&
                                                               integer(*sampler, "minFilter") == 9728;
                            data.material.baseTextureRepeat = !sampler || (integer(*sampler, "wrapS", 10497) == 10497 &&
                                                                           integer(*sampler, "wrapT", 10497) == 10497);
                        }
                        data.material.metallicRoughnessTexture =
                            materialTexturePath(root, pbr->obj("metallicRoughnessTexture"), baseDir);
                    }
                    const auto* normal = material->obj("normalTexture");
                    data.material.normalTexture = materialTexturePath(root, normal, baseDir);
                    if (normal) {
                        const auto* scale = normal->obj("scale");
                        if (scale) {
                            if (scale->type != JsonValue::Type::Number || !std::isfinite(scale->number) ||
                                std::abs(scale->number) > std::numeric_limits<float>::max()) {
                                throw std::runtime_error("Invalid glTF normal scale");
                            }
                            data.material.normalScale = static_cast<float>(scale->number);
                        }
                    }
                    const auto* occlusion = material->obj("occlusionTexture");
                    data.material.occlusionTexture = materialTexturePath(root, occlusion, baseDir);
                    if (occlusion) {
                        data.material.occlusionStrength = materialFactor(*occlusion, "strength", 1);
                    }
                }
            }

            primitives.push_back(std::move(data));
        }
    }

    if (primitives.empty()) {

        throw std::runtime_error("Could not import glTF: " + gltfPath);
    }

    const float height = maxY - minY;
    if (height <= 0.0001f) {

        throw std::runtime_error("Could not import glTF: " + gltfPath);
    }
    const float sizeScale = targetHeight / height;
    const float centerX = (minX + maxX) * 0.5f;
    const float centerZ = (minZ + maxZ) * 0.5f;

    for (PrimitiveData& data : primitives) {
        for (std::size_t vertex = 0; vertex < data.vertices.size(); vertex += kModelVertexStride) {
            data.vertices[vertex] = (data.vertices[vertex] - centerX) * sizeScale;
            data.vertices[vertex + 1] = (data.vertices[vertex + 1] - minY) * sizeScale;
            data.vertices[vertex + 2] = (data.vertices[vertex + 2] - centerZ) * sizeScale;
        }
    }

    result.normalization =
        multiply(math::scale({sizeScale, sizeScale, sizeScale}), translation({-centerX, -minY, -centerZ}));
    result.denormalization = inverseAffine(result.normalization);
    result.primitives = std::move(primitives);
    return result;
}

} // namespace
ModelData loadGltf(const std::filesystem::path& path, float targetHeight) {
    try {
        return loadModel(path, targetHeight);
    } catch (const std::exception& error) {
        throw std::runtime_error("glTF import " + path.string() + ": " + error.what());
    }
}

} // namespace engine::assets
