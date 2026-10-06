#include "GameConfig.h"
#include "TestSupport.h"
#include "engine/graphics/SceneRenderer.h"
#include "game/SceneryLayout.h"
#include "game/WorldScene.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <glad/glad.h>
#include <set>
#include <type_traits>
#include <vector>

using namespace engine::graphics;
using namespace engine::math;
namespace {
struct Bounds {
    float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
};
Bounds bounds(const Quad& quad, const TrackCurve* track = nullptr) {
    Bounds result;
    for (float x : {-.5f, .5f}) {
        for (float y : {-.5f, .5f}) {
            auto p = transformPoint(quad.model, {x, y, 0});
            if (track) {
                p.x -= track->offset(p.z);
            }
            result.x0 = std::min(result.x0, p.x);
            result.x1 = std::max(result.x1, p.x);
            result.z0 = std::min(result.z0, p.z);
            result.z1 = std::max(result.z1, p.z);
        }
    }
    return result;
}
void parkLayoutChecks(const game::GameAssets& assets, game::WorldScene& world) {
    const TrackCurve track({GameConfig::kWorldCurvature, GameConfig::kRoadSway, -5, 24, GameConfig::kLaneStartZ});
    constexpr float sidewalkEdge =
        (GameConfig::kLanePositions.back() - GameConfig::kLanePositions.front() + 1.85f) * .5f + 1.1f;
    std::set<int> treeHeights;
    bool left = false, right = false, leftGarden = false, rightGarden = false, trimmedTile = false;
    int joinedParks = 0;
    for (float scroll : {0.0f, 5.0f, 37.0f, 99.0f, 1217.0f}) {
        const Scene scene = world.build(0, scroll, 1, {}, {});
        std::vector<Bounds> lawns, roofs, localLawns, localFences, fenceBounds;
        std::vector<const Quad*> doors, fenceQuads, grassQuads;
        std::vector<Mat4> poses;
        const auto parkPose = [&](const DrawCommand& object) -> const Mat4* {
            if (const auto* quad = std::get_if<Quad>(&object);
                quad && (quad->texture == &assets.texture(game::TextureId::Grass) ||
                         quad->texture == &assets.texture(game::TextureId::Fence))) {
                return &quad->model;
            }
            if (const auto* tree = std::get_if<ModelInstance>(&object);
                tree && tree->model == &assets.model(game::ModelId::Tree)) {
                return &tree->transform;
            }
            return nullptr;
        };
        int fences = 0, trees = 0;
        for (const auto& object : scene.objects) {
            if (const auto* pose = parkPose(object)) {
                poses.push_back(*pose);
            }
            if (const auto* quad = std::get_if<Quad>(&object)) {
                if (quad->texture == &assets.texture(game::TextureId::Grass)) {
                    grassQuads.push_back(quad);
                    CHECK(quad->pbrSurface && quad->surfaceMaterial.roughness > .9f && !quad->castsShadow);
                    CHECK(quad->materialTextures.properties ==
                          assets.materialTextures(game::TextureId::Grass).properties);
                    CHECK(quad->materialTextures.normal == assets.materialTextures(game::TextureId::Grass).normal);
                    CHECK(quad->worldSize.x > 0 && quad->worldSize.x <= GameConfig::kGrassTileSize + .00001f);
                    CHECK(near(quad->textureScale.x, quad->worldSize.x / GameConfig::kGrassTileSize));
                    CHECK(near(quad->textureScale.y, 1)); // Trimmed tiles retain the same texel density.
                    trimmedTile |= quad->textureScale.x < .99f;
                    const float x = quad->model.values[12] - track.offset(quad->model.values[14]);
                    const auto localBounds = bounds(*quad, &track);
                    CHECK(localBounds.x0 > sidewalkEdge || localBounds.x1 < -sidewalkEdge);
                    left |= x < 0;
                    right |= x > 0;
                    lawns.push_back(bounds(*quad));
                    localLawns.push_back(bounds(*quad, &track));
                } else if (quad->texture == &assets.texture(game::TextureId::Fence)) {
                    fenceQuads.push_back(quad);
                    CHECK(quad->pbrSurface && quad->doubleSided && near(quad->alphaCutoff, .5f));
                    CHECK(quad->materialTextures.properties ==
                          assets.materialTextures(game::TextureId::Fence).properties);
                    CHECK(quad->materialTextures.normal == assets.materialTextures(game::TextureId::Fence).normal);
                    CHECK(quad->castsShadow && quad->receivesShadow);
                    ++fences;
                    localFences.push_back(bounds(*quad, &track));
                    fenceBounds.push_back(bounds(*quad));
                } else if (quad->texture == &assets.texture(game::TextureId::BrickDoor)) {
                    doors.push_back(quad);
                } else if (quad->texture == &assets.texture(game::TextureId::BuildingTop)) {
                    roofs.push_back(bounds(*quad));
                }
            }
        }
        for (const auto& lawn : lawns) {
            for (const auto& roof : roofs) {
                CHECK(lawn.x1 <= roof.x0 + .00002f || roof.x1 <= lawn.x0 + .00002f || lawn.z1 <= roof.z0 ||
                      roof.z1 <= lawn.z0); // Gardens share their wall boundary without extending under the building.
            }
        }
        const auto grassy = [&](Vec3 p) {
            p.x -= track.offset(p.z);
            return std::any_of(localLawns.begin(), localLawns.end(), [&](const auto& lawn) {
                return p.x >= lawn.x0 - .001f && p.x <= lawn.x1 + .001f && p.z >= lawn.z0 - .001f &&
                       p.z <= lawn.z1 + .001f;
            });
        };
        for (const auto* fence : fenceQuads) {
            const auto& m = fence->model.values;
            const auto normal = normalize(fence->normal);
            const Vec3 a{m[12] + normal.x * .05f, 0, m[14] + normal.z * .05f};
            const Vec3 b{m[12] - normal.x * .05f, 0, m[14] - normal.z * .05f};
            CHECK(!grassy(a) || !grassy(b)); // No fence divides adjacent lawn tiles, including stepped outlines.
        }
        const int firstLot = static_cast<int>(std::floor((scroll - 12) / game::scenery::buildingSpacing));
        const int lastLot = static_cast<int>(std::ceil((scroll + 100) / game::scenery::buildingSpacing));
        for (float side : {-1.f, 1.f}) {
            for (int lot = firstLot; lot < lastLot; ++lot) {
                if (!game::scenery::hasPocketPark(game::scenery::streetLotSeed(lot, side)) ||
                    !game::scenery::hasPocketPark(game::scenery::streetLotSeed(lot + 1, side))) {
                    continue;
                }
                ++joinedParks;
                const float seam = scroll - (lot + .5f) * game::scenery::buildingSpacing;
                const float x = side * (sidewalkEdge + .6f + GameConfig::kGrassTileSize * 1.5f);
                for (int step = -10; step <= 10; ++step) {
                    const float z = seam + step * GameConfig::kGrassTileSize * .25f;
                    CHECK(grassy({x + track.offset(z), 0, z})); // Fill the old gap between consecutive green lots.
                }
                const Quad* previous = nullptr;
                for (const auto* grass : grassQuads) {
                    const auto& m = grass->model.values;
                    const float localX = m[12] - track.offset(m[14]);
                    if (side * localX <= 0 || std::abs(m[14] - seam) > GameConfig::kGrassTileSize * 2) {
                        continue;
                    }
                    if (previous) {
                        CHECK(near(grass->color.r, previous->color.r));
                        const float u = grass->textureOffset.x - previous->textureOffset.x;
                        const float v = grass->textureOffset.y - previous->textureOffset.y;
                        CHECK(near(u, std::round(u), .0002f) && near(v, std::round(v), .0002f));
                    }
                    previous = grass;
                }
                CHECK(previous); // Joined lots share texture phase and tint instead of showing a module seam.
            }
        }
        for (const auto* door : doors) {
            const float x = door->model.values[12] - track.offset(door->model.values[14]);
            const float z = door->model.values[14];
            const float pathX0 = x < 0 ? x : sidewalkEdge;
            const float pathX1 = x < 0 ? -sidewalkEdge : x;
            const auto clearEntrance = [&](const Bounds& patch) {
                return patch.x1 <= pathX0 + .002f || patch.x0 >= pathX1 - .002f ||
                       patch.z1 <= z - GameConfig::kGardenDoorPathWidth * .5f + .002f ||
                       patch.z0 >= z + GameConfig::kGardenDoorPathWidth * .5f - .002f;
            };
            CHECK(std::all_of(localLawns.begin(), localLawns.end(), clearEntrance));
            CHECK(std::all_of(localFences.begin(), localFences.end(), clearEntrance));
            const float wallX = door->model.values[12];
            const float streetX = (x < 0 ? -sidewalkEdge : sidewalkEdge) + track.offset(z);
            const float gardenX0 = std::min(wallX, streetX), gardenX1 = std::max(wallX, streetX);
            std::array<Bounds, 2> beds;
            bool before = false, after = false;
            for (const auto& lawn : lawns) {
                if (lawn.x0 < gardenX0 - .00002f || lawn.x1 > gardenX1 + .00002f ||
                    lawn.z0 < z - game::scenery::brickDepth * .5f || lawn.z1 > z + game::scenery::brickDepth * .5f) {
                    continue;
                }
                before |= lawn.z1 < z;
                after |= lawn.z0 > z;
                auto& bed = beds[lawn.z0 > z ? 1 : 0];
                bed.x0 = std::min(bed.x0, lawn.x0);
                bed.x1 = std::max(bed.x1, lawn.x1);
                bed.z0 = std::min(bed.z0, lawn.z0);
                bed.z1 = std::max(bed.z1, lawn.z1);
            }
            CHECK(before == after); // Forecourt beds flank the door; one side never blocks access to the other.
            if (before && after) {
                leftGarden |= x < 0;
                rightGarden |= x > 0;
                for (const auto& bed : beds) {
                    CHECK(near(x < 0 ? bed.x0 : bed.x1, wallX,
                               .00002f)); // No strip of pavement separates garden and facade.
                    for (const auto& lawn : lawns) {
                        if (lawn.x0 >= bed.x0 - .00002f && lawn.x1 <= bed.x1 + .00002f && lawn.z0 >= bed.z0 &&
                            lawn.z1 <= bed.z1) {
                            CHECK(std::any_of(lawns.begin(), lawns.end(), [&](const auto& edge) {
                                return near(edge.z0, lawn.z0) && near(edge.z1, lawn.z1) &&
                                       near(x < 0 ? edge.x0 : edge.x1, wallX, .00002f);
                            })); // Every garden row reaches the same straight wall, including on the curved track.
                        }
                    }
                    for (const auto& fence : fenceBounds) {
                        const bool followsWall = near(fence.x0, wallX, .00002f) && near(fence.x1, wallX, .00002f);
                        CHECK(!followsWall || fence.z1 <= bed.z0 || fence.z0 >= bed.z1);
                    }
                }
            }
        }
        for (const auto& object : scene.objects) {
            if (const auto* tree = std::get_if<ModelInstance>(&object);
                tree && tree->model == &assets.model(game::ModelId::Tree)) {
                CHECK(tree->pbrSurface);
                const auto& m = tree->transform.values;
                CHECK(m[5] >= GameConfig::kParkTreeMinHeight - .001f && m[5] <= GameConfig::kParkTreeMaxHeight + .001f);
                CHECK(std::any_of(lawns.begin(), lawns.end(), [&](const auto& lawn) {
                    return m[12] >= lawn.x0 && m[12] <= lawn.x1 && m[14] >= lawn.z0 && m[14] <= lawn.z1;
                }));
                CHECK(near(m[13], .04f - track.drop(m[14])));
                Bounds plot;
                const int lot = static_cast<int>(std::round((scroll - m[14]) / game::scenery::buildingSpacing));
                const bool negative = m[12] - track.offset(m[14]) < 0;
                for (const auto& lawn : localLawns) {
                    const float centerZ = (lawn.z0 + lawn.z1) * .5f;
                    if ((lawn.x0 + lawn.x1 < 0) == negative &&
                        static_cast<int>(std::round((scroll - centerZ) / game::scenery::buildingSpacing)) == lot) {
                        plot.x0 = std::min(plot.x0, lawn.x0);
                        plot.x1 = std::max(plot.x1, lawn.x1);
                        plot.z0 = std::min(plot.z0, lawn.z0);
                        plot.z1 = std::max(plot.z1, lawn.z1);
                    }
                }
                CHECK(plot.x1 - plot.x0 >= GameConfig::kParkMinWidthTiles * GameConfig::kGrassTileSize - .002f);
                CHECK(plot.z1 - plot.z0 >= GameConfig::kParkMinLengthTiles * GameConfig::kGrassTileSize - .002f);
                for (const auto& roof : roofs) {
                    const float dx = m[12] - std::clamp(m[12], roof.x0, roof.x1);
                    const float dz = m[14] - std::clamp(m[14], roof.z0, roof.z1);
                    CHECK(dx * dx + dz * dz > m[5] * m[5] * .25f); // Rotated canopies retain building clearance too.
                }
                treeHeights.insert(static_cast<int>(m[5] * 100));
                ++trees;
            }
        }
        CHECK(!lawns.empty() && fences > 0 && trees > 0);
        const auto& moved = world.build(0, scroll + .05f, 40, {}, {}, 0, 1);
        std::size_t index = 0;
        for (const auto& object : moved.objects) {
            if (const auto* pose = parkPose(object)) {
                CHECK(index < poses.size());
                const auto& a = poses[index++].values;
                const auto& b = pose->values;
                CHECK(std::abs(b[14] - a[14] - .05f) < .0002f);
                CHECK(std::abs((b[12] - track.offset(b[14])) - (a[12] - track.offset(a[14]))) < .001f);
                CHECK(std::abs((b[13] + track.drop(b[14])) - (a[13] + track.drop(a[14]))) < .001f);
            }
        }
        CHECK(index == poses.size());
    }
    CHECK(left && right && leftGarden && rightGarden && trimmedTile && treeHeights.size() > 5 && joinedParks > 0);
}

void leafMotionChecks(const game::GameAssets& assets) {
    const TrackCurve track({GameConfig::kWorldCurvature, GameConfig::kRoadSway, -5, 24, GameConfig::kLaneStartZ});
    game::WorldScene moving(assets), centerLane(assets), fastPass(assets);
    const auto checkLeaves = [&](const Scene& scene) {
        CHECK(!scene.particles.empty() && scene.particles.size() <= GameConfig::kMaxVisibleLeaves);
        for (const auto& leaf : scene.particles) {
            CHECK(leaf.texture == &assets.texture(game::TextureId::Leaf));
            CHECK(leaf.materialTextures.properties == assets.materialTextures(game::TextureId::Leaf).properties);
            CHECK(leaf.materialTextures.normal == assets.materialTextures(game::TextureId::Leaf).normal);
            CHECK(leaf.doubleSided && !leaf.castsShadow && leaf.pbrSurface);
            CHECK(leaf.opacity > 0 && leaf.opacity <= 1);
            CHECK(near(leaf.worldSize.x / leaf.worldSize.y, leaf.texture->aspect()));
            CHECK(near(leaf.affineStrength, 0) && near(leaf.alphaCutoff, .1f));
        }
    };
    const auto sameParticles = [&](const Scene& a, const Scene& b) {
        CHECK(a.particles.size() == b.particles.size());
        for (std::size_t index = 0; index < a.particles.size(); ++index) {
            CHECK(near(a.particles[index].opacity, b.particles[index].opacity));
            for (std::size_t axis = 0; axis < 16; ++axis) {
                CHECK(near(a.particles[index].model.values[axis], b.particles[index].model.values[axis]));
            }
        }
    };
    const Scene initial = moving.build(0, 37, 10, {}, {});
    checkLeaves(initial);
    const auto groundHeight = [&](const Quad& leaf) { return -.04f - track.drop(leaf.model.values[14]) + .018f; };
    const auto target = std::find_if(initial.particles.begin(), initial.particles.end(), [&](const auto& leaf) {
        const auto& m = leaf.model.values;
        return m[12] - track.offset(m[14]) > 0 && m[14] < 1 && m[14] > -40 && near(m[13], groundHeight(leaf));
    });
    CHECK(target != initial.particles.end());
    const float width = target->worldSize.x;
    const auto findTarget = [&](const Scene& scene) -> const Quad* {
        const auto found = std::find_if(scene.particles.begin(), scene.particles.end(),
                                        [&](const auto& leaf) { return near(leaf.worldSize.x, width, .000001f); });
        return found == scene.particles.end() ? nullptr : &*found;
    };
    const float scroll = 37 + GameConfig::kPlayerZ - target->model.values[14] - .2f;
    const float playerX = GameConfig::kLanePositions.back();
    moving.build(playerX, scroll - 1.5f, 20, {}, {});
    moving.build(playerX, scroll, 20.15f, {}, {});
    const Scene lifted = moving.build(playerX, scroll, 20.7f, {}, {});
    const auto* flying = findTarget(lifted);
    CHECK(flying && flying->model.values[13] > groundHeight(*flying) + .5f);
    CHECK(flying->opacity > .1f && flying->opacity < .95f);
    sameParticles(lifted, moving.build(playerX, scroll, 20.7f, {}, {})); // Frozen game time freezes flutter and fading.
    const auto& faded = moving.build(playerX, scroll, 20.15f + GameConfig::kLeafLiftSeconds + .1f, {}, {});
    CHECK(!findTarget(faded));
    CHECK(!findTarget(
        moving.build(playerX, scroll, 24, {}, {})));         // Spent roadside leaves cannot respawn behind the runner.
    sameParticles(initial, moving.build(0, 37, 10, {}, {})); // Restart restores the seeded ground litter.

    centerLane.build(0, scroll - 1.5f, 20, {}, {});
    const Scene undisturbed = centerLane.build(0, scroll, 20.7f, {}, {});
    const auto* resting = findTarget(undisturbed);
    CHECK(resting && near(resting->model.values[13], groundHeight(*resting)) && near(resting->opacity, 1));
    CHECK(near(resting->model.values[14], GameConfig::kPlayerZ - .2f, .0001f));
    const Scene fluttered = centerLane.build(0, scroll, 21, {}, {});
    CHECK(fluttered.particles.size() == undisturbed.particles.size());
    CHECK(std::any_of(fluttered.particles.begin(), fluttered.particles.end(), [&](const auto& leaf) {
        const auto previous = std::find_if(undisturbed.particles.begin(), undisturbed.particles.end(),
                                           [&](const auto& before) { return before.worldSize.x == leaf.worldSize.x; });
        return previous != undisturbed.particles.end() &&
               std::abs(previous->model.values[13] - leaf.model.values[13]) > .05f;
    })); // Canopy leaves continue falling even without nearby player interaction.

    fastPass.build(playerX, scroll - 4, 30, {}, {});
    const auto* swept = findTarget(fastPass.build(playerX, scroll + 4, 30.2f, {}, {}));
    CHECK(swept &&
          swept->model.values[13] > groundHeight(*swept) + .1f); // Crossing between frames still disturbs leaves.
}
void uploadPattern(const Texture& texture, bool mask) {
    const std::array<unsigned char, 16> pixels =
        mask ? std::array<unsigned char, 16>{255, 255, 255, 0, 255, 255, 255, 255, 255, 255, 255, 0, 255, 255, 255, 255}
             : std::array<unsigned char, 16>{0, 0, 128, 255, 0, 0, 128, 255, 0, 0, 128, 255, 0, 0, 128, 255};
    glBindTexture(GL_TEXTURE_2D, texture.id());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}
std::array<int, 3> sample(const std::vector<unsigned char>& pixels, const Scene& scene, Size size, Vec3 point) {
    const auto projection = scene.camera.viewProjection(float(size.width) / size.height);
    const auto clip = transformPoint(projection, point);
    const auto& m = projection.values;
    const float w = m[3] * point.x + m[7] * point.y + m[11] * point.z + m[15];
    const int x = static_cast<int>((clip.x / w * .5f + .5f) * size.width);
    const int y = static_cast<int>((clip.y / w * .5f + .5f) * size.height);
    CHECK(x >= 0 && x < size.width && y >= 0 && y < size.height);
    const auto index = (y * size.width + x) * 3;
    return {pixels[index], pixels[index + 1], pixels[index + 2]};
}
std::vector<unsigned char> frame(SceneRenderer& renderer, const Scene& scene, Size size) {
    renderer.render(scene, size);
    std::vector<unsigned char> pixels(static_cast<std::size_t>(size.width) * size.height * 3);
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, size.width, size.height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    CHECK(glGetError() == GL_NO_ERROR);
    return pixels;
}
void sceneryMaterialChecks(const game::GameAssets& assets, Size size) {
    const auto& tree = assets.model(game::ModelId::Tree).primitives().front().material;
    CHECK(tree.textures().properties && tree.textures().normal && tree.packedOcclusion && tree.packedReflectivity);
    CHECK(near(tree.normalScale, GameConfig::kTreeNormalScale));
    CHECK(tree.surfaceMaterial.metallic == 0 && tree.surfaceMaterial.roughness > .8f); // Retain the imported finish.
    struct Surface {
        const Texture* color;
        MaterialTextures maps;
        SurfaceMaterial finish;
        float cutoff;
    };
    const std::array<Surface, 4> surfaces = {{
        {&assets.texture(game::TextureId::Grass), assets.materialTextures(game::TextureId::Grass), {1, 0, 1}, 0},
        {&assets.texture(game::TextureId::Fence), assets.materialTextures(game::TextureId::Fence), {1, 1, 1}, .5f},
        {&assets.texture(game::TextureId::Leaf), assets.materialTextures(game::TextureId::Leaf), {1, 0, 1}, .1f},
        {&tree.texture, tree.textures(), tree.surfaceMaterial, tree.alphaCutoff},
    }};
    const auto texels = [](const Texture& texture) {
        std::vector<unsigned char> bytes(static_cast<std::size_t>(texture.size().width) * texture.size().height * 4);
        glBindTexture(GL_TEXTURE_2D, texture.id());
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        CHECK(glGetError() == GL_NO_ERROR);
        return bytes;
    };
    for (std::size_t index = 0; index < surfaces.size(); ++index) {
        const auto& surface = surfaces[index];
        CHECK(surface.maps.properties && surface.maps.normal && surface.maps.occlusion == surface.maps.properties);
        CHECK(surface.maps.packedReflectivity && !surface.maps.glass && surface.maps.normalScale > 0);
        CHECK(surface.maps.properties->repeats() == surface.color->repeats());
        CHECK(surface.maps.normal->repeats() == surface.color->repeats());
        const auto colors = texels(*surface.color), properties = texels(*surface.maps.properties),
                   normals = texels(*surface.maps.normal);
        CHECK(colors.size() == properties.size() && colors.size() == normals.size());
        int opaque = 0, relief = 0, paint = 0, metal = 0, bark = 0, foliage = 0;
        for (std::size_t offset = 0; offset < colors.size(); offset += 4) {
            const int r = colors[offset], g = colors[offset + 1], b = colors[offset + 2];
            if (colors[offset + 3] == 0) {
                CHECK(properties[offset] == 255 && properties[offset + 1] == 255 && properties[offset + 2] == 0 &&
                      properties[offset + 3] == 0);
                CHECK(normals[offset] == 128 && normals[offset + 1] == 128 && normals[offset + 2] == 255);
                continue; // Transparent pixels cannot add dark rims or raised silhouette edges.
            }
            ++opaque;
            relief += std::abs(int(normals[offset]) - 128) + std::abs(int(normals[offset + 1]) - 128) > 2;
            if (index == 1) {
                if (std::max({r, g, b}) <= 26) {
                    CHECK(properties[offset + 1] > 190 && properties[offset + 2] == 0 && properties[offset + 3] < 110);
                    ++paint;
                } else if (std::min({r, g, b}) > 170) {
                    CHECK(properties[offset + 1] < 150 && properties[offset + 2] > 220 && properties[offset + 3] > 190);
                    ++metal;
                }
            } else {
                CHECK(properties[offset + 2] == 0); // Grass, foliage and bark remain dielectric.
                if (index == 0) {
                    CHECK(properties[offset + 1] > 220 && properties[offset + 3] < 125);
                } else if (index == 2) {
                    CHECK(properties[offset + 1] < 190 && properties[offset + 3] < 130);
                } else if (r > g * 1.15f && r > b * 1.5f) {
                    CHECK(properties[offset + 1] > 220 && properties[offset + 3] < 80);
                    ++bark;
                } else if (g > r + 15 && g > b + 30) {
                    CHECK(properties[offset + 1] < 190 && properties[offset + 3] > 80);
                    ++foliage;
                }
            }
        }
        CHECK(opaque > 100 && relief > opaque / 100);
        CHECK(index != 1 || (paint > 100 && metal > 20));
        CHECK(index != 3 || (bark > 200 && foliage > 200));
    }

    RenderSettings settings;
    settings.retroSize = size;
    settings.retroPixelStep = settings.retroAffineStrength = settings.retroDitherStrength = 0;
    settings.worldReflectionMapSize = 0;
    settings.shadowMapSize = 64;
    SceneRenderer renderer(settings);
    Scene scene;
    scene.camera.position = {0, 0, 4};
    scene.camera.target = {0, 0, 0};
    scene.sky.top = scene.sky.bottom = {.11f, .27f, .42f};
    scene.sky.sunSize = scene.sky.cloudCoverage = 0;
    scene.light.ambient = .2f;
    scene.light.direction = {-.8f, .65f, 1};
    const auto differences = [](const auto& a, const auto& b) {
        int changed = 0;
        for (std::size_t index = 0; index < a.size(); index += 3) {
            changed += std::abs(int(a[index]) - int(b[index])) > 1 ||
                       std::abs(int(a[index + 1]) - int(b[index + 1])) > 1 ||
                       std::abs(int(a[index + 2]) - int(b[index + 2])) > 1;
        }
        return changed;
    };
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        scene.objects.clear();
        const auto background = frame(renderer, scene, size);
        for (const auto& surface : surfaces) {
            Quad quad;
            quad.model = scale({4, 4, 1});
            quad.normal = {0, 0, 1};
            quad.texture = surface.color;
            quad.materialTextures = surface.maps;
            quad.surfaceMaterial = surface.finish;
            quad.pbrSurface = quad.doubleSided = true;
            quad.alphaCutoff = surface.cutoff;
            quad.castsShadow = quad.receivesShadow = false;
            scene.objects = {quad};
            const auto detailed = frame(renderer, scene, size);
            std::get<Quad>(scene.objects[0]).materialTextures.normalScale = 0;
            CHECK(differences(detailed, frame(renderer, scene, size)) >
                  30); // Relief affects real lighting in both modes.
            std::get<Quad>(scene.objects[0]).materialTextures = {};
            const auto uniform = frame(renderer, scene, size);
            CHECK(differences(detailed, uniform) > 100);
            for (std::size_t index = 0; index < uniform.size(); index += 3) {
                if (uniform[index] == background[index] && uniform[index + 1] == background[index + 1] &&
                    uniform[index + 2] == background[index + 2]) {
                    CHECK(detailed[index] == background[index] && detailed[index + 1] == background[index + 1] &&
                          detailed[index + 2] == background[index + 2]);
                }
            }
        }
    }
}
void cutoutChecks(Size size) {
    const auto path = std::filesystem::path(TEST_ASSET_DIR) / "textures/grassy/spikedfence_decor.png";
    Texture texture(path, {TextureFilter::Nearest, true});
    uploadPattern(texture, true);
    engine::assets::ModelData data;
    engine::assets::PrimitiveData primitive;
    primitive.vertices = {-2, -2, 0, 0, 0, 1, .25f,  0, 2,  -2, 0, 0, 0, 1, 2.25f, 0,
                          2,  2,  0, 0, 0, 1, 2.25f, 1, -2, 2,  0, 0, 0, 1, .25f,  1};
    primitive.indices = {0, 1, 2, 0, 2, 3};
    primitive.material.baseColor = {.6f, 0, 0, 1};
    primitive.material.baseTexture = path;
    primitive.material.baseTextureNearest = true;
    primitive.material.baseTextureRepeat = true;
    primitive.material.alphaCutoff = .5f;
    primitive.material.doubleSided = true;
    primitive.material.metallic = 0;
    data.primitives.push_back(std::move(primitive));
    Model model(data);
    // Model UVs deliberately include tiling/offset; their shadow pass must read UV0 rather than normal xy.
    uploadPattern(model.primitives()[0].material.texture, true);
    Quad fence;
    fence.model = scale({4, 4, 1});
    fence.normal = {0, 0, 1};
    fence.color = {.8f, 0, 0};
    fence.texture = &texture;
    fence.textureScale = {2, 1};
    fence.textureOffset = {.25f, 0};
    fence.alphaCutoff = .5f;
    fence.doubleSided = fence.pbrSurface = true;
    fence.surfaceMaterial = {1, 0, 0};
    fence.castsShadow = fence.receivesShadow = false;
    Quad unlitFence = fence;
    unlitFence.pbrSurface = false;
    const std::array<DrawCommand, 4> cutouts = {fence, unlitFence, ModelInstance{identity(), &model},
                                                ModelInstance{identity(), &model, true}};
    RenderSettings settings;
    settings.retroSize = size;
    settings.retroPixelStep = settings.retroAffineStrength = settings.retroDitherStrength = 0;
    settings.shadowMapSize = 512;
    settings.worldReflectionMapSize = 0;
    SceneRenderer renderer(settings);
    Scene scene;
    scene.sky.sunSize = scene.sky.cloudCoverage = 0;
    scene.light.ambient = 1;
    scene.camera.position = {0, 0, 8};
    scene.camera.target = {0, 0, 0};
    scene.light.shadow.halfWidth = scene.light.shadow.halfHeight = 8;
    scene.light.shadow.lightDistance = 24;
    scene.light.shadow.farPlane = 64;
    Quad backdrop;
    backdrop.model = multiply(translation({0, 0, -1}), scale({12, 12, 1}));
    backdrop.normal = {0, 0, 1};
    backdrop.color = {0, 0, 1};
    backdrop.receivesShadow = false;
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        for (const auto& cutout : cutouts) {
            scene.objects = {backdrop, cutout};
            const auto pixels = frame(renderer, scene, size);
            for (float x : {-1.25f, .75f}) {
                const auto color = sample(pixels, scene, size, {x, 0, 0});
                CHECK(color[0] > 60 && color[0] > color[2] + 60); // PBR models can also reflect a little blue sky.
            }
            for (float x : {-.25f, 1.75f}) {
                const auto color = sample(pixels, scene, size, {x, 0, 0});
                CHECK(color[0] == 0 && color[2] > 240); // Holes preserve the opaque world behind them.
            }
        }
        scene.light.direction = {0, 0, 1};
        scene.light.ambient = .2f;
        for (auto cutout : cutouts) {
            scene.objects = {backdrop, cutout};
            const auto front = sample(frame(renderer, scene, size), scene, size, {-1.25f, 0, 0});
            std::visit(
                [](auto& item) {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, Quad>) {
                        item.model = multiply(rotateY(kPi), scale({4, 4, 1}));
                        item.normal = {0, 0, -1};
                    } else if constexpr (std::is_same_v<T, ModelInstance>) {
                        item.transform = rotateY(kPi);
                    }
                },
                cutout);
            scene.objects = {backdrop, cutout};
            const auto back = sample(frame(renderer, scene, size), scene, size, {1.25f, 0, 0});
            CHECK(front[0] > 80 && std::abs(front[0] - back[0]) <= 8);
        }
        backdrop.model = scale({12, 12, 1});
        backdrop.color = {1, 1, 1};
        backdrop.receivesShadow = true;
        scene.light.direction = {1, 0, 1};
        scene.light.ambient = .2f;
        scene.objects = {backdrop};
        const auto lit = frame(renderer, scene, size);
        for (auto cutout : cutouts) {
            std::visit(
                [](auto& caster) {
                    using T = std::decay_t<decltype(caster)>;
                    if constexpr (std::is_same_v<T, Quad>) {
                        caster.model = multiply(translation({4, 0, 4}), scale({4, 4, 1}));
                        caster.castsShadow = true;
                    } else if constexpr (std::is_same_v<T, ModelInstance>) {
                        caster.transform = translation({4, 0, 4});
                    }
                },
                cutout);
            scene.objects = {backdrop, cutout};
            const auto shaded = frame(renderer, scene, size);
            for (float x : {-1.25f, .75f}) {
                CHECK(sample(shaded, scene, size, {x, 0, 0})[0] < sample(lit, scene, size, {x, 0, 0})[0] - 40);
            }
            for (float x : {-.25f, 1.75f}) {
                CHECK(std::abs(sample(shaded, scene, size, {x, 0, 0})[0] - sample(lit, scene, size, {x, 0, 0})[0]) <=
                      8);
            }
        }
        backdrop.model = multiply(translation({0, 0, -1}), scale({12, 12, 1}));
        backdrop.color = {0, 0, 1};
        backdrop.receivesShadow = false;
        scene.light.ambient = 1;
    }

    Texture mirrorTexture(path, {TextureFilter::Nearest});
    uploadPattern(mirrorTexture, false);
    Quad mirror;
    mirror.model = scale({4, 4, 1});
    mirror.normal = {0, 0, 1};
    mirror.texture = &mirrorTexture;
    mirror.worldSize = {4, 4};
    mirror.skyscraperWindows = mirror.buildingSurface = true;
    mirror.windowMaterial = {.03f, 1, true, 1, 0};
    mirror.castsShadow = false;
    settings.worldReflectionMapSize = 32;
    settings.planarReflectionSize = 256;
    SceneRenderer reflections(settings);
    scene.camera.position = {0, 0, 4};
    backdrop.model = multiply(translation({0, 0, 9}), scale({8, 8, 1}));
    backdrop.color = {0, 1, 0};
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        reflections.setMode(mode);
        for (auto cutout : cutouts) {
            std::visit(
                [](auto& item) {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, Quad>) {
                        item.model = multiply(translation({0, 0, 7}), scale({4, 4, 1}));
                    } else if constexpr (std::is_same_v<T, ModelInstance>) {
                        item.transform = translation({0, 0, 7});
                    }
                },
                cutout);
            scene.objects = {mirror, backdrop, cutout};
            const auto pixels = frame(reflections, scene, size);
            for (float x : {-1.25f, .75f}) {
                const auto color = sample(pixels, scene, size, {x, 0, -7});
                CHECK(color[0] > color[1] + 40);
            }
            for (float x : {-.25f, 1.75f}) {
                const auto color = sample(pixels, scene, size, {x, 0, -7});
                CHECK(color[1] > color[0] + 40 && color[1] > color[2] + 40);
            }
        }
        Quad particle = unlitFence;
        particle.model = multiply(translation({0, 0, 7}), scale({4, 4, 1}));
        particle.opacity = .5f;
        scene.objects = {mirror, backdrop};
        scene.particles = {particle};
        const auto reflected = frame(reflections, scene, size);
        const auto solid = sample(reflected, scene, size, {-1.25f, 0, -7});
        const auto hole = sample(reflected, scene, size, {-.25f, 0, -7});
        CHECK(solid[0] > 60 && solid[1] > 60); // Fading leaves composite over reflected scenery.
        CHECK(hole[0] == 0 && hole[1] > 240);
        scene.particles.clear();
    }
}

void particleRenderingChecks(Size size) {
    Texture texture(std::filesystem::path(TEST_ASSET_DIR) / "textures/grassy/leaf.png", {TextureFilter::Nearest, true});
    uploadPattern(texture, true);
    RenderSettings settings;
    settings.retroSize = size;
    settings.retroPixelStep = settings.retroAffineStrength = settings.retroDitherStrength = 0;
    settings.shadowMapSize = 64;
    settings.worldReflectionMapSize = 0;
    SceneRenderer renderer(settings);
    Scene scene;
    scene.camera.position = {0, 0, 8};
    scene.camera.target = {0, 0, 0};
    scene.sky.sunSize = scene.sky.cloudCoverage = 0;
    scene.light.ambient = 1;
    Quad backdrop;
    backdrop.model = multiply(translation({0, 0, -1}), scale({12, 12, 1}));
    backdrop.normal = {0, 0, 1};
    backdrop.color = {0, 0, 1};
    backdrop.receivesShadow = false;
    Quad green;
    green.model = scale({4, 4, 1});
    green.normal = {0, 0, 1};
    green.texture = &texture;
    green.textureScale = {2, 1};
    green.textureOffset = {.25f, 0};
    green.alphaCutoff = .5f;
    green.color = {0, 1, 0};
    green.opacity = .6f;
    green.receivesShadow = false;
    Quad red = green;
    red.model = multiply(translation({0, 0, 1}), scale({4, 4, 1}));
    red.color = {1, 0, 0};
    red.opacity = .5f;
    for (auto mode : {RenderMode::Normal, RenderMode::Psx}) {
        renderer.setMode(mode);
        scene.objects = {backdrop};
        scene.particles = {red, green}; // Deliberately submit the nearer particle first.
        const auto sorted = frame(renderer, scene, size);
        const auto color = sample(sorted, scene, size, {-1.25f, 0, 0});
        CHECK(color[0] > color[1] + 25 && color[1] > color[2] + 10);
        const auto hole = sample(sorted, scene, size, {-.25f, 0, 0});
        CHECK(hole[0] == 0 && hole[1] == 0 && hole[2] > 240);
        std::reverse(scene.particles.begin(), scene.particles.end());
        CHECK(frame(renderer, scene, size) == sorted); // Camera depth, rather than submission order, controls blending.
        Quad occluder = backdrop;
        occluder.model = multiply(translation({0, 0, 2}), scale({4, 4, 1}));
        occluder.color = {1, 1, 0};
        scene.objects.push_back(occluder);
        const auto hidden = sample(frame(renderer, scene, size), scene, size, {-1.25f, 0, 0});
        CHECK(hidden[0] > 240 && hidden[1] > 240 && hidden[2] == 0);
        scene.objects = {backdrop};
        green.opacity = .25f;
        scene.particles = {green};
        const auto faded = sample(frame(renderer, scene, size), scene, size, {-1.25f, 0, 0});
        green.opacity = .75f;
        scene.particles = {green};
        const auto visible = sample(frame(renderer, scene, size), scene, size, {-1.25f, 0, 0});
        CHECK(faded[1] > 40 &&
              visible[1] > faded[1] + 100); // Opacity fades coverage without triggering texture cutout.
    }
}
} // namespace
void sceneryChecks(const game::GameAssets& assets, game::WorldScene& world, Size size) {
    parkLayoutChecks(assets, world);
    leafMotionChecks(assets);
    sceneryMaterialChecks(assets, size);
    cutoutChecks(size);
    particleRenderingChecks(size);
}
