#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform vec3 uTopColor;
uniform vec3 uBottomColor;
uniform float uTime;
uniform float uNightBlend;

// This is deliberately drawn on a small virtual screen.  The color steps,
// Bayer pattern, and angular silhouettes are part of the PS1-era look.
float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

float bayer4(ivec2 p) {
    const float pattern[16] = float[16](
        0.0, 8.0, 2.0, 10.0,
        12.0, 4.0, 14.0, 6.0,
        3.0, 11.0, 1.0, 9.0,
        15.0, 7.0, 13.0, 5.0);
    return pattern[(p.y & 3) * 4 + (p.x & 3)] / 16.0;
}

// Interpolates only along the face of each low-poly mountain, never across
// the entire sky.  Segment boundaries remain visibly angular.
float mountainRidge(float x, float frequency, float base, float height, float seed) {
    float segment = floor(x * frequency);
    float local = fract(x * frequency);
    float left = base + hash21(vec2(segment, seed)) * height;
    float right = base + hash21(vec2(segment + 1.0, seed)) * height;
    return left + (right - left) * local;
}

void main() {
    const vec2 virtualResolution = vec2(160.0, 120.0);
    vec2 pixel = floor(vUv * virtualResolution);
    vec2 uv = (pixel + 0.5) / virtualResolution;
    float dither = bayer4(ivec2(pixel));

    // The fade is intentionally reduced to five palette steps so it reads as
    // a mid-90s color-cycle, never as a modern smooth cross-fade.
    float night = floor(clamp(uNightBlend, 0.0, 1.0) * 4.0 + 0.5) / 4.0;
    vec3 dayTop = uTopColor;
    vec3 dayMiddle = vec3(0.47, 0.70, 0.89);
    vec3 dayHaze = uBottomColor;
    vec3 nightTop = vec3(0.045, 0.055, 0.14);
    vec3 nightMiddle = vec3(0.18, 0.17, 0.32);
    vec3 nightHaze = vec3(0.38, 0.31, 0.40);

    // Four hard palette bands: no smooth gradient or HDR-style falloff.
    vec3 color;
    if (uv.y < 0.27 + dither * 0.012) {
        color = mix(vec3(0.69, 0.76, 0.78), vec3(0.18, 0.15, 0.22), night);
    } else if (uv.y < 0.38 + dither * 0.012) {
        color = mix(dayHaze, nightHaze, night);
    } else if (uv.y < 0.58 + dither * 0.012) {
        color = mix(dayMiddle, nightMiddle, night);
    } else {
        color = mix(dayTop, nightTop, night);
    }

    // Distant and near low-poly mountain layers at the horizon.
    float farRidge = mountainRidge(uv.x, 11.0, 0.245, 0.095, 7.0);
    float nearRidge = mountainRidge(uv.x, 7.0, 0.205, 0.12, 19.0);
    if (uv.y < farRidge) {
        color = mix(vec3(0.34, 0.42, 0.48), vec3(0.16, 0.14, 0.24), night);
    }
    if (uv.y < nearRidge) {
        color = mix(vec3(0.20, 0.29, 0.33), vec3(0.095, 0.09, 0.16), night);
    }

    // Sprite-like clouds drift in coarse whole-pixel steps, retaining their
    // flat low-poly silhouettes instead of becoming volumetric.
    vec2 cloudUv = uv + vec2(floor(uTime * 2.0) * 0.0009, 0.0);
    vec2 cloudCell = floor(cloudUv * vec2(26.0, 15.0));
    float cloudSeed = hash21(vec2(floor(cloudCell.x * 0.25), 31.0));
    // Keep the cloud deck high above the horizon so it cannot read as a
    // mid-screen fog artifact.
    float cloudRow = 10.0 + floor(cloudSeed * 4.0);
    bool cloud = cloudCell.y == cloudRow && mod(cloudCell.x, 4.0) != 3.0;
    cloud = cloud || (cloudCell.y == cloudRow + 1.0 && mod(cloudCell.x + floor(cloudSeed * 5.0), 5.0) < 3.0);
    if (cloud && uv.y > 0.62 && uv.y < 0.94) {
        color = mix(vec3(0.85, 0.84, 0.76), vec3(0.25, 0.24, 0.37), night);
    }

    // Sparse radial point sprites with only two hard brightness rings.
    // They are procedurally placed, not textured star shapes.
    vec2 starGrid = uv * vec2(40.0, 30.0);
    vec2 starCell = floor(starGrid);
    vec2 starPoint = fract(starGrid) - vec2(0.5);
    float starDistance = length(starPoint);
    if (night > 0.0 && uv.y > 0.56 && hash21(starCell) > 0.985 && starDistance < 0.19) {
        vec3 starColor = starDistance < 0.095 ? vec3(0.95, 0.89, 0.67) : vec3(0.55, 0.53, 0.62);
        color = mix(color, starColor, night);
    }

    // Flat-shaded, pixel-quantized moon with a few square crater marks.
    vec2 moonPos = vec2(0.76, 0.76);
    vec2 moonDelta = (uv - moonPos) * vec2(1.0, 1.25);
    float moonDistance = max(abs(moonDelta.x) + abs(moonDelta.y) * 0.35, abs(moonDelta.y));
    if (night > 0.0 && moonDistance < 0.065) {
        color = mix(color, vec3(0.79, 0.73, 0.57), night);
        vec2 crater = floor((uv - moonPos) * 150.0);
        if ((crater.x == -2.0 && crater.y == 1.0) || (crater.x == 2.0 && crater.y == -2.0) ||
            (crater.x == 0.0 && crater.y == -3.0)) {
            color = mix(color, vec3(0.56, 0.51, 0.46), night);
        }
    }

    FragColor = vec4(color, 1.0);
}
