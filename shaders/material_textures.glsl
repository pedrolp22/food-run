// Linear surface data, independent of the albedo's palette/gamma and final RGB555 pass.
uniform sampler2D uMaterialMap;
uniform sampler2D uNormalMap;
uniform sampler2D uOcclusionMap;
uniform bool uHasMaterialMap;
uniform bool uHasNormalMap;
uniform bool uHasOcclusionMap;
uniform bool uPackedReflectivity;
uniform bool uMaterialRetro;
uniform int uMaxMaterialLod;
uniform int uMaxNormalLod;
uniform int uMaxOcclusionLod;
uniform float uNormalScale;
uniform float uOcclusionStrength;

vec4 sampleMaterialData(sampler2D tex, vec2 uv, vec2 perspectiveUv, int maxLod) {
    vec2 dx = dFdx(perspectiveUv);
    vec2 dy = dFdy(perspectiveUv);
    if (!uMaterialRetro) {
        return textureGrad(tex, uv, dx, dy);
    }
    // Material detail follows the affine sample location but its mip footprint
    // stays perspective-correct. Nearest texels blend continuously between mips,
    // so tiny aggregate glints do not reintroduce the sidewalk's motion shimmer.
    vec2 dimensions = vec2(textureSize(tex, 0));
    float footprint = max(length(dx * dimensions), length(dy * dimensions));
    float lod = clamp(log2(max(footprint, 1.0)), 0.0, float(maxLod));
    float lower = floor(lod);
    return mix(textureLod(tex, uv, lower), textureLod(tex, uv, min(lower + 1.0, float(maxLod))), fract(lod));
}

vec3 materialProperties(vec2 uv, vec2 perspectiveUv) {
    if (!uHasMaterialMap) {
        return vec3(1.0);
    }
    vec4 data = sampleMaterialData(uMaterialMap, uv, perspectiveUv, uMaxMaterialLod);
    return vec3(data.g, data.b, uPackedReflectivity ? data.a : 1.0);
}

float materialOcclusion(vec2 uv, vec2 perspectiveUv) {
    if (!uHasOcclusionMap) {
        return 1.0;
    }
    return mix(1.0, sampleMaterialData(uOcclusionMap, uv, perspectiveUv, uMaxOcclusionLod).r,
               uOcclusionStrength);
}

vec3 materialNormal(vec2 uv, vec2 perspectiveUv, vec3 normal, vec3 worldPosition) {
    if (!uHasNormalMap || uNormalScale == 0.0) {
        return normal;
    }
    vec3 tangentNormal = sampleMaterialData(uNormalMap, uv, perspectiveUv, uMaxNormalLod).xyz * 2.0 - 1.0;
    tangentNormal.xy *= uNormalScale;
    tangentNormal.z = max(tangentNormal.z, 0.001);
    vec3 dx = dFdx(worldPosition);
    vec3 dy = dFdy(worldPosition);
    vec2 du = dFdx(perspectiveUv);
    vec2 dv = dFdy(perspectiveUv);
    float determinant = du.x * dv.y - du.y * dv.x;
    if (abs(determinant) < 1e-10) {
        return normal; // Missing/constant UVs must not poison the material with NaNs.
    }
    // Reconstruct a cotangent frame from real UVs, including flipped/tiled faces.
    // The PSX affine wobble never rotates the normal-map basis or affects silhouettes.
    vec3 tangent = (dx * dv.y - dy * du.y) / determinant;
    vec3 bitangent = (dy * du.x - dx * dv.x) / determinant;
    tangent -= normal * dot(normal, tangent);
    bitangent -= normal * dot(normal, bitangent);
    tangent /= max(length(tangent), 0.00001);
    bitangent /= max(length(bitangent), 0.00001);
    return normalize(tangent * tangentNormal.x + bitangent * tangentNormal.y + normal * tangentNormal.z);
}
