"""Build linear material data from the game's existing texture palettes.

Run with Python 3 and Pillow. The checked-in PNGs are runtime assets; Python is
not a build dependency. R=occlusion, G=roughness multiplier, B=metallic multiplier,
A=reflectivity multiplier. Streets use roughness/reflectivity factors of one and
zero metallic. Road and sidewalk relief also emits inspectable height maps. Facades
emit an R-channel glass mask and use factors of one for all mapped properties.
"""

import argparse
from collections import deque
from math import isfinite, sqrt
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1] / "assets"


def smoothstep(low, high, value):
    t = max(0.0, min(1.0, (value - low) / (high - low)))
    return t * t * (3.0 - 2.0 * t)


def encode(value):
    return round(max(0.0, min(1.0, value)) * 255)


def noise(x, y, seed=0):
    """Stable offline grain: never dependent on Python's randomized hash or time."""
    value = (x * 73856093 ^ y * 19349663 ^ seed * 83492791) & 0xffffffff
    value = ((value ^ (value >> 16)) * 0x7feb352d) & 0xffffffff
    value = ((value ^ (value >> 15)) * 0x846ca68b) & 0xffffffff
    return (value ^ (value >> 16)) / 0xffffffff


def facade_data(image, seed):
    """Use the source pane shapes to author local finishes, recesses and wear."""
    width, height = image.size
    pixels = list(image.get_flattened_data() if hasattr(image, "get_flattened_data") else image.getdata())
    glass = [smoothstep(0.12, 0.22, (b - max(r, g)) / 255) for r, g, b in pixels]
    labels = [-1] * len(pixels)
    panes = []
    for start, mask in enumerate(glass):
        if mask < 0.5 or labels[start] >= 0:
            continue
        label = len(panes)
        labels[start] = label
        queue = deque([start])
        left = right = start % width
        top = bottom = start // width
        while queue:
            index = queue.popleft()
            x, y = index % width, index // width
            left, right = min(left, x), max(right, x)
            top, bottom = min(top, y), max(bottom, y)
            for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if 0 <= nx < width and 0 <= ny < height:
                    neighbor = ny * width + nx
                    if glass[neighbor] >= 0.5 and labels[neighbor] < 0:
                        labels[neighbor] = label
                        queue.append(neighbor)
        panes.append((left, top, right, bottom))

    surface, heights = [], []
    for index, (r, g, b) in enumerate(pixels):
        x, y = index % width, index // width
        grain = noise(x, y, seed)
        if labels[index] >= 0:
            left, top, right, bottom = panes[labels[index]]
            u = 2 * (x - left + 0.5) / (right - left + 1) - 1
            v = 2 * (y - top + 0.5) / (bottom - top + 1) - 1
            edge = 1 - smoothstep(0, 2.5, min(x - left, right - x, y - top, bottom - y))
            weather = noise(left, top, seed + 31)
            light = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255
            # Low-frequency finish differences give identical blue panes their
            # own light response without baked stripes or noisy specular flecks.
            roughness = 0.20 + 0.09 * weather + 0.035 * edge + 0.015 * (u * u + v * v)
            reflection = 0.98 - 0.12 * weather - 0.07 * edge - 0.04 * (1 - light)
            properties = (1 - 0.035 * edge, roughness, 0, reflection)
            tilt_x = (noise(left, top, seed + 47) - 0.5) * 0.012
            tilt_y = (noise(left, top, seed + 59) - 0.5) * 0.012
            height_value = -0.025 - 0.009 * (1 - u * u) * (1 - v * v) + tilt_x * u + tilt_y * v
        else:
            # Gray dividers are rough masonry, with restrained, pixel-aligned wear.
            weather = noise(x // 8, y // 8, seed + 73)
            properties = (0.93 + 0.04 * grain, 0.80 + 0.10 * weather + 0.025 * grain,
                          0, 0.48 + 0.12 * (1 - weather))
            height_value = 0.055 + 0.004 * (grain - 0.5)
        surface.append(tuple(encode(value) for value in properties))
        heights.append(height_value)
    return surface, heights, [encode(mask) for mask in glass]


def surface_pixel(rgb, profile):
    r, g, b = (value / 255 for value in rgb)
    light = 0.2126 * r + 0.7152 * g + 0.0722 * b
    if profile == "road":
        white = min(r, g, b) > 0.62
        yellow = r > 0.4 and g > 0.2 and r > b * 1.5 and g > b * 1.3
        if white or yellow:
            # Thin, weathered road paint is smoother than the surrounding asphalt.
            return (1, 0.40 + 0.16 * (1 - light), 1, 0.90 + 0.10 * light), 0.10 + 0.014 * (light - 0.5)
        grain = smoothstep(0.08, 0.20, light)
        # The dominant tar/blue-black/gray palette falls below the old grain
        # threshold. Keep pits and raised aggregate distinct even in dark texels;
        # the paint's brightness must not become a towering ridge in the relief.
        relief = 0.025 + 0.24 * sqrt(smoothstep(0.0, 0.22, light))
        return (0.92 + 0.08 * grain, 0.97 - 0.27 * grain, 1, 0.40 + 0.46 * grain), relief
    if profile == "concrete":
        grain = smoothstep(0.62, 0.77, light)
        dirt = 1 - smoothstep(0.18, 0.43, light)
        roughness = (0.94 - 0.05 * light) * (1 - grain) + 0.63 * grain
        roughness = roughness * (1 - dirt) + 0.99 * dirt
        reflection = (0.48 + 0.20 * light) * (1 - grain) + 0.92 * grain
        reflection = reflection * (1 - dirt) + 0.30 * dirt
        # Dark diagonal slab joints are recessed; brighter stone grains stand
        # above the cement. Retain intermediate cement variation, too.
        relief = 0.13 + 0.12 * grain - 0.13 * dirt + 0.04 * (light - 0.5)
        return (1 - 0.16 * dirt, roughness, 1, reflection), relief
    if profile == "masonry":
        saturation = max(r, g, b) - min(r, g, b)
        mortar = (1 - smoothstep(0.08, 0.22, saturation)) * smoothstep(0.55, 0.85, light)
        roughness = (0.94 - 0.20 * saturation * light) * (1 - mortar) + 0.98 * mortar
        reflection = (0.48 + 0.35 * saturation) * (1 - mortar) + 0.32 * mortar
        return (0.92 + 0.08 * smoothstep(0.12, 0.35, light), roughness, 1, reflection), 0.07 + 0.06 * light
    if profile == "roof":
        dirt = 1 - smoothstep(0.18, 0.44, light)
        return (1 - 0.12 * dirt, 0.90 + 0.09 * dirt, 1, 0.58 - 0.28 * dirt), 0.09 - 0.05 * dirt
    # Preserve each prop's authored metallic/roughness factors. Bright grain or
    # intact coatings can be shinier; dark wear reduces the reflected environment.
    strength = {"wood": 0.12, "metal": 0.18, "coated": 0.28}[profile]
    return (0.94 + 0.06 * light, 1 - strength * light, 1, 0.60 + 0.40 * light), 0


def build(source, destination, profile, normal=False, max_dimension=None, repeat=True, seed=0, normal_strength=None):
    image = Image.open(source).convert("RGBA").convert("RGB")
    if max_dimension:
        image.thumbnail((max_dimension, max_dimension), Image.Resampling.BOX)
    width, height = image.size
    if profile == "skyscraper":
        surface, heights, glass = facade_data(image, seed)
    else:
        surface, heights = [], []
        pixels = image.get_flattened_data() if hasattr(image, "get_flattened_data") else image.getdata()
        for rgb in pixels:
            properties, height_value = surface_pixel(rgb, profile)
            surface.append(tuple(encode(value) for value in properties))
            heights.append(height_value)
    destination.parent.mkdir(parents=True, exist_ok=True)
    output = Image.new("RGBA", image.size)
    output.putdata(surface)
    output.save(destination, compress_level=9)
    if normal:
        strength = normal_strength if normal_strength is not None else (3.0 if profile in ("road", "concrete") else 1.5)
        normals = []
        def height_at(x, y):
            if repeat:
                return heights[(y % height) * width + x % width]
            return heights[max(0, min(height - 1, y)) * width + max(0, min(width - 1, x))]
        for y in range(height):
            for x in range(width):
                left = height_at(x - 1, y)
                right = height_at(x + 1, y)
                top = height_at(x, y - 1)
                bottom = height_at(x, y + 1)
                # Source images are flipped vertically by the game loader;
                # tangent +Y must therefore oppose image-down, as on the color map.
                nx, ny, nz = (left - right) * strength, (bottom - top) * strength, 1.0
                magnitude = sqrt(nx * nx + ny * ny + nz * nz)
                normals.append(tuple(encode(component / magnitude * 0.5 + 0.5) for component in (nx, ny, nz)))
        output = Image.new("RGB", image.size)
        output.putdata(normals)
        output.save(destination.with_name(destination.name.replace("_surface", "_normal")), compress_level=9)
        if profile in ("road", "concrete"):
            output = Image.new("L", image.size)
            output.putdata([encode(value / 0.32) for value in heights])
            output.save(destination.with_name(destination.name.replace("_surface", "_height")), compress_level=9)
    if profile == "skyscraper":
        output = Image.new("L", image.size)
        output.putdata(glass)
        output.save(destination.with_name(destination.name.replace("_surface", "_glass")), compress_level=9)
    print(destination.relative_to(ROOT) if destination.is_relative_to(ROOT) else destination, image.size, profile)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--texture", type=Path, help="Generate maps for one source texture instead of the game catalog.")
    parser.add_argument("--profile", choices=["road", "concrete", "masonry", "roof", "skyscraper", "wood", "metal", "coated"])
    parser.add_argument("--output-dir", type=Path, help="Override the catalog's map destinations.")
    parser.add_argument("--seed", type=int, default=0, help="Deterministic facade wear/pane variation (default: 0).")
    parser.add_argument("--normal-strength", type=float, help="Override relief strength (streets: 3.0, other surfaces: 1.5).")
    args = parser.parse_args()
    if args.normal_strength is not None and (not isfinite(args.normal_strength) or args.normal_strength < 0):
        parser.error("--normal-strength must be finite and nonnegative")
    if args.texture:
        if not args.profile:
            parser.error("--texture requires --profile")
        prop = args.profile in ("wood", "metal", "coated")
        build(args.texture, (args.output_dir or ROOT / "materials") / (args.texture.stem + "_surface.png"), args.profile,
              normal=not prop, max_dimension=256 if prop else None,
              repeat=args.profile != "skyscraper", seed=args.seed, normal_strength=args.normal_strength)
        return
    if args.profile:
        parser.error("--profile requires --texture")
    for filename, profile in [("road", "road"), ("sidewalk", "concrete"),
                              ("wall_brick", "masonry"), ("building_top", "roof")]:
        build(ROOT / (filename + ".png"), (args.output_dir or ROOT / "materials") / (filename + "_surface.png"), profile,
              normal=True, normal_strength=args.normal_strength)
    build(ROOT / "skyscraper.png", (args.output_dir or ROOT / "materials") / "skyscraper_surface.png", "skyscraper",
          normal=True, repeat=False, seed=args.seed, normal_strength=args.normal_strength)
    for model, filename, profile in [
        ("bench", "Material.001_baseColor", "wood"),
        ("bench", "Material.002_baseColor", "metal"),
        ("trash_can", "RGB_texture_baseColor", "coated"),
    ]:
        folder = ROOT / "models" / model / "textures"
        build(folder / (filename + ".png"), (args.output_dir or folder) / (filename + "_surface.png"), profile, max_dimension=256)


if __name__ == "__main__":
    main()
