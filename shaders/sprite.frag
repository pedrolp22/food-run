#version 330 core

in vec2 vTexCoord;
noperspective in vec2 vAffineTexCoord;
uniform float uAffine;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uTime;
uniform float uSpawnTime;
uniform vec3 uRippleColor;
uniform float uAspect;

void main() {
    vec2 tc = mix(vTexCoord, vAffineTexCoord, uAffine);
    vec4 texColor = sampleSurface(uTexture, tc);
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

    FragColor = vec4(finalColor, uThreePoint ? texColor.a : 1.0);
}
