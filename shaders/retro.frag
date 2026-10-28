#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uDitherStrength = 1.0;

// PS1 GPU signed 4x4 offsets, stored as columns for [x][y] access.
const mat4 psxDither = mat4(
    vec4(-4.0, 2.0, -3.0, 3.0),
    vec4(0.0, -2.0, 1.0, -1.0),
    vec4(-3.0, 3.0, -4.0, 2.0),
    vec4(1.0, -1.0, 0.0, -2.0)
);

vec3 frameRgb8(ivec2 pixel, ivec2 size) {
    return floor(texelFetch(uTexture, clamp(pixel, ivec2(0), size - 1), 0).rgb * 255.0 + 0.5);
}

vec3 transitionMask(ivec2 pixel, ivec2 size, vec3 center) {
    // Look in native framebuffer space, independent of the lens or window size.
    // A small radius catches gradual shading even after rounding to 8-bit RGB.
    vec3 left = frameRgb8(pixel - ivec2(2, 0), size);
    vec3 right = frameRgb8(pixel + ivec2(2, 0), size);
    vec3 below = frameRgb8(pixel - ivec2(0, 2), size);
    vec3 above = frameRgb8(pixel + ivec2(0, 2), size);
    vec3 span = max(abs(right - left), abs(above - below));
    vec3 curvature = max(abs(right + left - 2.0 * center), abs(above + below - 2.0 * center));
    // Flat channels stay clean. Coherent ramps get dither; isolated color steps
    // and alternating texels do not. Allow for one byte of rounding in slow fades.
    vec3 blend = smoothstep(vec3(0.0), vec3(2.0), span);
    blend *= 1.0 - smoothstep(vec3(0.35), vec3(0.85), curvature / max(span, vec3(2.0)));
    vec3 difference = max(max(abs(left - center), abs(right - center)),
                          max(abs(below - center), abs(above - center)));
    float edge = max(difference.r, max(difference.g, difference.b));
    // Protect silhouettes, window frames and other high-contrast texture details.
    return blend * (1.0 - smoothstep(8.0, 24.0, edge));
}

void main() {
    ivec2 size = textureSize(uTexture, 0);
    ivec2 pixel = clamp(ivec2(lensUv(vTexCoord) * vec2(size)), ivec2(0), size - 1);
    vec4 frame = texelFetch(uTexture, pixel, 0);
    if (frame.a > 0.0) {
        frame.rgb *= screenSpaceAmbientVisibility((vec2(pixel) + 0.5) / vec2(size));
    }
    vec3 rgb8 = floor(frame.rgb * 255.0 + 0.5);
    // Anchor dither to native, top-down framebuffer coordinates, before the
    // lens and enlargement. PS1 saturates 8-bit RGB + offset, then drops 3 bits.
    int row = (size.y - 1 - pixel.y) % 4;
    float dither = psxDither[pixel.x % 4][row];
    // Alpha is zero for visible sprite fragments and one for the 3D scene.
    // Sample it at the same native pixel as color so lens mapping cannot shift the exemption.
    vec3 mask = uDitherStrength > 0.0 && frame.a > 0.0
                ? transitionMask(pixel, size, rgb8) * frame.a : vec3(0.0);
    vec3 quantized = floor(clamp(rgb8 + dither * mask * uDitherStrength, 0.0, 255.0) / 8.0) / 31.0;
    FragColor = vec4(quantized, 1.0);
}
