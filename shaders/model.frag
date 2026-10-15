#version 330 core

in vec3 vNormal;
in vec2 vUv;
noperspective in vec2 vAffineUv;
noperspective in vec2 vVertexLight;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec3 uBaseColor;
uniform float uHasTexture;
uniform float uAlphaCutoff;
uniform float uBaseAlpha;
uniform bool uDoubleSided;
uniform vec3 uSunDir;
uniform float uAmbient;
uniform float uAffine;
uniform bool uVertexLighting;

void main() {
    vec2 uv = vUv;
    if (uAffine > 0.001) {
        uv = mix(vUv, vAffineUv, uAffine);
    }
    vec4 texel = uHasTexture > 0.5 ? sampleSurface(uTexture, uv) : vec4(1.0);
    if (texel.a * uBaseAlpha < uAlphaCutoff) { discard; }
    vec3 baseColor = uBaseColor * texel.rgb;
    vec3 normal = normalize(vNormal);
    bool backFace = uDoubleSided && !gl_FrontFacing;
    if (backFace) { normal = -normal; }
    float diffuse = max(dot(normal, normalize(uSunDir)), 0.0);
    float ambient = clamp(uAmbient * (0.95 + 0.30 * normal.y), 0.0, 1.0);
    vec2 shade = uVertexLighting && !backFace ? vVertexLight : vec2(ambient, (1.0 - ambient) * diffuse);
    float visibility = sunVisibility(diffuse);
    float light = shade.x * ditheredShadowAmbient(visibility, diffuse) * ambientOcclusionVisibility(visibility) +
                  shade.y * visibility;
    FragColor = vec4(distanceFog(baseColor * light), 1.0);
}
