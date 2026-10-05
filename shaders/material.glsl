// Shared linear-space material model for buildings, streets and props.
uniform vec3 uSunDir;
uniform vec3 uCameraPosition;
uniform vec3 uSkyColor;
uniform vec3 uSunColor;
uniform vec3 uHazeColor;

const float PI = 3.14159265;

vec3 linearColor(vec3 color) {
    color = max(color, vec3(0.0));
    return mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), color));
}

vec3 displayColor(vec3 radiance) {
    // Preserve the texture palette below the shoulder, and compress bright
    // specular energy without independently clipping its color channels.
    float peak = max(radiance.r, max(radiance.g, radiance.b));
    float mapped = peak <= 0.8 ? peak : 0.8 + 0.2 * (1.0 - exp(-(peak - 0.8) / 0.2));
    vec3 color = max(radiance, vec3(0.0)) * mapped / max(peak, 0.00001);
    return mix(color * 12.92, 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), color));
}

vec3 fresnelSchlick(float cosine, vec3 f0) {
    return f0 + (vec3(1.0) - f0) * pow(1.0 - cosine, 5.0);
}

// Cook-Torrance BRDF: GGX distribution, height-correlated Smith visibility,
// Schlick Fresnel and energy-weighted Lambert diffuse. Roughness is perceptual.
float distributionGGX(float noH, float alphaSquared) {
    float denominator = noH * noH * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / max(PI * denominator * denominator, 0.0000001);
}

float visibilitySmith(float noV, float noL, float alphaSquared) {
    float view = noL * sqrt(noV * noV * (1.0 - alphaSquared) + alphaSquared);
    float light = noV * sqrt(noL * noL * (1.0 - alphaSquared) + alphaSquared);
    return 0.5 / max(view + light, 0.00001);
}

vec3 environmentColor(float elevation) {
    vec3 horizon = linearColor(uHazeColor);
    vec3 ground = horizon * vec3(0.12, 0.14, 0.18);
    vec3 sky = mix(horizon, linearColor(uSkyColor), smoothstep(0.0, 0.85, elevation));
    return mix(ground, sky, smoothstep(-0.18, 0.08, elevation));
}

vec3 diffuseEnvironment(vec3 normal) {
    // Hemisphere irradiance approximation: walls see both sky and ground;
    // roofs see more sky. The material's base color remains in linear space.
    vec3 sky = linearColor(uSkyColor) * 0.35 + linearColor(uHazeColor) * 0.65;
    vec3 ground = linearColor(uHazeColor) * vec3(0.12, 0.14, 0.18);
    return mix(ground, sky, normal.y * 0.5 + 0.5);
}

vec3 reflectedEnvironment(vec3 reflection, vec3 normal, float roughness) {
    // Convolve the analytic sky/ground environment over a widening lobe. This
    // approximates a prefiltered environment, without drawing painted bands.
    vec3 dominant = normalize(mix(reflection, normal, roughness * roughness));
    float spread = roughness * roughness * 0.75;
    vec3 radiance = environmentColor(dominant.y) * 0.4;
    radiance += environmentColor(dominant.y + spread) * 0.2;
    radiance += environmentColor(dominant.y - spread) * 0.2;
    radiance += environmentColor(dominant.y + spread * 2.0) * 0.1;
    radiance += environmentColor(dominant.y - spread * 2.0) * 0.1;
    return radiance;
}

vec3 environmentReflectance(float noV, float roughness, vec3 f0) {
    // Analytic split-sum DFG approximation (Karis, 2014). At high roughness
    // the broad environment lobe loses its sharp grazing-angle mirror response.
    vec4 fit = roughness * vec4(-1.0, -0.0275, -0.572, 0.022) + vec4(1.0, 0.0425, 1.04, -0.04);
    float a004 = min(fit.x * fit.x, exp2(-9.28 * noV)) * fit.x + fit.y;
    vec2 ab = vec2(-1.04, 1.04) * a004 + fit.zw;
    return clamp(f0 * ab.x + ab.y, vec3(0.0), vec3(1.0));
}

vec3 materialRadianceEnvironment(vec3 albedo, vec3 normal, vec3 worldPosition, vec2 shade, float visibility,
                                 float roughness, float metallic, float reflectivity, float occlusion,
                                 vec3 environment, float dielectricReflectance) {
    roughness = clamp(roughness, 0.08, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);
    reflectivity = clamp(reflectivity, 0.0, 1.0);
    vec3 f0 = mix(vec3(clamp(dielectricReflectance, 0.0, 1.0)), albedo, metallic);
    vec3 toCamera = uCameraPosition - worldPosition;
    vec3 view = toCamera / max(length(toCamera), 0.00001);
    vec3 light = normalize(uSunDir);
    vec3 halfVector = light + view;
    halfVector /= max(length(halfVector), 0.00001);
    float noV = max(dot(normal, view), 0.0001);
    float noL = max(dot(normal, light), 0.0);
    float noH = max(dot(normal, halfVector), 0.0);
    float voH = clamp(dot(view, halfVector), 0.0, 1.0);

    // Widen unresolved sun glints at the actual raster footprint. Environment
    // roughness remains independent of the sun, with no jitter or timed noise.
    vec3 dx = dFdx(halfVector);
    vec3 dy = dFdy(halfVector);
    vec3 normalDx = dFdx(normal);
    vec3 normalDy = dFdy(normal);
    float variance = min(0.25 * (dot(dx, dx) + dot(dy, dy) +
                                dot(normalDx, normalDx) + dot(normalDy, normalDy)), 0.25);
    float alphaSquared = min(pow(roughness, 4.0) + variance, 1.0);
    vec3 fresnel = fresnelSchlick(voH, f0) * reflectivity;
    vec3 specular = distributionGGX(noH, alphaSquared) * visibilitySmith(noV, noL, alphaSquared) * fresnel;
    vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - metallic) * albedo / PI;
    vec3 direct = (diffuse + specular) * linearColor(uSunColor) * PI * shade.y * visibility;

    vec3 envFresnel = environmentReflectance(noV, roughness, f0) * reflectivity;
    vec3 indirect = (vec3(1.0) - envFresnel) * (1.0 - metallic) * albedo * diffuseEnvironment(normal);
    indirect += environment * envFresnel;
    indirect *= clamp(shade.x, 0.0, 1.0) * 2.0 * occlusion * ditheredShadowAmbient(visibility, noL);
    return direct + indirect;
}

vec3 materialRadianceEnvironment(vec3 albedo, vec3 normal, vec3 worldPosition, vec2 shade, float visibility,
                                 float roughness, float metallic, float reflectivity, float occlusion,
                                 vec3 environment) {
    return materialRadianceEnvironment(albedo, normal, worldPosition, shade, visibility, roughness,
                                       metallic, reflectivity, occlusion, environment, 0.04);
}

vec3 materialRadiance(vec3 albedo, vec3 normal, vec3 worldPosition, vec2 shade, float visibility,
                      float roughness, float metallic, float reflectivity, float occlusion) {
    vec3 toCamera = uCameraPosition - worldPosition;
    vec3 view = toCamera / max(length(toCamera), 0.00001);
    vec3 reflection = reflect(-view, normal);
    return materialRadianceEnvironment(albedo, normal, worldPosition, shade, visibility,
                                       roughness, metallic, reflectivity, occlusion,
                                       reflectedEnvironment(reflection, normal, clamp(roughness, 0.08, 1.0)));
}
