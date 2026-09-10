#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform vec3 uColor;
uniform float uCore;
uniform float uHaze;

void main() {
    vec2 centered = (vUv - vec2(0.5)) * 2.0;
    float dist = length(centered);
    float core = exp(-dist * dist * uCore);
    float haze = exp(-dist * uHaze * 2.0) * 0.5;
    float brightness = core + haze;
    FragColor = vec4(uColor * brightness, 1.0);
}
