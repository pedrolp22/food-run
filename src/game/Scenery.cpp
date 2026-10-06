#include "game/SceneryLayout.h"
#include "game/WorldScene.h"
#include <algorithm>
#include <cmath>

namespace game {
using namespace engine::math;
using namespace engine::graphics;
using namespace GameConfig;
using namespace scenery;

void WorldScene::addGrassPatch(float side, float innerX, float startZ, int columns, int rows, unsigned int seed,
                               bool notched, bool fenced, bool entrance, std::optional<Vec2> buildingWall,
                               const GrassLayout* layout) {
    // An attached garden shares the building's straight facade, rather than curving away from it with the road.
    const float anchorOffset = buildingWall ? track_.offset(buildingWall->y) : 0;
    const float width = buildingWall ? side * (buildingWall->x - anchorOffset) - innerX : columns * kGrassTileSize;
    const auto columnDistance = [&](float column) { return std::min(column * kGrassTileSize, width); };
    const auto choose = [&](unsigned int index, int low, int high) {
        return low + std::min(high - low, static_cast<int>(hashFloat(seed, index) * (high - low + 1)));
    };
    const int corner = choose(6, 0, 3);
    const int notchRows = rows >= 5 ? choose(7, 1, 2) : 1;
    const int notchColumns = columns >= 5 ? choose(14, 1, 2) : 1;
    const auto occupied = [&](int column, int row) {
        if (layout) {
            return layout->occupied(column, row);
        }
        if (column < 0 || column >= columns || row < 0 || row >= rows) {
            return false;
        }
        const bool cornerColumn = corner % 2 ? column >= columns - notchColumns : column < notchColumns;
        const bool cornerRow = corner < 2 ? row < notchRows : row >= rows - notchRows;
        return !(notched && cornerColumn && cornerRow);
    };
    int entryRow = rows > 2 ? choose(8, 1, rows - 2) : rows / 2;
    if (!occupied(0, entryRow)) {
        entryRow = rows / 2;
    }
    const Vec2 grassOffset = layout ? layout->textureOffset : Vec2{hashFloat(seed, 9), hashFloat(seed, 10)};
    const float tint = std::lerp(.88f, 1.0f, hashFloat(seed, 11));
    const auto groundPoint = [&](float column, float row) {
        const float z = startZ + row * kGrassTileSize;
        return Vec3{side * (innerX + columnDistance(column)) + (buildingWall ? anchorOffset : track_.offset(z)),
                    sidewalkTopY - track_.drop(z), z};
    };
    const auto fence = [&](float c0, float r0, float c1, float r1, float uvOffset) {
        const auto a = groundPoint(c0, r0), b = groundPoint(c1, r1);
        const auto delta = b - a;
        Quad command;
        command.model.values[0] = delta.x;
        command.model.values[1] = delta.y;
        command.model.values[2] = delta.z;
        command.model.values[5] = kParkFenceHeight;
        command.normal = normalize(cross(delta, {0, 1, 0}));
        command.model.values[8] = command.normal.x;
        command.model.values[10] = command.normal.z;
        command.model.values[12] = (a.x + b.x) * .5f;
        command.model.values[13] = (a.y + b.y + kParkFenceHeight) * .5f;
        command.model.values[14] = (a.z + b.z) * .5f;
        command.texture = &assets_.texture(TextureId::Fence);
        command.materialTextures = assets_.materialTextures(TextureId::Fence);
        const float length = c0 == c1 ? (r1 - r0) * kGrassTileSize : std::abs(columnDistance(c1) - columnDistance(c0));
        const float repeat = length / (kParkFenceHeight * command.texture->aspect());
        command.textureScale = {repeat, 1};
        const float origin = layout && c0 == c1 ? layout->textureOffset.y : 0;
        command.textureOffset = {(uvOffset + origin) * kGrassTileSize / (kParkFenceHeight * command.texture->aspect()),
                                 0};
        command.worldSize = {length, kParkFenceHeight};
        command.alphaCutoff = .5f;
        command.doubleSided = command.pbrSurface = true;
        command.surfaceMaterial = {1, 1, 1}; // The map separates dark paint from exposed metal highlights.
        scene_.objects.emplace_back(command);
    };
    for (int column = 0; column < columns; ++column) {
        for (int row = 0; row < rows; ++row) {
            if (!occupied(column, row)) {
                continue;
            }
            const float tileWidth = columnDistance(column + 1) - columnDistance(column);
            const auto p = groundPoint(column + tileWidth / (2 * kGrassTileSize), row + .5f);
            auto& tile =
                addTexturedQuad(assets_.texture(TextureId::Grass), {p.x, p.y, p.z, tileWidth, kGrassTileSize, 1},
                                Face::Top, tileWidth / kGrassTileSize, 1, {0, 1, 0});
            track_.bendSegment(tile.model, p.z, kGrassTileSize, 1);
            if (buildingWall) {
                tile.model.values[4] = 0;
                tile.model.values[12] = p.x;
            }
            tile.normal = track_.normal(p.z, kGrassTileSize);
            // Left-side beds grow opposite to quad U; crop the final tile from the matching end to retain its seam.
            tile.textureOffset = {grassOffset.x + column + (side < 0 ? 1 - tileWidth / kGrassTileSize : 0),
                                  grassOffset.y + row};
            tile.color = {tint, tint, tint};
            tile.pbrSurface = true;
            tile.surfaceMaterial = {1, 0, 1}; // Rough tufts and recessed dirt use their generated per-texel finish.
            tile.castsShadow = false;
            if (!fenced) {
                continue;
            }
            const bool openEntry =
                layout ? std::find(layout->entrances.begin(), layout->entrances.end(), row) != layout->entrances.end()
                       : entrance && row == entryRow;
            if (!occupied(column - 1, row) && !(column == 0 && openEntry)) {
                fence(column, row, column, row + 1, row);
            }
            if (!occupied(column + 1, row) && !(buildingWall && column == columns - 1)) {
                fence(column + 1, row, column + 1, row + 1, row);
            }
            if (!occupied(column, row - 1)) {
                fence(column, row, column + 1, row, column);
            }
            if (!occupied(column, row + 1)) {
                fence(column, row + 1, column + 1, row + 1, column);
            }
        }
    }
}

void WorldScene::addBrickGarden(float side, float facadeX, float centerZ, float depth, unsigned int seed) {
    static_assert(kGardenDoorPathWidth >= kBrickFacadeTileWidth + .4f);
    static_assert(kBrickGardenChance >= 0 && kBrickGardenChance <= 1);
    static_assert(kGardenFenceChance >= 0 && kGardenFenceChance <= 1);
    const float innerX = sidewalkHalfWidth + .2f;
    const float width = facadeX - innerX;
    const int columns = static_cast<int>(std::ceil(width / kGrassTileSize));
    const int rows = static_cast<int>((depth - kGardenDoorPathWidth - .6f) * .5f / kGrassTileSize);
    if (width < kGrassTileSize || rows < 1 || hashFloat(seed, 20) >= kBrickGardenChance) {
        return; // Close entrances have no forecourt; leave their sidewalk access clear.
    }
    const bool fenced = hashFloat(seed, 21) < kGardenFenceChance;
    const float length = rows * kGrassTileSize;
    const float pathHalf = kGardenDoorPathWidth * .5f;
    const Vec2 wall = {side * facadeX + track_.offset(centerZ), centerZ};
    addGrassPatch(side, innerX, centerZ - pathHalf - length, columns, rows, hashSeed(seed + 101), false, fenced, false,
                  wall);
    addGrassPatch(side, innerX, centerZ + pathHalf, columns, rows, hashSeed(seed + 103), false, fenced, false, wall);
}

void WorldScene::addParks(float roadScroll) {
    static_assert(kPocketParkChance >= 0 && kPocketParkChance <= 1);
    static_assert(kGrassTileSize > 0 && kParkFenceHeight > 0 && kParkBuildingCells >= 2 && kParkBuildingCells <= 3);
    static_assert(kParkMinWidthTiles >= 2 && kParkMaxWidthTiles >= kParkMinWidthTiles);
    static_assert(kParkMinLengthTiles >= 3 && kParkMaxLengthTiles >= kParkMinLengthTiles);
    static_assert(kParkNotchChance >= 0 && kParkNotchChance <= 1);
    static_assert(kParkTreeMinHeight > 0 && kParkTreeMaxHeight >= kParkTreeMinHeight);
    constexpr float rearEdge = buildingCellStartX + kParkBuildingCells * buildingCellWidth;
    constexpr int maxColumns =
        std::min(kParkMaxWidthTiles, static_cast<int>((rearEdge - sidewalkHalfWidth - .8f) / kGrassTileSize));
    constexpr int maxRows = std::min(kParkMaxLengthTiles, static_cast<int>((buildingSpacing - 2) / kGrassTileSize));
    static_assert(maxColumns >= 2 && maxRows >= 3, "Grass modules must fit the reserved park lots");
    constexpr int minColumns = std::min(kParkMinWidthTiles, maxColumns);
    constexpr int minRows = std::min(kParkMinLengthTiles, maxRows);
    const int first = static_cast<int>(std::floor((roadScroll - 12) / buildingSpacing));
    const int last = static_cast<int>(std::ceil((roadScroll + 100) / buildingSpacing));
    struct ParkLot {
        bool present = false;
        unsigned int seed = 0;
        int columns = 0, rows = 0, originalStart = 0, start = 0, end = 0;
        int corner = 0, notchColumns = 0, notchRows = 0;
        bool notched = false;
    };
    const auto choose = [](unsigned int seed, unsigned int index, int low, int high) {
        return low + std::min(high - low, static_cast<int>(hashFloat(seed, index) * (high - low + 1)));
    };
    for (float side : {-1.0f, 1.0f}) {
        // Neighbor lots outside the draw range still determine the border.
        // Scrolling a joined park into view must not add a fence through it.
        std::vector<ParkLot> lots(static_cast<std::size_t>(last - first + 3));
        int firstRow = 0, lastRow = 0;
        bool hasGrass = false;
        for (int lot = first - 1; lot <= last + 1; ++lot) {
            const auto lotSeed = streetLotSeed(lot, side);
            if (!hasPocketPark(lotSeed)) {
                continue;
            }
            auto& plot = lots[lot - first + 1];
            plot.present = true;
            plot.seed = parkSeed(lotSeed);
            const auto seed = plot.seed;
            plot.columns = choose(seed, 1, minColumns, maxColumns);
            plot.rows = choose(seed, 2, minRows, maxRows);
            const float length = plot.rows * kGrassTileSize;
            const float zJitter = (hashFloat(seed, 4) - .5f) * (buildingSpacing - length - 2);
            plot.originalStart =
                static_cast<int>(std::round((-lot * buildingSpacing + zJitter - length * .5f) / kGrassTileSize));
            const bool farNeighbor = hasPocketPark(streetLotSeed(lot + 1, side));
            const bool nearNeighbor = hasPocketPark(streetLotSeed(lot - 1, side));
            plot.start = farNeighbor ? static_cast<int>(std::round(-(lot + .5f) * buildingSpacing / kGrassTileSize))
                                     : plot.originalStart;
            plot.end = nearNeighbor ? static_cast<int>(std::round(-(lot - .5f) * buildingSpacing / kGrassTileSize))
                                    : plot.originalStart + plot.rows;
            plot.corner = choose(seed, 6, 0, 3);
            plot.notchRows = plot.rows >= 5 ? choose(seed, 7, 1, 2) : 1;
            plot.notchColumns = plot.columns >= 5 ? choose(seed, 14, 1, 2) : 1;
            plot.notched = hashFloat(seed, 5) < kParkNotchChance && !(plot.corner < 2 ? farNeighbor : nearNeighbor);
            if (lot >= first && lot <= last) {
                firstRow = hasGrass ? std::min(firstRow, plot.start) : plot.start;
                lastRow = hasGrass ? std::max(lastRow, plot.end) : plot.end;
                hasGrass = true;
            }
        }
        if (!hasGrass) {
            continue;
        }
        GrassLayout layout;
        layout.occupied = [&](int column, int row) {
            if (column < 0 || column >= maxColumns) {
                return false;
            }
            const int globalRow = firstRow + row;
            const int lot = static_cast<int>(std::round(-(globalRow + .5f) * kGrassTileSize / buildingSpacing));
            if (lot < first - 1 || lot > last + 1) {
                return false;
            }
            const auto& plot = lots[lot - first + 1];
            if (!plot.present || column >= plot.columns || globalRow < plot.start || globalRow >= plot.end) {
                return false;
            }
            const bool cornerColumn =
                plot.corner % 2 ? column >= plot.columns - plot.notchColumns : column < plot.notchColumns;
            const bool cornerRow =
                plot.corner < 2 ? globalRow < plot.start + plot.notchRows : globalRow >= plot.end - plot.notchRows;
            return !(plot.notched && cornerColumn && cornerRow);
        };
        for (int lot = first; lot <= last; ++lot) {
            const auto& plot = lots[lot - first + 1];
            if (!plot.present) {
                continue;
            }
            int entry = plot.originalStart + choose(plot.seed, 8, 1, plot.rows - 2) - firstRow;
            if (!layout.occupied(0, entry)) {
                entry = plot.originalStart + plot.rows / 2 - firstRow;
            }
            layout.entrances.push_back(entry);
        }
        // One UV grid and tint keep the connected lawn continuous across lot
        // boundaries; widths and outer corner notches still vary by lot.
        const auto areaSeed = parkSeed(streetLotSeed(0, side));
        const float innerX = sidewalkHalfWidth + std::lerp(.2f, .6f, hashFloat(areaSeed, 3));
        layout.textureOffset = {hashFloat(areaSeed, 9), hashFloat(areaSeed, 10) + firstRow};
        addGrassPatch(side, innerX, roadScroll + firstRow * kGrassTileSize, maxColumns, lastRow - firstRow, areaSeed,
                      false, true, false, std::nullopt, &layout);
        for (int lot = first; lot <= last; ++lot) {
            const auto& plot = lots[lot - first + 1];
            if (!plot.present) {
                continue;
            }
            const auto seed = plot.seed;
            const int columns = plot.columns, rows = plot.rows;
            const float width = columns * kGrassTileSize;
            const float startZ = roadScroll + plot.originalStart * kGrassTileSize;
            const float lotZ = roadScroll - lot * buildingSpacing;
            const int trees = 1 + (rows >= 5 && hashFloat(seed, 12) < .65f) + (rows >= 7 && hashFloat(seed, 13) < .3f);
            for (int tree = 0; tree < trees; ++tree) {
                const auto treeSeed = hashSeed(seed + tree * 293u);
                const float column = columns * (.5f + (hashFloat(treeSeed, 0) - .5f) * .1f);
                const float row = 1 + (rows - 2) * (tree + .5f) / trees + (hashFloat(treeSeed, 1) - .5f) * .5f;
                const float z = startZ + row * kGrassTileSize;
                const float localX = innerX + column * kGrassTileSize;
                const Vec3 p = {side * localX + track_.offset(z), sidewalkTopY - track_.drop(z), z};
                // The tree's rotated canopy radius is under half its height. Keep neighboring facades clear.
                const float maxHeight = std::min({kParkTreeMaxHeight, width + .8f, (rearEdge - localX - .2f) * 2,
                                                  (buildingSpacing * .5f + .2f - std::abs(z - lotZ)) * 2});
                const float height =
                    std::lerp(std::min(kParkTreeMinHeight, maxHeight), maxHeight, hashFloat(treeSeed, 2));
                const float angle = hashFloat(treeSeed, 3) * 2 * kPi;
                ModelInstance command{
                    multiply(translation(p), multiply(rotateY(angle), scale({height, height, height}))),
                    &assets_.model(ModelId::Tree)};
                command.pbrSurface = true;
                scene_.objects.emplace_back(command);
                const auto id = (std::uint64_t(static_cast<std::uint32_t>(lot)) << 32) |
                                (std::uint64_t(side > 0) << 31) | (std::uint64_t(tree) << 16);
                trees_.push_back({id, p, height, treeSeed});
            }
        }
    }
}
} // namespace game
