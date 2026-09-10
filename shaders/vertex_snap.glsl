// xy: render-target dimensions, z: grid spacing in target pixels (zero disables snapping).
uniform vec3 uSnap;

vec4 snapVertex(vec4 clipPosition) {
    // Leave near-plane clipping to the GPU. Dividing vertices at/behind the eye
    // produces unbounded coordinates and unstable triangles as objects pass the camera.
    if (uSnap.z <= 0.0 || clipPosition.w <= 0.00001 || clipPosition.z < -clipPosition.w) {
        return clipPosition;
    }
    vec2 pixel = (clipPosition.xy / clipPosition.w * 0.5 + 0.5) * uSnap.xy;
    pixel = floor(pixel / uSnap.z + 0.5) * uSnap.z;
    clipPosition.xy = (pixel / uSnap.xy * 2.0 - 1.0) * clipPosition.w;
    // Keep depth and perspective division intact; only rasterized XY is quantized.
    return clipPosition;
}
