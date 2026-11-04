#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform bool uAntiAliasing;

vec3 framePixel(ivec2 pixel) {
    ivec2 size = textureSize(uTexture, 0);
    return texelFetch(uTexture, clamp(pixel, ivec2(0), size - 1), 0).rgb;
}

vec3 sceneSample(vec2 uv) {
    vec2 pixel = uv * vec2(textureSize(uTexture, 0)) - 0.5;
    ivec2 base = ivec2(floor(pixel));
    vec2 f = fract(pixel);
    // Normal mode reconstructs the lens smoothly, independently of sampler state.
    vec3 lower = mix(framePixel(base), framePixel(base + ivec2(1, 0)), f.x);
    vec3 upper = mix(framePixel(base + ivec2(0, 1)), framePixel(base + ivec2(1, 1)), f.x);
    return mix(lower, upper, f.y);
}

vec3 antialiasedSample(vec2 uv) {
    vec3 center = sceneSample(uv);
    if (!uAntiAliasing)
        return center;
    vec2 texel = 1.0 / vec2(textureSize(uTexture, 0));
    const vec3 luma = vec3(0.299, 0.587, 0.114);
    float nw = dot(sceneSample(uv + texel * vec2(-1, 1)), luma);
    float ne = dot(sceneSample(uv + texel * vec2(1, 1)), luma);
    float sw = dot(sceneSample(uv + texel * vec2(-1, -1)), luma);
    float se = dot(sceneSample(uv + texel * vec2(1, -1)), luma);
    float middle = dot(center, luma);
    float low = min(middle, min(min(nw, ne), min(sw, se)));
    float high = max(middle, max(max(nw, ne), max(sw, se)));
    if (high - low < max(0.0312, high * 0.125))
        return center;
    vec2 direction = vec2(-((nw + ne) - (sw + se)), (nw + sw) - (ne + se));
    float reduce = max((nw + ne + sw + se) * 0.03125, 0.0078125);
    direction = clamp(direction / (min(abs(direction.x), abs(direction.y)) + reduce), vec2(-8), vec2(8)) * texel;
    vec3 inner = 0.5 * (sceneSample(uv - direction / 6.0) + sceneSample(uv + direction / 6.0));
    vec3 outer = inner * 0.5 + 0.25 * (sceneSample(uv - direction * 0.5) + sceneSample(uv + direction * 0.5));
    float outerLuma = dot(outer, luma);
    return outerLuma < low || outerLuma > high ? inner : outer;
}

void main() {
    vec2 sceneUv = lensUv(vTexCoord);
    vec3 color = antialiasedSample(sceneUv) * screenSpaceAmbientVisibility(sceneUv);
    FragColor = vec4(color, 1.0);
}
