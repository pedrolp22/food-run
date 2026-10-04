#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uMvp;
uniform mat4 uModel;
uniform mat4 uLightViewProj;
uniform bool uQuad;
uniform vec3 uFaceNormal;
uniform vec2 uTexScale;
uniform vec2 uTexOffset;
uniform vec3 uSunDir;
uniform float uAmbient;

out vec4 vShadowPosition;
out vec3 vDitherPosition;
out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vTexCoord;
noperspective out vec2 vAffineTexCoord;
noperspective out vec2 vVertexLight;

void main() {
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    vWorldPosition = worldPosition.xyz;
    vDitherPosition = surfaceDitherPosition(aPosition, uModel);
    vShadowPosition = uLightViewProj * worldPosition;
    gl_Position = snapVertex(uMvp * vec4(aPosition, 1.0));
    // The compact quad mesh stores UV xy at location 1, while models and cubes
    // store a normal there. Its authored face normal is already in world space.
    vNormal = uQuad ? uFaceNormal : transpose(inverse(mat3(uModel))) * aNormal;
    vTexCoord = (uQuad ? aNormal.xy : aTexCoord) * uTexScale + uTexOffset;
    vAffineTexCoord = vTexCoord;
    vec3 normal = normalize(vNormal);
    float ambient = clamp(uAmbient * (0.95 + 0.30 * normal.y), 0.0, 1.0);
    float diffuse = max(dot(normal, normalize(uSunDir)), 0.0);
    vVertexLight = floor(vec2(ambient, (1.0 - ambient) * diffuse) * 255.0 + 0.5) / 255.0;
}
