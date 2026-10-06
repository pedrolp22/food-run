#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform vec3 uTopColor;
uniform vec3 uBottomColor;
uniform float uTime;
uniform float uNightBlend;
uniform vec3 uSunDir;
uniform float uAspect;
uniform vec2 uRasterSize;
uniform float uCloudCoverage;
uniform vec3 uCameraForward;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;
uniform vec3 uCameraPosition;
uniform float uTanHalfFov;
uniform vec3 uSunColor;
uniform vec2 uSunRadii;
uniform vec4 uViewBounds = vec4(-1.0, -1.0, 1.0, 1.0);
uniform bool uHasPanorama;

// Continuous cloud fields inherit resolution and color precision from the
// selected rendering mode, without a second pixel grid or palette pass.
float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

vec2 cloudGradient(vec2 cell) {
    float angle = hash21(cell) * 6.283185;
    return vec2(cos(angle), sin(angle));
}

// Gradient Perlin noise: quintic interpolation keeps rounded billows continuous
// across lattice cells.
float perlin2D(vec2 p) {
    vec2 cell = floor(p);
    vec2 local = fract(p);
    vec2 fade = local * local * local * (local * (local * 6.0 - 15.0) + 10.0);
    float a = dot(cloudGradient(cell), local);
    float b = dot(cloudGradient(cell + vec2(1.0, 0.0)), local - vec2(1.0, 0.0));
    float c = dot(cloudGradient(cell + vec2(0.0, 1.0)), local - vec2(0.0, 1.0));
    float d = dot(cloudGradient(cell + vec2(1.0, 1.0)), local - vec2(1.0));
    return mix(mix(a, b, fade.x), mix(c, d, fade.x), fade.y);
}

struct NoiseCell {
    vec2 cell;
    vec4 gradientX;
    vec4 gradientY;
};

float cachedPerlin2D(vec2 p, inout NoiseCell cache) {
    vec2 cell = floor(p);
    // Adjacent ray samples usually stay in the same broad noise cell. Reuse
    // its four gradients; interpolation is identical across cell boundaries.
    if (any(notEqual(cell, cache.cell))) {
        vec2 a = cloudGradient(cell);
        vec2 b = cloudGradient(cell + vec2(1.0, 0.0));
        vec2 c = cloudGradient(cell + vec2(0.0, 1.0));
        vec2 d = cloudGradient(cell + vec2(1.0));
        cache.cell = cell;
        cache.gradientX = vec4(a.x, b.x, c.x, d.x);
        cache.gradientY = vec4(a.y, b.y, c.y, d.y);
    }
    vec2 local = fract(p);
    vec2 fade = local * local * local * (local * (local * 6.0 - 15.0) + 10.0);
    vec4 values = cache.gradientX * vec4(local.x, local.x - 1.0, local.x, local.x - 1.0) +
                  cache.gradientY * vec4(local.y, local.y, local.y - 1.0, local.y - 1.0);
    return mix(mix(values.x, values.y, fade.x), mix(values.z, values.w, fade.x), fade.y);
}

float hash31(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.x + p.y) * p.z);
}

float gradientDot(vec3 cell, vec3 local) {
    // Classic Perlin edge gradients avoid trigonometry for the 3D detail field.
    int h = int(hash31(cell) * 16.0);
    float u = h < 8 ? local.x : local.y;
    float v = h < 4 ? local.y : ((h == 12 || h == 14) ? local.x : local.z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float perlin3D(vec3 p) {
    vec3 cell = floor(p);
    vec3 local = fract(p);
    vec3 fade = local * local * local * (local * (local * 6.0 - 15.0) + 10.0);
    float a = gradientDot(cell, local);
    float b = gradientDot(cell + vec3(1.0, 0.0, 0.0), local - vec3(1.0, 0.0, 0.0));
    float c = gradientDot(cell + vec3(0.0, 1.0, 0.0), local - vec3(0.0, 1.0, 0.0));
    float d = gradientDot(cell + vec3(1.0, 1.0, 0.0), local - vec3(1.0, 1.0, 0.0));
    float e = gradientDot(cell + vec3(0.0, 0.0, 1.0), local - vec3(0.0, 0.0, 1.0));
    float f = gradientDot(cell + vec3(1.0, 0.0, 1.0), local - vec3(1.0, 0.0, 1.0));
    float g = gradientDot(cell + vec3(0.0, 1.0, 1.0), local - vec3(0.0, 1.0, 1.0));
    float h = gradientDot(cell + vec3(1.0), local - vec3(1.0));
    return mix(mix(mix(a, b, fade.x), mix(c, d, fade.x), fade.y),
               mix(mix(e, f, fade.x), mix(g, h, fade.x), fade.y), fade.z) * 0.7071;
}

float densityFromBroadNoise(vec3 p, float footprint, float broad, float threshold) {
    if (p.y <= 0.0 || p.y >= 1.0) {
        return 0.0;
    }
    // Broad Perlin islands decide where whole clouds form. Finer 3D noise only
    // shapes their billows, so it cannot fill every gap with unrelated wisps.
    float roundedHeight = (p.y - 0.48) * (p.y - 0.48) * 1.15;
    float shape = broad - threshold - roundedHeight;
    if (shape < -0.12) {
        return 0.0;
    }
    float detail = perlin3D(p * vec3(2.6, 1.8, 2.6)) * 0.18 *
                   (1.0 - smoothstep(0.18, 0.65, footprint * 2.6));
    detail += perlin3D(p * vec3(5.2, 3.6, 5.2) + vec3(9.7, 17.3, 3.1)) * 0.025 *
              (1.0 - smoothstep(0.18, 0.65, footprint * 5.2));
    float bounds = smoothstep(0.0, 0.10, p.y) * (1.0 - smoothstep(0.85, 1.0, p.y));
    return smoothstep(0.0, 0.045, shape + detail) * bounds;
}

float cloudDensity(vec3 p, float footprint, float threshold) {
    if (p.y <= 0.0 || p.y >= 1.0) {
        return 0.0;
    }
    float broad = perlin2D(p.xz * 0.7) * 0.72 +
                  perlin2D(p.xz * 1.41 + vec2(13.7, 8.1)) * 0.23;
    return densityFromBroadNoise(p, footprint, broad, threshold);
}

vec3 viewRay(vec2 uv) {
    vec2 plane = mix(uViewBounds.xy, uViewBounds.zw, uv) * vec2(uAspect, 1.0) * uTanHalfFov;
    return normalize(uCameraForward + uCameraRight * plane.x + uCameraUp * plane.y);
}

// Directional cube coordinates give stars a fixed place in the world,
// including the zenith. No latitude singularity or scrolling UVs.
vec3 skyCoordinates(vec3 ray) {
    vec3 magnitude = abs(ray);
    if (magnitude.z >= magnitude.x && magnitude.z >= magnitude.y) {
        return vec3(ray.xy / magnitude.z, ray.z > 0.0 ? 0.0 : 1.0);
    }
    if (magnitude.x >= magnitude.y) {
        return vec3(ray.zy / magnitude.x, ray.x > 0.0 ? 2.0 : 3.0);
    }
    return vec3(ray.xz / magnitude.y, ray.y > 0.0 ? 4.0 : 5.0);
}

vec2 planePoint(vec3 ray, float height, float frequency) {
    return (uCameraPosition.xz + ray.xz * height / max(ray.y, 0.025)) * frequency;
}

vec4 cloudVolume(vec3 ray, vec3 nextX, vec3 nextY, float altitude, float thickness, float frequency,
                 vec2 offset, vec2 velocity, float coverageBias, float opacityScale, float night, vec3 hazeColor) {
    float topHeight = altitude + thickness - uCameraPosition.y;
    if (ray.y <= 0.05 || topHeight <= 0.0 || uCloudCoverage <= 0.001) {
        return vec4(0.0);
    }
    float baseHeight = altitude - uCameraPosition.y;
    float entry = max(baseHeight / ray.y, 0.0);
    float exit = topHeight / ray.y;
    float middleHeight = max(baseHeight + thickness * 0.5, 1.0);
    vec2 centerPoint = planePoint(ray, middleHeight, frequency);
    float footprint = max(length(planePoint(nextX, middleHeight, frequency) - centerPoint),
                          length(planePoint(nextY, middleHeight, frequency) - centerPoint));
    float visibility = smoothstep(0.05, 0.18, ray.y) *
                       (1.0 - smoothstep(0.35, 0.75, footprint));
    if (visibility <= 0.001) {
        return vec4(0.0);
    }
    vec2 wind = offset + floor(uTime * 4.0) * 0.25 * velocity;
    vec3 lightDirection = normalize(uSunDir);
    vec3 lightStep = lightDirection * vec3(thickness * frequency, 1.0, thickness * frequency) /
                     max(abs(lightDirection.y), 0.3);
    vec3 shadowColor = mix(vec3(0.40, 0.49, 0.62), vec3(0.10, 0.12, 0.20), night);
    vec3 highlightColor = mix(vec3(1.0, 0.98, 0.93) * uSunColor, vec3(0.48, 0.48, 0.57), night);
    float rim = pow(max(dot(ray, lightDirection), 0.0), 6.0) * 0.08;
    const int samples = 32;
    float stepLength = (exit - entry) / float(samples);
    float transmittance = 1.0;
    vec3 radiance = vec3(0.0);
    float threshold = mix(0.45, -0.35, uCloudCoverage) + coverageBias;
    NoiseCell broadCell = NoiseCell(vec2(1e6), vec4(0.0), vec4(0.0));
    NoiseCell secondaryCell = NoiseCell(vec2(1e6), vec4(0.0), vec4(0.0));
    // Stable midpoint samples, with no animated jitter. Beer-Lambert opacity
    // accounts for path length, so camera angle reveals actual cloud thickness.
    for (int sampleIndex = 0; sampleIndex < samples; ++sampleIndex) {
        float distance = entry + (float(sampleIndex) + 0.5) * stepLength;
        vec3 worldPoint = uCameraPosition + ray * distance;
        vec3 point = vec3(worldPoint.x * frequency + wind.x,
                          (worldPoint.y - altitude) / thickness,
                          worldPoint.z * frequency + wind.y);
        float broad = cachedPerlin2D(point.xz * 0.7, broadCell) * 0.72 +
                      cachedPerlin2D(point.xz * 1.41 + vec2(13.7, 8.1), secondaryCell) * 0.23;
        float density = densityFromBroadNoise(point, footprint, broad, threshold);
        if (density <= 0.001) {
            continue;
        }
        float nearLight = cloudDensity(point + lightStep * 0.33, footprint, threshold);
        float farLight = cloudDensity(point + lightStep * 0.75, footprint, threshold);
        float lightOpticalDepth = (nearLight * 0.45 + farLight * 0.35 + density * 0.12) * thickness * 0.025;
        float shade = clamp(0.40 + exp(-lightOpticalDepth) * 0.54 + point.y * 0.06 +
                            rim * (1.0 - density), 0.0, 1.0);
        vec3 cloudColor = mix(shadowColor, highlightColor, shade);
        float haze = 1.0 - exp(-distance * 0.00025);
        cloudColor = mix(cloudColor, hazeColor, haze * 0.65);
        float alpha = 1.0 - exp(-density * stepLength * 0.035);
        radiance += transmittance * alpha * cloudColor;
        transmittance *= 1.0 - alpha;
        if (transmittance < 0.02) {
            break;
        }
    }
    float opacity = 1.0 - transmittance;
    return vec4(radiance / max(opacity, 0.001), opacity * visibility * opacityScale);
}

void main() {
    // Retain the five palette steps for day/night in either sky path.
    float night = floor(clamp(uNightBlend, 0.0, 1.0) * 4.0 + 0.5) / 4.0;
    // Use the actual render target: smooth at full resolution, naturally
    // pixel-sized on the PSX raster. This also filters distant noise at
    // the true pixel footprint rather than an artificially enlarged grid.
    vec3 ray = viewRay(vUv);
    if (uHasCloudSky) {
        FragColor = vec4(sampleCloudSky(ray), 1.0);
        return; // Gradient and authored alpha sprites; no volumetric cloud marching.
    }
    if (uHasPanorama) {
        vec3 color = samplePanorama(ray);
        FragColor = vec4(panoramaNight(color, night), 1.0);
        return; // No noise fields, cloud ray marches or sky texture generation.
    }
    vec3 nextX = viewRay(vUv + vec2(1.0 / uRasterSize.x, 0.0));
    vec3 nextY = viewRay(vUv + vec2(0.0, 1.0 / uRasterSize.y));
    vec3 coordinates = skyCoordinates(ray);

    // The fade is intentionally reduced to five palette steps so it reads as
    // a mid-90s color-cycle, never as a modern smooth cross-fade.
    vec3 dayTop = uTopColor;
    vec3 dayMiddle = vec3(0.47, 0.70, 0.89);
    vec3 dayHaze = uBottomColor;
    vec3 nightTop = vec3(0.045, 0.055, 0.14);
    vec3 nightMiddle = vec3(0.18, 0.17, 0.32);
    vec3 nightHaze = vec3(0.38, 0.31, 0.40);

    float elevation = sqrt(max(ray.y, 0.0));
    vec3 lowerSky = mix(dayHaze, nightHaze, night);
    vec3 middleSky = mix(dayMiddle, nightMiddle, night);
    vec3 upperSky = mix(dayTop, nightTop, night);
    vec3 color = elevation < 0.5 ? mix(lowerSky, middleSky, elevation * 2.0)
                                : mix(middleSky, upperSky, elevation * 2.0 - 1.0);

    // Sparse radial point sprites with only two hard brightness rings.
    // Face cells place their stars away from the edges, avoiding cut sprites.
    vec2 starGrid = (coordinates.xy * 0.5 + 0.5) * 64.0;
    vec2 starCell = floor(starGrid);
    vec2 starPoint = fract(starGrid) - vec2(0.5);
    float starDistance = length(starPoint);
    if (night > 0.0 && ray.y > 0.16 &&
        hash21(starCell + coordinates.z * vec2(71.0, 113.0)) > 0.985 && starDistance < 0.19) {
        vec3 starColor = starDistance < 0.095 ? vec3(0.95, 0.89, 0.67) : vec3(0.55, 0.53, 0.62);
        color = mix(color, starColor, night);
    }

    // The sun and moon share the light's world bearing. Angular discs do not
    // flatten at oblique views, and clouds can pass in front of either one.
    vec3 lightDirection = normalize(uSunDir);
    vec3 lightUp = abs(lightDirection.y) > 0.99 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 lightRight = normalize(cross(lightDirection, lightUp));
    lightUp = cross(lightRight, lightDirection);
    float facingLight = dot(ray, lightDirection);
    vec2 disc = vec2(dot(ray, lightRight), dot(ray, lightUp)) / max(facingLight, 0.001);
    if (night < 1.0 && facingLight > 0.0 && length(disc) < uSunRadii.y) {
        vec3 sunColor = length(disc) < uSunRadii.x ? uSunColor : uSunColor * 0.78;
        color = mix(color, sunColor, 1.0 - night);
    }
    float moonDistance = max(abs(disc.x) + abs(disc.y) * 0.35, abs(disc.y));
    if (night > 0.0 && facingLight > 0.0 && moonDistance < 0.065) {
        color = mix(color, vec3(0.79, 0.73, 0.57), night);
        vec2 crater = floor(disc * 75.0);
        if ((crater.x == -2.0 && crater.y == 1.0) || (crater.x == 2.0 && crater.y == -2.0) ||
            (crater.x == 0.0 && crater.y == -3.0)) {
            color = mix(color, vec3(0.56, 0.51, 0.46), night);
        }
    }

    // Sparse upper clouds sit behind the thicker, larger foreground billows.
    vec4 farCloud = cloudVolume(ray, nextX, nextY, 520.0, 80.0, 0.0034, vec2(21.7, 14.3),
                               vec2(0.009, 0.003), 0.04, 0.30, night, lowerSky);
    color = mix(color, farCloud.rgb, farCloud.a);
    vec4 nearCloud = cloudVolume(ray, nextX, nextY, 180.0, 100.0, 0.006, vec2(8.2, 3.7),
                                vec2(0.017, 0.006), 0.0, 0.96, night, lowerSky);
    color = mix(color, nearCloud.rgb, nearCloud.a);

    FragColor = vec4(color, 1.0);
}
