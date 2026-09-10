#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;

// 4x4 Bayer ordered-dither matrix (row-major).
const mat4 bayer = mat4(
    vec4(0.0, 12.0, 3.0, 15.0),
    vec4(8.0, 4.0, 11.0, 7.0),
    vec4(2.0, 14.0, 1.0, 13.0),
    vec4(10.0, 6.0, 9.0, 5.0)
);

void main() {
    vec3 color = texture(uTexture, vTexCoord).rgb;

    vec2 retroPixel = floor(vTexCoord * vec2(textureSize(uTexture, 0)));
    float dither = (bayer[int(mod(retroPixel.x, 4.0))][int(mod(retroPixel.y, 4.0))] + 0.5) / 16.0;

    vec3 levels = vec3(15.0);
    vec3 quantized = min(floor(color * levels + dither), levels) / levels;

    FragColor = vec4(quantized, 1.0);
}
