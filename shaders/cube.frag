#version 330 core

in vec3 vNormal;
noperspective in vec2 vVertexLight;
out vec4 FragColor;

uniform vec3 uColor;
uniform vec3 uSunDir;
uniform float uAmbient;
uniform bool uVertexLighting;

void main() {
    vec3 normal = normalize(vNormal);
    float diffuse = max(dot(normal, normalize(uSunDir)), 0.0);
    float ambient = clamp(uAmbient * (0.95 + 0.30 * normal.y), 0.0, 1.0);
    vec2 shade = uVertexLighting ? vVertexLight : vec2(ambient, (1.0 - ambient) * diffuse);
    float visibility = sunVisibility(diffuse);
    float light = shade.x * ditheredShadowAmbient(visibility, diffuse) + shade.y * visibility;
    FragColor = vec4(distanceFog(uColor * light), 1.0);
}
