// Cheap world-direction sky shared by the background and window reflections.
uniform bool uHasCloudSky;
uniform sampler2D uCloudTextures[3];
// Width/spacing in world turns, elevation/height in radians.
uniform vec4 uCloudLayouts[3];
// Wrapped horizontal offset and opacity; zero opacity disables a layer.
uniform vec2 uCloudSettings[3];
// Horizontal scatter, elevation scatter (radians), scale variation and concave bow.
uniform vec4 uCloudVariation[3];
uniform vec2 uCloudWarp[3]; // UV billowing strength and speed.
uniform vec3 uCloudSkyTop;
uniform vec3 uCloudSkyHorizon;
uniform vec3 uCloudTint;
uniform uint uSkyLayoutSeed;
uniform float uSkyEffectTime;
uniform vec4 uNightStars; // Density, angular radius, brightness, night visibility.
uniform vec2 uStarTwinkle; // Speed and strength.
uniform sampler2D uMoonTexture;
uniform vec3 uMoonDirection;
uniform vec2 uMoonSize; // Tangent-plane half-width and half-height.
uniform vec3 uMoonLight; // Night visibility, disc brightness, halo strength.

float authoredSkyRandom(uint key) {
    key ^= uSkyLayoutSeed * 747796405u;
    key = (key ^ (key >> 16u)) * 2246822519u;
    key = (key ^ (key >> 13u)) * 3266489917u;
    return float((key ^ (key >> 16u)) & 65535u) / 65535.0;
}

vec4 cloudSprite(sampler2D cloudTexture, vec2 direction, vec4 placement, vec2 settings,
                 vec4 variation, vec2 warp, uint layer) {
    if (settings.y <= 0.0) { return vec4(0.0); }
    float longitude = direction.x + settings.x;
    float slot = floor(longitude / placement.y);
    float slotCount = round(1.0 / placement.y);
    vec4 result = vec4(0.0);
    // Adjacent slots retain a whole cloud as wind carries it over a cell boundary.
    for (int neighbor = -1; neighbor <= 1; ++neighbor) {
        if ((slotCount == 1.0 && neighbor != 0) || (slotCount == 2.0 && neighbor == -1)) { continue; }
        float cell = slot + float(neighbor);
        uint key = uint(mod(cell, slotCount)) + layer * 613u;
        float scatter = (authoredSkyRandom(key + 13u) * 2.0 - 1.0) * variation.x;
        float delta = (longitude - (cell + 0.5 + scatter) * placement.y) * 6.2831853;
        delta = mod(delta + 3.14159265, 6.2831853) - 3.14159265;
        float scale = 1.0 + (authoredSkyRandom(key + 71u) * 2.0 - 1.0) * variation.z;
        float center = clamp(placement.z + (authoredSkyRandom(key + 37u) * 2.0 - 1.0) * variation.y,
                             0.025, 1.25);
        float halfWidth = clamp(placement.x * 3.14159265 * scale, 0.001, 1.3);
        float sinCenter = sin(center), cosCenter = cos(center);
        if (abs(delta) > halfWidth * 1.5 / max(cosCenter, 0.25)) { continue; }
        float halfHeight = clamp(placement.w * 0.5 * scale, 0.001, 1.3);
        float sinElevation = sin(direction.y), cosElevation = cos(direction.y);
        float facing = sinElevation * sinCenter + cosElevation * cosCenter * cos(delta);
        if (facing <= 0.05) { continue; }
        // Project the world ray onto each cloud's local sky tangent plane. The
        // curved dome keeps silhouettes distant as the camera turns or moves.
        vec2 local = vec2(cosElevation * sin(delta),
                          sinElevation * cosCenter - cosElevation * sinCenter * cos(delta)) / facing;
        vec2 uv = local / (2.0 * vec2(tan(halfWidth), tan(halfHeight))) + 0.5;
        if (any(greaterThan(abs(uv - 0.5), vec2(0.7)))) { continue; }
        float x = (uv.x - 0.5) * 2.0;
        float phase = authoredSkyRandom(key + 101u) * 6.2831853;
        float wind = uSkyEffectTime * warp.y + phase;
        uv.y -= variation.w * (x * x - 0.25); // A slight concave bowl instead of a flat strip.
        uv += warp.x * vec2(0.45 * sin(uv.y * 5.0 + wind),
                            sin(x * 3.4 + wind) * sin(3.14159265 * clamp(uv.x, 0.0, 1.0)));
        if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) { continue; }
        vec4 cloud = textureLod(cloudTexture, uv, 0.0);
        float opacity = cloud.a * settings.y;
        // Accumulate premultiplied color; transparent padding never becomes a rectangle.
        result.rgb = mix(result.rgb, cloud.rgb * uCloudTint, opacity);
        result.a += opacity * (1.0 - result.a);
    }
    return result;
}

vec3 authoredStarCoordinates(vec3 ray) {
    vec3 magnitude = abs(ray);
    if (magnitude.z >= magnitude.x && magnitude.z >= magnitude.y) {
        return vec3(ray.xy / magnitude.z, ray.z > 0.0 ? 0.0 : 1.0);
    }
    if (magnitude.x >= magnitude.y) {
        return vec3(ray.zy / magnitude.x, ray.x > 0.0 ? 2.0 : 3.0);
    }
    return vec3(ray.xz / magnitude.y, ray.y > 0.0 ? 4.0 : 5.0);
}

vec3 authoredStars(vec3 ray) {
    if (uNightStars.x <= 0.0 || uNightStars.w <= 0.0 || ray.y <= 0.035) { return vec3(0.0); }
    vec3 coordinates = authoredStarCoordinates(ray);
    vec2 grid = (coordinates.xy * 0.5 + 0.5) * 64.0;
    float footprint = max(length(dFdx(grid)), length(dFdy(grid)));
    uvec2 cell = uvec2(floor(grid));
    uint key = cell.x + cell.y * 67u + uint(coordinates.z) * 8191u + 65521u;
    if (authoredSkyRandom(key) >= uNightStars.x) { return vec3(0.0); }
    vec2 point = mix(vec2(0.25), vec2(0.75), vec2(authoredSkyRandom(key + 19u), authoredSkyRandom(key + 43u)));
    vec2 local = fract(grid) - point;
    float radius = max(uNightStars.y * 32.0, footprint * 0.45);
    float glow = 1.0 - smoothstep(radius * 0.25, radius, length(local));
    if (authoredSkyRandom(key + 59u) > 0.88) {
        vec2 spike = (1.0 - smoothstep(vec2(0.0), vec2(radius * 0.3), abs(local))) *
                     (1.0 - smoothstep(vec2(radius * 0.2), vec2(radius * 2.2), abs(local.yx)));
        glow = max(glow, max(spike.x, spike.y) * 0.5);
    }
    float phase = authoredSkyRandom(key + 83u) * 6.2831853;
    float pulse = 0.5 + 0.5 * sin(uSkyEffectTime * uStarTwinkle.x *
                                (0.7 + authoredSkyRandom(key + 97u) * 0.8) + phase);
    float twinkle = mix(1.0, 0.2 + 0.8 * pulse * pulse, uStarTwinkle.y);
    vec3 tint = mix(vec3(0.82, 0.9, 1.0), vec3(1.0, 0.88, 0.72), authoredSkyRandom(key + 127u));
    return tint * glow * twinkle * uNightStars.z * uNightStars.w * smoothstep(0.035, 0.2, ray.y);
}

vec3 authoredMoon(vec3 ray, vec3 sky) {
    if (uMoonLight.x <= 0.0) { return sky; }
    float facing = dot(ray, uMoonDirection);
    if (facing <= 0.0) { return sky; }
    vec3 up = abs(uMoonDirection.y) > 0.99 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(uMoonDirection, up));
    up = cross(right, uMoonDirection);
    // A plane perpendicular to the moon's bearing preserves the texture's
    // shape at every camera bearing and viewport aspect.
    vec2 disc = vec2(dot(ray, right), dot(ray, up)) / facing / uMoonSize;
    float radius = length(disc);
    float halo = exp(-max(radius - 0.9, 0.0) * 2.6) * (1.0 - smoothstep(2.0, 3.5, radius));
    sky = min(sky + vec3(0.78, 0.86, 1.0) * halo * uMoonLight.z * uMoonLight.x, vec3(1.0));
    vec2 uv = disc * 0.5 + 0.5;
    if (all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0)))) {
        vec4 texel = textureLod(uMoonTexture, uv, 0.0);
        vec3 glow = min(texel.rgb * uMoonLight.y + vec3(0.035, 0.04, 0.05), vec3(1.0));
        sky = mix(sky, glow, texel.a * uMoonLight.x);
    }
    return sky;
}

vec3 sampleCloudSky(vec3 ray) {
    float elevation = sqrt(clamp(ray.y, 0.0, 1.0));
    vec3 color = mix(uCloudSkyHorizon, uCloudSkyTop, smoothstep(0.0, 0.85, elevation));
    if (ray.y <= 0.0) { return color; }
    float longitude = dot(ray.xz, ray.xz) > 0.000001 ? atan(ray.x, -ray.z) : 0.0;
    vec2 direction = vec2(longitude / 6.2831853 + 0.5, asin(clamp(ray.y, 0.0, 1.0)));
    // Distant layers sit behind the shaded foreground artwork. Transparent
    // pixels reveal the gradient, rather than drawing opaque image rectangles.
    color = min(color + authoredStars(ray), vec3(1.0)); // Clouds pass in front of fixed, twinkling stars.
    color = authoredMoon(ray, color);
    vec4 farCloud = cloudSprite(uCloudTextures[2], direction, uCloudLayouts[2], uCloudSettings[2],
                               uCloudVariation[2], uCloudWarp[2], 2u);
    vec4 midCloud = cloudSprite(uCloudTextures[1], direction, uCloudLayouts[1], uCloudSettings[1],
                               uCloudVariation[1], uCloudWarp[1], 1u);
    vec4 nearCloud = cloudSprite(uCloudTextures[0], direction, uCloudLayouts[0], uCloudSettings[0],
                                uCloudVariation[0], uCloudWarp[0], 0u);
    color = color * (1.0 - farCloud.a) + farCloud.rgb;
    color = color * (1.0 - midCloud.a) + midCloud.rgb;
    return color * (1.0 - nearCloud.a) + nearCloud.rgb;
}
