#version 330 core

out vec4 FragColor;
uniform vec3 uColor;
uniform float uAlpha;

uniform float uLight;
uniform float uAmbientLight;
uniform bool uReceivesShadow;

void main() {
    float light = uReceivesShadow ? shadowedLight(uLight, uAmbientLight) : uLight;
    FragColor = vec4(uColor * light, uAlpha);
}
