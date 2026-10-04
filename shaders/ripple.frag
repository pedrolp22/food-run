#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uDistanceField;
uniform vec3 uColor;
uniform float uRadius;
uniform float uPhase;
uniform float uWidth;
uniform float uOpacity;
uniform bool uBurst;
uniform float uBurstProgress;

float glowBand(float delta, float width) {
    float d = delta / max(width, 0.5);
    return exp(-0.5 * d * d);
}

vec4 collectionPop(float distance, float aa) {
    float t = clamp(uBurstProgress, 0.0, 1.0);
    float expansion = 1.0 - pow(1.0 - t, 3.0); // Fast attack, easing out instead of accelerating from rest.
    float travel = expansion * uRadius * 0.85;
    float width = max(uWidth * (1.0 + expansion * 0.6), aa);
    float front = uPhase * uRadius * 0.5 + travel;
    float ring = glowBand(distance - front, width) +
                 0.5 * glowBand(distance - front - uRadius * 0.5, width);
    float spatialFade = 1.0 - smoothstep(uRadius * 0.65, uRadius, distance);
    float life = 1.0 - smoothstep(0.0, 1.0, t);
    // An immediate silhouette flash bridges the removed artwork into its outgoing outline.
    float flash = exp(-t * 20.0) * (1.0 - smoothstep(-0.5, 0.5, distance));
    float rimGlow = exp(-max(distance, 0.0) / max(uWidth * 1.5, 1.0)) * exp(-t * 10.0);
    float alpha = uOpacity * life * spatialFade * (0.7 * flash + 0.9 * ring + 0.35 * rimGlow);
    vec3 color = mix(uColor, vec3(1.0), 0.45 * exp(-t * 14.0));
    return vec4(color, clamp(alpha, 0.0, 1.0));
}

void main() {
    float distance = texture(uDistanceField, vTexCoord).r;
    float aa = max(fwidth(distance), 0.5);
    if (uBurst) {
        vec4 pop = collectionPop(distance, aa);
        if (uBurstProgress >= 1.0 || distance >= uRadius || pop.a < 0.001) { discard; }
        FragColor = vec4(pop.rgb, pop.a * (1.0 - distanceFogAmount()));
        return;
    }
    float spacing = uRadius * 0.5;
    // Positive phase moves constant-distance contours outward. Two waves fit within the radius.
    float waveDistance = abs(fract(distance / spacing - uPhase + 0.5) - 0.5) * spacing;
    float width = min(uWidth, spacing * 0.4);
    float wave = 1.0 - smoothstep(max(width - aa, 0.0), width + aa, waveDistance);
    float outward = clamp(distance / uRadius, 0.0, 1.0);
    float fade = pow(1.0 - outward, 1.5);
    float edge = smoothstep(0.0, 1.0, distance);
    float alpha = uOpacity * edge * fade * (0.08 * exp(-outward * 8.0) + 0.92 * wave);
    // Never tint the food itself or reveal the expanded quad's rectangular boundary.
    if (distance <= 0.0 || distance >= uRadius || alpha < 0.001) { discard; }
    FragColor = vec4(uColor, alpha * (1.0 - distanceFogAmount()));
}
