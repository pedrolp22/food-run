// xy: render-target dimensions, z: grid spacing in target pixels (zero disables snapping).
uniform vec3 uSnap;
uniform float uSnapStrength = 1.0;
uniform vec2 uFogRange = vec2(0.0);
noperspective out float vDistanceFog;

vec3 surfaceDitherPosition(vec3 position, mat4 model) {
    // Fixed-size cells in mesh space follow translations/rotations without
    // swimming over the material. Model scale keeps their physical size consistent.
    return position * vec3(length(model[0].xyz), length(model[1].xyz), length(model[2].xyz));
}

vec4 snapVertex(vec4 clipPosition) {
    vDistanceFog = 0.0;
    if (uFogRange.y > uFogRange.x) {
        // PSX-style vertex depth cueing: eight-bit shades interpolate in screen
        // space. Use view distance instead of the old reciprocal-depth profile.
        float fog = (clipPosition.w - uFogRange.x) / (uFogRange.y - uFogRange.x);
        vDistanceFog = floor(clamp(fog, 0.0, 1.0) * 255.0 + 0.5) / 255.0;
    }
    // Leave near-plane clipping to the GPU. Dividing vertices at/behind the eye
    // produces unbounded coordinates and unstable triangles as objects pass the camera.
    if (uSnap.z <= 0.0 || uSnapStrength <= 0.0 || clipPosition.w <= 0.00001 || clipPosition.z < -clipPosition.w) {
        return clipPosition;
    }
    vec2 pixel = (clipPosition.xy / clipPosition.w * 0.5 + 0.5) * uSnap.xy;
    vec2 snapped = floor(pixel / uSnap.z + 0.5) * uSnap.z;
    pixel = mix(pixel, snapped, uSnapStrength);
    clipPosition.xy = (pixel / uSnap.xy * 2.0 - 1.0) * clipPosition.w;
    // Keep depth and perspective division intact; only rasterized XY is quantized.
    return clipPosition;
}
