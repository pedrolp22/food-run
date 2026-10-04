noperspective in float vDistanceFog;
uniform vec3 uFogColor;

float distanceFogAmount() {
    // A gentle distance cue across the configured range. Nearby materials keep
    // their full response; the completed distant surface fades toward the sky.
    return clamp(vDistanceFog, 0.0, 1.0);
}

vec3 distanceFog(vec3 color) {
    // Apply after material light so fog softens distant reflections and shadows
    // together. RGB555 presentation supplies the stable shade-transition dither.
    return mix(color, uFogColor, distanceFogAmount());
}
