#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;

uniform vec2 uScale;

out vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPosition.xy * uScale, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
