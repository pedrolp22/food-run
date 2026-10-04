#version 330 core

in vec2 vTexCoord;
noperspective in vec2 vAffineTexCoord;
flat in vec3 vNormal;
flat in float vSide;
uniform float uAffine;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uTime;
uniform float uSpawnTime;
uniform vec3 uRippleColor;
uniform float uAspect;
uniform bool uExtruded;
uniform float uOutlinePixels;
uniform vec3 uSunDir;

float silhouetteAlpha(ivec2 pixel, ivec2 size) {
    if (any(lessThan(pixel, ivec2(0))) || any(greaterThanEqual(pixel, size))) {
        return 0.0;
    }
    return step(0.1, texelFetch(uTexture, pixel, 0).a);
}

vec4 shadeItem(vec2 tc, vec4 texColor) {
    ivec2 size = textureSize(uTexture, 0);
    ivec2 pixel = clamp(ivec2(floor(tc * vec2(size))), ivec2(0), size - 1);
    vec3 edgeColor = vec3(0.035) + uRippleColor * 0.15;
    float diffuse = dot(normalize(vNormal), normalize(uSunDir));
    if (vSide > 0.5) {
        // Each contour wall samples its own opaque edge pixel. Filtering across transparent
        // padding here would punch holes in the profile when the sprite turns edge-on.
        vec3 edgeTexel = texelFetch(uTexture, pixel, 0).rgb;
        vec3 sideColor = mix(edgeTexel * 0.8, edgeColor, 0.35);
        return vec4(sideColor * (0.7 + 0.3 * max(diffuse, 0.0)), 1.0);
    }
    int radius = int(clamp(floor(uOutlinePixels + 0.5), 0.0, 8.0));
    float interior = 1.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            interior = min(interior, silhouetteAlpha(pixel + ivec2(x, y) * radius, size));
        }
    }
    // Keep both textured faces readable throughout the turn, with darker, directionally lit sides.
    vec3 faceColor = texColor.rgb * (0.98 + 0.12 * abs(diffuse));
    return vec4(mix(edgeColor, faceColor, interior), 1.0);
}

void main() {
    vec2 tc = mix(vTexCoord, vAffineTexCoord, uAffine);
    vec4 texColor = sampleSurface(uTexture, tc);
    if (uExtruded) {
        vec4 item = shadeItem(tc, texColor);
        if (vSide < 0.5 && texColor.a < 0.1) { discard; }
        FragColor = vec4(distanceFog(item.rgb), item.a);
        return;
    }
    float alpha = step(0.1, texColor.a);

    vec2 texelSize = 1.0 / vec2(textureSize(uTexture, 0));

    float transparentNeighbors =
        (1.0 - step(0.1, sampleSurface(uTexture, tc + vec2(0.0, texelSize.y)).a)) +
        (1.0 - step(0.1, sampleSurface(uTexture, tc - vec2(0.0, texelSize.y)).a)) +
        (1.0 - step(0.1, sampleSurface(uTexture, tc + vec2(texelSize.x, 0.0)).a)) +
        (1.0 - step(0.1, sampleSurface(uTexture, tc - vec2(texelSize.x, 0.0)).a));
    // Evaluate all texture derivatives before discarding fragments at cutout edges.
    if (alpha < 0.5) { discard; }
    float outline = step(0.5, transparentNeighbors);

    float pixelGrid = 12.0;

    vec2 centered = tc - 0.5;
    float dist = length(vec2(centered.x * uAspect, centered.y));
    float pixelDist = floor(dist * pixelGrid + 0.5) / pixelGrid;

    float ripple = sin(pixelDist * 14.0 - uTime * 5.0 + uSpawnTime * 2.0) * 0.5 + 0.5;

    vec3 outlineColor = uRippleColor + vec3(0.30);
    vec3 finalColor = texColor.rgb * 1.35 + outlineColor * outline * (0.55 + 1.35 * ripple);

    FragColor = vec4(distanceFog(finalColor), 1.0);
}
