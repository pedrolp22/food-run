#version 330 core
in vec2 vUv;
out vec4 FragColor;

uniform float uTime;
uniform int uSelection;

// Five-by-five bitmap glyphs, packed from the top left.
const uint glyphs[26] = uint[26](
    0x118fe2eu, // A
    0xf8be2fu, // B
    0x1e0843eu, // C
    0xf8c62fu, // D
    0x1f0bc3fu, // E
    0x0u, // F
    0x0u, // G
    0x0u, // H
    0x1f2109fu, // I
    0x0u, // J
    0x0u, // K
    0x1f08421u, // L
    0x118d771u, // M
    0x11cd671u, // N
    0xe8c62eu, // O
    0x10be2fu, // P
    0x0u, // Q
    0x92be2fu, // R
    0xf8383eu, // S
    0x42109fu, // T
    0xe8c631u, // U
    0x0u, // V
    0x11dd631u, // W
    0x1151151u, // X
    0x421151u, // Y
    0x0u // Z
);

float textMask(vec2 pixel, float top, int letters[12], int count, float scale) {
    vec2 origin = vec2((240.0 - float(count * 6 - 1) * scale) * 0.5, top);
    ivec2 cell = ivec2(floor((pixel - origin) / scale));
    if (cell.x < 0 || cell.x >= count * 6 || cell.y < 0 || cell.y >= 5) {
        return 0.0;
    }
    int letter = letters[cell.x / 6];
    int column = cell.x % 6;
    if (letter < 0 || column >= 5) {
        return 0.0;
    }
    return float((glyphs[letter] >> uint(cell.y * 5 + column)) & 1u);
}

vec4 drawText(vec4 background, vec2 pixel, float top, int letters[12], int count, float scale, vec3 ink) {
    float label = textMask(pixel, top, letters, count, scale);
    float outline = max(max(textMask(pixel + vec2(1.0, 0.0), top, letters, count, scale),
                            textMask(pixel - vec2(1.0, 0.0), top, letters, count, scale)),
                        max(textMask(pixel + vec2(0.0, 1.0), top, letters, count, scale),
                            textMask(pixel - vec2(0.0, 1.0), top, letters, count, scale)));
    background = mix(background, vec4(0.02, 0.03, 0.06, 0.92), max(outline, label));
    return mix(background, vec4(ink, 1.0), label);
}

void main() {
    vec2 pixel = floor(vec2(vUv.x, 1.0 - vUv.y) * vec2(240.0, 320.0));
    vec4 color = vec4(0.0);
    color = drawText(color, pixel, 56.0, int[12](18, 20, 1, 22, 0, 24, -1, 12, 14, 13, 4, 24),
                     12, 3.0, vec3(0.95, 0.78, 0.25));

    for (int row = 0; row < 4; ++row) {
        float top = 134.0 + float(row) * 30.0;
        bool selected = row == clamp(uSelection, 0, 3);
        if (selected && pixel.x >= 52.0 && pixel.x < 188.0 && pixel.y >= top - 8.0 && pixel.y < top + 18.0) {
            color = vec4(0.12, 0.20, 0.34, 0.75);
        }
        float pulse = 0.85 + 0.15 * sin(uTime * 3.0);
        vec3 ink = selected ? vec3(1.0, 0.85, 0.35) * pulse : vec3(0.82, 0.88, 0.96);
        if (row == 0) {
            color = drawText(color, pixel, top, int[12](15, 11, 0, 24, -1, -1, -1, -1, -1, -1, -1, -1),
                             4, 2.0, ink);
        } else if (row == 1) {
            color = drawText(color, pixel, top, int[12](14, 15, 19, 8, 14, 13, 18, -1, -1, -1, -1, -1),
                             7, 2.0, ink);
        } else if (row == 2) {
            color = drawText(color, pixel, top, int[12](2, 17, 4, 3, 8, 19, 18, -1, -1, -1, -1, -1),
                             7, 2.0, ink);
        } else {
            color = drawText(color, pixel, top, int[12](4, 23, 8, 19, -1, -1, -1, -1, -1, -1, -1, -1),
                             4, 2.0, ink);
        }
    }
    FragColor = color;
}
