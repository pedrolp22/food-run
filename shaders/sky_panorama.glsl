// Mirror the upper-hemisphere bitmap horizontally to join nonmatching image edges.
uniform sampler2D uPanorama;
const float PANORAMA_PI = 3.14159265;

vec2 panoramaUv(vec3 ray) {
    float longitude = dot(ray.xz, ray.xz) > 0.000001 ? atan(ray.x, -ray.z) : 0.0;
    // Compress elevation toward the horizon so sky artwork remains visible
    // in the game's downward-facing camera rather than exposing only its bottom row.
    return vec2(longitude / PANORAMA_PI + 0.5, sqrt(clamp(ray.y, 0.0, 1.0)));
}

vec3 samplePanorama(vec3 ray) {
    // Preserve source texels in the sky and glass, without mip or pole blending.
    return textureLod(uPanorama, panoramaUv(ray), 0.0).rgb;
}

vec3 panoramaNight(vec3 color, float night) {
    return mix(color, color * vec3(0.16, 0.20, 0.32), night);
}
