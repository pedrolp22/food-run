#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform vec3 uColor;
uniform float uCore;
uniform float uHaze;

void main() {
    vec2 centered = (vUv - vec2(0.5)) * 2.0;
    float dist = length(centered);
    // Keep the established controls, but use them for stepped disc radii
    // rather than a soft glow. This preserves a flat PS1-era sun.
    float innerRadius = clamp(0.38 + uCore * 0.015, 0.38, 0.54);
    float outerRadius = clamp(0.60 + uHaze * 0.05, 0.62, 0.76);
    if (dist > outerRadius) {
        discard;
    }
    vec3 shade = dist < innerRadius ? uColor : uColor * 0.78;
    FragColor = vec4(shade, 1.0);
}
