#version 330 core

in vec3 vNormal;
in vec2 vUv;
noperspective in vec2 vAffineUv;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec3 uBaseColor;
uniform float uHasTexture;
uniform vec3 uSunDir;
uniform float uAmbient;
uniform float uAffine;

void main() {
    vec2 uv = vUv;
    if (uAffine > 0.001) {
        uv = mix(vUv, vAffineUv, uAffine);
    }
    vec3 baseColor = uBaseColor * (uHasTexture > 0.5 ? sampleSurface(uTexture, uv).rgb : vec3(1.0));
    vec3 normal = normalize(vNormal);
    float diffuse = max(dot(normal, normalize(uSunDir)), 0.0);
    float ambient = uAmbient * (0.95 + 0.30 * normal.y);
    float light = ambient + (1.0 - ambient) * diffuse * sunVisibility(diffuse);
    FragColor = vec4(baseColor * light, 1.0);
}
