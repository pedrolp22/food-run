// Shared linear-space material model for buildings, streets and props.
uniform vec3 uSunDir;
uniform vec3 uCameraPosition;
uniform vec3 uSkyRadiance;
uniform vec3 uHorizonRadiance;
uniform vec3 uSunRadiance;
uniform vec3 uHazeColor;
uniform int uPointLightCount;
uniform vec4 uPointLightPositionRadius[MATERIAL_MAX_POINT_LIGHTS]; // Capacity supplied from Scene.h by program assembly.
uniform vec3 uPointLightRadiance[MATERIAL_MAX_POINT_LIGHTS];

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

vec3 environmentColor(float elevation) {
    vec3 horizon = uHorizonRadiance;
    vec3 ground = horizon * vec3(0.12, 0.14, 0.18);
    vec3 sky = mix(horizon, uSkyRadiance, smoothstep(0.0, 0.85, elevation));
    return mix(ground, sky, smoothstep(-0.18, 0.08, elevation));
}

vec3 diffuseEnvironment(vec3 normal) {
    // Hemisphere irradiance approximation: walls see both sky and ground;
    // roofs see more sky. The material's base color remains in linear space.
    vec3 sky = uSkyRadiance * 0.35 + uHorizonRadiance * 0.65;
    vec3 ground = uHorizonRadiance * vec3(0.12, 0.14, 0.18);
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

MaterialBrdf materialBrdf(vec3 albedo, vec3 normal, vec3 view, float roughness, float metallic,
                           float reflectivity, float dielectricReflectance) {
    vec3 dx = dFdx(normal);
    vec3 dy = dFdy(normal);
    float variance = min(0.25 * (dot(dx, dx) + dot(dy, dy)), 0.25);
    return prepareBrdf(albedo, roughness, metallic, reflectivity, dielectricReflectance,
                       dot(normal, view), variance);
}

vec3 materialRadianceEnvironment(MaterialBrdf material, vec3 normal, vec3 worldPosition, vec3 view,
                                 vec2 shade, float visibility, float occlusion, vec3 environment) {
    vec3 light = uSunDir;
    vec3 halfVector = light + view;
    halfVector /= max(length(halfVector), 0.00001);
    float noL = max(dot(normal, light), 0.0);
    float noH = max(dot(normal, halfVector), 0.0);
    float voH = clamp(dot(view, halfVector), 0.0, 1.0);
    // Also widen subpixel sun glints at the actual raster footprint.
    vec3 dx = dFdx(halfVector);
    vec3 dy = dFdy(halfVector);
    float sunAlpha = min(material.alphaSquared + min(0.25 * (dot(dx, dx) + dot(dy, dy)), 0.25), 1.0);
    vec3 direct = evaluateBrdf(material, noL, noH, voH, sunAlpha) * uSunRadiance *
                  PI * shade.y * visibility;

    occlusion = clamp(occlusion, 0.0, 1.0);
    vec3 indirect = (vec3(1.0) - material.environmentWeight) * material.diffuseColor *
                     diffuseEnvironment(normal) * occlusion;
    indirect += environment * material.environmentWeight *
                 specularOcclusion(material.noV, occlusion, material.roughness);
    indirect *= clamp(shade.x, 0.0, 1.0) * 2.0 * ditheredShadowAmbient(visibility, noL) *
                ambientOcclusionVisibility(visibility);
    vec3 local = vec3(0.0);
    for (int i = 0; i < uPointLightCount; ++i) {
        vec3 delta = uPointLightPositionRadius[i].xyz - worldPosition;
        float distanceSquared = dot(delta, delta);
        float radiusSquared = uPointLightPositionRadius[i].w * uPointLightPositionRadius[i].w;
        if (distanceSquared >= radiusSquared) { continue; }
        vec3 direction = delta * inversesqrt(max(distanceSquared, 0.0001));
        float facing = max(dot(normal, direction), 0.0);
        if (facing <= 0.0) { continue; }
        float edge = 1.0 - distanceSquared / radiusSquared;
        float attenuation = edge * edge / (1.0 + 0.14 * distanceSquared);
        vec3 halfway = direction + view;
        halfway /= max(length(halfway), 0.00001);
        // A small luminous fixture is wider than a mathematical point source.
        vec3 response = evaluateBrdf(material, facing, max(dot(normal, halfway), 0.0),
                                      clamp(dot(view, halfway), 0.0, 1.0), max(material.alphaSquared, 0.004));
        local += response * uPointLightRadiance[i] * PI * facing * attenuation;
    }
    return direct + indirect + local;
}

vec3 materialRadiance(vec3 albedo, vec3 normal, vec3 worldPosition, vec2 shade, float visibility,
                      float roughness, float metallic, float reflectivity, float occlusion,
                      float dielectricReflectance) {
    vec3 toCamera = uCameraPosition - worldPosition;
    vec3 view = toCamera / max(length(toCamera), 0.00001);
    MaterialBrdf material = materialBrdf(albedo, normal, view, roughness, metallic, reflectivity, dielectricReflectance);
    vec3 reflection = reflect(-view, normal);
    return materialRadianceEnvironment(material, normal, worldPosition, view, shade, visibility, occlusion,
                                       reflectedEnvironment(reflection, normal, material.roughness));
}
