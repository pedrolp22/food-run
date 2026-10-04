#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 uMvp;
uniform mat4 uModel;
uniform mat4 uLightViewProj;
out vec4 vShadowPosition;
out vec3 vDitherPosition;
uniform vec2 uTexScale;
uniform vec2 uTexOffset;

out vec2 vTexCoord;
noperspective out vec2 vAffineTexCoord;
out vec2 vFacadeUv;
out vec3 vWorldPosition;

void main() {
    vDitherPosition = surfaceDitherPosition(aPosition, uModel);
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vWorldPosition = worldPosition.xyz;
    vFacadeUv = aTexCoord;
    vShadowPosition = uLightViewProj * worldPosition;
    gl_Position = snapVertex(uMvp * vec4(aPosition, 1.0));
    vTexCoord = aTexCoord * uTexScale + uTexOffset;
    vAffineTexCoord = vTexCoord;
}
