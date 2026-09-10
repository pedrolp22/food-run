#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uMvp;
uniform mat4 uModel;
uniform mat4 uLightViewProj;
out vec4 vShadowPosition;

out vec3 vNormal;
out vec2 vUv;
noperspective out vec2 vAffineUv;

void main() {
    vShadowPosition = uLightViewProj * uModel * vec4(aPosition, 1.0);
    gl_Position = snapVertex(uMvp * vec4(aPosition, 1.0));
    vNormal = transpose(inverse(mat3(uModel))) * aNormal;
    vUv = aTexCoord;
    vAffineUv = aTexCoord;
}
