#include "PlanarReflectionPass.h"
#include "RenderState.h"
#include "engine/graphics/RenderTarget.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

namespace engine::graphics::detail {
using namespace math;
namespace {
bool reflectiveFacade(const Quad& surface) {
    return surface.texture && surface.opacity > 0 && surface.windowMaterial.worldReflections &&
           (surface.skyscraperWindows || (surface.buildingSurface && surface.materialTextures.glass));
}
Vec3 center(const Quad& surface) {
    return {surface.model.values[12], surface.model.values[13], surface.model.values[14]};
}
Vec3 mirrorPoint(Vec3 point, Vec3 normal, float planeOffset) {
    const float distance = 2 * (dot(normal, point) - planeOffset);
    return {point.x - normal.x * distance, point.y - normal.y * distance, point.z - normal.z * distance};
}
float smoothTransition(float start, float end, float value) {
    const float t = std::clamp((value - start) / (end - start), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}
std::optional<ScreenBounds> facadeBounds(const Quad& surface, const Mat4& viewProjection) {
    const auto& m = multiply(viewProjection, surface.model).values;
    std::array<std::array<float, 4>, 4> corners{};
    int index = 0;
    for (float x : {-.5f, .5f}) {
        for (float y : {-.5f, .5f}) {
            corners[index++] = {m[0] * x + m[4] * y + m[12], m[1] * x + m[5] * y + m[13], m[2] * x + m[6] * y + m[14],
                                m[3] * x + m[7] * y + m[15]};
        }
    }
    // Reject the rectangle only if every corner is outside one frustum plane.
    for (int axis = 0; axis < 3; ++axis) {
        for (float sign : {-1.0f, 1.0f}) {
            if (std::all_of(corners.begin(), corners.end(),
                            [&](const auto& corner) { return corner[3] + sign * corner[axis] < 0; })) {
                return {};
            }
        }
    }
    ScreenBounds bounds;
    for (const auto& point : corners) {
        if (point[3] <= .001f) {
            return ScreenBounds{-1, -1, 1, 1}; // Conservatively retain near-plane crossings.
        }
        bounds.left = std::min(bounds.left, point[0] / point[3]);
        bounds.right = std::max(bounds.right, point[0] / point[3]);
        bounds.bottom = std::min(bounds.bottom, point[1] / point[3]);
        bounds.top = std::max(bounds.top, point[1] / point[3]);
    }
    bounds.left = std::max(bounds.left, -1.0f);
    bounds.right = std::min(bounds.right, 1.0f);
    bounds.bottom = std::max(bounds.bottom, -1.0f);
    bounds.top = std::min(bounds.top, 1.0f);
    if (bounds.right <= bounds.left || bounds.top <= bounds.bottom) {
        return {};
    }
    return bounds;
}
float facadeDistance(const Quad& surface, Vec3 point) {
    const auto& m = surface.model.values;
    const Vec3 x = {m[0], m[1], m[2]}, y = {m[4], m[5], m[6]};
    const float width = std::sqrt(dot(x, x)), height = std::sqrt(dot(y, y));
    const Vec3 tangent = normalize(x), bitangent = normalize(y);
    const auto offset = point - center(surface);
    const float u = std::clamp(dot(offset, tangent), -width * .5f, width * .5f);
    const float v = std::clamp(dot(offset, bitangent), -height * .5f, height * .5f);
    const Vec3 closestOffset = {offset.x - tangent.x * u - bitangent.x * v, offset.y - tangent.y * u - bitangent.y * v,
                                offset.z - tangent.z * u - bitangent.z * v};
    return std::sqrt(dot(closestOffset, closestOffset));
}
struct PlanarCapture {
    Vec3 normal;
    float offset = 0;
    ScreenBounds bounds;
    float score = 0;
    Mat4 viewProjection;
    RenderTarget* target = nullptr;
    float strength = 1;
    std::vector<std::uint64_t> ids;
    int history = -1;
    std::size_t slot = 0;
    bool selected = false;
    float detail = 1;
    float distance = 0;
    float nextUpdate = 0;
    std::uint64_t anchorGroup = 0;
    Vec3 anchorPosition;
    Mat4 capturedProjection;
    Camera capturedCamera;
    Vec2 textureScale = {1, 1};
    float maxLod = 0;
};
struct ReflectionGroup {
    Vec3 minimum = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::infinity()};
    Vec3 maximum = {-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                    -std::numeric_limits<float>::infinity()};
    float distance = std::numeric_limits<float>::infinity();
    float strength = 1;
    float nearDepth = std::numeric_limits<float>::infinity(), farDepth = 0;
    std::vector<const Quad*> surfaces;
    std::vector<Vec2> silhouette;
    bool solid = false;
    bool occluded = false;
};
float turn(Vec2 a, Vec2 b, Vec2 c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}
void projectGroup(ReflectionGroup& group, const Mat4& projection, float nearPlane) {
    std::array<Vec2, 8> points{};
    std::size_t index = 0;
    const auto& m = projection.values;
    for (float x : {group.minimum.x, group.maximum.x}) {
        for (float y : {group.minimum.y, group.maximum.y}) {
            for (float z : {group.minimum.z, group.maximum.z}) {
                const float w = m[3] * x + m[7] * y + m[11] * z + m[15];
                if (w <= nearPlane) {
                    return; // Never occlude through a near-plane crossing.
                }
                group.nearDepth = std::min(group.nearDepth, w);
                group.farDepth = std::max(group.farDepth, w);
                const auto clip = transformPoint(projection, {x, y, z});
                points[index++] = {clip.x / w, clip.y / w};
            }
        }
    }
    std::sort(points.begin(), points.end(), [](auto a, auto b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    // The convex silhouette is stricter than a screen rectangle: retain all
    // partially visible faces and gaps between neighboring buildings.
    for (auto point : points) {
        while (group.silhouette.size() >= 2 &&
               turn(group.silhouette[group.silhouette.size() - 2], group.silhouette.back(), point) <= 0) {
            group.silhouette.pop_back();
        }
        group.silhouette.push_back(point);
    }
    const auto lower = group.silhouette.size();
    for (auto it = std::next(points.rbegin()); it != points.rend(); ++it) {
        while (group.silhouette.size() > lower &&
               turn(group.silhouette[group.silhouette.size() - 2], group.silhouette.back(), *it) <= 0) {
            group.silhouette.pop_back();
        }
        group.silhouette.push_back(*it);
    }
    group.silhouette.pop_back();
    if (group.silhouette.size() < 3) {
        group.silhouette.clear();
    }
}
bool groupHiddenBy(const ReflectionGroup& group, const ReflectionGroup& occluder) {
    if (!occluder.solid || group.silhouette.empty() || occluder.silhouette.empty() ||
        group.nearDepth <= occluder.farDepth + .02f) {
        return false;
    }
    for (auto point : group.silhouette) {
        for (std::size_t i = 0; i < occluder.silhouette.size(); ++i) {
            if (turn(occluder.silhouette[i], occluder.silhouette[(i + 1) % occluder.silhouette.size()], point) <=
                .00001f) {
                return false;
            }
        }
    }
    return true;
}
bool solidGroup(const ReflectionGroup& group) {
    std::array<std::vector<ScreenBounds>, 5> panels;
    const auto extent = group.maximum - group.minimum;
    for (const auto* surface : group.surfaces) {
        if (!surface->buildingSurface || surface->opacity != 1 || surface->alphaCutoff > 0) {
            return false;
        }
        const auto normal = normalize(surface->normal);
        const auto position = center(*surface);
        int face = -1;
        float offset = 0;
        if (std::abs(normal.x) > .9999f) {
            face = normal.x > 0 ? 0 : 1;
            offset = std::abs(position.x - (normal.x > 0 ? group.maximum.x : group.minimum.x));
        } else if (std::abs(normal.z) > .9999f) {
            face = normal.z > 0 ? 2 : 3;
            offset = std::abs(position.z - (normal.z > 0 ? group.maximum.z : group.minimum.z));
        } else if (normal.y > .9999f) {
            face = 4;
            offset = std::abs(position.y - group.maximum.y);
        }
        if (face < 0 || offset > .1f) {
            return false;
        }
        const auto& m = surface->model.values;
        const Vec3 x{m[0], m[1], m[2]}, y{m[4], m[5], m[6]};
        const auto axisAligned = [&](Vec3 axis) {
            const float length = std::sqrt(dot(axis, axis));
            return std::abs(axis.x) + std::abs(axis.y) + std::abs(axis.z) <= length * 1.0001f;
        };
        if (!axisAligned(x) || !axisAligned(y) || std::abs(dot(normal, x)) > .001f ||
            std::abs(dot(normal, y)) > .001f) {
            return false;
        }
        const Vec3 radius{(std::abs(x.x) + std::abs(y.x)) * .5f, (std::abs(x.y) + std::abs(y.y)) * .5f,
                          (std::abs(x.z) + std::abs(y.z)) * .5f};
        const float horizontal = face < 2 ? position.z : position.x;
        const float vertical = face == 4 ? position.z : position.y;
        const float halfWidth = face < 2 ? radius.z : radius.x;
        const float halfHeight = face == 4 ? radius.z : radius.y;
        panels[face].push_back(
            {horizontal - halfWidth, vertical - halfHeight, horizontal + halfWidth, vertical + halfHeight});
    }
    std::array<float, 5> area{};
    for (std::size_t face = 0; face < panels.size(); ++face) {
        std::vector<float> edges;
        for (auto panel : panels[face]) {
            edges.push_back(panel.left);
            edges.push_back(panel.right);
        }
        std::sort(edges.begin(), edges.end());
        for (std::size_t i = 1; i < edges.size(); ++i) {
            const float middle = (edges[i - 1] + edges[i]) * .5f;
            std::vector<std::pair<float, float>> intervals;
            for (auto panel : panels[face]) {
                if (panel.left <= middle && panel.right >= middle)
                    intervals.emplace_back(panel.bottom, panel.top);
            }
            std::sort(intervals.begin(), intervals.end());
            float top = -std::numeric_limits<float>::infinity(), height = 0;
            for (auto [bottom, upper] : intervals) {
                height += std::max(0.0f, upper - std::max(bottom, top));
                top = std::max(top, upper);
            }
            area[face] += (edges[i] - edges[i - 1]) * height;
        }
    }
    // Union coverage avoids counting overlapping panels twice and treating
    // a wall with gaps as a closed occluder.
    return area[0] >= extent.y * extent.z - .001f && area[1] >= extent.y * extent.z - .001f &&
           area[2] >= extent.x * extent.y - .001f && area[3] >= extent.x * extent.y - .001f &&
           area[4] >= extent.x * extent.z - .001f;
}
bool sameCapture(const PlanarCapture& a, const PlanarCapture& b) {
    if (dot(a.normal, b.normal) <= .9999f) {
        return false;
    }
    if (!a.ids.empty() && !b.ids.empty()) {
        return std::any_of(a.ids.begin(), a.ids.end(),
                           [&](auto id) { return std::find(b.ids.begin(), b.ids.end(), id) != b.ids.end(); });
    }
    return std::abs(a.offset - b.offset) < .01f;
}
} // namespace
struct PlanarReflectionPass::Impl {
    RenderSettings settings;
    std::vector<std::unique_ptr<RenderTarget>> planarTargets;
    std::vector<PlanarCapture> planarCaptures;
    std::vector<PlanarCapture> planarCandidates;
    std::vector<std::size_t> distantRefreshOrder;
    std::vector<unsigned char> planarRefresh;
    std::unordered_map<std::uint64_t, ReflectionGroup> reflectionGroups;
    std::unordered_map<std::uint64_t, ReflectionGroup> previousGroups;
    std::vector<unsigned char> matchedCaptures;
    std::vector<unsigned char> usedTargetSlots;
    std::optional<float> previousPlanarTime;
    RenderMode planarMode = RenderMode::Normal;
    Size planarViewSize{};
    std::vector<PlanarReflectionView> views;
    explicit Impl(const RenderSettings& config) : settings(config) {}
    void prepareReflectionGroups(const Scene& scene, float step, bool fresh, float time, const Mat4& viewProjection) {
        reflectionGroups.swap(previousGroups);
        // Two buffers keep per-building vector capacity across frames. Reset
        // geometry while retaining storage, and erase departed IDs below.
        for (auto& [id, group] : reflectionGroups) {
            ReflectionGroup empty;
            empty.surfaces.swap(group.surfaces);
            empty.silhouette.swap(group.silhouette);
            empty.surfaces.clear();
            empty.silhouette.clear();
            group = std::move(empty);
        }
        reflectionGroups.reserve(previousGroups.size());
        for (const auto& object : scene.objects) {
            const auto* surface = std::get_if<Quad>(&object);
            if (!surface || !surface->reflectionGroupId || surface->opacity <= 0) {
                continue;
            }
            auto& group = reflectionGroups[surface->reflectionGroupId];
            group.surfaces.push_back(surface);
            group.distance = std::min(group.distance, facadeDistance(*surface, scene.camera.position));
            const auto& m = surface->model.values;
            const Vec3 radius{(std::abs(m[0]) + std::abs(m[4])) * .5f, (std::abs(m[1]) + std::abs(m[5])) * .5f,
                              (std::abs(m[2]) + std::abs(m[6])) * .5f};
            const auto position = center(*surface);
            group.minimum.x = std::min(group.minimum.x, position.x - radius.x);
            group.minimum.y = std::min(group.minimum.y, position.y - radius.y);
            group.minimum.z = std::min(group.minimum.z, position.z - radius.z);
            group.maximum.x = std::max(group.maximum.x, position.x + radius.x);
            group.maximum.y = std::max(group.maximum.y, position.y + radius.y);
            group.maximum.z = std::max(group.maximum.z, position.z + radius.z);
        }
        std::erase_if(reflectionGroups, [](const auto& entry) { return entry.second.surfaces.empty(); });
        for (auto& [id, group] : reflectionGroups) {
            const auto old = previousGroups.find(id);
            group.strength = group.distance > settings.planarReflectionDistance ? 0
                             : fresh || time == 0                               ? 1
                             : old == previousGroups.end()                      ? std::min(step, 1.0f)
                                                           : std::min(old->second.strength + step, 1.0f);
            group.solid = solidGroup(group);
            projectGroup(group, viewProjection, scene.camera.nearPlane);
        }
        for (auto& [id, group] : reflectionGroups) {
            for (const auto& [otherId, other] : reflectionGroups) {
                if (id != otherId && groupHiddenBy(group, other)) {
                    group.occluded = true;
                    break;
                }
            }
        }
    }
    void preparePlanarUpdates(const Scene& scene, float elapsed, float time) {
        distantRefreshOrder.clear();
        planarRefresh.assign(planarCaptures.size(), 0);
        for (std::size_t i = 0; i < planarCaptures.size(); ++i) {
            const auto& capture = planarCaptures[i];
            const auto cameraMotion = scene.camera.position - capture.capturedCamera.position;
            const bool cameraChanged =
                dot(cameraMotion, cameraMotion) > .25f ||
                scene.camera.verticalFov != capture.capturedCamera.verticalFov ||
                scene.camera.nearPlane != capture.capturedCamera.nearPlane ||
                scene.camera.farPlane != capture.capturedCamera.farPlane ||
                dot(normalize(scene.camera.target - scene.camera.position),
                    normalize(capture.capturedCamera.target - capture.capturedCamera.position)) < .999f ||
                dot(normalize(scene.camera.up), normalize(capture.capturedCamera.up)) < .999f;
            if (!capture.target || capture.selected || capture.distance < settings.nearPlanarReflectionDistance ||
                elapsed == 0 || cameraChanged || !capture.anchorGroup ||
                settings.distantPlanarReflectionUpdateSeconds == 0) {
                planarRefresh[i] = 1; // New faces and nearby detail never wait behind distant work.
            } else if (time >= capture.nextUpdate) {
                distantRefreshOrder.push_back(i);
            }
        }
        std::sort(distantRefreshOrder.begin(), distantRefreshOrder.end(), [&](auto a, auto b) {
            const auto first = planarCaptures[a].nextUpdate, second = planarCaptures[b].nextUpdate;
            return first == second ? a < b : first < second;
        });
        int updates = 0;
        for (auto index : distantRefreshOrder) {
            if (planarRefresh[index]) {
                continue;
            }
            const auto group = planarCaptures[index].anchorGroup;
            const int faces =
                static_cast<int>(std::count_if(distantRefreshOrder.begin(), distantRefreshOrder.end(),
                                               [&](auto i) { return planarCaptures[i].anchorGroup == group; }));
            // Oldest work wins; allow an entire building even if it exceeds the
            // budget alone, so its side and end faces cannot refresh separately.
            if (updates > 0 && settings.maxDistantPlanarUpdatesPerFrame > 0 &&
                updates + faces > settings.maxDistantPlanarUpdatesPerFrame) {
                continue;
            }
            for (auto i : distantRefreshOrder) {
                if (planarCaptures[i].anchorGroup == group) {
                    planarRefresh[i] = 1;
                }
            }
            updates += faces;
        }
    }
    void update(const Scene& scene, const ReflectionFrame& frame, RenderState& state) {
        views.clear();
        const auto& viewProjection = frame.viewProjection;
        const float time = std::isfinite(scene.time) ? scene.time : 0;
        const bool fresh = !previousPlanarTime || time < *previousPlanarTime || frame.mode != planarMode ||
                           frame.size.width != planarViewSize.width || frame.size.height != planarViewSize.height;
        const float elapsed = fresh ? 0 : std::clamp(time - *previousPlanarTime, 0.0f, .1f);
        previousPlanarTime = time;
        planarMode = frame.mode;
        planarViewSize = frame.size;
        if (fresh) {
            planarCaptures.clear();
        }
        planarCandidates.clear();
        if (!settings.worldReflectionMapSize || !settings.planarReflectionSize || !settings.maxPlanarReflections) {
            planarCaptures.clear();
            reflectionGroups.clear();
            return;
        }
        const float step =
            settings.planarReflectionBlendSeconds > 0 ? elapsed / settings.planarReflectionBlendSeconds : 1;
        prepareReflectionGroups(scene, step, fresh, time, viewProjection);
        for (const auto& object : scene.objects) {
            const auto* surface = std::get_if<Quad>(&object);
            if (!surface || !reflectiveFacade(*surface)) {
                continue;
            }
            const auto normal = normalize(surface->normal);
            const float offset = dot(normal, center(*surface));
            if (dot(normal, scene.camera.position) - offset <= .05f) {
                continue; // The facade must face the viewer.
            }
            const auto group = reflectionGroups.find(surface->reflectionGroupId);
            if (group != reflectionGroups.end() && group->second.occluded) {
                continue;
            }
            const float distance = group != reflectionGroups.end() ? group->second.distance
                                                                   : facadeDistance(*surface, scene.camera.position);
            if (distance > settings.planarReflectionDistance) {
                continue;
            }
            const auto bounds = facadeBounds(*surface, viewProjection);
            if (!bounds) {
                continue;
            }
            const float score = (bounds->right - bounds->left) * (bounds->top - bounds->bottom) *
                                (1 - std::clamp(surface->haze, 0.0f, 1.0f)) * (1 / (1 + distance * .07f));
            auto samePlane = std::find_if(planarCandidates.begin(), planarCandidates.end(), [&](const auto& plane) {
                return dot(normal, plane.normal) > .9999f && std::abs(offset - plane.offset) < .01f;
            });
            if (samePlane != planarCandidates.end()) {
                samePlane->score += score;
                samePlane->distance = std::min(samePlane->distance, distance);
                samePlane->bounds.left = std::min(samePlane->bounds.left, bounds->left);
                samePlane->bounds.bottom = std::min(samePlane->bounds.bottom, bounds->bottom);
                samePlane->bounds.right = std::max(samePlane->bounds.right, bounds->right);
                samePlane->bounds.top = std::max(samePlane->bounds.top, bounds->top);
            } else {
                PlanarCapture capture;
                capture.normal = normal;
                capture.offset = offset;
                capture.bounds = *bounds;
                capture.score = score;
                capture.distance = distance;
                capture.anchorGroup = surface->reflectionGroupId;
                planarCandidates.push_back(std::move(capture));
                samePlane = std::prev(planarCandidates.end());
            }
            if (surface->reflectionId) {
                samePlane->ids.push_back(surface->reflectionId);
            }
        }
        matchedCaptures.assign(planarCaptures.size(), 0);
        for (auto& candidate : planarCandidates) {
            for (std::size_t index = 0; index < planarCaptures.size(); ++index) {
                if (!matchedCaptures[index] && sameCapture(candidate, planarCaptures[index])) {
                    candidate.history = static_cast<int>(index);
                    matchedCaptures[index] = 1;
                    // Hysteresis only changes detail priority. All visible
                    // planes keep live captures, including narrow side faces.
                    if (planarCaptures[index].selected) {
                        candidate.score *= 1.2f;
                    }
                    break;
                }
            }
        }
        std::stable_sort(planarCandidates.begin(), planarCandidates.end(),
                         [](const auto& a, const auto& b) { return a.score > b.score; });
        const auto count = std::min(planarCandidates.size(), static_cast<std::size_t>(settings.maxPlanarReflections));
        for (std::size_t index = 0; index < planarCandidates.size(); ++index) {
            auto& candidate = planarCandidates[index];
            candidate.selected = index < count;
            const float detail =
                candidate.selected
                    ? 1
                    : 1 - smoothTransition(settings.nearPlanarReflectionDistance,
                                           settings.distantPlanarReflectionDetailDistance, candidate.distance);
            if (candidate.history >= 0) {
                const auto& previous = planarCaptures[candidate.history];
                candidate.slot = previous.slot;
                candidate.strength = std::min(previous.strength + step, 1.0f);
                candidate.detail = previous.detail + std::clamp(detail - previous.detail, -step, step);
                candidate.target = previous.target;
                candidate.textureScale = previous.textureScale;
                candidate.maxLod = previous.maxLod;
                candidate.nextUpdate = previous.nextUpdate;
                if (previous.anchorGroup && reflectionGroups.contains(previous.anchorGroup)) {
                    candidate.anchorGroup = previous.anchorGroup;
                } else if (previous.anchorGroup != candidate.anchorGroup) {
                    candidate.target = nullptr; // A shared plane's old anchor left the scene.
                }
                candidate.anchorPosition = previous.anchorPosition;
                candidate.capturedProjection = previous.capturedProjection;
                candidate.capturedCamera = previous.capturedCamera;
            } else {
                // Stateless reusable scenes remain immediately renderable. The
                // game supplies IDs and time so newly arriving facades ease in.
                candidate.strength = fresh || time == 0 ? 1 : std::min(step, 1.0f);
                candidate.detail = detail;
            }
        }
        // Reuse allocations by facade identity, independently of ranking.
        // The detail budget never removes a visible face's reflection.
        usedTargetSlots.assign(std::max(planarTargets.size(), planarCandidates.size()), 0);
        for (const auto& capture : planarCandidates) {
            if (capture.history >= 0) {
                usedTargetSlots[capture.slot] = true;
            }
        }
        for (auto& capture : planarCandidates) {
            if (capture.history < 0) {
                capture.slot = static_cast<std::size_t>(std::find(usedTargetSlots.begin(), usedTargetSlots.end(), 0) -
                                                        usedTargetSlots.begin());
                usedTargetSlots[capture.slot] = true;
            }
        }
        planarCaptures.swap(planarCandidates);
        if (planarCaptures.empty()) {
            return;
        }
        const auto savedSize = frame.size;
        const float aspect = static_cast<float>(savedSize.width) / savedSize.height;
        views.reserve(planarCaptures.size());
        preparePlanarUpdates(scene, elapsed, time);
        for (std::size_t index = 0; index < planarCaptures.size(); ++index) {
            auto& capture = planarCaptures[index];
            const auto anchor = reflectionGroups.find(capture.anchorGroup);
            const Vec3 position = anchor != reflectionGroups.end()
                                      ? Vec3{(anchor->second.minimum.x + anchor->second.maximum.x) * .5f,
                                             (anchor->second.minimum.y + anchor->second.maximum.y) * .5f,
                                             (anchor->second.minimum.z + anchor->second.maximum.z) * .5f}
                                      : Vec3{capture.normal.x * capture.offset, capture.normal.y * capture.offset,
                                             capture.normal.z * capture.offset};
            // Cached distant views keep their image attached to scrolling
            // geometry. New faces are captured immediately, never queued.
            if (!planarRefresh[index]) {
                capture.viewProjection =
                    multiply(capture.capturedProjection, translation(capture.anchorPosition - position));
                continue;
            }
            capture.anchorPosition = position;
            capture.capturedCamera = scene.camera;
            const float interval = settings.distantPlanarReflectionUpdateSeconds;
            const std::uint64_t id = capture.anchorGroup   ? capture.anchorGroup
                                     : capture.ids.empty() ? capture.slot
                                                           : capture.ids.front();
            const auto hash = (id ^ (id >> 32)) * std::uint64_t{0x9e3779b97f4a7c15};
            const float phase = static_cast<float>((hash >> 32) & 65535) / 65536;
            capture.nextUpdate = interval > 0 ? (std::floor(time / interval - phase) + 1 + phase) * interval : time;
            const float resolution = std::lerp(
                static_cast<float>(std::min(settings.distantPlanarReflectionSize, settings.planarReflectionSize)),
                static_cast<float>(settings.planarReflectionSize), capture.detail);
            const float density = std::min(1.0f, resolution / std::max(savedSize.width, savedSize.height));
            // Padding retains facade edges under subpixel motion and leaves
            // room for glass blur without sampling outside the cropped view.
            const float padX = 4 / (savedSize.width * density), padY = 4 / (savedSize.height * density);
            capture.bounds.left = std::max(-1.0f, capture.bounds.left - padX);
            capture.bounds.right = std::min(1.0f, capture.bounds.right + padX);
            capture.bounds.bottom = std::max(-1.0f, capture.bounds.bottom - padY);
            capture.bounds.top = std::min(1.0f, capture.bounds.top + padY);
            // Mirroring reverses handedness. lookAt's right-handed basis flips
            // screen X, so reflect the visible facade rectangle before cropping.
            PlanarReflectionView view;
            view.skyBounds = {-capture.bounds.right, capture.bounds.bottom, -capture.bounds.left, capture.bounds.top};
            view.aspect = aspect;
            const float width = view.skyBounds.right - view.skyBounds.left;
            const float height = view.skyBounds.top - view.skyBounds.bottom;
            view.size = {std::max(1, static_cast<int>(std::ceil(width * .5f * savedSize.width * density))),
                         std::max(1, static_cast<int>(std::ceil(height * .5f * savedSize.height * density)))};
            // Match the raster density while keeping persistent allocations.
            // Targets grow in power-of-two buckets and never shrink/reorder.
            view.size.width = (view.size.width + 31) / 32 * 32;
            view.size.height = (view.size.height + 31) / 32 * 32;
            if (capture.slot >= planarTargets.size()) {
                planarTargets.resize(capture.slot + 1);
            }
            auto& target = planarTargets[capture.slot];
            if (!target || target->size().width < view.size.width || target->size().height < view.size.height) {
                const Size capacitySize = {
                    std::max(target ? target->size().width : 64,
                             static_cast<int>(std::bit_ceil(static_cast<unsigned int>(view.size.width)))),
                    std::max(target ? target->size().height : 64,
                             static_cast<int>(std::bit_ceil(static_cast<unsigned int>(view.size.height))))};
                target = std::make_unique<RenderTarget>(capacitySize, RenderTarget::Kind::Reflection);
                state.forgetTextureBinding(GL_TEXTURE_2D);
            }
            capture.target = target.get();
            capture.textureScale = {static_cast<float>(view.size.width) / target->size().width,
                                    static_cast<float>(view.size.height) / target->size().height};
            // Keep blurred texel footprints inside both populated dimensions,
            // even when a thin crop reuses a previously much larger target.
            capture.maxLod = std::bit_width(static_cast<unsigned int>(std::min(view.size.width, view.size.height))) - 1;
            view.camera = scene.camera;
            view.camera.position = mirrorPoint(scene.camera.position, capture.normal, capture.offset);
            view.camera.target = mirrorPoint(scene.camera.target, capture.normal, capture.offset);
            view.camera.up = mirrorPoint(scene.camera.up, capture.normal, 0);
            view.camera.nearPlane = .1f;
            Mat4 crop = identity();
            crop.values[0] = 2 / width;
            crop.values[5] = 2 / height;
            crop.values[12] = (capture.bounds.left + capture.bounds.right) / width;
            crop.values[13] = -(capture.bounds.top + capture.bounds.bottom) / height;
            // Sampling and rendering must use this same mirrored, cropped
            // projection so objects stay attached to their correct facade positions.
            view.viewProjection = multiply(crop, view.camera.viewProjection(aspect));
            capture.viewProjection = view.viewProjection;
            capture.capturedProjection = view.viewProjection;
            view.clipPlane = {capture.normal.x, capture.normal.y, capture.normal.z, -capture.offset - .02f};
            view.target = capture.target;
            views.push_back(view);
        }
    }
    std::optional<PlanarReflectionBinding> binding(const Quad& surface, Vec3 cameraPosition) const {
        if (!settings.worldReflectionMapSize || !surface.windowMaterial.worldReflections) {
            return {};
        }
        const auto group = reflectionGroups.find(surface.reflectionGroupId);
        const float distance =
            group != reflectionGroups.end() ? group->second.distance : facadeDistance(surface, cameraPosition);
        if (distance > settings.planarReflectionDistance ||
            (group != reflectionGroups.end() && group->second.occluded)) {
            return {};
        }
        const auto normal = normalize(surface.normal);
        const float offset = dot(normal, center(surface));
        for (const auto& capture : planarCaptures) {
            if (capture.target && dot(normal, capture.normal) > .9999f && std::abs(offset - capture.offset) < .01f) {
                float strength = group != reflectionGroups.end() ? group->second.strength : capture.strength;
                if (settings.planarReflectionFadeDistance > 0) {
                    const float fadeStart =
                        std::max(0.0f, settings.planarReflectionDistance - settings.planarReflectionFadeDistance);
                    strength *= 1 - smoothTransition(fadeStart, settings.planarReflectionDistance, distance);
                }
                return PlanarReflectionBinding{capture.target->texture(), capture.viewProjection, capture.textureScale,
                                               capture.maxLod, strength};
            }
        }
        return {};
    }
};
PlanarReflectionPass::PlanarReflectionPass(const RenderSettings& settings) : impl_(std::make_unique<Impl>(settings)) {}
PlanarReflectionPass::~PlanarReflectionPass() = default;
std::span<const PlanarReflectionView> PlanarReflectionPass::update(const Scene& scene, const ReflectionFrame& frame,
                                                                   RenderState& state) {
    impl_->update(scene, frame, state);
    return impl_->views;
}
std::optional<PlanarReflectionBinding> PlanarReflectionPass::binding(const Quad& surface,
                                                                     math::Vec3 cameraPosition) const {
    return impl_->binding(surface, cameraPosition);
}
} // namespace engine::graphics::detail
