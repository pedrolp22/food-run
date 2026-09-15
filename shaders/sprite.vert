#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

uniform mat4 uMvp;
uniform mat4 uModel;
uniform bool uExtruded;

out vec2 vTexCoord;
noperspective out vec2 vAffineTexCoord;
flat out vec3 vNormal;
flat out float vSide;

void main() {
    gl_Position = snapVertex(uMvp * vec4(aPosition, 1.0));
    vTexCoord = aTexCoord;
    vAffineTexCoord = aTexCoord;
    vNormal = uExtruded ? normalize(transpose(inverse(mat3(uModel))) * aNormal) : vec3(0.0, 0.0, 1.0);
    vSide = uExtruded ? 1.0 - abs(aNormal.z) : 0.0;
}
