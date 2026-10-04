#version 330 core

in vec3 vNormal;
in vec2 vUv;
noperspective in vec2 vAffineUv;
noperspective in vec2 vVertexLight;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec3 uBaseColor;
uniform float uHasTexture;
uniform vec3 uSunDir;
uniform float uAmbient;
uniform float uAffine;
uniform bool uVertexLighting;

void main() {
    vec2 uv = vUv;
    if (uAffine > 0.001) {
        uv = mix(vUv, vAffineUv, uAffine);
    }
    vec3 baseColor = uBaseColor * (uHasTexture > 0.5 ? sampleSurface(uTexture, uv).rgb : vec3(1.0));
    vec3 normal = normalize(vNormal);
    float diffuse = max(dot(normal, normalize(uSunDir)), 0.0);
    float ambient = clamp(uAmbient * (0.95 + 0.30 * normal.y), 0.0, 1.0);
    vec2 shade = uVertexLighting ? vVertexLight : vec2(ambient, (1.0 - ambient) * diffuse);
    float visibility = sunVisibility(diffuse);
    float light = shade.x * ditheredShadowAmbient(visibility, diffuse) + shade.y * visibility;
    FragColor = vec4(distanceFog(baseColor * light), 1.0);
}
