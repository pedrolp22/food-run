in vec4 vShadowPosition;
uniform sampler2DShadow uShadowMap;

float sunVisibility(float diffuse) {
    vec3 projected = vShadowPosition.xyz / vShadowPosition.w * 0.5 + 0.5;
    if (any(lessThanEqual(projected, vec3(0.0))) || any(greaterThanEqual(projected, vec3(1.0)))) {
        return 1.0;
    }
    float bias = mix(0.00035, 0.00006, clamp(diffuse, 0.0, 1.0));
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            visibility += texture(uShadowMap, vec3(projected.xy + vec2(x, y) * texel,
                                                   projected.z - bias));
        }
    }
    return visibility / 9.0;
}

float shadowedLight(float light, float ambient) {
    float direct = max(light - ambient, 0.0);
    return min(light, ambient) + direct * sunVisibility(direct / (1.0 - ambient));
}
