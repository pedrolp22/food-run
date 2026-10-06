#version 330 core

in vec2 vTexCoord;
noperspective in vec2 vAffineTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uLod;
uniform float uLight;
uniform float uAmbientLight;
uniform float uAlpha;
uniform float uAlphaCutoff;
uniform float uTime;
uniform float uHeat;
uniform float uAffine;
uniform bool uReceivesShadow;
uniform vec3 uColor;

void main() {
    vec2 tc = vTexCoord;
    if (uAffine > 0.001) {
        tc = mix(vTexCoord, vAffineTexCoord, uAffine);
    }
    float heatBoost = 0.0;
    if (uHeat > 0.0001) {
        tc += vec2(sin(uTime * 2.3 + vTexCoord.y * 7.0) * 0.7 + sin(uTime * 3.4 + vTexCoord.y * 17.0) * 0.3,
                   sin(uTime * 1.7 + vTexCoord.y * 11.0) * 0.5) * (uHeat * 0.008);
        heatBoost = 0.04 * uHeat * sin(uTime * 4.2 + vTexCoord.y * 3.0);
    }
    float light = uReceivesShadow ? shadowedLight(uLight, uAmbientLight) : uLight;
    vec4 texel = sampleSurfaceLod(uTexture, tc, uLod, vTexCoord);
    if (texel.a < uAlphaCutoff) { discard; }
    vec3 surface = texel.rgb * uColor * light * (1.0 + heatBoost);
    FragColor = vec4(distanceFog(surface), uAlpha);
}
