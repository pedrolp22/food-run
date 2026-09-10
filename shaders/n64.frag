#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;

void main() {
    // Edge coverage is already resolved from MSAA. Preserve those pixels when
    // enlarging the image; filtering the whole frame again blurs every detail.
    ivec2 size = textureSize(uTexture, 0);
    ivec2 pixel = clamp(ivec2(vTexCoord * vec2(size)), ivec2(0), size - 1);
    FragColor = vec4(texelFetch(uTexture, pixel, 0).rgb, 1.0);
}
