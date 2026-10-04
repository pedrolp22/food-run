#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;

out vec2 vUv;

void main() {
    // Coverage quad only; the fragment shader reconstructs the camera's world
    // direction so the sky has the same projection as scene geometry.
    gl_Position = vec4(aPosition.xy * 2.0, 0.0, 1.0);
    vUv = aTexCoord;
}
