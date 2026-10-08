// References and approximation limits: docs/architecture.md.
// Reusable, scene-independent BRDFs. Colors and reflectance are linear;
// roughness is perceptual. No uniforms, textures or render-pass dependencies.
const float PI = 3.14159265;

float pow5(float value) {
    float squared = value * value;
    return squared * squared * value;
}

vec3 fresnelSchlick(float cosine, vec3 f0, float f90) {
    return f0 + (vec3(f90) - f0) * pow5(1.0 - cosine);
}

float distributionGGX(float noH, float alphaSquared) {
    float denominator = noH * noH * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / max(PI * denominator * denominator, 0.0000001);
}

float visibilitySmith(float noV, float noL, float alphaSquared) {
    float view = noL * sqrt(noV * noV * (1.0 - alphaSquared) + alphaSquared);
    float light = noV * sqrt(noL * noL * (1.0 - alphaSquared) + alphaSquared);
    return 0.5 / max(view + light, 0.00001);
}

// Karis' analytic split-sum fit avoids a BRDF lookup texture.
vec2 environmentBrdf(float noV, float roughness) {
    vec4 fit = roughness * vec4(-1.0, -0.0275, -0.572, 0.022) + vec4(1.0, 0.0425, 1.04, -0.04);
    float a004 = min(fit.x * fit.x, exp2(-9.28 * noV)) * fit.x + fit.y;
    return max(vec2(-1.04, 1.04) * a004 + fit.zw, vec2(0.0));
}

float ggxDirectionalAlbedo(float noV, float alphaSquared) {
    // Conservative analytic white-GGX energy estimate, rising toward unity
    // at grazing angles. Using A+B from the split-sum fit directly here
    // overcompensates smooth metals and grazing views of rough metals.
    float grazing = 1.0 - noV;
    return mix(1.0 / (1.0 + 2.0 * alphaSquared), 1.0, grazing * grazing);
}

struct MaterialBrdf {
    vec3 f0;
    float f90;
    vec3 diffuseColor;
    vec3 energyCompensation;
    vec3 environmentWeight;
    float roughness;
    float alphaSquared;
    float noV;
    float viewScatter;
};

MaterialBrdf prepareBrdf(vec3 albedo, float roughness, float metallic, float reflectivity,
                         float dielectricReflectance, float noV, float normalVariance) {
    MaterialBrdf material;
    roughness = clamp(roughness, 0.08, 1.0);
    float alpha = roughness * roughness;
    material.alphaSquared = min(alpha * alpha + clamp(normalVariance, 0.0, 0.25), 1.0);
    // Use the same filtered finish for sun, lamps and environment reflections.
    material.roughness = sqrt(sqrt(material.alphaSquared));
    metallic = clamp(metallic, 0.0, 1.0);
    material.f90 = clamp(reflectivity, 0.0, 1.0);
    material.f0 = mix(vec3(clamp(dielectricReflectance, 0.0, 1.0)), clamp(albedo, 0.0, 1.0), metallic) * material.f90;
    material.diffuseColor = albedo * (1.0 - metallic);
    material.noV = clamp(noV, 0.0001, 1.0);
    material.viewScatter = pow5(1.0 - material.noV);
    vec2 dfg = environmentBrdf(material.noV, material.roughness);
    // Turquin's scaled GGX compensation, with an analytic energy estimate
    // instead of a lookup table. Normalize the DFG to that same white energy.
    float energy = ggxDirectionalAlbedo(material.noV, material.alphaSquared);
    dfg *= energy / max(dfg.x + dfg.y, 0.05);
    material.energyCompensation = vec3(1.0) + material.f0 * (1.0 / energy - 1.0);
    material.environmentWeight = clamp((material.f0 * dfg.x + material.f90 * dfg.y) *
                                        material.energyCompensation, vec3(0.0), vec3(1.0));
    return material;
}

float diffuseBurley(MaterialBrdf material, float noL, float voH) {
    // Frostbite's energy-normalized Disney diffuse: rough surfaces scatter
    // back toward the light, while smooth ones darken at grazing angles.
    float f90 = material.roughness * (0.5 + 2.0 * voH * voH);
    float lightScatter = 1.0 + (f90 - 1.0) * pow5(1.0 - noL);
    float viewScatter = 1.0 + (f90 - 1.0) * material.viewScatter;
    return lightScatter * viewScatter * mix(1.0, 1.0 / 1.51, material.roughness) / PI;
}

vec3 evaluateBrdf(MaterialBrdf material, float noL, float noH, float voH, float alphaSquared) {
    vec3 fresnel = fresnelSchlick(voH, material.f0, material.f90);
    vec3 reflected = fresnel * material.energyCompensation;
    vec3 specular = distributionGGX(noH, alphaSquared) * visibilitySmith(material.noV, noL, alphaSquared) * reflected;
    vec3 diffuse = (vec3(1.0) - clamp(reflected, vec3(0.0), vec3(1.0))) * material.diffuseColor *
                   diffuseBurley(material, noL, voH);
    return diffuse + specular;
}

float specularOcclusion(float noV, float occlusion, float roughness) {
    // Lagarde's view-dependent specular AO; diffuse AO remains separate.
    return clamp(pow(noV + occlusion, exp2(-16.0 * roughness - 1.0)) - 1.0 + occlusion, 0.0, 1.0);
}
