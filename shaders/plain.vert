#version 330 core
layout (location = 0) in vec3 aPosition;

uniform mat4 uMvp;
uniform mat4 uModel;
uniform mat4 uLightViewProj;
out vec4 vShadowPosition;
out vec3 vDitherPosition;

void main() {
    clipReflection(uModel * vec4(aPosition, 1.0));
    vDitherPosition = surfaceDitherPosition(aPosition, uModel);
    vShadowPosition = uLightViewProj * uModel * vec4(aPosition, 1.0);
    gl_Position = snapVertex(uMvp * vec4(aPosition, 1.0));
}
