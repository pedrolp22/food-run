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
uniform float uAlphaCutoff;
uniform float uAffine;
uniform bool uReceivesShadow;
uniform vec3 uColor;
uniform vec3 uFaceNormal;
uniform float uSurfaceRoughness;
uniform float uSurfaceMetallic;
uniform float uSurfaceReflectivity;
uniform float uSurfaceReflectance;
uniform float uWindowRoughness;
uniform float uWindowReflectivity;
uniform bool uHasWindows;
uniform sampler2D uGlassMap;
uniform bool uHasGlassMap;
uniform int uMaxGlassLod;
uniform float uHaze;
uniform samplerCube uWorldReflection;
uniform bool uHasWorldReflection;
uniform float uWorldReflectionMaxLod;
uniform sampler2D uPlanarReflection;
uniform bool uHasPlanarReflection;
uniform mat4 uPlanarReflectionViewProjection;
uniform float uPlanarReflectionMaxLod;
uniform float uPlanarReflectionStrength;
uniform vec2 uPlanarReflectionTextureScale;
uniform float uWindowReflectance;
uniform float uWindowBackingTint;
uniform bool uHasSkyPanorama;
uniform float uSkyPanoramaNight;

void main() {
    vec2 tc = mix(vTexCoord, vAffineTexCoord, uAffine);
    vec4 texel = sampleSurfaceLod(uTexture, tc, uLod, vTexCoord);
    if (texel.a < uAlphaCutoff) { discard; }
    vec3 base = texel.rgb;
    // Authored masks decouple material identity from the albedo's blue palette.
    // Unmapped callers retain the scalar window finish and color-mask fallback.
    float glass = uHasGlassMap ? sampleMaterialData(uGlassMap, tc, vTexCoord, uMaxGlassLod).r
                              : (uHasWindows ? smoothstep(0.12, 0.22, base.b - max(base.r, base.g)) : 0.0);
    vec3 albedo = linearColor(base) * linearColor(uColor);
    // An opaque, tinted backing under a dielectric glass coat; towers do not
    // have modeled interiors. The blue pigment does not tint the sun reflection.
    albedo *= mix(1.0, uWindowBackingTint, glass);
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
    float noL = max(dot(normal, uSunDir), 0.0);
    float visibility = uReceivesShadow ? sunVisibility(noL) : 1.0;
    float ambient = clamp(uAmbientLight, 0.0, 1.0);
    vec2 shade = vec2(ambient, (1.0 - ambient) * noL);
    // Foundation occlusion only affects indirect light, independent of UV tiling.
    float occlusion = mix(0.82 + 0.18 * smoothstep(0.0, 0.45, vFacadeUv.y), 1.0, abs(normal.y));
    occlusion *= mappedFacade ? materialOcclusion(tc, vTexCoord)
                              : mix(materialOcclusion(tc, vTexCoord), 1.0, glass);
    vec3 toCamera = uCameraPosition - vWorldPosition;
    vec3 view = toCamera / max(length(toCamera), 0.00001);
    MaterialBrdf material = materialBrdf(albedo, normal, view, roughness, metallic, reflectivity,
                                           mix(uSurfaceReflectance, uWindowReflectance, glass));
    roughness = material.roughness;
    vec3 reflection = reflect(-view, normal);
    vec3 environment = reflectedEnvironment(reflection, normal, clamp(roughness, 0.08, 1.0));
    vec3 worldEnvironment = environment;
    if (uHasCloudSky && glass > 0.0) {
        vec3 sky = linearColor(sampleCloudSky(reflection));
        worldEnvironment = mix(environment, sky, smoothstep(-0.08, 0.04, reflection.y));
    }
    if (uHasSkyPanorama && !uHasCloudSky && glass > 0.0) {
        vec3 sky = linearColor(panoramaNight(samplePanorama(reflection), uSkyPanoramaNight));
        // The bitmap has no ground detail. Nearby streets and actors come from
        // the planar capture; retain the analytic ground farther away.
        worldEnvironment = mix(environment, sky, smoothstep(-0.08, 0.04, reflection.y));
    }
    if (uHasWorldReflection && glass > 0.0) {
        // The sRGB cubemap supplies decoded linear radiance to the BRDF.
        // The geometric reflection ray stays fixed as roughness blurs the mip chain.
        float lod = (roughness * roughness) * uWorldReflectionMaxLod;
        vec3 world = textureLod(uWorldReflection, reflection, lod).rgb;
        worldEnvironment = world;
    }
    if (uHasPlanarReflection && glass > 0.0) {
        // The same mirrored camera projects the capture and the actual world
        // position on the facade; albedo tiling never stretches the reflection.
        vec4 projected = uPlanarReflectionViewProjection * vec4(vWorldPosition, 1.0);
        vec2 reflectionUv = projected.xy / max(projected.w, 0.00001) * 0.5 + 0.5;
        if (projected.w > 0.0 && all(greaterThanEqual(reflectionUv, vec2(0.0))) &&
            all(lessThanEqual(reflectionUv, vec2(1.0)))) {
            float lod = (roughness * roughness) * uPlanarReflectionMaxLod;
            // Persistent buffers may be larger than the current cropped view.
            // Sample only its populated viewport, including at blurred mip levels.
            vec2 texelRadius = vec2(exp2(lod) * 0.5) / vec2(textureSize(uPlanarReflection, 0));
            vec2 sampleUv = clamp(reflectionUv * uPlanarReflectionTextureScale, texelRadius,
                                  uPlanarReflectionTextureScale - texelRadius);
            vec3 world = textureLod(uPlanarReflection, sampleUv, lod).rgb;
            worldEnvironment = mix(worldEnvironment, world, uPlanarReflectionStrength);
        }
    }
    // Apply the pane mask once, after choosing the capture. Reapplying it at
    // each stage leaves a displaced cubemap image underneath the planar one.
    environment = mix(environment, worldEnvironment, glass);
    vec3 radiance = materialRadianceEnvironment(material, normal, vWorldPosition, view, shade, visibility,
                                               occlusion, environment);
    vec3 surface = mix(displayColor(radiance), uHazeColor, clamp(uHaze, 0.0, 1.0));
    FragColor = vec4(distanceFog(surface), uAlpha);
}
