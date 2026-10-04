#version 330 core

in vec2 vTexCoord;
noperspective in vec2 vAffineTexCoord;
in vec2 vFacadeUv;
in vec3 vWorldPosition;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uLod;
uniform float uAmbientLight;
uniform float uAlpha;
uniform float uAffine;
uniform bool uReceivesShadow;
uniform vec3 uColor;
uniform vec3 uFaceNormal;
uniform float uSurfaceRoughness;
uniform float uSurfaceMetallic;
uniform float uSurfaceReflectivity;
uniform float uWindowRoughness;
uniform float uWindowReflectivity;
uniform bool uHasWindows;
uniform sampler2D uGlassMap;
uniform bool uHasGlassMap;
uniform int uMaxGlassLod;
uniform float uHaze;

void main() {
    vec2 tc = mix(vTexCoord, vAffineTexCoord, uAffine);
    vec3 base = sampleSurfaceLod(uTexture, tc, uLod, vTexCoord).rgb;
    // Authored masks decouple material identity from the albedo's blue palette.
    // Unmapped callers retain the scalar window finish and color-mask fallback.
    float glass = uHasGlassMap ? sampleMaterialData(uGlassMap, tc, vTexCoord, uMaxGlassLod).r
                              : (uHasWindows ? smoothstep(0.12, 0.22, base.b - max(base.r, base.g)) : 0.0);
    vec3 albedo = linearColor(base) * linearColor(uColor);
    // An opaque, tinted backing under a dielectric glass coat; towers do not
    // have modeled interiors. The blue pigment does not tint the sun reflection.
    albedo *= mix(1.0, 0.5, glass);
    float roughness = mix(uSurfaceRoughness, uWindowRoughness, glass);
    float metallic = uSurfaceMetallic * (1.0 - glass);
    float reflectivity = mix(uSurfaceReflectivity, uWindowReflectivity, glass);
    vec3 properties = materialProperties(tc, vTexCoord);
    bool mappedFacade = uHasGlassMap && uHasMaterialMap;
    if (mappedFacade) {
        // The packed map defines every pane and frame, using the shared factors.
        roughness = uSurfaceRoughness * properties.x;
        metallic = uSurfaceMetallic * properties.y;
        reflectivity = uSurfaceReflectivity * properties.z;
    } else {
        roughness *= mix(properties.x, 1.0, glass);
        metallic *= properties.y;
        reflectivity *= mix(properties.z, 1.0, glass);
    }
    vec3 geometricNormal = normalize(uFaceNormal);
    vec3 mappedNormal = materialNormal(tc, vTexCoord, geometricNormal, vWorldPosition);
    vec3 normal = mappedFacade ? mappedNormal : normalize(mix(mappedNormal, geometricNormal, glass));
    float noL = max(dot(normal, normalize(uSunDir)), 0.0);
    float visibility = uReceivesShadow ? sunVisibility(noL) : 1.0;
    float ambient = clamp(uAmbientLight, 0.0, 1.0);
    vec2 shade = vec2(ambient, (1.0 - ambient) * noL);
    // Foundation occlusion only affects indirect light, independent of UV tiling.
    float occlusion = mix(0.82 + 0.18 * smoothstep(0.0, 0.45, vFacadeUv.y), 1.0, abs(normal.y));
    occlusion *= mappedFacade ? materialOcclusion(tc, vTexCoord)
                              : mix(materialOcclusion(tc, vTexCoord), 1.0, glass);
    vec3 radiance = materialRadiance(albedo, normal, vWorldPosition, shade, visibility,
                                    roughness, metallic, reflectivity, occlusion);
    vec3 surface = mix(displayColor(radiance), uHazeColor, clamp(uHaze, 0.0, 1.0));
    FragColor = vec4(distanceFog(surface), uAlpha);
}
