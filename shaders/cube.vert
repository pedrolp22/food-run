#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;

uniform mat4 uMvp;
uniform mat4 uModel;
uniform mat4 uLightViewProj;
out vec4 vShadowPosition;

out vec3 vNormal;

void main() {
    vShadowPosition = uLightViewProj * uModel * vec4(aPosition, 1.0);
    gl_Position = snapVertex(uMvp * vec4(aPosition, 1.0));
    vNormal = transpose(inverse(mat3(uModel))) * aNormal;
}
