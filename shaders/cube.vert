#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;

uniform mat4 uMvp;
uniform mat4 uModel;
uniform mat4 uLightViewProj;
uniform vec3 uSunDir;
uniform float uAmbient;
out vec4 vShadowPosition;
out vec3 vDitherPosition;

out vec3 vNormal;
noperspective out vec2 vVertexLight;

void main() {
    clipReflection(uModel * vec4(aPosition, 1.0));
    vDitherPosition = surfaceDitherPosition(aPosition, uModel);
    vShadowPosition = uLightViewProj * uModel * vec4(aPosition, 1.0);
    gl_Position = snapVertex(uMvp * vec4(aPosition, 1.0));
    vNormal = transpose(inverse(mat3(uModel))) * aNormal;
    vec3 normal = normalize(vNormal);
    float ambient = clamp(uAmbient * (0.95 + 0.30 * normal.y), 0.0, 1.0);
    float diffuse = max(dot(normal, normalize(uSunDir)), 0.0);
    vVertexLight = floor(vec2(ambient, (1.0 - ambient) * diffuse) * 255.0 + 0.5) / 255.0;
}
