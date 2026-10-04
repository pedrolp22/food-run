#version 330 core

in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vTexCoord;
noperspective in vec2 vAffineTexCoord;
noperspective in vec2 vVertexLight;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform bool uHasTexture;
uniform bool uColorIsLinear;
uniform vec3 uColor;
uniform float uSurfaceRoughness;
uniform float uSurfaceMetallic;
uniform float uSurfaceReflectivity;
uniform float uLod;
uniform float uAmbient;
uniform bool uVertexLighting;
uniform bool uReceivesShadow;
uniform float uAffine;
uniform float uAlpha;
uniform float uHaze;
uniform float uTime;
uniform float uHeat;

void main() {
    vec2 tc = mix(vTexCoord, vAffineTexCoord, uAffine);
    float heatBoost = 0.0;
    if (uHeat > 0.0001) {
        tc += vec2(sin(uTime * 2.3 + vTexCoord.y * 7.0) * 0.7 + sin(uTime * 3.4 + vTexCoord.y * 17.0) * 0.3,
                   sin(uTime * 1.7 + vTexCoord.y * 11.0) * 0.5) * (uHeat * 0.008);
        heatBoost = 0.04 * uHeat * sin(uTime * 4.2 + vTexCoord.y * 3.0);
    }
    vec3 texel = vec3(1.0);
    if (uHasTexture) {
        texel = uLod < 0.0 ? sampleSurface(uTexture, tc).rgb : sampleSurfaceLod(uTexture, tc, uLod, vTexCoord).rgb;
    }
    // glTF factors are linear; game colors and base-color textures use sRGB.
    vec3 albedo = linearColor(texel) * (uColorIsLinear ? uColor : linearColor(uColor));
    vec3 geometricNormal = normalize(vNormal);
    vec3 normal = materialNormal(tc, vTexCoord, geometricNormal, vWorldPosition);
    vec3 properties = materialProperties(tc, vTexCoord);
    float occlusion = materialOcclusion(tc, vTexCoord);
    float noL = max(dot(normal, normalize(uSunDir)), 0.0);
    float ambient = clamp(uAmbient * (0.95 + 0.30 * normal.y), 0.0, 1.0);
    vec2 shade = uVertexLighting ? vVertexLight : vec2(ambient, (1.0 - ambient) * noL);
    if (uVertexLighting && uHasNormalMap) {
        float geometricLight = max(dot(geometricNormal, normalize(uSunDir)), 0.0);
        shade.y = max(shade.y + (1.0 - ambient) * (noL - geometricLight), 0.0);
    }
    float visibility = uReceivesShadow ? sunVisibility(noL) : 1.0;
    vec3 radiance = materialRadiance(albedo, normal, vWorldPosition, shade, visibility,
                                    uSurfaceRoughness * properties.x, uSurfaceMetallic * properties.y,
                                    uSurfaceReflectivity * properties.z, occlusion);
    vec3 surface = displayColor(radiance * (1.0 + heatBoost));
    FragColor = vec4(distanceFog(mix(surface, uHazeColor, clamp(uHaze, 0.0, 1.0))), uAlpha);
}
