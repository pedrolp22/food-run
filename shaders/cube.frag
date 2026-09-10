#version 330 core

in vec3 vNormal;
out vec4 FragColor;

uniform vec3 uColor;
uniform vec3 uSunDir;
uniform float uAmbient;

void main() {
    vec3 normal = normalize(vNormal);
    float diffuse = max(dot(normal, normalize(uSunDir)), 0.0);
    float ambient = uAmbient * (0.95 + 0.30 * normal.y);
    float light = ambient + (1.0 - ambient) * diffuse * sunVisibility(diffuse);
    FragColor = vec4(uColor * light, 1.0);
}
