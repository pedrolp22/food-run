#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aQuadUv;
layout (location = 2) in vec2 aModelUv;
uniform mat4 uMvp;
uniform bool uModelMesh;
uniform vec2 uTexScale;
uniform vec2 uTexOffset;
out vec2 vUv;
void main() {
    vec4 position = vec4(aPosition, 1.0);
#ifdef MODEL_MAX_SKIN_JOINTS
    if (uSkinned) position = skinMatrix() * position;
#endif
    gl_Position = uMvp * position;
    vUv = (uModelMesh ? aModelUv : aQuadUv.xy) * uTexScale + uTexOffset;
}
