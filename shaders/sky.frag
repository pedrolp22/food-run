#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform vec3 uTopColor;
uniform vec3 uBottomColor;

void main() {
    FragColor = vec4(mix(uBottomColor, uTopColor, vUv.y), 1.0);
}
