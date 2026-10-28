#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;

vec3 framePixel(ivec2 pixel) {
    ivec2 size = textureSize(uTexture, 0);
    return texelFetch(uTexture, clamp(pixel, ivec2(0), size - 1), 0).rgb;
}

void main() {
    vec2 sceneUv = lensUv(vTexCoord);
    vec2 pixel = sceneUv * vec2(textureSize(uTexture, 0)) - 0.5;
    ivec2 base = ivec2(floor(pixel));
    vec2 f = fract(pixel);
    // Normal mode reconstructs the lens smoothly, independently of sampler state.
    vec3 lower = mix(framePixel(base), framePixel(base + ivec2(1, 0)), f.x);
    vec3 upper = mix(framePixel(base + ivec2(0, 1)), framePixel(base + ivec2(1, 1)), f.x);
    vec3 color = mix(lower, upper, f.y) * screenSpaceAmbientVisibility(sceneUv);
    FragColor = vec4(color, 1.0);
}
