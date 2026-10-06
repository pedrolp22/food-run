#version 330 core
in vec2 vUv;
uniform sampler2D uTexture;
uniform bool uTextured;
uniform float uOpacity;
uniform float uAlphaCutoff;
uniform float uBaseAlpha;
void main() {
    float alpha = uTextured ? texture(uTexture, vUv).a : 1.0;
    if (alpha * uBaseAlpha < uAlphaCutoff) { discard; }
    // Fade distant building shadows without writing a solid invisible occluder.
    const int bayer[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
    ivec2 pixel = ivec2(gl_FragCoord.xy) % 4;
    if (uOpacity < (float(bayer[pixel.y * 4 + pixel.x]) + 0.5) / 16.0) { discard; }
}
